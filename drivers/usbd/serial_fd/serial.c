/*
 * linux/drivers/usb/device/serial_fd/serial.c
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

/* 
 * The encapsultaion is designed to overcome difficulties with some USB hardware.
 *
 * While the USB protocol has a CRC over the data while in transit, i.e. while
 * being carried over the bus, there is no end to end protection. If the hardware
 * has any problems getting the data into or out of the USB transmit and receive
 * FIFO's then data can be lost. 
 *
 * This protocol adds a two byte trailer to each USB packet to specify the number
 * of bytes of valid data and a 10 bit CRC that will allow the receiver to verify
 * that the entire USB packet was received without error.
 *
 * This means we now have end to end protection from the class driver to the function
 * driver and back.
 *
 * There is an additional option that can be used to force all transmitted packets
 * to be padded to the maximum packet size. This provides a work around for some
 * devices which have problems with small USB packets.
 *
 * Assuming a packetsize of N:
 *
 *      0..N-2  data and optional padding
 *
 *      N-2     bits 7-2 - number of bytes of valid data
 *              bits 1-0 top two bits of 10 bit CRC
 *      N-1     bottom 8 bits of 10 bit CRC
 *
 *
 *      | Data Length       | 10 bit CRC                                |
 *      + 7 . 6 . 5 . 4 . 3 . 2 . 1 . 0 | 7 . 6 . 5 . 4 . 3 . 2 . 1 . 0 +
 *      
 * The 10 bit CRC is computed across the sent data, followed by the trailer with
 * the length set and the CRC set to zero. The CRC is then OR'd into the trailer.
 *
 * When received a 10 bit CRC is computed over the entire frame including the trailer
 * and should be equal to zero.
 *
 * Two module parameters are used to control the encapsulation, if both are
 * turned of the module works as a simple serial device with NO
 * encapsulation.
 *
 * See linux/drivers/usb/serial/safe_serial.c for a host class driver
 * implementation of this.
 *
 */


#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"


MODULE_AUTHOR ("sl@belcarra.com, tbr@belcarra.com");
MODULE_DESCRIPTION ("USB Device Serial Function");

USBD_MODULE_INFO ("serial_fd 0.1-beta");

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

#if 0
#define MIN(a, b) ({            \
        typeof(a) _a = (a);     \
        typeof(b) _b = (b);     \
        _a < _b ? _a : _b;      \
})
#endif


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


struct usb_serial_private {
	int interface;
	struct usb_device_instance *device;
	rwlock_t rwlock;
};

/* Module Parameters ************************************************************************* */

static char *dbg = NULL;
static u32 vendor_id;
static u32 product_id;
static u32 txqueue_urbs;
static u32 txqueue_bytes = 3032;

#ifndef CONFIG_USBD_SERIAL_SAFE_DEFAULT
#define CONFIG_USBD_SERIAL_SAFE_DEFAULT 0
#endif
#ifndef CONFIG_USBD_SERIAL_SAFE_PADDED
#define CONFIG_USBD_SERIAL_SAFE_PADDED 0
#endif

static int safe = CONFIG_USBD_SERIAL_SAFE_DEFAULT;
static int padded = CONFIG_USBD_SERIAL_SAFE_PADDED;

MODULE_PARM (dbg, "s");
MODULE_PARM (vendor_id, "i");
MODULE_PARM (product_id, "i");
MODULE_PARM (txqueue_urbs, "i");
MODULE_PARM (txqueue_bytes, "i");
MODULE_PARM (safe, "i");
MODULE_PARM (padded, "i");

MODULE_PARM_DESC (dbg, "USB Device Debug options");
MODULE_PARM_DESC (vendor_id, "USB Device Vendor ID");
MODULE_PARM_DESC (product_id, "USB Device Product ID");
MODULE_PARM_DESC (txqueue_urbs, "Maximum TX Queue Urbs");
MODULE_PARM_DESC (txqueue_bytes, "Maximum TX Queue Bytes");
MODULE_PARM_DESC (safe, "Safe Encapsulation");
MODULE_PARM_DESC (padded, "Safe Encapsulation Padding");



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

