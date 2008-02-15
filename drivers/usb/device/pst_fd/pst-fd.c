/*
 * linux/drivers/usbd/pst_fd/pst-fd.c - Motorola PST USB function driver
 *
 *Motorola PST USB Function Driver is Create by Levis. (Levis@motorola.com) 01/07/03
 */


#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("Levis@Motorola.com");
MODULE_DESCRIPTION ("PST USB Function Driver");

USBD_MODULE_INFO ("pst_usb 0.9");

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

#define VERSION "0.90"

#define USBC_MINOR0 17
#define USBC_MINOR1 18
#define USBC_MINOR2 19

// 1 == lots of trace noise,  0 = only "important' stuff
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

#define BMAXPOWER                               1

/*
 * setup some default values for pktsizes and endpoint addresses.
 */
#define CONFIG_TESTCOMMAND_BPDATALOG	1
#define TEST_CMD_PKTSIZE             	8
#define DATA_LOG_IN_PKTSIZE          64
#define PTF_OUT_PKTSIZE               64
#define PTF_IN_PKTSIZE           	64
#define AP_DATA_LOG_OUT_PKTSIZE   	64
#define AP_DATA_LOG_IN_PKTSIZE       	64
#define TEST_CMD_ENDPOINT            	0
#define DATA_LOG_IN_ENDPOINT       	1
#define PTF_OUT_ENDPOINT       		7
#define PTF_IN_ENDPOINT          	6
#define AP_DATA_LOG_OUT_ENDPOINT 	12
#define AP_DTAT_LOG_IN_ENDPOINT    	11

/* Module Parameters ************************************************************************* */
struct usb_function_driver *function_driver;
interface_t intf6,intf8;
int intf_index;
static int usbi0_ref_count=0, usbi1_ref_count=0, usbi2_ref_count=0;
int polling_times6=0,polling_times8=0;
int specific_times6=0,specific_times8=0;
int polling_ready6, polling_ready8;
int polling_len;
struct urb *urb_polling;
int usb_ep0_return=0;
static struct usb_device_instance * pst_dev_ep0=NULL;
static struct usb_function_instance * pst_func_ep0=NULL;
static struct usb_device_instance * pst_dev_ep1=NULL;
static struct usb_function_instance * pst_func_ep1=NULL;
static int tx_pending = 0;

DECLARE_WAIT_QUEUE_HEAD(wq_read6);
DECLARE_WAIT_QUEUE_HEAD(wq_read8);
DECLARE_WAIT_QUEUE_HEAD(wq_write6d);

struct usb_pst_private {
	int flags;
	struct usb_device_instance *device;
	unsigned int maxtransfer;
	int index;
};

/* USB Configuration Description ************************************************************* */
/* USB Interface and Endpoint Configuration ********************** */
/* BP Data Log Interface Alternate 1 endpoint(EP1--BULK IN).
 */
static __initdata struct usb_endpoint_description datalog_endpoints[] = {
      {	
	      bEndpointAddress:DATA_LOG_IN_ENDPOINT,
	      bmAttributes:BULK,
	      wMaxPacketSize:DATA_LOG_IN_PKTSIZE,
	      bInterval:0,
	      direction:IN,
            transferSize:64,},
};

/* testcmd_datalog Interface Alternate description(s)
 */
static __initdata struct usb_alternate_description testcmd_alternate_descriptions[] = {
	{   	iInterface:"Motorola Test Command",
	      bAlternateSetting:0,
            endpoints:0,//sizeof (testcmd_endpoints) / sizeof (struct usb_endpoint_description),
            endpoint_list:NULL/*testcmd_endpoints*/,},              
};

static __initdata struct usb_alternate_description datalog_alternate_descriptions[] = {
	{	iInterface:"Motorola MCU Data Logger",
		bAlternateSetting:0,
             endpoints:sizeof (datalog_endpoints) / sizeof (struct usb_endpoint_description),
             endpoint_list:datalog_endpoints,},
};

/* Interface description(s)
 */

