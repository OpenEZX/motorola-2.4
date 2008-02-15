/*
 * linux/drivers/usb/device/serial_fd/mouse.c
 *
 * Copyright (c) 2002 Belcarra
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Tom Rushworth <tbr@belcarra.com>, 
 *      Bruce Balden <balden@belcarra.com>
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
MODULE_DESCRIPTION ("USB Device Mouse Function");

USBD_MODULE_INFO ("mouse 0.1-beta");

#ifndef MODULE
#undef GET_USE_COUNT
#define GET_USE_COUNT(foo) 1
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-debug.h"
#include "../usbd-inline.h"
#include "../usbd-arch.h"

#include "crc10.h"

#include "serproto.h"


#define MAX_INTERFACES 1

#define MTU                             1500+100


#if !defined (CONFIG_USBD_VENDORID) && !defined(CONFIG_USBD_SERIAL_VENDORID)
#error No Vendor ID
#endif
#if !defined (CONFIG_USBD_PRODUCTID) && !defined(CONFIG_USBD_SERIAL_PRODUCTID)
#error No Product ID
#endif


#if CONFIG_USBD_SERIAL_VENDORID
#undef CONFIG_USBD_VENDORID
#define CONFIG_USBD_VENDORID CONFIG_USBD_SERIAL_VENDORID
#endif

#if CONFIG_USBD_SERIAL_PRODUCTID
#undef CONFIG_USBD_PRODUCTID
#define CONFIG_USBD_PRODUCTID CONFIG_USBD_SERIAL_PRODUCTID
#endif

#ifndef CONFIG_USBD_SERIAL_NUMBER_STR
#define CONFIG_USBD_SERIAL_NUMBER_STR           ""
#endif


struct usb_mouse_private {
	int interface;
	struct usb_device_instance *device;
	rwlock_t rwlock;
};

/* Module Parameters ************************************************************************* */

static char *dbg = NULL;
static u32 vendor_id;
static u32 product_id;
static u32 txqueue_urbs;

MODULE_PARM (dbg, "s");
MODULE_PARM (vendor_id, "i");
MODULE_PARM (product_id, "i");
MODULE_PARM (txqueue_urbs, "i");

MODULE_PARM_DESC (dbg, "USB Device Debug options");
MODULE_PARM_DESC (vendor_id, "USB Device Vendor ID");
MODULE_PARM_DESC (product_id, "USB Device Product ID");
MODULE_PARM_DESC (txqueue_urbs, "Maximum TX Queue Urbs");

/* Debug switches (module parameter "dbg=...") *********************************************** */

extern int dbgflg_usbdfd_init;
int dbgflg_usbdfd_usbe;
int dbgflg_usbdfd_tx;
int dbgflg_usbdfd_rx;
int dbgflg_usbdfd_loopback;

static debug_option dbg_table[] = {
	{&dbgflg_usbdfd_init, NULL, "init", "initialization and termination"},
	{&dbgflg_usbdfd_usbe, NULL, "usbe", "USB events"},
	{&dbgflg_usbdfd_rx, NULL, "rx", "receive (from host)"},
	{&dbgflg_usbdfd_tx, NULL, "tx", "transmit (to host)"},
	{&dbgflg_usbdfd_loopback, NULL, "loop", "loopback mode if non-zero"},
	{NULL, NULL, "ser", "serial device (tty) handling"},
	{NULL, NULL, NULL, NULL}
};

#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_init,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_usbe,lvl,fmt,##args)
#define dbg_rx(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_rx,lvl,fmt,##args)
#define dbg_tx(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_tx,lvl,fmt,##args)
#define dbg_loop(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_loopback,lvl,fmt,##args)

/* ******************************************************************************************* */

static int mouse_created;

static struct usb_mouse_private *mouse_private_array[MAX_INTERFACES];

static rwlock_t mouse_rwlock = RW_LOCK_UNLOCKED;	// lock for serproto device array access

static __inline__ struct usb_mouse_private *get_mouse_private (int interface)
{
	if (interface < 0 || interface >= MAX_INTERFACES) {
		return NULL;
	}
	return mouse_private_array[interface];
}


/* usb-func.c ******************************************************************************** */



/*
// C:\usb\MSDEV\Projects\test\mouse.h


char ReportDescriptor[50] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};
*/




/* Communications Interface Class descriptions 
 */

static __u8 mouse_data_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT, INT_IN_PKTSIZE, 0x00, 0x01, };
static __u8 mouse_hid_1[] = { 0x09, 0x21, 0x10, 0x01, 0x00, 0x01, 0x22, 0x34, 0x00, };

