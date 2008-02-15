/*
 * linux/driver/char/mstone_keypad.c
 * Keypad driver for Intel Mainstone development board
 *
 * Copyright (C) 2003, Intel Corporation (yu.tang@intel.com)
 * Copyright 2003 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *	   source@mvista.com
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/sem.h>
#include <linux/miscdevice.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/hardirq.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>

#include <asm/arch/keypad.h>

/* Direct-KEY scan-code for Mainstone-I Board */
#define ROTARY_DEFAULT			0x7F
#define NO_KEY				0xFF
#define SCAN_CODE_SCROLL_UP		0xA
#define SCAN_CODE_SCROLL_DOWN		0xB
#define SCAN_CODE_ACTION		0xC

DECLARE_MUTEX(kpdrv_mutex);
static struct kpe_queue kpe_queue;
static int kpdrv_refcount = 0;

//#define DEBUG 1
#if DEBUG
static unsigned int mstone_keypad_dbg = 1;
#else
#define mstone_keypad_dbg   0
#endif

#ifdef CONFIG_DPM
#include <linux/device.h>

static int pxakpd_suspend(struct device *dev, u32 state, u32 level);
static int pxakpd_resume(struct device *dev, u32 level);
static int pxakpd_scale(struct bus_op_point *op, u32 level);

static struct device_driver pxakpd_driver_ldm = {
	name:          "pxa-kpd",
	devclass:      NULL,
	probe:         NULL,
	suspend:       pxakpd_suspend,
	resume:        pxakpd_resume,
	scale:         pxakpd_scale,
	remove:        NULL,
	constraints:   NULL,
};

static struct device pxakpd_device_ldm = {
	name:         "PXA Keypad",
	bus_id:       "pxakpd",
	driver:       NULL,
	power_state:  DPM_POWER_ON,
};

static void
pxakpd_ldm_register(void)
{
	extern void pxaopb_driver_register(struct device_driver *driver);
	extern void pxaopb_device_register(struct device *device);
	
	pxaopb_driver_register(&pxakpd_driver_ldm);
	pxaopb_device_register(&pxakpd_device_ldm);
}

static void
pxakpd_ldm_unregister(void)
{
	extern void pxaopb_driver_unregister(struct device_driver *driver);
	extern void pxaopb_device_unregister(struct device *device);
	
	pxaopb_device_unregister(&pxakpd_device_ldm);
	pxaopb_driver_unregister(&pxakpd_driver_ldm);
}

static int
pxakpd_resume(struct device *dev, u32 level)
{
	if (mstone_keypad_dbg)
	  printk("+++: in pxakpd_resume()\n");

	switch (level) {
	case RESUME_POWER_ON:
		/* enable direct and matrix interrupts */
		KPC |= (KPC_DIE | KPC_MIE);
		
		/* enable clock to keypad */
		CKEN |= CKEN19_KEYPAD;
        	{
	            struct kp_event kpe;
	            unsigned char c;
	
	            kpe.flags |= (KP_DIRECT | KP_MATRIX );
	            c = get_scancode(&kpe);
	            if (c != NO_KEY) {
	               /*  Insert it into key event list. */
	                kpq_put(&kpe);
	            }
	           else {
	               /*  We are not woke by key press. */
	           }
	        }
	           break;
	}
	
	return 0;
}

static int
pxakpd_suspend(struct device *dev, u32 state, u32 level)
{
	if (mstone_keypad_dbg)
	  printk("+++: in pxakpd_suspend()\n");

	switch (level) {
	case SUSPEND_POWER_DOWN:
		/* disable clock to keypad */
		CKEN &= ~CKEN19_KEYPAD;
		
		/* disable direct and matrix interrupts */
		KPC &= ~(KPC_DIE | KPC_MIE);
		break;
	}
	
	return 0;
}

static int
pxakpd_scale(struct bus_op_point *op, u32 level)
{
	printk("+++: in pxakpd_scale()\n");
	
	return 0;
}
#endif /* CONFIG_DPM */

/* Init kpe queue */
static void kpq_init(void)
{
	kpe_queue.head = kpe_queue.tail = kpe_queue.len = 0;
	kpe_queue.spinlock = SPIN_LOCK_UNLOCKED;
	init_waitqueue_head(&kpe_queue.waitq);
}

