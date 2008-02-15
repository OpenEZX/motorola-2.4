/*
 * linux/drivers/usbd/ep0.c
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Tom Rushworth <tbr@belcarra.com>, 
 *      Bruce Balden <balden@lineo.com>
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
 * This is the builtin ep0 control function. It implements all required functionality
 * for responding to control requests (SETUP packets).
 *
 * XXX 
 *
 * Currently we do not pass any SETUP packets (or other) to the configured
 * function driver. This may need to change.
 *
 * XXX
 */

#include <linux/config.h>
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
extern void udc_send_ack_to_set_conf_req(void);
extern int dbgflg_usbdcore_ep0;
#define dbg_ep0(lvl,fmt,args...) dbgPRINT(dbgflg_usbdcore_ep0,lvl,fmt,##args)

#if VERBOSE

char *usbd_device_events[] = {
	"DEVICE_UNKNOWN",
	"DEVICE_INIT",
	"DEVICE_CREATE",
	"DEVICE_HUB_CONFIGURED",
	"DEVICE_RESET",
	"DEVICE_ADDRESS_ASSIGNED",
	"DEVICE_CONFIGURED",
	"DEVICE_SET_INTERFACE",
	"DEVICE_SET_FEATURE",
	"DEVICE_CLEAR_FEATURE",
	"DEVICE_DE_CONFIGURED",
	"DEVICE_BUS_INACTIVE",
	"DEVICE_BUS_ACTIVITY",
	"DEVICE_POWER_INTERRUPTION",
	"DEVICE_HUB_RESET",
	"DEVICE_DESTROY",
	"DEVICE_FUNCTION_PRIVATE",
};

char *usbd_device_states[] = {
	"STATE_INIT",
	"STATE_CREATED",
	"STATE_ATTACHED",
	"STATE_POWERED",
	"STATE_DEFAULT",
	"STATE_ADDRESSED",
	"STATE_CONFIGURED",
	"STATE_UNKNOWN",
};

char *usbd_device_requests[] = {
        "GET STATUS",           // 0
        "CLEAR FEATURE",        // 1
        "RESERVED",             // 2
        "SET FEATURE",          // 3
        "RESERVED",             // 4
        "SET ADDRESS",          // 5
        "GET DESCRIPTOR",       // 6
        "SET DESCRIPTOR",       // 7
        "GET CONFIGURATION",    // 8
        "SET CONFIGURATION",    // 9
        "GET INTERFACE",        // 10
        "SET INTERFACE",        // 11
        "SYNC FRAME",           // 12
};

char *usbd_device_descriptors[] = {
        "UNKNOWN",              // 0
        "DEVICE",               // 1
        "CONFIG",               // 2
        "STRING",               // 3
        "INTERFACE",            // 4
        "ENDPOINT",             // 5
        "DEVICE QUALIFIER",     // 6
        "OTHER SPEED",          // 7
        "INTERFACE POWER",      // 8
};

char *usbd_device_status[] = {
	"USBD_OPENING",
	"USBD_OK",
	"USBD_SUSPENDED",
	"USBD_CLOSING",	
};
#define USBD_DEVICE_REQUESTS(x) (((unsigned int)x <= USB_REQ_SYNCH_FRAME) ? usbd_device_requests[x] : "UNKNOWN")
#define USBD_DEVICE_DESCRIPTORS(x) (((unsigned int)x <= USB_DESCRIPTOR_TYPE_INTERFACE_POWER) ? \
                usbd_device_descriptors[x] : "UNKNOWN")
#define USBD_DEVICE_EVENTS(x) (((unsigned int)x <= DEVICE_FUNCTION_PRIVATE) ? \
                usbd_device_events[x] : "UNKNOWN")
#define USBD_DEVICE_STATE(x) (((unsigned int)x <= STATE_UNKNOWN) ? usbd_device_states[x] : "UNKNOWN")
#define USBD_DEVICE_STATUS(x) (((unsigned int)x <= USBD_CLOSING) ? usbd_device_status[x] : "UNKNOWN")
#endif

/* EP0 Configuration Set ********************************************************************* */


/**
 * ep0_event_irq - respond to USB event
 * @device:
 * @event:
 *
 * Called by lower layers to indicate some event has taken place. Typically
 * this is reset, suspend, resume and close.
 */
static void ep0_event_irq (struct usb_device_instance *device,
		       usb_device_event_t event, int dummy /* to make fn signature correct */ )
{
        // do nothing
}

/**
 * ep0_get_status - fill in URB data with appropriate status
 * @device:
 * @urb:
 * @index:
 * @requesttype:
 *
 */
