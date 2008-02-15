/*
 * linux/drivers/usbd/pst_fd/pst-fd.c 
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

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("Levis@Motorola.com");
MODULE_DESCRIPTION ("PST USB Function Driver");

USBD_MODULE_INFO ("pst_usb 1.0");

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <asm/arch-pxa/pxa-regs.h>

#include <linux/autoconf.h>

#include <asm/hardware.h>
#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "../usbd-arch.h"

#include "../hotplug.h"

#include "pst-usb.h"
#include "ptfproto.h"
#define VERSION "0.90"
#define VERSION25 1

#define USBC_MINOR0 17
#define USBC_MINOR1 18
#define USBC_MINOR2 19

//#define PST_DEBUG 1
//#define PST_DUMP_DEBUG 1

#if PST_DEBUG
#define PRINTKD(fmt, args...) printk( fmt , ## args)
#else
#define PRINTKD(fmt, args...)
#endif

#ifndef CONFIG_USBD_MAXPOWER
#define CONFIG_USBD_MAXPOWER                    0
#endif

#ifndef CONFIG_USBD_MANUFACTURER
#define CONFIG_USBD_MANUFACTURER                "Lineo"
#endif

#define BMAXPOWER                               0x0a

/*
 * setup some default values for pktsizes and endpoint addresses.
 */
#define TEST_CMD_PKTSIZE             	8
#define DATA_LOG_IN_PKTSIZE          	64
#define PTF_OUT_PKTSIZE               	64
#define PTF_IN_PKTSIZE           	64
#define AP_DATA_LOG_OUT_PKTSIZE   	64
#define AP_DATA_LOG_IN_PKTSIZE       	64
#define TEST_CMD_ENDPOINT            	0
#define DATA_LOG_IN_ENDPOINT       	11
#define PTF_OUT_ENDPOINT       		2//7
#define PTF_IN_ENDPOINT          	1//6
#define AP_DATA_LOG_OUT_ENDPOINT 	7//12
#define AP_DTAT_LOG_IN_ENDPOINT    	6//11

/* Module Parameters ***/
interface_t intf6,intf8;
static int usbi0_ref_count=0, usbi1_ref_count=0, usbi2_ref_count=0;
static int polling_times6=0,polling_times8=0;
static int specific_times6=0,specific_times8=0;
static int polling_ready6, polling_ready8;
static int polling_len, bplog = 0; 
static volatile int tx_pending = 0;
static struct urb *urb_polling;
static struct usb_device_instance * pst_dev=NULL;
static struct usb_function_instance * pst_func=NULL;
int cable_connected = 0;

DECLARE_WAIT_QUEUE_HEAD(wq_read6);
DECLARE_WAIT_QUEUE_HEAD(wq_read8);
DECLARE_WAIT_QUEUE_HEAD(wq_write6d);

//a17400:add ptf
struct usb_ptf_private {
	int device_index;	//it's device index in device array.
  int out_ep_addr;		/* hold OUT endpoint address for recv, IN ep address will be desinated in xxx_xmit_data */
	struct usb_device_instance *device;
};


static struct usb_ptf_private *ptf_private_array[PTF_MAX_DEV_NUMBER];	//yes, keep this number same as device number

/* *******************PTF Tool Functions******************************************************************* */

/*get_ptf_private (int device_index) --get a private instance from private array according it's device numer.
device_index:	the device num the returned private should include

return null, no such private

*/

//static __inline__ struct usb_ptf_private *get_ptf_private (int device_index)
static  struct usb_ptf_private *get_ptf_private (int device_index)
{
	int i=0;
	if (device_index < 0 || device_index >= PTF_MAX_DEV_NUMBER) {
		return NULL;
	}
	//return ptf_private_array[interface]; we don'e follow net-fd and serial-fd, it is not clear.
	for (i=0;i<PTF_MAX_DEV_NUMBER;i++)
	  if(ptf_private_array[i]){//a17400: fixed, null pointer should be check
		if(ptf_private_array[i]->device_index==device_index)
		  //return ptf_private_array[device_index];
		  return ptf_private_array[i]; /* it is better... */
	  }
	return NULL;
	
}

/* get private data by look up its OUT endpoints */
static  struct usb_ptf_private *get_ptf_private_by_out_ep (int out_endpoint)
{
	int i=0;
	if (out_endpoint < 0 || out_endpoint >15) {
		return NULL;
	}
	
	for (i=0;i<PTF_MAX_DEV_NUMBER;i++)
	  if(ptf_private_array[i]){//a17400: fixed, null pointer should be check
		if(ptf_private_array[i]->out_ep_addr==out_endpoint)
			return ptf_private_array[i];
	  }
	return NULL;
	
}
//ptf_private_create():create a private and put it in private array
/* out_ep: out endpoint
   xmit_data: xmit_data callback
   tx_size: max tx_size
   return <0 error
   retunr 0 success

*/
int ptf_private_create(struct usb_device_instance *device,int out_ep,int (*xmit_data) (int, unsigned char *, int), int tx_size){
  struct usb_ptf_private * ptf_private;
  int device_index,i;
  for (i = 0; i < PTF_MAX_DEV_NUMBER; i++) {
    if (ptf_private_array[i] == NULL) {
      break;
    }
  }
  if (i >= PTF_MAX_DEV_NUMBER) {
    printk (KERN_WARNING "ptf private array full\n");
    return -1;
  }
  
  // allocate private data
  if ((ptf_private =
       kmalloc (sizeof (struct usb_ptf_private), GFP_ATOMIC)) == NULL) {//??? GFP_ATOMIC is ok?
    printk (KERN_WARNING "can't allocate mem for ptf private \n");
    return -1;
  }
  if (0>(device_index = ptfproto_create ("", xmit_data, tx_size) )){//???max tx size?
    kfree (ptf_private);
    printk (KERN_WARNING "can't apply new device in ptf device array\n");
    return-1;
  }
  // lock and modify device array
  ptf_private->device_index= device_index;
  ptf_private->device = device;
  ptf_private->out_ep_addr=out_ep;
  ptf_private_array[i]=ptf_private;
  return 0;
			
}
//ptf_private_destroy() destory all private in private array and proto devices