static int kpq_empty(void)
{
	int flags, empty;

	spin_lock_irqsave(&kpe_queue.spinlock,flags);
	empty = kpe_queue.len ? 0 : 1;
	spin_unlock_irqrestore(&kpe_queue.spinlock, flags);

	return empty;
}

static int kpq_get(struct kp_event *kpe)
{
	int flags, err = 0;

	spin_lock_irqsave(&kpe_queue.spinlock, flags);
	if (kpe_queue.head == kpe_queue.tail) {
		printk(KERN_ERR "keypad: empty event queue, looks bad\n");
		err = -EAGAIN;
		goto out;
	}
	memcpy(kpe, kpe_queue.kpes + kpe_queue.tail, sizeof(struct kp_event));
	kpe_queue.len--;
	kpe_queue.tail = (kpe_queue.tail + 1) % MAX_KPES;
 out:
	spin_unlock_irqrestore(&kpe_queue.spinlock, flags);
	return err;
}

static void kpq_put(struct kp_event *kpe)
{
	int flags;
	
	spin_lock_irqsave(&kpe_queue.spinlock, flags);
	memcpy(kpe_queue.kpes + kpe_queue.head, kpe, sizeof(struct kp_event));
	kpe_queue.len++;
	if (kpe_queue.len == MAX_KPES) {
		printk(KERN_ERR "keypad: events are comes too fast, will discard next\n");
		kpe_queue.len--;
		goto out;
	}
	kpe_queue.head = (kpe_queue.head + 1) % MAX_KPES;
 out:
	spin_unlock_irqrestore(&kpe_queue.spinlock, flags);
	/* Wake up the waiting processes */
	wake_up_interruptible(&kpe_queue.waitq);
}

static int kp_direct_scan(struct kp_event *kpe)
{
	kpe->flags 	|= KP_DIRECT;
	kpe->direct 	= KPDK;
	kpe->rotary	= KPREC;
	return 0;
}

static int kp_matrix_scan(struct kp_event *kpe)
{
	KPC |= KPC_AS;
	while (KPAS &KPAS_SO);

	kpe->flags 	|= KP_MATRIX;
	kpe->matrix[0] 	= KPAS;
	kpe->matrix[1]  = KPASMKP0;
	kpe->matrix[2]	= KPASMKP1;
	kpe->matrix[3]	= KPASMKP2;
	kpe->matrix[4]	= KPASMKP3;

	KPC &= ~KPC_AS;

	return 0;
}

/* Interrupt kandler for keypad */
static void kp_interrupt(int irq, void *ptr, struct pt_regs *regs) 
{
	struct kp_event kpe = {0};
	int flags;
	unsigned long kpc_val;

	printk("+++: kp_interrupt()\n");

	spin_lock_irqsave(&kpe_queue.spinlock,flags);

	/* ACK interrupt */
	kpc_val = KPC;
	kpe.jiffies = jiffies;

	/* Direct interrupt */
	if (kpc_val & KPC_DI) 
		kp_direct_scan(&kpe);

	/* Matrix interrupt */
	if (kpc_val & KPC_MI) 
		kp_matrix_scan(&kpe);

	spin_unlock_irqrestore(&kpe_queue.spinlock, flags);
	
	if (((kpe.flags & KP_DIRECT) || (kpe.flags & KP_MATRIX)) && kpdrv_refcount) 
		kpq_put(&kpe);
}

/* Wait for keypad event, must be in task-context */
int kp_wait(struct kp_event *kpe, signed int timeout)
{
	int n, err = 0;

	while(1) {
		if (!kpq_empty()) {
			n = kpq_get(kpe);
			if (n < 0)
				continue;
			break;
		}
	
		/* No event available ? */
		if (!timeout) {
			err = -EAGAIN;
			break;
		}

		if (timeout > 0) {
			/* Wait for timeout */
			timeout = interruptible_sleep_on_timeout(&kpe_queue.waitq, timeout);
			continue;
		}
		else{
			/* Wait for kpevent/signal */
			while(1) {
				interruptible_sleep_on(&kpe_queue.waitq);
				if (kpq_empty() && !signal_pending(current))
					continue;
				break;
			}

			if (signal_pending(current)) {
				err = -EINTR;
				break;
			}
		}
	}
	return err;
}

