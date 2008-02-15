/*
 * linux/drivers/char/ezx-button.c, button driver on ezx
 *
 * Copyright (C) 2002 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* GPIO2  - SCAN ROW1
 * GPIO3  - SCAN ROW2
 * GPIO52 - SCAN COL1
 * GPIO54 - SCAN COL2
 * GPIO55 - SCAN COL3
 * GPIO40 - VOLTAGE CONTROL
 * GPIO5  - VOICE RECORD 
 * GPIO10 - FLIP
 */

#include <linux/init.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/apm_bios.h>

#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/semaphore.h>
#include <asm/mach-types.h>

/* add by jia for cebus*/
#include <linux/proc_fs.h>
#include <linux/wrapper.h>

#include "ezx-button.h"
#include "../ssp/ssp_pcap.h"


/*add by jia for cebus*/
#define OPTION1  (1<<6)     /*GPIO6*/
#define OPTION2  (1<<7)     /*GPIO7*/
#define ASEL1    (1<<8)     /*GPIO8*/
#define ASEL0    (1<<5)     /*GPIO37*/
#define ASEL2    (1<<6)     /*GPIO38*/


/*
int CEBus_proc_read(char *buffer,char **buffer_location,off_t offset,int buffer_length);
struct CEBus_entry{
	char *name;
	int (*fn)(char*,char**,off_t,int);
};
static struct CEBus_entry CEBus_proc={"CEBus_indication",CEBus_proc_read};
struct proc_dir_entry *CEBus_dir;
*/

#undef CEBUS_INT_HNDL
#define CEBUS_INT_HNDL
//#define CEBUS_DEBUG
//#define DEBUG
#undef DEBUG

#define FULL(queue)	((queue.head+1==queue.tail)||(queue.head+1-NR_BUTTONS==queue.tail))

#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

struct button_queue{
	unsigned short button_buffer[NR_BUTTONS];
	unsigned int head;
	unsigned int tail;
};

static struct timer_list gpio2_timer;
static struct timer_list gpio5_timer;	
static struct timer_list gpio10_timer;	
static struct timer_list answer_timer;	
static int  dev_id[2]={2,3};

static DECLARE_WAIT_QUEUE_HEAD(button_wait_queue); /* Used for blocking read */
static struct button_queue    buttonevent;

// 0-up 1-down 2-joy_m 3-home 4-joy_l 5-joy_r
struct button_flag
{   
    unsigned int state;
    unsigned int need_up;
}button_flags[MATRIX_NUMBER] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

const unsigned short button_event[MATRIX_NUMBER]={UP, DOWN, JOY_M, HOME, JOY_L, JOY_R};

static int vr_flag=0, answer_flag=0, flip_flag=0;

EXPORT_SYMBOL(answer_button_handler);
EXPORT_SYMBOL(headset_in_handler);
EXPORT_SYMBOL(headset_out_handler);

u32 cebus_is_open = 0;
#ifdef CEBUS_INT_HNDL
static u16 cebus_current_device = CEBUS_NONE_DEVICE; 

extern int usb_gpio_init(void);
extern int stop_usb(void);
extern void cebus_stop_ffuart(u16 device);
extern void cebus_start_ffuart(u16 device);

static void cebus_stop_EIHFCarkit(void)
{
    return;
}
static void cebus_start_EIHFCarkit(void)
{
    /* GPIO 36 as GPIO mode output and put high  */
    GPDR1   |= 0x00000010;
    GPSR1    = 0x00000010;
    PGSR1   |= 0x00000010;     
    GAFR1_L &= 0xfffffcff;
    return ;
}

static void restore_init_status()
{
        // init gpio40 output, low
        GPDR1 |= 0x00000100; 	
        GPCR1 = 0x00000100;  	
        // init gpio52, gpio54, gpio55 output, low
        GPDR1 |= 0x00D00000; 	
        GPCR1 = 0x00D00000; 	
}

