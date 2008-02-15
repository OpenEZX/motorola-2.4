/*
 * linux/drivers/usbd/bi/usbd-bi.c - USB Bus Interface Driver
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@belcara.com>, 
 *      Tom Rushworth <tbr@belcara.com>, 
 *      Bruce Balden <balden@belcara.com>
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>

#include <linux/proc_fs.h>

#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>

#include <linux/netdevice.h>
#include <linux/proc_fs.h>

//#include <asm/dma.h>
//#include <asm/mach/dma.h>
//#include <asm/irq.h>
//#include <asm/system.h>
//#include <asm/hardware.h>
//#include <asm/types.h>

#include <asm/uaccess.h>

#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
#include <linux/pm.h>
#endif


#include "../usbd.h"
#include "../usbd-debug.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

//#include "trace.h"
int trace_init (void);
void trace_exit (void);



/* Module Parameters ************************************************************************* */

static char *dbg = NULL;
MODULE_PARM (dbg, "s");

static char *iso = NULL;
MODULE_PARM (iso, "s");

// override serial number
static char *serial_number_str;
MODULE_PARM (serial_number_str, "s");
MODULE_PARM_DESC (serial_number_str, "Serial Number");


char * usbd_bi_iso(void)
{
	return(iso);
}

/* Debug switches (module parameter "dbg=...") *********************************************** */

extern int dbgflg_usbdbi_init;
int dbgflg_usbdbi_intr;
int dbgflg_usbdbi_tick;
int dbgflg_usbdbi_usbe;
int dbgflg_usbdbi_rx;
int dbgflg_usbdbi_tx;
int dbgflg_usbdbi_dma_flg;
int dbgflg_usbdbi_setup;
int dbgflg_usbdbi_ep0;
int dbgflg_usbdbi_udc;
int dbgflg_usbdbi_stall;
int dbgflg_usbdbi_pm;
int dbgflg_usbdbi_pur;

static debug_option dbg_table[] = {
	{&dbgflg_usbdbi_init, NULL, "init", "initialization and termination"},
	{&dbgflg_usbdbi_intr, NULL, "intr", "interrupt handling"},
	{&dbgflg_usbdbi_tick, NULL, "tick", "interrupt status monitoring on clock tick"},
	{&dbgflg_usbdbi_usbe, NULL, "usbe", "USB events"},
	{&dbgflg_usbdbi_rx, NULL, "rx", "USB RX (host->device) handling"},
	{&dbgflg_usbdbi_tx, NULL, "tx", "USB TX (device->host) handling"},
	{&dbgflg_usbdbi_dma_flg, NULL, "dma", "DMA handling"},
	{&dbgflg_usbdbi_setup, NULL, "setup", "Setup packet handling"},
	{&dbgflg_usbdbi_ep0, NULL, "ep0", "End Point 0 packet handling"},
	{&dbgflg_usbdbi_udc, NULL, "udc", "USB Device"},
	{&dbgflg_usbdbi_stall, NULL, "stall", "Testing"},
	{&dbgflg_usbdbi_pm, NULL, "pm", "Power Management"},
	{&dbgflg_usbdbi_pur, NULL, "pur", "USB cable Pullup Resistor"},
	{NULL, NULL, NULL, NULL}
};

/* globals */

int have_cable_irq;

#ifdef DOTICKER
/* ticker */
int ticker_terminating;
int ticker_timer_set;

/**
 * kickoff_thread - start management thread
 */
void ticker_kickoff (void)_;

/**
 * killoff_thread - stop management thread
 */
void ticker_killoff (void);
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
struct pm_dev *pm_dev;		// power management
#endif


/* Bus Interface Callback Functions ********************************************************** */

/**
 * bi_find_endpoint - find endpoint
 * @device: device
 * @endpoint_address: endpoint address
 */
struct usb_endpoint_instance *bi_find_endpoint(struct usb_device_instance *device, int endpoint_address)
{
	if (device && device->bus && device->bus->endpoint_array) {
                int i;
		for (i = 0; i < udc_max_endpoints (); i++) {
			struct usb_endpoint_instance *endpoint;
			if ((endpoint = device->bus->endpoint_array + i)) {
				if ((endpoint->endpoint_address & 0x7f) == (endpoint_address & 0x7f)) {
                                        return endpoint;
                                }
			}
		}
	}
        return NULL;
}

/**
 * bi_endpoint_halted - check if endpoint halted
 * @device: device
 * @endpoint: endpoint to check
 *
 * Used by the USB Device Core to check endpoint halt status.
 */
int bi_endpoint_halted (struct usb_device_instance *device, int endpoint_address)
{
        struct usb_endpoint_instance *endpoint;
        if ((endpoint = bi_find_endpoint(device, endpoint_address))) {
                //dbg_ep0 (1, "endpoint: %d status: %d", endpoint_address, endpoint->status);
                return endpoint->status;
        }
        //dbg_ep0 (0, "endpoint: %02x NOT FOUND", endpoint_address);
	return 0;
}


/**
 * bi_device_feature - handle set/clear feature requests
 * @device: device
 * @endpoint: endpoint to check
 * @flag: set or clear
 *
 * Used by the USB Device Core to check endpoint halt status.
 */
