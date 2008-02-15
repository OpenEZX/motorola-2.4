/*
 * linux/drivers/usbd/network_fd/network.c - Network Function Driver
 *
 * Copyright (c) 2002, 2003 Belcarra
 *
 * By: 
 *      Chris Lynne <cl@belcarra.com>
 *      Stuart Lynne <sl@belcarra.com>
 *      Bruce Balden <balden@belcarra.com>
 *      Ted Powell <ted@belcarra.com>
 *
 * This software is dual licensed. 
 *
 * When compiled with BELCARRA_NON_GPL undefined this file may be
 * distributed and used under the GPL license.
 *
 * When compiled with BELCARRA_NON_GPL defined and linked with additional
 * software such as the Belcarra RNDIS or MYDHCPD modules which are non-GPL
 * this file may be used and distributed only under a distribution license
 * from Belcarra.
 *
 * Any patches submitted to Belcarra for use with this file will only be
 * accepted if the changes become the property of Belcarra and the resulting
 * changed file can continue to be distributed under either of the two
 * license schemes.
 *
 *
 * This network function driver intended to interoperate with the following:
 *              
 *                                                      WinXX   WinXX   Mac     Linux   Linux   Linux   
 *                                                      USBLAN  RNDIS   CDC     usbdnet usbnet  CDCether        
 *
 *      RNDIS   Windows RNDIS Class Driver                      yes             Note 1  Note 1
 *
 *      CDC     CDC Compatible Class Driver                             yes     yes     yes     yes
 *
 *      SAFE    MDLM SAFE Compatible Class Driver                               yes     yes
 *
 *      BLAN    MDLM BLAN Compatible Class Driver                               yes     yes
 *
 *      BASIC  Simple Compatible Class Driver                                  yes
 *
 *
 * The mode can be set with a module parameter when it is loaded, the default is RNDIS.
 *
 *      insmod  network_fd         # RNDIS
 *      insmod  network_fd cdc=1   # CDC
 *      insmod  network_fd safe=1  # SAFE
 *
 * Note 1: * When used in RNDIS mode the driver can also be used with the
 * usbdnet driver.  The network_fd driver distinguishes between an RNDIS host
 * and a usbdnet host based on behaviour during enumeration. If the host
 * enumeration sequence proceeds past the set configuration with the RNDIS
 * extended enumeration, the driver will use the RNDIS encapsulation and act
 * as an RNDIS Function driver.
 *
 * If the host does nothing during enumeration, other than the standard
 * linux sequence, possibly including a set configuration then the driver
 * will act as a usbnet function driver. 
 *
 * The usbdnet driver can automatically detect endpoint configurations and
 * should function properly with network_fd in any mode.
 *
 * It is assumed that the linux usbnet class driver has an appropriate entry
 * to force selection and define the correct endpoints to match the device
 * this is running on.
 *
 * When running in usbnet/usbdnet compatible, CDC or SAFE modes the host can
 * use a setup command to enable CRC checking. 
 *
 * CRC checking is always done (except with RNDIS encapsulation). Bad
 * frames will only be dropped if we previously saw a correct CRC.
 *
 * The CRC table is generated at runtime.
 *
 */


#include <linux/config.h>
#include <linux/module.h>

#include <usbd-build.h>
#include <usbd-module.h>

#define CONFIG_USBD_NET_NFS_SUPPORT 1

MODULE_AUTHOR ("sl@belcarra.com, balden@belcarra.com");

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
MODULE_LICENSE("PROPRIETARY");
MODULE_DESCRIPTION ("Belcarra USB RNDIS/CDC Function");
USBD_MODULE_INFO ("rndis_fd");
#else
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION ("USB Network Function");
USBD_MODULE_INFO ("network_fd");
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <linux/random.h>

#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/inetdevice.h>

#include <linux/kmod.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <usbd.h>
#include <usbd-func.h>
#include <usbd-bus.h>
#include <usbd-inline.h>
#include <usbd-debug.h>
#include <usbd-arch.h>

#include <network.h>

#ifdef CONFIG_USBD_NET_NFS_SUPPORT
wait_queue_head_t usb_netif_wq;
int usb_is_configured = 0;
#endif

// Must have one of these directly defined for building into kernel.
//#define CONFIG_USBD_NETWORK_CDC 1
#define CONFIG_USBD_NETWORK_BASIC                 1

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
#endif

#if defined(CONFIG_USBD_NETWORK_CDC) && defined(CONFIG_USBD_NETWORK_SAFE)
#abort "Only one of CDC or MDLM-SAFE can be selected"
#endif

#if defined(CONFIG_USBD_NETWORK_SAFE)
#endif

#if defined(CONFIG_USBD_NETWORK_BLAN)
#endif

#if defined(CONFIG_USBD_NETWORK_BASIC)
#endif





#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MYDHCPD)
extern u_int8_t * checkfordhcp(struct net_device *net_device, u_int8_t *, int, int* );
#endif

/*
#undef CONFIG_USBD_NETWORK_CDC                   1
#undef CONFIG_USBD_NETWORK_SAFE                  1
#define CONFIG_USBD_NETWORK_BLAN                  1
#undef CONFIG_USBD_NETWORK_BASIC                 1
*/
#if defined(CONFIG_USBD_NET_NFS_SUPPORT)
#undef CONFIG_USBD_NETWORK_CDC
#undef CONFIG_USBD_NETWORK_SAFE
#undef CONFIG_USBD_NETWORK_BLAN
#define CONFIG_USBD_NETWORK_BASIC                 1
#endif
/*
#define CONFIG_USBD_NETWORK_LOCAL_MACADDR          "020001000001"
#define CONFIG_USBD_NETWORK_REMOTE_MACADDR         "020002000001"
*/


//#define CONFIG_USBD_NETWORK_ALLOW_SETPKTSIZE      1
#define CONFIG_USBD_NETWORK_ALLOW_SETID           1

unsigned char SAFE_GUID[16] = {
        0x5d, 0x34, 0xcf, 0x66, 0x11, 0x18, 0x11, 0xd6,  /* bGUID */
        0xa2, 0x1a, 0x00, 0x01, 0x02, 0xca, 0x9a, 0x7f,  /* bGUID */
};

unsigned char BLAN_GUID[16] = {
        0x74, 0xf0, 0x3d, 0xbd, 0x1e, 0xc1, 0x44, 0x70,  /* bGUID */
        0xa3, 0x67, 0x71, 0x34, 0xc9, 0xf5, 0x54, 0x37,  /* bGUID */
};



/* Module Parameters ************************************************************************* */

#ifdef CONFIG_USBD_NETWORK_ALLOW_SETID
// override vendor ID
static u32 vendor_id = CONFIG_USBD_NETWORK_VENDORID;
MODULE_PARM (vendor_id, "i");
MODULE_PARM_DESC (vendor_id, "vendor id");

// override product ID
static u32 product_id = CONFIG_USBD_NETWORK_PRODUCTID;
MODULE_PARM (product_id, "i");
MODULE_PARM_DESC (product_id, "product id");
#endif

// override local mac address
#ifdef CONFIG_USBD_NETWORK_LOCAL_MACADDR
static char *local_mac_address_str = CONFIG_USBD_NETWORK_LOCAL_MACADDR;
#else
static char *local_mac_address_str;
#endif
MODULE_PARM (local_mac_address_str, "s");
MODULE_PARM_DESC (local_mac_address_str, "Local MAC");

// override remote mac address
#ifdef CONFIG_USBD_NETWORK_REMOTE_MACADDR
static char *remote_mac_address_str = CONFIG_USBD_NETWORK_REMOTE_MACADDR;
#else
static char *remote_mac_address_str;
#endif
MODULE_PARM (remote_mac_address_str, "s");
MODULE_PARM_DESC (remote_mac_address_str, "Remote MAC");

static int in_pkt_sz = BULK_IN_PKTSIZE;
static int out_pkt_sz = BULK_OUT_PKTSIZE;
#ifdef CONFIG_USBD_NETWORK_ALLOW_SETPKTSIZE
// override bulk in packet size
MODULE_PARM (in_pkt_sz, "i");
MODULE_PARM_DESC (in_pkt_sz, "Override Bulk IN packet size");

// override bulk out packet size
MODULE_PARM (out_pkt_sz, "i");
MODULE_PARM_DESC (out_pkt_sz, "Override Bulk OUT packet size");
#endif

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
static u32 rndis = 0;
MODULE_PARM (rndis, "i");
MODULE_PARM_DESC (rndis, "Enable RNDIS only mode");
#endif

#if defined(CONFIG_USBD_NETWORK_CDC)
static u32 cdc = 0;
MODULE_PARM (cdc, "i");
MODULE_PARM_DESC (cdc, "Enable CDC mode");
#endif

#if defined(CONFIG_USBD_NETWORK_SAFE)
static u32 safe = 0;
MODULE_PARM (safe, "i");
MODULE_PARM_DESC (safe, "Enable SAFE mode");
#endif

#if defined(CONFIG_USBD_NETWORK_BLAN)
static u32 blan = 0;
MODULE_PARM (blan, "i");
MODULE_PARM_DESC (blan, "Enable BLAN mode");
#endif

#if defined(CONFIG_USBD_NETWORK_BASIC)
#if defined(CONFIG_USBD_NET_NFS_SUPPORT)
static u32 basic = 1;  // for building into kernel
#else
static u32 basic = 0;
#endif
MODULE_PARM (basic, "i");
MODULE_PARM_DESC (basic, "Enable BASIC mode");
#endif

#ifdef CONFIG_USBD_NETWORK_EP0TEST
static u32 ep0test;
MODULE_PARM (ep0test, "i");
MODULE_PARM_DESC (ep0test, "Test EP0 String Handling - set to ep0 packetsize [8,16,32,64]");
#endif


#define NETWORK_CREATED         0x01
#define NETWORK_REGISTERED      0x02
#define NETWORK_DESTROYING      0x04
#define NETWORK_ATTACHED        0x08


#define MCCI_ENABLE_CRC         0x03


#define BELCARRA_GETMAC         0x04

#define BELCARRA_SETTIME        0x04
#define BELCARRA_SETIP          0x05
#define BELCARRA_SETMSK         0x06
#define BELCARRA_SETROUTER      0x07
#define BELCARRA_SETDNS         0x08



#define RFC868_OFFSET_TO_EPOCH  0x83AA7E80      // RFC868 - seconds from midnight on 1 Jan 1900 GMT to Midnight 1 Jan 1970 GMT



static __u8 local_dev_addr[ETH_ALEN];
static __u8 remote_dev_addr[ETH_ALEN];
static __u8 zeros[ETH_ALEN];

static __u32 ip_addr;
static __u32 router_ip;
static __u32 network_mask;
static __u32 dns_server_ip;

static int hotplug_active;


int crc_checks[64];
int crc_errors[64];

rwlock_t network_rwlock = RW_LOCK_UNLOCKED;

struct net_device network_net_device;


#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
/* RNDIS USB Configuration ******************************************************************* */

/* RNDIS Endpoints descriptor lists and transfer sizes
 */
static __u8 rndis_data_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK, BULK_OUT_PKTSIZE, 0x00, 0x00, };
static __u8 rndis_data_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT | IN,   BULK, BULK_IN_PKTSIZE,  0x00, 0x00, };
static __u8 rndis_comm_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN, INTERRUPT, INT_IN_PKTSIZE, 0x00, 0x0a, };

static __u8 *rndis_data_endpoints[] = { rndis_data_1, rndis_data_2, };
static __u8 *rndis_comm_endpoints[] = { rndis_comm_1, };

static __u32 rndis_data_transferSizes[] = { MAXFRAMESIZE + 48, MAXFRAMESIZE + 48, };
static __u32 rndis_comm_transferSizes[] = { INT_IN_PKTSIZE, };


/* RNDIS Alternate Interface descriptions and descriptors
 */
static __u8 rndis_comm_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (rndis_comm_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        COMMUNICATIONS_INTERFACE_CLASS, COMMUNICATIONS_ACM_SUBCLASS, VENDOR, 0x00,
};
static __u8 rndis_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x01, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (rndis_data_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        DATA_INTERFACE_CLASS, COMMUNICATIONS_NO_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};

static struct usb_alternate_description rndis_comm_alternate_descriptions[] = {
      { iInterface:"Comm Intf",
                interface_descriptor: (struct usb_interface_descriptor *)&rndis_comm_alternate_descriptor,
	        endpoints:sizeof (rndis_comm_endpoints) / sizeof(__u8 *),
	        //endpoint_list:(struct usb_endpoint_descriptor *)rndis_comm_endpoints, 
	        endpoint_list:rndis_comm_endpoints, 
                endpoint_transferSizes_list: rndis_comm_transferSizes,
                },
};


static struct usb_alternate_description rndis_data_alternate_descriptions[] = {
      { iInterface:"Data Intf",
                interface_descriptor: (struct usb_interface_descriptor *)&rndis_data_alternate_descriptor,
                endpoints:sizeof (rndis_data_endpoints) / sizeof(__u8 *),
                //endpoint_list:(struct usb_endpoint_descriptor *)rndis_data_endpoints, 
                endpoint_list: rndis_data_endpoints, 
                endpoint_transferSizes_list: rndis_data_transferSizes,
        },
};

/* RNDIS Interface description and descriptors
 */
static struct usb_interface_description rndis_interfaces[] = {
        { alternates:sizeof (rndis_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:rndis_comm_alternate_descriptions,},

        { alternates:sizeof (rndis_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:rndis_data_alternate_descriptions,},
};

/* Configuration description and descriptors
 */
static __u8 rndis_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (rndis_interfaces) / sizeof (struct usb_interface_description), 
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,

};

struct usb_configuration_description rndis_description[] = {
        { iConfiguration: "RNDIS Net Cfg", 
                configuration_descriptor: (struct usb_configuration_descriptor *) rndis_configuration_descriptor,
	        interfaces:sizeof (rndis_interfaces) / sizeof (struct usb_interface_description),
                interface_list:rndis_interfaces,},
};

static __u8 rndis_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 0x00, 0x02, // bcdUSB
        COMMUNICATIONS_DEVICE_CLASS, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description rndis_device_description = {
        device_descriptor: (struct usb_device_descriptor *)rndis_device_descriptor,
	idVendor:CONFIG_USBD_NETWORK_VENDORID,
	idProduct:CONFIG_USBD_NETWORK_PRODUCTID,
        bcdDevice: CONFIG_USBD_NETWORK_BCDDEVICE,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
	iProduct:CONFIG_USBD_PRODUCT_NAME,
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};
#endif                          /* CONFIG_USBD_RNDIS */

#ifdef CONFIG_USBD_NETWORK_BASIC
/* USB BASIC Configuration ******************************************************************** */

/* BASIC Communication Interface Class descriptors
 */
static __u8 basic_data_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,     BULK_OUT_PKTSIZE, 0x00, 0x00,  };
static __u8 basic_data_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT | IN,   BULK,     BULK_IN_PKTSIZE,  0x00, 0x00, };
static __u8 basic_comm_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT,INT_IN_PKTSIZE, 0x00, 0x0a, };