u16 get_current_cebus_status(void)
{
    return cebus_current_device;
}
/* return 0 : success  1: fail  */
void  destroy_cebus_device(u16 cebus_device)
{
#ifdef CEBUS_DEBUG
    u8 temp[100];
    sprintf(temp,"destroy cebus device %d!",cebus_device);
    pcap_log_add_data(temp,strlen(temp));
#endif
    switch(cebus_device)
    {
        case CEBUS_NONE_DEVICE:
            break;
        case CEBUS_PC_POWERED_USB:
            stop_usb();
            break;
        case CEBUS_DUMB_TTY:
            break;
        case CEBUS_DUMB_DESKTOP_SPEAKERPHONE:
            break;
        case CEBUS_DUMB_PTT_HEADSET:
            break;
        case CEBUS_DUMB_FM_RADIO_HEADSET:
            break;
        case CEBUS_DUMB_IRDA_ADAPTER:
            break;
        case CEBUS_DUMB_CLIP_ON_SPEAKERPHONE:
            break;
        case CEBUS_DUMB_SMART_AUDIO_DEVICE:
            /* we support 4 wires uart smart audio device */
            cebus_stop_ffuart(CEBUS_RS232_4_WIRES);
            break;
        case CEBUS_DUMB_EIHF:
            cebus_stop_EIHFCarkit();
            break;
        case CEBUS_PHONE_POWERED_USB:
            break;
        case CEBUS_RS232_8_WIRES:
            /* we should care 8 wires case */
            cebus_stop_ffuart(CEBUS_RS232_8_WIRES);
            break;
        case CEBUS_RS232_4_WIRES:
            /* we should care 4 wires case */
            cebus_stop_ffuart(CEBUS_RS232_4_WIRES);
            break;
        case CEBUS_PUSHTOTALK_PTT:
            break;
        default:
            break;
    }
}

void  cebus_none_device_gpio_init(void)
{
      /* GPIO 9  as input gpio */
      GPDR0   &= 0xfffffdff;
      GAFR0_L &= 0xfff3ffff;

      /* GPIO 34 as input gpio */      
      GPDR1   &= 0xfffffffb;
      /* GPIO 39 output gpio and put low  */
      GPDR1   |= 0x00000080;
      PGSR1   &= ~0x00000080;
      /* GPIO 36/41 as output and put high  */
      GPDR1   |= 0x00000210;
      PGSR1   |= 0x00000210;
      
      GAFR1_L &= 0xfff33ccf;
      GPSR1   =  0x00000210;
      GPCR1   =  0x00000080;
      /* GPIO 8/37/38 as output gpios put high */
      GPDR0   |= CEBUS_ASEL1;
      PGSR0   |= CEBUS_ASEL1; 
      GPDR1   |= (CEBUS_ASEL0|CEBUS_ASEL2);
      GAFR0_L &= 0xfffcffff; 
      GAFR1_L &= 0xffffc3ff;
      PGSR1   |= (CEBUS_ASEL0|CEBUS_ASEL2);
      GPSR0    = CEBUS_ASEL1;
      GPSR1    = (CEBUS_ASEL0|CEBUS_ASEL2);
      /* GPIO 6/7 init by auto-detector in cebus_int_hndler() */                   
      return;
}

void  create_cebus_device(u16 cebus_device)
{
#ifdef CEBUS_DEBUG
    u8 temp[100];
    sprintf(temp,"create cebus device %d!",cebus_device);
    pcap_log_add_data(temp,strlen(temp));
#endif
    switch(cebus_device)
    {
        case CEBUS_NONE_DEVICE:
            cebus_none_device_gpio_init();
            break;
        case CEBUS_PC_POWERED_USB:
            SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_HIGH_USB_PORT);
            usb_gpio_init();
            break;
        case CEBUS_DUMB_TTY:
            break;
        case CEBUS_DUMB_DESKTOP_SPEAKERPHONE:
            break;
        case CEBUS_DUMB_PTT_HEADSET:
            break;
        case CEBUS_DUMB_FM_RADIO_HEADSET:
            break;
        case CEBUS_DUMB_IRDA_ADAPTER:
            break;
        case CEBUS_DUMB_CLIP_ON_SPEAKERPHONE:
            break;
        case CEBUS_DUMB_SMART_AUDIO_DEVICE:
            /* we support 4 wires uart smart audio device */
	    SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_SERIAL_PORT);
            cebus_start_ffuart(CEBUS_RS232_4_WIRES);
            break;
        case CEBUS_DUMB_EIHF:
            cebus_start_EIHFCarkit();
            break;
        case CEBUS_PHONE_POWERED_USB:
            break;
        case CEBUS_RS232_8_WIRES:
            /* we should care 8 wires case */          
	    SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_SERIAL_PORT);
            cebus_start_ffuart(CEBUS_RS232_8_WIRES);
            break;
        case CEBUS_RS232_4_WIRES:
            /* we should care 4 wires case */
	    SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_SERIAL_PORT);
            cebus_start_ffuart(CEBUS_RS232_4_WIRES);
            break;
        case CEBUS_PUSHTOTALK_PTT:
            break;
        default:
            break;
    }
}
#endif