int bi_device_feature (struct usb_device_instance *device, int endpoint_address, int flag)
{
        struct usb_endpoint_instance *endpoint;
        //dbg_ep0 (0, "endpoint: %d flag: %d", endpoint_address, flag);
        if ((endpoint = bi_find_endpoint(device, endpoint_address))) {

                //dbg_ep0 (1, "endpoint: %d status: %d", endpoint_address, endpoint->status);
                if (flag && !endpoint->status) {
                        //dbg_ep0 (1, "stalling endpoint");
                        // XXX logical to physical?
                        udc_stall_ep (endpoint_address);
                        endpoint->status = 1;
                }
                else if (!flag && endpoint->status){
                        //dbg_ep0 (1, "reseting endpoint %d", endpoint_address);
                        udc_reset_ep (endpoint_address);
                        endpoint->status = 0;
                }
                return 0;
        }
        //dbg_ep0 (0, "endpoint: %d NOT FOUND", endpoint_address);
        return -EINVAL;
}

/* 
 * bi_disable_endpoints - disable udc and all endpoints
 */
static void bi_disable_endpoints (struct usb_device_instance *device)
{
	int i;
	//dbg_init (1, "-");
	if (device && device->bus && device->bus->endpoint_array) {
		for (i = 0; i < udc_max_endpoints (); i++) {
			struct usb_endpoint_instance *endpoint;
			if ((endpoint = (device->bus->endpoint_array + i))) {
                                //dbg_init (1, "%d endpoint: %p endpoint_address: %x", i, endpoint, endpoint->endpoint_address);
				usbd_flush_ep (endpoint);
			}
		}
	}
}

/* bi_config - commission bus interface driver
 */
static int bi_config (struct usb_device_instance *device)
{
	int interface;
	int endpoint;

        struct usb_function_driver *function_driver;
        struct usb_configuration_instance *configuration_instance;
        struct usb_configuration_descriptor *configuration_descriptor;

	// MMM Can't config without a function
	if (!device->active_function_instance) {
		return 0;
	}
        function_driver = device->active_function_instance->function_driver;
        configuration_instance = &function_driver->configuration_instance_array[device->configuration];
        configuration_descriptor = configuration_instance->configuration_descriptor;
        //printk(KERN_INFO"%s: configuration: %d\n", __FUNCTION__, device->configuration );
        //dbg_init(0, "---> device->status: %d", device->status);

	bi_disable_endpoints (device);

        // iterate across all interfaces for this configuration and verify they are valid 
        for (interface = 0 ; interface < configuration_descriptor->bNumInterfaces; interface++) {

                int alternate;
                struct usb_interface_instance *interface_instance =
                        configuration_instance->interface_instance_array + interface;

                //printk(KERN_INFO"%s: interface: %d\n", __FUNCTION__, interface );

                for (alternate = 0 ; alternate < interface_instance->alternates; alternate++) {

                        struct usb_alternate_instance *alternate_instance =
                                interface_instance->alternates_instance_array + alternate;

                        int endpoint;

                        //printk(KERN_INFO"%s: alternate: %d\n", __FUNCTION__, alternate );

                        // iterate across all endpoints for this interface and verify they are valid 
                        for (endpoint = 0; endpoint < alternate_instance->endpoints; endpoint++) {

                                struct usb_endpoint_descriptor *endpoint_descriptor =
                                        (struct usb_endpoint_descriptor *) *(alternate_instance->endpoint_list + endpoint);


                                int transfersize =
                                        *(alternate_instance->endpoint_transfersize_array + endpoint);

                                int physical_endpoint;
                                int logical_endpoint;
                                struct usb_endpoint_instance *endpoint_instance;


                                logical_endpoint = endpoint_descriptor->bEndpointAddress;

                                //printk(KERN_INFO"%s: endpoint: %d logical: %d\n", __FUNCTION__, endpoint, logical_endpoint );

                                if (!(physical_endpoint = udc_check_ep (logical_endpoint, endpoint_descriptor->wMaxPacketSize))) {

                                        //dbg_init (0, "endpoint[%d]: l: %02x p: %02x transferSize: %d packetSize: %02x INVALID",
                                        //                endpoint, logical_endpoint, physical_endpoint, transfersize,
                                        //                endpoint_descriptor->wMaxPacketSize);

                                        //dbg_init (0, "invalid endpoint: %d %d", logical_endpoint, physical_endpoint);
                                        return -EINVAL;
                                } 


                                // get pointer to our physical endpoint descriptor

                                endpoint_instance = device->bus->endpoint_array + physical_endpoint;

                                //dbg_init (0, "endpoint[%d]: l: %02x p: %02x transferSize: %d packetSize: %02x FOUND",
                                //                endpoint, logical_endpoint, physical_endpoint, transfersize,
                                //                endpoint_descriptor->wMaxPacketSize);

                                //dbg_init (0, "epd->bEndpointAddress=%02x", endpoint_descriptor->bEndpointAddress);
                                endpoint_instance->endpoint_address = endpoint_descriptor->bEndpointAddress;

                                if (endpoint_descriptor->wMaxPacketSize > 64) {
                                        dbg_init (0, "incompatible with endpoint size: %x", endpoint_descriptor->wMaxPacketSize);
                                        return -EINVAL;
                                }

                                if (endpoint_descriptor->bEndpointAddress & IN) {
                                        endpoint_instance->tx_attributes = endpoint_descriptor->bmAttributes;
                                        endpoint_instance->tx_transferSize = transfersize & 0xfff;
                                        endpoint_instance->tx_packetSize = endpoint_descriptor->wMaxPacketSize;
                                        endpoint_instance->last = 0;
                                        if (endpoint_instance->tx_urb) {
                                                //dbg_init (0, "CLEARING tx_urb: %p", endpoint_instance->tx_urb);
                                                usbd_dealloc_urb (endpoint_instance->tx_urb);
                                                endpoint_instance->tx_urb = NULL;
                                        }
                                }
                                else {
                                        endpoint_instance->rcv_attributes = endpoint_descriptor->bmAttributes;
                                        endpoint_instance->rcv_transferSize = transfersize & 0xfff;
                                        endpoint_instance->rcv_packetSize = endpoint_descriptor->wMaxPacketSize;
                                        if (endpoint_instance->rcv_urb) {
                                                //dbg_init (0, "CLEARING rcv_urb: %p", endpoint_instance->tx_urb);
                                                usbd_dealloc_urb (endpoint_instance->rcv_urb);
                                                endpoint_instance->rcv_urb = NULL;
                                        }
                                }
                        } /* for (endpoint = 0; ... */
                } /* for (alternate = 0; ... */
        } /* for (interface = 0;  ... */

        // iterate across all endpoints and enable them

        //dbg_init(0, "---> device->status: %d", device->status);

        //if (device->status == USBD_OK) {
                //int endpoint;

                //dbg_init (0, "enabling endpoints max_endpoints: %d", device->bus->driver->max_endpoints);

                for (endpoint = 1; endpoint < device->bus->driver->max_endpoints; endpoint++) {

                        struct usb_endpoint_instance *endpoint_instance = device->bus->endpoint_array + endpoint;

                        //dbg_init (0, "endpoint[%d]: %02x addr: %02x transferSize: %d:%d packetSize: %d:%d SETUP",
                        //                endpoint, endpoint, endpoint_instance->endpoint_address,
                        //                endpoint_instance->rcv_transferSize, endpoint_instance->tx_transferSize,
                        //                endpoint_instance->rcv_packetSize, endpoint_instance->tx_packetSize);

                        //udc_setup_ep(device, endpoint, endpoint_instance->endpoint_address?  endpoint : NULL);
                        
                        udc_setup_ep (device, endpoint, endpoint_instance);

                } /* for (endpoint = 1; ... */
        //}

        return 0;
}