static int serial_created;

static struct usb_serial_private *serial_private_array[MAX_INTERFACES];

static rwlock_t serial_rwlock = RW_LOCK_UNLOCKED;	// lock for serproto device array access

static __inline__ struct usb_serial_private *get_serial_private (int interface)
{
	if (interface < 0 || interface >= MAX_INTERFACES) {
		return NULL;
	}
	return serial_private_array[interface];
}


/* usb-func.c ******************************************************************************** */

/*

Device descriptor                  12 01 00 02 02 00 00 10 86 80 c8 a6 00 00 00 00 00 01 

Configuration descriptor [0      ] 09 02 27 00 01 01 01 c0 00 

Interface descriptor     [0:1:0  ] 09 04 00 00 03 ff 02 01 02 
Endpoint descriptor      [0:1:0:0] 07 05 02 02 40 00 00 
Endpoint descriptor      [0:1:0:1] 07 05 81 02 40 00 00 
Endpoint descriptor      [0:1:0:2] 07 05 85 03 08 00 00 


String                   [ 0]      04 09 
String                   [ 1:64]   USB Simple Serial Configuration
String                   [ 2:82]   Simple Serial Data Interface - Bulk mode


 */


/* Communications Interface Class descriptions 
 */

static __u8 serial_data_1[] = {
        0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,      BULK_OUT_PKTSIZE, 0x00, 0x00, 
};
static __u8 serial_data_2[] = {
        0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT  | IN,  BULK,      BULK_IN_PKTSIZE,  0x00, 0x00, 
};
static __u8 serial_comm_1[] = {
#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
        0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT, INT_IN_PKTSIZE, 0x00, 0x0a, 
#endif
};

static __u8 *serial_default[] = {
        serial_data_1, serial_data_2,
#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
        serial_comm_1,
#endif
};


static __u32 serial_default_transferSizes[] = {
        BULK_OUT_PKTSIZE, BULK_IN_PKTSIZE, INT_IN_PKTSIZE,
};

/* Data Interface Alternate description(s)
 */
static __u8 serial_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (serial_default) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        LINEO_CLASS, LINEO_SUBCLASS_BASIC_SERIAL, LINEO_BASIC_SERIAL_CRC, 0x00,
};
//Interface descriptor     [0:1:0  ] 09 04 00 00 03 ff 02 01 02 
static struct usb_alternate_description serial_data_alternate_descriptions[] = {
      { iInterface:"Simple Serial Data Interface - Bulk mode",
              interface_descriptor: (struct usb_interface_descriptor *)&serial_data_alternate_descriptor,
	      //bAlternateSetting:0,
	      endpoints:sizeof (serial_default) / sizeof(__u8 *),
              endpoint_list: serial_default,
              endpoint_transferSizes_list: serial_default_transferSizes,
      },
};

/* Interface description(s)
 */
static struct usb_interface_description serial_interfaces[] = {
      {//iInterface:"Simple Serial Data Interface",
	      alternates:sizeof (serial_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:serial_data_alternate_descriptions,
      },
};


#ifdef CONFIG_USBD_SERIAL_CDC
/*
 * CDC ACM Configuration
 */


/* Communication Interface Class descriptors
 */


static __u8 serial_alt_1_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,      BULK_OUT_PKTSIZE, 0x00, 0x00, };
static __u8 serial_alt_1_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT  | IN,  BULK,      BULK_IN_PKTSIZE,  0x00, 0x00, };