static int button_ioctl(struct inode *inode, struct  file *file, uint cmd, ulong arg)
{
  	int data, ret;
    /*add by jia for cebus*/
	unsigned int opt1,opt2;

 
	switch(cmd){
	case BUTTON_GET_FLIPSTATUS:
		data = GPLR0 & 0X400;
		if(!data)
			ret = FLIP_OFF;
		else
			ret = FLIP_ON;
		return put_user(ret, (int *)arg);
	case BUTTON_GET_HEADSETSTATUS:
		data = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
		if(!data)
			ret = HEADSET_IN;
		else
			ret = HEADSET_OUT;
		return put_user(ret, (int *)arg);
        /*add by jia for cebus*/
        case BUTTON_GET_CEBUSSTATUS:
             return put_user(CEBUS_BUTTON|cebus_current_device, (int *)arg);
	default:
		return  -EINVAL;
	}
	return 0;
}

static unsigned int button_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &button_wait_queue, wait);
	if(buttonevent.head != buttonevent.tail)
		return POLLIN | POLLRDNORM;
	return 0;
}
/*
 * This function is called when a user space program attempts to read
 * /dev/button. It puts the device to sleep on the wait queue until
 * irq handler writes some data to the buffer and flushes
 * the queue, at which point it writes the data out to the device and
 * returns the number of characters it has written. This function is
 * reentrant, so that many processes can be attempting to read from the
 * device at any one time.
 */

static int button_read (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval = 0;

#ifdef DEBUG
//	printk("enter button read\n");
//	printk("buffer=0x%lx\n", buffer);
//	printk("count=0x%lx\n", count);
#endif

	if (buttonevent.head == buttonevent.tail) 
	{
#ifdef DEBUG
//	printk("button buffer is null\n");
#endif
		add_wait_queue(&button_wait_queue, &wait);
		current->state = TASK_INTERRUPTIBLE;

		while (buttonevent.head == buttonevent.tail) 
		{
			if (file->f_flags & O_NONBLOCK) 
			{
#ifdef DEBUG
//				printk("nonblock return -EAGAIN\n");
#endif
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) 
			{
#ifdef DEBUG
//				printk("has non pending signal, RESTARTSYS\n");
#endif
				retval = -ERESTARTSYS;
				break;
			}
#ifdef DEBUG
//			printk("schedule current task\n");
#endif
			schedule();
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&button_wait_queue, &wait);
	}
	
	if (retval)
		return retval;

	while ((buttonevent.head != buttonevent.tail) && (retval + sizeof(unsigned short)) <= count) 
	{
		if (copy_to_user(buffer+retval, buttonevent.button_buffer+buttonevent.tail, sizeof(unsigned short))) 
			return -EFAULT;
		buttonevent.tail = (buttonevent.tail+1)%(NR_BUTTONS);
		retval += sizeof(unsigned short);
	}
#ifdef DEBUG
//	printk("retval=0x%lx\n", retval);
#endif
	return retval;	
}

