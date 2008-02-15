/*
 * linux/drivers/usb/device/acm_fd/acm.c
 *
 * Copyright (c) 2003 Belcarra
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Tom Rushworth <tbr@belcarra.com>, 
 *      Bruce Balden <balden@belcarra.com>
 *
 * This software is dual licensed. 
 *
 * When compiled with BELCARRA_NON_GPL undefined this file may be
 * distributed and used under the GPL license.
 *
 * When compiled with BELCARRA_NON_GPL defined and linked with additional
 * software  which is non-GPL this file may be used and distributed only
 * under a distribution license from Belcarra.
 *
 * Any patches submitted to Belcarra for use with this file will only be
 * accepted if the changes become the property of Belcarra and the resulting
 * changed file can continue to be distributed under either of the two
 * license schemes.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("sl@belcarra.com, tbr@belcarra.com");
USBD_MODULE_INFO ("acm_fd 0.2-alpha");

#if defined(BELCARRA_NON_GPL) 
MODULE_LICENSE("PROPRIETARY");
MODULE_DESCRIPTION ("Belcarra CDC-ACM Function");
#else
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION ("CDC-ACM Function");
#endif

#ifndef MODULE
#undef GET_USE_COUNT
#define GET_USE_COUNT(foo) 1
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/smp_lock.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-debug.h"
#include "../usbd-inline.h"
#include "../usbd-arch.h"

extern unsigned int udc_interrupts;
#include <asm/arch-pxa/pxa-regs.h>
#include <asm/hardware.h>
#include "../bi/trace.h"

#define MAX_QUEUED_BYTES        256
#define MAX_QUEUED_URBS        	64 
/* Module Parameters ************************************************************************* */

static u32 vendor_id = CONFIG_USBD_ACM_VENDORID;
static u32 product_id = CONFIG_USBD_ACM_PRODUCTID;
//static u32 product_id = 0x3804;
static u32 max_queued_urbs = MAX_QUEUED_URBS;
static u32 max_queued_bytes = MAX_QUEUED_BYTES;

extern int pst_dev_create(struct usb_device_instance *device);
extern void pst_dev_destroy(void);
extern void ep0_process_vendor_request( struct urb *urb );
extern int pst_urb_sent (struct urb *urb, int rc);
extern int cable_connected;
	
MODULE_PARM (vendor_id, "i");
MODULE_PARM (product_id, "i");
MODULE_PARM (max_queued_urbs, "i");
MODULE_PARM (max_queued_bytes, "i");

MODULE_PARM_DESC (vendor_id, "Device Vendor ID");
MODULE_PARM_DESC (product_id, "Device Product ID");
MODULE_PARM_DESC (max_queued_urbs, "Maximum TX Queued Urbs");
MODULE_PARM_DESC (max_queued_bytes, "Maximum TX Queued Bytes");

/*
 * CDC ACM Configuration
 *
 * Endpoint, Class, Interface, Configuration and Device descriptors/descriptions
 */
static __u8 acm_alt_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,      BULK_OUT_PKTSIZE, 0x00, 0x00, };
static __u8 acm_alt_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT  | IN,  BULK,      BULK_IN_PKTSIZE,  0x00, 0x00, };
static __u8 *acm_alt_endpoints[] = { acm_alt_1, acm_alt_2, };
static __u32 acm_alt_transferSizes[] = { BULK_OUT_PKTSIZE, BULK_IN_PKTSIZE, };
#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
static __u8 acm_comm_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT, INT_IN_PKTSIZE, 0x00, 0x0a, };
static __u8 *acm_comm_endpoints[] = { acm_comm_1 };
static __u32 acm_comm_transferSizes[] = { 8};
#endif

static __u8 cdc_class_1[] = { 0x05, CS_INTERFACE, USB_ST_HEADER, 0x01, 0x01, /* CLASS_BDC_VERSION, CLASS_BDC_VERSION */ };
static __u8 cdc_class_2[] = { 0x05, CS_INTERFACE, USB_ST_CMF, 0x03, 0x01, /* bMasterInterface: 0, bSlaveInterface: 1 */ };
static __u8 cdc_class_3[] = { 0x05, CS_INTERFACE, USB_ST_UF, 0x00, 0x01, /* bMasterInterface: 0, bSlaveInterface: 1 */ };
static __u8 cdc_class_4[] = { 0x04, CS_INTERFACE, USB_ST_ACMF, 0x02, };
static __u8 *cdc_comm_class_descriptors[] = { cdc_class_1, cdc_class_2, cdc_class_3, cdc_class_4, };

static __u8 cdc_comm_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting, bNumEndpoints
        COMMUNICATIONS_INTERFACE_CLASS, COMMUNICATIONS_ACM_SUBCLASS, 0x01, 0x00, };

static __u8 cdc_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x01, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting, bNumEndpoints
        DATA_INTERFACE_CLASS, COMMUNICATIONS_NO_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00, };

static struct usb_alternate_description cdc_comm_alternate_descriptions[] = {
      { iInterface: CONFIG_USBD_ACM_COMM_INTF,
              interface_descriptor: (struct usb_interface_descriptor *)&cdc_comm_alternate_descriptor,
	      classes:sizeof (cdc_comm_class_descriptors) / sizeof (__u8 *),
              class_list: cdc_comm_class_descriptors,
#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
              endpoint_list: acm_comm_endpoints,
	      endpoints:sizeof (acm_comm_endpoints) / sizeof(__u8 *),
              endpoint_transferSizes_list: acm_comm_transferSizes,
#endif
      }, };

static struct usb_alternate_description cdc_data_alternate_descriptions[] = {
      { iInterface: CONFIG_USBD_ACM_DATA_INTF,
              interface_descriptor: (struct usb_interface_descriptor *)&cdc_data_alternate_descriptor,
              endpoint_list: acm_alt_endpoints,
	      endpoints:sizeof (acm_alt_endpoints) / sizeof(__u8 *),
              endpoint_transferSizes_list: acm_alt_transferSizes, }, };

