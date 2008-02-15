/*
 * linux/drivers/usbd/usbd.c - USB Device Core Layer
 *
 * Copyright (c) 2002, 2003 Belcarra
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


/*
 * Notes...
 *
 * 1. The order of loading must be:
 *
 *        usb-device
 *        usb-function(s)
 *        usb-bus(s)
 *
 * 2. Loading usb-function modules allows them to register with the usb-device
 * layer. This makes their usb configuration descriptors available. Currently
 * function modules cannot be loaded after a bus module has been loaded.
 *
 * 3. Loading usb-bus modules causes them to register with the usb-device
 * layer and then enable their upstream port. This in turn will cause the
 * upstream host to enumerate this device. Note that bus modules can create
 * multiple devices, one per physical interface.
 *
 * 4. The usb-device layer provides a default configuration for endpoint 0 for
 * each device. 
 *
 * 5. Additional configurations are added to support the configuration
 * function driver when entering the configured state.
 *
 * 6. The usb-device maintains a list of configurations from the loaded
 * function drivers. It will provide this to the USB HOST as part of the
 * enumeration process and will add someof them as the active configuration as
 * per the USB HOST instructions.  
 *
 * 6. Two types of bus interface modules can be implemented: simple and compound.
 *
 * 7. Simple bus interface modules can only support a single function module. The
 * first function module is used.
 *
 * 8. Compound bus interface modules can support multiple functions. When implemented
 * these will use all available function modules.
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#include "usbd-build.h"
#include "usbd-module.h"

MODULE_AUTHOR ("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION ("USB Device Core Support");
MODULE_LICENSE("GPL");


USBD_MODULE_INFO ("usbdcore 0.1");

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

#include "usbd.h"
#include "usbd-debug.h"
#include "usbd-func.h"
#include "usbd-bus.h"
#include "usbd-inline.h"

#include "hotplug.h"

#define MAX_INTERFACES 2

/* Module Parameters ************************************************************************* */

MODULE_PARM (dbg, "s");

MODULE_PARM_DESC (dbg, "debug parameters");

int maxstrings = 20;
static char *dbg = NULL;

/* Debug switches (module parameter "dbg=...") *********************************************** */

int dbgflg_usbdcore_init;
int dbgflg_usbdcore_ep0;
int dbgflg_usbdcore_rx;
int dbgflg_usbdcore_tx;
int dbgflg_usbdcore_usbe;
int dbgflg_usbdcore_urbmem;

static debug_option dbg_table[] = {
	{&dbgflg_usbdcore_init, NULL, "init", "initialization and termination"},
	{&dbgflg_usbdcore_ep0, NULL, "ep0", "End Point 0 (setup) packet handling"},
	{&dbgflg_usbdcore_rx, NULL, "rx", "USB RX (host->device) handling"},
	{&dbgflg_usbdcore_tx, NULL, "tx", "USB TX (device->host) handling"},
	{&dbgflg_usbdcore_usbe, NULL, "usbe", "USB events"},
	{&dbgflg_usbdcore_urbmem, NULL, "mem", "URB memory allocation"},
#if 0
	{NULL, NULL, "hot", "hotplug actions"},
	{NULL, NULL, "mon", "USB conection monitor"},
#endif
	{NULL, NULL, NULL, NULL}
};

#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_init,lvl,fmt,##args)
#define dbg_ep0(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_ep0,lvl,fmt,##args)
#define dbg_rx(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_rx,lvl,fmt,##args)
#define dbg_tx(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_tx,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_usbe,lvl,fmt,##args)
#define dbg_urbmem(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_urbmem,lvl,fmt,##args)

/* Global variables ************************************************************************** */

int usb_devices;

extern struct usb_function_driver ep0_driver;

int registered_functions;
int registered_devices;


/* List support functions ******************************************************************** */

LIST_HEAD (function_drivers);	// list of all registered function modules
LIST_HEAD (devices);		// list of all registered devices


static inline struct usb_function_driver *list_entry_func (const struct list_head *le)
{
	return list_entry (le, struct usb_function_driver, drivers);
}