static void scan_gpio2_gpio3_button(unsigned long parameters)
{
	unsigned long data;
	int bdelay = BUTTON_INTERVAL;	
	unsigned int count = 0;
	unsigned int i;
	struct button_flag *flag;
	const unsigned short *event; 

#ifdef DEBUG
//	printk("enter scan_gpio2_gpio3_button\n");
#endif
	data = GPLR0;
	if(!(data & 0x4) || !(data & 0x8))	// still pressed, begin scan
	{
#ifdef DEBUG
//		printk("gpio2/gpio3 still low\n");
#endif
		disable_irq(IRQ_GPIO(2));
		disable_irq(IRQ_GPIO(3));
		
		// set gpio40 output, high
		GPDR1 |= 0x00000100;
		GPSR1 = 0x00000100;
		// set gpio52, gpio54, gpio55 input
		GPDR1 &= 0xFF2FFFFF;	
#ifdef DEBUG
//		printk("one bye one test colume\n");
#endif
		// one bye one, set gpio52, gpio54, gpio55 low	
		GPDR1 |= 0x00100000;		
		GPCR1 = 0x00100000;
		udelay(10);	
		data = GPLR0;
		if(!(data & 0x4))	// up pressed
		{
			button_flags[0].state ++;
			 count ++;
                }
                else    // up release
                {
                        button_flags[0].state = 0;
                }
                if(!(data & 0x8))       // down pressed
                {
                        button_flags[1].state ++;
                        count ++;
                }
                else
                {
                        button_flags[1].state = 0;
                }
		GPDR1 &= ~0x00100000;

                GPDR1 |= 0x00400000;
                GPCR1 = 0x00400000;
                udelay(10);
                data = GPLR0;
                if(!(data & 0x4))       // joy_m pressed
                {
                        button_flags[2].state ++;
		        count ++;
                }
                else
                {
                        button_flags[2].state = 0;
                }
	        if(!(data & 0x8))       // home pressed
                {
                        button_flags[3].state ++;
        		count ++;
                }
                else
                {
                        button_flags[3].state = 0;
                }
		GPDR1 &= ~0x00400000;

                GPDR1 |= 0x00800000;
                GPCR1 = 0x00800000;
                udelay(10);
                data = GPLR0;
                if(!(data & 0x4))       // joy_L pressed
                {
                        button_flags[4].state ++;
		        count ++;
                }
                else
                {
		        button_flags[4].state = 0;
                }
                if(!(data & 0x8))       // joy_R pressed
                {
                        button_flags[5].state ++;
		        count ++;
                }
                else
                {
                        button_flags[5].state = 0;
                }
		restore_init_status();

                gpio2_timer.expires = (jiffies + bdelay);
                add_timer (&gpio2_timer);
	}
	else
    	{
        	for(i = 0, flag = button_flags; i < MATRIX_NUMBER ; i ++, flag++)
        	{
	            flag->state = 0;
        	}
		restore_init_status();

	        enable_irq(IRQ_GPIO(2));
        	enable_irq(IRQ_GPIO(3));
#ifdef DEBUG
                printk("reenable gpio2/gpio3 interrupt\n");
#endif
    	}
	for(i = 0, flag = button_flags, event = button_event; i < MATRIX_NUMBER; i ++, flag++, event++)
    	{
	        if((flag->state > 0) && (count <= 3))
        	{
	            if(!FULL(buttonevent) && ((flag->state == 1) || ((flag->state%NR_REPEAT) == 0)))
        	    {
                	flag->need_up = 1;
                        buttonevent.button_buffer[buttonevent.head] = (*event)|KEYDOWN;
                        buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
                        wake_up_interruptible(&button_wait_queue);
#ifdef CONFIG_PM
			queue_apm_event(KRNL_KEYPAD, NULL);
#endif
#ifdef DEBUG
                         printk("button %d pressed\n", i);
#endif
                    }
        	}
	        else if(!flag->state && flag->need_up)
        	{
	            if(!FULL(buttonevent))
        	    {
                	flag->need_up = 0;
                        buttonevent.button_buffer[buttonevent.head] = (*event)|KEYUP;
                        buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
                        wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
                        printk("button %d released\n", i);
#endif
                    }
		}
        }
}

static void scan_gpio5_button(unsigned long parameters)
{
	unsigned long data;
	int bdelay = BUTTON_INTERVAL;	

#ifdef DEBUG
//	printk("enter scan_gpio4_button\n");
#endif
	data = GPLR0;
	if(!(data & 0x20))	// keydown
	{
		vr_flag ++;
                if((!FULL(buttonevent))&&((vr_flag==1)||((vr_flag%NR_REPEAT)==0)))
		{
			buttonevent.button_buffer[buttonevent.head] = VOICE_REC | KEYDOWN;
			buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
			wake_up_interruptible(&button_wait_queue);
#ifdef CONFIG_PM
			queue_apm_event(KRNL_VOICE_REC, NULL);
#endif
#ifdef DEBUG
			printk("voice record keydown\n");
#endif
		}
		gpio5_timer.expires = (jiffies + bdelay);
		add_timer (&gpio5_timer);
	}
	else if(vr_flag)	// keyup
	{
		del_timer(&gpio5_timer);
		vr_flag = 0;
		if(!FULL(buttonevent))
		{
			buttonevent.button_buffer[buttonevent.head] = VOICE_REC | KEYUP;
			buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
			wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
			printk("voice record keyup\n");
#endif
		}
	}
}