static __u8 *mouse_default[] = { mouse_data_1, };
static __u8 *mouse_hid_descriptors[] = { mouse_hid_1, };
static __u32 mouse_default_transferSizes[] = { INT_IN_PKTSIZE, };

/* Data Interface Alternate description(s)
 */
static __u8 mouse_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (mouse_default) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        0x03, 0x01, 0x02, 0x00,
};

static struct usb_alternate_description mouse_data_alternate_descriptions[] = {
      { iInterface:"Simple Mouse Interface - Interrupt",
              interface_descriptor: (struct usb_interface_descriptor *)&mouse_data_alternate_descriptor,
              classes:sizeof (mouse_hid_descriptors) / sizeof (__u8 *),
              class_list: mouse_hid_descriptors,
	      endpoints:sizeof (mouse_default) / sizeof(__u8 *),
              endpoint_list: mouse_default,
              endpoint_transferSizes_list: mouse_default_transferSizes,
      },
};

/* Interface description(s)
 */
static struct usb_interface_description mouse_interfaces[] = {
      { alternates:sizeof (mouse_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:mouse_data_alternate_descriptions,},
};


/* Configuration description(s)
 */

static __u8 mouse_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (mouse_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};

struct usb_configuration_description mouse_description[] = {
      {       
              configuration_descriptor: (struct usb_configuration_descriptor *)mouse_configuration_descriptor,
              iConfiguration:"USB Simple Serial Configuration",
	      interfaces:sizeof (mouse_interfaces) / sizeof (struct usb_interface_description),
              interface_list:mouse_interfaces,},
};

/* Device Description
 */

static __u8 mouse_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 
        0x00, 0x02, // bcdUSB
        0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 
        0x00, 0x11, // bcdDevice
        0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description mouse_device_description = {
        device_descriptor: (struct usb_device_descriptor *)mouse_device_descriptor,
	idVendor:CONFIG_USBD_VENDORID,
	idProduct:CONFIG_USBD_PRODUCTID,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
	iProduct:CONFIG_USBD_PRODUCT_NAME,
};



/* Transmit Function - called by serproto ****************************************************** */

static int mouse_xmit_data (int interface, __u8 *data, int data_length)
{
	int port = 0;		// XXX compound device
	struct usb_mouse_private *mouse_private;
	struct usb_device_instance *device;
	struct urb *urb;
	int packet_length = data_length;

	dbg_tx (2, "data: %p data_length: %d", data, data_length);

	if ((mouse_private = get_mouse_private (interface)) == NULL) {
		dbg_tx (0, "cannot recover serial private");
		return -EINVAL;
	}

	if ((device = mouse_private->device) == NULL) {
		dbg_tx (0, "cannot recover serial private device");
		return -EINVAL;
	}
	// XXX Check if we are busy

	if ((urb = usbd_alloc_urb (mouse_private->device, INT_IN_ENDPOINT, 0)) == NULL) {
		dbg_tx (0, "failed to alloc urb");
		return -EINVAL;
	}

	urb->buffer = data;
	urb->actual_length = packet_length;


	//dbgPRINTmem(dbgflg_usbdfd_tx,3,urb->buffer,urb->actual_length);
	dbgPRINTmem (dbgflg_usbdfd_tx, 3, urb->buffer, INT_IN_PKTSIZE + 4);

	// push it down into the usb-device layer
	return usbd_send_urb (urb);
}

/* mouse_urb_sent - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 */
int mouse_urb_sent (struct urb *urb, int rc)
{
	int port = 0;		// XXX compound device
	struct usb_device_instance *device = urb->device;
	struct usb_mouse_private *mouse_private =
	    (device->active_function_instance)->privdata;

	dbg_tx (2, "%s length: %d", urb->device->name, urb->actual_length);
	serproto_done (mouse_private->interface, urb->buffer, urb->actual_length, 0);
	usbd_dealloc_urb (urb);
	return 0;
}


/* USB Device Functions ************************************************************************ */

/* mouse_event_irq - process a device event
 *
 */
void mouse_event_irq (struct usb_device_instance *device, usb_device_event_t event, int data)
{
	int port = 0;		// XXX compound device
	struct usb_function_instance *function;
	unsigned int flags;

	if ((function = device->active_function_instance) == NULL) {
		dbg_usbe (1, "no function");
		return;
	}

	dbg_usbe (3, "");
        switch (event) {
        case DEVICE_UNKNOWN:
        case DEVICE_INIT: 
        case DEVICE_CREATE:     
        case DEVICE_HUB_CONFIGURED:
        case DEVICE_RESET:
        case DEVICE_ADDRESS_ASSIGNED:
        case DEVICE_CONFIGURED:
        case DEVICE_DE_CONFIGURED:
        case DEVICE_SET_INTERFACE:
        case DEVICE_SET_FEATURE:
        case DEVICE_CLEAR_FEATURE:
        case DEVICE_BUS_INACTIVE:
        case DEVICE_BUS_ACTIVITY:
        case DEVICE_POWER_INTERRUPTION:
        case DEVICE_HUB_RESET:
        case DEVICE_DESTROY:
        case DEVICE_FUNCTION_PRIVATE:
                dbg_usbe (1,"%s data: %d", usbd_device_events[event], data);
                break;
        default:
                dbg_usbe (1,"%s", usbd_device_events[DEVICE_UNKNOWN]);
                break;
        }
	switch (event) {

	case DEVICE_UNKNOWN:
	case DEVICE_INIT:
		break;

	case DEVICE_CREATE:
		{
			int i;
			int interface;
			struct usb_mouse_private *mouse_private;

			// There is no way to indicate error, so make this unconditional
			// and undo it in the DESTROY event unconditionally as well.
			// It the responsibility of the USBD core and the bus interface
			// to see that there is a matching DESTROY for every CREATE.
			// XXX XXX MOD_INC_USE_COUNT;  // Before any sleepable fns such as ckmalloc
			// XXX XXX dbg_init(0,"CREATE sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));

			// sanity checks
			if (mouse_created >= MAX_INTERFACES) {
				dbg_usbe (1,
					  "---> CREATE %s mouse_created >= MAX_INTERFACES %d %d",
					  device->name, mouse_created, MAX_INTERFACES);
				dbg_init (0, "CREATE Z1 sc=%d uc=%d", mouse_created,
					  GET_USE_COUNT (THIS_MODULE));
				return;
			}

			write_lock_irqsave (&mouse_rwlock, flags);
			for (i = 0; i < MAX_INTERFACES; i++) {
				if (mouse_private_array[i] == NULL) {
					break;
				}
			}
			if (i >= MAX_INTERFACES) {
				write_unlock_irqrestore (&mouse_rwlock, flags);
				dbg_usbe (1, "---> CREATE %s no free interfaces %d %d",
					  device->name, i, MAX_INTERFACES);
				dbg_init (0, "CREATE Z2 sc=%d uc=%d", mouse_created,
					  GET_USE_COUNT (THIS_MODULE));
				return;
			}
			mouse_created++;

			// allocate private data
			if ((mouse_private =
			     ckmalloc (sizeof (struct usb_mouse_private), GFP_ATOMIC)) == NULL) {
				mouse_created--;
				//MOD_DEC_USE_COUNT;
				//dbg_init(0,"CREATE X1 sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));
				write_unlock_irqrestore (&mouse_rwlock, flags);
				dbg_usbe (1, "---> CREATE malloc failed %s", device->name);
				return;
			}

			mouse_private->rwlock = RW_LOCK_UNLOCKED;
			mouse_private_array[i] = mouse_private;
			write_unlock_irqrestore (&mouse_rwlock, flags);

			dbg_usbe (1, "---> calling serproto_create(%s,-,%u,%u)", "ttyUSBx",
				  INT_IN_PKTSIZE, txqueue_urbs);

			if ((interface = serproto_create ("ttyUSBx", mouse_xmit_data, (INT_IN_PKTSIZE),
							  txqueue_urbs, 100, 0, 0)) < 0) 
                        {
				// lock and modify device array
				write_lock_irqsave (&mouse_rwlock, flags);
				lkfree (mouse_private);
				mouse_created--;
				//MOD_DEC_USE_COUNT;
				//dbg_init(0,"CREATE X2 sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));
				write_unlock_irqrestore (&mouse_rwlock, flags);
				dbg_usbe (1, "---> serproto_create FAILED");
				return;
			}
			// lock and modify device array
			write_lock_irqsave (&mouse_rwlock, flags);
			mouse_private->interface = interface;
			mouse_private->device = device;
			write_unlock_irqrestore (&mouse_rwlock, flags);

			function->privdata = mouse_private;

			dbg_usbe (1, "---> START %s privdata assigned: %p interface: %d",
				  device->name, mouse_private, interface);

			return;
		}
		break;

	case DEVICE_HUB_CONFIGURED:
		break;
	case DEVICE_RESET:
		break;
	case DEVICE_ADDRESS_ASSIGNED:
		break;
	case DEVICE_CONFIGURED:
		{
			struct usb_mouse_private *mouse_private;
			int interface;
			if ((mouse_private =
			     (device->active_function_instance)->privdata) == NULL) {
				return;
			}
			interface = mouse_private->interface;
			if (interface < 0 || interface >= MAX_INTERFACES) {
				return;
			}
			serproto_control (interface, SERPROTO_CONNECT);
		}
		break;
	case DEVICE_SET_INTERFACE:
		break;
	case DEVICE_SET_FEATURE:
		break;
	case DEVICE_CLEAR_FEATURE:
		break;
	case DEVICE_DE_CONFIGURED:
		{
			struct usb_mouse_private *mouse_private;
			int interface;
			if ((mouse_private =
			     (device->active_function_instance)->privdata) == NULL) {
				return;
			}
			interface = mouse_private->interface;
			if (interface < 0 || interface >= MAX_INTERFACES) {
				return;
			}
			serproto_control (interface, SERPROTO_DISCONNECT);
		}
		break;
	case DEVICE_BUS_INACTIVE:
		break;
	case DEVICE_BUS_ACTIVITY:
		break;
	case DEVICE_POWER_INTERRUPTION:
		break;
	case DEVICE_HUB_RESET:
		break;

	case DEVICE_DESTROY:
		{
			struct usb_mouse_private *mouse_private;
			int interface;

			if ((mouse_private =
			     (device->active_function_instance)->privdata) == NULL) {
				dbg_usbe (1, "---> DESTROY %s mouse_private null", device->name);
				// XXX XXX MOD_DEC_USE_COUNT;
				// XXX XXX dbg_init(0,"DESTROY Z1 sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));
				return;
			}
			dbg_usbe (1, "---> DESTROY %s mouse_private %p", device->name,
				  mouse_private);
			interface = mouse_private->interface;

			if (interface < 0 || interface >= MAX_INTERFACES) {
				// XXX XXX MOD_DEC_USE_COUNT;
				// XXX XXX dbg_init(0,"DESTROY Z2 sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));
				return;
			}

			if (mouse_private_array[interface] != mouse_private) {
				// XXX XXX MOD_DEC_USE_COUNT;
				// XXX XXX dbg_init(0,"DESTROY Z3 sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));
				return;
			}

			write_lock_irqsave (&mouse_rwlock, flags);
			mouse_private_array[interface] = NULL;
			lkfree (mouse_private);
			mouse_created--;
			write_unlock_irqrestore (&mouse_rwlock, flags);

			serproto_destroy (interface);
			// XXX XXX MOD_DEC_USE_COUNT;
			// XXX XXX dbg_init(0,"DESTROY sc=%d uc=%d",mouse_created,GET_USE_COUNT(THIS_MODULE));

			dbg_usbe (1, "---> STOP %s", device->name);
			return;
		}
		break;

	case DEVICE_FUNCTION_PRIVATE:
		break;
	}
}

struct usb_function_operations function_ops = {
	event_irq:mouse_event_irq,
	urb_sent:mouse_urb_sent
};

struct usb_function_driver function_driver = {
	name:"function prototype",
	ops:&function_ops,
	device_description:&mouse_device_description,
	configurations:sizeof (mouse_description) / sizeof (struct usb_configuration_description),
	configuration_description:mouse_description,
	this_module:THIS_MODULE,
};


/*
 * mouse_modinit - module init
 *
 */
static int mouse_modinit (void)
{
	debug_option *op = find_debug_option (dbg_table, "ser");

	printk (KERN_INFO "%s (%s)\n", __usbd_module_info, dbg);

	printk (KERN_INFO "vendor_id: %04x product_id: %04x\n", vendor_id, product_id);
	printk (KERN_INFO "dbg: %s\n", dbg);

	if (NULL != op) {
		op->sub_table = serproto_get_dbg_table ();
	}
	if (0 != scan_debug_options ("mouse_fd", dbg_table, dbg)) {
		return (-EINVAL);
	}

	if (vendor_id) {
		mouse_device_description.idVendor = vendor_id;
	}
	if (product_id) {
		mouse_device_description.idProduct = product_id;
	}
	if (serproto_modinit ("ttyUSBn", MAX_INTERFACES)) {
		return -EINVAL;
	}

	// register us with the usb device support layer
	if (usbd_register_function (&function_driver)) {
		serproto_modexit ();
		return -EINVAL;
	}
	dbg_loop (1, "LOOPBACK mode");

	return 0;
}


/* mouse_modexit - module cleanup
 */
static void __exit mouse_modexit (void)
{
	// de-register us with the usb device support layer
	usbd_deregister_function (&function_driver);

	// tell the serproto library to exit
	serproto_modexit ();

}

module_init (mouse_modinit);
module_exit (mouse_modexit);
