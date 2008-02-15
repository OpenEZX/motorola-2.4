/*
 *  linux/drivers/char/ezx-ts.c
 *
 *  Copyright (C) 2002 Lili Jiang, All Rights Reserved.
 * 
 *
 *  June, 2002 Lili Jiang, create
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/pm.h>
#include <linux/config.h>
#include <linux/fb.h>

#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/arch/irqs.h>
#include <linux/timex.h>
#include <linux/interrupt.h>

#include "ezx-ts.h"
#include "ssp_pcap.h"

#define DEBUG 0  /*change it when formal release*/
#if DEBUG
#  define TS_DPRINTK(s...)	printk(s)
#else
#  define TS_DPRINTK(s...)
#endif

/*
 * This structure is nonsense - millisecs is not very useful
 * since the field size is too small.  Also, we SHOULD NOT
 * be exposing jiffies to user space directly.
 */
struct ts_event {
	u16		pressure;
	u16		x;
	u16		y;
	u16		pad;
//	struct timeval	stamp;
};

static struct timer_list ezx_ts_penup_timer;

static struct irqaction ezx_ts_custom_timer_irq = {
        name: "ezx_ts_custom_timer",
};

static u8 bSetPenupTimer = 0;
static u8 bFirstPen = 0;
//static unsigned long penCurJiffies = 0;
static unsigned int penSampleCount = 0;
static u32  tspenSampleCount = 0;
static u32  tsDiscardDataCount = 0;

typedef enum
{
    PRESSURE,
    COORDINATE	
}EZX_TS_READ_STATE;

EZX_TS_READ_STATE ezx_ts_read_state = PRESSURE;

#define TS_NR_EVENTS	128
/*250 //pay attention to that haed and tail is defined as u8*/

#define TS_EDGE_MIN_LIMIT  (0) /* 10 */
#define TS_EDGE_MAX_LIMIT  (1023)

/* jiffies : 10ms,  pen shake 50ms????? */
//#define EZX_TS_READ_MIN_SPACE   10 /*100//5*/
//#define EZX_TS_PENUP_TIMEOUT_MS 8 

#define EZX_TS_PENSAMPLE_TIMEOUT_MS  2
                                    //8,6
//#define EZX_TS_HWR_TIMEOUT_MS   2

#define QT_ONEPEN_SAMPLES_NUM   1
                               //3,5
//#define HWR_ONEPEN_SAMPLES_NUM  1

#define CUSTOM_ONEPEN_SAMPLES_NUM  1
#define EZX_TS_CUSTOM_TIMEOUT_MS   12
                                   //15ms
#define X_DIFF 85
#define Y_DIFF 75
#define MIN_X_HOLD              5
#define MIN_Y_HOLD              4  
#define SAMPLE_COUNT_NOT_JUMPER   20

typedef enum
{
	EZX_PEN_NORMAL,
	EZX_PEN_CUSTOM
}EZX_TS_PEN_STYLE;

static EZX_TS_PEN_STYLE gPenStyle = EZX_PEN_NORMAL;
static u8 gPenSamplesNum = QT_ONEPEN_SAMPLES_NUM;
//static u32 gHwrTimer = EZX_TS_HWR_TIMEOUT_MS;
static u32 gCustomTimer = EZX_TS_CUSTOM_TIMEOUT_MS;
static u8 getPenUp = 0;

struct ezx_ts_info {
	/*struct ucb1x00		*ucb;*/
	struct fasync_struct	*fasync;
	wait_queue_head_t	read_wait;

	/*
	struct semaphore	irq_wait;
	struct semaphore	sem;
	struct completion	init_exit;
	struct task_struct	*rtask;
	*/
	u16			x_cur; /*store the current pen value read from ADC*/
	u16			y_cur; /*store the current pen value read from ADC */
    u16         cur_pressure;
    u16         x_pre;
    u16         y_pre;
	u8			evt_head;
	u8			evt_tail;
	struct ts_event		events[TS_NR_EVENTS]; /* buffer for store pen data*/
/*	
    int			restart:1;
	int			adcsync:1;

#ifdef CONFIG_PM
	struct pm_dev		*pmdev;
#endif
*/
};