static inline struct usb_device_instance *list_entry_device (const struct list_head *le)
{
	return list_entry (le, struct usb_device_instance, devices);
}


/* Support Functions ************************************************************************* */

static int mallocs;

void *_ckmalloc (char *func, int line, int n, int f)
{
	void *p;
	if ((p = kmalloc (n, f)) == NULL) {
		return NULL;
	}
	memset (p, 0, n);
        ++mallocs;
        //printk(KERN_INFO"ckmalloc: %p %s %d %d\n", p, func, line, mallocs);
	return p;
}

char *_lstrdup (char *func, int line, char *str)
{
	int n;
	char *s;
	if (str && (n = strlen (str) + 1) && (s = kmalloc (n, GFP_ATOMIC))) {
                ++mallocs;
                //printk(KERN_INFO"lstrdup: %s %p %d %d\n", s, func, line, mallocs);
		return strcpy (s, str);
	}
	return NULL;
}

void _lkfree (char *func, int line, void *p)
{
	if (p) {
                --mallocs;
                //printk(KERN_INFO"lkfree: %p %s %d %d\n", p, func, line, mallocs);
		kfree (p);
                if (mallocs < 0) {
                        printk(KERN_INFO"lkfree: %p %s %d %d mallocs less zero!\n", p, func, line, mallocs);
                }
	}
        else {
                printk(KERN_INFO"lkfree: %s %d NULL\n", func, line);
        }
}


/* Descriptor support functions ************************************************************** */



/**
 * usbd_cancel_urb - cancel an urb being sent
 * @urb: pointer to an urb structure
 *
 * Used by a USB Function driver to cancel an urb that is being
 * sent.
 */
int usbd_cancel_urb (struct usb_device_instance *device, struct urb *urb)
{
	if (!device->bus->driver->ops->cancel_urb) {
		// XXX should we do usbd_dealloc_urb(urb);
		return 0;
	}
	return device->bus->driver->ops->cancel_urb (urb);
}

/**
 * usbd_alloc_urb_data - allocate urb data buffer
 * @urb: pointer to urb
 * @len: size of buffer to allocate
 *
 * Allocate a data buffer for the specified urb structure.
 */
int usbd_alloc_urb_data (struct urb *urb, int len)
{
	len += 2;
	urb->buffer_length = len;
	urb->actual_length = 0;
	if (len == 0) {
		return 0;
	}

	if (urb->endpoint && urb->endpoint->endpoint_address && urb->function_instance && 
                urb->function_instance->function_driver->ops->alloc_urb_data) 
        {
		return urb->function_instance->function_driver->ops->alloc_urb_data (urb, len);
	}
	return !(urb->buffer = ckmalloc (len, GFP_ATOMIC));
}


/**
 * usbd_alloc_urb - allocate an URB appropriate for specified endpoint
 * @device: device instance
 * @endpoint: endpoint 
 * @length: size of transfer buffer to allocate
 *
 * Allocate an urb structure. The usb device urb structure is used to
 * contain all data associated with a transfer, including a setup packet for
 * control transfers.
 *
 */
struct urb *usbd_alloc_urb (struct usb_device_instance *device, __u8 endpoint_address, int length)
{
        struct usb_function_instance *function_instance = device->active_function_instance;
	int i;

	if (!function_instance) {
		// MMM no active function, better not alloc an urb
		return(NULL);
	}

	for (i = 0; i < device->bus->driver->max_endpoints; i++) {
		struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

		//dbg_urbmem (2, "i=%d epa=%d want %d", i, (endpoint->endpoint_address & 0x7f), endpoint_address);

		if ((endpoint->endpoint_address & 0x7f) == endpoint_address) {

			struct urb *urb;

			if (!(urb = ckmalloc (sizeof (struct urb), GFP_ATOMIC))) {
				return NULL;
			}

			urb->endpoint = endpoint;
			urb->device = device;
			urb->function_instance = function_instance;
			urb_link_init (&urb->link);

			if (length) {
				if (usbd_alloc_urb_data (urb, length)) {
					lkfree (urb);
					return NULL;
				}
			} 
                        else {
				urb->buffer_length = urb->actual_length = 0;
                        }
			return urb;
		}
	}
	return NULL;
}