static void scan_gpio10_button(unsigned long parameters)
{
	unsigned long data;
	int bdelay = BUTTON_INTERVAL;	

#ifdef DEBUG
//	printk("enter scan_gpio10_button\n");
#endif
	data = GPLR0 & 0x400;
	if(data != flip_flag)
	{
		flip_flag = data;
		if(!data)	
		{
		    if(!FULL(buttonevent))
		    {
			buttonevent.button_buffer[buttonevent.head] = FLIP | KEYDOWN;
			buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
			wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
			printk("flip close\n");
#endif
		    }
#ifdef CONFIG_PM
			queue_apm_event(KRNL_FLIP_OFF, NULL);
#endif
		}
		else 
		{
		    if(!FULL(buttonevent))
		    {
			buttonevent.button_buffer[buttonevent.head] = FLIP | KEYUP;
			buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
			wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
			printk("flip open\n");
#endif
	            }
#ifdef CONFIG_PM
			queue_apm_event(KRNL_FLIP_ON, NULL);
#endif
		}
	}
}

static void scan_answer_button(unsigned long parameters)
{
	unsigned long data;
	int ssp_pcap_bit_status;	
	int bdelay = BUTTON_INTERVAL;	

#ifdef DEBUG
//	printk("enter scan_answer_button\n");
#endif
	/* read pcap register to check if headjack is in */
        ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
        if( ssp_pcap_bit_status == 0 )  /* headjack is in */
        {
		ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_MB2SNS);
		if(ssp_pcap_bit_status == 0)	// keydown
		{
			answer_flag ++;
                	if((!FULL(buttonevent))&&((answer_flag==1)||((answer_flag%NR_REPEAT)==0)))
			{	
				buttonevent.button_buffer[buttonevent.head] = ANSWER | KEYDOWN;
				buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
				wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
				printk("answer keydown\n");
#endif
			}
			answer_timer.expires = (jiffies + bdelay);
			add_timer (&answer_timer);
		}
		else if(answer_flag)	// keyup
		{
			del_timer(&answer_timer);
			answer_flag = 0;
			if(!FULL(buttonevent))
			{
				buttonevent.button_buffer[buttonevent.head] = ANSWER | KEYUP;
				buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
				wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
				printk("answer keyup\n");
#endif
			}
		}
	}
}

static void gpio2_gpio3_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int bdelay = BUTTON_DELAY;	/* The delay, in jiffies */

#ifdef DEBUG
	printk("enter gpio2/gpio3 interrupt handler\n");
#endif
	del_timer (&gpio2_timer);
	gpio2_timer.expires = (jiffies + bdelay);
	add_timer (&gpio2_timer);
}

static void gpio5_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int bdelay = BUTTON_DELAY;	/* The delay, in jiffies */

#ifdef DEBUG
//	printk("enter gpio5 interrupt handler\n");
#endif
	del_timer (&gpio5_timer);
	gpio5_timer.expires = (jiffies + bdelay);
	add_timer (&gpio5_timer);
}

static void gpio10_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int bdelay = 10;	/* The delay, in jiffies */

#ifdef DEBUG
//	printk("enter gpio10 interrupt handler\n");
#endif
	del_timer (&gpio10_timer);
	gpio10_timer.expires = (jiffies + bdelay);
	add_timer (&gpio10_timer);
}

void answer_button_handler(int irq, void *dev_id, struct pt_regs *regs)
{

	int bdelay = BUTTON_DELAY;	/* The delay, in jiffies */

#ifdef DEBUG
//	printk("enter answer interrupt handler\n");
#endif
	del_timer (&answer_timer);
	answer_timer.expires = (jiffies + bdelay);
	add_timer (&answer_timer);

}

void headset_in_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if(!FULL(buttonevent))
	{	
		buttonevent.button_buffer[buttonevent.head] = HEADSET_INT | KEYDOWN;
		buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
		wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
		printk("headset insert\n");
#endif
	}
}


/*add by jia for cebus*/
#ifdef CEBUS_INT_HNDL

