/*
 * linux/drivers/usbd/usbd-bus.c - USB Device Prototype
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

#include <linux/config.h>
#define USBD_CONFIG_NOWAIT_DEREGISTER_DEVICE 1
#include <linux/module.h>

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

extern int usb_devices;
extern struct usb_function_driver ep0_driver;
extern int registered_functions;

extern struct list_head function_drivers;
extern struct list_head devices;


static __inline__ struct usb_function_driver *list_entry_func (const struct list_head *le)
{
	return list_entry (le, struct usb_function_driver, drivers);
}

static __inline__ struct usb_device_instance *list_entry_device (const struct list_head *le)
{
	return list_entry (le, struct usb_device_instance, devices);
}

// Debug stuff - dbgflg_usbdbi_init must be defined in the main part of the bus driver (e.g. *_bi/init.c)
int dbgflg_usbdbi_init;
#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_init,lvl,fmt,##args)

extern int registered_functions;
extern int registered_devices;


/**
 * usbd_fill_rcv - fill the rx recyle queue with empty urbs
 * @endpoint:
 *
 * Fill the recycle queue so that receive has some empty urbs to use.
 */
void usbd_fill_rcv (struct usb_device_instance *device, struct usb_endpoint_instance *endpoint,
		    int num)
{
	int i;
	unsigned buffersize;

	//dbg_init (1, "endpoint: %x num: %d", endpoint->endpoint_address, num);

	if (endpoint->rcv_packetSize) {

		// we need to allocate URBs with enough room for the endpoints transfersize plus one rounded 
		// up to the next packetsize

		buffersize =
		    ((endpoint->rcv_transferSize +
		      endpoint->rcv_packetSize) / endpoint->rcv_packetSize) *
		    endpoint->rcv_packetSize;

                // XXX should we do this here?
                usbd_flush_rcv(endpoint);
		for (i = 0; i < num; i++) {
			usbd_recycle_urb (usbd_alloc_urb (device, endpoint->endpoint_address, buffersize));
		}
	} 
}

/**
 * usbd_flush_rcv - flush rcv
 * @endpoint:
 *
 * Iterate across the rx list and dispose of any urbs.
 */
void usbd_flush_rcv (struct usb_endpoint_instance *endpoint)
{
	struct urb *rcv_urb = NULL;
	unsigned long flags;

        //dbg_init (1, "endpoint: %x", endpoint->endpoint_address);
	if (endpoint && endpoint->endpoint_address) {
		local_irq_save (flags);

		if ((rcv_urb = endpoint->rcv_urb)) {
			endpoint->rcv_urb = NULL;
			usbd_dealloc_urb (rcv_urb);
		}

		while ((rcv_urb = first_urb_detached (&endpoint->rdy))) {
			usbd_dealloc_urb (rcv_urb);
		}
		local_irq_restore (flags);
	}
}

/**
 * usbd_flush_tx - flush tx urbs from endpoint
 * @endpoint:
 *
 * Iterate across the tx list and cancel any outstanding urbs.
 */
void usbd_flush_tx (struct usb_endpoint_instance *endpoint)
{
	struct urb *send_urb = NULL;
	unsigned long flags;

        //dbg_init (1, "endpoint: %x", endpoint->endpoint_address);
	if (endpoint && endpoint->endpoint_address) {
		local_irq_save (flags);

                //dbg_init (1, "endpoint: %d tx_urb: %p tx: %p", 
                //                endpoint->endpoint_address, endpoint->tx_urb, endpoint->tx);

		if ((send_urb = endpoint->tx_urb)) {
                        usbd_cancel_urb(send_urb->device, send_urb);
			//endpoint->tx_urb = NULL;
			//usbd_urb_sent_irq (send_urb, SEND_FINISHED_ERROR);
		}

		while ((send_urb = first_urb_detached (&endpoint->tx))) {
			usbd_urb_sent_irq (send_urb, SEND_FINISHED_ERROR);
		}
		local_irq_restore (flags);
	}
}