#if 0
/**
 * usbd_alloc_urb_pst - allocate an URB appropriate for specified endpoint
 * @device: device instance
 * @function_instance: device instance
 * @endpoint: endpoint 
 * @length: size of transfer buffer to allocate
 *
 * Allocate an urb structure. The usb device urb structure is used to
 * contain all data associated with a transfer, including a setup packet for
 * control transfers.
 *
 */
struct urb *usbd_alloc_urb_pst (struct usb_device_instance *device, struct usb_function_instance *function_instance, 
        __u8 endpoint_address, int length)
{
	int i;

	for (i = 0; i < device->bus->driver->max_endpoints; i++) {
		struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

		dbg_urbmem (2, "i=%d epa=%d want %d", i, (endpoint->endpoint_address & 0x7f), endpoint_address);
		if ((endpoint->endpoint_address & 0x7f) == endpoint_address) {

			struct urb *urb;

			dbg_urbmem (2, "[%d]: endpoint: %p %02x length: %d", i, endpoint, endpoint->endpoint_address, length);

			if (!(urb = ckmalloc (sizeof (struct urb), GFP_ATOMIC))) {
				dbg_urbmem (0, "ckmalloc(%u,GFP_ATOMIC) failed", sizeof (struct urb));
				return NULL;
			}

			urb->endpoint = endpoint;
			urb->device = device;
			urb->function_instance = function_instance;
			urb_link_init (&urb->link);

			if (length) {
				if (usbd_alloc_urb_data (urb, length)) {
					dbg_urbmem (0, "usbd_alloc_urb_data(%u,GFP_ATOMIC) failed", length);
					kfree (urb);
					return NULL;
				}
			} else {
				urb->buffer_length = urb->actual_length = 0;
                        }
                        dbg_urbmem(1, "[%d] urb: %p len: %d", endpoint_address, urb, length);
			return urb;
		}
	}
	return NULL;
}
/************end of Levis's********************/
#endif

/**
 * usbd_dealloc_urb - deallocate an URB and associated buffer
 * @urb: pointer to an urb structure
 *
 * Deallocate an urb structure and associated data.
 */
void usbd_dealloc_urb (struct urb *urb)
{
	if (urb) {
		if (urb->buffer) {
			if (urb->function_instance && urb->function_instance->function_driver->ops->dealloc_urb_data) {
				urb->function_instance->function_driver->ops->dealloc_urb_data (urb);
			} 
			else {
				lkfree (urb->buffer);
			}
		}
		lkfree (urb);
	}
}

/**
 * usbd_device_event - called to respond to various usb events
 * @device: pointer to struct device
 * @event: event to respond to
 *
 * Used by a Bus driver to indicate an event.
 */