void ptf_private_destroy(){
  int i;
  for(i=0;i < PTF_MAX_DEV_NUMBER; i++) {
    int dev_index;
    struct usb_ptf_private *ptf_private=ptf_private_array[i];
    if (ptf_private){
      dev_index=ptf_private->device_index;
      
      //destory private in private array
      ptf_private_array[i] = NULL;	//we have made sure dev_index is equal to private's index in private array
      
      kfree (ptf_private);	//in fact, free ptf_private_array[dev_index] 
      
      //destory device in device array
      ptfproto_destory (dev_index);
    }

  }
}
//ap_log_xmit_data():xmit data via ap IN endpoint

static int ap_log_xmit_data (int dev_index, unsigned char *data, int data_length)
{
	int port = 0;		// XXX compound device //a17400 always 0
	struct usb_ptf_private *ptf_private;
	struct usb_device_instance *device;
	struct urb *urb;
	int packet_length = data_length;
	//printk("ptf_xmit_data() begining:\nUDCCS11=%x UICR1=%x\n",UDCCS11,UICR1);
	if ((ptf_private = get_ptf_private (dev_index)) == NULL) {
		printk (KERN_WARNING "Can't recover private\n");
		return -EINVAL;
	}

	if ((device = ptf_private->device) == NULL) {
	  printk (KERN_WARNING "Can't recover private's device\n");
		return -EINVAL;
	}
	//build urb
	//??? functions array changed!! need make sure
#if 0
	if ((urb = usbd_alloc_urb (ptf_private->device,
				   (ptf_private->device->functions_instance_array + port),
				   AP_DTAT_LOG_IN_ENDPOINT, 0)) == NULL)
#endif
	  if((urb=usbd_alloc_urb (ptf_private->device,AP_DTAT_LOG_IN_ENDPOINT,0))==NULL)
	  {
		printk (KERN_WARNING  "ap_xmit_data: failed to alloc urb\n");
		return -EINVAL;
	}

	urb->buffer = data;
	urb->actual_length = packet_length;

	// push it down into the usb-device layer
	return usbd_send_urb (urb);	//??? finally bi_send_urb(), return 0 when finished
	
	//usbd_dealloc_urb() will kfree urb in ptf_urb_sent()

	
}

//ptf_xmit_data(): xmit data via ptf IN endpoint

static int ptf_xmit_data (int dev_index, unsigned char *data, int data_length)
{
	int port = 0;		// XXX compound device //a17400 always 0
	struct usb_ptf_private *ptf_private;
	struct usb_device_instance *device;
	struct urb *urb;
	int packet_length = data_length;
	//printk("ptf_xmit_data() begining:\nUDCCS11=%x UICR1=%x\n",UDCCS11,UICR1);
	if ((ptf_private = get_ptf_private (dev_index)) == NULL) {
		printk (KERN_WARNING "Can't recover private\n");
		return -EINVAL;
	}

	if ((device = ptf_private->device) == NULL) {
	  printk (KERN_WARNING "Can't recover private's device\n");
		return -EINVAL;
	}
	//build urb
	//??? functions array changed!! need make sure
#if 0
	if ((urb = usbd_alloc_urb (ptf_private->device,
				   (ptf_private->device->functions_instance_array + port),
				   PTF_IN_ENDPOINT, 0)) == NULL)
#endif
	  if((urb=usbd_alloc_urb (ptf_private->device,PTF_IN_ENDPOINT,0))==NULL)
	  {
		printk (KERN_WARNING  "ptf_xmit_data: failed to alloc urb\n");
		return -EINVAL;
	}

	urb->buffer = data;
	urb->actual_length = packet_length;

	// push it down into the usb-device layer
	return usbd_send_urb (urb);	//??? finally bi_send_urb(), return 0 when finished
	
	//usbd_dealloc_urb() will kfree urb in ptf_urb_sent()

	
}

//int ptf_urb_sent (struct urb *urb) ???


/* ptf_recv_urb - called to indicate URB has been received
 * @urb - pointer to struct urb
 *
 * Return non-zero if we failed and urb is still valid (not disposed)
 */
 
int ptf_recv_urb (struct urb *urb)
{
  //int port = 0;		// XXX compound device	//a17400: always 0???
	struct usb_device_instance *device = urb->device;
	//??? functions array changed!! need make sure
	//get private data from urb->device
	//changed!! need make sure
#if 0
	struct usb_ptf_private *ptf_private = (device->functions_instance_array + port)->privdata;
#endif
	struct usb_ptf_private *func_priv = (device->active_function_instance)->privdata;

	if (func_priv==ptf_private_array){ /* yes, it is our function driver and it is our turn! */
	  
	  struct usb_ptf_private *ptf_private = get_ptf_private_by_out_ep(urb->endpoint->endpoint_address);
	  
	  if(NULL==ptf_private){
	    printk("no suiable private for this OUT endpoint");
	    return -EINVAL;
	  }
       
	  //get device array index from private
	  int dev_index = ptf_private->device_index;
	  //printk("ptf_recv_urb():\n active func instance=%x\n",device->active_function_instance);
	  // printk("private=%x\nurb's endpoint logic addr=%x\n",ptf_private,urb->endpoint->endpoint_address);
	  // push the data up
	  if (ptfproto_recv (dev_index, urb->buffer, urb->actual_length)) {
	    return (-1);
	  }
	  // free urb
	  usbd_recycle_urb (urb);
	  return 0;
	} /* endif func_priv==  */
	return (-1);
}


/* USB Configuration Description by Tom************************************************************* */
/* USB Interface and Endpoint Configuration ********************** */

static __u8 datalog_endpoint[] = {
	0x07,                      // bLength
	USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
	DATA_LOG_IN_ENDPOINT | IN, // bEndpointAddress
	BULK,                      // bmAttributes
	BULK_IN_PKTSIZE, 0x00,     // wMaxPacketSize   ???????????? DATA_LOG_IN_PKTSIZE
	0x00,                      // bInterval
};

static  __u8 *datalog_endpoints[] = { datalog_endpoint };
static __u32  datalog_transferSizes[] = { BULK_IN_PKTSIZE }; //?????????
/*a17400: add ptf and AP logger endpoints according to new excel*/

