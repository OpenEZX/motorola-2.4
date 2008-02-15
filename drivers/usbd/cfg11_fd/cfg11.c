/*
 * linux/drivers/usbd/cfg11_fd/cfg11.c
 *
 * This function driver is composite function which consists of 
 * acm interface, test command interface and MCU data logger interface. 
 *
 * Copyright (C) 2004-2005 Motorola
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                                               
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * 2004-Mar-30 - (Motorola) Created
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

#include <usbd-export.h>
#include <usbd-build.h>

MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("Motorola cfg11 Function");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,17)
MODULE_LICENSE("GPL");
#endif


#define MCEL 1
#define CONFIG_USBD_ACM_DATALOG 1
#define PST_FD_AVAILABLE
#define USBD_CFG11_VENDORID	0x22b8
#define USBD_CFG11_PRODUCTID	0x6004	
#define USBD_CFG11_BCDDEVICE	0x0001


#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>

#include "../usbd-chap9.h"
#include "../usbd-mem.h"
#include "../usbd.h"
#include "../usbd-func.h"

#include "../usbd-bus.h"		// UDC


USBD_MODULE_INFO("cfg11_fd 1.0-beta");


#define MAX_QUEUED_BYTES        256
#define MAX_QUEUED_URBS         10	// 64???

#define MAX_RECV_URBS           2      // Max for receiving data
#define RECV_RING_SIZE          (MAX_RECV_URBS+1)


// Endpoint indexes in cfg11_endpoint_requests[] and the endpoint map.
#define BULK_OUT        0x00
#define BULK_IN         0x01
#define INT_IN          0x02
#define DATALOG_BULK_IN	0x03
#define ENDPOINTS_CFG11	0x04


// Interface index for cfg11_fd
#define COMM_INTF       0x00
#define DATA_INTF       0x01
#define DATALOG_INTF    0x02
#define TESTCMD_INTF    0x03


/*from pst-fd.c*/
extern int pst_dev_create(struct usb_function_instance *function);
extern void pst_dev_destroy(void);
extern void ep0_process_vendor_request(struct usb_device_request *req);
extern int pst_urb_sent (struct urb *, int );
extern int pst_function_enable(struct usb_function_instance *);
extern int pst_function_disable(struct usb_function_instance *);


/* plan to put following into acm.h */
extern struct usb_alternate_description cdc_comm_alternate_descriptions[];
extern struct usb_alternate_description cdc_data_alternate_descriptions[];
extern void acm_event_irq (struct usb_function_instance *function, usb_device_event_t event, int data);
extern int acm_function_enable (struct usb_function_instance *function);
extern void acm_function_disable (struct usb_function_instance *function);
extern int acm_recv_setup_irq (struct usb_device_request *request);

/* Module Parameters */
static u32 vendor_id;
static u32 product_id;
static u32 max_queued_urbs = MAX_QUEUED_URBS;
static u32 max_queued_bytes = MAX_QUEUED_BYTES;

MODULE_PARM (vendor_id, "i");
MODULE_PARM (product_id, "i");
MODULE_PARM (max_queued_urbs, "i");
MODULE_PARM (max_queued_bytes, "i");

MODULE_PARM_DESC (vendor_id, "Device Vendor ID");
MODULE_PARM_DESC (product_id, "Device Product ID");
MODULE_PARM_DESC (max_queued_urbs, "Maximum TX Queued Urbs");
MODULE_PARM_DESC (max_queued_bytes, "Maximum TX Queued Bytes");

struct cfg11_private{
	struct usb_function_instance *function;
	int usb_driver_registered;
};
static struct cfg11_private cfg11_private;

/*
 * cfg11 Configuration
 *
 * Endpoint, Class, Interface, Configuration and Device Descriptors/Descriptions
 *
 */

/*Endpoint*/
static __u8 datalog_endpoint[] = {
        0x07,                      // bLength
        USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
        /*DATA_LOG_IN_ENDPOINT |*/ IN, // bEndpointAddress
        BULK,                      // bmAttributes
        0, 0x00,                       // wMaxPacketSize 
        0x00,                      // bInterval
};
static struct usb_endpoint_descriptor *datalog_endpoints[] = { datalog_endpoint };
u8 datalog_indexes1[] = { DATALOG_BULK_IN, };  

// No endpoints needed for TestCmd interface.

/*Interface*/
static __u8 datalog_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, DATALOG_INTF, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting, bNumEndpoints
        // bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, iInterface
        0xFF,               0x02,               0xFF,               0x00, };