static __initdata struct usb_interface_description testcmd_datalog_interfaces[] = {
	{
                iInterface:"Motorola MCU Data Logger",
                bInterfaceClass:VENDOR,
                bInterfaceSubClass:0x02,
                bInterfaceProtocol:0xff,
                bInterfaceNumber:6,
                alternates:sizeof (datalog_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:datalog_alternate_descriptions,},        
	{
                iInterface:"Motorola Test Command",
                bInterfaceClass:VENDOR,
                bInterfaceSubClass:0x02,
                bInterfaceProtocol:0xff,
                bInterfaceNumber:8,
                alternates:sizeof (testcmd_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:testcmd_alternate_descriptions,},               
};

/***USB Interface and Endpoint Configuration**********************************************/

/***USB Configuration ***/

/* Configuration description(s)
 */
struct __initdata usb_configuration_description pst_description[] = {
#ifdef CONFIG_TESTCOMMAND_BPDATALOG
	{
                iConfiguration:"MCU Logging+test cmd+accy",//"MCU Logging+Test Cmd",
                bmAttributes:0x40,//BMATTRIBUTE,
                bMaxPower:BMAXPOWER,
                interfaces:sizeof (testcmd_datalog_interfaces) / sizeof (struct usb_interface_description),
                interface_list:testcmd_datalog_interfaces,},
#endif	/* CONFIG_TESTCOMMAND_BPDATALOG */
};

/***usb modem device, config, interface, endpoint descriptions***/
static __initdata struct usb_endpoint_description modem_comm_endpoints[] = {
      {	bEndpointAddress:5,
	      bmAttributes:INTERRUPT,
	      wMaxPacketSize:16,
	      bInterval:0x0A,
	      direction:IN,
            transferSize:16,},//?      
};
/* Communication Interface Class descriptions
 */
static struct usb_class_description modem_comm_class[] = {
      {USB_ST_HEADER, 0, {header: {bcdCDC:CLASS_BCD_VERSION,}}},

	{USB_ST_CMF, 0, {call_management: {bmCapabilities:0x03,bDataInterface:0x01,}}},

      {USB_ST_UF, 1, {union_function: {bMasterInterface: 0, bSlaveInterface:{1},}}},
      {USB_ST_ACMF, 0, {abstract_control: {bmCapabilities: 0x02,}}},      
};

static __initdata struct usb_endpoint_description modem_data_endpoints[] = {
      {	bEndpointAddress:1,
	      bmAttributes:BULK,
	      wMaxPacketSize:64,
	      bInterval:0,
	      direction:IN,
            transferSize:64,},//?
      {	bEndpointAddress:2,
	      bmAttributes:BULK,
	      wMaxPacketSize:64,
	      bInterval:0,
	      direction:OUT,
            transferSize:64,},//?
};
static __initdata struct usb_alternate_description modem_comm_alternate_descriptions[] = {
	{   	iInterface:"Motorola Communication Interface",
	      bAlternateSetting:0,
            classes:sizeof (modem_comm_class) / sizeof (struct usb_class_description),
            class_list:modem_comm_class,
            endpoints:sizeof (modem_comm_endpoints) / sizeof (struct usb_endpoint_description),
            endpoint_list:modem_comm_endpoints,},                    
};

static __initdata struct usb_alternate_description modem_data_alternate_descriptions[] = {
	{	iInterface:"Logarithmic Streaming Audio Speaker Interface",
		bAlternateSetting:0,
             endpoints:sizeof (modem_data_endpoints) / sizeof (struct usb_endpoint_description),
             endpoint_list:modem_data_endpoints,},
};
static __initdata struct usb_interface_description modem_interfaces[] = {
	{
                iInterface:"Motorola Communication Interface",
                bInterfaceClass:COMMUNICATIONS_INTERFACE_CLASS,
                bInterfaceSubClass:COMMUNICATIONS_ACM_SUBCLASS,
                bInterfaceProtocol:0x01,
                bInterfaceNumber:0,
                alternates:sizeof (modem_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:modem_comm_alternate_descriptions,},        
	{
                iInterface:"Logarithmic Streaming Audio Speaker Interface",
                bInterfaceClass:DATA_INTERFACE_CLASS,
                bInterfaceSubClass:COMMUNICATIONS_NO_SUBCLASS,
                bInterfaceProtocol:COMMUNICATIONS_NO_PROTOCOL,
                bInterfaceNumber:1,
                alternates:sizeof (modem_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:modem_data_alternate_descriptions,},               
};

struct __initdata usb_configuration_description modem_description[] = {
	{
                iConfiguration:"Motorola Communication Class",//usb modem
                bmAttributes:0x40,//BMATTRIBUTE,
                bMaxPower:BMAXPOWER,
                interfaces:sizeof (modem_interfaces) / sizeof (struct usb_interface_description),
                interface_list:modem_interfaces,},
};
/********************************************************************/

/* Device Description
 */
 /***PST USB Device Descriptor***/
struct __initdata usb_device_description pst_device_description = {
	bDeviceClass:0,//VENDOR,
	bDeviceSubClass:0,	
	bDeviceProtocol:0,	// XXX
	idVendor:make_word_c(0x22B8),	/* vendor ID defined *///CONFIG_USBD_VENDORID
	idProduct:make_word_c(0x3801), /* product ID*///CONFIG_USBD_PRODUCTID
	iManufacturer:"Motorola Inc.",
	iProduct: "Motorola Ezx Phone",
	iSerialNumber:0,
};


 /***CDC ACM USB Modem Device Descriptor***/ //need to be defined???????
struct __initdata usb_device_description modem_device_description = {
	bDeviceClass:COMMUNICATIONS_DEVICE_CLASS,
	bDeviceSubClass:0,	
	bDeviceProtocol:0,	// XXX
	idVendor:make_word_c(0x22B8),	/* vendor ID defined */
	idProduct:make_word_c(0x3802), /* product ID*/
	iManufacturer:"Motorola Inc.",
	iProduct: "Motorola Ezx Phone",
	iSerialNumber:0,
};

/***USB Configuration End**************************************************/

/* *
 * pxa_usb_send - called to transmit an urb
 * @buf: buffer need to be transfered
 * @len: buffer length
 *
 */
static int pxa_usb_send(char *buf, int len, __u8 ENDPOINT)
{	
	struct urb *urb;

	tx_pending = 1;

	PRINTKD("\nENDPOINT=%d\n", ENDPOINT);
	PRINTKD("\nlen=%d\n", len);
	PRINTKD("\npst_dev_ep1=%x\n", &pst_dev_ep1);
	PRINTKD("\npst_func_ep1=%x\n", &pst_func_ep1);
	if (!(urb = usbd_alloc_urb (pst_dev_ep1, pst_dev_ep1->function_instance_array, ENDPOINT, len))) {
		PRINTKD ("urb alloc failed len: %d", len);
		return -ENOMEM;
	}
	
	PRINTKD("\npxa_usb_send alloc urb successfully!\n");
	
	urb->actual_length = len;
	memcpy(urb->buffer, buf, len);//copy data to urb.
	if (usbd_send_urb (urb)) {
		PRINTKD ("usbd_send_urb failed");
		kfree(urb->buffer);
		kfree(urb);
		return -ECOMM;
	}
	
	PRINTKD("\npxa_usb_send: send urb successfully!\n");
	return 0;
}

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
	if (stCount < ret)
		return -EFAULT;		
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
		return -EFAULT;		
	if ( copy_to_user (pUserBuffer, node->body, ret) ) 
	{
		kfree (node->body);
		printk ( KERN_INFO " copy_to_user() has some problems! node->size=%d\n", node->size);
		return 0;
	}

	PRINTKD("\nThis is intf8_read2.\n");
	list_del (ptr);
	kfree (node->body);
	node->body = NULL;
	kfree (node);

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

static int
usbc_interface6d_open (struct inode *pInode, struct file *pFile)
{
	usbi2_ref_count++;
	MOD_INC_USE_COUNT;
	return 0;
}

static ssize_t
usbc_interface6d_write (struct file *pFile, const char *pUserBuffer,
		       size_t stCount, loff_t * pPos)
{
	u8 *data;

	PRINTKD("\nstCount=%d\n", stCount);
	PRINTKD("\nThis is intf6d_write1.\n");
	
	data = (u8 *) kmalloc (stCount, GFP_KERNEL);
	if (!data)
	{
		printk ( KERN_INFO "kmalloc(%d) out of memory!\n", stCount);
		return -EFAULT;
	}
	PRINTKD("\nThis is intf6d_write2.\n");

	if ( copy_from_user (data, pUserBuffer, stCount) )
	{
		kfree(data);
		printk ( KERN_INFO "copy_from_user() failed. stCount=%d\n", stCount);
		return 0;
	}

	PRINTKD("\nThis is intf6d_write3.\n");

	while(tx_pending == 1)
	{
		interruptible_sleep_on(&wq_write6d);
		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	if ( pxa_usb_send(data, stCount, DATA_LOG_IN_ENDPOINT)<0 )
	{
		printk ( KERN_INFO "pxa_usb_send() failed!\n");
		return 0;
	}

	kfree(data);
	PRINTKD("\nBP_Data_write: write data successfully.\n");
	
	return stCount;
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
static int pst_dev_create (void)
{
	int rc;
	
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
static void pst_dev_destroy (void)
{
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
	pst_dev_ep1 = device;
	pst_dev_ep0 = device;
	switch (event) {
	case DEVICE_CONFIGURED:
		PRINTKD("\nPST USB Device is configured.\n");
			break;	

	case DEVICE_CREATE:	// a bus interface driver has created a usb device
		PRINTKD("\nPST Misc Device is created.\n");
		ret=pst_dev_create ();
		if(ret !=0){
				PRINTKD("Create PST Device Failed!\n");
				return;
			}
		break;

	case DEVICE_DESTROY:	// the bus interface driver is unloading
		PRINTKD("\nPST Misc Device is destroyed.\n");
		pst_dev_destroy ();
		pst_dev_ep1 = NULL;
		pst_dev_ep0 = NULL;
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
            else
            {
		polling_times6++;
		usb_ep0_return = 3;
            }
        }
	else if(req->wIndex == 8)
        {
            if( (specific_times8 == 0) || (polling_ready8 > 0) )
            {
                polling_message_return( urb, &intf8, polling_len );
                if(polling_ready8 > 0) polling_ready8--;
            }
            else
            {
		polling_times8++;
		usb_ep0_return = 3;
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
	if (!(urb = usbd_alloc_urb (pst_dev_ep0, pst_func_ep0, TEST_CMD_ENDPOINT, polling_data_len))) {
		PRINTKD ("urb alloc failed len: %d", polling_data_len);
		return -ENOMEM;
	}

	/* write back to HOST */
	send_urb_to_host( &pd, urb, sizeof(__u8)*2+sizeof(__u16)*pd.numMessages );
	
  	PRINTKD("\nThis is polling3\n");
  	urb->privdata = NULL;
  	kfree(urb->buffer);
  	kfree(urb);

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
		
	urb->actual_length = data_size;
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
	if ((request->bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_VENDOR) 	{
		ep0_process_vendor_request( urb );
		if (usb_ep0_return == USB_RECV_SETUP_PENDING) 
			{
				usb_ep0_return = USB_RECV_SETUP_DONE;
				return USB_RECV_SETUP_PENDING;
			}
	}

	return USB_RECV_SETUP_DONE;		
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
 * usb_pst_done - transmit done
 * @rc: non-zero indicates failure
 *
 * Call to indicate that a user buffer has been transferred.
 *
 */

static int usb_pst_done (int rc)
{
	if( rc != SEND_FINISHED_OK ){
		PRINTKD("usb device is doing with the urb, rc=%d: .\n", rc);
		return -EINVAL;
		}
		
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
//	urb->privdata = NULL;
//	kfree(urb->buffer);
//	kfree(urb);
		usbd_dealloc_urb (urb);

	// tell user we are done with the urb, it will test for NULL
	if( usb_pst_done (rc) ){
		return -EINVAL;
		}
	
	wake_up_interruptible(&wq_write6d);
	tx_pending = 0;

	return 0;
}

int pst_alloc_urb (struct urb *urb, int len)
{
	void *p;
	
	if((p=kmalloc(len, GFP_ATOMIC)==NULL)){
			return NULL;
			}
			
	memset(p,0,len);
	urb->buffer=p;
	return 0;
}

void test_event ( struct usb_device_instance *device, usb_device_event_t event, int data )
{
	/* NOTHING*/
	printk(KERN_INFO"test_event create!**********************\n");
	printk(KERN_INFO"Now the comein event is %d\n", event );
}

int test_recv_setup( struct urb* urb )
{
	printk(KERN_INFO"test_recv_setup *****************************\n");
	return 0;
}

int test_recv_urb( struct urb* urb )
{
	/* DO NOTHING */
	printk(KERN_INFO"test_recv_urb *****************************\n");
	return 0;
}

int test_urb_sent( struct urb* urb, int rc )
{
	/* DO NOTHING */
	printk(KERN_INFO"test_urb_sent *******************\n");
	return 0;
}

static void test_function_init( struct usb_bus_instance *bus,
				struct usb_device_instance* device,
				struct usb_function_instance* function_driver )
{
	printk(KERN_INFO"****************************\n");
}

static void test_function_exit( struct usb_device_instance *device )
{
	printk(KERN_INFO"CLOSING************************\n");
}

static void pst_function_init( struct usb_bus_instance *bus,
				struct usb_device_instance* device,
				struct usb_function_instance* function_driver )
{
	pst_func_ep1 = function_driver;
	pst_func_ep0 = function_driver;
	printk(KERN_INFO"INIT**************************\n");
}

static void pst_function_exit( struct usb_device_instance *device )
{
	pst_func_ep1 = NULL;
	pst_func_ep0 = NULL;
	printk(KERN_INFO"CLOSING************************\n");
}
#if 0
/***usb gpio setting ***/
int usb_gpio_init(void)
{
	GPSR1 = 0x02000000;	//Set GPIO57 to 1,to set USB transceiver to receive mode
	GPCR1 = 0x01000000;	//Clear GPIO56 to 0, to set USB transceiver to normal mode

	GPDR0 &= 0xfffffdff;  	//Setup GPIO09 Input
	GPDR1 &= 0xfffffffa;		//Setup GPIO32/34 Input
	GPDR1 |= 0x03000080;		//Setup GPIO57/56/39 Output

	GAFR0_L = (GAFR0_L & 0xfff3ffff)|0x00040000; 	//GPIO09 to01
	GAFR1_L = (GAFR1_L & 0xffff3fcc)|0x0000c022;	//GPIO39/34/32 to 11/10/10
	GAFR1_U = (GAFR1_U & 0xfff0ffff)|0x00050000;	//GPIO57/56 to 01/01

    return 0;
}

int stop_usb(void)
{
    return 0;
}
#endif
/***for PST***/
struct usb_function_operations pst_function_ops = {
	event:pst_event,
	recv_urb:pst_recv_urb,
	recv_setup:pst_recv_setup,
	urb_sent:pst_urb_sent,
#if 0
	alloc_urb_data:pst_alloc_urb,
#endif
	function_init:pst_function_init,
	function_exit:pst_function_exit,
};

struct usb_function_driver pst_function_driver = {
	name:"PST DEVICE",
	ops:&pst_function_ops,
	device_description:&pst_device_description,
	configurations:sizeof (pst_description) / sizeof (struct usb_configuration_description),
	configuration_description:pst_description,
	this_module:THIS_MODULE,
};
/***for CDC ACM Modem***/
struct usb_function_operations modem_function_ops = {
	event:test_event,
	recv_urb:test_recv_urb,
	recv_setup:test_recv_setup,
	urb_sent:test_urb_sent,
	function_init:test_function_init,
	function_exit:test_function_exit,
};

struct usb_function_driver modem_function_driver = {
	name:"MODEM DEVICE",
	ops:&modem_function_ops,
	device_description:&modem_device_description,
	configurations:sizeof (modem_description) / sizeof (struct usb_configuration_description),
	configuration_description:modem_description,
	this_module:THIS_MODULE,
};

/* Module init and exit *******************************************************************/
/*
 * pst_modinit - module init
 *
 */
static int __init pst_modinit (void)
{
	PRINTKD ("init pst_mod\n");
	function_driver=&pst_function_driver;
	//function_driver=&modem_function_driver;
	// register us with the usb device support layer
	if (usbd_register_function (function_driver)) {
		PRINTKD ("usbd_register_function failed\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * function_exit - module cleanup
 *
 */
static void __exit pst_modexit (void)
{
	PRINTKD ("exit pst_mod\n");
	// de-register us with the usb device support layer
	usbd_deregister_function (function_driver);

}

module_init (pst_modinit);
module_exit (pst_modexit);