#define DATA_LOG_IN_ENDPOINT       11	

static __u8 datalog_endpoint[] = {
	0x07,                      // bLength
	USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
	DATA_LOG_IN_ENDPOINT | IN, // bEndpointAddress
	BULK,                      // bmAttributes
	BULK_IN_PKTSIZE, 0x00,     // wMaxPacketSize 
	0x00,                      // bInterval
};

static  __u8 *datalog_endpoints[] = { datalog_endpoint };
static __u32  datalog_transferSizes[] = { BULK_IN_PKTSIZE }; //?????????

static __u8 datalog_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09,              // bLength
        USB_DT_INTERFACE,  // bDescriptorType   // 0x04
        0x02,              // bInterfaceNumber
        0x00,              // bAlternateSetting
        sizeof (datalog_endpoints) / sizeof(__u8 *),
                           // bNumEndpoints
        0xFF,    // bInterfaceClass
        0x02, // bInterfaceSubClass
        0xFF, // bInterfaceProtocol
        0x00,              // iInterface
};

static __u8 testcmd_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09,              // bLength
        USB_DT_INTERFACE,  // bDescriptorType   // 0x04
        0x03,              // bInterfaceNumber
        0x00,              // bAlternateSetting
        0x00,              // bNumEndpoints
        0xFF,    // bInterfaceClass
        0x03, // bInterfaceSubClass
        0xFF, // bInterfaceProtocol
        0x00,              // iInterface
};

static struct usb_alternate_description datalog_alternate_descriptions[] = {
      { iInterface:"Motorola MCU Data Logger",
	       interface_descriptor: (struct usb_interface_descriptor *)&datalog_alternate_descriptor,
             endpoint_list: datalog_endpoints,
             endpoints:sizeof (datalog_endpoints) / sizeof (__u8 *),
             endpoint_transferSizes_list: datalog_transferSizes,
      },
};

static struct usb_alternate_description testcmd_alternate_descriptions[] = {
      { iInterface:"Motorola Test Command",
	        interface_descriptor: (struct usb_interface_descriptor *)&testcmd_alternate_descriptor,
      },              
};

static struct usb_interface_description acm_interfaces[] = {
      { alternates:sizeof (cdc_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:cdc_comm_alternate_descriptions,},

      { alternates:sizeof (cdc_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:cdc_data_alternate_descriptions,},
};

static struct usb_interface_description cdc_interfaces[] = {
      { alternates:sizeof (cdc_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:cdc_comm_alternate_descriptions,},

      { alternates:sizeof (cdc_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:cdc_data_alternate_descriptions,},

      { alternates:sizeof (datalog_alternate_descriptions) / sizeof (struct usb_alternate_description),
               alternate_list:datalog_alternate_descriptions,
      },
      
      { alternates:sizeof (testcmd_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:testcmd_alternate_descriptions,
     }, 
};

static __u8 acm_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, sizeof (acm_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, BMATTRIBUTE, BMAXPOWER, };

static __u8 cdc_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, BMATTRIBUTE, BMAXPOWER, };

struct usb_configuration_description acm_description[] = {
      { iConfiguration: CONFIG_USBD_ACM_DESC,
              configuration_descriptor: (struct usb_configuration_descriptor *)acm_configuration_descriptor,
	      interfaces:sizeof (acm_interfaces) / sizeof (struct usb_interface_description),
              interface_list:acm_interfaces,
//              interface_no_renumber: 1
}, };

struct usb_configuration_description cdc_description[] = {
      {
          iConfiguration: "Motorola CDC Configuration",
              configuration_descriptor: (struct usb_configuration_descriptor *)cdc_configuration_descriptor,
	      interfaces:sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
              interface_list:cdc_interfaces,
              interface_no_renumber: 1
}, };

static __u8 acm_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 0x00, 0x00, COMMUNICATIONS_DEVICE_CLASS, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

struct usb_device_description acm_device_description = {
        device_descriptor: (struct usb_device_descriptor *)acm_device_descriptor,
	idVendor:CONFIG_USBD_ACM_VENDORID,
	idProduct:CONFIG_USBD_ACM_PRODUCTID,
        bcdDevice:0x0001,
	iManufacturer:"Motorola Inc.",
	iProduct:"Motorola Phone",
};

static __inline__ int atomic_post_inc(volatile atomic_t *v)
{
	unsigned long flags;
	int result;

	__save_flags_cli(flags);
	result = (v->counter)++;
	__restore_flags(flags);

	return(result);
}

static __inline__ int atomic_pre_dec(volatile atomic_t *v)
{
	unsigned long flags;
	int result;

	__save_flags_cli(flags);
	result = --(v->counter);
	__restore_flags(flags);

	return(result);
}

struct usb_device_description cdc_device_description = {
        device_descriptor: (struct usb_device_descriptor *)acm_device_descriptor,
	idVendor: CONFIG_USBD_ACM_VENDORID,
	idProduct: 0x6004,
        bcdDevice: 0x0001,
	iManufacturer: CONFIG_USBD_MANUFACTURER,
	iProduct: CONFIG_USBD_PRODUCT_NAME,
};

/* ACM ***************************************************************************************** */
/*
 * Output and Input control lines and line errors.
 */
#define LINE_OUT_DTR            0x01
#define LINE_OUT_RTS            0x02
#define LINE_IN_DCD             0x01
#define LINE_IN_DSR             0x02
#define LINE_IN_BRK             0x04
#define LINE_IN_RI              0x08
#define LINE_IN_FRAMING         0x10
#define LINE_IN_PARITY          0x20
#define LINE_IN_OVERRUN         0x40

/* ******************************************************************************************* */

#define ACM_TTY_MAJOR   166
#define ACM_TTY_MINOR   0
#define ACM_TTY_MINORS  1
#if 0
struct acm_line {
        __u32 speed;
        __u8 stopbits;
        __u8 parity;
        __u8 databits;
} __attribute__ ((packed));
#endif
struct acm_private {
        struct usb_device_instance *device;
        struct tty_driver *tty_driver;
        int tty_driver_registered;              // non-zero if tty_driver registered
        int usb_driver_registered;              // non-zero if usb function registered