static __u8 cdc_class_1[] = { 0x05, CS_INTERFACE, USB_ST_HEADER, 0x10, 0x01, /* CLASS_BDC_VERSION, CLASS_BDC_VERSION */ };
static __u8 cdc_class_2[] = { 0x05, CS_INTERFACE, USB_ST_UF, 0x00, 0x01, /* bMasterInterface: 0, bSlaveInterface: 1 */ };
static __u8 cdc_class_3[] = { 0x05, CS_INTERFACE, USB_ST_CMF, 0x00, 0x01, /* bMasterInterface: 0, bSlaveInterface: 1 */ };
static __u8 cdc_class_4[] = { 0x06, CS_INTERFACE, USB_ST_ACMF, 0x00, 0x00, 0x00, };


static __u8 *serial_alt_1_endpoints[] = { serial_alt_1_1, serial_alt_1_2, };
static __u8 *cdc_comm_class_descriptors[] = { cdc_class_1, cdc_class_2, cdc_class_3, cdc_class_4, };
static __u32 serial_alt_1_transferSizes[] = { BULK_OUT_PKTSIZE, BULK_IN_PKTSIZE, };

/*
Endpoint descriptor      [0:1:0:2] 07 05 85 03 08 00 0a 
*/
#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
static __u8 *comm_alt_1 = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT, INT_IN_PKTSIZE, 0x00, 0x0a, };
static __u8 *comm_alt_1_endpoints[] = { comm_alt_1, };
static __u32 comm_alt_1_transferSizes[] = { INT_IN_PKTSIZE, };
#endif

/* Data Interface Alternate description(s)
 */
static __u8 cdc_comm_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (comm_alt_1_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        COMMUNICATIONS_INTERFACE_CLASS, COMMUNICATIONS_ACM_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};
static __u8 cdc_nodata_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x01, 0x00, // bInterfaceNumber, bAlternateSetting
        0x00, // bNumEndpoints
        DATA_INTERFACE_CLASS, COMMUNICATIONS_NO_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};
static __u8 cdc_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x01, 0x01, // bInterfaceNumber, bAlternateSetting
        sizeof (serial_alt_1_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        DATA_INTERFACE_CLASS, COMMUNICATIONS_NO_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};

static struct usb_alternate_description cdc_comm_alternate_descriptions[] = {
      { iInterface:"CDC ACM Comm Interface",
              interface_descriptor: (struct usb_interface_descriptor *)&cdc_comm_alternate_descriptor,
	      classes:sizeof (cdc_comm_class_descriptors) / sizeof (__u8 *),
              class_list:(struct usb_class_descriptor *)cdc_comm_class_descriptors,
              endpoint_list: comm_alt_1_endpoints,
	      endpoints:sizeof (comm_alt_1_endpoints) / sizeof(__u8 *),
              endpoint_transferSizes_list: comm_alt_1_transferSizes,
      },
};

static struct usb_alternate_description cdc_data_alternate_descriptions[] = {
      { iInterface:"CDC ACM Data Interface - Disabled mode",
              interface_descriptor: (struct usb_interface_descriptor *)&cdc_nodata_alternate_descriptor,
      },

      { iInterface:"CDC ACM Data Interface - Bulk mode",
              interface_descriptor: (struct usb_interface_descriptor *)&cdc_data_alternate_descriptor,
              endpoint_list: serial_alt_1_endpoints,
	      endpoints:sizeof (serial_alt_1_endpoints) / sizeof(__u8 *),
              endpoint_transferSizes_list: serial_alt_1_transferSizes,
      },
};


/* Interface description(s)
 */
static struct usb_interface_description cdc_interfaces[] = {
      { alternates:sizeof (cdc_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:cdc_comm_alternate_descriptions,},

      { alternates:sizeof (cdc_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
              alternate_list:cdc_data_alternate_descriptions,},
};

#endif				/* CONFIG_USBD_SERIAL_CDC */

/* Configuration description(s)
 */
#ifdef CONFIG_USBD_SERIAL_CDC
static __u8 cdc_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};
#endif
static __u8 safe_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (serial_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};


struct usb_configuration_description serial_description[] = {
#ifdef CONFIG_USBD_SERIAL_CDC
      { iConfiguration:"CDC 1.1 ACM Configuration",
              configuration_descriptor: (struct usb_configuration_descriptor *)cdc_configuration_descriptor,
	      interfaces:sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
              interface_list:cdc_interfaces,},
#endif