static int ep0_get_status (struct usb_device_instance *device, struct urb *urb, int index, int requesttype)
{
	char *cp;

	urb->actual_length = 2;
	cp = urb->buffer;
        cp[0] = cp[1] = 0;

	switch (requesttype) {
	case USB_REQ_RECIPIENT_DEVICE:
		cp[0] = USB_STATUS_SELFPOWERED;
		break;
	case USB_REQ_RECIPIENT_INTERFACE:
		break;
	case USB_REQ_RECIPIENT_ENDPOINT:
		cp[0] = usbd_endpoint_halted (device, index);
		break;
	case USB_REQ_RECIPIENT_OTHER:
		urb->actual_length = 0;
        default:
                break;
	}
	//dbg_ep0(2, "%02x %02x", cp[0], cp[1]);
	return 0;
}

/**
 * ep0_get_one
 * @device:
 * @urb:
 * @result:
 *
 * Set a single byte value in the urb send buffer. Return non-zero to signal
 * a request error.
 */
static int ep0_get_one (struct usb_device_instance *device, struct urb *urb, __u8 result)
{
	urb->actual_length = 1;	// XXX 2?
	((char *) urb->buffer)[0] = result;
	return 0;
}

/**
 * copy_config
 * @urb: pointer to urb
 * @data: pointer to configuration data
 * @length: length of data
 *
 * Copy configuration data to urb transfer buffer if there is room for it.
 */
static void copy_config (struct urb *urb, void *data, int max_buf)
{
	int available;
	int length;

	//dbg_ep0(3, "-> actual: %d buf: %d max_buf: %d max_length: %d data: %p", 
	//        urb->actual_length, urb->buffer_length, max_buf, max_length, data);

	if (!data) {
		dbg_ep0 (1, "data is NULL");
		return;
	}
	if (!(length = *(unsigned char *) data)) {
		dbg_ep0 (1, "length is zero");
		return;
	}

	//dbg_ep0(0, "   actual: %d buf: %d max_buf: %d length: %d", 
	//        urb->actual_length, urb->buffer_length, max_buf, length);

	if ((available = max_buf - urb->actual_length) <= 0) {
		return;
	}
	//dbg_ep0(1, "actual: %d buf: %d max_buf: %d length: %d available: %d", 
	//        urb->actual_length, urb->buffer_length, max_buf, length, available);

	if (length > available) {
		length = available;
	}
	//dbg_ep0(1, "actual: %d buf: %d max_buf: %d length: %d available: %d", 
	//        urb->actual_length, urb->buffer_length, max_buf, length, available);

	memcpy (urb->buffer + urb->actual_length, data, length);
	urb->actual_length += length;

	//dbg_ep0(3, "<- actual: %d buf: %d max_buf: %d max_length: %d available: %d", 
	//        urb->actual_length, urb->buffer_length, max_buf, max_length, available);
}

/**
 * ep0_get_descriptor
 * @device:
 * @urb:
 * @max:
 * @descriptor_type:
 * @index:
 *
 * Called by ep0_rx_process for a get descriptor device command. Determine what
 * descriptor is being requested, copy to send buffer. Return zero if ok to send,
 * return non-zero to signal a request error.
 */