/**
 * bi_device_event - handle generic bus event
 * @device: device pointer
 * @event: interrupt event
 *
 * Called by usb core layer to inform bus of an event.
 */
int bi_device_event (struct usb_device_instance *device, usb_device_event_t event, int data)
{

        //printk(KERN_DEBUG "bi_device_event: event: %d\n", event);

        if (!device) {
                return 0;
        }

        //dbg_usbe (1,"%s", USBD_DEVICE_EVENTS(event));

        switch (event) {
        case DEVICE_UNKNOWN:
                break;
        case DEVICE_INIT:
                break;
        case DEVICE_CREATE:	// XXX should this stuff be in DEVICE_INIT?
                // enable upstream port

                //ep0_enable(device);

		// MMM con't do this if no active device
                // for net_create
                bi_config (device);

                // enable udc, enable interrupts, enable connect
                udc_enable (device);

                // XXX verify
                udc_suspended_interrupts (device);
                //udc_all_interrupts (device);

                //dbg_usbe (1, "CREATE done");
                break;

        case DEVICE_HUB_CONFIGURED:
		if (device->function_state != FN_STATE_SWITCHING) {
			udc_connect ();
		}
		break;

	case DEVICE_RESET:
		device->address = 0;
		udc_set_address (device->address);
		udc_reset_ep (0);
                
                // XXX verify
		udc_suspended_interrupts (device);
		//dbg_usbe (1, "DEVICE RESET done: %d", device->address);
		break;


	case DEVICE_ADDRESS_ASSIGNED:
		udc_set_address (device->address);
		device->status = USBD_OK;

                // XXX verify
		udc_all_interrupts (device);	// XXX
		break;

	case DEVICE_CONFIGURED:
		device->status = USBD_OK;
		bi_config (device);
		break;

	case DEVICE_DE_CONFIGURED:
                // XXX 
		udc_reset_ep (1);
		udc_reset_ep (2);
		udc_reset_ep (3);
		break;


	case DEVICE_SET_INTERFACE:
		bi_config (device);
		break;

	case DEVICE_SET_FEATURE:
		break;

	case DEVICE_CLEAR_FEATURE:
		break;

	case DEVICE_BUS_INACTIVE:
		// disable suspend interrupt
		udc_suspended_interrupts (device);

		// XXX check on linkup and sl11
		// if we are no longer connected then force a reset
		if (!udc_connected ()) {
                        //dbg_usbe (1, "BUS_INACTIVE - DEVICE RESET done: %d", device->address);
			usbd_device_event_irq (device, DEVICE_RESET, 0);
		}
		break;

	case DEVICE_BUS_ACTIVITY:
		// enable suspend interrupt
		udc_all_interrupts (device);
		break;

	case DEVICE_POWER_INTERRUPTION:
		break;

	case DEVICE_HUB_RESET:
		break;

	case DEVICE_DESTROY:
		if (device->function_state != FN_STATE_SWITCHING) {
			udc_disconnect ();
		}
		bi_disable_endpoints (device);
		udc_disable_interrupts (device);
		udc_disable ();
		break;

	case DEVICE_FUNCTION_PRIVATE:
		break;
	case DEVICE_CABLE_DISCONNECTED:
		break;
	}
	return 0;
}