      { iConfiguration:"USB Simple Serial Configuration",
              configuration_descriptor: (struct usb_configuration_descriptor *)safe_configuration_descriptor,
	      interfaces:sizeof (serial_interfaces) / sizeof (struct usb_interface_description),
              interface_list:serial_interfaces,},
};

/* Device Description
 */

static __u8 serial_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 
        0x00, 0x02, // bcdUSB
        COMMUNICATIONS_DEVICE_CLASS, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description serial_device_description = {
        device_descriptor: (struct usb_device_descriptor *)serial_device_descriptor,
	idVendor:CONFIG_USBD_VENDORID,
	idProduct:CONFIG_USBD_PRODUCTID,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
	iProduct:CONFIG_USBD_PRODUCT_NAME,
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};



/* ACM ***************************************************************************************** */

/*
 * Requests.
 */

#define USB_RT_ACM              (USB_TYPE_CLASS | USB_RECIP_INTERFACE)

#define ACM_REQ_COMMAND         0x00
#define ACM_REQ_RESPONSE        0x01
#define ACM_REQ_SET_FEATURE     0x02
#define ACM_REQ_GET_FEATURE     0x03
#define ACM_REQ_CLEAR_FEATURE   0x04

#define ACM_REQ_SET_LINE        0x20
#define ACM_REQ_GET_LINE        0x21
#define ACM_REQ_SET_CONTROL     0x22
#define ACM_REQ_SEND_BREAK      0x23

/*
 * IRQs.
 */

#define ACM_IRQ_NETWORK         0x00
#define ACM_IRQ_LINE_STATE      0x20

/*
 * Output control lines.
 */

#define ACM_CTRL_DTR            0x01
#define ACM_CTRL_RTS            0x02

/*
 * Input control lines and line errors.
 */

#define ACM_CTRL_DCD            0x01
#define ACM_CTRL_DSR            0x02
#define ACM_CTRL_BRK            0x04
#define ACM_CTRL_RI             0x08

#define ACM_CTRL_FRAMING        0x10
#define ACM_CTRL_PARITY         0x20
#define ACM_CTRL_OVERRUN        0x40

/*
 * Line speed and caracter encoding.
 */

struct acm_line {
        __u32 speed;
        __u8 stopbits;
        __u8 parity;
        __u8 databits;
} __attribute__ ((packed));



/* Transmit Function - called by serproto ****************************************************** */

static int serial_xmit_data (int interface, __u8 *data, int data_length)
{
	int port = 0;		// XXX compound device
	struct usb_serial_private *serial_private;
	struct usb_device_instance *device;
	struct urb *urb;
	int packet_length = data_length;

	dbg_tx (2, "data: %p data_length: %d", data, data_length);

	if ((serial_private = get_serial_private (interface)) == NULL) {
		dbg_tx (0, "cannot recover serial private");
		return -EINVAL;
	}

	if ((device = serial_private->device) == NULL) {
		dbg_tx (0, "cannot recover serial private device");
		return -EINVAL;
	}
	// XXX Check if we are busy

	if ((urb = usbd_alloc_urb (serial_private->device, BULK_IN_ENDPOINT, 0)) == NULL) {
		dbg_tx (0, "failed to alloc urb");
		return -EINVAL;
	}
#ifdef CONFIG_USBD_SERIAL_SAFE
	if (safe) {
		__u16 fcs;

		dbg_tx (1, "safe mode: padded: %d data_length: %d packet_length: %d", padded,
			data_length, packet_length);

		// extend length to pad if required
		if (padded) {
			//packet_length = MAX(packet_length, BULK_IN_PKTSIZE - 2);
			//packet_length = MIN(MAX(packet_length, packet_length), BULK_IN_PKTSIZE - 2);

			packet_length = BULK_IN_PKTSIZE - 2;
			//packet_length = 2;
			//packet_length++;
		}
		// set length and a null byte 
		data[packet_length] = data_length << 2;
		data[packet_length + 1] = 0;

		// compute CRC across data, padding, data_length and null byte
		fcs = fcs_compute10 (data, packet_length + 2, CRC10_INITFCS);

		// OR CRC into last two bytes
		data[packet_length] |= fcs >> 8;
		data[packet_length + 1] = fcs & 0xff;
		packet_length += 2;
		dbg_tx (1, "safe mode: data_length: %d packet_length: %d", data_length,
			packet_length);
	}
#endif

	urb->buffer = data;
	urb->actual_length = packet_length;


	//dbgPRINTmem(dbgflg_usbdfd_tx,3,urb->buffer,urb->actual_length);
	dbgPRINTmem (dbgflg_usbdfd_tx, 3, urb->buffer, BULK_IN_PKTSIZE + 4);

	// push it down into the usb-device layer
	return usbd_send_urb (urb);
}

/* serial_urb_sent - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 */
int serial_urb_sent (struct urb *urb, int rc)
{
	int port = 0;		// XXX compound device
	struct usb_device_instance *device = urb->device;
	struct usb_serial_private *serial_private =
	    (device->active_function_instance)->privdata;

	dbg_tx (2, "%s safe: %d length: %d", urb->device->name, safe, urb->actual_length);
	serproto_done (serial_private->interface,
		       urb->buffer,
		       safe ? (urb->buffer[urb->actual_length - 2] >> 2) : urb->actual_length, 0);
	usbd_dealloc_urb (urb);
	return 0;
}


/* USB Device Functions ************************************************************************ */

/* serial_event_irq - process a device event
 *
 */
void serial_event_irq (struct usb_device_instance *device, usb_device_event_t event, int data)
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
			struct usb_serial_private *serial_private;