static int ep0_get_descriptor (struct usb_device_instance *device, struct urb *urb, int max, int descriptor_type, int index)
{
	//int port = 0;		// XXX compound device
	char *cp;
        struct usb_function_driver *function_driver = device->active_function_instance->function_driver;

	//dbg_ep0(0, "max: %x type: %x index: %x", max, descriptor_type, index);

	if (!urb || !urb->buffer || !urb->buffer_length || (urb->buffer_length < 255)) {
		//dbg_ep0 (2, "invalid urb %p", urb);
		return -EINVAL;
	}

	// setup tx urb
	urb->actual_length = 0;
	cp = urb->buffer;

#ifdef VERBOSE
        //dbg_ep0(2, "%s", USBD_DEVICE_DESCRIPTORS(descriptor_type));
#else
        //dbg_ep0(2, "%d", descriptor_type);
#endif

	switch (descriptor_type) {
	case USB_DESCRIPTOR_TYPE_DEVICE:
		{
			struct usb_device_descriptor *device_descriptor = function_driver->device_descriptor;

			// copy descriptor for this device
			copy_config (urb, device_descriptor, max);

			// correct the correct control endpoint 0 max packet size into the descriptor
			device_descriptor = (struct usb_device_descriptor *) urb->buffer;
			device_descriptor->bMaxPacketSize0 = urb->device->bus->driver->maxpacketsize;

		}
		//dbg_ep0(0, "copied device configuration, actual_length: %x", urb->actual_length);
		break;

                // c.f. 9.6.2 Device Qualifier
                // until and unless we have a high speed device we MUST return request error
	case USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
		{
			//struct usb_device_qualifier_descriptor *device_qualifier_descriptor;

			//if (!(device_qualifier_descriptor = usbd_device_qualifier_descriptor (device, port))) {
			//	return -EINVAL;
			//}
			// copy descriptor for this device
			//copy_config (urb, device_qualifier_descriptor, max);

			// correct the correct control endpoint 0 max packet size into the descriptor
			//device_qualifier_descriptor = (struct usb_device_qualifier_descriptor *) urb->buffer;
			//device_qualifier_descriptor->bMaxPacketSize0 = urb->device->bus->driver->maxpacketsize;

		}
		//dbg_ep0(0, "copied device configuration, actual_length: %x", urb->actual_length);
		return -EINVAL;

	case USB_DESCRIPTOR_TYPE_CONFIGURATION:
		{
			int interface;

                        struct usb_configuration_instance *configuration_instance = 
                                &function_driver->configuration_instance_array[index];

			struct usb_configuration_descriptor *configuration_descriptor = 
                                configuration_instance->configuration_descriptor;

                        if (!configuration_descriptor){
                                dbg_ep0(0, "configuration_descriptor NULL");
                                return -EINVAL;
                        }


			if (index > function_driver->device_descriptor->bNumConfigurations) {
				dbg_ep0 (0, "index too large: %d > %d", 
                                                index, function_driver->device_descriptor->bNumConfigurations);
				return -EINVAL;
			}

                        //dbg_ep0(3, "%p %d", configuration_descriptor, configuration_descriptor->bConfigurationValue);
			copy_config (urb, configuration_descriptor, max);


			// iterate across interfaces for specified configuration
			for (interface = 0; interface < configuration_descriptor->bNumInterfaces; interface++) {

				int alternate;
				struct usb_interface_instance *interface_instance =
                                        configuration_instance->interface_instance_array + interface;

				//dbg_ep0(0, "[%d] interface: %d", interface, configuration_descriptor->bNumInterfaces);


				// iterate across interface alternates
				for (alternate = 0; alternate < interface_instance->alternates; alternate++) {

					int class;
					int endpoint;

					struct usb_alternate_instance *alternate_instance =
                                                interface_instance->alternates_instance_array + alternate;

                                        //struct usb_interface_descriptor *usb_interface_descriptor;

					// copy descriptor for this interface
					copy_config (urb, alternate_instance->interface_descriptor, max);

					//dbg_ep0(0, "[%d:%d] classes: %d endpoints: %d", interface, alternate, 
                                        //                alternate_instance->classes, alternate_instance->endpoints);

					// iterate across classes for this alternate interface
					for (class = 0; class < alternate_instance->classes; class++) {
						copy_config (urb, *(alternate_instance->class_list + class), max);
					}

					// iterate across endpoints for this alternate interface
					//interface_descriptor = alternate_instance->interface_descriptor;

					for (endpoint = 0; endpoint < alternate_instance->endpoints; endpoint++) {
						copy_config (urb, *(alternate_instance->endpoint_list + endpoint), max);
					}
				}
			}
			//dbg_ep0(0, "lengths: %d %d", le16_to_cpu(configuration_descriptor->wTotalLength), urb->actual_length);
		}
		break;

	case USB_DESCRIPTOR_TYPE_STRING:
		{
			struct usb_string_descriptor *string_descriptor;
			if (!(string_descriptor = usbd_get_string (index))) {
				return -EINVAL;
			}
			//dbg_ep0(3, "string_descriptor: %p", string_descriptor);
			copy_config (urb, string_descriptor, max);
		}
		break;

	default:
		return -EINVAL;
	}


	//dbg_ep0(1, "urb: buffer: %p buffer_length: %2d actual_length: %2d packet size: %2d", 
	//        urb->buffer, urb->buffer_length, urb->actual_length, device->bus->endpoint_array[0].tx_packetSize);
	return 0;

}

/**
 * ep0_recv_setup - called to indicate URB has been received
 * @urb: pointer to struct urb
 *
 * Check if this is a setup packet, process the device request, put results
 * back into the urb and return zero or non-zero to indicate success (DATA)
 * or failure (STALL).
 *
 */
