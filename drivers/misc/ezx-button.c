/*
 * linux/drivers/misc/ezx-button.c
 *
 * Support for the Motorola Ezx A780 Development Platform.
 * 
 * Copyright (C) 2003-2004 Motorola
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
 * 
 * Dec 16,2003 - (Motorola) Created
 * Mar 15,2004 - (Motorola) Headset judgement condition reverse
 * Apr 13,2004 - (Motorola) Reorganise file header
 * 
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
#include "ssp_pcap.h"
#include "ezx-button.h"

#define DEBUG 
#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

#define FULL(queue)     ((queue.head+1==queue.tail)||(queue.head+1-NR_BUTTONS==queue.tail))

struct button_queue{
	unsigned short button_buffer[NR_BUTTONS];
	unsigned int head;
	unsigned int tail;
};

static struct timer_list lockscreen_timer;	
static struct timer_list answer_timer;	

static DECLARE_WAIT_QUEUE_HEAD(button_wait_queue); /* Used for blocking read */
static struct button_queue    buttonevent;

static int answer_flag=0;
int lockscreen_flag=0;

static int first_open=0;
static int before_emu=0;
static int before_charge_500mA=0;
static int before_charge_0mA=0;

void usb_cable_event_handler(unsigned short cable_event)
{
        if(!FULL(buttonevent))
        {
	        buttonevent.button_buffer[buttonevent.head] = cable_event;
                buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
                wake_up_interruptible(&button_wait_queue);
                printk("usb cable event = %d\n", cable_event);
        }
}

static int button_ioctl(struct inode *inode, struct  file *file, uint cmd, ulong arg)
{
  	int data, ret;
 
	switch(cmd){
/*		
*/
	case BUTTON_GET_HEADSETSTATUS:
		data = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
		if(data)
			ret = HEADSET_IN;
		else
			ret = HEADSET_OUT;
		return put_user(ret, (int *)arg);     
/*
*/
	case BUTTON_GET_FLIPSTATUS:
		data = GPLR(GPIO_FLIP_PIN) & GPIO_bit(GPIO_FLIP_PIN);
        	if(!data)
	            	ret = FLIP_OFF;
            	else
	                ret = FLIP_ON;
		lockscreen_flag = ret;
            	return put_user(ret, (int *)arg);
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

	if (buttonevent.head == buttonevent.tail) 
	{
		add_wait_queue(&button_wait_queue, &wait);
		current->state = TASK_INTERRUPTIBLE;

		while (buttonevent.head == buttonevent.tail) 
		{
			if (file->f_flags & O_NONBLOCK) 
			{
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) 
			{
   			    retval = -ERESTARTSYS;
				break;
			}
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

	return retval;	
}

static void scan_lockscreen_button(unsigned long parameters)
{
	unsigned long data;
	int bdelay = BUTTON_INTERVAL;	

#ifdef DEBUG
	printk("enter scan_lockscreen_button\n");
#endif
	data = GPLR(GPIO_FLIP_PIN);
	if(!(data & GPIO_bit(GPIO_FLIP_PIN)))	// lockscreen - 0
	{
	   	lockscreen_flag = 0;
	        if(!FULL(buttonevent))
		{ 
			buttonevent.button_buffer[buttonevent.head] = FLIP | KEYDOWN;
			buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
#ifdef CONFIG_KEYPAD_E680			
			ssp_pcap_screenlock_lock(0);
#endif			
			wake_up_interruptible(&button_wait_queue);
#ifdef CONFIG_APM
			queue_apm_event(KRNL_FLIP_OFF, NULL);
#endif
 
#ifdef DEBUG
			printk("LockScreen keydown\n");
#endif
		}
	}
	else 	// lockscreen - 1
	{
		lockscreen_flag = 1;
		if(!FULL(buttonevent))
		{
		 	buttonevent.button_buffer[buttonevent.head] = FLIP | KEYUP;
			buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
#ifdef CONFIG_KEYPAD_E680			
			ssp_pcap_screenlock_unlock(0);
#endif			
			wake_up_interruptible(&button_wait_queue);
#ifdef CONFIG_APM
			queue_apm_event(KRNL_FLIP_ON, NULL);
#endif
			
#ifdef DEBUG
			printk("LockScreen keyup\n");
#endif
		}
	}
}

static void scan_answer_button(unsigned long parameters)
{
	int ssp_pcap_bit_status;	
	int bdelay = BUTTON_INTERVAL;	

#ifdef DEBUG
	printk("enter scan_answer_button\n");
#endif
	/* read pcap register to check if headjack is in */
//        ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
//        if( ssp_pcap_bit_status != 0 )  /* headjack is in */
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

static void lockscreen_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int bdelay = 10;	/* The delay, in jiffies */

	del_timer (&lockscreen_timer);
	lockscreen_timer.expires = (jiffies + bdelay);
	add_timer (&lockscreen_timer);
}

void answer_button_handler(int irq, void *dev_id, struct pt_regs *regs)
{
#if 0
	int bdelay = BUTTON_DELAY;	/* The delay, in jiffies */

	del_timer (&answer_timer);
	answer_timer.expires = (jiffies + bdelay);
	add_timer (&answer_timer);
	printk("answer_button_handler\n");
#endif	
	scan_answer_button(0);
}

void headset_in_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if(!FULL(buttonevent))
	{	
		buttonevent.button_buffer[buttonevent.head] = HEADSET_INT | KEYDOWN;
		buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
		wake_up_interruptible(&button_wait_queue);
		printk("headset button insert\n");
		answer_flag = 0;
	}
}


void headset_out_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if(!FULL(buttonevent))
	{	
		buttonevent.button_buffer[buttonevent.head] = HEADSET_INT | KEYUP;
		buttonevent.head = (buttonevent.head+1)%(NR_BUTTONS);
		wake_up_interruptible(&button_wait_queue);
		printk("headset button remove\n");
		answer_flag = 0;
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

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);	/* clear interrupt */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);	/* mask interrupt */

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_A1I);	/* clear interrupt */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_A1M);	/* mask interrupt */

	// init gpio12 input, 