static __u8 testcmd_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, TESTCMD_INTF, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting, bNumEndpoints
        // bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, iInterface
        0xFF,               0x03,               0xFF,               0x00, };

/*Description*/
static struct usb_alternate_description datalog_alternate_descriptions[] = {
      { iInterface:"Motorola MCU Data Logger",
             interface_descriptor: (struct usb_interface_descriptor *)&datalog_alternate_descriptor,
             endpoint_list: datalog_endpoints,
             endpoints:sizeof (datalog_endpoints) / sizeof (struct usb_endpoint_descriptor *),
             endpoint_indexes: datalog_indexes1,	
      },
};

static struct usb_alternate_description testcmd_alternate_descriptions[] = {
      { iInterface:"Motorola Test Command",
                interface_descriptor: (struct usb_interface_descriptor *)&testcmd_alternate_descriptor,
      },
};
/*interfaces*/
static struct usb_interface_description cfg11_interfaces[] = {
      { alternates:1,//sizeof (cdc_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:cdc_comm_alternate_descriptions,
      },

      { alternates:1,//sizeof (cdc_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:cdc_data_alternate_descriptions,
      },

 #if 0
      // Space the INTERFACEs out by adding as many dummies as required (for now, none).
      { alternate_list: dummy_alternate_descriptions, }, /* 0xN */
#endif

      { alternates:sizeof (datalog_alternate_descriptions) / sizeof (struct usb_alternate_description),
               alternate_list:datalog_alternate_descriptions,
      },