static int ep0_recv_setup (struct urb *urb)
{
	//struct usb_device_request *request = urb->buffer;
	//struct usb_device_instance *device = urb->device;

	struct usb_device_request *request;
	struct usb_device_instance *device;
	int address;

	request = &urb->device_request;
	device = urb->device;

	//dbg_ep0(3, "urb: %p device: %p", urb, urb->device);

#if 0
#ifdef VERBOSE
	dbg_ep0(2, "bmRequestType:%02x bRequest:%02x wValue:%04x wIndex:%04x wLength:%04x %s",
	        request->bmRequestType, request->bRequest, 
	        le16_to_cpu(request->wValue), le16_to_cpu(request->wIndex), le16_to_cpu(request->wLength),
                USBD_DEVICE_REQUESTS(request->bRequest));
#else
	dbg_ep0(2, "bmRequestType:%02x bRequest:%02x wValue:%04x wIndex:%04x wLength:%04x %d",
	        request->bmRequestType, request->bRequest, 
	        le16_to_cpu(request->wValue), le16_to_cpu(request->wIndex), le16_to_cpu(request->wLength),
                request->bRequest);
#endif
#endif

	// handle USB Standard Request (c.f. USB Spec table 9-2)
	if ((request->bmRequestType & USB_REQ_TYPE_MASK) != 0) {
		//dbg_ep0 (1, "non standard request: %x", request->bmRequestType & USB_REQ_TYPE_MASK);
		return 0;	// XXX
	}

	switch (device->device_state) {
	case STATE_CREATED:
	case STATE_ATTACHED:
	case STATE_POWERED:
#ifdef VERBOSE
		dbg_ep0 (1, "request %s not allowed in this state: %s", 
                                USBD_DEVICE_REQUESTS(request->bRequest), usbd_device_states[device->device_state]);
#else
		dbg_ep0 (1, "request %s not allowed in this state: %d", request->bRequest, device->device_state);
#endif
		return -EINVAL;

	case STATE_INIT:
	case STATE_DEFAULT:
		switch (request->bRequest) {
		case USB_REQ_GET_STATUS:
		case USB_REQ_GET_INTERFACE:
		case USB_REQ_SYNCH_FRAME:	// XXX should never see this (?)
		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
		case USB_REQ_SET_DESCRIPTOR:
		// case USB_REQ_SET_CONFIGURATION:
		case USB_REQ_SET_INTERFACE:
#ifdef VERBOSE
			dbg_ep0 (1, "request %s not allowed in DEFAULT state: %s",
				 USBD_DEVICE_REQUESTS(request->bRequest), usbd_device_states[device->device_state]);
#else
			dbg_ep0 (1, "request %d not allowed in DEFAULT state: %d", request->bRequest, device->device_state);
#endif
			return -EINVAL;

		case USB_REQ_SET_CONFIGURATION:
		case USB_REQ_SET_ADDRESS:
		case USB_REQ_GET_DESCRIPTOR:
		case USB_REQ_GET_CONFIGURATION:
			break;
		}
	case STATE_ADDRESSED:
	case STATE_CONFIGURED:
		break;
	case STATE_UNKNOWN:
#ifdef VERBOSE
		dbg_ep0 (1, "request %s not allowed in UNKNOWN state: %s", 
                                USBD_DEVICE_REQUESTS(request->bRequest), usbd_device_states[device->device_state]);
#else
		dbg_ep0 (1, "request %d not allowed in UNKNOWN state: %d", 
                                request->bRequest, device->device_state);
#endif
		return -EINVAL;
	}

	// handle all requests that return data (direction bit set on bm RequestType)
	if ((request->bmRequestType & USB_REQ_DIRECTION_MASK)) {

		//dbg_ep0(3, "Device-to-Host");

		switch (request->bRequest) {

		case USB_REQ_GET_STATUS:
			return ep0_get_status (device, urb, le16_to_cpu(request->wIndex), 
                                        request->bmRequestType & USB_REQ_RECIPIENT_MASK);

		case USB_REQ_GET_DESCRIPTOR:
			return ep0_get_descriptor (device, urb,
                                        le16_to_cpu (request->wLength), 
                                        le16_to_cpu (request->wValue >> 8), 
                                        le16_to_cpu (request->wValue) & 0xff);

		case USB_REQ_GET_CONFIGURATION:
			return ep0_get_one (device, urb, device->configuration);

		case USB_REQ_GET_INTERFACE:
			return ep0_get_one (device, urb, device->alternate);

		case USB_REQ_SYNCH_FRAME:	// XXX should never see this (?)
			return -EINVAL;

		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
		case USB_REQ_SET_ADDRESS:
		case USB_REQ_SET_DESCRIPTOR:
		case USB_REQ_SET_CONFIGURATION:
		case USB_REQ_SET_INTERFACE:
			return -EINVAL;
		}
	}
	// handle the requests that do not return data
	else {

		switch (request->bRequest) {

		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
                        //dbg_ep0(0, "Host-to-Device");
			switch (request->bmRequestType & USB_REQ_RECIPIENT_MASK) {
			case USB_REQ_RECIPIENT_DEVICE:
                                // XXX DEVICE_REMOTE_WAKEUP or TEST_MODE would be added here
                                // XXX fall through for now as we do not support either
			case USB_REQ_RECIPIENT_INTERFACE:
			case USB_REQ_RECIPIENT_OTHER:
                                //dbg_ep0 (0, "request %s not", USBD_DEVICE_REQUESTS(request->bRequest));
                        default:
				return -EINVAL;

                        case USB_REQ_RECIPIENT_ENDPOINT:
                                //dbg_ep0(0, "ENDPOINT: %x", le16_to_cpu(request->wValue));
                                if (le16_to_cpu(request->wValue) == USB_ENDPOINT_HALT) {
                                        return usbd_device_feature (device, le16_to_cpu (request->wIndex) & 0x7f,
                                                        request->bRequest == USB_REQ_SET_FEATURE);
                                }
                                else {
#ifdef VERBOSE
                                        dbg_ep0 (1, "request %s bad wValue: %04x", 
                                                        USBD_DEVICE_REQUESTS(request->bRequest), le16_to_cpu(request->wValue));
#else
                                        dbg_ep0 (1, "request %d bad wValue: %04x", 
                                                        request->bRequest, le16_to_cpu(request->wValue));
#endif
                                        return -EINVAL ;
                                }
                        }

                case USB_REQ_SET_ADDRESS:
                        // check if this is a re-address, reset first if it is (this shouldn't be possible)
                        if (device->device_state != STATE_DEFAULT) {
				return -EINVAL;
			}
			address = le16_to_cpu (request->wValue);
			device->address = address;
			usbd_device_event (device, DEVICE_ADDRESS_ASSIGNED, 0);
			return 0;

		case USB_REQ_SET_DESCRIPTOR:	// XXX should we support this?
			//dbg_ep0 (0, "set descriptor: NOT SUPPORTED");
			return -EINVAL;

		case USB_REQ_SET_CONFIGURATION:
			// c.f. 9.4.7 - the top half of wValue is reserved
			//
                        // c.f. 9.4.7 - zero is the default or addressed state, in our case this
                        // is the same is configuration zero, but will be fixed in usbd.c when used.
			device->configuration = le16_to_cpu (request->wValue) & 0x7f;
                        device->configuration = device->configuration ? device->configuration - 1 : 0;

			// reset interface and alternate settings
			device->interface = device->alternate = 0;

			//dbg_ep0(2, "set configuration: %d", device->configuration);
			usbd_device_event (device, DEVICE_CONFIGURED, 0);
			//	__REG(0x40600008)=0xff;//a17400 send ACK
			udc_send_ack_to_set_conf_req();
			return 0;

		case USB_REQ_SET_INTERFACE:
			device->interface = le16_to_cpu (request->wIndex);
			device->alternate = le16_to_cpu (request->wValue);
			//dbg_ep0(2, "set interface: %d alternate: %d", device->interface, device->alternate);
			usbd_device_event (device, DEVICE_SET_INTERFACE, 0);
			//__REG(0x40600008)=0xff;//a17400 send ACK
			udc_send_ack_to_set_conf_req();
			return 0;

		case USB_REQ_GET_STATUS:
		case USB_REQ_GET_DESCRIPTOR:
		case USB_REQ_GET_CONFIGURATION:
		case USB_REQ_GET_INTERFACE:
		case USB_REQ_SYNCH_FRAME:	// XXX should never see this (?)
			return -EINVAL;
		}
	}
	return -EINVAL;
}

/**
 * ep0_urb_sent - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 */
static int ep0_urb_sent (struct urb *urb, int rc)
{
	//dbg_ep0(2, "%s rc: %d", urb->device->name, rc);
	//usbd_dealloc_urb(urb);
	return 0;
}

static struct usb_function_operations ep0_ops = {
	event_irq:	ep0_event_irq,
	recv_setup:	ep0_recv_setup,
	urb_sent:	ep0_urb_sent,
};

struct usb_function_driver ep0_driver = {
	name:		"EP0",
	ops:		&ep0_ops,
};