//	GPDR(GPIO_LOCK_SCREEN_PIN) &= 0xFFFFEFFF;
    	GPDR(GPIO_FLIP_PIN) &= ~(GPIO_bit(GPIO_FLIP_PIN));
	del_timer (&lockscreen_timer);
	del_timer (&answer_timer);
	free_irq(IRQ_GPIO(GPIO_FLIP_PIN), NULL);

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

	init_timer (&lockscreen_timer);
	lockscreen_timer.function = scan_lockscreen_button;
	init_timer (&answer_timer);
	answer_timer.function = scan_answer_button;
        GAFR(GPIO_FLIP_PIN) &= ~(3<<(2*(GPIO_bit(GPIO_FLIP_PIN) & 0xf)));
        GPDR(GPIO_FLIP_PIN) |= GPIO_bit(GPIO_FLIP_PIN);
        udelay(10);
        set_GPIO_IRQ_edge(GPIO_FLIP_PIN, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
        if (request_irq (IRQ_GPIO(GPIO_FLIP_PIN), lockscreen_handler, SA_INTERRUPT, "flip", NULL))
        {
            printk (KERN_WARNING "flip irq is free.\n");
            return -EIO;
	}
						
	ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_A1I);	/* clear interrupt */
	udelay(10);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_A1M);	/* unmask interrupt */
	data = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
	if(data)
	{
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);	
		udelay(10);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);	
	}
#ifdef CONFIG_PM
	pm_dev = pm_register(PM_SYS_DEV, 0, button_pm_callback);
#endif

#ifdef DEBUG
	printk("button device opened\n");
#endif
	
#ifdef CONFIG_KEYPAD_E680
	data = GPLR(GPIO_FLIP_PIN) & GPIO_bit(GPIO_FLIP_PIN);
	if(!data)
		ssp_pcap_screenlock_lock(0);
#endif		

	first_open = 1;
	return 0;
}

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
}

static void __exit button_exit (void) 
{
	misc_deregister(&button_misc_device);
}

module_init(button_init);
module_exit(button_exit);