			// There is no way to indicate error, so make this unconditional
			// and undo it in the DESTROY event unconditionally as well.
			// It the responsibility of the USBD core and the bus interface
			// to see that there is a matching DESTROY for every CREATE.
			// XXX XXX MOD_INC_USE_COUNT;  // Before any sleepable fns such as ckmalloc
			// XXX XXX dbg_init(0,"CREATE sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));

			// sanity checks
			if (serial_created >= MAX_INTERFACES) {
				dbg_usbe (1,
					  "---> CREATE %s serial_created >= MAX_INTERFACES %d %d",
					  device->name, serial_created, MAX_INTERFACES);
				dbg_init (0, "CREATE Z1 sc=%d uc=%d", serial_created,
					  GET_USE_COUNT (THIS_MODULE));
				return;
			}

			write_lock_irqsave (&serial_rwlock, flags);
			for (i = 0; i < MAX_INTERFACES; i++) {
				if (serial_private_array[i] == NULL) {
					break;
				}
			}
			if (i >= MAX_INTERFACES) {
				write_unlock_irqrestore (&serial_rwlock, flags);
				dbg_usbe (1, "---> CREATE %s no free interfaces %d %d",
					  device->name, i, MAX_INTERFACES);
				dbg_init (0, "CREATE Z2 sc=%d uc=%d", serial_created,
					  GET_USE_COUNT (THIS_MODULE));
				return;
			}
			serial_created++;

			// allocate private data
			if ((serial_private =
			     ckmalloc (sizeof (struct usb_serial_private), GFP_ATOMIC)) == NULL) {
				serial_created--;
				//MOD_DEC_USE_COUNT;
				//dbg_init(0,"CREATE X1 sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));
				write_unlock_irqrestore (&serial_rwlock, flags);
				dbg_usbe (1, "---> CREATE malloc failed %s", device->name);
				return;
			}

			serial_private->rwlock = RW_LOCK_UNLOCKED;
			serial_private_array[i] = serial_private;
			write_unlock_irqrestore (&serial_rwlock, flags);