static struct ezx_ts_info ezxts;

#define EZX_TS_SHAKE_SAMPLES 3
static u8 ezx_ts_shake_samples_count = 0;
static struct ts_event ezx_ts_shake_samples[EZX_TS_SHAKE_SAMPLES];

static int ezx_ts_init(void);
static void ezx_ts_exit(void);
static int ezx_ts_open(struct inode *inode, struct file * filp);
static int ezx_ts_release(struct inode *inode, struct file *filp);
static ssize_t ezx_ts_read(struct file* filp, char* buffer, size_t count, loff_t *ppos);
static unsigned int ezx_ts_poll(struct file* filp, struct poll_table_struct * wait);
static int ezx_ts_fasync(int fd, struct file* filp, int mode);

/* internal function */
static void ezx_ts_init_penEvent(void);
static void ezx_ts_pen_touch_handler(void);
static void ezx_ts_penup_timeout(unsigned long data);
static void ezx_ts_set_timer(struct timer_list *timer, void (*timer_timeout)(unsigned long), signed long timeout);
static void ezx_ts_check_pen_samples(void);
static void ezx_ts_evt_add(u16 pressure, u16 x, u16 y);
static struct ts_event ezx_ts_get_onedata(void);

/* for setup ezx ts self timer,begin */
static void ezx_ts_custom_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    TS_DPRINTK("ezx_ts_custom_timer_interrupt entered.jiffies: %d\n@@@\n", jiffies);
    SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
    SSP_PCAP_TSI_start_XY_read(); 
    
    /* clear this timer */
    OIER &= ~OIER_E3;
    OSSR = OSSR_M3;  /* Clear match on timer 2 */
}

static void ezx_ts_custom_setup_timer()
{
    ezx_ts_custom_timer_irq.handler = ezx_ts_custom_timer_interrupt;
    setup_arm_irq(IRQ_OST3, &ezx_ts_custom_timer_irq);
}


static void ezx_ts_custom_timer_start(void)
{
    OIER |= OIER_E3;
    //OSMR2 = OSCR + sleep_limit * LATCH; //OSCR: 0x40A00010
    OSMR3 = OSCR + (gCustomTimer * HZ * LATCH) /1000;
}

static void ezx_ts_custom_timer_stop(void)
{
    OIER &= ~OIER_E3;
    OSSR = OSSR_M3;  /* Clear match on timer 2 */
}

/* for setup ezx ts self timer, end */


static void ezx_ts_init_penEvent(void)
{
   int i;
   
   ezxts.x_cur = 0;
   ezxts.y_cur = 0;
   ezxts.x_pre = 0;
   ezxts.y_pre = 0;
   ezxts.cur_pressure = PEN_UP;

   for (i = 0; i < TS_NR_EVENTS; i++)
   {       
       ezxts.events[i].pressure=(u16)0;
       ezxts.events[i].x=(u16)0;
       ezxts.events[i].y=(u16)0;
       ezxts.events[i].pad=(u16)0;
   }
   ezxts.evt_head = 0;
   ezxts.evt_tail = 0;
   
   gPenStyle = EZX_PEN_NORMAL;	
   gPenSamplesNum = QT_ONEPEN_SAMPLES_NUM;
   //gHwrTimer = EZX_TS_HWR_TIMEOUT_MS;
   gCustomTimer = EZX_TS_CUSTOM_TIMEOUT_MS;
}


static void ezx_ts_pen_touch_handler(void)
{
    SSP_PCAP_TSI_mode_set(PCAP_TS_PRESSURE_MEASUREMENT);
    SSP_PCAP_TSI_start_XY_read();  
    ezx_ts_read_state = PRESSURE;
    ezxts.x_pre = 0;
    ezxts.y_pre = 0;

    TS_DPRINTK("ezx_ts pen touch handler done.\n");
}
    
static void ezx_ts_penup_timeout(unsigned long data)
{
    TS_DPRINTK("@@@\nIn pen drag! jiffies: %d\n@@@\n", jiffies);
    bSetPenupTimer = 0;
    SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
    SSP_PCAP_TSI_start_XY_read(); 
}

