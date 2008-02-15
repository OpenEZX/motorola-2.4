/*
 * linux/arch/arm/mach-pxa/usb-dev.c, Motorola PST interfaces device driver
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



#include <linux/module.h>
#include <linux/config.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/cache.h>

#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/mach-types.h>
#include <asm/proc/page.h>

#include "pxa_usb.h"
#include "usb_ep0.h"

#define VERSION "0.54"

#define USBC_MINOR0 17
#define USBC_MINOR1 18
#define USBC_MINOR2 19

/*
 * PXAUSBDEBUG macro. 
 * 1 --- debug mode
 * 0 --- quiet  mode 
 */
#define PXAUSBDEBUG 0

/* Here, args list can't be empty */
#ifdef PXAUSBDEBUG
#     define PRINTKD(fmt, args...) printk(KERN_DEBUG "DEBUG %s: " fmt "\n", __FUNCTION__, ##args)
#else
#     define PRINTKD(fmt, args...)
#endif

static const char pszMe[] = "USB-dev: ";
static int usbi0_ref_count=0, usbi1_ref_count=0,usbi2_ref_count=0;

//lw
int polling_ready6=0, polling_ready8=0;
extern int pxa_usb_xmit(char *buf, int len);
extern wait_queue_head_t wq_read6;
extern wait_queue_head_t wq_read8;
extern interface_t intf6, intf8;
extern polling_times6,polling_times8,polling_len;
extern specific_times6,specific_times8;

static int last_rx_result = 0;

#define TX_EP0_SIZE 16
#define RX_EP0_SIZE 16
#define RBUF_SIZE  (1*PAGE_SIZE)

static struct ep0_buf {
  char *buf;
  int in;
  int out;
} rx_ep0 = { NULL, 0, 0 };

/*
 * dump all data in the message list for debug.
 */
#ifdef PXAUSBDEBUG
static void dump_msg_list( struct list_head *list )
{
	struct list_head *pos;
	msg_node_t *node;
	int i;

	printk( KERN_DEBUG "%s dump_msg_list\n", pszMe);
	list_for_each( pos, list ) {
		node = list_entry ( pos, msg_node_t, list);
		if (!node) { 
			printk( KERN_DEBUG "a NULL node!");
		} else {
			for ( i=0; i<node->size; i++ ) {
				printk( KERN_DEBUG "0x%2.2x ", *(node->body+i));
			}
			printk( KERN_DEBUG "\n");
		}
	}
}
#else
static void dump_msg_list( list_head_t *list ) 
{
}
#endif 

static int
init_interface (interface_t * intf)
{
	intf->enable = 0;
	intf->list_len = 0;
	INIT_LIST_HEAD (&intf->in_msg_list);
	INIT_LIST_HEAD (&intf->out_msg_list);

	return 0;
}

static int
usbc_interface6c_open (struct inode *pInode, struct file *pFile)
{
  
	if (init_interface (&intf6) < 0) 
		return -1;

	if (usbi0_ref_count == 0)
		intf6.enable = 1;

	usbi0_ref_count++;
	MOD_INC_USE_COUNT;

	return 0;
}

static ssize_t
usbc_interface6c_read (struct file *pFile, char *pUserBuffer,
		      size_t stCount, loff_t * pPos)
{
	int retval,ret;
	struct list_head* ptr=NULL;
	msg_node_t *node;

	while (list_empty (&intf6.out_msg_list))
	{
		if (pFile->f_flags & O_NONBLOCK) 
			{
				printk ( KERN_INFO "no data in interface6 msg list now!\n");
				return -EAGAIN;
			}
		PRINTKD("\"%s\" reading: going to sleep\n", current->comm);
		interruptible_sleep_on(&wq_read6);
		if (signal_pending(current))
				return -ERESTARTSYS;
	}

	ptr = intf6.out_msg_list.next;
	node = list_entry (ptr, msg_node_t, list);
	ret = node->size;
	if (stCount < ret)
		return -EFAULT;		/* This error number is OK? */
	if ( copy_to_user (pUserBuffer, node->body, ret) ) 
	{
		printk ( KERN_INFO " copy_to_user() has some problems! node->size=%d\n", node->size);
		return 0;
	}
		
	list_del (ptr);
	kfree (node->body);
	kfree (node);
	return ret;
}

