/*
 * linux/include/asm-arm/arch-pxa/time.h
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/apm_bios.h>

static inline unsigned long pxa_get_rtc_time(void)
{
	return RCNR;
}

static int pxa_set_rtc(void)
{
	unsigned long current_time = xtime.tv_sec;

	if (RTSR & RTSR_ALE) {
		/* make sure not to forward the clock over an alarm */
		unsigned long alarm = RTAR;
		if (current_time >= alarm && alarm >= RCNR)
			return -ERESTARTSYS;
	}
	RCNR = current_time;
	return 0;
}

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long pxa_gettimeoffset (void)
{
	unsigned long ticks_to_match, elapsed, usec;

	/* Get ticks before next timer match */
	ticks_to_match = OSMR0 - OSCR;

	/* We need elapsed ticks since last match */
	elapsed = LATCH - ticks_to_match;

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed*tick)/LATCH;

	return usec;
}

static void pxa_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	long flags;
	int next_match;

	/* Loop until we get ahead of the free running timer.
	 * This ensures an exact clock tick count and time acuracy.
	 * IRQs are disabled inside the loop to ensure coherence between
	 * lost_ticks (updated in do_timer()) and the match reg value, so we
	 * can use do_gettimeofday() from interrupt handlers.
	 */
	do {
		do_leds();
		do_set_rtc();
		save_flags_cli( flags );
		do_timer(regs);
		OSSR = OSSR_M0;  /* Clear match on timer 0 */
		next_match = (OSMR0 += LATCH);
		restore_flags( flags );
	} while( (signed long)(next_match - OSCR) <= 0 );
}

/*
 * by zxf, 11/11/2002
 * add sleep timer to timer init
 */
static struct irqaction kernel_sharedtimer_irq = {
	name: "kernel-sharedtimer",
};
/* change pm to kernel shared timer by linwq 2003-3-26 */
#define  KERNEL_TIMER_OSCR2_GATE_COUNT         4       /*  1 us */
#define  KERNEL_TIMER_WAITING_QUEUE_LEN        10
#define  KERNEL_TIMER_HANDLING_QUEUE_LEN       10

typedef enum {
     KERNEL_RETURN_SUCCESS                    = 0,
     KERNEL_RETURN_TIMER_ARRIVED,               
     KERNEL_RETURN_FAILED,    
     KERNEL_RETURN_TOO_TIMER_HANDLING,
     KERNEL_RETURN_NO_HANDLER,
     KERNEL_RETURN_UNKNOW                     =0xffffffff
} KERNEL_RETURN_RESULT;

static struct timer_list* waiting_queue[KERNEL_TIMER_WAITING_QUEUE_LEN];
static struct timer_list* handling_queue;

static unsigned int handling_number = 0;
static unsigned int waiting_number  = 0;

static unsigned int waiting_no  = KERNEL_TIMER_WAITING_QUEUE_LEN;

static spinlock_t kernel_timer_lock = SPIN_LOCK_UNLOCKED;

static void init_timer_queue( struct timer_list** timer,unsigned int no )
{
    unsigned int i;
    for(i=0;i<no;i++)
        timer[i]  = NULL;
    return;
}

static unsigned int find_empty_no( struct timer_list** timer,unsigned int no )
{
    unsigned int i;
    for(i=0;i<no;i++)
    {
        if(timer[i] == NULL)
            break;
    }
    return i;
}

KERNEL_RETURN_RESULT insert_waiting_queue_orderly(struct timer_list *timer,unsigned long expires)
{
    unsigned long temp,tempno,tempbackno;

    temp = find_empty_no(waiting_queue,KERNEL_TIMER_WAITING_QUEUE_LEN);
    if(temp == KERNEL_TIMER_WAITING_QUEUE_LEN)
    {
        return KERNEL_RETURN_FAILED;
    }
    waiting_queue[temp] = timer;
    if(0 == waiting_number)
    {
        waiting_number = 1;
        waiting_no = temp;
        timer->list.next = temp; 
    }
    else
    {
        waiting_number++;
        if(timer->expires > expires)
        {
            if((waiting_queue[waiting_no]->expires >= timer->expires)||(waiting_queue[waiting_no]->expires < expires ))
            {
                timer->list.next = waiting_no;
                waiting_no = temp;
            }
            else
            {
                tempno = waiting_no;
                tempbackno = tempno; 
                tempno = waiting_queue[tempno]->list.next;
                while( (timer->expires > waiting_queue[tempno]->expires)&&(waiting_queue[tempno]->expires > expires ) \
                        &&(!(waiting_queue[tempno]->list.next == tempno) ) )
                {
                    tempbackno = tempno; 
                    tempno = waiting_queue[tempno]->list.next;
                }  
                
                if((timer->expires > waiting_queue[tempno]->expires)&&(waiting_queue[tempno]->expires > expires ))  
                {
                    timer->list.next = temp;
                    waiting_queue[tempno]->list.next = temp;
                }
                else
                {
                    timer->list.next = waiting_queue[tempbackno]->list.next;
                    waiting_queue[tempbackno]->list.next = temp;
                }
            }
        }
        else /* case : timer->expires < expires */
        {
            if((waiting_queue[waiting_no]->expires >= timer->expires)&&(waiting_queue[waiting_no]->expires < expires ))
            {
                timer->list.next = waiting_no;
                waiting_no = temp;
            }
            else
            {
                tempno = waiting_no;
                tempbackno = tempno; 
                tempno = waiting_queue[tempno]->list.next;
           
                while( (!(waiting_queue[tempno]->list.next == tempno) )&& ( (waiting_queue[tempno]->expires > expires)||  \
                    (timer->expires > waiting_queue[tempno]->expires)  ) )
                {
                    tempbackno = tempno; 
                    tempno = waiting_queue[tempno]->list.next;
                }  
            
                if((waiting_queue[tempno]->expires > expires)||(timer->expires > waiting_queue[tempno]->expires))
                {
                    timer->list.next = temp;
                    waiting_queue[tempno]->list.next = temp;
                }
                else
                {  
                    timer->list.next = waiting_queue[tempbackno]->list.next;
                    waiting_queue[tempbackno]->list.next = temp;
                }
            }
        }
    }
}