        struct tty_struct *tty;                 // non-null if tty open
        struct tq_struct wqueue;                // task queue for writer wakeup 
        struct tq_struct hqueue;                // task queue for hangup
        wait_queue_head_t open_wait;            // wait queue for blocking open
	int open_wait_count;			// count of (possible) blocked
        int exiting;				// True if module exiting

        unsigned char throttle;                 // non-zero if we are throttled
        unsigned char clocal;                   // non-zero if clocal set
        unsigned char connected;                // non-zero if connected to host (configured)

        unsigned int writesize;                 // packetsize * 4
        unsigned int ctrlin;                    // line state device sends to host
        unsigned int ctrlout;                   // line state device received from host

        atomic_t used;
        atomic_t queued_bytes;
        atomic_t queued_urbs;

        struct urb *int_urb;                    // pending interrupt urb
};

static struct tty_driver acm_tty_driver;
static struct acm_private acm_private = {
        tty_driver:                     &acm_tty_driver,
};

static int acm_send_int_notification(struct usb_device_instance *, int , int );

/* Serial Functions **************************************************************************** */

static void acm_schedule(struct tq_struct *queue)
{
        RETURN_IF(!queue->data || queue->sync);
        MOD_INC_USE_COUNT;
        queue_task(queue, &tq_immediate);
        mark_bh(IMMEDIATE_BH);
}

static int block_until_ready(
		struct tty_struct *tty,
		struct file *filp,
		struct acm_private *acm
		)
{
	unsigned long flags;
	int rc = 0;

	// FUTURE: check tty for non-blocking open...

	local_irq_save(flags);
	acm->open_wait_count += 1;
	for (;;) {
		if (tty_hung_up_p(filp)) {
        		//printk (KERN_INFO"%s: tty_hung_up_p()\n",__FUNCTION__);
			rc = -ERESTARTSYS;
			break;
		}
		if (signal_pending(current)) {
        		//printk (KERN_INFO"%s: signal_pending()\n",__FUNCTION__);
			rc = -ERESTARTSYS;
			break;
		}
		if (acm->exiting) {
        		//printk (KERN_INFO"%s: module exiting\n",__FUNCTION__);
			rc = -ENODEV;
			break;
		}
		if (acm->ctrlout & LINE_OUT_DTR) {
			// OK, there's somebody on the other end, let's go...
        		//printk (KERN_INFO"%s: got DTR\n",__FUNCTION__);
			break;
		}
        	//printk (KERN_INFO"%s: owc=%d sleeping...\n",__FUNCTION__,acm->open_wait_count);
		interruptible_sleep_on(&acm->open_wait);
        	//printk (KERN_INFO"%s: owc=%d got WAKEUP\n",__FUNCTION__,acm->open_wait_count);
	}
	acm->open_wait_count -= 1;
	local_irq_restore(flags);
	return(rc);
}

static void acm_hangup(void *private)
{
        struct acm_private *acm = &acm_private;
        struct tty_struct *tty = acm->tty;

	if (tty && !acm->clocal) {
        	tty_hangup(tty);
	}

        wake_up_interruptible(&acm->open_wait);

        MOD_DEC_USE_COUNT;
        //printk (KERN_INFO"%s: MOD: %d connected: %d tty: %p clocal: %x\n", 
        //                __FUNCTION__, MOD_IN_USE, acm->connected, tty, acm->clocal);
}

static int acm_tty_open(struct tty_struct *tty, struct file *filp)
{
        struct acm_private *acm = &acm_private;
	 struct usb_device_instance *device = acm->device;
        int used;
	 int rc = 0;

        //printk (KERN_INFO"%s: device: %p used: %d MOD: %d\n", __FUNCTION__, device, atomic_read(&acm->used), MOD_IN_USE);
	if (filp->f_flags & O_NONBLOCK) 
	{
		if(!(acm->connected))
		{
			//printk(KERN_INFO "\nusb cable not connected!\n");
			return -EINVAL;
		}
			// lock and increment used counter, save current value
		used = atomic_post_inc(&acm->used);
	        // done if previously open
	        RETURN_EBUSY_IF(used); 
	        
	        //printk (KERN_INFO"%s: FIRST OPEN used_nonblock: %d\n", __FUNCTION__, atomic_read(&acm->used));
	        MOD_INC_USE_COUNT;

	        tty->driver_data = acm;
	        acm->tty = tty;
	        tty->low_latency = 1;
	        acm->ctrlin = LINE_IN_DCD | LINE_IN_DSR;
	        RETURN_ZERO_IF(!acm->device); 
	        return acm_send_int_notification(device, CDC_NOTIFICATION_SERIAL_STATE, acm->ctrlin);
		}else {
        		//acm_private.hqueue.routine = acm_hangup;
			used = atomic_post_inc(&acm->used);
	        	RETURN_EBUSY_IF(used); 
			if (used <= 0) {
		        	MOD_INC_USE_COUNT;

		        	//printk (KERN_INFO"%s: FIRST OPEN used_block: s=%d, a=%d\n", __FUNCTION__, used, atomic_read(&acm->used));

		        	tty->driver_data = acm;
		        	acm->tty = tty;
		        	tty->low_latency = 1;
		        	acm->ctrlin = LINE_IN_DCD | LINE_IN_DSR;
		        	if (NULL != acm->device) {
		        		rc = acm_send_int_notification(device, CDC_NOTIFICATION_SERIAL_STATE, acm->ctrlin);
				}
			}
			if (0 == rc) {
				rc = block_until_ready(tty,filp,acm);
			}
			/* The tty layer calls acm_tty_close() even if this open fails,
		           so any cleanup (rc != 0) will be done there. */
			return(rc);
		}
}               
                
static void acm_tty_close(struct tty_struct *tty, struct file *filp)
{       
        struct acm_private *acm = tty->driver_data;
	struct usb_device_instance *device;	// = acm->device;
        int used;

	if(acm == NULL)	// if open without cable, tty will call _close and here acm is NULL!
		return ;

	device = acm->device;
	
        //printk (KERN_INFO"%s: device: %p used: %d MOD: %d\n", __FUNCTION__, device, atomic_read(&acm->used), MOD_IN_USE);

#if 0
        // lock and decrement used counter, save result
        lock_kernel();
        atomic_dec(&acm->used); 
        used = atomic_read(&acm->used);
        unlock_kernel();
#else
	used = atomic_pre_dec(&acm->used);
#endif

        // finished unless this is the last close
	if (used <= 0) {
		// This is the last close, clean up

	        acm->tty = NULL;
		acm->ctrlin = 0x0;

		if (acm->device) {
        		acm_send_int_notification(device, CDC_NOTIFICATION_SERIAL_STATE, acm->ctrlin);
		}
		/* This should never happen if this is the last close,
                   but it can't hurt to check. */
		if (acm->open_wait_count) {
			wake_up_interruptible(&acm->open_wait);
		}
        	MOD_DEC_USE_COUNT;
        	//printk (KERN_INFO"%s: LAST CLOSE used: %d MOD: %d\n", __FUNCTION__, atomic_read(&acm->used), MOD_IN_USE);
	}
}       

static int acm_tty_write(struct tty_struct *tty, int from_user, const unsigned char *buf, int count)
{
        struct acm_private *acm = tty->driver_data;
	struct usb_device_instance *device = acm->device;
	struct urb *urb;

        //printk (KERN_INFO"%s: used: %d count: %d\n", __FUNCTION__, atomic_read(&acm->used), count);
        // sanity check and are we connect
        RETURN_EINVAL_IF(!atomic_read(&acm->used)); 
        RETURN_ZERO_IF (!acm->device || !count || !acm->connected);

	/***by Levis 10/10/03***/
	RETURN_ZERO_IF(max_queued_urbs <= atomic_read(&acm->queued_urbs));
	/***end Levis***/		
        
	// allocate a write urb
        count = MIN(count, acm->writesize);

	if(!(count%64))
	{
		count--;
	}
	 RETURN_ENOMEM_IF (!(urb = usbd_alloc_urb (device, BULK_IN_ENDPOINT, count)));

	//urb = usbd_alloc_urb (device, BULK_IN_ENDPOINT, count);
	//RETURN_EINVAL_IF (!urb);

        if (from_user) {
                copy_from_user(urb->buffer, buf, count);
        }
        else {
                memcpy(urb->buffer, buf, count);
        }
#if 0
        {
                unsigned char *data = urb->buffer;
                int i;
                lock_kernel ();
                printk(KERN_INFO"%s: ", __FUNCTION__);
                for (i = 0; i < count; i++) {
                        printk("%02x ", data[i]);
                }
                printk("\n");
                unlock_kernel ();
        }
#endif
        urb->privdata = acm;
        urb->actual_length = count;
        atomic_add(count, &acm->queued_bytes);
        atomic_inc(&acm->queued_urbs);
        usbd_send_urb(urb);
        return count;
}

static int acm_tty_write_room(struct tty_struct *tty)
{       
        struct acm_private *acm = &acm_private;

        //printk (KERN_INFO"%s: used: %d queued: %d %d room: %d\n", __FUNCTION__, atomic_read(&acm->used),
        //                atomic_read(&acm->queued_urbs), atomic_read(&acm->queued_bytes),
        //                (max_queued_urbs > atomic_read(&acm->queued_urbs)) && 
        //                (max_queued_bytes > atomic_read(&acm->queued_bytes)) ? acm->writesize : 0);

        RETURN_EINVAL_IF(!atomic_read(&acm->used)); 
        return ( !acm->device || (max_queued_urbs <= atomic_read(&acm->queued_urbs)) ||
                        (max_queued_bytes <= atomic_read(&acm->queued_bytes))) ?  0 : acm->writesize ;
}       

static int acm_tty_chars_in_buffer(struct tty_struct *tty)
{       
        struct acm_private *acm = &acm_private;
        RETURN_EINVAL_IF(!atomic_read(&acm->used)); 
        RETURN_ZERO_IF(!acm->device); 
        return atomic_read(&acm->queued_bytes);
}       

static void acm_tty_throttle(struct tty_struct *tty)
{       
        struct acm_private *acm = &acm_private;
        //printk (KERN_INFO"%s:\n", __FUNCTION__);
        if (acm && atomic_read(&acm->used)) {
                acm->throttle = 1;
        }
}               

static void acm_tty_unthrottle(struct tty_struct *tty)
{               
        struct acm_private *acm = &acm_private;
        //printk (KERN_INFO"%s:\n", __FUNCTION__);
        if (acm && atomic_read(&acm->used)) {
                acm->throttle = 0;
        }
}       

static int acm_tty_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{       
        struct acm_private *acm = &acm_private;
	struct usb_device_instance *device = acm->device;
        unsigned int mask;
        unsigned int newctrl;

        //printk (KERN_INFO"%s:\n", __FUNCTION__);
        //printk (KERN_INFO"%s: used: %d\n", __FUNCTION__, atomic_read(&acm->used));
        RETURN_EINVAL_IF(!atomic_read(&acm->used)); 

#if 1	// temporary solution for IOCTL
	if(cmd == TIOCMGET)
		return put_user( acm->ctrlout & LINE_OUT_DTR ? TIOCM_DTR : 0 | TIOCM_CTS, (unsigned long *) arg);
#endif
	
       	return -ENOIOCTLCMD;


        switch(cmd) {
        case TCGETS:
        case TCFLSH:
        case TCSETS:
                printk (KERN_INFO"%s: ignored cmd: %x\n", __FUNCTION__, cmd);
                return 0;

        case TCSETSW:

                //user_termios_to_kernel_termios(&priv->termios, (struct termios *)arg);
                printk (KERN_INFO"%s: ignored cmd: %x\n", __FUNCTION__, cmd);
                return 0;

        case TIOCMGET:
                //printk (KERN_INFO"%s: TIOCMGET: %04x\n", __FUNCTION__, acm->ctrlout & LINE_OUT_DTR ? TIOCM_DTR : 0 | TIOCM_CTS);
                return put_user( acm->ctrlout & LINE_OUT_DTR ? TIOCM_DTR : 0 | TIOCM_CTS, (unsigned long *) arg);

        case TIOCMSET:
        case TIOCMBIS:
        case TIOCMBIC:

                printk (KERN_INFO"%s: TIOCM{SET,BIS,BIC}:\n", __FUNCTION__);
                RETURN_EFAULT_IF (get_user(mask, (unsigned long *) arg));

                printk (KERN_INFO"%s: TIOCM{SET,BIS,BIC}: %04x\n", __FUNCTION__, mask);

                newctrl = acm->ctrlin;

                mask = (mask & TIOCM_DTR ? LINE_IN_DCD|LINE_IN_DSR : 0)/* | (mask & TIOCM_RTS ? LINE_OUT_RTS : 0)*/;

                switch(cmd) {
                case TIOCMSET: newctrl = mask; break;
                case TIOCMBIS: newctrl |= mask; break;
                case TIOCMBIC: newctrl &= mask; break;
                }
                RETURN_ZERO_IF(acm->ctrlin == newctrl);

                printk (KERN_INFO"%s: newctrl: %04x\n", __FUNCTION__, newctrl);
                return acm_send_int_notification(device, CDC_NOTIFICATION_SERIAL_STATE, acm->ctrlin);
        case 3:
                return 0;
        default:
                printk (KERN_INFO"%s: unknown: %04x\n", __FUNCTION__, cmd);
        }
        return -ENOIOCTLCMD;
}

static void acm_tty_set_termios(struct tty_struct *tty, struct termios *termios_old)
{
        struct acm_private *acm = &acm_private;
        struct termios *termios;

        //printk (KERN_INFO"%s: tty: %p\n", __FUNCTION__, tty);

        RETURN_IF(!atomic_read(&acm->used) || !tty || !tty->termios);

        termios = tty->termios;
        acm->clocal = termios->c_cflag & CLOCAL;

        //printk (KERN_INFO"%s: clocal: %d\n", __FUNCTION__, acm->clocal);
}

/* ********************************************************************************************* */

static void acm_wakeup_writers(void *private)
{
        struct acm_private *acm = &acm_private;
        struct tty_struct *tty = acm->tty;

        MOD_DEC_USE_COUNT;
        
        //printk (KERN_INFO"%s: MOD: %d connected: %d tty: %p\n", __FUNCTION__, MOD_IN_USE, acm->connected, tty);

        RETURN_IF(!acm->connected || !atomic_read(&acm->used) || !tty);

        if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
                (tty->ldisc.write_wakeup)(tty);
        }
        wake_up_interruptible(&tty->write_wait);
}


/* ********************************************************************************************* */

static int acm_tty_refcount;
static struct tty_struct *acm_tty_table[ACM_TTY_MINORS];
static struct termios *acm_tty_termios[ACM_TTY_MINORS];
static struct termios *acm_tty_termios_locked[ACM_TTY_MINORS];

static struct tty_driver acm_tty_driver = {
        magic:                  TTY_DRIVER_MAGIC,
        type:                   TTY_DRIVER_TYPE_SERIAL,
        subtype:                SERIAL_TYPE_NORMAL,
        flags:                  TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS,
        driver_name:            "acm",
        name:                   "usb/acm/%d",
        major:                  ACM_TTY_MAJOR,
        num:                    ACM_TTY_MINORS,
        minor_start:            0, 