/*
  In this function, we need some critical section tests.
*/
static ssize_t
usbc_interface6c_write (struct file *pFile, const char *pUserBuffer,
		       size_t stCount, loff_t * pPos)
{
	u8 *data;
	msg_node_t *node;

	if (stCount == 0)
		return 0;

	data = (u8 *) kmalloc (stCount, GFP_KERNEL);
	if (!data)
	{
		printk( KERN_INFO " kmalloc(%d) out of memory!\n", stCount);
		return -EFAULT;
	}

	if ( copy_from_user (data, pUserBuffer, stCount) )
	{
		kfree(data);
		printk( KERN_INFO "copy_from_user() failed.\n" );
		return -EFAULT;
	}

	node = (msg_node_t *) kmalloc (sizeof (msg_node_t), GFP_KERNEL);
	if (!node)
	{
		printk( KERN_INFO "kmalloc(%d) out of memory!\n", sizeof (msg_node_t));
		kfree(data);
		return -EFAULT;
	}

	node->size = stCount;		/* Default, retval==stCount */
	node->body = data;

	list_add_tail (&node->list, &intf6.in_msg_list);
	intf6.list_len++;

	while(1){
		if(specific_times6)
		{
                        if(polling_times6)
                        {
			    polling_message(&intf6, polling_len);
			    polling_times6--;
                        }
                        else
                            polling_ready6++;
                        specific_times6--;
			break;
		}
	}
	
	*pPos = stCount;
	return stCount;
}

static int
usbc_interface6c_release (struct inode *pInode, struct file *pFile)
{
	dump_msg_list( &intf6.in_msg_list );

	usbi0_ref_count--;
	MOD_DEC_USE_COUNT;

	if (!usbi0_ref_count)
		intf6.enable = 0;

	return 0;
}

static int
usbc_interface8_open (struct inode *pInode, struct file *pFile)
{
	if (init_interface (&intf8) < 0)
		return -1;

	if ( usbi1_ref_count==0 ) 
		intf8.enable = 1;

	usbi1_ref_count++;
	MOD_INC_USE_COUNT;

	return 0;
}

static ssize_t
usbc_interface8_read (struct file *pFile, char *pUserBuffer,
		      size_t stCount, loff_t * pPos)
{
	int ret;
	struct list_head* ptr=NULL;
	msg_node_t *node;

//	printk("stCount=%d\n",stCount);

	while (list_empty (&intf8.out_msg_list))
	{
		if (pFile->f_flags & O_NONBLOCK) 
			{
				printk ( KERN_INFO "no data in interface8 msg list now!\n");
				return -EAGAIN;
			}
		PRINTKD("\"%s\" reading: going to sleep\n", current->comm);
		interruptible_sleep_on(&wq_read8);
		if (signal_pending(current))                  
				return -ERESTARTSYS;
	}

	ptr = intf8.out_msg_list.next;
	node = list_entry (ptr, msg_node_t, list);
	ret = node->size;
	if (stCount < ret)
		return -EFAULT;		/* This error number is OK? */
	if ( copy_to_user (pUserBuffer, node->body, ret) ) 
	{
		printk ( KERN_INFO " copy_to_user() has some problems! node->size=%d\n", node->size);
		return 0;
	}
	
	list_del (ptr);
    kfree (node->body);
    kfree (node);
    
	return ret;
}

static ssize_t
usbc_interface8_write (struct file *pFile, const char *pUserBuffer,
		       size_t stCount, loff_t * pPos)
{
	u8 *data;
	msg_node_t *node;

	if (stCount == 0)
		return 0;
//	printk("\nThis is test_cmd write\n");
	data = (u8 *) kmalloc (stCount, GFP_KERNEL);
	if (!data)
	{
		printk( KERN_INFO " kmalloc(%d) out of memory!\n", stCount);
		return -EFAULT;
	}

	if ( copy_from_user (data, pUserBuffer, stCount) )
	{
		kfree(data);
		printk( KERN_INFO "copy_from_user() failed.\n" );
		return -EFAULT;
	}

	node = (msg_node_t *) kmalloc (sizeof (msg_node_t), GFP_KERNEL);
	if (!node)
	{
		printk( KERN_INFO "kmalloc(%d) out of memory!\n", sizeof (msg_node_t));
		kfree(data);
		return -EFAULT;
	}

	node->size = stCount;		/* Default, retval==stCount */
	node->body = data;

//	printk("\nbefore list_add_tail\n");
	list_add_tail (&node->list, &intf8.in_msg_list);
	intf8.list_len++;

	while(1){
		if(specific_times8)
		{
			if(polling_times8)
                        {
                            polling_message(&intf8, polling_len);
			    polling_times8--;
                        }
                        else
                            polling_ready8++;
                        specific_times8--;
			break;
		}
	}
	
	*pPos = stCount;
	return stCount;
}