/**
 * usbd_flush_ep - flush urbs from endpoint
 * @endpoint:
 *
 * Iterate across the approrpiate tx or rcv list and cancel any outstanding urbs.
 */
void usbd_flush_ep (struct usb_endpoint_instance *endpoint)
{
        //dbg_init (1, "endpoint: %x", endpoint->endpoint_address);
	if (endpoint) {
		if (endpoint->endpoint_address & 0x80) {
			usbd_flush_tx (endpoint);
		} 
                else {
			usbd_flush_rcv (endpoint);
		}
	}
}

/**
 * usbd_device_bh - 
 * @data:
 *
 * Bottom half handler to process sent or received urbs.
 */
void usbd_device_bh (void *data)
{
        struct usb_device_instance *device;

        if ((device = data)) {
                int i;

                for (i = 0; i < device->bus->driver->max_endpoints; i++) {
                        struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + i;

                        // process received urbs
                        if (endpoint->endpoint_address && 
                                        (endpoint->endpoint_address & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) 
                        {
                                struct urb *urb;
                                while ((urb = first_urb_detached (&endpoint->rcv))) {
                                        if (!urb->function_instance || 
                                                        !urb->function_instance->function_driver->ops->recv_urb) 
                                        {
                                                //dbg_init (0, "usbd_device_bh: no recv_urb function");
                                                usbd_recycle_urb (urb);
                                        } 
                                        else if (urb->function_instance->function_driver->ops->recv_urb (urb)) {
						usbd_resume_urb (urb);	//by Levis
						break;
                                        }
                                }
                        }
                        // process sent urbs
                        if (endpoint->endpoint_address && 
                                        (endpoint->endpoint_address & USB_REQ_DIRECTION_MASK) == USB_REQ_DEVICE2HOST) 
                        {
                                struct urb *urb;
                                while ((urb = first_urb_detached (&endpoint->done))) {
                                        if (!urb->function_instance || 
                                                        !urb->function_instance->function_driver->ops->urb_sent 
                                           )
                                        {
                                                //dbg_init (0, "usbd_device_bh: no urb_sent function");
                                                usbd_dealloc_urb (urb);
                                        }
                                        if ( urb->function_instance->function_driver->ops->urb_sent (urb, urb-> status))
                                        {
                                                //dbg_init (0, "usbd_device_bh: urb_sent function failed");
                                                usbd_dealloc_urb (urb);
                                        }
                                }
                        }

                }

#if defined(CONFIG_SA1100_COLLIE) && defined(CONFIG_PM)
                {
                        // Please clear autoPowerCancel flag during the transmission and  the reception.
                        // XXX XXX extern 
                        int autoPowerCancel;
                        autoPowerCancel = 0;	// Auto Power Off Cancel
                }
#endif

#if defined(CONFIG_SA1110_CALYPSO) && defined(CONFIG_PM)
                // Shouldn't need to make this atomic, all we need is a change indicator
                device->usbd_rxtx_timestamp = jiffies;
#endif

                if (device->status == USBD_CLOSING) {
                        device->device_bh.data = NULL;
                }
        }
}


/* usb-device USB BUS INTERFACE generic functions ******************************************** */

/**
 * usbd_register_bus - called by a USB BUS INTERFACE driver to register a bus driver
 * @driver: pointer to bus driver structure
 *
 * Used by a USB Bus interface driver to register itself with the usb device
 * layer.
 */
struct __init usb_bus_instance *usbd_register_bus (struct usb_bus_driver *driver)
{
	struct usb_bus_instance *bus;
	int i;

	if ((bus = ckmalloc (sizeof (struct usb_bus_instance), GFP_ATOMIC)) == NULL) {
		return NULL;
	}
	bus->driver = driver;
	if (!(bus->endpoint_array =
	     ckmalloc (sizeof (struct usb_endpoint_instance) * bus->driver->max_endpoints, GFP_ATOMIC))) 
        {
		lkfree (bus);
		return NULL;
	}

	for (i = 0; i < bus->driver->max_endpoints; i++) {

		struct usb_endpoint_instance *endpoint = bus->endpoint_array + i;

		urb_link_init (&endpoint->events);
		urb_link_init (&endpoint->rcv);
		urb_link_init (&endpoint->rdy);
		urb_link_init (&endpoint->tx);
		urb_link_init (&endpoint->done);
	}

	return bus;
}

/**
 * usbd_deregister_bus - called by a USB BUS INTERFACE driver to deregister a bus driver
 * @bus: pointer to bus driver instance
 *
 * Used by a USB Bus interface driver to de-register itself with the usb device
 * layer.
 */
void __exit usbd_deregister_bus (struct usb_bus_instance *bus)
{
	lkfree (bus->endpoint_array);
	lkfree (bus);
}

/* usb-device USB Device generic functions *************************************************** */

/**
 * usbd_register_device - called to create a virtual device
 * @name: name
 * @bus: pointer to struct usb_device_instance
 * @maxpacketsize: ep0 maximum packetsize
 *
 * Used by a USB Bus interface driver to create a virtual device.
 */
struct usb_device_instance *__init usbd_register_device (char *name, struct usb_bus_instance *bus,
							 int maxpacketsize)
{
	struct usb_device_instance *device;
	struct usb_function_instance *functions_instance_array;
	struct list_head *lhd;
	int num = usb_devices++;
	char buf[32];
	int function;

	// MMM bail early if there are obvious problems.
	if (registered_functions <= 0) {
		printk(KERN_ERR "No registered USB Device functions!\n");
		return(NULL);
	}

	// allocate a usb_device_instance structure
	if (!(device = ckmalloc (sizeof (struct usb_device_instance), GFP_ATOMIC))) {
		//dbg_init (0, "ckmalloc device failed");
		return NULL;
	}
	// create a name
	if (!name || !strlen (name)) {
		sprintf (buf, "usb%d", num);
		name = buf;
	}
	if ((device->name = lstrdup (name)) == NULL) {
                THROW(device_error);
	}

	device->device_state = STATE_CREATED;
	device->function_state = FN_STATE_RUNNING; // Normal operation
	device->status = USBD_OPENING;

	// allocate a usb_function_instance for ep0 default control function driver
	if ((device->ep0 = ckmalloc (sizeof (struct usb_function_instance), GFP_ATOMIC)) == NULL) {
                THROW(name_error);
	}
	device->ep0->function_driver = &ep0_driver;

	// allocate an array of usb configuration instances
	if ((functions_instance_array =
	     ckmalloc (sizeof (struct usb_function_instance) * registered_functions,
		       GFP_ATOMIC)) == NULL) {
                THROW(ep0_error);
	}

	device->functions = registered_functions;
	device->functions_instance_array = functions_instance_array;
	device->active_function_instance = NULL;
	device->future_function_instance = NULL;

	// connect us to bus
	device->bus = bus;

	// iterate across all of the function drivers to construct a complete list of configuration descriptors
	// XXX there may be more than one!
	function = 0;
	list_for_each (lhd, &function_drivers) {
		struct usb_function_driver *function_driver;
		function_driver = list_entry_func (lhd);
		// build descriptors
		//dbg_init (1, "calling function_init");
		printk(KERN_ERR "%s: checking [%s] (%d/%d)\n",
		       __FUNCTION__,function_driver->name,
		       function,device->functions);
                // XXX
		if (1 == device->functions) {
			// There is only one, might as well be active
			printk(KERN_ERR "%s: only one function, calling init\n",
			       __FUNCTION__);
			usbd_function_init (bus, device, function_driver, functions_instance_array + function);
		}
#ifdef CONFIG_USBD_MULTIFUNCTION_DEFAULT
		else {
			printk(KERN_ERR "%s: check [%s]==[%s]\n",
			       __FUNCTION__,function_driver->name,CONFIG_USBD_MULTIFUNCTION_DEFAULT);
			if (0 == strcmp(function_driver->name,CONFIG_USBD_MULTIFUNCTION_DEFAULT)) {
				printk(KERN_ERR "%s: match, calling init\n",
				       __FUNCTION__);
				usbd_function_init (bus, device, function_driver, functions_instance_array + function);
			}
		}
#endif
		// save 
		functions_instance_array[function].function_driver = function_driver;
                function++;
	}
	// MMM no longer needed...
        if (!function) {
                THROW(function_error);
        }

	/* MMM fallback multifunction is first one */
	if (1 < device->functions && NULL == device->active_function_instance) {
		printk(KERN_ERR "%s: fallback, calling init for [%s]\n",
		       __FUNCTION__,
		       functions_instance_array[0].function_driver->name);
		usbd_function_init (bus, device,
		                    functions_instance_array[0].function_driver,
		                    functions_instance_array + 0);
	}

	// device bottom half
	device->device_bh.routine = usbd_device_bh;
	device->device_bh.data = device;

	// add to devices queue
	list_add_tail (&device->devices, &devices);
	registered_devices++;

        CATCH(device_error) {

                CATCH(name_error) {

                        CATCH(ep0_error) {
                                CATCH(function_error) {
                                        lkfree(functions_instance_array);
                                }
                                lkfree (device->ep0);
                        }
                        lkfree (device->name);
                }
		lkfree (device);
		return NULL;
        }
	return device;
}

/**
 * usbd_deregister_device - called by a USB BUS INTERFACE driver to deregister a physical interface
 * @device: pointer to struct usb_device_instance
 *
 * Used by a USB Bus interface driver to destroy a virtual device.
 */
void __exit usbd_deregister_device (struct usb_device_instance *device)
{
	struct usb_function_instance *functions_instance_array;
	struct usb_function_instance *function_instance;

	//dbg_init (3, "%s start", device->bus->driver->name);

	// prevent any more bottom half scheduling
	device->status = USBD_CLOSING;

#ifdef USBD_CONFIG_NOWAIT_DEREGISTER_DEVICE
        /* We need to run the bottom halves at least once with the
           status == USBD_CLOSING in order to make sure all the
           URBS are freed (otherwise we have a memleak).  The
           bh's set the data pointer to NULL when they notice
           the USBD_CLOSING status. */
	if (device->device_bh.sync) {
        	// There is a bh queued.  The only way deal with this other
		// than waiting is to do the task.
		run_task_queue(&tq_immediate);
	}
	// Verify the status flag was detected.
	if (NULL != device->device_bh.data) {
		// It hasn't been detected yet, so queue the bh fn,
		queue_task(&device->device_bh, &tq_immediate);
		// then run it.
		run_task_queue(&tq_immediate);
		/* If the second time fails, something is wrong. */
		if (NULL != device->device_bh.data) {
			dbg_init(0, "run_task_queue(&tq_immediate) fails to clear device_bh");
		}
	}

	/* Leave the wait in as paranoia - in case one of the task operations
	   above fails to clear the queued task.  It should never happen, but ... */
#endif

	// wait for pending device and function bottom halfs to finish
	while (device->device_bh.data /*|| device->function_bh.data */) {

                if (device->device_bh.data) {
                        dbg_init(0, "waiting for usbd_device_bh %ld %p", device->device_bh.sync, device->device_bh.data);
			// This can probably be either, but for consistency's sake...
			queue_task(&device->device_bh, &tq_immediate);
                        // schedule_task(&device->device_bh);
                }
		schedule_timeout (10 * HZ);
	}

	// tell the function driver to close
	usbd_function_close (device);

	// disconnect from bus
	device->bus = NULL;

	// remove from devices queue
	list_del (&device->devices);

	// free function_instances

	if ((functions_instance_array = device->functions_instance_array)) {
		device->functions_instance_array = NULL;
		lkfree (functions_instance_array);
	}
	// de-configured ep0
	if ((function_instance = device->ep0)) {
		device->ep0 = NULL;
		lkfree (function_instance);
	}

	lkfree (device->name);
	lkfree (device);

	registered_devices--;
}