        open:                   acm_tty_open,
        close:                  acm_tty_close,
        write:                  acm_tty_write,
        write_room:             acm_tty_write_room,
        ioctl:                  acm_tty_ioctl,
        throttle:               acm_tty_throttle,
        unthrottle:             acm_tty_unthrottle,
        chars_in_buffer:        acm_tty_chars_in_buffer,
        set_termios:            acm_tty_set_termios,

        refcount:               &acm_tty_refcount,
        table:                  acm_tty_table,
        termios:                acm_tty_termios,
        termios_locked:         acm_tty_termios_locked,
};      


/* Transmit INTERRUPT ************************************************************************** */

void acm_check_interrupt(void)
{
	struct acm_private *acm = &acm_private;
        return;
        
        //printk (KERN_INFO"%s: int_urb: %p\n", __FUNCTION__, acm->int_urb);
        if (acm->connected && acm->int_urb) {
                usbd_cancel_urb(acm->int_urb->device, acm->int_urb);
                acm->int_urb = NULL;
        }
}

/**
 * Generates a response urb on the notification (INTERRUPT) endpoint.
 * Return a non-zero result code to STALL the transaction.
 * CALLED from interrupt context.
 */
static int acm_send_int_notification(struct usb_device_instance *device, int bnotification, int data)
{
	struct acm_private *acm = &acm_private;
        struct urb *urb;
        struct cdc_notification_descriptor *notification;
        int rc;

        //printk (KERN_INFO"%s: bnotification: %x data: %d\n", __FUNCTION__, bnotification, data);

        RETURN_ZERO_IF(!acm->connected || !acm->ctrlout & LINE_OUT_DTR);

        acm_check_interrupt();

        RETURN_ENOMEM_IF(!(urb = usbd_alloc_urb (device, INT_IN_ENDPOINT, sizeof(struct cdc_notification_descriptor)))); 
        
        memset(urb->buffer, 0, urb->buffer_length);
        urb->actual_length = sizeof(struct cdc_notification_descriptor);
        urb->privdata = &acm_private;

        // fill in notification structure
        notification = (struct cdc_notification_descriptor *) urb->buffer;

        notification->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
        notification->bNotification = bnotification;

        switch (bnotification) {
        case CDC_NOTIFICATION_NETWORK_CONNECTION:
                notification->wValue = data;
                break;
        case CDC_NOTIFICATION_SERIAL_STATE:
                notification->wLength = cpu_to_le16(2);
                *((unsigned short *)notification->data) = cpu_to_le16(data);
                break;
        }

	acm->int_urb = urb;
        RETURN_ZERO_IF(!(rc = usbd_send_urb (urb)));

        printk(KERN_ERR"%s: usbd_send_urb failed err: %x\n", __FUNCTION__, rc);
	acm->int_urb = urb;
        urb->privdata = NULL;
        usbd_dealloc_urb (urb);
        return -EINVAL;
}

static int acm_send_int_network(struct usb_device_instance *device, int connected)
{
        return acm_send_int_notification(device, CDC_NOTIFICATION_NETWORK_CONNECTION, connected);
}


/* Transmit Function - called by serproto ****************************************************** */

/* acm_urb_sent - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 */
int acm_urb_sent (struct urb *urb, int rc)
{
	//struct acm_private *acm = urb->privdata;
	struct acm_private *acm = &acm_private;

        //printk (KERN_INFO"%s:\n", __FUNCTION__);
        switch(urb->endpoint->endpoint_address&0xf) {

        case INT_IN_ENDPOINT:
                //printk(KERN_INFO "%s: %s INT length: %d\n", __FUNCTION__, urb->device->name, urb->actual_length);
                acm->int_urb = NULL;
                usbd_dealloc_urb(urb);
                return 0;

        case BULK_IN_ENDPOINT:
                //printk(KERN_INFO "%s: %s IN length: %d\n", __FUNCTION__, urb->device->name, urb->actual_length);
                atomic_sub(urb->actual_length, &acm->queued_bytes);
                atomic_dec(&acm->queued_urbs);
                urb->privdata = NULL;
                usbd_dealloc_urb (urb);
                acm_schedule(&acm->wqueue);
                return 0;

        case DATA_LOG_IN_ENDPOINT:
        	pst_urb_sent(urb, rc);
        	return 0;
        }
        return -EINVAL;
}

/* USB Device Functions ************************************************************************ */

/* acm_event_irq - process a device event
 *
 */
void acm_event_irq (struct usb_device_instance *device, usb_device_event_t event, int data)
{
        struct acm_private *acm = &acm_private;
	struct usb_function_instance *function;
        int ret;

	RETURN_IF ((function = device->active_function_instance) == NULL);

	switch (event) {
        case DEVICE_CREATE:
                if((ret=pst_dev_create(device)) != 0) return;
                break;

        case DEVICE_DESTROY:
                pst_dev_destroy();
                break;
                
	case DEVICE_CONFIGURED:
                cable_connected = 1;
		acm->connected = 1;
                acm_schedule(&acm->wqueue);
		break;

	case DEVICE_RESET:
	case DEVICE_DE_CONFIGURED:
 /*               BREAK_IF(!acm->connected);
                acm->connected = 0;
                acm->int_urb = NULL;
                acm_schedule(&acm->hqueue);*/
		break;
	
	case DEVICE_CABLE_DISCONNECTED:
                cable_connected = 0;
                BREAK_IF(!acm->connected);
                acm->connected = 0;
                acm->int_urb = NULL;
                acm_schedule(&acm->hqueue);
		break;

        default: break;
	}
}

#undef MIN
#define MIN(a,b)        ((a) < (b) ? (a):(b))

/**updated for flow control by Levis 09/23/03**/
int acm_recv_urb (struct urb *urb)
{
	struct acm_private *acm = &acm_private;
        struct tty_struct *tty = acm->tty;
        unsigned char *cp = urb->buffer;
        int i;

#define ACM_WORD_COPY	// - w20146 - Jul 23
#ifndef ACM_WORD_COPY
        if ((urb->status == RECV_OK) && tty && !acm->throttle) {
                for (i = 0; i < urb->actual_length; i++) {
                        if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
                                tty_flip_buffer_push(tty);
                        }
                        tty_insert_flip_char(tty, *cp++, 0);
                }
                tty_flip_buffer_push(tty);
		usbd_recycle_urb (urb);
		return (0);
        }
#else
	if ((urb->status == RECV_OK) && tty && !acm->throttle) 
	{
		int count = urb->actual_length;
		/***add by Levis to fix data loss***/
		if(count > (TTY_FLIPBUF_SIZE-tty->flip.count))
		{
			return (1);
		}
		/***end Levis***/
		if(tty->flip.count + count > TTY_FLIPBUF_SIZE)
		{
			tty_flip_buffer_push(tty);
			//count = MIN(count, TTY_FLIPBUF_SIZE - tty->flip.count);
		}
		
		if(count > 0)
		{
			// If urb->actual_length > TTY_FLIPBUF_SIZE, it will lose date.
			// But for USB, max packet is 64 for BULK OUT.
			memcpy(tty->flip.char_buf_ptr, cp, count);	// see: arch/arm/lib/memcpy.S

			tty->flip.count += count;
			tty->flip.flag_buf_ptr += count;
			tty->flip.char_buf_ptr += count;        // w20146 - Jul 31 
			tty_flip_buffer_push(tty);
		}
		usbd_recycle_urb (urb);
		return (0);
	}	
#endif
	return (1);
}

/* acm_recv_setup - called to indicate urb has been received
 */
int acm_recv_setup (struct urb *urb)
{
        struct usb_device_instance *device = urb->device;
        struct usb_device_request *request = &urb->device_request;
        struct usb_function_instance *function;

        acm_check_interrupt();

        // verify that this is a usb class request per cdc-acm specification or a vendor request.
        RETURN_ZERO_IF (!(request->bmRequestType & (USB_REQ_TYPE_CLASS | USB_REQ_TYPE_VENDOR)));
       
        // determine the request direction and process accordingly
        switch (request->bmRequestType & (USB_REQ_DIRECTION_MASK | USB_REQ_TYPE_MASK)) {

        case USB_REQ_HOST2DEVICE | USB_REQ_TYPE_CLASS:
                switch (request->bRequest) {
                case CDC_CLASS_REQUEST_SEND_ENCAPSULATED: break;
                case CDC_CLASS_REQUEST_SET_COMM_FEATURE: break;
                case CDC_CLASS_REQUEST_CLEAR_COMM_FEATURE: break;
                case CDC_CLASS_REQUEST_SET_LINE_CODING:
#if 0
                        {
                                //struct usb_device_request *results = (struct usb_device_request *)urb->buffer;
                                //results->dwdterate = 0x4b00; // 19200
                                //results ->bdatabits = 0x08;
                                //urb->actual_length = 7;
                                break;
                        }
#endif
                        break;
                case CDC_CLASS_REQUEST_SET_CONTROL_STATE:
                        {
                                struct acm_private *acm = &acm_private;
                                acm->ctrlout = le16_to_cpu(request->wValue);
                                
                                //printk(KERN_INFO"%s: tty: %p clocal: %x ctrlout: %02x DTR: %x\n", 
				//	__FUNCTION__, acm->tty, acm->clocal, acm->ctrlout, acm->ctrlout & LINE_OUT_DTR);
                                
                                // schedule writers or hangup IFF open
                                BREAK_IF(!acm->tty);
                                acm_schedule(((acm->ctrlout & LINE_OUT_DTR) ? &acm->wqueue : &acm->hqueue));
				// wake up blocked opens
				if (acm->open_wait_count > 0) {
					wake_up_interruptible(&acm->open_wait);
				}

                                // send notification if we have DCD
                                BREAK_IF(!(acm->ctrlin & (LINE_IN_DCD | LINE_IN_DSR)));
                                acm_send_int_notification(device, CDC_NOTIFICATION_SERIAL_STATE, acm->ctrlin);
                        }
                        break;

                case CDC_CLASS_REQUEST_SEND_BREAK: break;
                default: break;
                }
                return 0;

        case USB_REQ_DEVICE2HOST | USB_REQ_TYPE_CLASS:
                switch (request->bRequest) {
                case CDC_CLASS_REQUEST_GET_ENCAPSULATED: break;
                case CDC_CLASS_REQUEST_GET_COMM_FEATURE: break;
                case CDC_CLASS_REQUEST_GET_LINE_CODING: 
                        {
                                struct cdc_acm_line_coding *results = (struct cdc_acm_line_coding *)urb->buffer;
                                
				// win
                                //results->dwDTERate = 0x4b00; // 19200
				results->dwDTERate = 0x1c200; // 115200, - w20146 - Jul 25
				
                                results ->bDataBits = 0x08;
                                urb->actual_length = 7;
                                break;
                        }
                default: break;
                }
                return 0;

        case USB_REQ_HOST2DEVICE | USB_REQ_TYPE_VENDOR: 
        case USB_REQ_DEVICE2HOST | USB_REQ_TYPE_VENDOR: 
	        ep0_process_vendor_request( urb );
		return 0;

        default: break;
        }
        return 0;
}


static void acm_function_init (struct usb_bus_instance *bus, struct usb_device_instance *device,
                               struct usb_function_driver *function_driver, struct usb_function_instance *function_instance)
{
        struct acm_private *acm = &acm_private;
        acm->device = device;
        acm->writesize = BULK_OUT_PKTSIZE * 4;
}

static void acm_function_exit (struct usb_device_instance *device)
{               
        struct acm_private *acm = &acm_private;
        acm->device = NULL;
        acm->writesize = 0;
}

struct usb_function_operations function_ops_acm = {
	event_irq:acm_event_irq,
	recv_urb:acm_recv_urb,
	recv_setup:acm_recv_setup,
	urb_sent:acm_urb_sent,
        function_init: acm_function_init,
        function_exit: acm_function_exit,
};

struct usb_function_driver function_driver_acm = {
	name:"CDC acm",
	ops:&function_ops_acm,
	device_description:&acm_device_description,
	configurations:sizeof (acm_description) / sizeof (struct usb_configuration_description),
	configuration_description:acm_description,
	this_module:THIS_MODULE,
};

static struct usb_function_driver cdc_function_driver = {
	name:"CDC cfg11",
	ops:&function_ops_acm,
	device_description:&cdc_device_description,
	configurations:sizeof (cdc_description) / sizeof (struct usb_configuration_description),
	configuration_description:cdc_description,
	this_module:THIS_MODULE,
};

#undef USE_TICKER
#if defined(USE_TICKER)
/* usb ticker ********************************************************************************** */

#define retrytime 10
                
int ticker_terminating;
int ticker_timer_set;

static DECLARE_MUTEX_LOCKED (ticker_sem_start);
static DECLARE_MUTEX_LOCKED (ticker_sem_work);

void ticker_tick (unsigned long data)
{
        ticker_timer_set = 0;
        up (&ticker_sem_work);
}

void udc_ticker_poke (void)
{
        up (&ticker_sem_work);
}

int ticker_thread (void *data)
{
        struct timer_list ticker;
        // detach
        lock_kernel ();
        exit_mm (current);
        exit_files (current);
        exit_fs (current);

        // setsid equivalent, used at start of kernel thread, no error checks needed, or at least none made :). 
        current->leader = 1;
        current->session = current->pgrp = current->pid;
        current->tty = NULL;
        current->tty_old_pgrp = 0;
        sprintf (current->comm, "acm_fd");

        // setup signal handler
        current->exit_signal = SIGCHLD;
        spin_lock (&current->sigmask_lock);
        flush_signals (current);
        spin_unlock (&current->sigmask_lock);

        // run at a high priority, ahead of sync and friends
        current->policy = SCHED_OTHER;
        unlock_kernel ();

        // setup timer
        init_timer (&ticker);
        ticker.data = 0;
        ticker.function = ticker_tick;

        // let startup continue
        up (&ticker_sem_start);
        
        // process loop
        for (ticker_timer_set = ticker_terminating = 0; !ticker_terminating;) {

                struct acm_private * acm = &acm_private;
                struct usb_device_instance *device = acm->device;

                if (!ticker_timer_set) {
                        mod_timer (&ticker, jiffies + HZ * retrytime);
                }

                // wait for someone to tell us to do something
                down (&ticker_sem_work);

                // sanity checks before proceeding
                BREAK_IF(ticker_terminating);
                CONTINUE_IF(!(device = acm->device));
                CONTINUE_IF(USBD_OK != device->status);

                // do what we need to do
                acm_send_int_notification(device, CDC_NOTIFICATION_SERIAL_STATE, acm->ctrlin);
        }

        // remove timer, let the process stopping us know we are done and return
        del_timer (&ticker);
        up (&ticker_sem_start);
        return 0;
}
#endif

/* USB Module init/exit ************************************************************************ */
/*
 * acm_modinit - module init
 *
 */
int acm_modinit (void)
{
	printk (KERN_INFO "%s: %s vendor_id: %04x product_id: %04x\n", __FUNCTION__, __usbd_module_info, vendor_id, product_id);

        acm_device_description.idVendor = vendor_id;
        acm_device_description.idProduct = product_id;
	init_waitqueue_head(&acm_private.open_wait);

        // register as tty driver
        acm_tty_driver.init_termios = tty_std_termios;
        
	//acm_tty_driver.init_termios.c_cflag = B19200 | CS8 | CREAD | HUPCL | CLOCAL;
        acm_tty_driver.init_termios.c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;	// - w20146 - Jul 25

	acm_tty_driver.init_termios.c_lflag &= ~(ECHO | ICANON);
        THROW_IF(tty_register_driver(&acm_tty_driver), error);

        tty_register_devfs(&acm_tty_driver, 0, ACM_TTY_MINOR);
        acm_private.tty_driver_registered++;

	// register as usb function driver
	THROW_IF (usbd_register_function (&function_driver_acm), error);
	THROW_IF (usbd_register_function (&cdc_function_driver), error);
        acm_private.usb_driver_registered++;

#if defined(USE_TICKER)
        // kickoff_thread - start management thread
        //ticker_terminating = 0;
        //kernel_thread (&ticker_thread, NULL, 0);
        //down (&ticker_sem_start);
#endif

        // initialize private structure
        acm_private.wqueue.routine = acm_wakeup_writers;
	acm_private.wqueue.data = &acm_private;
        acm_private.hqueue.routine = acm_hangup;
        acm_private.hqueue.data = &acm_private;

        CATCH(error) {
                printk(KERN_ERR"%s: ERROR\n", __FUNCTION__);

                if (acm_private.tty_driver_registered) {
                        tty_unregister_driver(&acm_tty_driver);
                        acm_private.tty_driver_registered = 0;
                }
                if (acm_private.usb_driver_registered) {
                        usbd_deregister_function (&function_driver_acm);
                        usbd_deregister_function (&cdc_function_driver);
                        acm_private.usb_driver_registered = 0;
                }
                return -EINVAL;
        }
	return 0;
}

void acm_wait_task(struct tq_struct *queue)
{
        RETURN_IF(!queue->data);
        queue->data = NULL;
        while (queue->sync) {
                printk(KERN_INFO"%s: waiting for queue: %p\n", __FUNCTION__, queue);
                schedule_timeout(HZ);
        }
}

/* acm_modexit - module cleanup
 */
void acm_modexit (void)
{
	unsigned long flags;
        printk(KERN_INFO"%s:\n", __FUNCTION__);

#if defined(USE_TICKER)
        // killoff_thread - stop management thread
        //if (!ticker_terminating) {
        //        ticker_terminating = 1;
        //        up (&ticker_sem_work);
        //        down (&ticker_sem_start);
        //}
#endif

	local_irq_save(flags);
	acm_private.exiting = 1;
	if (acm_private.open_wait_count > 0) {
		wake_up_interruptible(&acm_private.open_wait);
	}
	local_irq_restore(flags);

        // verify no tasks are running
        acm_wait_task(&acm_private.wqueue);
        acm_wait_task(&acm_private.hqueue);

        // de-register as tty  and usb drivers
        if (acm_private.tty_driver_registered) {
                tty_unregister_driver(&acm_tty_driver);
        }
        if (acm_private.usb_driver_registered) {
                usbd_deregister_function (&function_driver_acm);
                usbd_deregister_function (&cdc_function_driver);
        }
}

//module_init (acm_modinit);
//module_exit (acm_modexit);
EXPORT_SYMBOL(acm_modinit);
EXPORT_SYMBOL(acm_modexit);