static __u8 ap_log_endpoint_in[] = {
/*endpoint in*/
	0x07,                      // bLength
	USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
	AP_DTAT_LOG_IN_ENDPOINT | IN, // bEndpointAddress
	BULK,                      // bmAttributes
	BULK_IN_PKTSIZE, 0x00,     // wMaxPacketSize   ???????????? DATA_LOG_IN_PKTSIZE
	0x00,                      // bInterval
};
static __u8 ap_log_endpoint_out[] = {
/*endpoint out*/
	0x07,                      // bLength
	USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
	AP_DATA_LOG_OUT_ENDPOINT | OUT, // bEndpointAddress
	BULK,                      // bmAttributes
	BULK_OUT_PKTSIZE, 0x00,     // wMaxPacketSize   ???????????? DATA_LOG_IN_PKTSIZE
	0x00,                      // bInterval
};

static  __u8 *ap_log_endpoints[] = { ap_log_endpoint_in,ap_log_endpoint_out };
static __u32  ap_log_transferSizes[] = { BULK_IN_PKTSIZE, BULK_OUT_PKTSIZE}; //?????????


static __u8 ptf_endpoint_in[] = {
/*endpoint in*/
	0x07,                      // bLength
	USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
	PTF_IN_ENDPOINT | IN, // bEndpointAddress
	BULK,                      // bmAttributes
	BULK_IN_PKTSIZE, 0x00,     // wMaxPacketSize   ???????????? DATA_LOG_IN_PKTSIZE
	0x00,   // bInterval
};

static __u8 ptf_endpoint_out[] = {                   
/*endpoint out*/
	0x07,                      // bLength
	USB_DT_ENDPOINT,           // bDescriptorType   // 0x5
	PTF_OUT_ENDPOINT | OUT, // bEndpointAddress
	BULK,                      // bmAttributes
	BULK_OUT_PKTSIZE, 0x00,     // wMaxPacketSize   ???????????? DATA_LOG_IN_PKTSIZE
	0x00,                      // bInterval

	
};

static  __u8 *ptf_endpoints[] = {ptf_endpoint_in,ptf_endpoint_out };
static __u32  ptf_transferSizes[] = { BULK_IN_PKTSIZE,BULK_OUT_PKTSIZE }; //?????????

/*
 * Data Interface Alternate descriptor and description (one endpoint)
 */

#define PST_DATA_CLASS         0xff
#define PST_DATA_SUBCLASS      0x02
#define PST_DATA_PROTOCOL      0xff

static __u8 datalog_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09,              // bLength
        USB_DT_INTERFACE,  // bDescriptorType   // 0x04
        0x06,              // bInterfaceNumber
        0x00,              // bAlternateSetting
        sizeof (datalog_endpoints) / sizeof(__u8 *),
                           // bNumEndpoints
        PST_DATA_CLASS,    // bInterfaceClass
        PST_DATA_SUBCLASS, // bInterfaceSubClass
        PST_DATA_PROTOCOL, // bInterfaceProtocol
        0x00,              // iInterface
};

static struct usb_alternate_description datalog_alternate_descriptions[] = {
    {
 	iInterface:"Motorola MCU Data Logger",
	interface_descriptor: (struct usb_interface_descriptor *)&datalog_alternate_descriptor,
        endpoints:sizeof (datalog_endpoints) / sizeof (__u8 *),
        endpoint_list:datalog_endpoints,
	endpoint_transferSizes_list:datalog_transferSizes,
    },
};
/*a17400 add ptf and ap logger alternate description(or)s*/

static __u8 ap_log_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09,              // bLength
        USB_DT_INTERFACE,  // bDescriptorType   // 0x04
        0x0d,              // bInterfaceNumber
        0x00,              // bAlternateSetting
        sizeof (ap_log_endpoints) / sizeof(__u8 *),
                           // bNumEndpoints
        0xff,    // bInterfaceClass
        0x35, // bInterfaceSubClass
        0xff, // bInterfaceProtocol
        0x0c,              // iInterface
};


static struct usb_alternate_description ap_log_alternate_descriptions[] = {
    {
 	iInterface:"Motorola AP Data Logger",
	interface_descriptor: (struct usb_interface_descriptor *)&ap_log_alternate_descriptor,
        endpoints:sizeof (ap_log_endpoints) / sizeof (__u8 *),
        endpoint_list:ap_log_endpoints,
	endpoint_transferSizes_list:ap_log_transferSizes,
    },
};


static __u8 ptf_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09,              // bLength
        USB_DT_INTERFACE,  // bDescriptorType   // 0x04
        0x0e,              // bInterfaceNumber
        0x00,              // bAlternateSetting
        sizeof (ptf_endpoints) / sizeof(__u8 *),
                           // bNumEndpoints
        0xff,    // bInterfaceClass
        0x36, // bInterfaceSubClass
        0xff, // bInterfaceProtocol
        0x0c,              // iInterface
};


static struct usb_alternate_description ptf_alternate_descriptions[] = {
    {
 	iInterface:"Motorola EzX PTF",
	interface_descriptor: (struct usb_interface_descriptor *)&ptf_alternate_descriptor,
        endpoints:sizeof (ptf_endpoints) / sizeof (__u8 *),
        endpoint_list:ptf_endpoints,
	endpoint_transferSizes_list:ptf_transferSizes,
    },
};



/*
 * Comm Interface Alternate descriptor and description (no endpoints)
 */

#define PST_COMM_CLASS         0xff
#define PST_COMM_SUBCLASS      0x03
#define PST_COMM_PROTOCOL      0xff

static __u8 testcmd_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09,              // bLength
        USB_DT_INTERFACE,  // bDescriptorType   // 0x04
        0x08,              // bInterfaceNumber
        0x00,              // bAlternateSetting
        0x00,              // bNumEndpoints
        PST_COMM_CLASS,    // bInterfaceClass
        PST_COMM_SUBCLASS, // bInterfaceSubClass
        PST_COMM_PROTOCOL, // bInterfaceProtocol
        0x00,              // iInterface
};

static struct usb_alternate_description testcmd_alternate_descriptions[] = {
    {
	iInterface:"Motorola Test Command",
	interface_descriptor: (struct usb_interface_descriptor *)&testcmd_alternate_descriptor,
    },              
};

/* Interface Description(s) */

static struct usb_interface_description testcmd_datalog_interfaces[] = {

    {

        alternates:sizeof (datalog_alternate_descriptions) / sizeof (struct usb_alternate_description),
        alternate_list:datalog_alternate_descriptions,
    },
    {
        alternates:sizeof (testcmd_alternate_descriptions) / sizeof (struct usb_alternate_description),
        alternate_list:testcmd_alternate_descriptions,
     }, 

//a17400 add ap logger and ptf
    {
        alternates:sizeof (ap_log_alternate_descriptions) / sizeof (struct usb_alternate_description),
        alternate_list:ap_log_alternate_descriptions,
    },