static void ezx_ts_check_pen_samples(void)
{
	if ( (ezx_ts_read_state == COORDINATE) && (penSampleCount != 0 ) )
	{
	    int k;
        if (penSampleCount < gPenSamplesNum)
        {
			for ( k = 0; k < (gPenSamplesNum - penSampleCount); k++)
			{
			    ezx_ts_evt_add(PEN_DOWN, ezxts.x_cur, ezxts.y_cur);
			    TS_DPRINTK("ezx_ts_add_additional_samples: %d\n", k);
			}    
		}	
		TS_DPRINTK("ezx_ts_evt_add: PEN_UP\n");
	    //ezx_ts_evt_add(PEN_UP, 0, 0);
	}
	
    if (bSetPenupTimer)
	{
        del_timer(&ezx_ts_penup_timer);
        bSetPenupTimer = 0;
        TS_DPRINTK("^^^^Delete penup timer1.\n");
    }
    penSampleCount = 0;
}	

static void ezx_ts_set_timer(struct timer_list *timer, void (*timer_timeout)(unsigned long), 
	                        signed long timeout)
{
	init_timer(timer);
	timer->expires = timeout + jiffies;
	timer->data = (unsigned long) current;
    timer->function = timer_timeout;
    	
	add_timer(timer);
	//TS_DPRINTK("ezx_ts set timer done.\n");
}

static void ezx_ts_evt_add(u16 pressure, u16 x, u16 y)
{
	int next_head;
	int last_head;
    /* if pen_up, then copy pressure=0, last_evt.x y to current evt*/
   
    next_head = ezxts.evt_head + 1;
    if (next_head == TS_NR_EVENTS)
    {
    	next_head = 0;
    }
    
    last_head = ezxts.evt_head - 1;
    if (last_head == -1)
    {
    	last_head = TS_NR_EVENTS - 1;
    }
    
/*    if (next_head != ezxts.evt_tail)*/
    //{
	    if ( pressure == PEN_DOWN)
	    {
/*	        if ( (penSampleCount >1) && ((penCurJiffies + EZX_TS_READ_MIN_SPACE) < jiffies))
	        {
	            // see two ~ four data or just ignore the data except the first one????
                    return;
	        }
	        else*/
	        //{
			    ezxts.events[ezxts.evt_head].pressure = pressure;
				ezxts.events[ezxts.evt_head].x = x;
				ezxts.events[ezxts.evt_head].y = y;
				TS_DPRINTK("ezx_store: x: %d, y :%d, pressure: 0x%x\n", x, y, pressure);
	        //}
	    }	
	    else
	    {
	        ezxts.events[ezxts.evt_head].pressure = pressure;
	        ezxts.events[ezxts.evt_head].x = ezxts.events[last_head].x;
			ezxts.events[ezxts.evt_head].y = ezxts.events[last_head].y;
			TS_DPRINTK("ezx_store: PEN_UP\n");
	    }
	//}   
//      do_gettimeofday(&(ezxts.events[ezxts.evt_head].stamp)); 
	ezxts.evt_head = next_head;
	
	if (ezxts.evt_head == ezxts.evt_tail)
	{
	    if ( ++ezxts.evt_tail == TS_NR_EVENTS )
		    ezxts.evt_tail = 0;
	}

	if (ezxts.fasync)
			kill_fasync(&ezxts.fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&ezxts.read_wait);
}

static struct ts_event ezx_ts_get_onedata(void)
{
	int cur = ezxts.evt_tail;

	if (++ezxts.evt_tail == TS_NR_EVENTS)
		ezxts.evt_tail = 0;

	return ezxts.events[cur];
}

/*****************************************************************************
 *  FUNCTION NAME  : ezx_ts_open()
 * 
 *  INPUTS: 
 *    
 *  OUTPUTS:  pen info from ts event structure
 *                
 *  VALUE RETURNED: none
 *    
 *  DESCRIPTION: 
 *    i) Increase this device module used count.
 *    ii) Initialize a buffer for store data read from touch screen interface.
 *    iii) Initialize touch screen interface hardware.
 *    iv) Setup touch screen wait queue.
*****************************************************************************
 */