static int
usbc_interface8_release (struct inode *pInode, struct file *pFile)
{
	dump_msg_list( &intf8.in_msg_list );

	usbi1_ref_count--;
	if (!usbi1_ref_count)
		intf8.enable = 0;
	
	MOD_DEC_USE_COUNT;

	return 0;
}

static int
usbc_interface6d_open (struct inode *pInode, struct file *pFile)
{
  
/*	if (init_interface (&intf6) < 0) 
		return -1;
	if (usbi2_ref_count == 0)
		intf6.enable = 1;*/

	usbi2_ref_count++;
	MOD_INC_USE_COUNT;

	return 0;
}

static ssize_t
usbc_interface6d_write (struct file *pFile, const char *pUserBuffer,
		       size_t stCount, loff_t * pPos)
{
	u8 *data;

	data = (u8 *) kmalloc (stCount, GFP_KERNEL);
	if (!data)
	{
		printk ( KERN_INFO "kmalloc(%d) out of memory!\n", stCount);
		return -EFAULT;
	}

	if ( copy_from_user (data, pUserBuffer, stCount) )
	{
		kfree(data);
		printk ( KERN_INFO "copy_from_user() failed. stCount=%d\n", stCount);
		return 0;
	}

	if ( pxa_usb_xmit(data, stCount) < 0 )
	{
		printk ( KERN_INFO "pxa_usb_send() failed!\n");
		return 0;
	}

	kfree(data);
	return stCount;
}

static int
usbc_interface6d_release (struct inode *pInode, struct file *pFile)
{
/*	if (!usbi2_ref_count)
		intf6.enable = 0;
	*/
	usbi2_ref_count--;
	MOD_DEC_USE_COUNT;
	return 0;
}


/* device fileoperation structures */
static struct file_operations usbc_fop0s = {
	owner:THIS_MODULE,
	open:usbc_interface6c_open,
	read:usbc_interface6c_read,
	write:usbc_interface6c_write,
	poll:NULL,
	ioctl:NULL,
	release:usbc_interface6c_release,
};

static struct file_operations usbc_fop1s = {
	owner:THIS_MODULE,
	open:usbc_interface8_open,
	read:usbc_interface8_read,
	write:usbc_interface8_write,
	poll:NULL,
	ioctl:NULL,
	release:usbc_interface8_release,
};

static struct file_operations usbc_fop2s = {
	owner:THIS_MODULE,
	open:usbc_interface6d_open,
	read:NULL,
	write:usbc_interface6d_write,
	poll:NULL,
	ioctl:NULL,
	release:usbc_interface6d_release,
};

static struct miscdevice usbc_misc_device0 = {
	USBC_MINOR0, "datacmd", &usbc_fop0s
};

static struct miscdevice usbc_misc_device1 = {
	USBC_MINOR1, "testcmd", &usbc_fop1s
};

static struct miscdevice usbc_misc_device2 = {
	USBC_MINOR2, "datalog", &usbc_fop2s
};

int __init
usbc_init (void)
{
	int rc;

	/* initialize UDC */
	rc = pxa_usb_open ("usb-dev");
	if (rc)
		return rc;

	rc = pxa_usb_start ();
	if (rc)
		return rc;

	/* register device driver */
	rc = misc_register (&usbc_misc_device0);
	if (rc != 0)
	{
		printk (KERN_INFO "%sCouldn't register misc device0(data command).\n",
			pszMe);
		return -EBUSY;
	}

	rc = misc_register (&usbc_misc_device1);
	if (rc != 0)
	{
		printk (KERN_INFO "%sCouldn't register misc device1(test command).\n", pszMe);
		goto dev1_register_error;
	}

	rc = misc_register (&usbc_misc_device2);
	if (rc != 0)
	{
		printk (KERN_INFO "%sCouldn't register misc device3(data logger).\n",
			pszMe);
		goto dev1_register_error;
	}

	printk (KERN_INFO "USB Function Character Driver Interface"
		" -%s, (C) 2002, Intel Corporation.\n", VERSION);

	return 0;

dev1_register_error:
	misc_deregister (&usbc_misc_device0);

	return -EBUSY;
}

void __exit
usbc_exit (void)
{
	misc_deregister (&usbc_misc_device0);
	misc_deregister (&usbc_misc_device1);
	misc_deregister (&usbc_misc_device2);

	pxa_usb_stop ();
	pxa_usb_close ();
}

module_init (usbc_init);
module_exit (usbc_exit);

MODULE_AUTHOR ("Levis Xu <Levis@Motorola.com>");
MODULE_DESCRIPTION ("USB PST driver");