    {
        alternates:sizeof (ptf_alternate_descriptions) / sizeof (struct usb_alternate_description),
        alternate_list:ptf_alternate_descriptions,
    },
	
};

/***USB Interface and Endpoint Configuration**********************************************/

/***USB Configuration ***/

/* Configuration description(s) */
static __u8 pst_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09,          // bLength
        USB_DT_CONFIG, // bDescriptorType   // 0x2
        0x00, 0x00,    // wTotalLength
        sizeof (testcmd_datalog_interfaces) / sizeof (struct usb_interface_description),
                       // bNumInterfaces
        0x01,          // bConfigurationValue
        0x00,          // iConfiguration
        BMATTRIBUTE,   // bmAttributes   ??????? 0xC0
        BMAXPOWER,     // bMaxPower

};

struct usb_configuration_description pst_description[] = {
    {
         iConfiguration:"Motorola PTF AP BP Configuration",
         configuration_descriptor: (struct usb_configuration_descriptor *)pst_configuration_descriptor,
         interfaces:sizeof (testcmd_datalog_interfaces) / sizeof (struct usb_interface_description),
         interface_list:testcmd_datalog_interfaces,
#ifdef VERSION25
         interface_no_renumber: 1,
#endif
    },
};
/********************************************************************/

/* Device Description */
 /***PST USB Device Descriptor***/
static __u8 pst_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12,          // bLength
        USB_DT_DEVICE, // bDescriptorType
        0x00, 0x00,    // bcdUSB,              set in usbd_function_init()
        0x00,          // bDeviceClass         ?????
        0x00,          // bDeviceSubClass      ?????
        0x00,          // bDeviceProtocol      ?????
        0x00,          // bMaxPacketSize0,     set in usbd_function_init()
        0x00, 0x00,    // idVendor,            filled from below in usbd_function_init()
        0x00, 0x00,    // idProduct,           filled from below in usbd_function_init()
        0x00, 0x00,    // bcdDevice,           filled from below in usbd_function_init()
        0x01,          // iManufacturer,       filled from below in usbd_function_init()
        0x02,          // iProduct,            filled from below in usbd_function_init()
        0x00,          // iSerialNumber,       filled from below in usbd_function_init()
        0x01,          // bNumConfigurations
};

struct usb_device_description pst_device_description = {
	device_descriptor:(struct usb_device_descriptor *)pst_device_descriptor,
	idVendor:0x22B8,
	idProduct:0x6009,	//a17400 change to config 16
	bcdDevice:0x0001,
	iManufacturer:"Motorola Inc.",
	iProduct:"Motorola Phone",
	iSerialNumber:"0",
};
/***USB Configuration End**************************************************/

/*
 * Motorola PST interfaces device driver
 *
 * USB interface 6c --- Data Command
 * Major: 10, Minor 17
 *
 *   % mknod /dev/usbi6c c 10 17
 *
 *    endpoints: ep0
 *      read and write:
 *
 * USB interface 6d --- Data Logger
 * Major: 10, Minor 19
 *
 *   % mknod /dev/usbi6d c 10 19
 *
 *    endpoints: ep1
 *      write:  write log data to host by ep1 bulkin transfer mode.
 *
 * USB interface 8 --- Test Command
 * Major: 10, Minor 18
 *
 *   % mknod /dev/usbi8 c 10 18
 *
 *    endpoints: ep0
 *      write: write messages or command results to host by ep0 control transfer mode.
 *      read: read request->wValue to user space.
 *
 *
 * dump all data in the message list for debug.
 */