static  __u8 *basic_default[] = { basic_data_1, basic_data_2, basic_comm_1, };

static __u32 basic_default_transferSizes[] = { MAXFRAMESIZE + 4, MAXFRAMESIZE + 4, INT_IN_PKTSIZE };


/* BASIC Data Interface Alternate endpoints
 */
static __u8 basic_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (basic_default) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        LINEO_CLASS, LINEO_SUBCLASS_BASIC_NET, LINEO_BASIC_NET_CRC, 0x00,
};

static struct usb_alternate_description basic_data_alternate_descriptions[] = {
	{ iInterface: CONFIG_USBD_NETWORK_BASIC_INTF,
                interface_descriptor: (struct usb_interface_descriptor *)&basic_data_alternate_descriptor,
                endpoints:sizeof (basic_default) / sizeof(__u8 *),
                endpoint_list: basic_default,
                endpoint_transferSizes_list: basic_default_transferSizes,
                },
};


/* BASIC Data Interface Alternate descriptions and descriptors
 */

/* BASIC Interface descriptions and descriptors
 */
static struct usb_interface_description basic_interfaces[] = {
	{ alternates:sizeof (basic_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:basic_data_alternate_descriptions,},
};

/* BASIC Configuration descriptions and descriptors
 */
static __u8 basic_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (basic_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};

struct usb_configuration_description basic_description[] = {
	{ iConfiguration: CONFIG_USBD_NETWORK_BASIC_DESC,
                configuration_descriptor: (struct usb_configuration_descriptor *)basic_configuration_descriptor,
                interfaces:sizeof (basic_interfaces) / sizeof (struct usb_interface_description),
                interface_list:basic_interfaces,},

};

/* BASIC Device Description
 */
static __u8 basic_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 0x00, 0x02, /* bcdUSB */
        COMMUNICATIONS_DEVICE_CLASS, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description basic_device_description = {
        device_descriptor: (struct usb_device_descriptor *)basic_device_descriptor,
	idVendor:CONFIG_USBD_VENDORID,
	idProduct:CONFIG_USBD_PRODUCTID,
        bcdDevice: CONFIG_USBD_NETWORK_BCDDEVICE,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
        iProduct:CONFIG_USBD_PRODUCT_NAME,
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};

#endif				/* CONFIG_USBD_NETWORK_BASIC */


#ifdef CONFIG_USBD_NETWORK_CDC
/* USB CDC Configuration ********************************************************************* */

/* CDC Communication Interface Class descriptors
 */
static __u8 cdc_data_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,     BULK_OUT_PKTSIZE, 0x00, 0x00, };
static __u8 cdc_data_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT  | IN,  BULK,     BULK_IN_PKTSIZE,  0x00, 0x00, };

#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
static __u8 cdc_comm_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT,INT_IN_PKTSIZE, 0x00, 0x0a, };
#endif

static __u8 cdc_class_1[] = { 0x05, CS_INTERFACE, USB_ST_HEADER, 0x10, 0x01, /* CLASS_BDC_VERSION, CLASS_BDC_VERSION */ };
static __u8 cdc_class_2[] = { 
        0x0d, CS_INTERFACE, USB_ST_ENF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x05, /* 1514 maximum frame size */
        0x00, 0x00, 0x00 , };
static __u8 cdc_class_3[] = { 0x05, CS_INTERFACE, USB_ST_UF, 0x00, 0x01, /* bMasterInterface, bSlaveInterface */};


static __u8 *cdc_data_endpoints[] = { cdc_data_1, cdc_data_2, };
static __u8 *cdc_comm_class_descriptors[] = { cdc_class_1, cdc_class_2, cdc_class_3, };

#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
static __u8 *cdc_comm_endpoints[] = { cdc_comm_1, };
static __u32 cdc_comm_transferSizes[] = { INT_IN_PKTSIZE, };
#endif
static __u32 cdc_data_transferSizes[] = { MAXFRAMESIZE + 4, MAXFRAMESIZE + 4, };



/* Data Interface Alternate descriptions and descriptors
 */
static __u8 cdc_comm_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (cdc_comm_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        COMMUNICATIONS_INTERFACE_CLASS, COMMUNICATIONS_ENCM_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};
static __u8 cdc_nodata_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x01, 0x00, // bInterfaceNumber, bAlternateSetting
        0x00, // bNumEndpoints
        DATA_INTERFACE_CLASS, COMMUNICATIONS_NO_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};
static __u8 cdc_data_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x01, 0x01, // bInterfaceNumber, bAlternateSetting
        sizeof (cdc_data_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        DATA_INTERFACE_CLASS, COMMUNICATIONS_NO_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};

static struct usb_alternate_description cdc_comm_alternate_descriptions[] = {
	{ iInterface: CONFIG_USBD_NETWORK_CDC_COMM_INTF,
                interface_descriptor: (struct usb_interface_descriptor *)&cdc_comm_alternate_descriptor,
                classes:sizeof (cdc_comm_class_descriptors) / sizeof (__u8 *),
                class_list: cdc_comm_class_descriptors,
#if defined(INT_IN_ENDPOINT) && (INT_IN_ENDPOINT > 0)
                endpoints:sizeof (cdc_comm_endpoints) / sizeof(__u8 *),
                endpoint_list: cdc_comm_endpoints,
                endpoint_transferSizes_list: cdc_comm_transferSizes,
#endif
        },
};


static struct usb_alternate_description cdc_data_alternate_descriptions[] = {
	{ iInterface: CONFIG_USBD_NETWORK_CDC_NODATA_INTF,
                interface_descriptor: (struct usb_interface_descriptor *)&cdc_nodata_alternate_descriptor, },
	{ iInterface: CONFIG_USBD_NETWORK_CDC_DATA_INTF,
                interface_descriptor: (struct usb_interface_descriptor *)&cdc_data_alternate_descriptor,
                endpoints:sizeof (cdc_data_endpoints) / sizeof(__u8 *),
                endpoint_list: cdc_data_endpoints,
                endpoint_transferSizes_list: cdc_data_transferSizes, },
};

/* Interface descriptions and descriptors
 */
static struct usb_interface_description cdc_interfaces[] = {
	{ alternates:sizeof (cdc_comm_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:cdc_comm_alternate_descriptions,},

	{ alternates:sizeof (cdc_data_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:cdc_data_alternate_descriptions,},
};

/* Configuration descriptions and descriptors
 */

static __u8 cdc_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};

struct usb_configuration_description cdc_description[] = {
	{ iConfiguration: CONFIG_USBD_NETWORK_CDC_DESC,
                configuration_descriptor: (struct usb_configuration_descriptor *)cdc_configuration_descriptor,
                interfaces:sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
                interface_list:cdc_interfaces,},
};



/* Device Description
 */

static __u8 cdc_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 0x00, 0x02, // bcdUSB
        COMMUNICATIONS_DEVICE_CLASS, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description cdc_device_description = {
        device_descriptor: (struct usb_device_descriptor *)cdc_device_descriptor,
	idVendor:CONFIG_USBD_NETWORK_VENDORID,
	idProduct:CONFIG_USBD_NETWORK_PRODUCTID,
        bcdDevice: CONFIG_USBD_NETWORK_BCDDEVICE,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
	iProduct:CONFIG_USBD_PRODUCT_NAME,
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};

#endif				/* CONFIG_USBD_NETWORK_CDC */


#ifdef CONFIG_USBD_NETWORK_SAFE
/* USB SAFE  Configuration ******************************************************************** */

/*
 * SAFE Ethernet Configuration
 */

/* Communication Interface Class descriptors
 */

static __u8 safe_data_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,     BULK_OUT_PKTSIZE, 0x00, 0x00, };
static __u8 safe_data_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT | IN,   BULK,     BULK_IN_PKTSIZE,  0x00, 0x00, };
static __u8 safe_comm_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT,INT_IN_PKTSIZE, 0x00, 0x0a, };

static __u8 safe_class_1[] = { 0x05, CS_INTERFACE, USB_ST_HEADER, 0x10, 0x01, /* CLASS_BDC_VERSION, CLASS_BDC_VERSION */ };
static __u8 safe_class_2[] = { 0x15, CS_INTERFACE, USB_ST_MDLM, 0x00, 0x01, /* bcdVersion, bcdVersion */
                0x5d, 0x34, 0xcf, 0x66, 0x11, 0x18, 0x11, 0xd6,  /* bGUID */
                0xa2, 0x1a, 0x00, 0x01, 0x02, 0xca, 0x9a, 0x7f,  /* bGUID */ };

static __u8 safe_class_3[] = { 0x06, CS_INTERFACE, USB_ST_MDLMD, 0x00, 0x00, 0x03,    /* bDetailData */ };

static __u8 safe_class_4[] = { 0x0d, CS_INTERFACE, USB_ST_ENF, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x05, /* 1514 maximum frame size */
        0x00, 0x00, 0x00 , };


static __u8 *safe_alt_endpoints[] = { safe_data_1, safe_data_2, safe_comm_1, };
static __u8 *safe_comm_class_descriptors[] = { safe_class_1, safe_class_2, safe_class_3, safe_class_4, };
static __u32 safe_alt_transferSizes[] = { MAXFRAMESIZE + 4, MAXFRAMESIZE + 4, INT_IN_PKTSIZE };



/* Data Interface Alternate descriptions and descriptors
 */
static __u8 safe_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (safe_alt_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        COMMUNICATIONS_INTERFACE_CLASS, COMMUNICATIONS_MDLM_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};

static struct usb_alternate_description safe_alternate_descriptions[] = {
	{ iInterface: CONFIG_USBD_NETWORK_SAFE_INTF,
                interface_descriptor: (struct usb_interface_descriptor *)&safe_alternate_descriptor,
                classes:sizeof (safe_comm_class_descriptors) / sizeof (__u8 *),
                class_list: safe_comm_class_descriptors,
                endpoints:sizeof (safe_alt_endpoints) / sizeof(__u8 *),
                endpoint_list: safe_alt_endpoints,
                endpoint_transferSizes_list: safe_alt_transferSizes, },
};

/* Interface descriptions and descriptors
 */
