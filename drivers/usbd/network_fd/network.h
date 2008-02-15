/*
 * linux/drivers/usbd/network_fd/network.h - RNDIS Network Function Driver
 *
 * Copyright (c) 2002, 2003 Belcarra
 *
 * By: 
 *      Chris Lynne <cl@belcarra.com>
 *      Bruce Balden <balden@belcarra.com>
 *
 */

#ifndef NETWORK_FD_H
#define NETWORK_FD_H 1

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
#include <rndis.h>
#endif                          /* CONFIG_USBD_RNDIS */

typedef enum network_encapsulation {
        simple_net, simple_crc, 
#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        rndis_encapsulation
#endif                          /* CONFIG_USBD_RNDIS */
} network_encapsulation_t;

typedef enum network_hotplug_status {
	hotplug_unkown,
	hotplug_attached,
	hotplug_detached
} network_hotplug_status_t;

typedef enum network_type {
        network_unknown,
        network_blan,
        network_safe,
        network_cdc,
        network_basic,
        network_rndis,
        network_rndis_cdc,
        network_rndis_safe
} network_type_t;

struct usb_network_private {

        struct net_device_stats stats;  /* network device statistics */

	int flags;
	struct usb_device_instance *device;
	unsigned int maxtransfer;
        rwlock_t rwlock;

	network_hotplug_status_t hotplug_status;
        network_type_t network_type;

        int state;

        int mtu; 
	int crc;

        unsigned int stopped; 
        unsigned int restarts;

        unsigned int max_queue_entries;
        unsigned int max_queue_bytes;

        unsigned int queued_entries;
        unsigned int queued_bytes;

        time_t avg_queue_entries;

        time_t jiffies;
        unsigned long samples;

        network_encapsulation_t encapsulation;

#ifdef CONFIG_HOTPLUG
	struct tq_struct hotplug_bh;
#endif
#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)
        msg_t *rndis_msg;
#endif                          /* CONFIG_USBD_RNDIS */

};

// XXX this needs to be co-ordinated with rndis.c maximum's
#define MAXFRAMESIZE 2000

#if defined(BELCARRA_NON_GPL) && defined(CONFIG_USBD_RNDIS_MODULE)

	#if defined (CONFIG_USBD_RNDIS_)
		#undef CONFIG_USBD_NETWORK_VENDORID
		#define CONFIG_USBD_NETWORK_VENDORID                    CONFIG_USBD_RNDIS_VENDORID
	#endif
	#if defined (CONFIG_USBD_RNDIS_PRODUCTID)
		#undef CONFIG_USBD_NETWORK_PRODUCTID
		#define CONFIG_USBD_NETWORK_PRODUCTID                   CONFIG_USBD_RNDIS_PRODUCTID
	#endif
	#if defined (CONFIG_USBD_RNDIS_PRODUCT_NAME)
		#undef CONFIG_USBD_NETWORK_PRODUCT_NAME
		#define CONFIG_USBD_NETWORK_PRODUCT_NAME                CONFIG_USBD_RNDIS_PRODUCT_NAME
	#endif
	#if defined (CONFIG_USBD_RNDIS_MANUFACTURER)
		#undef CONFIG_USBD_NETWORK_MANUFACTURER
		#define CONFIG_USBD_NETWORK_MANUFACTURER                CONFIG_USBD_RNDIS_MANUFACTURER
	#endif
	#if defined (CONFIG_USBD_RNDIS_BCDDEVICE)
		#undef CONFIG_USBD_NETWORK_BCDDEVICE
		#define CONFIG_USBD_NETWORK_BCDDEVICE                CONFIG_USBD_RNDIS_BCDDEVICE
        #endif

#endif                          /* CONFIG_USBD_RNDIS */

#if defined(CONFIG_USBD_VENDORID) && (CONFIG_USBD_VENDORID > 0) && (CONFIG_USBD_NETWORK_VENDORID == 0)
	#undef CONFIG_USBD_NETWORK_VENDORID
	#define CONFIG_USBD_NETWORK_VENDORID                    CONFIG_USBD_VENDORID
#endif
#if defined(CONFIG_USBD_PRODUCTID) && (CONFIG_USBD_PRODUCTID > 0) && (CONFIG_USBD_NETWORK_PRODUCTID == 0)
	#undef CONFIG_USBD_NETWORK_PRODUCTID
	#define CONFIG_USBD_NETWORK_PRODUCTID                   CONFIG_USBD_PRODUCTID
#endif
#if defined(CONFIG_USBD_BCDDEVICE) && (CONFIG_USBD_BCDDEVICE > 0) && (CONFIG_USBD_NETWORK_BCDDEVICE == 0)
	#undef CONFIG_USBD_NETWORK_BCDDEVICE
	#define CONFIG_USBD_NETWORK_BCDDEVICE                   CONFIG_USBD_BCDDEVICE
#endif


//#if !defined(CONFIG_USBD_NETWORK_LOCAL_MACADDR)
//	#define CONFIG_USBD_NETWORK_LOCAL_MACADDR          "020001000001"
//#endif

//#if !defined(CONFIG_USBD_NETWORK_REMOTE_MACADDR)
//	#define CONFIG_USBD_NETWORK_REMOTE_MACADDR         "020002000001"
//#endif


#if !defined(CONFIG_USBD_MAXPOWER)
	#define CONFIG_USBD_MAXPOWER                    0
#endif

#if !defined(CONFIG_USBD_MANUFACTURER)
	#define CONFIG_USBD_MANUFACTURER              	"Belcarra"
#endif


#if !defined(CONFIG_USBD_VENDORID)
	#error "CONFIG_USBD_VENDORID not defined"
#endif

#if !defined(CONFIG_USBD_PRODUCTID)
	#error "CONFIG_USBD_PRODUCTID not defined"
#endif


#if !defined(CONFIG_USBD_PRODUCT_NAME)
	#define CONFIG_USBD_PRODUCT_NAME 		"Belcarra Network Device"
#endif

#if !defined(CONFIG_USBD_SERIAL_NUMBER_STR)
	#define CONFIG_USBD_SERIAL_NUMBER_STR           ""
#endif


#endif /* NETWORK_FD_H */