static int ezx_ts_open(struct inode *inode, struct file * filp)
{
	struct ezx_ts_info *ts = &ezxts;

/*    ssp_pcap_init(); //  change to _start_kernel 
      ssp_pcap_open(); */
    ssp_pcap_open(SSP_PCAP_TS_OPEN);
    SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM);	 
    
    ezx_ts_init_penEvent();
    filp->private_data = ts;
    
    
	MOD_INC_USE_COUNT;
	TS_DPRINTK("ezx touch screen driver opened\n");
	return 0;
}

/*  Decrease this device module used count. */
static int ezx_ts_release(struct inode *inode, struct file *filp)
{
    ezx_ts_init_penEvent();
    ezx_ts_fasync(-1, filp, 0);
    
	MOD_DEC_USE_COUNT;
	TS_DPRINTK("ezx touch screen driver closed\n");
	return 0;
}

/*****************************************************************************
 *  FUNCTION NAME  :  ezx_ts_read
 * 
 *  INPUTS: 
 *    
 *  OUTPUTS:  pen info from ts event structure
 *                
 *  VALUE RETURNED: none
 *    
 *  DESCRIPTION: 
 *   This is used for UI app to call.
 *   If device is opened by nonblock mode then copy available data from the ts event buffer
 *   to user buffer and return the actual read data count, if device is opened by block mode 
 *   and no data available then sleep until the required data count be read completed. 
 *
 *  CAUTIONS: 
 *    
 *    
 *****************************************************************************
 */