			dbg_usbe (1, "---> calling serproto_create(%s,-,%u,%u,%u,%u,%u)", "ttyUSBx",
				  BULK_IN_PKTSIZE, txqueue_urbs, txqueue_bytes, safe,
				  safe ? 2 : 0);

			if ((interface = serproto_create ("ttyUSBx", serial_xmit_data,
							  (safe
							   ? (BULK_IN_PKTSIZE -
							      2) : BULK_IN_PKTSIZE),
							  txqueue_urbs, txqueue_bytes, safe,
							  safe ? 2 : 0)) < 0) {
				// lock and modify device array
				write_lock_irqsave (&serial_rwlock, flags);
				lkfree (serial_private);
				serial_created--;
				//MOD_DEC_USE_COUNT;
				//dbg_init(0,"CREATE X2 sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));
				write_unlock_irqrestore (&serial_rwlock, flags);
				dbg_usbe (1, "---> serproto_create FAILED");
				return;
			}
			// lock and modify device array
			write_lock_irqsave (&serial_rwlock, flags);
			serial_private->interface = interface;
			serial_private->device = device;
			write_unlock_irqrestore (&serial_rwlock, flags);

			function->privdata = serial_private;

			dbg_usbe (1, "---> START %s privdata assigned: %p interface: %d",
				  device->name, serial_private, interface);

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
			struct usb_serial_private *serial_private;
			int interface;
			if ((serial_private =
			     (device->active_function_instance)->privdata) == NULL) {
				return;
			}
			interface = serial_private->interface;
			if (interface < 0 || interface >= MAX_INTERFACES) {
				return;
			}
			serproto_control (interface, SERPROTO_CONNECT);
		}
		break;
	case DEVICE_SET_INTERFACE:
#ifdef CONFIG_USBD_SERIAL_CDC
#endif
		break;
	case DEVICE_SET_FEATURE:
		break;
	case DEVICE_CLEAR_FEATURE:
		break;
	case DEVICE_DE_CONFIGURED:
		{
			struct usb_serial_private *serial_private;
			int interface;
			if ((serial_private =
			     (device->active_function_instance)->privdata) == NULL) {
				return;
			}
			interface = serial_private->interface;
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
			struct usb_serial_private *serial_private;
			int interface;

			if ((serial_private =
			     (device->active_function_instance)->privdata) == NULL) {
				dbg_usbe (1, "---> DESTROY %s serial_private null", device->name);
				// XXX XXX MOD_DEC_USE_COUNT;
				// XXX XXX dbg_init(0,"DESTROY Z1 sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));
				return;
			}
			dbg_usbe (1, "---> DESTROY %s serial_private %p", device->name,
				  serial_private);
			interface = serial_private->interface;

			if (interface < 0 || interface >= MAX_INTERFACES) {
				// XXX XXX MOD_DEC_USE_COUNT;
				// XXX XXX dbg_init(0,"DESTROY Z2 sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));
				return;
			}

			if (serial_private_array[interface] != serial_private) {
				// XXX XXX MOD_DEC_USE_COUNT;
				// XXX XXX dbg_init(0,"DESTROY Z3 sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));
				return;
			}

			write_lock_irqsave (&serial_rwlock, flags);
			serial_private_array[interface] = NULL;
			lkfree (serial_private);
			serial_created--;
			write_unlock_irqrestore (&serial_rwlock, flags);

			serproto_destroy (interface);
			// XXX XXX MOD_DEC_USE_COUNT;
			// XXX XXX dbg_init(0,"DESTROY sc=%d uc=%d",serial_created,GET_USE_COUNT(THIS_MODULE));

			dbg_usbe (1, "---> STOP %s", device->name);
			return;
		}
		break;

	case DEVICE_FUNCTION_PRIVATE:
		break;
	}
}



/* serial_recv_urb - called to indicate URB has been received
 * @urb - pointer to struct urb
 *
 * Return non-zero if we failed and urb is still valid (not disposed)
 */