void usbd_device_event_irq (struct usb_device_instance *device, usb_device_event_t event, int data)
{
        struct usb_function_instance *function;
        //struct usb_function_driver *function_driver;
	if ( device == NULL ) {
		return;
	}

        dbg_usbe (1,"%d", event);
	switch (event) {
	case DEVICE_UNKNOWN:
		break;
	case DEVICE_INIT:
		device->device_state = STATE_INIT;
		break;

	case DEVICE_CREATE:
		device->device_state = STATE_ATTACHED;
		break;

	case DEVICE_HUB_CONFIGURED:
		device->device_state = STATE_POWERED;
		break;

	case DEVICE_RESET:
		// MMM check future_function_instance and switch if present
		if (NULL != (function = device->future_function_instance)) {
			device->future_function_instance = NULL;
			/* MMM watch out that these events don't create
			   a lightning quick disc/connect pair. */
			device->function_state = FN_STATE_SWITCHING;
			// A little recursion never hurt anyone (well, hardly ever).
			usbd_device_event_irq(device,DEVICE_DESTROY,data);
			// Start up the new function (closes old one)
		        // MMM check mallocs OK (GFP atomic)?
			usbd_function_init(device->bus,device,function->function_driver,function);
			// Catch up on events
			usbd_device_event_irq(device,DEVICE_INIT,data);
			usbd_device_event_irq(device,DEVICE_CREATE,data);
			usbd_device_event_irq(device,DEVICE_HUB_CONFIGURED,data);
			device->function_state = FN_STATE_RUNNING;
		}
		device->device_state = STATE_DEFAULT;
		device->address = 0;
		break;

	case DEVICE_ADDRESS_ASSIGNED:
		device->device_state = STATE_ADDRESSED;
		break;

	case DEVICE_CONFIGURED:
		device->device_state = STATE_CONFIGURED;
		break;

	case DEVICE_DE_CONFIGURED:
		device->device_state = STATE_ADDRESSED;
		break;

	case DEVICE_BUS_INACTIVE:
                device->status = (device->status != USBD_CLOSING)? USBD_SUSPENDED : device->status; 
		break;
	case DEVICE_BUS_ACTIVITY:
                device->status = (device->status != USBD_CLOSING)? USBD_OK : device->status; 
		break;

	case DEVICE_SET_INTERFACE:
		break;
	case DEVICE_SET_FEATURE:
		break;
	case DEVICE_CLEAR_FEATURE:
		break;

	case DEVICE_POWER_INTERRUPTION:
		device->device_state = STATE_POWERED;
		break;
	case DEVICE_HUB_RESET:
		device->device_state = STATE_ATTACHED;
		break;
	case DEVICE_DESTROY:
		device->device_state = STATE_UNKNOWN;
		break;
	case DEVICE_FUNCTION_PRIVATE:
		break;
	case DEVICE_CABLE_DISCONNECTED:
		break;
	}

	if (device->bus && device->bus->driver->ops->device_event) {
		device->bus->driver->ops->device_event (device, event, data);
	}

	if (device->ep0 && device->ep0->function_driver->ops->event_irq) {
		device->ep0->function_driver->ops->event_irq (device, event, data);
	}

	if ((function = device->active_function_instance) && 
                        function->function_driver->ops &&
                        function->function_driver->ops->event_irq) 
        {
                function->function_driver->ops->event_irq(device, event, data);
	}
}

void usbd_device_event (struct usb_device_instance *device, usb_device_event_t event, int data)
{
	unsigned long flags;
	local_irq_save (flags);
	usbd_device_event_irq (device, event, data);
	local_irq_restore (flags);
}

/**
 * usbd_endpoint_halted
 * @device: point to struct usb_device_instance
 * @endpoint: endpoint to check
 *
 * Return non-zero if endpoint is halted.
 */
int usbd_endpoint_halted (struct usb_device_instance *device, int endpoint)
{
	if (!device->bus->driver->ops->endpoint_halted) {
		return -EINVAL;
	}
	return device->bus->driver->ops->endpoint_halted (device, endpoint);
}


/**
 * usbd_device_feature - set usb device feature
 * @device: point to struct usb_device_instance
 * @endpoint: which endpoint
 * @feature: feature value
 *
 * Return non-zero if endpoint is halted.
 */
int usbd_device_feature (struct usb_device_instance *device, int endpoint, int feature)
{
	if (!device->bus->driver->ops->device_feature) {
		return -EINVAL;
	}
	return device->bus->driver->ops->device_feature (device, endpoint, feature);
}



/* Module init ******************************************************************************* */


unsigned intset (unsigned int a, unsigned b)
{
        return (a ? a : b);
}

char *strset (char *s, char *d)
{
        return ((s && strlen (s) ? s : d));
}

static int __init usbd_device_init (void)
{
        printk (KERN_INFO "usbdcore: %s (dbg=\"%s\")\n",
		 __usbd_module_info, dbg ? dbg : "");

        if (0 != scan_debug_options ("usb-device", dbg_table, dbg)) {
                return (-EINVAL);
        }
        return 0;
}

static void __exit usbd_device_exit (void)
{
        printk (KERN_INFO "usbdcore: %s exiting\n", __usbd_module_info);

        if (mallocs) {
                printk(KERN_INFO"%s: mallocs non-zero! %d\n", __FUNCTION__, mallocs);
        }
}

module_init (usbd_device_init);
module_exit (usbd_device_exit);