KERNEL_RETURN_RESULT kernel_OSCR2_setup(struct timer_list *timer)
{
    unsigned long temp,tempOSCR;
    unsigned long expires;
    unsigned long flags;
    
    if((NULL == timer)||(timer->function == NULL))
        return KERNEL_RETURN_NO_HANDLER;
    expires = timer->expires;
    if(expires < KERNEL_TIMER_OSCR2_GATE_COUNT)
        return KERNEL_RETURN_TIMER_ARRIVED;
    if( KERNEL_TIMER_WAITING_QUEUE_LEN == waiting_number )
        return KERNEL_RETURN_TOO_TIMER_HANDLING;
    if(0xffffffff - OSCR > expires)
    {
	timer->expires = OSCR + expires;
    }
    else
    {
        timer->expires = expires -(0xffffffff - OSCR);
    }
     
    if(0 == handling_number )
    {
        spin_lock_irqsave(&kernel_timer_lock,flags); 
        handling_queue = timer ;
        /* enable the timer   */
        OSMR2 = handling_queue -> expires;
        OIER |= OIER_E2;
        handling_number = 1;        
        spin_unlock_irqrestore(&kernel_timer_lock,flags);
    }
    else
    {
        spin_lock_irqsave(&kernel_timer_lock,flags); 
        tempOSCR = OSCR;
        if(tempOSCR <= handling_queue->expires )
        {
            temp = handling_queue->expires - tempOSCR;
        }
        else
        {
            temp = 0xffffffff - tempOSCR+ handling_queue->expires;
        }
        
        if(expires < temp)
        {
            if(KERNEL_RETURN_FAILED == insert_waiting_queue_orderly(handling_queue,tempOSCR))
            {
                spin_unlock_irqrestore(&kernel_timer_lock,flags);
                return KERNEL_RETURN_FAILED;
            }
            handling_queue = timer ;

            /* enable the timer   */
            OSMR2 = handling_queue -> expires;
            OIER |= OIER_E2;
            handling_number = 1;        
        }
        else
        {
            if(KERNEL_RETURN_FAILED == insert_waiting_queue_orderly(timer,tempOSCR))
            {
                 spin_unlock_irqrestore(&kernel_timer_lock,flags);
                 return KERNEL_RETURN_FAILED;
            }
        }
        spin_unlock_irqrestore(&kernel_timer_lock,flags);
    }
    return KERNEL_RETURN_SUCCESS;
}