static struct usb_interface_description safe_interfaces[] = {
	{ alternates:sizeof (safe_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:safe_alternate_descriptions, },
};


/* Configuration descriptions and descriptors
 */

static __u8 safe_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (safe_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};

struct usb_configuration_description safe_description[] = {
	{ iConfiguration: CONFIG_USBD_NETWORK_SAFE_DESC,
                configuration_descriptor: (struct usb_configuration_descriptor *)safe_configuration_descriptor,
	        interfaces:sizeof (safe_interfaces) / sizeof (struct usb_interface_description),
                interface_list:safe_interfaces, },
};

/* Device Description
 */

static __u8 safe_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 
        0x00, 0x02, // bcdUSB
        COMMUNICATIONS_DEVICE_CLASS, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description safe_device_description = {
        device_descriptor: (struct usb_device_descriptor *)safe_device_descriptor,
	idVendor:CONFIG_USBD_VENDORID,
	idProduct:CONFIG_USBD_PRODUCTID,
        bcdDevice: CONFIG_USBD_NETWORK_BCDDEVICE,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
        iProduct:CONFIG_USBD_PRODUCT_NAME,
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};
#endif				/* CONFIG_USBD_NETWORK_SAFE */

#ifdef CONFIG_USBD_NETWORK_BLAN
/* USB BLAN  Configuration ******************************************************************** */

/*
 * BLAN Ethernet Configuration
 */

/* Communication Interface Class descriptors
 */

static __u8 blan_data_1[] = { 0x07, USB_DT_ENDPOINT, BULK_OUT_ENDPOINT | OUT, BULK,     BULK_OUT_PKTSIZE, 0x00, 0x00, };
static __u8 blan_data_2[] = { 0x07, USB_DT_ENDPOINT, BULK_IN_ENDPOINT | IN,   BULK,     BULK_IN_PKTSIZE,  0x00, 0x00, };
static __u8 blan_comm_1[] = { 0x07, USB_DT_ENDPOINT, INT_IN_ENDPOINT | IN,  INTERRUPT,INT_IN_PKTSIZE, 0x00, 0x0a, };

static __u8 blan_class_1[] = { 0x05, CS_INTERFACE, USB_ST_HEADER, 0x10, 0x01, /* CLASS_BDC_VERSION, CLASS_BDC_VERSION */ };
static __u8 blan_class_2[] = { 0x15, CS_INTERFACE, USB_ST_MDLM, 0x00, 0x01, /* bcdVersion, bcdVersion */
                0x74, 0xf0, 0x3d, 0xbd, 0x1e, 0xc1, 0x44, 0x70,  /* bGUID */
                0xa3, 0x67, 0x71, 0x34, 0xc9, 0xf5, 0x54, 0x37,  /* bGUID */ };

static __u8 blan_class_3[] = { 0x06, CS_INTERFACE, USB_ST_MDLMD, 0x00, 0x00, 0x03,    /* bDetailData */ };

static __u8 blan_class_4[] = { 0x0d, CS_INTERFACE, USB_ST_ENF, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x05, /* 1514 maximum frame size */
        0x00, 0x00, 0x00 , };


static __u8 *blan_alt_endpoints[] = { blan_data_1, blan_data_2, blan_comm_1, };
static __u8 *blan_comm_class_descriptors[] = { blan_class_1, blan_class_2, blan_class_3, blan_class_4, };
static __u32 blan_alt_transferSizes[] = { MAXFRAMESIZE + 4, MAXFRAMESIZE + 4, INT_IN_PKTSIZE };



/* Data Interface Alternate descriptions and descriptors
 */
static __u8 blan_alternate_descriptor[sizeof(struct usb_interface_descriptor)] = {
        0x09, USB_DT_INTERFACE, 0x00, 0x00, // bInterfaceNumber, bAlternateSetting
        sizeof (blan_alt_endpoints) / sizeof(struct usb_endpoint_descriptor), // bNumEndpoints
        COMMUNICATIONS_INTERFACE_CLASS, COMMUNICATIONS_MDLM_SUBCLASS, COMMUNICATIONS_NO_PROTOCOL, 0x00,
};

static struct usb_alternate_description blan_alternate_descriptions[] = {
	{ iInterface: CONFIG_USBD_NETWORK_BLAN_INTF,
                interface_descriptor: (struct usb_interface_descriptor *)&blan_alternate_descriptor,
                classes:sizeof (blan_comm_class_descriptors) / sizeof (__u8 *),
                class_list: blan_comm_class_descriptors,
                endpoints:sizeof (blan_alt_endpoints) / sizeof(__u8 *),
                endpoint_list: blan_alt_endpoints,
                endpoint_transferSizes_list: blan_alt_transferSizes, },
};

/* Interface descriptions and descriptors
 */
static struct usb_interface_description blan_interfaces[] = {
	{ alternates:sizeof (blan_alternate_descriptions) / sizeof (struct usb_alternate_description),
                alternate_list:blan_alternate_descriptions, },
};


/* Configuration descriptions and descriptors
 */

static __u8 blan_configuration_descriptor[sizeof(struct usb_configuration_descriptor)] = {
        0x09, USB_DT_CONFIG, 0x00, 0x00, // wLength
        sizeof (blan_interfaces) / sizeof (struct usb_interface_description),
        0x01, 0x00, // bConfigurationValue, iConfiguration
        BMATTRIBUTE, BMAXPOWER,
};

struct usb_configuration_description blan_description[] = {
	{ iConfiguration: CONFIG_USBD_NETWORK_BLAN_DESC,
                configuration_descriptor: (struct usb_configuration_descriptor *)blan_configuration_descriptor,
	        interfaces:sizeof (blan_interfaces) / sizeof (struct usb_interface_description),
                interface_list:blan_interfaces, },
};

/* Device Description
 */

static __u8 blan_device_descriptor[sizeof(struct usb_device_descriptor)] = {
        0x12, USB_DT_DEVICE, 
        0x00, 0x02, // bcdUSB
        COMMUNICATIONS_DEVICE_CLASS, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct usb_device_description blan_device_description = {
        device_descriptor: (struct usb_device_descriptor *)blan_device_descriptor,
	idVendor:CONFIG_USBD_VENDORID,
	idProduct:CONFIG_USBD_PRODUCTID,
        bcdDevice: CONFIG_USBD_NETWORK_BCDDEVICE,
	iManufacturer:CONFIG_USBD_MANUFACTURER,
        iProduct:CONFIG_USBD_PRODUCT_NAME,
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
};
#endif				/* CONFIG_USBD_NETWORK_BLAN */

//_________________________________________________________________________________________________

/*
 * If the following are defined we implement the crc32_copy routine using
 * Duff's device. This will unroll the copy loop by either 4 or 8. Do not
 * use these without profiling to test if it actually helps on any specific
 * device.
 */
#undef CONFIG_USBD_NETWORK_CRC_DUFF4
#undef CONFIG_USBD_NETWORK_CRC_DUFF8

static __u32 *crc32_table;

#define CRC32_INIT   0xffffffff      // Initial FCS value
#define CRC32_GOOD   0xdebb20e3      // Good final FCS value

#define CRC32_POLY   0xedb88320      // Polynomial for table generation
        
#define COMPUTE_FCS(val, c) (((val) >> 8) ^ crc32_table[((val) ^ (c)) & 0xff])

//_________________________________________________________________________________________________
//                                      crc32_copy

/**
 * Generate the crc32 table
 *
 * return non-zero if malloc fails
 */
int make_crc_table(void)
{
        //printk(KERN_INFO"make_crc_table: crc32_table: %p\n", crc32_table);

        if (!crc32_table) {
                u32 n;


                if (!(crc32_table = (u32 *)ckmalloc(256*4, GFP_KERNEL))) {
                        printk(KERN_ERR"malloc crc32_table failed\n");
                        return -EINVAL;
                }
                
                //printk(KERN_INFO"make_crc_table: generating crc32_table: %p\n", crc32_table);

                for (n = 0; n < 256; n++) {
                        int k;
                        u32 c = n;
                        for (k = 0; k < 8; k++) {
                                c =  (c & 1) ? (CRC32_POLY ^ (c >> 1)) : (c >> 1);
                        }
                        crc32_table[n] = c;
                }
        }
        return 0;
}


/**
 * Copies a specified number of bytes, computing the 32-bit CRC FCS as it does so.
 *
 * @param dst   Pointer to the destination memory area.
 * @param src   Pointer to the source memory area.
 * @param len   Number of bytes to copy.
 * @param val   Starting value for the CRC FCS.
 *
 * @return      Final value of the CRC FCS.
 *
 * @sa crc32_pad
 */
static __u32 crc32_copy (__u8 *dst, __u8 *src, int len, __u32 val)
{
#if !defined(CONFIG_USBD_NETWORK_CRC_DUFF4) && !defined(CONFIG_USBD_NETWORK_CRC_DUFF8)
        for (; len-- > 0; val = COMPUTE_FCS (val, *dst++ = *src++));
#else

#if defined(CONFIG_USBD_NETWORK_CRC_DUFF8)
        int n = (len + 7) / 8;
        switch (len % 8) 
#elif defined(CONFIG_USBD_NETWORK_CRC_DUFF4)
                int n = (len + 3) / 4;
        switch (len % 4) 
#endif
        {
        case 0: do {
                        val = COMPUTE_FCS (val, *dst++ = *src++);
#if defined(CONFIG_USBD_NETWORK_CRC_DUFF8)
                case 7:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
                case 6:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
                case 5:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
                case 4:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
#endif
                case 3:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
                case 2:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
                case 1:
                        val = COMPUTE_FCS (val, *dst++ = *src++);
                } while (--n > 0);
        }
#endif
        return val;
}

#if 0
static __u32 crc32_copy (__u8 *dst, __u8 *src, int len, __u32 val)
{

        int n = (len + 3) / 4;
        __u32 l;
       
        while (--n) {

                __u8 i;
                l = *((__u32 *)dst) = *((__u32 *)src);
                dst += 4;
                src += 4;

                for (i = 0; i < 4; i++) {
                        val = COMPUTE_FCS(val, l&0xff);
                        l = l >> 8;
                }

        } 

        switch (len % 4) {

        case 3:
                val = COMPUTE_FCS (val, l = *dst++ = *src++);
        case 2:
                val = COMPUTE_FCS (val, l = *dst++ = *src++);
        case 1:
                val = COMPUTE_FCS (val, l = *dst++ = *src++);
        }
        return val;
}
#endif



//_________________________________________________________________________________________________
//                                      crc32_pad

/**
 * Fills a memory area with zero bytes, computing the 32-bit CRC FCS as it does so.
 *
 * @param dst   Pointer to the destination memory area.
 * @param len   Number of bytes to fill.
 * @param val   Starting value for the CRC FCS.
 *
 * @return      Final value of the CRC FCS.
 *
 * @sa crc32_copy
 */
static __u32 __inline__ crc32_pad (__u8 *dst, int len, __u32 val)
{
        for (; len-- > 0; val = COMPUTE_FCS (val, *dst++ = '\0'));
        return val;
}


//______________________________________ Hotplug Functions ________________________________________

#ifdef CONFIG_HOTPLUG

int hotplug_attach (char *agent, char *interface, __u32 ip, __u32 mask)
{
	char *argv[3];
        char *envp[9];
        char ifname[20+12 + IFNAMSIZ];
        char *action_str = "ACTION=attach";
        unsigned char *cp;
        char ip_str[20+32];
        char mask_str[20+32];
        __u32 nh;
        int i;

	if (!hotplug_path[0]) {
		printk (KERN_ERR "hotplug: hotplug_path empty\n");
		return -EINVAL;
	}
	//printk(KERN_DEBUG "hotplug: %s\n", hotplug_path);

	if (in_interrupt ()) {
		printk (KERN_ERR "hotplug: in_interrupt\n");
		//return -EINVAL;
	}
	sprintf (ifname, "INTERFACE=%s", interface);

        nh = htonl(ip);
        cp = (unsigned char*) &nh;
	sprintf (ip_str, "IP=%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);

        nh = htonl(mask);
        cp = (unsigned char*) &nh;
	sprintf (mask_str, "MASK=%d.%d.%d.%d", cp[0], cp[1], cp[2], cp[3]);

        i = 0;
	argv[i++] = hotplug_path;
	argv[i++] = agent;
	argv[i++] = 0;

        i = 0;
	envp[i++] = "HOME=/";
	envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp[i++] = ifname;
	envp[i++] = action_str;
	envp[i++] = ip_str;
	envp[i++] = mask_str;
	envp[i++] = 0;

	return call_usermodehelper (argv[0], argv, envp);
}

int hotplug_detach (char *agent, char *interface)
{
	char *argv[3];
        char *envp[6];
        char ifname[20+12 + IFNAMSIZ];
        char *action_str = "ACTION=detach";
        int i;

	if (!hotplug_path[0]) {
		printk (KERN_ERR "hotplug: hotplug_path empty\n");
		return -EINVAL;
	}
	//printk(KERN_DEBUG "hotplug: %s\n", hotplug_path);

	if (in_interrupt ()) {
		printk (KERN_ERR "hotplug: in_interrupt\n");
		return -EINVAL;
	}
	sprintf (ifname, "INTERFACE=%s", interface);

        i = 0;
	argv[i++] = hotplug_path;
	argv[i++] = agent;
	argv[i++] = 0;

        i = 0;
	envp[i++] = "HOME=/";
	envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp[i++] = ifname;
	envp[i++] = action_str;
	envp[i++] = 0;

	return call_usermodehelper (argv[0], argv, envp);
}


/**
 * hotplug_bh - 
 * @ignored:
 *
 * Check connected status and load/unload as appropriate.
 */
void hotplug_bh (void *data)
{
        struct usb_network_private *network_private = network_net_device.priv;

        
        
        if (network_private && network_private->device) {

                //printk (KERN_INFO "hotplug_bh: network_private: %p device: %p status: %d hotplug: %d\n", 
                //                network_private, network_private->device, 
                //                network_private->device->status, network_private->hotplug_status);


                if ((network_private->device->status != USBD_OK) && (network_private->hotplug_status != hotplug_detached)) {
                        network_private->hotplug_status = hotplug_detached;
                        hotplug_detach ("network_fd", network_net_device.name);
                }

                else if ((network_private->device->status == USBD_OK) && (network_private->hotplug_status != hotplug_attached)) {
                        network_private->hotplug_status = hotplug_attached;
                        hotplug_attach ("network_fd", network_net_device.name, ip_addr, network_mask);
                }
        } 

        //hotplug_detach ("network_fd", "usbl0");
        hotplug_active = 0;
        MOD_DEC_USE_COUNT;
}

/**
 * hotplug_schedule_bh -
 */
void hotplug_schedule_bh (struct usb_network_private *network_private)
{
        //printk (KERN_INFO "hotplug_schedule_bh: schedule bh\n");

        if (network_private->hotplug_bh.sync) {
                return;
        }

        MOD_INC_USE_COUNT;
        //network_private->hotplug_bh.data = network_private;
        hotplug_active = 1;

        if (!schedule_task (&network_private->hotplug_bh)) {
                printk (KERN_DEBUG "hotplug_schedule_bh: failed\n");
                MOD_DEC_USE_COUNT;
        }
}


#endif				/* CONFIG_HOTPLUG */


//______________________________________Network Layer Functions____________________________________


//_________________________________________________________________________________________________
//                                      network_init

/**
 * Initializes the specified network device.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 *
 * @return Status code; zero for success.
 */
static int network_init (struct net_device *net_device)
{
	//printk(KERN_INFO"%s: net_device=0x%p; ->priv=0x%p; ->name=%s\n", 
        //                __FUNCTION__, net_device, net_device->priv, net_device->name);

	return 0;
}

//_________________________________________________________________________________________________
//                                      network_uninit

/**
 * Uninitializes the specified network device.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 */
static void network_uninit (struct net_device *net_device)
{
	//printk(KERN_INFO"%s: %s\n", __FUNCTION__, net_device->name);
	return;
}

//_________________________________________________________________________________________________
//                                      network_open

/**
 * Opens the specified network device.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 *
 * @return Status code; zero for success, -EBUSY if the device is already in use.
 */
static int network_open (struct net_device *net_device)
{
	//printk(KERN_INFO"%s\n", net_device->name);

	MOD_INC_USE_COUNT;

	if (netif_running (net_device)) {
		printk(KERN_INFO"%s: BUSY %s\n", __FUNCTION__, net_device->name);
		return -EBUSY;
	}

	netif_wake_queue (net_device);

#ifdef CONFIG_USBD_NET_NFS_SUPPORT
	if ( !usb_is_configured ) {
		if ( !in_interrupt() ) {
			printk("Please replug USB cable and then ifconfig host interface.\n");
			interruptible_sleep_on( &usb_netif_wq );
		} else {
			printk("Warning! In interrupt!\n");
		}
	}
#endif
	return 0;
}

//_________________________________________________________________________________________________
//                                      network_stop

/**
 * Stops the specified network device.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 *
 * @return Status code; zero for success.
 */
static int network_stop (struct net_device *net_device)
{
	//printk(KERN_INFO"%s: %s\n", __FUNCTION__, net_device->name);
	netif_stop_queue (net_device);
	MOD_DEC_USE_COUNT;
	return 0;
}

//_________________________________________________________________________________________________
//                                      network_get_stats

/**
 * Gets statistics from the specified network device.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 *
 * @return A net_device_stats structure with the required information.
 */
static struct net_device_stats *network_get_stats (struct net_device *net_device)
{
	struct usb_network_private *network_private = net_device->priv;
	return &network_private->stats;
}

//_________________________________________________________________________________________________
//                                      network_set_mac_addr

/**
 * Sets the MAC address of the specified network device. Fails if the device is in use.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 * @param p             Pointer to a sockaddr structure containing the required address.
 *
 * @return Status code; zero for success, -EBUSY if the device was in use.
 */
static int network_set_mac_addr (struct net_device *net_device, void *p)
{
	struct usb_network_private *network_private = net_device->priv;
	struct sockaddr *addr = p;

	//printk(KERN_INFO"%s: %s\n", __FUNCTION__, net_device->name);

	if (netif_running (net_device)) {
		return -EBUSY;
	}

	{
		unsigned long flags;
		write_lock_irqsave (&network_private->rwlock, flags);
		memcpy (net_device->dev_addr, addr->sa_data, net_device->addr_len);
		write_unlock_irqrestore (&network_private->rwlock, flags);
	}
	return 0;
}

//_________________________________________________________________________________________________
//                                      network_tx_timeout

/**
 * Tells the specified network device that its current transmit attempt has timed out.
 * Called by the network layer.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 */
static void network_tx_timeout (struct net_device *net_device)
{
        struct usb_network_private *network_private = net_device->priv;

	//printk(KERN_INFO"%s: %s\n", __FUNCTION__, net_device->name);

        if (network_private) {
                usbd_cancel_urb (network_private->device, NULL);
        }
}

//_________________________________________________________________________________________________
//                                      network_set_config

/**
 * Sets the specified network device's configuration. Fails if the device is in use.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 * @param map           An ifmap structure containing configuration values.
 *                      Those values which are non-zero/non-null update the corresponding fields
 *                      in net_device.
 *
 * @return Status code; zero for success, -EBUSY if the device was in use.
 */
static int network_set_config (struct net_device *net_device, struct ifmap *map)
{
	//printk(KERN_INFO"%s: %s\n", __FUNCTION__, net_device->name);

	if (netif_running (net_device)) {
		return -EBUSY;
	}
	if (map->base_addr) {
		net_device->base_addr = map->base_addr;
	}
	if (map->mem_start) {
		net_device->mem_start = map->mem_start;
	}
	if (map->irq) {
		net_device->irq = map->irq;
	}
	return 0;
}

//_________________________________________________________________________________________________
//                                      network_change_mtu

/**
 * Sets the specified network device's MTU. Fails if the new value is larger and
 * the device is in use.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 * @param mtu           The new value for Maximum Transmission Unit.
 *
 * @return Status code; zero for success, -EBUSY if the new value is larger and
 *         the device is in use.
 */
static int network_change_mtu (struct net_device *net_device, int mtu)
{
	struct usb_network_private *network_private = net_device->priv;

	//printk(KERN_INFO"%s: %s\n", __FUNCTION__, net_device->name);

	if (mtu > network_private->mtu && netif_running (net_device)) {
                return -EBUSY;
        }

	net_device->mtu = mtu;
	return 0;
}

//_________________________________________________________________________________________________
//                                      network_hard_start_xmit

/**
 * Starts a network device's transmit function.
 * Called by kernel network layer to transmit a frame on this device.
 * Grab locks and pass to @c netproto_do_xmit().
 * The network layer flow control is managed to prevent more than 
 * @c device->max_queue_entries from being outstanding.
 *
 * @param skb           Pointer to the sk_buff structure to be sent.
 * @param net_device    Specifies the device by pointing to its net_device struct.
 *
 * @return 0 - success; 1 - I'm busy, give it to me later
 *
 * @sa netproto_done().
 */
static int network_hard_start_xmit (struct sk_buff *skb, struct net_device *net_device)
{
	struct usb_network_private *network_private = net_device->priv;
        struct usb_device_instance *device;
	struct urb *urb = NULL;
	int len = skb->len;
	int rc = 1;


#if 0
	printk(KERN_INFO"%s: %s len: %d encap: %d\n", __FUNCTION__, net_device->name, skb->len, network_private->encapsulation);
        printk(KERN_INFO"start_xmit: len: %x head: %p data: %p tail: %p\n", 
                        skb->len, skb->head, skb->data, skb->tail);
        {
                __u8 *cp = skb->data;
                int i;
                for (i = 0; i < skb->len; i++) {
                        if ((i%32) == 0) {
                                printk("\ntx[%2x] ", i);
                        }
                        printk("%02x ", *cp++);
                }
                printk("\n");
        }
#endif

	if (!(device = network_private->device)) {
		printk(KERN_INFO"%s: network_private NULL\n", __FUNCTION__);
                THROW(not_ok);
	}

	if (!netif_carrier_ok (net_device)) {
		//printk(KERN_INFO"%s: no carrier %p\n", __FUNCTION__, skb);
                THROW(not_ok);
	}

        // check usb device status
        if (device->status != USBD_OK) {
		//printk(KERN_INFO"%s: !USBD_OK\n", __FUNCTION__);
                THROW(not_ok);
        }

        CATCH(not_ok) {
		dev_kfree_skb_any (skb);
                network_private->stats.tx_dropped++;
		netif_stop_queue(net_device); // XXX
                return 0;
        }

	// stop queue, it will be restart only when we are ready for another skb
	netif_stop_queue (net_device);

	// lock and update some stats
	{
		unsigned long flags;
		write_lock_irqsave (&network_private->rwlock, flags);
		network_private->stopped++;
		network_private->queued_entries++;
		network_private->queued_bytes += skb->len;
		write_unlock_irqrestore (&network_private->rwlock, flags);
	}


	// Set the timestamp for tx timeout
	net_device->trans_start = jiffies;

        switch (network_private->encapsulation) {
#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        case rndis_encapsulation:
                {
                        u32 len = rndis_message_sizeof(skb->len);

                        len = (len % in_pkt_sz) ? len : len + 1;

                        /*
                         * XXX if multiple packets are allowed and desired, it may
                         * be possible to to implement by always producing urbs with
                         * data that is a multiple of the packet size until the
                         * appropriate maximum size is reached.
                         *
                         * Subsequent urbs would simply be sent and windows would
                         * view the result as a single large transfer with multiple
                         * messages.
                         *
                         * We terminate the transfer in one of two ways. First by
                         * allowing through an urb that is not a multiple of the
                         * packet size so that it will generate a short transfer. 
                         *
                         * Or by noticing in network_urb_sent() that there are no
                         * outstanding urbs to be sent, in which case we simply send
                         * a dummy urb with a single NULL data byte.
                         *
                         * This assumes that we have setup appropriate packet size in
                         * rndis enumeration, we pay attention to max transfer size, 
                         * and we allow multiple skbs to be outstanding.
                         *
                         * Most likely this would entail not stopping the queue until
                         * (max transfer - max framesize < max framesize) || (packets > max packets)
                         *
                         */

                        if (!(urb = usbd_alloc_urb (network_private->device, BULK_IN_ENDPOINT, len + 1))) {
                                printk(KERN_INFO"%s: urb alloc failed len: %d\n", __FUNCTION__, skb->len);
                                return -ENOMEM;
                        }
                        rndis_message_copyin((msg_t *) urb->buffer, len, skb->data, skb->len);
                        urb->actual_length = len;
                        break;
                }
#endif                          /* CONFIG_USBD_RNDIS */
#if 0 
        case simple_net:
                //printk(KERN_INFO"%s: BASIC\n", __FUNCTION__);
                {
                        // allocate urb 1 byte larger than required
                        if (!(urb = usbd_alloc_urb (network_private->device, BULK_IN_ENDPOINT, skb->len + 1))) {
                                printk(KERN_INFO"%s: urb alloc failed len: %d\n", __FUNCTION__, skb->len);
                                return -ENOMEM;
                        }

                        // XXX the upper layers can produce non 4-byte aligned data which can
                        // cause some lower layers problems (like smdk2500)

                        // copy skb->len bytes, increment actual_length to ensure short packet
                        memcpy(urb->buffer, skb->data, skb->len);
                        urb->actual_length = (skb->len % in_pkt_sz) ? skb->len : skb->len + 1;
                        break;
                }
#endif
        case simple_crc:
        	    {
                        // allocate urb 1 byte larger than required
                        if (!(urb = usbd_alloc_urb (network_private->device, BULK_IN_ENDPOINT, skb->len + 1))) {
                                printk(KERN_INFO"%s: urb alloc failed len: %d\n", __FUNCTION__, skb->len);
                                return -ENOMEM;
                        }

                        // XXX the upper layers can produce non 4-byte aligned data which can
                        // cause some lower layers problems (like smdk2500)

                        // copy skb->len bytes, increment actual_length to ensure short packet
                        memcpy(urb->buffer, skb->data, skb->len);
                        urb->actual_length = (skb->len % in_pkt_sz) ? skb->len : skb->len + 1;
                        break;
                }
#if 0
                        //printk(KERN_INFO"%s: BASIC_CRC\n", __FUNCTION__);
                {
                        u32 crc;

                        // allocate urb 5 bytes larger than required
                        if (!(urb = usbd_alloc_urb (network_private->device, BULK_IN_ENDPOINT, skb->len + 5 + in_pkt_sz ))) {
                                printk(KERN_INFO"%s: urb alloc failed len: %d\n", __FUNCTION__, skb->len);
                                return -ENOMEM;
                        }

                        // copy and crc skb->len bytes
                        crc = crc32_copy(urb->buffer, skb->data, skb->len, CRC32_INIT);
                        
                        // add a pad byte if required to ensure a short packet, usbdnet driver
                        // will correctly handle pad byte before or after CRC, but the MCCI driver
                        // wants it before the CRC.
                        if ((urb->actual_length % in_pkt_sz) == (in_pkt_sz - 4)) {
                                crc = crc32_pad(urb->buffer + urb->actual_length, 1, crc);
                                urb->actual_length++;
                        }

                        // munge and append crc
                        crc = ~crc;
                        urb->buffer[urb->actual_length++] = crc & 0xff;
                        urb->buffer[urb->actual_length++] = (crc >> 8) & 0xff;
                        urb->buffer[urb->actual_length++] = (crc >> 16) & 0xff;
                        urb->buffer[urb->actual_length++] = (crc >> 24) & 0xff;

                        break;
                }
#endif
        default:
                break;

        }
        if (!urb) {
                printk(KERN_ERR"%s: unknown encapsulation\n", __FUNCTION__);
                return -EINVAL;
        }

        // save skb for netproto_done
	urb->privdata = (void *) skb;
#if 0
        printk(KERN_INFO"start_xmit: len: %d : %d data: %p\n", skb->len, urb->actual_length, urb->buffer);
        {
                __u8 *cp = urb->buffer;
                int i;
                for (i = 0; i < urb->actual_length; i++) {
                        if ((i%32) == 0) {
                                printk("\ntx[%2x] ", i);
                        }
                        printk("%02x ", *cp++);
                }
                printk("\n");
        }
#endif

	if ((rc = usbd_send_urb (urb))) {
		
		urb->privdata = NULL;
		usbd_dealloc_urb (urb);

		switch (rc) {

		case -EINVAL:
		case -EUNATCH:
			printk(KERN_INFO"%s: not attached, send failed: %d\n", __FUNCTION__, rc);
                        network_private->stats.tx_errors++;
                        network_private->stats.tx_carrier_errors++;
			netif_wake_queue (net_device);
			break;

		case -ENOMEM:
			printk(KERN_INFO"%s: no mem, send failed: %d\n", __FUNCTION__, rc);
                        network_private->stats.tx_errors++;
                        network_private->stats.tx_fifo_errors++;
			netif_wake_queue (net_device);
			break;

		case -ECOMM:
			printk(KERN_INFO"%s: comm failure, send failed: %d %p\n", __FUNCTION__, rc, net_device);
                        network_private->stats.tx_dropped++;
			break;

		}
		dev_kfree_skb_any (skb);
                return -ECOMM;
	}
        else {
		// XXX should we restart network queue
                network_private->stats.tx_packets++;
                network_private->stats.tx_bytes += len;

                read_lock (&network_private->rwlock);
                if ((network_private->queued_entries < network_private->max_queue_entries) && 
                                (network_private->queued_bytes < network_private->max_queue_bytes)) 
                {
                        read_unlock (&network_private->rwlock);
                        netif_wake_queue (net_device);
                }
                else {
                        read_unlock (&network_private->rwlock);
                }
        }
        return 0;
}

//_________________________________________________________________________________________________
//______________________________________Network IOCTL function_____________________________________0


//_________________________________________________________________________________________________
//                                      network_do_ioctl

/**
 * Carries out IOCTL commands for the specified network device.
 *
 * @param net_device    Specifies the device by pointing to its net_device struct.
 * @param rp            Points to an ifreq structure containing the IOCTL parameter(s).
 * @param cmd           The IOCTL command.
 *
 * @return Zero - success; -ENOIOCTLCMD - command not implemented; non-zero - command failed.
 */
static int network_do_ioctl (struct net_device *net_device, struct ifreq *rp, int cmd)
{
	//printk(KERN_INFO"%s: %s cmd=%d\n", __FUNCTION__, net_device->name, cmd);
	return -ENOIOCTLCMD;
}


//_________________________________________________________________________________________________
//______________________________________Intialize/attach and detach/destroy functions______________

//_________________________________________________________________________________________________
//                                      network_attach

/**
 * Attaches the specified USB device to the network device pointed to indirectly by
 * @c network_private. It then starts the network device by calling @c netif_carrier_on and
 * @c netif_wake_queue.
 *
 * @param device        Specifies the USB device by pointing to its @c usb_device_instance
 *                      structure.
 * @param function      This parameter appears to be redundant.
 *                      Just prior to the only existing call to @c network_attach, @a function
 *                      is set equal to @a device -> @c active_function_instance.
 */
static void network_attach (struct usb_device_instance *device,
                        struct usb_function_instance *function,
                        struct usb_network_private *network_private
                        )
{
	//printk(KERN_INFO"%s:\n", __FUNCTION__);

        network_private->flags |= NETWORK_ATTACHED;

        netif_carrier_on (&network_net_device);
        netif_wake_queue (&network_net_device);
}

//_________________________________________________________________________________________________
//                                      network_create

struct usb_network_private usb_network_private;
/*
 * Currently only one...
 */
static struct usb_network_private *Network_private;
//struct net_device_stats network_net_device_stats;  // network device statistics

struct net_device network_net_device = {
        get_stats: network_get_stats,
        tx_timeout: network_tx_timeout,
        do_ioctl: network_do_ioctl,
        set_config: network_set_config,
        set_mac_address: network_set_mac_addr,
        hard_start_xmit: network_hard_start_xmit,
        change_mtu: network_change_mtu,
        init: network_init,
        uninit: network_uninit,
        open: network_open,
        stop: network_stop,
        priv: &usb_network_private,
};

/**
 * Allocates a @c usb_network_private structure, which includes a @c net_device structure,
 * and initializes them. It stops the network device by calling @c netif_stop_queue and
 * @c netif_carrier_off. 
 *
 * @return network_private - success; NULL - either the structure could not be allocated or
 *         registering the network device failed.
 *
 */
static struct usb_network_private * network_create (void)
{
	struct usb_network_private *network_private = &usb_network_private;

	// Set some fields to generic defaults and register the network device with the kernel networking code

        ether_setup (&network_net_device);
        if (register_netdev(&network_net_device)) {
                return NULL;
        }

        netif_stop_queue (&network_net_device);
        netif_carrier_off (&network_net_device);

        network_private->flags |= NETWORK_CREATED;
        network_private->rwlock = RW_LOCK_UNLOCKED;

        network_private->maxtransfer = MAXFRAMESIZE + 4 + in_pkt_sz;

        network_private->flags |= NETWORK_REGISTERED;

        network_private->network_type = network_unknown;

        return network_private;
}

//_________________________________________________________________________________________________
//                                      network_detach

/**
 * Detaches the specified USB device from the network device pointed to indirectly by
 * @c network_private. It stops the network device by calling @c netif_stop_queue and
 * @c netif_carrier_off.
 *
 * @warning This function has some serious unresolved issues.
 */
static void network_detach (struct usb_device_instance *device)
{
        struct usb_network_private *network_private;
        unsigned long flags;

	//printk(KERN_INFO"%s: device: %p\n", __FUNCTION__, device);

        // Return if argument is null.
        if (!device || !device->active_function_instance ||
            !(network_private = device->active_function_instance->privdata)) {
                return;
        }

        network_private->flags &= ~NETWORK_ATTACHED;

        /*
         * Disable our net-device.
         * Apparently it doesn't matter if we should do this more than once.
         */
        netif_stop_queue(&network_net_device);
        netif_carrier_off(&network_net_device);

        // Lock everybody out.
        write_lock_irqsave(&network_rwlock, flags);

        // If we aren't already tearing things down, do it now.
        if (!(network_private->flags & NETWORK_DESTROYING)) {
                network_private->flags |= NETWORK_DESTROYING;
                //network_private->device = NULL;
        }

        // Release the lock.
        write_unlock_irqrestore(&network_rwlock, flags);
}

//_________________________________________________________________________________________________
//                                      network_destroy

/**
 * Destroys the network interface referenced by the global variable @c network_private.
 */
static void network_destroy (void)
{
	//printk(KERN_INFO"%s: %p\n", __FUNCTION__, Network_private);

        if (Network_private) {
                printk(KERN_INFO"%s: flags: %x\n", __FUNCTION__, Network_private->flags);

                if (Network_private->flags & NETWORK_REGISTERED) {
                        netif_stop_queue (&network_net_device);
                        netif_carrier_off (&network_net_device);
                        unregister_netdev (&network_net_device);
                }
                Network_private->flags = 0;
        }

#ifdef CONFIG_HOTPLUG
        //while (Network_private->hotplug_bh.data) {
        while (hotplug_active) {
                printk(KERN_INFO"%s: waiting for hotplug bh\n", __FUNCTION__);
                schedule_timeout(10 * HZ);
        }
#endif
        //printk(KERN_INFO"%s: finished\n", __FUNCTION__);
}

//_________________________________________________________________________________________________
//______________________________________USB Function Initialization________________________________

//_________________________________________________________________________________________________
//                                      network_function_init

/**
 * Does something. XXX
 *
 * @param bus                   XXX
 * @param device                XXX
 * @param function_driver       This parameter is not referenced.
 *
 * @note According to <i>usbdfunc(7)</i>:
 *       The <i>USB Device</i> core layer will call <i>function_init</i> after a USB
 *       bus interface driver has called to create a new logical USB device.
 */

static void network_function_init (struct usb_bus_instance *bus,
			       struct usb_device_instance *device,
			       struct usb_function_driver *function_driver,
                               struct usb_function_instance *function_instance)
{
	u32 serial;		// trim for use in MAC addr
        struct usb_class_ethernet_networking_descriptor *ethernet;

	//if (!(function = device->active_function_instance)) {
	//	return;
	//}

        // use bus device addr for local or remote as appropriate if available
        if (memcmp(bus->dev_addr, zeros, 6)) {
                memcpy( Network_private->network_type == network_blan ? 
                                local_dev_addr : remote_dev_addr , bus->dev_addr, ETH_ALEN);
        }

        // set the network device address from the local device address
        memcpy(network_net_device.dev_addr, local_dev_addr, ETH_ALEN);

        function_instance->privdata = Network_private;
        Network_private->device = device;


#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        // initialize rndis, give it the remote mac address
        rndis_initialize(remote_dev_addr, sizeof(remote_dev_addr));
#endif                          /* CONFIG_USBD_RNDIS */



#if defined(CONFIG_USBD_NETWORK_CDC) || defined(CONFIG_USBD_NETWORK_SAFE) 
        // Update the iMACAddress field in the ethernet descriptor
        {
                char address_str[14];
                snprintf(address_str, 13, "%02x%02x%02x%02x%02x%02x",
                                remote_dev_addr[0], remote_dev_addr[1], remote_dev_addr[2], 
                                remote_dev_addr[3], remote_dev_addr[4], remote_dev_addr[5]);
#if 0
#if defined(CONFIG_USBD_NETWORK_CDC)
                if ((ethernet = (struct usb_class_ethernet_networking_descriptor *)cdc_class_2)) {
                        if (ethernet->iMACAddress) {
                                usbd_dealloc_string(ethernet->iMACAddress);
                        }
                        ethernet->iMACAddress = usbd_alloc_string(address_str);
                }
#endif
#endif
#if defined(CONFIG_USBD_NETWORK_SAFE)
                if ((ethernet = (struct usb_class_ethernet_networking_descriptor *)safe_class_4)) {
                        if (ethernet->iMACAddress) {
                                usbd_dealloc_string(ethernet->iMACAddress);
                        }
                        ethernet->iMACAddress = usbd_alloc_string(address_str);
                }
#endif
        }
#endif
#if defined(CONFIG_USBD_NETWORK_BLAN)
        // Update the iMACAddress field in the ethernet descriptor
        {
                char address_str[14];
                snprintf(address_str, 13, "%02x%02x%02x%02x%02x%02x",
                                local_dev_addr[0], local_dev_addr[1], local_dev_addr[2], 
                                local_dev_addr[3], local_dev_addr[4], local_dev_addr[5]);

#if defined(CONFIG_USBD_NETWORK_BLAN)
                if ((ethernet = (struct usb_class_ethernet_networking_descriptor *)blan_class_4)) {
                        if (ethernet->iMACAddress) {
                                usbd_dealloc_string(ethernet->iMACAddress);
                        }
                        ethernet->iMACAddress = usbd_alloc_string(address_str);
                }
#endif
        }
#endif

#ifdef CONFIG_USBD_NET_NFS_SUPPORT
	init_waitqueue_head( &usb_netif_wq );
        //network_attach (device, device->active_function_instance, Network_private);
#endif

}

//_________________________________________________________________________________________________
//                                      network_function_exit

/**
 * Just calls @c network_destroy.
 *
 * @note According to <i>usbdfunc(7)</i>:
 *       The <i>USB Device</i> core layer will call <i>function_exit</i> after a USB
 *       bus interface driver has called to destroy a new<i>[sic]</i> logical USB device.
 *
 * @sa network_destroy
 */
static void network_function_exit (struct usb_device_instance *device)
{
        struct usb_class_ethernet_networking_descriptor *ethernet;
	//printk(KERN_INFO"%s: %p\n", __FUNCTION__, Network_private);

	network_destroy ();
}

//_________________________________________________________________________________________________
//______________________________________Handle USB Events__________________________________________

/**
 * Processes a USB event.
 *
 * @param device        Pointer to the @c usb_device_instance structure.
 * @param event         Event descriptor.
 * @param data          Somebody's private data. Haven't determined whose yet.
 *
 * @note This is called for each USB event.
 * @note NOT CALLED from interrupt context.
 * @note According to <i>usbdfunc(7)</i>:
 *       This function is called by the <i>USB Device</i> core layer to pass various events to
 *       the <i>Function</i> driver. These match the changes defined in the state diagram defined
 *       in <i>chapter 9 USB Device Framework, section 9.1.1</i>, in the USB specification. 
 *
 *       XXX NOW IN INTERRUPT CONTEXT
 *       XXX The
 *       XXX events are passed through a <i>task queue</i>. This means that these functions are not
 *       XXX executing in an interrupt context and may call other kernel functions that may sleep.
 */
static void network_event_irq (struct usb_device_instance *device, usb_device_event_t event, int data)
{
	struct usb_function_instance *function;
	struct usb_network_private *network_private;

	if (!(function = device->active_function_instance)) {
		return;
	}

        //printk(KERN_INFO"%s: %d data: %d privdata: %p\n", __FUNCTION__, event, data, function->privdata);

	switch (event) {
	case DEVICE_UNKNOWN:
	case DEVICE_INIT: 
	printk(KERN_INFO"%s: INIT \n", __FUNCTION__);
		break;
		
	case DEVICE_CREATE:     
	printk(KERN_INFO"%s: CREATE \n", __FUNCTION__);
		break;
		
	case DEVICE_ADDRESS_ASSIGNED:
		printk(KERN_INFO"%s: ADDRESS_ASSIGNED \n", __FUNCTION__);
		break;
		
	case DEVICE_SET_INTERFACE:
	printk(KERN_INFO"%s: SET_INTERFACE \n", __FUNCTION__);
		break;
	
	case DEVICE_SET_FEATURE:
	printk(KERN_INFO"%s: SET_FEATURE \n", __FUNCTION__);
		break;
	
	case DEVICE_CLEAR_FEATURE:
	printk(KERN_INFO"%s: CLEAR_FEATURE \n", __FUNCTION__);
		break;
	
	case DEVICE_POWER_INTERRUPTION:
	printk(KERN_INFO"%s: POWER_INTERRUPTION \n", __FUNCTION__);
		break;
	
	case DEVICE_HUB_RESET:
	printk(KERN_INFO"%s: HUB_RESET \n", __FUNCTION__);
		break;
	
	case DEVICE_HUB_CONFIGURED:
	printk(KERN_INFO"%s: HUB_CONFIGURED \n", __FUNCTION__);
		break;
		
	case DEVICE_FUNCTION_PRIVATE:
	printk(KERN_INFO"%s: FUNCTION_PRIVATE \n", __FUNCTION__);
		break;
	              	
	case DEVICE_RESET:
                printk(KERN_INFO"%s: RESET \n", __FUNCTION__);
		break;
	case DEVICE_DESTROY:	
                printk(KERN_INFO"%s: DESTROY\n", __FUNCTION__);
                network_detach (device);
                printk(KERN_INFO"%s: detach finished\n", __FUNCTION__);
		break;

	case DEVICE_CONFIGURED:
                printk(KERN_INFO"%s: CONFIGURED\n", __FUNCTION__);
#ifdef CONFIG_USBD_NET_NFS_SUPPORT
		if ( !usb_is_configured ) {
			wake_up( &usb_netif_wq );
			usb_is_configured = 1;
		}
#endif
                network_attach (device, function, Network_private);
		break;

        case DEVICE_BUS_INACTIVE:
                printk(KERN_INFO"%s: BUS INACTIVE\n", __FUNCTION__);
#ifndef CONFIG_USBD_NET_NFS_SUPPORT
                printk(KERN_INFO"%s: BUS INACTIVE\n", __FUNCTION__);
                if ((network_private = function->privdata) && !(network_private->flags & NETWORK_DESTROYING) ) {
                        printk(KERN_INFO"%s: BUS_INACTIVE - network down\n", __FUNCTION__);
                        netif_stop_queue (&network_net_device);
                        netif_carrier_off (&network_net_device);
                }
#endif
                break;

        case DEVICE_BUS_ACTIVITY:
                printk(KERN_INFO"%s: BUS ACTIVITY\n", __FUNCTION__);
#ifndef CONFIG_USBD_NET_NFS_SUPPORT
                printk(KERN_INFO"%s: BUS ACTIVITY\n", __FUNCTION__);
                if ((network_private = function->privdata) && !(network_private->flags & NETWORK_DESTROYING) ) {
                        printk(KERN_INFO"%s: BUS_ACTIVITY - network up\n", __FUNCTION__);
                        netif_carrier_on (&network_net_device);
                        netif_wake_queue (&network_net_device);
                }
#endif
                break;
        default:
                break;
        }
#ifdef CONFIG_HOTPLUG
        hotplug_schedule_bh(Network_private);
#endif
}

//_________________________________________________________________________________________________
//                                      network_send_int

/**
 * Generates a response urb on the notification (INTERRUPT) endpoint.
 * Return a non-zero result code to STALL the transaction.
 * CALLED from interrupt context.
 *
 * @param device        Pointer to the usb_device_instance structure.
 *
 * @return 0 - success; -EINVAL error.
 */
static int network_send_int(struct usb_device_instance *device )
{
        struct urb *urb;

        /*
         * get an urb, fill with 0x01 0x00 0x00 0x00, and send it
         */
	if (!  (urb = usbd_alloc_urb (device, INT_IN_ENDPOINT, 4))) {
		printk(KERN_INFO"%s: urb alloc failed len: %d\n", __FUNCTION__, 4);
		return -EINVAL;
	}
        memset(urb->buffer, 0, 4);
        urb->buffer[0] = 1;
        urb->actual_length = 4;

	switch (usbd_send_urb (urb)) {
        case 0:
                return 0;
        case -EINVAL:
        case -EUNATCH:
                printk(KERN_INFO"%s: not attached, send failed: EINVAL or EUNATCH\n", __FUNCTION__);
                break;
        case -ENOMEM:
                printk(KERN_INFO"%s: no mem, send failed: ENOMEM\n", __FUNCTION__);
                break;
        case -ECOMM:
                printk(KERN_INFO"%s: comm failure, send failed: ECOMM\n", __FUNCTION__);
                break;
        default:
                printk(KERN_INFO"%s: usbd_send_urb failed\n", __FUNCTION__);
                break;
        }

        urb->privdata = NULL;
        usbd_dealloc_urb (urb);
        return -EINVAL;
}

//_________________________________________________________________________________________________
//                                      network_recv_setup

/**
 * Processes a received setup packet and CONTROL WRITE data.
 * Results for a CONTROL READ are placed in urb->buffer.
 *
 * @param urb   Pointer to the received urb.
 *
 * @return 0 - success; XXX
 *
 * @note Return a non-zero result code to STALL the transaction.
 * @note CALLED from interrupt context.
 * @note According to <i>usbdfunc(7)</i>:
 *       The <i>USB Device</i> core layer will use the <i>recv_setup</i> function to pass
 *       received setup packets (from the USB host via an OUT transaction) to the <i>Function</i>
 *       driver.
 */
static int network_recv_setup (struct urb *urb)
{
	struct usb_device_instance *device = urb->device;
	struct usb_device_request *request = &urb->device_request;
	struct usb_function_instance *function;
	struct usb_network_private *network_private;

        // Verify that this is a USB Class request per RNDIS specification
        // or a vendor request.
        if (!(request->bmRequestType & (USB_REQ_TYPE_CLASS | USB_REQ_TYPE_VENDOR))) {
                //printk(KERN_INFO"%s: not class or vendor\n", __FUNCTION__);
                return 0;
        }       

        // sanity checks
        if (!urb || !(device = urb->device) || !(function = device->active_function_instance)  ||
                        !(network_private = function->privdata) 
                        ) 
        {
                //printk(KERN_INFO"%s: NULL found\n\n", __FUNCTION__);
                return -EINVAL;
        }

        //printk(KERN_INFO"%s: u: %p b: %p l: %d m: %p\n\n", __FUNCTION__, 
        //                urb, urb->buffer, urb->actual_length, network_private->rndis_msg);

        // Determine the request direction and process accordingly
        switch (request->bmRequestType & (USB_REQ_DIRECTION_MASK | USB_REQ_TYPE_MASK)) {

        case USB_REQ_HOST2DEVICE | USB_REQ_TYPE_VENDOR:

                //printk(KERN_INFO"%s: H2D VENDOR\n", __FUNCTION__);
                switch (request->bRequest) {
                case MCCI_ENABLE_CRC:
                        //printk(KERN_INFO"%s: Enabling CRC\n", __FUNCTION__);
                        if (make_crc_table()) {
                                return -EINVAL;
                        }
                        Network_private->encapsulation = simple_crc;
                        return 0;

                case BELCARRA_SETTIME:
                        {
                                struct timeval tv;

                                // wIndex and wLength contain RFC868 time - seconds since midnight 1 jan 1900

                                tv.tv_sec = ntohl( request->wValue << 16 | request->wIndex);
                                tv.tv_usec = 0;

                                // convert to Unix time - seconds since midnight 1 jan 1970
                                
                                tv.tv_sec -= RFC868_OFFSET_TO_EPOCH; 

                                //printk(KERN_INFO"%s: H2D VENDOR TIME: %08x\n", __FUNCTION__, tv.tv_sec);

                                // set the time
                                do_settimeofday(&tv);
                                break;
                        }

                case BELCARRA_SETIP:
                        ip_addr = ntohl( request->wValue << 16 | request->wIndex);
                        //printk(KERN_INFO"%s: H2D VENDOR IP: %08x\n", __FUNCTION__, ip_addr);
                        break;

                case BELCARRA_SETMSK:
                        network_mask = ntohl( request->wValue << 16 | request->wIndex);
                        //printk(KERN_INFO"%s: H2D VENDOR network: %08x\n", __FUNCTION__, network_mask);
                        break;

                case BELCARRA_SETROUTER:
                        router_ip = ntohl( request->wValue << 16 | request->wIndex);
                        //printk(KERN_INFO"%s: H2D VENDOR router: %08x\n", __FUNCTION__, router_ip);
                        break;

                case BELCARRA_SETDNS:
                        dns_server_ip = ntohl( request->wValue << 16 | request->wIndex);
                        //printk(KERN_INFO"%s: H2D VENDOR dns: %08x\n", __FUNCTION__, dns_server_ip);
                        break;
                }
                return 0;

        case USB_REQ_DEVICE2HOST | USB_REQ_TYPE_VENDOR:
                //printk(KERN_INFO"%s: D2H VENDOR\n", __FUNCTION__);
                urb->actual_length = 0;
                switch (request->bRequest) {
                case BELCARRA_GETMAC:
                        {
                                // copy and free the original buffer
                                memcpy(urb->buffer, network_net_device.dev_addr, ETH_ALEN);
                                urb->actual_length = ETH_ALEN;
                                return 0;
                        }
                }
                return 0;

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        case USB_REQ_HOST2DEVICE | USB_REQ_TYPE_CLASS:
                //printk(KERN_INFO"%s: H2D CLASS\n", __FUNCTION__);
                //

                // in case we get multiple messages
                if (Network_private->rndis_msg) {
                        lkfree(Network_private->rndis_msg);
                }

                /* process urb buffer as an RNDIS message and save the resulting message for
                 * the next DEVICE2HOST request
                 */
                Network_private->rndis_msg = rndis_create_response_message((msg_t *) urb->buffer, urb->actual_length);

                // send a notification to host via notification endpoint
                return network_send_int(device);

        case USB_REQ_DEVICE2HOST | USB_REQ_TYPE_CLASS:
                //printk(KERN_INFO"%s: D2H CLASS\n", __FUNCTION__);

                // copy the previously created RNDIS message to urb buffer
                if (Network_private->rndis_msg) {

                        int len = rndis_message_size(Network_private->rndis_msg);

                        // activate rndis network mode
                        Network_private->encapsulation = rndis_encapsulation;

                        // copy and free the original buffer
                        memcpy(urb->buffer, Network_private->rndis_msg, MIN(len, urb->buffer_length));
                        lkfree(Network_private->rndis_msg);
                        Network_private->rndis_msg = NULL;
                        urb->actual_length = len;
                        return 0;
                }
#endif                          /* CONFIG_USBD_RNDIS */
        default:
                // return failure if no message or message too long
                break;
        }
        return -EINVAL;
}

//_________________________________________________________________________________________________
//                                      network_recv

/**
 * Passes received data to the network layer.
 * Passes skb to network layer.
 *
 * @param network_private private data structure XXX
 * @param net_device    network data structure XXX
 * @param skb           skb pointer XXX
 *
 * @note NOT CALLED from interrupt context.
 *
 */
static __inline__ int network_recv (struct usb_network_private *network_private, 
                struct net_device *net_device, struct sk_buff *skb)
{
	int rc;
#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MYDHCPD)
        __u8 *dhcp;
        int length;
#endif
#if 0
        printk(KERN_INFO"%s: len: %x head: %p data: %p tail: %p\n", __FUNCTION__, 
                        skb->len, skb->head, skb->data, skb->tail);
        {
                __u8 *cp = skb->data;
                int i;
                for (i = 0; i < skb->len; i++) {
                        if ((i%32) == 0) {
                                printk("\nrx[%2x] ", i);
                        }
                        printk("%02x ", *cp++);
                }
                printk("\n");
        }
#endif

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MYDHCPD)
        /*
         * check received frame for dhcp request, if found send reply
         * back via network_hard_start_xmit
         */
        if ((dhcp = checkfordhcp(net_device, skb->data, skb->len, &length))) {
                dev_kfree_skb_any (skb);

                // allocate skb of appropriate length, reserve 2 to align ip
                if (!(skb = dev_alloc_skb (length + 2))) {
                        printk(KERN_INFO"%s: cannot malloc skb\n", __FUNCTION__);
                        lkfree(dhcp);
                        return 0;
                }       
                skb_reserve(skb, 2);
                memcpy (skb_put (skb, length), dhcp, length);

                lkfree(dhcp);

                // XXX what if queue is stopped?
                if (network_hard_start_xmit (skb, net_device)) {
                        dev_kfree_skb_any (skb);
                }
                return 0;
        }
#endif

        // refuse if no device present
        if (!netif_device_present (net_device)) {
                printk(KERN_INFO"%s: device not present\n", __FUNCTION__);
                return -EINVAL;
        }

        // refuse if no carrier
        if (!netif_carrier_ok (net_device)) {
                printk(KERN_INFO"%s: no carrier\n", __FUNCTION__);
		return -EINVAL;
	}

	// refuse if the net device is down
	if (!(net_device->flags & IFF_UP)) {
		//printk(KERN_INFO"%s: not up net_dev->flags: %x\n", __FUNCTION__, net_device->flags);
		network_private->stats.rx_dropped++;
		return -EINVAL;
	}

	skb->dev = net_device;
	skb->pkt_type = PACKET_HOST;
	skb->protocol = eth_type_trans (skb, net_device);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

        //printk(KERN_INFO"%s: len: %x head: %p data: %p tail: %p\n", __FUNCTION__, 
        //                skb->len, skb->head, skb->data, skb->tail);


	// pass it up to kernel networking layer
	if ((rc = netif_rx (skb))) {
		//printk(KERN_INFO"%s: netif_rx rc: %d\n", __FUNCTION__, rc);
	}
        network_private->stats.rx_bytes += skb->len;
        network_private->stats.rx_packets++;

	return 0;
}

//_________________________________________________________________________________________________
//                                      network_recv_urb

/**
 * Processes a received data[sic].
 *
 * @param urb   A pointer to the received urb.
 *
 * @return 0 - success; -EINVAL - failure.
 *
 * @note According to <i>usbdfunc(7)</i>:
 *       The <i>USB Device</i> core layer will use the <i>recv_urb</i> function to pass
 *       received [data? urbs?] (from the USB host via an OUT transaction) to the <i>Function</i>
 *       driver.
 * @note A non-zero result code will STALL the transaction.
 * @note NOT CALLED from interrupt context.
 */
static int network_recv_urb (struct urb *urb)
{
	struct usb_device_instance *device = NULL;
	struct usb_function_instance *function = NULL;
	struct usb_network_private *network_private = NULL;
        struct net_device *net_device = NULL;

	struct sk_buff *skb = NULL;

        //printk(KERN_INFO"%s: urb: %p device: %p\n", __FUNCTION__, urb, urb->device);

        if (!urb || !(device = urb->device) || 
                        !(function = device->active_function_instance) ||
                        !(network_private = function->privdata) ||
                        !(net_device = &network_net_device) ||
                        !(network_private->flags & NETWORK_ATTACHED)
                        )
        {
                printk(KERN_INFO"%s: device: %p function: %p network_private: %p net_device: %p\n", 
                                __FUNCTION__, device, function, network_private, net_device);
                return -EINVAL;
        }


        network_private->stats.rx_missed_errors += urb->dropped_errors;
        network_private->stats.rx_over_errors += urb->previous_errors;


#if 0 
        printk(KERN_INFO"%s: urb: %p len: %d maxtransfer: %d encap: %d\n", __FUNCTION__, 
                        urb, urb->actual_length, network_private->maxtransfer, network_private->encapsulation);

        {
                int i = urb->actual_length - 32;
                __u8 *cp = urb->buffer + i;

                printk("\n[%4x] ", i);

                for (; i < urb->actual_length; i++) {
                        printk("%02x ", *cp++);
                }
                printk("\n");
        }
#endif
#if 0
        printk(KERN_INFO"%s: urb: %p len: %d maxtransfer: %d encap: %d\n", __FUNCTION__, 
                        urb, urb->actual_length, network_private->maxtransfer, network_private->encapsulation);

        {
                __u8 *cp = urb->buffer;
                int i;
                for (i = 0; i < urb->actual_length; i++) {
                        if ((i%32) == 0) {
                                printk("\n[%2x] ", i);
                        }
                        printk("%02x ", *cp++);
                }
                printk("\n");
        }
#endif

        if (urb->status != RECV_OK) {
                printk(KERN_INFO"%s: !RECV_OK urb->actual_length: %d maxtransfer: %d\n", __FUNCTION__, 
                                urb->actual_length, network_private->maxtransfer);
                THROW(error);
        }

        // Is RNDIS active (we have received CONTROL WRITE setup packets indicating real RNDIS host)
        switch (network_private->encapsulation) {
                int len;
#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
                u32 pkt_offset;
                u32 pkt_count;

        case rndis_encapsulation:

                if (!rndis_message_type_data((msg_t *)urb->buffer, urb->actual_length, 0)) {
                        //printk(KERN_INFO"%s: not a DATA packet - wrong type actual_length: %d\n", 
                        //                __FUNCTION__, urb->actual_length);
                        //printk(KERN_INFO"%s: urb: %p len: %d maxtransfer: %d encapsulation: %d\n", __FUNCTION__, 
                        //                urb, urb->actual_length, network_private->maxtransfer, 
                        //                network_private->encapsulation);
                        THROW(crc_error);
                }

                if (!(pkt_count = rndis_message_packets((msg_t *) urb->buffer, urb->actual_length))) {
                        //printk(KERN_INFO"%s: not a DATA packet - no packets actual_length: %d\n", 
                        //                __FUNCTION__, urb->actual_length);
                        //printk(KERN_INFO"%s: urb: %p len: %d maxtransfer: %d encapsulation: %d\n", __FUNCTION__, 
                        //                urb, urb->actual_length, network_private->maxtransfer, 
                        //                network_private->encapsulation);
                        THROW(fifo_error);
                }

                //printk(KERN_INFO"%s: pkt_count: %d\n", __FUNCTION__, pkt_count);

                for (pkt_offset = 0; pkt_offset < pkt_count; pkt_offset++) {

                        u32 len;
                        pkt_t *request_packet;

                        //printk(KERN_INFO"%s: rndis packet:  %d type %d\n", __FUNCTION__, 
                        //                pkt_offset, rndis_message_type((msg_t *) urb->buffer, 
                        //                urb->actual_length, pkt_offset));


                        // get address of request from rndis message
                        request_packet = rndis_message_packet((msg_t *) urb->buffer, urb->actual_length, pkt_offset);


                        // is this a data packet?
                        if (rndis_message_type((msg_t *) urb->buffer, urb->actual_length, pkt_offset) == REMOTE_NDIS_PACKET_MSG) 
                        {

                                // what is length of data in request
                                if ((len = rndis_packet_data_length(request_packet))) {

                                        //printk(KERN_INFO"%s: rndis packet: %d len: %d offset: %d\n", __FUNCTION__, 
                                        //                pkt_offset, len, rndis_packet_data_offset(request_packet));

                                        //return 0;

                                        // allocate skb of appropriate length, reserve 2 to align ip

                                        if (!(skb = dev_alloc_skb (len + 2))) {
                                                printk(KERN_INFO"%s: cannot malloc skb\n", __FUNCTION__);
                                                THROW(error);
                                        }
                                        skb_reserve(skb, 2);

                                        //printk(KERN_INFO"len: %x len: %x head: %p data: %p tail: %p\n", 
                                        //                len, skb->len, skb->head, skb->data, skb->tail);

                                        // copy data from request packet to skb
                                        memcpy(skb_put(skb, len), (char *) request_packet + 
                                                        rndis_packet_data_offset(request_packet), len);

                                        //printk(KERN_INFO"len: %x len: %x head: %p data: %p tail: %p\n", 
                                        //                len, skb->len, skb->head, skb->data, skb->tail);

                                        //dbgPRINTmem (0, 0, skb->data, skb->len);

                                        // pass it up, free skb if non zero
                                        if (network_recv (network_private, net_device, skb)) {
                                                THROW(skb_error);
                                        }
                                }
                                else {
                                        printk(KERN_INFO"%s: packet %d of %d len: %d failed\n", __FUNCTION__,
                                                        pkt_offset, pkt_count, len);
                                        network_private->stats.rx_dropped++;
                                }
                        }
                        // or some other type of message
                        else {
                                msg_t *rndis_msg;

                                //printk(KERN_INFO"%s: rndis packet: control\n", __FUNCTION__);

                                rndis_msg = rndis_create_response_message((msg_t *) urb->buffer, urb->actual_length);

                                if (rndis_msg) {

                                        int len = rndis_message_size(rndis_msg);
                                        struct urb *reply_urb;

                                        if (!(reply_urb = usbd_alloc_urb (network_private->device, BULK_IN_ENDPOINT, skb->len))) {
                                                printk(KERN_INFO"%s: urb alloc failed len: %d\n", __FUNCTION__, skb->len);
                                                THROW(error);
                                        }

                                        // copy and free the original buffer
                                        memcpy(reply_urb->buffer, rndis_msg, len);

                                        lkfree(rndis_msg);

                                        rndis_msg = NULL;
                                        urb->actual_length = len;
                                        if (usbd_send_urb (reply_urb)) {
                                                THROW(error);
                                        }
                                }
                        }
                }
                break;

#endif                          /* CONFIG_USBD_RNDIS */
#if 0 
        case simple_net: 

                // Not RNDIS, hopefully usbnet on Linux (or other) host
                len = urb->actual_length;

                // allocate skb of appropriate length, reserve 2 to align ip

                if (!(skb = dev_alloc_skb (len + 2))) {
                        printk(KERN_INFO"%s: cannot malloc skb\n", __FUNCTION__);
                        THROW(error);
                }       
                skb_reserve(skb, 2);

                memcpy (skb_put (skb, urb->actual_length), urb->buffer, urb->actual_length);

                // pass it up, free skb if non zero
                if (network_recv (network_private, net_device, skb)) {
                        THROW(skb_error);
                }
                break;
#endif
        case simple_crc:

                len = urb->actual_length;

               // printk(KERN_INFO "simple_crc1!\n");
                // allocate skb of appropriate length, reserve 2 to align ip
                if (!(skb = dev_alloc_skb (len+2))) {
                        printk(KERN_INFO"%s: cannot malloc skb\n", __FUNCTION__);
                        THROW(error);
                }
                skb_reserve(skb, 2);

                /* 
                 * The CRC can be sent in two ways when the size of the transfer 
                 * ends up being a multiple of the packetsize:
                 *
                 *                                           |
                 *                <data> <CRC><CRC><CRC><CRC>|<???>     case 1
                 *                <data> <NUL><CRC><CRC><CRC>|<CRC>     case 2
                 *           <data> <NUL><CRC><CRC><CRC><CRC>|          case 3
                 *     <data> <NUL><CRC><CRC><CRC>|<CRC>     |          case 4
                 *                                           |
                 *        
                 * This complicates CRC checking, there are four scenarios:
                 *
                 *      1. length is 1 more than multiple of packetsize with a trailing byte
                 *      2. length is 1 more than multiple of packetsize 
                 *      3. length is multiple of packetsize
                 *      4. none of the above
                 *
                 * Finally, even though we always compute CRC, we do not actually throw
                 * things away until and unless we have previously seen a good CRC.
                 * This allows backwards compatibility with hosts that do not support
                 * adding a CRC to the frame.
                 *
                 */
#if 0
                crc_checks[len%64]++;

                // test if 1 more than packetsize multiple
                if (1 == (len % out_pkt_sz)) {

                        // copy and CRC up to the packetsize boundary
                        u32 crc = crc32_copy(skb_put(skb, len - 1), urb->buffer, len - 1, CRC32_INIT);

                        // if the CRC is good then this is case 1
                        if (CRC32_GOOD != crc) {

                                crc = crc32_copy(skb_put(skb, 1), urb->buffer + len - 1, 1, crc);

                                if (CRC32_GOOD != crc) {
                                        crc_errors[len%64]++;
                                        //printk(KERN_INFO"%s: A CRC error %08x\n", __FUNCTION__, crc);
                                        THROW_IF(network_private->crc, crc_error);
                                }
                                else {
                                        network_private->crc = 1;
                                }
                        }
                        else {
                                network_private->crc = 1;
                        }
                }
                else {
                        u32 crc = crc32_copy(skb_put(skb, len), urb->buffer, len, CRC32_INIT);

                        if (CRC32_GOOD != crc) {
                                crc_errors[len%64]++;
                                THROW_IF(network_private->crc, crc_error);
                        }
                        else {
                                network_private->crc = 1;
                        }
                }
		if (network_private->crc) {
                	skb_trim(skb, skb->len - 4);
		}
#endif
                //memcpy(skb_put(skb, len), urb->buffer, len);
                memcpy (skb_put (skb, urb->actual_length), urb->buffer, urb->actual_length);
                // pass it up, free skb if non zero
                if (network_recv (network_private, net_device, skb)) {
                        THROW(skb_error);
                }
//                printk(KERN_INFO "simple_crc2!\n");
                break;
        default:
                break;
        }

        usbd_recycle_urb (urb);
        return 0;

        // catch a simple error, just increment missed error and general error
        CATCH(error) {

                network_private->stats.rx_frame_errors++;
                network_private->stats.rx_errors++;

                // catch error where skb may need to be released
                CATCH(skb_error) {

                        // catch a CRC error
                        
                        // XXX We need to track whether we have seen a correct CRC, until then 
                        // we ignore CRC errors.

                        CATCH(crc_error) {
#if 0
                                printk(KERN_INFO"%s: urb: %p len: %d maxtransfer: %d encap: %d\n", __FUNCTION__, 
                                                urb, urb->actual_length, network_private->maxtransfer, 
                                                network_private->encapsulation);

                                {
                                        __u8 *cp = urb->buffer;
                                        int i;
                                        for (i = 0; i < urb->actual_length; i++) {
                                                if ((i%32) == 0) {
                                                        printk("\n[%2x] ", i);
                                                }
                                                printk("%02x ", *cp++);
                                        }
                                        printk("\n");
                                }
#endif


                                network_private->stats.rx_crc_errors++;
                                network_private->stats.rx_errors++;
                        }

                        // catch an overrun error
                        // (Only used if CONFIG_USBD_NETWORK_RNDIS is defined.)
                        CATCH(fifo_error) {
                                network_private->stats.rx_fifo_errors++;
                                network_private->stats.rx_errors++;
                        }

                        // if skb defined free it
                        if (skb) {
                                dev_kfree_skb_any (skb);
                        }
                }
                network_private->stats.rx_dropped++;
                return -EINVAL;
        }
}

//_________________________________________________________________________________________________
//                                      network_urb_sent

/**
 * Handles notification that an urb has been sent (successfully or otherwise).
 *
 * @param urb   Pointer to the urb that has been sent.
 * @param rc    Result code from the send operation.
 *
 * @return 0 - success; -EINVAL - failure.
 *
 * @note NOT CALLED from interrupt context.
 * @note According to <i>usbdfunc(7)</i>:
 *       The <i>Function</i> driver sends data in a structure called an <i>urb</i> using
 *       a function called <i>usbd_send_urb()</i>. When the <i>urb</i> has been sent the
 *       <i>USB Device</i> core layer will call the <i>urb_sent</i> function to notify
 *       the <i>Function</i> driver that it has finished sending the data or has stopped
 *       attempting to send the data.
 */
static int network_urb_sent (struct urb *urb, int rc)
{
        struct usb_device_instance *device = NULL;
        struct usb_function_instance *function = NULL;
        struct usb_network_private *network_private = NULL;
        struct sk_buff *skb = NULL;
        struct net_device *net_device = NULL;

        if (!urb || 
                        !(device = urb->device) ||
                        !(function = device->active_function_instance) ||
                        !(network_private = function->privdata) ||
                        !(network_private->flags & NETWORK_ATTACHED)
                        ) 
        {
                printk(KERN_INFO"%s: device: %p function: %p network_private: %p\n", 
                                __FUNCTION__, device, function, network_private);
                return -EINVAL;
        }

        //printk(KERN_INFO"urb: %p device: %p address: %x\n", urb, urb->device, urb->endpoint->endpoint_address);

        switch (urb->endpoint->endpoint_address&0xf) {

        case INT_IN_ENDPOINT:

                //printk(KERN_INFO"INT len: %d endpoint_address: %x\n", urb->actual_length, urb->endpoint->endpoint_address);
                usbd_dealloc_urb (urb);
                break;

        case BULK_IN_ENDPOINT:

                //printk(KERN_INFO"IN len: %d endpoint_address: %x\n", urb->actual_length, urb->endpoint->endpoint_address);

                if (!(network_private->flags & NETWORK_CREATED)) {
                        // The interface is being/has been shutdown.
                        // XXX Check this logic for mem leaks, but the
                        // shutdown should have taken care of it, so
                        // a leak is not likely.
                        return -EINVAL;
                }

                // retrieve skb pointer and unlink from urb pointers
                skb = (struct sk_buff *) urb->privdata;

                urb->privdata = NULL;
                usbd_dealloc_urb (urb);

                // tell netproto we are done with the skb, it will test for NULL
                //netproto_done (interface, skb, rc != SEND_FINISHED_OK);

                if (!skb) {
                        return -EINVAL;
                }

                if (!(net_device = &network_net_device)) {
                        printk(KERN_INFO"%s: bad net_dev: %p\n", __FUNCTION__, net_device);
                        return -EINVAL;
                }

                if (rc != SEND_FINISHED_OK) {
                        network_private->stats.tx_dropped++;
                }

                {			// lock and update stats
                        unsigned long flags;
                        write_lock_irqsave (&network_private->rwlock, flags);

                        network_private->avg_queue_entries += network_private->queued_entries;
                        network_private->queued_entries--;
                        if (skb) {
                                network_private->samples++;
                                network_private->jiffies += jiffies - *(time_t *) (&skb->cb);
                                network_private->queued_bytes -= skb->len;
                                //elapsed = jiffies - *(time_t *) (&skb->cb);
                        }
                        write_unlock_irqrestore (&network_private->rwlock, flags);
                }

                if (skb) {
                        dev_kfree_skb_any (skb);
                }

                if (netif_queue_stopped (net_device)) {
                        unsigned long flags;
                        netif_wake_queue (net_device);
                        write_lock_irqsave (&network_private->rwlock, flags);
                        network_private->restarts++;
                        write_unlock_irqrestore (&network_private->rwlock, flags);
                }
                break;
        }
	return 0;
}

struct usb_function_operations function_ops_net = {
	event_irq:     network_event_irq,
	recv_urb:      network_recv_urb,
	recv_setup:    network_recv_setup,
	urb_sent:      network_urb_sent,
	function_init: network_function_init,
	function_exit: network_function_exit,
};



#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)

struct usb_function_driver rndis_function_driver = {
	name: "RNDIS Net Function",
	ops: &function_ops_net,
	device_description: &rndis_device_description,
	configurations: sizeof (rndis_description) / sizeof (struct usb_configuration_description),
	configuration_description: rndis_description,
	this_module: THIS_MODULE,
};

#ifdef CONFIG_USBD_NETWORK_CDC
struct usb_configuration_description rndis_cdc_description[] = {
        { iConfiguration: "RNDIS CDC Combo Net Cfg", 
                configuration_descriptor: (struct usb_configuration_descriptor *) rndis_configuration_descriptor,
	        interfaces:sizeof (rndis_interfaces) / sizeof (struct usb_interface_description),
                interface_list:rndis_interfaces,},
	{ iConfiguration:"CDC Combo Net Cfg",
                configuration_descriptor: (struct usb_configuration_descriptor *)cdc_configuration_descriptor,
                interfaces:sizeof (cdc_interfaces) / sizeof (struct usb_interface_description),
                interface_list:cdc_interfaces,},
#ifdef CONFIG_USBD_NETWORK_BASIC
	{ iConfiguration:"BASIC Combo Net Cfg",
                configuration_descriptor: (struct usb_configuration_descriptor *)basic_configuration_descriptor,
                interfaces:sizeof (basic_interfaces) / sizeof (struct usb_interface_description),
                interface_list:basic_interfaces,},
#endif
};

struct usb_function_driver rndis_cdc_function_driver = {
	name: "RNDIS/CDC Net Function",
	ops: &function_ops_net,
	device_description: &cdc_device_description,
	configurations: sizeof (rndis_cdc_description) / sizeof (struct usb_configuration_description),
	configuration_description: rndis_cdc_description,
	this_module: THIS_MODULE,
};
#endif				/* CONFIG_USBD_NETWORK_CDC */

#ifdef CONFIG_USBD_NETWORK_SAFE
struct usb_configuration_description rndis_safe_description[] = {
        { iConfiguration: "RNDIS SAFE Combo Net Cfg", 
                configuration_descriptor: (struct usb_configuration_descriptor *) rndis_configuration_descriptor,
	        interfaces:sizeof (rndis_interfaces) / sizeof (struct usb_interface_description),
                interface_list:rndis_interfaces,},
	{ iConfiguration:"SAFE Net Cfg",
                configuration_descriptor: (struct usb_configuration_descriptor *)safe_configuration_descriptor,
	        interfaces:sizeof (safe_interfaces) / sizeof (struct usb_interface_description),
                interface_list:safe_interfaces, },
#ifdef CONFIG_USBD_NETWORK_BASIC
	{ iConfiguration:"BASIC Combo Net Cfg",
                configuration_descriptor: (struct usb_configuration_descriptor *)basic_configuration_descriptor,
                interfaces:sizeof (basic_interfaces) / sizeof (struct usb_interface_description),
                interface_list:basic_interfaces,},
#endif
};

struct usb_function_driver rndis_safe_function_driver = {
	name: "RNDIS/SAFE Net Function",
	ops: &function_ops_net,
	device_description: &safe_device_description,
	configurations: sizeof (rndis_safe_description) / sizeof (struct usb_configuration_description),
	configuration_description: rndis_safe_description,
	this_module: THIS_MODULE,
};
#endif				/* CONFIG_USBD_NETWORK_SAFE */

#endif                          /* CONFIG_USBD_RNDIS */




#ifdef CONFIG_USBD_NETWORK_BASIC
struct usb_function_driver basic_function_driver = {
	name: "BASIC Net Function",
	ops: &function_ops_net,
	device_description: &basic_device_description,
	configurations: sizeof (basic_description) / sizeof (struct usb_configuration_description),
	configuration_description: basic_description,
	this_module: THIS_MODULE,
};
#endif				/* CONFIG_USBD_NETWORK_BASIC */

#ifdef CONFIG_USBD_NETWORK_CDC
struct usb_function_driver cdc_function_driver = {
	name: "CDC Net Function",
	ops: &function_ops_net,
	device_description: &cdc_device_description,
	configurations: sizeof (cdc_description) / sizeof (struct usb_configuration_description),
	configuration_description: cdc_description,
	this_module: THIS_MODULE,
};
#endif				/* CONFIG_USBD_NETWORK_CDC */

#ifdef CONFIG_USBD_NETWORK_SAFE
struct usb_function_driver safe_function_driver = {
	name: "SAFE Net Function",
	ops: &function_ops_net,
	device_description: &safe_device_description,
	configurations: sizeof (safe_device_description) / sizeof (struct usb_configuration_description),
	configuration_description: safe_description,
	this_module: THIS_MODULE,
};
#endif				/* CONFIG_USBD_NETWORK_SAFE */

#ifdef CONFIG_USBD_NETWORK_BLAN
struct usb_function_driver blan_function_driver = {
	name: "BLAN Net Function",
	ops: &function_ops_net,
	device_description: &blan_device_description,
	configurations: sizeof (blan_device_description) / sizeof (struct usb_configuration_description),
	configuration_description: blan_description,
	this_module: THIS_MODULE,
};
#endif				/* CONFIG_USBD_NETWORK_BLAN */


#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
#ifdef CONFIG_USBD_NETWORK_CDC
struct usb_function_driver *function_driver = &rndis_cdc_function_driver;
#elif CONFIG_USBD_NETWORK_SAFE
struct usb_function_driver *function_driver = &rndis_safe_function_driver;
#endif
#else
struct usb_function_driver *function_driver = NULL;
#endif                          /* CONFIG_USBD_RNDIS */

//_________________________________________________________________________________________________
//______________________________________Module Initialization and Removal__________________________

//_________________________________________________________________________________________________
//                                      hexdigit

/**
 * Converts characters in [0-9A-F] to 0..15, characters in [a-f] to 42..47, and all others to 0.
 *
 * @param c     The character to be converted.
 *
 * @return The converted value.
 *
 * @warning The conversion for the characters [a-f] is unconventional, and there are calls to
 *          this function which fail to mask the returned value to four bits.
 */
static __u8 hexdigit (char c)
{
	return isxdigit (c) ? (isdigit (c) ? (c - '0') : (c - 'A' + 10)) : 0;
}

//_________________________________________________________________________________________________
//                                      network_modinit
//

void set_address(char *mac_address_str, __u8 *dev_addr)
{
        int i;
        if (mac_address_str && strlen(mac_address_str)) {
                for (i = 0; i < ETH_ALEN; i++) {
                        dev_addr[i] = 
                                hexdigit (mac_address_str[i * 2]) << 4 | 
                                hexdigit (mac_address_str[i * 2 + 1]);
                }
        }
        else {
                get_random_bytes(dev_addr, ETH_ALEN);
                dev_addr[0] = (dev_addr[0] & 0xf0) | 0x02;
        }
}

int macstrtest(char *mac_address_str)
{
        int l = 0;

        if (mac_address_str) {
                l = strlen(mac_address_str);
        }
        return ((l != 0) && (l != 12));
}

/**
 * Initializes the module. Fails if:
 * - One or more of the parameters settable at module load time has been set to a bad value:
 *     - in_pkt_sz is less than eight
 *     - out_pkt_sz is less than eight
 *     - local MAC address string is not exactly 12 bytes long
 *     - remote MAC address string is not exactly 12 bytes long
 * - The call to @c network_create() fails
 * - The call to @c usbd_register_function() fails
 *
 * @return 0 - success; -EINVAL - failure.
 */
int network_modinit (void)
{
	int i;
        network_type_t  network_type = network_unknown;

	printk(KERN_INFO "%s: %s vendor_id: %04x product_id: %04x\n", __FUNCTION__, __usbd_module_info, vendor_id, product_id);


#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MYDHCPD)
	printk(KERN_INFO "%s: mydhcpd\n", __FUNCTION__);
#endif

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        if (rndis) {
                THROW_IF (network_type != network_unknown, select_error);
                if (cdc) {
                        printk(KERN_INFO "%s: rndis/cdc\n", __FUNCTION__);
                        network_type = network_rndis_cdc;
                }
                else if (safe) {
                        printk(KERN_INFO "%s: rndis/cdc\n", __FUNCTION__);
                        network_type = network_rndis_safe;
                }
                else {
                        printk(KERN_INFO "%s: rndis\n", __FUNCTION__);
                        network_type = network_blan;
                }
        }
#endif

#ifdef CONFIG_USBD_NETWORK_BASIC
        if (basic) {
                THROW_IF (network_type != network_unknown, select_error);
                printk(KERN_INFO "%s: basic\n", __FUNCTION__);
                network_type = network_basic;
        }
#endif
#ifdef CONFIG_USBD_NETWORK_CDC
        if (cdc) {
                printk(KERN_INFO "%s: cdc\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_cdc;
        }
#endif
#ifdef CONFIG_USBD_NETWORK_SAFE
        if (safe) {
                printk(KERN_INFO "%s: safe\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_safe;
        }
#endif
#ifdef CONFIG_USBD_NETWORK_BLAN
        if (blan) {
                printk(KERN_INFO "%s: blan\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_blan;
        }
#endif



        // still unknown - check for rndis and rndis combos?
#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        if (network_type == network_unknown) {
#if defined(CONFIG_USBD_NETWORK_CDC)
                printk(KERN_INFO "%s: rndis/cdc\n", __FUNCTION__);
                network_type = network_rndis_cdc;
#elif defined(CONFIG_USBD_NETWORK_SAFE)
                printk(KERN_INFO "%s: rndis/cdc\n", __FUNCTION__);
                network_type = network_rndis_safe;
#else 
                printk(KERN_INFO "%s: rndis\n", __FUNCTION__);
                network_type = network_blan;
        }
#endif
#endif

        // still unknown - check for other configurations

        if (network_type == network_unknown) {
#if defined(CONFIG_USBD_NETWORK_BASIC)
                printk(KERN_INFO "%s: basic\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_basic;
#endif
#if defined(CONFIG_USBD_NETWORK_CDC)
                printk(KERN_INFO "%s: cdc\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_cdc;
#endif
#if defined(CONFIG_USBD_NETWORK_SAFE)
                printk(KERN_INFO "%s: safe\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_safe;
#endif
#if defined(CONFIG_USBD_NETWORK_BLAN)
                printk(KERN_INFO "%s: blan\n", __FUNCTION__);
                THROW_IF (network_type != network_unknown, select_error);
                network_type = network_blan;
#endif
        }

        // sanity check
        THROW_IF (network_type == network_unknown, select_error);


        // select the function driver descriptors based on network_type

        switch (network_type) {

#if defined(CONFIG_USBD_NETWORK_BASIC)
        case network_basic:
                printk(KERN_INFO "%s: basic\n", __FUNCTION__);
                function_driver = &basic_function_driver;
                break;
#endif

#if defined(CONFIG_USBD_NETWORK_CDC)
        case network_cdc:
                function_driver = &cdc_function_driver;
                break;
#endif

#if defined(CONFIG_USBD_NETWORK_SAFE)
        case network_safe:
                function_driver = &safe_function_driver;
                break;
#endif

#if defined(CONFIG_USBD_NETWORK_BLAN)
        case network_blan:
                function_driver = &blan_function_driver;
                break;
#endif

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        case network_rndis:
                function_driver = &rndis_function_driver;
                break;
        case network_rndis_cdc:
                function_driver = &rndis_cdc_function_driver;
                break;
        case network_rndis_safe:
                function_driver = &rndis_safe_function_driver;
                break;
#endif
        default:
                THROW(select_error);
                break;
        }

        strncpy(network_net_device.name, network_type == network_blan ? "usbl0" : "usbb0", 6);

        THROW_IF (!function_driver, select_error);

        CATCH(select_error) {
                printk(KERN_INFO "%s: configuration selection error\n", __FUNCTION__);
		return -EINVAL;
        }

        usb_network_private.network_type = network_type;


#ifdef CONFIG_USBD_NETWORK_EP0TEST
        /*
         * ep0test - test that bus interface can do ZLP on endpoint zero
         *
         * This will artificially force iProduct string descriptor to be
         * exactly the same as the endpoint zero packetsize.  When the host
         * requests this string it will request it not knowing the strength
         * and will use a max length of 0xff. The bus interface driver must
         * send a ZLP to terminate the transaction.
         *
         * The iProduct descriptor is used because both the Linux and
         * Windows usb implmentations fetch this in a default enumeration. 
         *
         */
        if (ep0test) {
                switch (ep0test) {
                case 8:  function_driver->device_description->iProduct = "012"; break;
                case 16: function_driver->device_description->iProduct = "0123456"; break;
                case 32: function_driver->device_description->iProduct = "0123456789abcde"; break;
                case 64: function_driver->device_description->iProduct = "0123456789abcdef0123456789abcde"; break;
                default: printk(KERN_ERR"%s: ep0test: bad value: %d, must be one of 8, 16, 32 or 64\n", 
                                         __FUNCTION__, ep0test); return -EINVAL;
                         break;
                }
                printk(KERN_INFO"%s: ep0test: iProduct set to: %s\n", __FUNCTION__, 
                                function_driver->device_description->iProduct);
        }
        else {
                printk(KERN_INFO"%s: ep0test: not set\n", __FUNCTION__);
        }
#endif /* CONFIG_USBD_NETWORK_EP0TEST */

#ifdef CONFIG_USBD_NETWORK_ALLOW_SETPKTSIZE
        if (out_pkt_sz != BULK_OUT_PKTSIZE) {
                switch (out_pkt_sz) {
                default:
                        printk(KERN_ERR"%s: Bad pkt size %dl", __FUNCTION__, out_pkt_sz);
                        return -EINVAL;
                case 8: 
                case 16:
                case 32:
                case 64:
                        ((struct usb_endpoint_descriptor *)&network_data_endpoints[0])->wMaxPacketSize = out_pkt_sz;
                }
        }
       if (in_pkt_sz != BULK_IN_PKTSIZE) {
                switch (out_pkt_sz) {
                default:
                        printk(KERN_ERR"%s: Bad pkt size %d", __FUNCTION__, in_pkt_sz);
                        return -EINVAL;
                case 8:
                case 16:
                case 32:
                case 64:
                        ((struct usb_endpoint_descriptor *)&network_data_endpoints[1])->wMaxPacketSize = in_pkt_sz;
                }
        }
#endif


#if defined(CONFIG_USBD_NETWORK_MANUFACTURER)
        if (strlen(CONFIG_USBD_NETWORK_MANUFACTURER)) {
                function_driver->device_description->iManufacturer = CONFIG_USBD_NETWORK_MANUFACTURER;
        } else 
#endif
#if defined(CONFIG_USBD_MANUFACTURER)
        if (strlen(CONFIG_USBD_MANUFACTURER)) {
                function_driver->device_description->iManufacturer = CONFIG_USBD_MANUFACTURER;
        } else 
#endif

#if defined(CONFIG_USBD_NETWORK_PRODUCT_NAME)
        if (strlen(CONFIG_USBD_NETWORK_MANUFACTURER)) {
                function_driver->device_description->iProduct = CONFIG_USBD_NETWORK_PRODUCT_NAME;
        } else 
#endif
#if defined(CONFIG_USBD_PRODUCT_NAME)
        if (strlen(CONFIG_USBD_MANUFACTURER)) {
                function_driver->device_description->iProduct = CONFIG_USBD_PRODUCT_NAME;
        } else 
#endif

#ifdef CONFIG_USBD_NETWORK_ALLOW_SETID
                printk(KERN_INFO"%s: checking idVendor: %04x idPRoduct: %04x\n", __FUNCTION__, vendor_id, product_id);
	if (vendor_id) {
                printk(KERN_INFO"%s: overriding idVendor: %04x\n", __FUNCTION__, vendor_id);
		function_driver->device_description->idVendor = vendor_id;
	}
	if (product_id) {
                printk(KERN_INFO"%s: overriding idProduct: %04x\n", __FUNCTION__, product_id);
		function_driver->device_description->idProduct = product_id;
	}
#endif

        if ((macstrtest(local_mac_address_str) || macstrtest(remote_mac_address_str))) {
		printk(KERN_INFO"%s: bad size %s %s\n", __FUNCTION__, local_mac_address_str, remote_mac_address_str);
		return -EINVAL;
        }

        set_address(local_mac_address_str, local_dev_addr);
        set_address(remote_mac_address_str, remote_dev_addr);
       
       	if (!(Network_private = network_create())) {
		return -EINVAL;
        }
	
#ifdef CONFIG_HOTPLUG
        Network_private->hotplug_bh.routine = hotplug_bh;
        Network_private->hotplug_bh.data = NULL;
#endif

        memcpy(network_net_device.dev_addr, local_dev_addr, ETH_ALEN);

        if (make_crc_table()) {
                THROW(error);
        }
        Network_private->encapsulation = simple_crc;


	if (usbd_register_function (function_driver)) {
                THROW(error);
	}

        CATCH(error) {
                network_destroy();
		return -EINVAL;
        }
	//printk(KERN_INFO "%s: finish\n", __FUNCTION__);
	return 0;
}

//_________________________________________________________________________________________________
//                                      network_modexit

/**
 * Cleans up the module. Deregisters the function driver and destroys the network object.
 */
void network_modexit (void)
{
        int i;
	printk(KERN_INFO"%s: exiting\n", __FUNCTION__);
	usbd_deregister_function (function_driver);
        network_destroy();
        if (crc32_table) {
                lkfree(crc32_table);
                crc32_table = NULL;
        }
#if 0
        for (i = 0; i < 64; i++) {
                printk(KERN_INFO"%s: errors[%02d] %5d %4d %3d\n", __FUNCTION__, i, 
                                crc_checks[i], crc_errors[i],
                                crc_checks[i]?(100 * crc_errors[i])/crc_checks[i]:0
                                );
        }
#endif
}

////_________________________________________________________________________________________________
//______________________________________module_init and module_exit________________________________

//module_init (network_modinit);
//module_exit (network_modexit);
EXPORT_SYMBOL(network_modinit);
EXPORT_SYMBOL(network_modexit);
//_________________________________________________________________________________________________