static ssize_t ezx_ts_read(struct file* filp, char* buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	
	struct ezx_ts_info *ts = filp->private_data;
	char *ptr = buffer;
	int err = 0;
    struct ts_event evt;
    
	add_wait_queue(&ts->read_wait, &wait);
	while (count >= sizeof(struct ts_event)) 
	{
	    err = -ERESTARTSYS;
		if (signal_pending(current))
			break;
		if ((ts)->evt_head != (ts)->evt_tail)
		{
			evt = ezx_ts_get_onedata(); /*evt_tail pos changed in this func*/
			err = copy_to_user(ptr, &evt, sizeof(struct ts_event));

			if (err)
				break;
			//printk("ezx_ts_read: x:%d, y:%d, pressure:0x%x\n", evt.x, evt.y, evt.pressure);	
			ptr += sizeof(struct ts_event);
			count -= sizeof(struct ts_event);
			continue;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		err = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&(ts->read_wait), &wait);

	return ptr == buffer ? err : ptr - buffer;

}

/*
This function is used in nonblock mode to determine whether it can read data from touch screen 
device without blocking. 
Call poll_wait to wait until read operation status changed then return POLLIN (flag of device can be 
read without blocking) | POLLRDNORM (flag of data is available) bit mask if there is readable data 
now.
*/

static unsigned int ezx_ts_poll(struct file* filp, struct poll_table_struct * wait)
{
	struct ezx_ts_info *ts = filp->private_data;
	int ret = 0;

    if ( ts != NULL)
	{
        TS_DPRINTK("ezx_ts_poll.\n");
	    poll_wait(filp, &ts->read_wait, wait);
	    if (ts->evt_head != ts->evt_tail)
	    {
	    	TS_DPRINTK("ezx_ts_poll: ts->evt_head != ts->evt_tail\n");
		    ret = POLLIN | POLLRDNORM;
		}    
    }		    

	return ret;
}

/* Setup fasync queue for touch screen device. */

static int ezx_ts_fasync(int fd, struct file* filp, int mode)
{
	struct ezx_ts_info *ts = filp->private_data;

    if ( ts != NULL)
    {
    	TS_DPRINTK("ezx_ts_fasync.\n");
	    return fasync_helper(fd, filp, mode, &ts->fasync);
	}
	else return 0;    
}

static int
ezx_ts_ioctl(struct inode *inode, struct  file *file, uint cmd, ulong arg)
{	
	int res = 0;
	
	TS_DPRINTK("ezx_ts_ioctl: arg is %d\n", arg);
	switch (cmd) {
/*
	case SETTONORMAL:
	    TS_DPRINTK("ezx_ts_ioctl_test: This is SETTONORMAL.\n");
	    gPenStyle = EZX_PEN_NORMAL;	
	    gPenSamplesNum = QT_ONEPEN_SAMPLES_NUM;
	    //gHwrTimer = EZX_TS_HWR_TIMEOUT_MS;
	    gCustomTimer = EZX_TS_CUSTOM_TIMEOUT_MS;
	    break;
	    
	case SETTOHWR:
	    TS_DPRINTK("ezx_ts_ioctl_test: This is SETTOHWR.\n");
		gPenStyle = EZX_PEN_HWR;	
		gPenSamplesNum = HWR_ONEPEN_SAMPLES_NUM;
		if ( (arg > 0 ) && (arg < 100) )
		{
			gHwrTimer = arg;
			TS_DPRINTK("ezx_ts_ioctl_test: gHwrTimer is %d.\n", gHwrTimer);
		}	
		break;    

	case SETTOCUSTOM:
	    TS_DPRINTK("ezx_ts_ioctl_test: This is SETTOCUSTOM.\n");
		gPenStyle = EZX_PEN_CUSTOM;	
		gPenSamplesNum = CUSTOM_ONEPEN_SAMPLES_NUM;
		if ( (arg > 0 ) && (arg < 100) )
		{
			gCustomTimer = arg;
			TS_DPRINTK("ezx_ts_ioctl_test: gCustomTimer is %d.\n", gCustomTimer);
		}	
		break;    	
	case GETPENSTYLE:
	    TS_DPRINTK("ezx_ts_ioctl_test: This is GETPENSTYLE.\n");
	    put_user(gPenStyle, (u32*)arg);    
		break;
	default:
	    res = -EINVAL;
*/
	}
	
	return res;
}

/*****************************************************************************
 *  FUNCTION NAME  :  ezx_ts_touch_interrupt
 * 
 *  INPUTS:
 *    
 *  
 *  OUTPUTS:
 *    
 *
 *  VALUE RETURNED:
 *    
 *
 *  DESCRIPTION: 
 *  This is touch screen touch interrupt service routine called by PCAP module when touch screen 
 *  interrupt happened. First eliminate shake signal to get stable touch screen press information. 
 *  Then write instruction to PCAP register to read ADC conversion value of pressed spot x,y coordinate 
 *  and return.
 *
 *  CAUTIONS:
 *    
 *    
 *
 *****************************************************************************
 */
void ezx_ts_touch_interrupt(int irq, void* dev_id, struct pt_regs *regs)
{	
	TS_DPRINTK("ezx_ts_touch_irq happened.\n");
	ezx_ts_shake_samples_count = 0;
	//TS_DPRINTK("ezx_ts_touch_irq excuted.\n");
	ezx_ts_check_pen_samples();
    ezx_ts_pen_touch_handler();	    
	
}

/*****************************************************************************
 *  FUNCTION NAME  :  ezx_ts_dataReadok_interrupt
 * 
 *  INPUTS: call PCAP API to read x,y
 *    
 *  OUTPUTS:  Store the pen info to ts event structure
 *                
 *  VALUE RETURNED: none
 *    
 *  DESCRIPTION: 
 *   This is called by PCAP module to deal with the touch screen press raw information read from ADC
 *   when ADC data conversion finished. It performs write the data to ts event buffer for user to 
 *   read out. 
 *
 *  CAUTIONS: Consider pen shake
 *    
 *    
 *****************************************************************************
 */
void ezx_ts_dataReadok_interrupt(int irq, void* dev_id, struct pt_regs *regs)
{
    static u16 pressure;
    u16 x;
    u16 y;
    U16 tempx,tempy;

    if (SSP_PCAP_TSI_get_XY_value(&x, &y) != SSP_PCAP_SUCCESS)
    {
        return;
    }
    /* convert x,y???? */
   // TS_DPRINTK( "ezx_ts_get_XY_value: x:%d, y:%d\n",x, y);
   /* TS_DPRINTK( "\nezx_ts before convert: x:%d, y:%d\n",x, y);*/
  
    if ( ezx_ts_read_state == PRESSURE)
    {
        if (( y >= TS_EDGE_MAX_LIMIT ) || (y <= TS_EDGE_MIN_LIMIT))
        {
            TS_DPRINTK("ezx_ts_datareadok_irq: PEN_UP.\n");
            pressure = PEN_UP;
            ezxts.cur_pressure = PEN_UP;
           
            /*ezx_ts_check_pen_samples();*/
            if ( getPenUp == 0 )
            {
            	ezx_ts_evt_add( pressure, 0, 0);
            }
             SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM);
            return;
        }
        else 
        {
            TS_DPRINTK("ezx_ts_datareadok_irq: PEN_DOWN.\n");
            getPenUp = 0;
            pressure = PEN_DOWN;
            ezxts.cur_pressure = PEN_DOWN;
            tspenSampleCount = 0;
            ezxts.x_pre = 0;
            ezxts.y_pre = 0;
            bFirstPen = 1 ;
	        SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
	        SSP_PCAP_TSI_start_XY_read(); 
	        ezx_ts_read_state = COORDINATE;
	       
	        //TS_DPRINTK("ezx_ts_datareadok_irq: Begin read XY.\n");
            return;
        }
    }
    else if ( ezx_ts_read_state == COORDINATE)
    {/*
    	u8 count;
    	
        count = 0;
       	while ( (( x <= TS_EDGE_MIN_LIMIT)||( x >= TS_EDGE_MAX_LIMIT) ||
	            ( y <= TS_EDGE_MIN_LIMIT)||( y >= TS_EDGE_MAX_LIMIT))
	            && (count++ < 15) )
	    {
	    	SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM);
	    	SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
	        SSP_PCAP_TSI_start_XY_read(); 
 	    }
 	  */  
	    if ( ( x <= TS_EDGE_MIN_LIMIT)||( x >= TS_EDGE_MAX_LIMIT) ||
	         ( y <= TS_EDGE_MIN_LIMIT)||( y >= TS_EDGE_MAX_LIMIT) )	        
	    {  /* regard it as PEN_UP */
	        pressure = PEN_UP;
	        ezxts.cur_pressure = PEN_UP;
	   	    x = 0;
	        y = 0;
                tspenSampleCount = 0;
  	        SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM); 
	        /*del_timer(&ezx_ts_read_timer);*/
	        ezx_ts_check_pen_samples();
	        ezx_ts_evt_add( pressure, 0, 0);
	       /* SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM);*/
	        ezx_ts_read_state = PRESSURE;
	        getPenUp = 1;
	        TS_DPRINTK( "ezx_ts Invalid x,y position: PEN_UP?\n");
            return;
	    }
	    getPenUp = 0;
        penSampleCount++;
        //TS_DPRINTK("ezx_ts_datareadok_irq: Begin store XY %d.\n", penSampleCount);
	    
	    if ( gPenStyle == EZX_PEN_CUSTOM )
	    {
	    	if ( ((ezxts.x_pre == 0) && (ezxts.y_pre == 0)) 
	    	     || ( (ezxts.x_pre != 0) && (ezxts.y_pre != 0)
	    	     &&(!((abs(ezxts.x_pre - x) > X_DIFF) || (abs(ezxts.y_pre - y) > Y_DIFF))) )
	    	   )     
	    	{
	    		//TS_DPRINTK("ezx_ts_datareadok_irq: diff <= XY_DIFF.\n");
	    		
	    		ezxts.x_pre = x;
	    		ezxts.y_pre = y;
	    		 
	    		TS_DPRINTK("ezx_ts_datareadok_irq:x_pre=%d,y_pre=%d\n", ezxts.x_pre, ezxts.y_pre);
	    		ezxts.x_cur = x;
    			ezxts.y_cur = y;
    			ezx_ts_evt_add(pressure, ezxts.x_cur, ezxts.y_cur);
			//printk("ezx_ts_datareadok_irq_custom add pressure0x%x,x_cur%d,y_cur%d\n",pressure,ezxts.x_cur,ezxts.y_cur);
    			ezx_ts_custom_timer_start();
            }
            else
            {
            	//TS_DPRINTK("ezx_ts_datareadok_irq: DIFF > XY_DIFF\n");
            	SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
	            SSP_PCAP_TSI_start_XY_read(); 
            	TS_DPRINTK("ezx_ts_datareadok_irq:x_pre=%d,y_pre=%d, x=%d,y=%d\n", ezxts.x_pre, ezxts.y_pre,x,y);
	        }
	        
	        
	        /*
	        if ( ezx_ts_shake_samples_count < EZX_TS_SHAKE_SAMPLES )
	        {
	        	ezx_ts_shake_samples[ezx_ts_shake_samples_count].x = x;
	        	ezx_ts_shake_samples[ezx_ts_shake_samples_count].y = y;
	        	ezx_ts_shake_samples[ezx_ts_shake_samples_count++].pressure = PEN_DOWN;
	            SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
                SSP_PCAP_TSI_start_XY_read();
                TS_DPRINTK("ezx_ts_read_shake_samples%d:x=%d,y=%d\n", ezx_ts_shake_samples_count,x,y);
		    }
		    if (ezx_ts_shake_samples_count >= EZX_TS_SHAKE_SAMPLES)
	        {
	        	if ( (abs(ezx_ts_shake_samples[0].x - ezx_ts_shake_samples[1].x) > X_DIFF) ||
                     (abs(ezx_ts_shake_samples[0].y - ezx_ts_shake_samples[1].y) > Y_DIFF) ||   	        	
	        	     (abs(ezx_ts_shake_samples[1].x - ezx_ts_shake_samples[2].x) > X_DIFF) ||
                     (abs(ezx_ts_shake_samples[1].y - ezx_ts_shake_samples[2].y) > Y_DIFF)
                   )  
                {   
                	// throw these data and re-read
	        	    ezx_ts_shake_samples_count = 0;
	        	}
	        	else //save to ts_event queue
	        	{
	        		ezxts.x_pre = ezxts.x_cur;
		    		ezxts.y_pre = ezxts.y_cur;
		    		TS_DPRINTK("ezx_ts_datareadok_irq:x_pre=%d,y_pre=%d\n", ezxts.x_pre, ezxts.y_pre);
		    		ezxts.x_cur = (ezx_ts_shake_samples[0].x + ezx_ts_shake_samples[1].x + ezx_ts_shake_samples[2].x) / 3;
	    			ezxts.y_cur = (ezx_ts_shake_samples[0].y + ezx_ts_shake_samples[1].y + ezx_ts_shake_samples[2].y) / 3;;
    				ezx_ts_shake_samples_count = 0;
	        		ezx_ts_evt_add(pressure, ezxts.x_cur, ezxts.y_cur);
	        	}	    
	        	ezx_ts_custom_timer_start();
	        }	
    	    */
    	}    
        else 
        {
             /* ezxts.x_pre and ezxts.y_pre store the last position */
             tspenSampleCount ++;
             if(1 == tspenSampleCount)
             {
                 ezxts.x_cur = x;
                 ezxts.y_cur = y;   
                 if( 0 == bSetPenupTimer )
                 {              
                     SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
                     SSP_PCAP_TSI_start_XY_read(); 
                 }
             }
             else if (2 == tspenSampleCount)
             {
                 tspenSampleCount = 0;

                 tempx = (ezxts.x_cur > x? ezxts.x_cur - x:x - ezxts.x_cur);
                 tempy = (ezxts.y_cur > y? ezxts.y_cur - y:y - ezxts.y_cur);
                 if(tempx <= X_DIFF && tempy <= Y_DIFF )
                 {
                     x = (x+ezxts.x_cur)/2;
                     y = (y+ezxts.y_cur)/2;
                     if( 1 == bFirstPen)
                     {
                         bFirstPen = 0;
                         ezxts.x_pre = x;
                         ezxts.y_pre = y;
                     }
                     tempx = ( ezxts.x_pre > x? ezxts.x_pre -x:x - ezxts.x_pre);
                     tempy = ( ezxts.y_pre > y? ezxts.y_pre -y:y - ezxts.y_pre);
                     if(tempx <= X_DIFF && tempy <= Y_DIFF )
                     {
                         tsDiscardDataCount = 0;
                         if(tempx > MIN_X_HOLD || tempy > MIN_Y_HOLD)
                         {
                             ezxts.x_pre = x;
                             ezxts.y_pre = y;
                         }
                         ezx_ts_evt_add(pressure, ezxts.x_pre, ezxts.y_pre);	                
                         if ( bSetPenupTimer == 0 )
                         {
                             ezx_ts_set_timer(&ezx_ts_penup_timer, ezx_ts_penup_timeout, 
    	        	              EZX_TS_PENSAMPLE_TIMEOUT_MS);
                             TS_DPRINTK("ezx_ts_datareadok_irq: set timer pen sample\n");    	        	              
                             bSetPenupTimer = 1;
                         }
                     }
                     else
                     {
                         
                         tsDiscardDataCount ++; 
                         if(tsDiscardDataCount > SAMPLE_COUNT_NOT_JUMPER)
                         {
                             tsDiscardDataCount = 0;
                             ezxts.x_pre = x;
                             ezxts.y_pre = y;
                             ezx_ts_evt_add(pressure, ezxts.x_pre, ezxts.y_pre);	                
                             if ( bSetPenupTimer == 0 )
                             {
                                  ezx_ts_set_timer(&ezx_ts_penup_timer, ezx_ts_penup_timeout, 
    	        	                          EZX_TS_PENSAMPLE_TIMEOUT_MS);
                                  TS_DPRINTK("ezx_ts_datareadok_irq: set timer pen sample\n");    	        	              
                                  bSetPenupTimer = 1;
                             }
                         }
                         else if( 0 == bSetPenupTimer )
                         {              
                             SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
                             SSP_PCAP_TSI_start_XY_read(); 
                         }
                     }
                 }
                 else
                 {
                     if( 0 == bSetPenupTimer )
                     {              
                         SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
                         SSP_PCAP_TSI_start_XY_read(); 
                     }
                 }                                  
             }
             else /* discard the case */
             {
                 tspenSampleCount = 0;
                 if( 0 == bSetPenupTimer )
                 {              
                     SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
                     SSP_PCAP_TSI_start_XY_read(); 
                 }
             }
        }
        /*if (penSampleCount >= gPenSamplesNum)
        {
            penSampleCount = 0;
        }*/
        //TS_DPRINTK("ezx_ts_datareadok_irq: read XY finished.\n");
    }
    
}