#ifdef PST_DUMP_DEBUG
static void dump_msg_list( struct list_head *list )
{
	struct list_head *pos;
	msg_node_t *node;
	int i;

	printk( KERN_DEBUG "dump_msg_list\n");
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
static void dump_msg_list( struct list_head *list ) 
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
      PRINTKD("\nintf8 is opened.\n");
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
	int ret;
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
	if (stCount <= ret){
		ret = stCount;
	}
	if ( copy_to_user (pUserBuffer, node->body, ret) ) 
	{
		printk ( KERN_INFO " copy_to_user() has some problems! node->size=%d\n", node->size);
		return 0;
	}
		
	list_del (ptr);
	kfree (node->body);
	node->body = NULL;
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

	node->size = stCount;		/* Default, tCount==node->size */
	node->body = data;

	list_add_tail (&node->list, &intf6.in_msg_list);
	intf6.list_len++;

	while(1){
		if(specific_times6)
		{
			specific_times6--;
                        if(polling_times6)
                        {
				    polling_message(&intf6, stCount);
				    polling_times6--;
                        }
                        else
                        	polling_ready6++;
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
	
	if (!usbi0_ref_count)
		intf6.enable = 0;
	MOD_DEC_USE_COUNT;

	return 0;
}

static int
usbc_interface8_open (struct inode *pInode, struct file *pFile)
{
	if (init_interface (&intf8) < 0)
		return -1;
	PRINTKD("\nintf8 is opened.\n");
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
	__u16 ret;
	struct list_head* ptr=NULL;
	msg_node_t *node;

	PRINTKD("\nThis is intf8_read1.\n");
	//printk("\nThis is intf8_read1.\n");

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

	//printk("\nThis is intf8_read2.\n");
	ptr = intf8.out_msg_list.next;
	node = list_entry (ptr, msg_node_t, list);
	ret = node->size;
	if (stCount <= ret){
		ret = stCount;
	}
				
	//printk("\nThis is intf8_read3.\n");
	if ( copy_to_user (pUserBuffer, node->body, ret) ) 
	{
		kfree (node->body);
		printk ( KERN_INFO " copy_to_user() has some problems! node->size=%d\n", node->size);
		return 0;
	}

	PRINTKD("\nThis is intf8_read2.\n");
	//printk("\nThis is intf8_read4.\n");
	list_del (ptr);
	kfree (node->body);
	node->body = NULL;
	kfree (node);

	//printk("\nusb_read: ret=%d\n", ret);
	return ret;
}

static ssize_t
usbc_interface8_write (struct file *pFile, const char *pUserBuffer,
		       size_t stCount, loff_t * pPos)
{
	msg_node_t *node;

	if (stCount == 0)
		return 0;

	PRINTKD("\nstCount=%d\n", stCount);
	PRINTKD("\nThis is intf8_write1.\n");

	node = (msg_node_t *) kmalloc (sizeof (msg_node_t), GFP_KERNEL);
	if (!node)
	{
		printk( KERN_INFO "kmalloc(%d) out of memory!\n", sizeof (msg_node_t));
		return -EFAULT;
	}
	
	node->body = (__u8 *) kmalloc (stCount, GFP_KERNEL);
	if (!(node->body))
	{
		printk( KERN_INFO " kmalloc(%d) out of memory!\n", stCount);
		return -EFAULT;
	}
	PRINTKD("\nThis is intf8_write2.\n");
	if ( copy_from_user (node->body, pUserBuffer, stCount) )
	{
		kfree(node->body);
		printk( KERN_INFO "copy_from_user() failed.\n" );
		return -EFAULT;
	}

	PRINTKD("\nThis is intf8_write3.\n");
	node->size = stCount;		/* Default, tCount==node->size */
	list_add_tail (&node->list, &intf8.in_msg_list);
	intf8.list_len++;

	while(1){
		if(specific_times8)
		{
			specific_times8--;
			if(polling_times8)
                        {
                            polling_message(&intf8, stCount);
			    	  polling_times8--;
                        }
                        else
                            polling_ready8++;
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

/* *
 * pxa_usb_send - called to transmit an urb
 * @buf: buffer need to be transfered
 * @len: buffer length
 *
 */
static int pxa_usb_send(char *buf, int len, __u8 ENDPOINT)
{	
	struct usb_device_instance *device = pst_dev;
	struct urb *urb;

	//printk("\npxa_usb_send: ENDPOINT=%d\n", ENDPOINT);
	//printk("\npxa_usb_send: len=%d\n", len);
	if (!(urb = usbd_alloc_urb (device, ENDPOINT, len))) {
		printk("\nurb alloc failed len: %d\n", len);
		return -ENOMEM;
	}
	
	//printk("\npxa_usb_send alloc urb successfully!\n");
	
	urb->actual_length = len;
	memcpy(urb->buffer, buf, len);//copy data to urb.
	if (usbd_send_urb (urb)) {
		kfree(urb->buffer);
		kfree(urb);
		printk("\nusbd_send_urb failed\n");
		return -ECOMM;
	}
	
	//tx_pending = 1;
	//printk("\npxa_usb_send: send urb successfully!\n");
	return 0;
}

#define BP_LOG_MAX_SIZE		2048
static int
usbc_interface6d_open (struct inode *pInode, struct file *pFile)
{
	usbi2_ref_count++;
	MOD_INC_USE_COUNT;
	return 0;
}

static int normal_write( const char *buffer, int stCount)
{
	u8 *data;

	data = (u8 *) kmalloc (stCount, GFP_KERNEL);
	if (!data)
	{
		printk ( KERN_INFO "kmalloc(%d) out of memory!\n", stCount);
		return -EFAULT;
	}
	if ( copy_from_user (data, buffer, stCount) )
	{
		kfree(data);
		printk ( KERN_INFO "copy_from_user() failed. stCount=%d\n", stCount);
		return 0;
	}
	
	if ( pxa_usb_send(data, stCount, DATA_LOG_IN_ENDPOINT)<0 )
	{
		printk ( KERN_INFO "pxa_usb_send() failed!\n");
		return 0;
	}

	kfree(data);
	udelay(500);
	//printk("\nBP_Data_write: write data successfully.\n");
	return stCount;
}

static ssize_t
usbc_interface6d_write (struct file *pFile, const char *pUserBuffer,
		       size_t stCount, loff_t * pPos)
{	
	int last_packet_len = 1, count = 0, count1 = 0;
	
	if( !cable_connected )
	{
		return -2;
	}
	
	if(stCount <= 0 || stCount > BP_LOG_MAX_SIZE )
	{
		return 0;
	}
      
        //printk("\nhere is write_6d_1!\n");	
	if(stCount%64)
	{
        	//printk("\nhere is write_6d_2!\n");	
		return normal_write(pUserBuffer, stCount);
		}else {
        		//printk("\nhere is write_6d_3!\n");	
			stCount--;
			count = normal_write(pUserBuffer, stCount);
			count1 = normal_write(pUserBuffer+last_packet_len, last_packet_len);
			return (count+count1);
		}
}

static int
usbc_interface6d_release (struct inode *pInode, struct file *pFile)
{
	usbi2_ref_count--;
	MOD_DEC_USE_COUNT;
	return 0;
}

/* Intialize and destroy pst-dev interfaces ************************************************** */

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

/* *
 * pst_dev_create - create an interface
 *
 */
int pst_dev_create (struct usb_device_instance *device)
{
	int rc;
	pst_dev = device;
	
	/* register device driver */
	rc = misc_register (&usbc_misc_device0);
	PRINTKD("\nRegister device0(data cmd)\n");
	if (rc != 0)
	{
		printk (KERN_INFO "Couldn't register misc device0(Data Cmd).\n");
		return -EBUSY;
	}

	rc = misc_register (&usbc_misc_device1);
	PRINTKD("\nRegister device1(test cmd)\n");
	if (rc != 0)
	{
		printk (KERN_INFO "Couldn't register misc device1(Test Cmd).\n");
		goto dev1_register_error;
	}

	rc = misc_register (&usbc_misc_device2);
	PRINTKD("\nRegister device2(data log)\n");
	if (rc != 0)
	{
		printk (KERN_INFO "Couldn't register misc device1(Data Log).\n");
		goto dev1_register_error;
	}

	printk (KERN_INFO "USB Function Character Driver Interface"
		" -%s, (C) 2002, Motorola Corporation.\n", VERSION);

	return 0;

dev1_register_error:
	misc_deregister (&usbc_misc_device0);

	return -EBUSY;

}

/* *
 * pst_dev_destroy - destroy an interface
 *
 */
void pst_dev_destroy (void)
{
        pst_dev = NULL;

	misc_deregister (&usbc_misc_device0);
	misc_deregister (&usbc_misc_device1);
	misc_deregister (&usbc_misc_device2);
}


/* Called when a USB Device is created or destroyed  ***************************************** */

// XXX this should get passed a device structure, not bus

/* Called to handle USB Events  ************************************************************** */

/**
 * pst_event - process a device event
 * @device: usb device 
 * @event: the event that happened
 *
 * Called by the usb device core layer to respond to various USB events.
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
 
void pst_event (struct usb_device_instance *device, usb_device_event_t event, int data)
{
	int ret;
	int i;
	int device_index,dev_index;//interface
	struct usb_ptf_private * func_priv;
		
	switch (event) {
	case DEVICE_CONFIGURED:
		cable_connected = 1;
		ptf_cable_connected=1;
		wake_up_interruptible(&ptf_write_queue);	
		PRINTKD("\nPST USB Device is configured.\n");
		break;	

	case DEVICE_CREATE:	// a bus interface driver has created a usb device
		PRINTKD("\nPST Misc Device is created.\n");
		ret=pst_dev_create (device);
		if(ret !=0){
				PRINTKD("Create PST Device Failed!\n");
				return;
			}


//enter ptf and ap log part
		if(0>ptf_private_create(device,PTF_OUT_ENDPOINT,ptf_xmit_data,PTF_OUT_PKTSIZE)){
		  printk("can not create private for ptf\n");
		  return; 
		}

		if(0>ptf_private_create(device,AP_DATA_LOG_OUT_ENDPOINT,ap_log_xmit_data,AP_DATA_LOG_OUT_PKTSIZE)){
		  printk("can not create private for ap log\n");
		  return; 
		}

		(device->active_function_instance)->privdata=ptf_private_array;
		break;
	case DEVICE_CABLE_DISCONNECTED:
		cable_connected = 0;
		ptf_cable_connected=0;
		break;

	case DEVICE_DESTROY:	// the bus interface driver is unloading
		PRINTKD("\nPST Misc Device is destroyed.\n");
		pst_dev_destroy ();

//enter ptf and ap logger part
			
		if (ptf_private_array==(device->active_function_instance)->privdata){
		  ptf_private_destroy();

		  return;
		}
		
		break;

        default:
		break;
	}
	return;
}

void specific_message( interface_t *interface, struct urb *urb )
{
	msg_node_t *node;
	unsigned int len;
	struct usb_device_request *req;
	req = &urb->device_request;

	node = (msg_node_t*)kmalloc( sizeof(msg_node_t), GFP_ATOMIC );
	if ( !node ) {
		printk( "specific_messages: (%d) out of memory!\n", sizeof(msg_node_t) );
		return;
	}

	len = urb->actual_length;
	node->size = len; 
	node->body = (__u8*)kmalloc( sizeof(__u8)*len, GFP_ATOMIC );
	if ( !(node->body) ) {
		kfree(node);
		printk( "specific_messages: (%d) out of memory!\n", sizeof(__u8)*len );
		return;
	}
	memcpy( node->body, urb->buffer, len );
	list_add_tail( &node->list, &interface->out_msg_list);

	if(req->wIndex == 6)
			{
      				specific_times6++;
				wake_up_interruptible(&wq_read6);
			}else if(req->wIndex == 8)
				{
      					specific_times8++;
					wake_up_interruptible(&wq_read8);
				}

	PRINTKD( "specific_messages: return successfully!\n" );
	printk( "specific_messages: return successfully!\n" );
	return;
}


/* 
 * invoked by process_interface8_request and process_interface6_request
 *
 * process the polling request for interface
 *
 */

void polling_return( struct urb *urb )
{
	struct usb_device_request *req;
	req = &urb->device_request;
	
	polling_len = req->wLength;
	
	PRINTKD("\npolling_return..., polling_times6=%d polling_ready8=%d specific_times6=%d\n", polling_times6,polling_ready8,specific_times6);
	PRINTKD("\npolling_return..., polling_times8=%d polling_ready8=%d specific_times8=%d\n", polling_times8,polling_ready8,specific_times8);
	if(req->wIndex == 6)
        {
            if( (specific_times6 == 0) || (polling_ready6 > 0) )
            {
                polling_message_return( urb, &intf6, polling_len );
		if(polling_ready6 > 0) polling_ready6--;
            }
            else {
            	polling_times6++;
		urb->actual_length = -1;
	    }
        }
	else if(req->wIndex == 8)
        {
            if( (specific_times8 == 0) || (polling_ready8 > 0) )
            {
                polling_message_return( urb, &intf8, polling_len );
                if(polling_ready8 > 0) polling_ready8--;
            }
            else{
            	polling_times8++;
		urb->actual_length = -1;
	    }
        }
	return;
}
void polling_message( interface_t *interface, int polling_data_len )
{
	polling_data_t pd;
	struct list_head *ptr=NULL;
	msg_node_t* entry;
	int i;
	struct urb *urb;

	memset( (void*)&pd, 0, sizeof(polling_data_t) );
  	PRINTKD("\nThis is polling1\n");

	if ( interface->list_len>3 ) {
		pd.status = 0x02;
		pd.numMessages = 3;
		} else {
			pd.status = 0x00;
			pd.numMessages = interface->list_len;
	}
	
      ptr = interface->in_msg_list.next;
	for ( i=0; i<pd.numMessages; i++ ) {
		entry = list_entry( ptr, msg_node_t, list );
		pd.msgsiz[i] = make_word_b(entry->size);
		ptr = ptr->next;
	}

	//need to build a urb to send to host.
  	PRINTKD("\nThis is polling2\n");

	if (!(urb = usbd_alloc_urb(pst_dev, TEST_CMD_ENDPOINT, polling_data_len))) {
		PRINTKD ("urb alloc failed len: %d", polling_data_len);
		return -ENOMEM;
	}

	/* write back to HOST */
	send_urb_to_host( &pd, urb, sizeof(__u8)*2+sizeof(__u16)*pd.numMessages );

	PRINTKD("polling messages: send out response now!\n");
	return;
}

void polling_message_return( struct urb *urb, interface_t *interface, int polling_data_len )
{
	polling_data_t pd;
	struct list_head *ptr=NULL;
	msg_node_t* entry;
	int i,len;
	
	memset( (void*)&pd, 0, sizeof(polling_data_t) );
  	PRINTKD("\nThis is polling_message_return1\n");

	if ( interface->list_len>3 ) {
		pd.status = 0x02;
		pd.numMessages = 3;
		} else {
			pd.status = 0x00;
			pd.numMessages = interface->list_len;
	}
	
      ptr = interface->in_msg_list.next;
	for ( i=0; i<pd.numMessages; i++ ) {
		entry = list_entry( ptr, msg_node_t, list );
		pd.msgsiz[i] = make_word_b(entry->size);
		ptr = ptr->next;
	}

	/* write back to HOST */
	len = sizeof(__u8)*2+sizeof(__u16)*pd.numMessages;
	urb->actual_length = len;
	memcpy(urb->buffer, &pd, len);//data
	
	PRINTKD("polling messages return: send out response now!\n");
	return;
}
/* 
 * invoked by process_interface6_request and process_interface8_request
 *
 * process the queue request for interface
 *
 */
void queue_message( interface_t *interface, struct urb *urb )
{
	struct list_head* ptr=NULL,*tmp;
	msg_node_t* entry=NULL;
	queued_data_t *data=NULL;
	int numReq,i,numMsgBytes=0,numQueuedBytes=0,data_size=0;
	msg_t* msg_array;
	struct usb_device_request *req;
	
	req = &urb->device_request;
	numReq = MIN( interface->list_len, req->wValue );
	PRINTKD("\nThe request wanted to be returned to host is %d.\n", numReq );

	ptr = interface->in_msg_list.next;
	for (i=0; i<numReq; i++ ) {
		entry = list_entry( ptr, msg_node_t, list );
		numMsgBytes += entry->size;
		ptr = ptr->next;
	}

	for (i=0; i<(interface->list_len-numReq); i++ ) {
		entry = list_entry( ptr, msg_node_t, list );
		numQueuedBytes += entry->size;
		ptr = ptr->next;
	}

	PRINTKD("\nThis is queue1\n");
	data_size = sizeof(queued_data_t)+sizeof(__u16)*numReq+numMsgBytes;
	PRINTKD("\ndata_size= %d\n", data_size);
	data = (queued_data_t*) kmalloc( data_size, GFP_ATOMIC );

	if ( !data ) {
		printk( "\nqueue_message: (%d) out of memory!\n", data_size );
		return;
	}
	data->numMsgs = numReq;
	data->numQueuedMsgs = interface->list_len - numReq;
	data->numQueuedBytes = make_word_b(numQueuedBytes);
	msg_array = (msg_t*) (data+1);
	PRINTKD("\ndata= %x\n", data);
	PRINTKD("\nmsg_array= %x\n", &msg_array);

	ptr = interface->in_msg_list.next;
	for ( i=0; i<numReq; i++ ) {
		entry = list_entry(ptr,  msg_node_t, list );
		msg_array->size = make_word_b(entry->size);
		PRINTKD("\nentry->size= %d\n", entry->size);
		PRINTKD("\nentry->body= %x\n", entry->body);
		PRINTKD("\n((char*)(msg_array))+2= %x\n", ((char*)msg_array)+2);
		memcpy(((char*)msg_array)+2, entry->body, entry->size );
		msg_array = (msg_t*)(((char*)msg_array)+2+entry->size);

		tmp = ptr;
		ptr = ptr->next;

		interface->list_len--;
		list_del( tmp );
		kfree( entry->body );
		kfree( entry );
		PRINTKD("\nThis is queue4\n");
	}
		
	urb->actual_length = MIN(data_size, req->wLength);
	memcpy(urb->buffer, data, data_size);
	
	kfree( data );
	PRINTKD("queue_messages: send out response now.\n");
	return;
}

/* 
 * process_interface0_request 
 * process the request for interface6
 */
/* NOTE: process_interface0_request() is called in interrupt context, so we can't use wait queue here. */
void process_interface6_request( struct urb *urb )
{
	struct usb_device_request *req;
	req = &urb->device_request;
	
  switch ( req->bRequest ) {
  case 0: /* Polling */
    if ( DATA_DIRECTION(req->bmRequestType) == DATA_IN ) {
      polling_return( urb );
    } else {
      PRINTKD( "process_interface6_request() Invalid polling direction.\n" );
    }
    break;
  case 1: /* Queued Messages */
    if ( DATA_DIRECTION(req->bmRequestType) == DATA_IN ) {
      queue_message( &intf6, urb );
    } else {
      PRINTKD( "process_interface6_request() Invalid queue direction.\n" );
    }
    break;
  case 2: /* Interface specific request */
    if ( DATA_DIRECTION(req->bmRequestType) == DATA_OUT ) {
	specific_message( &intf6, urb );
    } 
    break;
  case 3: /* Security - TBD */
    PRINTKD( "TBD\n" );
    break;
  default:
    break;
  }
  return;
}

/* 
 * NOTE: process_interface8_request() is called in interrupt context, so we can't use wait queue here.
 *                             
 */
void process_interface8_request( struct urb *urb )
{
	struct usb_device_request *req;
	req = &urb->device_request;
	     	
  switch ( req->bRequest ) {
  case 0: /* Polling */
	PRINTKD("This is Polling case.\n");
    if ( DATA_DIRECTION(req->bmRequestType) == DATA_IN ) {
    	polling_return(urb);
    } else {
      PRINTKD( "process_interface8_request() Invalid polling direction.\n" );
    }
    break;

  case 1: /* Queued Messages */
	  PRINTKD("This is Queued Messages case.\n");
    if ( DATA_DIRECTION(req->bmRequestType) == DATA_IN ) {
      queue_message( &intf8, urb );
    } else {
      PRINTKD( "process_interface8_request() Invalid queued direction.\n" );
    }
    break;

  case 2: /* Interface specific request */
	PRINTKD("This is Test Command case.\n");
    if ( DATA_DIRECTION(req->bmRequestType) == DATA_OUT ) {
	specific_message( &intf8, urb );
    } else {
      PRINTKD( "process_interface8_request() Invalid interface command direction.\n" );
    }
    break;

  case 3: /* Security - TBD */
    PRINTKD( "Security command - TBD.\n" );
    break;

  default:
    break;
  }
  return;
}

/* We will process some interface specific request here. */
void ep0_process_vendor_request( struct urb *urb )
{
	int enable,count;
	struct usb_device_request *req;
	__u8 num_cfg = USB_NUM_OF_CFG_SETS;

	req = &urb->device_request;
	
#if PST_DEBUG
      {
        unsigned char * pdb = (unsigned char *) &req;
        PRINTKD( "VENDOR REQUEST:%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X ",
    	     pdb[0], pdb[1], pdb[2], pdb[3], pdb[4], pdb[5], pdb[6], pdb[7]
    	     );
      }
#endif
  /* 
   * Do we need to do any thing else? We only throw it to upper driver and let 
   * them to decide what to do 
   */ 
	enable = 1;
	count = 100;
	if ( (req->bmRequestType&0x0F) == 0x00 ) 
	{
		switch ( req->bRequest ) {
		  case 0:
		  	//This branch will Report Number of configurations.
		    PRINTKD( "DEVICE 0\n");
		    if (((req->bmRequestType & 0x80) != 0)
	             && (req->wValue == 0)
	             && (req->wIndex == 0)
	             && (req->wLength == 2))
	         {
	             send_urb_to_host( &num_cfg, urb, 1 );
	         }
		    break;
			
		  case 1:
		  	//This branch will Change to alternate configuration set.
		    PRINTKD( "DEVICE 1\n");
		    if (((req->bmRequestType & 0x80) == 0)
	             && (req->wIndex <= USB_NUM_OF_CFG_SETS)
	             && (req->wValue == 0)
	             && (req->wLength == 0) )
	         {
			send_urb_to_host( NULL, urb, 0 );
		/*	do{
				count--;
				udelay(1);
				}while(count);*/
		//	usb_reset_for_cfg8(enable);
			
		   }
		    break;
			
		  case 2:
		  	//This branch will List descriptors for alternate configuration set
		    PRINTKD( "DEVICE 2\n");
		    break;
			
		  case 3:
		  	//This branch will Quick-change to alternate configuration set
		    PRINTKD( "DEVICE 3\n");
		    if (((req->bmRequestType & 0x80) == 0)
	             && (req->wIndex <= USB_NUM_OF_CFG_SETS)
	             && (req->wValue == 0)
	             && (req->wLength == 0) )
	         {
	         	send_urb_to_host( NULL, urb, 0 );;
		   }
		    break;
			
		  default:
		    printk( "unknown device.\n" );
		    break;
		  }
	}
	else if ( (req->bmRequestType&0x0F) == 0x01 ) 
	{
		  switch ( req->wIndex ) {
		  case 6:
		    PRINTKD( "INTERFACE 6\n");
		    if ( intf6.enable ) 
		      process_interface6_request( urb );
		    else /* This branch is not only for test. It is necessary. */
		      PRINTKD("interface6 is not enabled.");

		    break;
		  case 8:
		    PRINTKD( "INTERFACE 8\n");
		    if ( intf8.enable ) 
		      process_interface8_request( urb );
		    else /* This branch is not only for test. It is necessary. */
		      PRINTKD("interface8 is not enabled.");

		    break;
		  default:
		    PRINTKD( "unknown interface.\n" );
		    break;
		  }

		  PRINTKD("process vendor command OK!\n");
		}
	return;
}

/**
 * pst_recv_setup - called with a control URB 
 * @urb - pointer to struct urb
 *
 * Check if this is a setup packet, process the device request, put results
 * back into the urb and return zero or non-zero to indicate success (DATA)
 * or failure (STALL).
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
int pst_recv_setup (struct urb *urb)
{
	struct usb_device_request *request;
	request = &urb->device_request;

	// handle USB Vendor Request (c.f. USB Spec table 9-2)
	if ((request->bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_VENDOR)
		ep0_process_vendor_request( urb );

	return 0;		
}

/*
 * send_urb()
 * data == data to send
 * urb == USB Data structure
 * len == bytes we actually have
 *
 */
static void send_urb_to_host( void * data, struct urb *urb, int len )
{
	PRINTKD( "send_urb_to_host: bytes actual=%d\n", len);

	urb->actual_length = len;
	memcpy(urb->buffer, data, len);

	PRINTKD("send out urb now! %p %p len=%d\n", urb, urb->buffer, urb->actual_length );
	usbd_send_urb(urb);
	UDCCS0 = UDCCS0_IPR;
	
	return;
}


/**
 * pst_recv_urb - called with a received URB 
 * @urb - pointer to struct urb
 *
 * Return non-zero if we failed and urb is still valid (not disposed)
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
int pst_recv_urb (struct urb *urb)
{
	return 0;
}

/**
 * pst_urb_sent - called to indicate URB transmit finished
 * @urb: pointer to struct urb
 * @rc: result
 *
 * The usb device core layer will use this to let us know when an URB has
 * been finished with.
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 *
 */
int pst_urb_sent (struct urb *urb, int rc)
{
	urb->privdata = NULL;
	usbd_dealloc_urb (urb);

	//printk("\nhere is pst_urb_sent: bplog=%d, tx_pending=%d\n", bplog, tx_pending);
	return 0;
}

static void pst_function_init( struct usb_bus_instance *bus,
				struct usb_device_instance* device,
				struct usb_function_instance* function_driver )
{
	printk(KERN_INFO"INIT**************************\n");
	//pst_dev = device;
}

static void pst_function_exit( struct usb_device_instance *device )
{
	printk(KERN_INFO"CLOSING************************\n");
	//pst_dev = NULL;
}
/***for PST***/
static struct usb_function_operations pst_function_ops = {
	event_irq:pst_event,
	//recv_urb:pst_recv_urb,
	recv_urb:ptf_recv_urb,
	recv_setup:pst_recv_setup,
	urb_sent:pst_urb_sent,
	function_init:pst_function_init,
	function_exit:pst_function_exit,
};

static struct usb_function_driver function_driver = {
	name:"Mot PST",
	ops:&pst_function_ops,
	device_description:&pst_device_description,
	configurations:sizeof (pst_description) / sizeof (struct usb_configuration_description),
	configuration_description:pst_description,
	this_module:THIS_MODULE,
};

/* Module init and exit *******************************************************************/
/*
 * pst_modinit - module init
 *
 */
int pst_modinit (void)
{
	PRINTKD ("init pst_mod\n");
		/*init ptf char device array */
	if (0>ptfproto_modinit ("", PTF_MAX_DEV_NUMBER)) {
		return -EINVAL;
	}
	// register us with the usb device support layer
	if (usbd_register_function (&function_driver)) {
		PRINTKD ("usbd_register_function failed\n");
		ptfproto_modexit();
		return -EINVAL;
	}

	return 0;
}

/*
 * function_exit - module cleanup
 *
 */
void pst_modexit (void)
{
	PRINTKD ("exit pst_mod\n");
	// de-register us with the usb device support layer
	usbd_deregister_function (&function_driver);
	ptfproto_modexit();
}

//module_init (pst_modinit);
//module_exit (pst_modexit);
EXPORT_SYMBOL(pst_modinit);
EXPORT_SYMBOL(pst_modexit);