/**
 * bi_send_urb - start transmit
 * @urb:
 */
#define CONFIG_MOTO_PST_FD 1
int bi_send_urb (struct urb *urb)
{
        unsigned long flags;

        //printk(KERN_INFO"%s: urb: %p\n", __FUNCTION__, urb);
        //printk(KERN_INFO"%s: tx_urb: %p\n", __FUNCTION__, urb->endpoint->tx_urb);

        local_irq_save (flags);

	if (urb && urb->endpoint && !urb->endpoint->tx_urb) {
#ifdef CONFIG_MOTO_PST_FD 
		if ( urb->endpoint->endpoint_address == 0 ) {
			urb->endpoint->tx_urb = urb;
		}
#endif
		usbd_tx_complete_irq (urb->endpoint, 0);
		udc_start_in_irq (urb);
	}

        local_irq_restore (flags);

	urb->device->usbd_rxtx_timestamp = jiffies;
	return 0;
}

/**
 * bi_cancel_urb - cancel sending an urb
 * @urb: data urb to cancel
 *
 * Used by the USB Device Core to cancel an urb.
 */
int bi_cancel_urb (struct urb *urb)
{
        unsigned long flags;
	//printk(KERN_INFO"%s: urb: %p\n", __FUNCTION__, urb);

        local_irq_save (flags);

	if (urb && urb->endpoint) {
		
                //printk(KERN_INFO"%s: tx_urb: %p\n", __FUNCTION__, urb->endpoint->tx_urb);

                // is this the active urb?
                if (urb->endpoint->tx_urb == urb) {
                        //printk(KERN_INFO"%s: TX_URB\n", __FUNCTION__, urb);
                        urb->endpoint->tx_urb = NULL;
                        udc_cancel_in_irq(urb);
                }
                // else detach it 
                else {
                        //printk(KERN_INFO"%s: DETACHING\n", __FUNCTION__, urb);
                        urb_detach_irq(urb);
                }
                // finish it
                usbd_urb_sent_irq (urb, SEND_FINISHED_CANCELLED);
	}
        local_irq_restore (flags);
	return 0;
}


#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
static int bi_pm_event (struct pm_dev *pm_dev, pm_request_t request, void *unused);
#endif



struct usb_bus_operations bi_ops = {
	send_urb: bi_send_urb,
	cancel_urb: bi_cancel_urb,
	endpoint_halted: bi_endpoint_halted,
	device_feature: bi_device_feature,
	device_event: bi_device_event,
};

struct usb_bus_driver bi_driver = {
	name: "",
	max_endpoints: 0,
	ops: &bi_ops,
	this_module: THIS_MODULE,
};


/* Bus Interface Received Data *************************************************************** */

struct usb_device_instance *device_array[MAX_DEVICES];


/* Bus Interface Support Functions *********************************************************** */

#if DOCABLEEVENT
/**
 * udc_cable_event - called from cradle interrupt handler
 */
void udc_cable_event (void)
{
	struct usb_bus_instance *bus;
	struct usb_device_instance *device;
	struct bi_data *data;

	dbgENTER (dbgflg_usbdbi_init, 1);

	// sanity check
	if (!(device = device_array[0]) || !(bus = device->bus) || !(data = bus->privdata)) {
		return;
	}

	{
		unsigned long flags;
		local_irq_save (flags);
		if (udc_connected ()) {
			//dbg_init (1, "state: %d connected: %d", device->device_state, 1);;
			if (device->device_state == STATE_ATTACHED) {
				//dbg_init (1, "LOADING");
				usbd_device_event_irq (device, DEVICE_HUB_CONFIGURED, 0);
				usbd_device_event_irq (device, DEVICE_RESET, 0);
			}
		} else {
			//dbg_init (1, "state: %d connected: %d", device->device_state, 0);;
			if (device->device_state != STATE_ATTACHED) {
				//dbg_init (1, "UNLOADING");
				usbd_device_event_irq (device, DEVICE_RESET, 0);
				usbd_device_event_irq (device, DEVICE_POWER_INTERRUPTION, 0);
				usbd_device_event_irq (device, DEVICE_HUB_RESET, 0);
			}
		}
		local_irq_restore (flags);
	}
	dbgLEAVE (dbgflg_usbdbi_init, 1);
}
#endif

/* Module Init - the device init and exit routines ******************************************* */

/**
 * bi_udc_init - initialize USB Device Controller
 * 
 * Get ready to use the USB Device Controller.
 *
 * Register an interrupt handler and IO region. Return non-zero for error.
 */