void cebus_int_hndler(int irq, void *dev_id, struct pt_regs *regs)
{
     u32 tempGpdr0,tempGpdr1;
     u16 tempDevice,tempDumb;
     u32 tempGafr0_l,tempGafr1_l;
#ifdef CEBUS_DEBUG
     u8 temp[100];
     sprintf(temp,"change cebus device %d!",cebus_current_device);
     pcap_log_add_data(temp,strlen(temp));
#endif
     switch(GPLR0&(CEBUS_OPTION1|CEBUS_OPTION2))
     {
         case (OPTION1|OPTION2):
             if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V))
             {
                 tempDevice = CEBUS_PC_POWERED_USB;
             }
             else
             {
                 tempDevice = CEBUS_NONE_DEVICE;
             }
             break;  
         case CEBUS_OPTION1:
             if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_audio_in_status())
             {
                 /*  dumb accessory */
                 tempGpdr0 = GPDR0;
                 tempGpdr1 = GPDR1;
                 GPDR0 &= ~CEBUS_ASEL1;
                 GPDR1 &= ~(CEBUS_ASEL0|CEBUS_ASEL2);
                 tempGafr0_l = GAFR0_L;
                 tempGafr1_l = GAFR1_L;
                 GAFR0_L    &= 0xfffcffff; 
                 GAFR1_L    &= 0xffffc3ff;
                 udelay(1000);
                 tempDumb = 0;
                 if(GPLR1&CEBUS_ASEL0)
                 {
                     tempDumb += 1;
                 }

                 if(GPLR0&CEBUS_ASEL1)
                 {
                     tempDumb += 2;
                 }

                 if(GPLR1&CEBUS_ASEL2)
                 {
                     tempDumb += 4;
                 }

                 tempDevice = (u16)(CEBUS_DUMB_TTY+tempDumb);
                 GAFR0_L = tempGafr0_l;
                 GAFR1_L = tempGafr1_l;    
                 GPDR0 =  tempGpdr0;
                 GPDR1 =  tempGpdr1;                            
             }
             else
             {
                 tempDevice = CEBUS_PHONE_POWERED_USB;                 
             }
                             
             break;  
         case CEBUS_OPTION2:
             /* UART cable  */
             if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_audio_in_status())
             {
                 tempDevice = CEBUS_RS232_8_WIRES;                 
             }
             else
             {
                 tempDevice = CEBUS_RS232_4_WIRES;                 
             }
             break;  
         default:
             tempDevice = CEBUS_PUSHTOTALK_PTT;                 
#ifdef CONFIG_CONTEXT_CAP
		/* PTT can be used to trigger current context capture */ 
		panic_trig("Manual trigger: context capture\n", regs, 0);
#endif
             break;  

     }
     if(cebus_current_device != tempDevice)
     {
         destroy_cebus_device(cebus_current_device);
         cebus_current_device = tempDevice;
         create_cebus_device(cebus_current_device);
         if(!FULL(buttonevent))
         {
             buttonevent.button_buffer[buttonevent.head] = (u16)(CEBUS_BUTTON|cebus_current_device);
             buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
             wake_up_interruptible(&button_wait_queue);
             printk("cebus state changes in int handler.\n");
         }
#ifdef CONFIG_PM
	if (cebus_current_device == CEBUS_NONE_DEVICE)
		queue_apm_event(KRNL_ACCS_DETACH, NULL);
	else
		queue_apm_event(KRNL_ACCS_ATTACH, NULL);
#endif
     }

#if 0    
/*for test to carkit*/
    if (0x10 == len)
    {
//	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);	
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
    }	
#endif
} 
#endif

void headset_out_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if(!FULL(buttonevent))
	{	
		buttonevent.button_buffer[buttonevent.head] = HEADSET_INT | KEYUP;
		buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
		wake_up_interruptible(&button_wait_queue);
#ifdef DEBUG
		printk("headset remove\n");
#endif
	}
}

#ifdef CONFIG_PM
static int button_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch(req)
	{
		case PM_SUSPEND:	
			break;

		case PM_RESUME:
                    /* handle CEBUS wakeup case when no CEBUS device */
                    if (cebus_current_device == CEBUS_NONE_DEVICE)
                    {
                        cebus_none_device_gpio_init();
                    }
			break;
	}
	return 0;
}
#endif