      { alternates:sizeof (testcmd_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:testcmd_alternate_descriptions,
     },
};


/*Configuration Descriptor and Description*/
static __u8 cfg11_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, sizeof (cfg11_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, BMATTRIBUTE, BMAXPOWER, };

struct usb_configuration_description cfg11_description[] = {
      { iConfiguration: "Motorola CDC Configuration",	// EZX
              configuration_descriptor: (struct usb_configuration_descriptor *)cfg11_configuration_descriptor,
	      bNumInterfaces:sizeof (cfg11_interfaces) / sizeof (struct usb_interface_description),
              interface_list:cfg11_interfaces,}, };

/*Device Descriptor*/
static struct usb_device_descriptor cfg11_device_descriptor = {
	bLength: sizeof(struct usb_device_descriptor),
	bDescriptorType: USB_DT_DEVICE,
	bcdUSB: __constant_cpu_to_le16(USB_BCD_VERSION),
	bDeviceClass: COMMUNICATIONS_DEVICE_CLASS,
	bDeviceSubClass: 0x02,	
	bDeviceProtocol: 0x00,
	bMaxPacketSize0: 0x00,
	idVendor: __constant_cpu_to_le16(USBD_CFG11_VENDORID),
	idProduct: __constant_cpu_to_le16(USBD_CFG11_PRODUCTID),
	bcdDevice: __constant_cpu_to_le16(USBD_CFG11_BCDDEVICE),
};

/*endpoint request map*/
static struct usb_endpoint_request cfg11_endpoint_requests[ENDPOINTS_CFG11+1] = {
        { 1, 1, 0, USB_DIR_OUT | USB_ENDPOINT_BULK, 64, 512, },
        { 1, 1, 0, USB_DIR_IN | USB_ENDPOINT_BULK, 64 /* * 4 */, 512, },
        { 1, 0, 0, USB_DIR_IN | USB_ENDPOINT_INTERRUPT, 16, 64, },
        { 1, 2, 0, USB_DIR_IN | USB_ENDPOINT_BULK, 64, 512, },
        { 0, },
};

struct usb_device_description cfg11_device_description = {
        device_descriptor: &cfg11_device_descriptor,
		iManufacturer: "Belcarra", //CONFIG_USBD_ACM_MANUFACTURER,
		iProduct: "Belcarra",
		  // "Motorola CDC Configuration", 
		  // //CONFIG_USBD_ACM_PRODUCT_NAME,
#if !defined(CONFIG_USBD_NO_SERIAL_NUMBER) && defined(CONFIG_USBD_SERIAL_NUMBER_STR)
	iSerialNumber: CONFIG_USBD_SERIAL_NUMBER_STR, 
#endif
        endpointsRequested: ENDPOINTS_CFG11,
        requestedEndpoints: cfg11_endpoint_requests,
};
/* 
 * acm_urb_sent_pst - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 */
 int acm_urb_sent_pst (struct urb *urb, int rc)
{
        struct usb_function_instance *function;

	if (!urb || !(function = urb->function_instance)) {
		return(-EINVAL);
	}
//	TRACE_MSG1("PST length=%d",urb->actual_length);

#if defined(PST_FD_AVAILABLE)
        pst_urb_sent(urb, rc);
#endif
        urb->privdata = NULL;
        usbd_dealloc_urb(urb);
        return 0;
}

extern int cable_connected;

void cfg11_event_irq (struct usb_function_instance *function, usb_device_event_t event, int data)
{
	switch (event) {
		// Fix me: -- w20146, Jun 1, 2004
		// CFG11 may need other event to handle other things
		// Refer to pst-fd.c for detailed information
		case DEVICE_CONFIGURED:
			cable_connected = 1;
			break;
			
		case DEVICE_HUB_RESET:
			cable_connected = 0;
			break;
			
		case DEVICE_CREATE:
			//if (0 != (ret = pst_dev_create(device)))
			if (0 != (pst_dev_create(function)))	// EZX
				return;
			break;
			
		case DEVICE_DESTROY:
			pst_dev_destroy();
			break;
	}

	acm_event_irq(function, event, data);
}
/*
 * cfg11_recv_setup_irq--called to indicate urb has been received
 */
 int cfg11_recv_setup_irq (struct usb_device_request *request)
{
     // verify that this is a usb class request per cdc-acm specification or a vendor request.
     if (!(request->bmRequestType & (USB_REQ_TYPE_CLASS | USB_REQ_TYPE_VENDOR))) {
//		TRACE_MSG("--> 0");
		return(0);
	}
     
    // determine the request direction and process accordingly
	switch (request->bmRequestType & (USB_REQ_DIRECTION_MASK | USB_REQ_TYPE_MASK)) {
	case USB_REQ_HOST2DEVICE | USB_REQ_TYPE_VENDOR: 	
        case USB_REQ_DEVICE2HOST | USB_REQ_TYPE_VENDOR: 	
        	ep0_process_vendor_request(request);
		return 0;
        default: break;
	}

    return acm_recv_setup_irq(request);
}
static int cfg11_function_enable(struct usb_function_instance *function)
{
	pst_function_enable(function);
//	TRACE_MSG("calling pst_function_enable ret!");

	return acm_function_enable(function);
}

static void cfg11_function_disable (struct usb_function_instance *function)
{
	  pst_function_disable(function);
//	  TRACE_MSG("calling pst_function_disable ret!");

	  return acm_function_disable(function);
}
static struct usb_function_operations cfg11_function_ops = {
	event_irq: cfg11_event_irq,
	recv_setup_irq: cfg11_recv_setup_irq,
        function_enable: cfg11_function_enable,
        function_disable: cfg11_function_disable,
};
static struct usb_function_driver cfg11_function_driver = {
	name: "cfg11",
	fops:&cfg11_function_ops,
	device_description:&cfg11_device_description,
	bNumConfigurations:sizeof (cfg11_description) / sizeof (struct usb_configuration_description),
	configuration_description:cfg11_description,
	idVendor: __constant_cpu_to_le16(USBD_CFG11_VENDORID),
	idProduct: __constant_cpu_to_le16(USBD_CFG11_PRODUCTID),
	bcdDevice: __constant_cpu_to_le16(USBD_CFG11_BCDDEVICE),
};

/*USB Module init/exit*/
/*
 *cfg11_modinit ---- module init
 */
 static int cfg11_modinit(void)
{

    printk (KERN_INFO "Copyright (c) 2004 cfg11 w20535@motorola.com\n");
    THROW_IF (usbd_register_function (&cfg11_function_driver), error);
    cfg11_private.usb_driver_registered++;
    
    CATCH(error) {
          printk(KERN_ERR"%s: ERROR\n", __FUNCTION__);

          if (cfg11_private.usb_driver_registered) {
			usbd_deregister_function (&cfg11_function_driver);
            cfg11_private.usb_driver_registered = 0;
          }
//		  TRACE_MSG("--> -EINVAL");
          return -EINVAL;
        }
    return 0;
}
static void cfg11_modexit(void)
{
//	TRACE_MSG("cfg11 exit entered");
	if (cfg11_private.usb_driver_registered) {
		usbd_deregister_function (&cfg11_function_driver);
		cfg11_private.usb_driver_registered = 0;
	}
}
module_init (cfg11_modinit);
module_exit (cfg11_modexit);