KERNEL_RETURN_RESULT kernel_OSCR2_delete(struct timer_list *timer)
{
    unsigned long i,tempno,tempbackno;
    unsigned long flags;
    if((NULL == timer)||(timer->function == NULL))
        return KERNEL_RETURN_NO_HANDLER;
    if(0 == handling_number )
    {
         return KERNEL_RETURN_NO_HANDLER;
    }
    else
    {
        if(timer == handling_queue )
        {
            spin_lock_irqsave(&kernel_timer_lock,flags); 
            OIER &= ~OIER_E2;
            OSSR = OSSR_M2;  
            handling_number = 0;
    
            if( waiting_number )
            {
                handling_queue = waiting_queue[waiting_no];
                waiting_queue[waiting_no] = NULL;
                waiting_no = handling_queue->list.next;
                waiting_number--;
                /* enable timer interrupt  */ 
 
                OSMR2 = handling_queue -> expires;
                OIER |= OIER_E2;
                handling_number = 1;        
            }   
            spin_unlock_irqrestore(&kernel_timer_lock,flags);
        }
        else /* maybe the timer is in waiting queue */
        {
            if(0 == waiting_number )
            {
                return KERNEL_RETURN_NO_HANDLER;
            }
            spin_lock_irqsave(&kernel_timer_lock,flags); 
            tempno = waiting_no;
            tempbackno = tempno;
            i = waiting_number;
            while(i)
            {
                if( timer == waiting_queue[tempno])
                    break;
                tempbackno = tempno;
                tempno = waiting_queue[tempno]->list.next;
                i--;
            }
            if( timer == waiting_queue[tempno])
            {
                if(tempno == waiting_no)
                {
                    waiting_no = waiting_queue[waiting_no]->list.next;
                }
                else
                {   
                    if(waiting_queue[tempno]->list.next ==tempno )
                    {
                        waiting_queue[tempbackno]->list.next = tempbackno;
                    }
                    else
                    {
                        waiting_queue[tempbackno]->list.next = waiting_queue[tempno]->list.next;
                    }
                } 
                waiting_number--;
                waiting_queue[tempno] = NULL;                
            }
            else
            {
                spin_unlock_irqrestore(&kernel_timer_lock,flags);
                return KERNEL_RETURN_NO_HANDLER;
            }
            spin_unlock_irqrestore(&kernel_timer_lock,flags);
        }
    }
    return KERNEL_RETURN_SUCCESS;
}

/* pm timer handler */
void pxa_kerneltimer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned long tempOSCR;
    unsigned long tempno;
    struct timer_list *timer;

    void (*fn)(unsigned long);
    unsigned long data;
    unsigned long flags;

    spin_lock_irqsave(&kernel_timer_lock,flags); 
    OIER &= ~OIER_E2;
    OSSR = OSSR_M2;  
    if(!handling_number)
    {
        spin_unlock_irqrestore(&kernel_timer_lock,flags);
        return;
    }
    fn = handling_queue->function;
    data = handling_queue->data;
    tempOSCR = handling_queue->expires;
    handling_queue = NULL;
    handling_number = 0;
    spin_unlock_irqrestore(&kernel_timer_lock,flags);

    fn(data);
    
    spin_lock_irqsave(&kernel_timer_lock,flags); 
    timer = waiting_queue[waiting_no];
    while(waiting_number&&(timer->expires <= OSCR))
    {
        if(timer->expires >= tempOSCR)
        {
            tempOSCR = timer->expires;
            tempno = timer->list.next;
            data = timer->data;
            fn = timer->function;
            spin_unlock_irqrestore(&kernel_timer_lock,flags);
            fn(data);
            spin_lock_irqsave(&kernel_timer_lock,flags); 
            waiting_number--;
            waiting_queue[waiting_no]  = NULL;

            if( 0 == waiting_number )
            {                 
                break;
            }
            else
            {
                waiting_no = tempno;
                timer = waiting_queue[waiting_no];    
            }
        }
        else
        {
            break;
        }
    }    
    
    if( waiting_number )
    {
        handling_queue = waiting_queue[waiting_no];
        waiting_queue[waiting_no] = NULL;
        waiting_no = handling_queue->list.next;
        waiting_number--;
        /* enable timer interrupt  */ 
        OSMR2 = handling_queue -> expires;
        OIER |= OIER_E2;
        handling_number = 1;        
    } 
    spin_unlock_irqrestore(&kernel_timer_lock,flags);
    /* finished the job */
#if 0
	extern int can_sleep;
	extern void queue_apm_event(apm_event_t event, struct apm_user *sender);
	APM_DPRINTK("pm isr: idle timeout\n");
	/* if pm sleep timer expired, queue event and enable OS timer0 */
	queue_apm_event(KRNL_IDLE_TIMEOUT, NULL);
	can_sleep = 1;
	OIER &= ~OIER_E2;
	OSSR = OSSR_M2;  /* Clear match on timer 2 */
#endif
}	

void kernel_setup_timer()
{
        init_timer_queue(waiting_queue,KERNEL_TIMER_WAITING_QUEUE_LEN);
        kernel_sharedtimer_irq.handler = pxa_kerneltimer_interrupt;
	setup_arm_irq(IRQ_OST2, &kernel_sharedtimer_irq);
}

extern inline void setup_timer (void)
{
	gettimeoffset = pxa_gettimeoffset;
	set_rtc = pxa_set_rtc;
	xtime.tv_sec = pxa_get_rtc_time();
	timer_irq.handler = pxa_timer_interrupt;
	OSMR0 = 0;		/* set initial match at 0 */
	OSSR = 0xf;		/* clear status on all timers */
	setup_arm_irq(IRQ_OST0, &timer_irq);
	OIER |= OIER_E0;	/* enable match on timer 0 to cause interrupts */
	OSCR = 0;		/* initialize free-running timer, force first match */
	kernel_setup_timer();
}