static int count=0;
static int button_release(struct inode * inode, struct file * file)
{
	int i;
	
	count --;
	if(count)
	{
		return -EBUSY;
	}

	free_irq(IRQ_GPIO(10), NULL);
	free_irq(IRQ_GPIO(5), NULL);
	free_irq(IRQ_GPIO(3), NULL);
	free_irq(IRQ_GPIO(2), NULL);

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);	/* clear interrupt */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);	/* mask interrupt */

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_A1I);	/* clear interrupt */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_A1M);	/* mask interrupt */

	// init gpio40 input
 	GPDR1 &= ~0x00000100;
	// init gpio2, gpio3 input
	GPDR0 &= 0xFFFFFFF3;
	// init gpio52, gpio54, gpio55 input
	GPDR1 &= ~0x00D00000;
	// init gpio5 input, 
	GPDR0 &= 0xFFFFFFDF;
	// init gpio10 input, 
	GPDR0 &= 0xFFFFFbFF;

	del_timer (&gpio2_timer);
	del_timer (&gpio5_timer);
	del_timer (&gpio10_timer);
	del_timer (&answer_timer);

    /*add by jia for cebus*/
#ifdef CEBUS_INT_HNDL
/*
  	remove_proc_entry(CEBus_proc.name,CEBus_dir);
  	CEBus_dir=NULL;
*/
        if(cebus_is_open)
        {
	    free_irq(IRQ_GPIO(6), NULL);
	    free_irq(IRQ_GPIO(7), NULL);
            cebus_is_open = 0;
        }
#endif


	for(i=0; i<NR_BUTTONS; i++)
		buttonevent.button_buffer[i] = 0;
	buttonevent.head = 0;
	buttonevent.tail = 0;

#ifdef CONFIG_PM
	pm_unregister(pm_dev);
#endif
#ifdef DEBUG
	printk("button device released\n");
#endif
    	
	return 0;
}