int bi_udc_init (void)
{
	//dbg_init (1, "Loading %s", udc_name ());

	bi_driver.name = udc_name ();
	bi_driver.max_endpoints = udc_max_endpoints ();
	bi_driver.maxpacketsize = udc_ep0_packetsize ();

	//dbg_init (1, "name: %s endpoints: %d ep0: %d", bi_driver.name, bi_driver.max_endpoints,
	//	  bi_driver.maxpacketsize);

        trace_init();

	// request device IRQ
	if (udc_request_udc_irq ()) {
		//dbg_init (0, "name: %s request udc irq failed", udc_name ());
                THROW(irq_err);
	}
	// request device IO
	if (udc_request_io ()) {
		dbg_init (0, "name: %s request udc io failed", udc_name ());
		//dbg_init (0, "name: %s request udc io failed", udc_name ());
		return -EINVAL;
	}
	// probe for device
	if (udc_init ()) {
		dbg_init (0, "name: %s probe failed", udc_name ());
		//dbg_init (1, "name: %s probe failed", udc_name ());
		return -EINVAL;
	}

	// optional cable IRQ 
	have_cable_irq = !udc_request_cable_irq ();
	//dbg_init (1, "name: %s request cable irq %d", udc_name (), have_cable_irq);

        CATCH(irq_err) {
                CATCH(io_err) {
                        CATCH(init_err) {
                                udc_release_io ();
                        }
                        udc_release_udc_irq ();
                }
                return -EINVAL;
        }

	return 0;
}


/**
 * bi_udc_exit - Stop using the USB Device Controller
 *
 * Stop using the USB Device Controller.
 *
 * Shutdown and free dma channels, de-register the interrupt handler.
 */
void bi_udc_exit (void)
{
	int i;

	//dbg_init (1, "Unloading %s", udc_name ());

	for (i = 0; i < udc_max_endpoints (); i++) {
		udc_disable_ep (i);
	}

	// free io and irq
	udc_disconnect ();
	udc_disable ();
	udc_release_io ();
	udc_release_udc_irq ();

	if (have_cable_irq) {
		udc_release_cable_irq ();
	}

        trace_exit();
}


/* Module Init - the module init and exit routines ************************************* */

#if 0
/* bi_init - commission bus interface driver
 */
static int bi_init (void)
{
	return 0;
}
#endif

/* bi_exit - decommission bus interface driver
 */
static void bi_exit (void)
{
}

#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
/* Module Init - Power management ************************************************************ */

/* The power management scheme is simple. Simply do the following:
 *
 *      Event           Call            Equivalent
 *      ------------------------------------------
 *      PM_SUSPEND      bi_exit();      rmmod
 *      PM_RESUME       bi_init();      insmod
 *
 */

static int pm_suspended;

/*
 * usbd_pm_callback
 * @dev:
 * @rqst:
 * @unused:
 *
 * Used to signal power management events.
 */