/* Poll for keypad event */
unsigned int kp_poll(struct file *file, poll_table *wait) 
{
	poll_wait(file, &kpe_queue.waitq, wait);
	return kpq_empty() ? 0 : POLLIN | POLLRDNORM;
}

static unsigned char get_scancode(struct kp_event *kpe)
{
	static unsigned int curr, prev = ROTARY_DEFAULT;
	unsigned int c;
	
	c = NO_KEY;

	if (kpe->flags & KP_DIRECT) {

		curr = kpe->rotary & 0xFF;

		if (kpe->rotary & KPREC_OF0) {
			KPREC 	&= ~KPREC_OF0;
			KPREC	|= ROTARY_DEFAULT;
			prev 	= ROTARY_DEFAULT;
			c 	= SCAN_CODE_SCROLL_UP;
		}
		else if (kpe->rotary & KPREC_UF0) {
			KPREC 	&= ~KPREC_UF0;
			KPREC	|= ROTARY_DEFAULT;
			prev 	= ROTARY_DEFAULT;
			c 	= SCAN_CODE_SCROLL_DOWN;
		}
		else if (curr > prev) {
			c 	= SCAN_CODE_SCROLL_UP;
			prev	= curr;
		}
		else if (curr < prev) {
			c	= SCAN_CODE_SCROLL_DOWN;
			prev	= curr;
		}
		else if (kpe->direct & KPDK_DK2) {
			c	= SCAN_CODE_ACTION;
			prev	= curr;
		}
	} 

	if (kpe->flags & KP_MATRIX) {
		unsigned int count = (kpe->matrix[0] >> 26) & 0x1f;

		c = kpe->matrix[0] & 0xff;
		
		/* Key up */
		if (!count) c |= 0x80; 
	}

	return c;
}

static ssize_t kpdrv_read(struct file * filp, char * buf,
			size_t count, loff_t *ppos)
{
	struct kp_event kpe;
	int err,left,timeout;
	unsigned char c;

	timeout = 0;
	left 	= count;
	do {
		err = kp_wait(&kpe, timeout);
		if (err) {
			if ( left != count ) return (count-left);
			
			if (filp->f_flags & O_NONBLOCK) return err;

			if (timeout) return err;

			timeout = -1;
			continue;
		}
		/* Parse if direct/matrix flags are set*/
		if ( (kpe.flags & KP_DIRECT) || (kpe.flags & KP_MATRIX) ) {
			c = get_scancode(&kpe);
			if (c != NO_KEY) {
				put_user(c, buf++);
				left --;
			}
		}
	} while (left);
	return (count-left);
}

static int kpdrv_ioctl(struct inode *ip, struct file *fp, 
			     unsigned int cmd, unsigned long arg) 
{
	int interval = KPKDI & 0xffff;
	int ign	= (KPC & KPC_IMKP);

	switch(cmd) {
	case KPIOGET_INTERVAL:
		if (copy_to_user((void*)arg, &interval, sizeof(int)))
			return -EFAULT;
		break;
	case KPIOGET_IGNMULTI:
		if (copy_to_user((void*)arg, &ign, sizeof(int)))
			return -EFAULT;
		break;
	case KPIOSET_INTERVAL:
		if (copy_from_user(&interval, (void*)arg, sizeof(int)))
			return -EFAULT;
		
		/* FIXME : sighting #34833, Matrix Key interval should be >10ms */
		if ( (interval & 0xff) < 10) return -EINVAL;

		KPKDI &= ~0xffff;
		KPKDI |= (interval & 0xffff);
		break;
	case KPIOSET_IGNMULTI:
		if (copy_from_user(&ign, (void*)arg, sizeof(int))) 
			return -EFAULT;

		if (ign) 
			KPC |= KPC_IMKP;
		else
			KPC &= ~KPC_IMKP;
		
		break;
		
	default:
		/* Unknown command */
		return -EINVAL;
	}
	return 0;
}