static int button_open(struct inode * inode, struct file * file)
{
	int i, data;

        if(count)
        {	
		return -EBUSY;
        }

	count ++;
	for(i=0; i<NR_BUTTONS; i++)
		buttonevent.button_buffer[i] = 0;
	buttonevent.head = 0;
	buttonevent.tail = 0;

	init_timer (&gpio2_timer);
	gpio2_timer.function = scan_gpio2_gpio3_button;

	init_timer (&gpio5_timer);
	gpio5_timer.function = scan_gpio5_button;

	init_timer (&gpio10_timer);
	gpio10_timer.function = scan_gpio10_button;
	
	init_timer (&answer_timer);
	answer_timer.function = scan_answer_button;

	// set gpio2,3,5,10
	GAFR0_L &= 0xFFCFF30F;
	// set gpio40,52,54,55
	GAFR1_L &= 0xFFFCFFFF;
	GAFR1_U &= 0xFFFF0CFF;

	// init gpio2, gpio3 input
	GPDR0 &= 0xFFFFFFF3;
	restore_init_status();
	// init gpio5 input
	GPDR0 &= 0xFFFFFFDF;
	// init gpio10 input
	GPDR0 &= 0xFFFFFBFF;

	udelay(10);
	// keep old flip status;
	flip_flag = GPLR0 & 0x400;
	
   
    /*add by jia for cebus*/
#ifdef CEBUS_INT_HNDL
/*
        CEBus_dir=&proc_root;
        create_proc_info_entry(CEBus_proc.name,0,CEBus_dir,CEBus_proc.fn);
*/
        set_GPIO_IRQ_edge(6, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
        set_GPIO_IRQ_edge(7, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
        if (request_irq (IRQ_GPIO(6), cebus_int_hndler, SA_INTERRUPT, "gpio6", NULL)) 
        {
            printk (KERN_WARNING "gpio6 irq is free.\n");
            return -EIO;
        }  
        if (request_irq (IRQ_GPIO(7), cebus_int_hndler, SA_INTERRUPT, "gpio7", NULL)) 
        {
            printk( KERN_WARNING "gpio7 irq is free.\n");
            return -EIO;
        }
        cebus_is_open = 1;
#endif

	// enable gpio2, gpio3, gpio5, gpio10 falling and rising interrupt
	set_GPIO_IRQ_edge(2, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(3, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(5, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(10, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);


#ifdef DEBUG
	printk("GRER0=0x%lx\n", GRER0);
	printk("GFER0=0x%lx\n", GFER0);
#endif
	if (request_irq (IRQ_GPIO(2), gpio2_gpio3_handler, SA_INTERRUPT,"gpio2", NULL )) {
		printk (KERN_WARNING "button: IRQ %d is not free.\n",IRQ_GPIO(2));
		return -EIO;
	}
	if (request_irq (IRQ_GPIO(3), gpio2_gpio3_handler, SA_INTERRUPT,"gpio3", NULL)) {
		printk (KERN_WARNING "button: IRQ %d is not free.\n",IRQ_GPIO(3));
		return -EIO;
	}
	if (request_irq (IRQ_GPIO(5), gpio5_handler, SA_INTERRUPT,"gpio5", NULL)) {
		printk (KERN_WARNING "button: IRQ %d is not free.\n",IRQ_GPIO(5));
		return -EIO;
	}
	if (request_irq (IRQ_GPIO(10), gpio10_handler, SA_INTERRUPT,"gpio10", NULL)) {
		printk (KERN_WARNING "button: IRQ %d is not free.\n",IRQ_GPIO(10));
		return -EIO;
	}

	ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_A1I);	/* clear interrupt */
	udelay(10);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_A1M);	/* unmask interrupt */
	data = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
	if(!data)
	{
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);	
		udelay(10);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);	
	}
#ifdef CONFIG_PM
	pm_dev = pm_register(PM_SYS_DEV, 0, button_pm_callback);

	if (cebus_current_device == CEBUS_NONE_DEVICE)
		queue_apm_event(KRNL_ACCS_DETACH, NULL);
	else
             queue_apm_event(KRNL_ACCS_ATTACH, NULL);

#endif
        if((cebus_current_device != CEBUS_NONE_DEVICE)&&(!FULL(buttonevent)))
        {
            buttonevent.button_buffer[buttonevent.head] = (u16)(CEBUS_BUTTON|cebus_current_device);
            buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
            wake_up_interruptible(&button_wait_queue);
        }

#ifdef DEBUG
	printk("button device opened\n");
#endif

	return 0;
}


/*add by jia for cebus*/
/*
int CEBus_proc_read(char *buffer,char **buffer_location,off_t offset,int buffer_length)
{
	len=sprintf(buffer,"CEBUS Type :  %d \n",cebus_current_device); 
	return len;
}
*/

/* add button_ioctl to check flip status */
static struct file_operations button_fops = {
	owner:		THIS_MODULE,
	open:		button_open,
	release:	button_release,
	read:		button_read,
	poll:		button_poll,
        ioctl:		button_ioctl,
};

/* 
 * This structure is the misc device structure, which specifies the minor
 * device number (158 in this case), the name of the device (for /proc/misc),
 * and the address of the above file operations structure.
 */

static struct miscdevice button_misc_device = {
	BUTTON_MINOR,
	"button",
	&button_fops,
};

/*
 * This function is called to initialise the driver, either from misc.c at
 * bootup if the driver is compiled into the kernel, or from init_module
 * below at module insert time. It attempts to register the device node
 * and the IRQ and fails with a warning message if either fails, though
 * neither ever should because the device number and IRQ are unique to
 * this driver.
 */

static int __init button_init(void)
{
	if (misc_register (&button_misc_device)) {
		printk (KERN_WARNING "button: Couldn't register device 10, "
				"%d.\n", BUTTON_MINOR);
		return -EBUSY;
	}
#ifdef DEBUG
	printk("button device initialized\n");
#endif

        /*add by jia for cebus*/
#ifdef CEBUS_INT_HNDL
 	GAFR0_L &= 0xffff0fff;
  	GPDR0   &= ~(OPTION1 | OPTION2);		
        cebus_none_device_gpio_init();
        cebus_int_hndler(0,NULL,NULL);
#endif
/*
	CEBus_dir=&proc_root;
	create_proc_info_entry(CEBus_proc.name,0,CEBus_dir,CEBus_proc.fn);
*/
	return 0;
}

static void __exit button_exit (void) 
{
	misc_deregister(&button_misc_device);
#ifdef DEBUG
	printk("button device exit\n");
#endif

    /*add by jia for cebus*/
/*  	remove_proc_entry(CEBus_proc.name,CEBus_dir);
  	CEBus_dir=NULL;
*/

}

module_init(button_init);
module_exit(button_exit);