static struct file_operations ezx_ts_fops = {
	owner:		THIS_MODULE,
	read:		ezx_ts_read,
	poll:		ezx_ts_poll,
	open:		ezx_ts_open,
	release:	ezx_ts_release,
	fasync:	    ezx_ts_fasync,
	ioctl:      ezx_ts_ioctl,
};

/*
 * miscdevice structure for misc driver registration.
 */
static struct miscdevice ezx_ts_dev = 
{
     minor: EZX_TS_MINOR_ID,
     name:  "ezx_ts",
     fops:  &ezx_ts_fops
};

/*Register touch screen device in misc devices.*/
static int ezx_ts_init(void)
{
	int ret;

    init_waitqueue_head(&ezxts.read_wait);
	ret = misc_register(&ezx_ts_dev);
	
	if (ret<0)
	{
	    printk("ezx_ts_dev: failed to registering the device\n");
	}	
    ezx_ts_custom_setup_timer();
    
    TS_DPRINTK("ezx_ts_init done.\n");
	return ret;
}	

/*Unregister touch screen device.*/
static void ezx_ts_exit(void)
{
	
    if (bSetPenupTimer)
    {
        del_timer(&ezx_ts_penup_timer);
        bSetPenupTimer = 0;
    }
    
    ezx_ts_custom_timer_stop();
    	
	misc_deregister(&ezx_ts_dev);

	TS_DPRINTK("ezx touch screen driver removed\n");
}

module_init(ezx_ts_init);
module_exit(ezx_ts_exit);

MODULE_DESCRIPTION("ezx touchscreen driver");
MODULE_LICENSE("GPL");