static int bi_pm_event (struct pm_dev *pm_dev, pm_request_t request, void *unused)
{
	struct usb_device_instance *device;

	//dbg_pm (0, "request: %d pm_dev: %p data: %p", request, pm_dev, pm_dev->data);

	if (!(device = pm_dev->data)) {
		//dbg_pm (0, "DATA NULL, NO DEVICE");
		return 0;
	}

	switch (request) {
#if defined(CONFIG_IRIS)
	case PM_STANDBY:
	case PM_BLANK:
#endif
	case PM_SUSPEND:
		dbg_pm (0, "PM_SUSPEND");
		if (!pm_suspended) {
			pm_suspended = 1;
			dbg_init (1, "MOD_INC_USE_COUNT %d", GET_USE_COUNT (THIS_MODULE));
			udc_disconnect ();	// disable USB pullup if we can
			udc_disable_interrupts (device);	// disable interupts
			udc_disable ();	// disable UDC
			dbg_pm (0, "PM_SUSPEND: finished");
		}
		break;
#if defined(CONFIG_IRIS)
	case PM_UNBLANK:
#endif
	case PM_RESUME:
		dbg_pm (0, "PM_RESUME");
		if (pm_suspended) {
			// probe for device
			if (udc_init ()) {
				//dbg_init (0, "udc_init failed");
				//return -EINVAL;
			}
			udc_enable (device);	// enable UDC
			udc_all_interrupts (device);	// enable interrupts
			udc_connect ();	// enable USB pullup if we can
			//udc_set_address(device->address);
			//udc_reset_ep(0);

			pm_suspended = 0;
			//dbg_init (1, "MOD_INC_USE_COUNT %d", GET_USE_COUNT (THIS_MODULE));
			dbg_pm (0, "PM_RESUME: finished");
		}
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_USBD_PROCFS
/* Proc Filesystem *************************************************************************** */

/* *
 * usbd_proc_read - implement proc file system read.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system read function.
 */
static ssize_t usbd_proc_read (struct file *file, char *buf, size_t count, loff_t * pos)
{
        struct usb_device_instance *device;
	unsigned long page;
	int len = 0;
	int index;

	MOD_INC_USE_COUNT;
	// get a page, max 4095 bytes of data...
	if (!(page = get_free_page (GFP_KERNEL))) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}

	len = 0;
	index = (*pos)++;

	switch (index) {
        case 0:
		len += sprintf ((char *) page + len, "USBD Status\n");
                break;

        case 1:
		len += sprintf ((char *) page + len, "Cable: %s\n", udc_connected ()? "Plugged" : "Unplugged");
                break;

        case 2:
                if ((device = device_array[0])) {
                        struct usb_function_instance * function_instance;
                        struct usb_bus_instance * bus;

                        //len += sprintf ((char *) page + len, "Device status: %s\n", USBD_DEVICE_STATUS(device->status));
                        //len += sprintf ((char *) page + len, "Device state: %s\n", USBD_DEVICE_STATE(device->device_state));

			// MMM Show future function only if present
                        if ((function_instance = device->future_function_instance)) {
                                len += sprintf ((char *) page + len, "Future function: %s\n", 
                                                function_instance->function_driver->name);
			}
			// MMM Show active function unconditionally
                        if ((function_instance = device->active_function_instance)) {
                                len += sprintf ((char *) page + len, "Active function: %s\n", 
                                                function_instance->function_driver->name);
                        } else {
                                len += sprintf ((char *) page + len, "Active function: NULL\n");
			}
                        if ((bus= device->bus)) {
                                len += sprintf ((char *) page + len, "Bus interface: %s\n", 
                                                bus->driver->name);
                                if (bus->serial_number_str && strlen(bus->serial_number_str)) {
                                        len += sprintf ((char *) page + len, "Serial number: %s\n", 
                                                        bus->serial_number_str);
                                }
                        }
                }
                break;
        case 3:
                if ((device = device_array[0])) {
                        struct usb_function_instance * fn;
			int f;
                	len += sprintf ((char *) page + len, "%d function%s available:\n", 
					device->functions,((device->functions==1)?"":"s"));
			for (f = 0; f < device->functions; f++) {
				fn = device->functions_instance_array + f;
                	        len += sprintf ((char *) page + len, "   %d [%s]\n", 
						f,fn->function_driver->name);
			}
		}
                break;

        default:
                break;
        }


	if (len > count) {
		len = -EINVAL;
	} else if (len > 0 && copy_to_user (buf, (char *) page, len)) {
		len = -EFAULT;
	}
	free_page (page);
	MOD_DEC_USE_COUNT;
	return len;
}

/* *
 * usbd_proc_write - implement proc file system write.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Proc file system write function, used to signal monitor actions complete.
 * (Hotplug script (or whatever) writes to the file to signal the completion
 * of the script.)  An ugly hack.
 */
#define MAX_FUNC_NAME_LEN  64
static ssize_t usbd_proc_write (struct file *file, const char *buf, size_t count, loff_t * pos)
{
	struct usb_device_instance *device = NULL;
	char command[MAX_FUNC_NAME_LEN+1];
	size_t n = count;
        size_t l;
	//char *cp = command;
	char c;
	int i = 0;

	MOD_INC_USE_COUNT;
	command[0] = 0;
	//printk(KERN_DEBUG "%s: count=%u\n",__FUNCTION__,count);
	if (n > 0) {
		l = MIN(n,MAX_FUNC_NAME_LEN);
		if (copy_from_user (command, buf, l)) {
			count = -EFAULT;
		} else {
			if (l > 0 && command[l-1] == '\n') {
				l -= 1;
			}
			command[l] = 0;
			n -= l;
			// flush remainder, if any
			while (n > 0) {
				// Not too efficient, but it shouldn't matter
				if (copy_from_user (&c, buf + (count - n), 1)) {
					count = -EFAULT;
					break;
				}
				n -= 1;
			}
		}
	}
	//printk(KERN_ERR "%s: searching for [%s]\n",__FUNCTION__,command);
	if (count > 0 && !strcmp (command, "plug")) {
		udc_connect ();
	} 
        else if (count > 0 && !strcmp (command, "unplug")) {
		udc_disconnect ();
                if ((device = device_array[0])) {
                        usbd_device_event (device, DEVICE_RESET, 0);
                }
	}
	else if (count > 0 && (device = device_array[0]) && device->functions > 0) {
		// We have some kind of function name, look for it.
               struct usb_function_instance *new_function;
		int fn_ndx;
		for (fn_ndx = 0;  fn_ndx < device->functions; fn_ndx++) {
			new_function = device->functions_instance_array + fn_ndx;
			//printk(KERN_ERR "%s: checking [%s]\n",__FUNCTION__,
			//       new_function->function_driver->name);
			if (0 == strcmp(command,new_function->function_driver->name)) {
				// Found it.
				//printk(KERN_ERR "%s: matched\n",__FUNCTION__);
				device->future_function_instance = new_function;
				break;
			}
		}
		if (fn_ndx >= device->functions) {
			// Unknown name 
			printk(KERN_ERR "%s: [%s] not available\n",__FUNCTION__, command);
			count = -EINVAL;
		}

	}
	MOD_DEC_USE_COUNT;
	return (count);
}

static struct file_operations usbd_proc_operations_functions = {
	read:usbd_proc_read,
	write:usbd_proc_write,
};

#endif
void switch_cfg11(struct usb_device_instance *device)
{
	struct usb_function_instance *new_function;
	int fn_ndx;
	
	device = device_array[0];
	for (fn_ndx = 0;  fn_ndx < device->functions; fn_ndx++) {
		new_function = device->functions_instance_array + fn_ndx;
		if (0 == strcmp("CDC cfg11",new_function->function_driver->name)) {
			device->future_function_instance = new_function;
			break;
		}
	}
}
/* Module Init - module loading init and exit routines *************************************** */

/* We must be able to use the real bi_init() and bi_exit() functions 
 * from here and the power management event function.
 *
 * We handle power management registration issues from here.
 *
 */

/* bi_modinit - commission bus interface driver
 */
int __init bi_modinit (void)
{
	extern const char *usbd_bi_module_info(void);
	struct usb_bus_instance *bus;
	struct usb_device_instance *device;
	struct bi_data *data;


	printk (KERN_INFO "%s: %s\n", __FUNCTION__, usbd_bi_module_info());
        
	//printk (KERN_INFO "%s (dbg=\"%s\")\n", usbd_bi_module_info(), dbg ? dbg : "");

	// process debug options
	if (0 != scan_debug_options ("usbd-bi", dbg_table, dbg)) {
		return -EINVAL;
	}
	// check if we can see the UDC
	if (bi_udc_init ()) {
		return -EINVAL;
	}
	// allocate a bi private data structure
	if ((data = ckmalloc (sizeof (struct bi_data), GFP_KERNEL)) == NULL) {
		bi_udc_exit ();
		return -EINVAL;
	}
	memset (data, 0, sizeof (struct bi_data));

	// register this bus interface driver and create the device driver instance
	if ((bus = usbd_register_bus (&bi_driver)) == NULL) {
		lkfree (data);
		bi_udc_exit ();
		return -EINVAL;
	}

	bus->privdata = data;

	// see if we can scrounge up something to set a sort of unique device address
	if (udc_serial_init (bus)) {
                if (serial_number_str && strlen(serial_number_str)) {
                        bus->serial_number_str = lstrdup(serial_number_str);
                }
        }

        dbg_init (1, "serial: %s %04x\n", bus->serial_number_str?bus->serial_number_str:"NOT SET", bus->serial_number);


	if ((device = usbd_register_device (NULL, bus, 8)) == NULL) {
		usbd_deregister_bus (bus);
		lkfree (data);
		bi_udc_exit ();
		return -EINVAL;
	}
#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
	// register with power management 
	pm_dev = pm_register (PM_USB_DEV, PM_SYS_UNKNOWN, bi_pm_event);
	pm_dev->data = device;
#endif

	bus->device = device;
	device_array[0] = device;
	// initialize the device
	{
		struct usb_endpoint_instance *endpoint = device->bus->endpoint_array + 0;

		// setup endpoint zero

		endpoint->endpoint_address = 0;

		endpoint->tx_attributes = 0;
		endpoint->tx_transferSize = 255;
		endpoint->tx_packetSize = udc_ep0_packetsize ();

		endpoint->rcv_attributes = 0;
		endpoint->rcv_transferSize = 0x8;
		endpoint->rcv_packetSize = udc_ep0_packetsize ();

		udc_setup_ep (device, 0, endpoint);
	}

	// hopefully device enumeration will finish this process
	udc_startup_events (device);

#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
	//dbg_pm (0, "pm_dev->callback#%p", pm_dev->callback);
	if (!udc_connected ()) {
		/* Fake a call from the PM system to suspend the UDC until it
		   is needed (cable connect, etc) */
		(void) bi_pm_event (pm_dev, PM_SUSPEND, NULL);
		/* There appears to be no constant for this, but inspection
		   of arch/arm/mach-l7200/apm.c:send_event() shows that the
		   suspended state is 3 (i.e. pm_send_all(PM_SUSPEND, (void *)3))
		   corresponding to ACPI_D3. */
		pm_dev->state = 3;
	}
#endif
#if DOTICKER
	if (dbgflg_usbdbi_tick > 0) {
		// start ticker
		ticker_kickoff ();
	}
#endif
#ifdef CONFIG_USBD_PROCFS
	{
		struct proc_dir_entry *p;

		// create proc filesystem entries
		if ((p = create_proc_entry ("usbd", 0666, 0)) == NULL) {
			return -ENOMEM;
		}
		p->proc_fops = &usbd_proc_operations_functions;
	}
#endif
	dbgLEAVE (dbgflg_usbdbi_init, 1);
	printk (KERN_INFO "%s: finish\n", __FUNCTION__);
	return 0;
}

/* bi_modexit - decommission bus interface driver
 */
void __exit bi_modexit (void)
{
	struct usb_bus_instance *bus;
	struct usb_device_instance *device;
	struct bi_data *data;

	//dbgENTER (dbgflg_usbdbi_init, 1);
        //dbg_init (0, "device_array[0]: %p", device_array[0]);

#ifdef CONFIG_USBD_PROCFS
	remove_proc_entry ("usbd", NULL);
#endif


        udc_disconnect ();
        udc_disable ();

	if ((device = device_array[0])) {

		// XXX moved to usbd_deregister_device()
		//device->status = USBD_CLOSING;

		// XXX XXX
#ifdef DOTICKER 
		if (dbgflg_usbdbi_tick > 0) {
			ticker_killoff ();
		}
#endif
		bus = device->bus;
		data = bus->privdata;

		// XXX
		usbd_device_event (device, DEVICE_RESET, 0);
		usbd_device_event (device, DEVICE_POWER_INTERRUPTION, 0);
		usbd_device_event (device, DEVICE_HUB_RESET, 0);

		//dbg_init (1, "DEVICE_DESTROY");
		usbd_device_event (device, DEVICE_DESTROY, 0);

		//dbg_init (1, "DISABLE ENDPOINTS");
		bi_disable_endpoints (device);

		//dbg_init(1,"UDC_DISABLE");
		//udc_disable();

		//dbg_init (1, "BI_UDC_EXIT");
		bi_udc_exit ();

		device_array[0] = NULL;
		//bus->privdata = NULL; // XXX moved to usbd-bus.c usbd_deregister_device()


#if defined(CONFIG_PM) && !defined(CONFIG_USBD_MONITOR) && !defined(CONFIG_USBD_MONITOR_MODULE)
		//dbg_init (1, "PM_UNREGISTER(pm_dev#%p)", pm_dev);
		if (pm_dev) {
			pm_unregister_all(bi_pm_event);
		}
#endif
		//dbg_init (1, "DEREGISTER DEVICE");
		usbd_deregister_device (device);
		bus->device = NULL;

		if (data) {
			lkfree (data);
		}

		if (bus->serial_number_str) {
			lkfree (bus->serial_number_str);
		}

		//dbg_init (1, "DEREGISTER BUS");
		usbd_deregister_bus (bus);

	} else {
		//dbg_init (0, "device is NULL");
	}
	//dbg_init (1, "BI_EXIT");
	bi_exit ();
        //udc_exit ();
	dbgLEAVE (dbgflg_usbdbi_init, 1);
}



unsigned int udc_interrupts;

#ifdef DOTICKER
/* ticker */

/* Clock Tick Debug support ****************************************************************** */


#define RETRYTIME 10

int ticker_terminating;
int ticker_timer_set;

unsigned int udc_interrupts_last;

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

	// Name this thread 
	sprintf (current->comm, "usbd-bi");

	// setup signal handler
	current->exit_signal = SIGCHLD;
	spin_lock (&current->sigmask_lock);
	flush_signals (current);
	spin_unlock (&current->sigmask_lock);

	// XXX Run at a high priority, ahead of sync and friends
	// current->nice = -20;
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

		char buf[100];
		char *cp;

		if (!ticker_timer_set) {
			mod_timer (&ticker, jiffies + HZ * RETRYTIME);
		}
		// wait for someone to tell us to do something
		down (&ticker_sem_work);

		if (udc_interrupts != udc_interrupts_last) {
			dbg_tick (3, "--------------");
		}
		// do some work
		memset (buf, 0, sizeof (buf));
		cp = buf;

		if (dbgflg_usbdbi_tick) {
			unsigned long flags;
			local_irq_save (flags);
			dbg_tick (2, "[%d]", udc_interrupts);
			udc_regs ();
			local_irq_restore (flags);
		}

#if 0
		// XXX
#if defined(CONFIG_SA1110_CALYPSO) && defined(CONFIG_PM) && defined(CONFIG_USBD_TRAFFIC_KEEPAWAKE)
		/* Check for rx/tx activity, and reset the sleep timer if present */
		if (device->usbd_rxtx_timestamp != device->usbd_last_rxtx_timestamp) {
			extern void resetSleepTimer (void);
			dbg_tick (7, "resetting sleep timer");
			resetSleepTimer ();
			device->usbd_last_rxtx_timestamp = device->usbd_rxtx_timestamp;
		}
#endif
#endif
#if 0
		/* Check for TX endpoint stall.  If the endpoint has
		   stalled, we have probably run into the DMA/TCP window
		   problem, and the only thing we can do is disconnect
		   from the bus, then reconnect (and re-enumerate...). */
		if (reconnect > 0 && 0 >= --reconnect) {
			dbg_init (0, "TX stall disconnect finished");
			udc_connect ();
		} else if (0 != USBD_STALL_TIMEOUT_SECONDS) {
			// Stall watchdog unleashed
			unsigned long now, tx_waiting;
			now = jiffies;
			tx_waiting = (now - tx_queue_head_timestamp (now)) / HZ;
			if (tx_waiting > USBD_STALL_TIMEOUT_SECONDS) {
				/* The URB at the head of the queue has waited too long */
				reconnect = USBD_STALL_DISCONNECT_DURATION;
				dbg_init (0, "TX stalled, disconnecting for %d seconds", reconnect);
				udc_disconnect ();
			}
		}
#endif
	}

	// remove timer
	del_timer (&ticker);

	// let the process stopping us know we are done and return
	up (&ticker_sem_start);
	return 0;
}

/**
 * kickoff_thread - start management thread
 */
void ticker_kickoff (void)
{
	ticker_terminating = 0;
	kernel_thread (&ticker_thread, NULL, 0);
	down (&ticker_sem_start);
}

/**
 * killoff_thread - stop management thread
 */
void ticker_killoff (void)
{
	if (!ticker_terminating) {
		ticker_terminating = 1;
		up (&ticker_sem_work);
		down (&ticker_sem_start);
	}
}
#endif          /* TICKER */

/* module */

//module_init (bi_modinit);
//module_exit (bi_modexit);
EXPORT_SYMBOL(bi_modinit);
EXPORT_SYMBOL(bi_modexit);