static unsigned int kpdrv_poll(struct file *filp, poll_table * wait) {
	return kp_poll(filp, wait);
}

static int kpdrv_open(struct inode *inode, struct file *filp)
{
	down(&kpdrv_mutex);
	if (!kpdrv_refcount)
		kpq_init();
	kpdrv_refcount++;
	up(&kpdrv_mutex);
	return 0;
}

static int kpdrv_close(struct inode *inode, struct file *filp)
{
	down(&kpdrv_mutex);
	kpdrv_refcount--;
	up(&kpdrv_mutex);

	return 0;
}

static struct file_operations kpdrv_fops = {
	owner:		THIS_MODULE,
	open:		kpdrv_open,
	release:	kpdrv_close,
	read:		kpdrv_read,
	ioctl:		kpdrv_ioctl,
	poll:		kpdrv_poll,
};


static struct miscdevice keypad_miscdevice = {
	minor:          MISC_DYNAMIC_MINOR,
	name:           "keypad",
	fops:           &kpdrv_fops
};

static int __init kpdrv_init(void)
{
	int err;

	/* Setup gpio */
	set_GPIO_mode(93 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(94 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(95 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(96 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(97 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(98 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(99 | GPIO_ALT_FN_1_IN);

	set_GPIO_mode(100 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(101 | GPIO_ALT_FN_1_IN);
	set_GPIO_mode(102 | GPIO_ALT_FN_1_IN);

	set_GPIO_mode(103 | GPIO_ALT_FN_2_OUT);
	set_GPIO_mode(104 | GPIO_ALT_FN_2_OUT);
	set_GPIO_mode(105 | GPIO_ALT_FN_2_OUT);
	set_GPIO_mode(106 | GPIO_ALT_FN_2_OUT);
	set_GPIO_mode(107 | GPIO_ALT_FN_2_OUT);
	set_GPIO_mode(108 | GPIO_ALT_FN_2_OUT);

	/* Set keypad control register */
	KPC = (KPC_ASACT | (3<<26) | (4<<23)|
	       KPC_ME | (2<<6) | KPC_DEE0 | KPC_DE | 
	       KPC_MS_ALL | KPC_MIE | KPC_DIE | KPC_IMKP);

	/* Set debouce time seperately for Matrix/Direct. */
    	KPKDI= 0x010A;    

	/* Set scroll wheel value to mid-point value */
	KPREC= 0x7F;

	/* Enable unit clock */
	CKEN |= CKEN19_KEYPAD;

	/* Request keypad IRQ */
	err = request_irq(IRQ_KEYPAD, kp_interrupt, 0, "keypad", NULL);
	if (err) {
		printk (KERN_CRIT "can't register IRQ%d for keypad, error %d\n",
			IRQ_KEYPAD, err);
		/* Disable clock unit */
		CKEN &= ~CKEN19_KEYPAD;
		return -ENODEV;
	}

	/* Register driver as miscdev */
	err = misc_register(&keypad_miscdevice);
	if (err) 
		printk(KERN_ERR "can't register keypad misc device, error %d\n", err);
	else
		printk(KERN_INFO "Intel Mainstone Keypad driver registered with minor %d\n",
		       keypad_miscdevice.minor);
	
#ifdef CONFIG_DPM
	pxakpd_ldm_register();
#endif /* CONFIG_DPM */ 
	
	return err;
}

static void __exit kpdrv_exit(void)
{
#ifdef CONFIG_DPM
	pxakpd_ldm_unregister();
#endif /* CONFIG_DPM */
	
	/* Disable clock unit */
	CKEN &= ~CKEN19_KEYPAD;
	
	/* Free keypad IRQ */
	free_irq(IRQ_KEYPAD, NULL);

	/* Unregister keypad device */
	misc_deregister(&keypad_miscdevice);
}

module_init(kpdrv_init);
module_exit(kpdrv_exit);

MODULE_AUTHOR("Yu Tang <yu.tang@intel.com>, source@mvista.com");
MODULE_DESCRIPTION("Intel Mainstone KEYPAD driver");
MODULE_LICENSE("GPL");