int serial_recv_urb (struct urb *urb)
{
	int port = 0;		// XXX compound device
	struct usb_device_instance *device = urb->device;
	struct usb_serial_private *serial_private =
	    (device->active_function_instance)->privdata;
	int interface = serial_private->interface;

	dbg_rx (2, "length=%d", urb->actual_length);
	dbgPRINTmem (dbgflg_usbdfd_rx, 3, urb->buffer, urb->actual_length);

#ifdef CONFIG_USBD_SERIAL_SAFE
	if (safe) {
		__u16 fcs;

		if ((fcs = fcs_compute10 (urb->buffer, urb->actual_length, CRC10_INITFCS))) {
			dbg_rx (0, "CRC check failed");
			//return -EINVAL;
		} else {
			// recover the length from the trailer
			urb->actual_length = (urb->buffer[urb->actual_length - 2] >> 2);
		}
	}
	dbg_rx (2, "revised length=%d", urb->actual_length);
#endif
#if 0
	if (0 != dbgflg_usbdfd_loopback) {
		if (usbd_send_urb (urb)) {
			//XXX verify not freed in usbd_send_urb()
			//urb->buffer = NULL;
			//urb->actual_length = 0;
			//usbd_dealloc_urb(urb);
		}
		return 0;
	}
#endif

	// push the data up
	if (serproto_recv (interface, urb->buffer, urb->actual_length)) {
		return (1);
	}
	// free urb
	usbd_recycle_urb (urb);

	return (0);
}


struct usb_function_operations function_ops = {
	event_irq:serial_event_irq,
	recv_urb:serial_recv_urb,
	urb_sent:serial_urb_sent
};

struct usb_function_driver function_driver = {
	name:"function prototype",
	ops:&function_ops,
	device_description:&serial_device_description,
	configurations:sizeof (serial_description) / sizeof (struct usb_configuration_description),
	configuration_description:serial_description,
	this_module:THIS_MODULE,
};


/*
 * serial_modinit - module init
 *
 */
static int __init serial_modinit (void)
{
	debug_option *op = find_debug_option (dbg_table, "ser");

	printk (KERN_INFO "%s (%s)\n", __usbd_module_info, dbg);

#ifdef CONFIG_USBD_SERIAL_SAFE
	printk (KERN_INFO "vendor_id: %04x product_id: %04x safe: %d padded: %d\n", vendor_id,
		product_id, safe, padded);
#else
	printk (KERN_INFO "vendor_id: %04x product_id: %04x\n", vendor_id, product_id);
	if (safe || padded) {
		printk (KERN_ERR "serial_fd: not compiled for safe mode\n");
		return -EINVAL;
	}
#endif
	printk (KERN_INFO "dbg: %s\n", dbg);

	if (NULL != op) {
		op->sub_table = serproto_get_dbg_table ();
	}
	if (0 != scan_debug_options ("serial_fd", dbg_table, dbg)) {
		return (-EINVAL);
	}

	if (vendor_id) {
		serial_device_description.idVendor = vendor_id;
	}
	if (product_id) {
		serial_device_description.idProduct = product_id;
	}
	// Initialize the function registration code.
	//
	//if (usbd_strings_init()) {
	//    return -EINVAL;
	//}
	// initialize the serproto library
	//
	if (serproto_modinit ("ttyUSBn", MAX_INTERFACES)) {
		return -EINVAL;
	}
	// register us with the usb device support layer
	//
	if (usbd_register_function (&function_driver)) {
		serproto_modexit ();
		return -EINVAL;
	}
	dbg_loop (1, "LOOPBACK mode");

	// return
	return 0;
}


/* serial_modexit - module cleanup
 */
static void __exit serial_modexit (void)
{
	// de-register us with the usb device support layer
	// 
	usbd_deregister_function (&function_driver);

	// tell the serproto library to exit
	//
	serproto_modexit ();

}

module_init (serial_modinit);
module_exit (serial_modexit);
