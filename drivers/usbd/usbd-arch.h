/*
 * linux/drivers/usbd/usbd-arch.h 
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
 * This file contains pre-canned configurations for specific architectures
 * and systems.
 *
 * The architecture specific areas concentrates on setting defaults that
 * will work with each architecture (endpoint restrictions etc).
 *
 * The system specific areas set defaults appropriate for a specific
 * system or board (vendor id etc).
 *
 * Typically during early development you will set these via the kernel
 * configuration and migrate the values into here once specific values
 * have been tested and will no longer change.
 */


/*
 * Lineo specific 
 */

#define VENDOR_SPECIFIC_CLASS           0xff
#define VENDOR_SPECIFIC_SUBCLASS        0xff
#define VENDOR_SPECIFIC_PROTOCOL        0xff

/*
 * Lineo Classes
 */
#define LINEO_CLASS            		0xff

#define LINEO_SUBCLASS_BASIC_NET          0x01
#define LINEO_SUBCLASS_BASIC_SERIAL       0x02

/*
 * Lineo Protocols
 */
#define	LINEO_BASIC_NET_CRC 		0x01
#define	LINEO_BASIC_NET_CRC_PADDED	0x02

#define	LINEO_BASIC_SERIAL_CRC 		0x01
#define	LINEO_BASIC_SERIAL_CRC_PADDED	0x02


/*
 * Architecture specific endpoint configurations
 */

#if defined(CONFIG_ARCH_SA1100) || defined(CONFIG_ARCH_SA1100_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR SA1110
/*
 * The StrongArm SA-1110 has fixed endpoints and the bulk endpoints
 * are limited to 64 byte packets and does not support ZLP.
 */

        #define ABS_BULK_OUT_ADDR                            1
        #define ABS_BULK_IN_ADDR                             2
        #define ABS_INT_IN_ADDR                            0

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64
        #undef MAX_INT_IN_PKTSIZE

        #undef CONFIG_USBD_SERIAL_OUT_ENDPOINT
        #undef CONFIG_USBD_SERIAL_IN_ENDPOINT
        #undef CONFIG_USBD_SERIAL_INT_ENDPOINT

        #define CONFIG_USBD_NO_ZLP_SUPPORT
	
	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1

#endif

#if defined(CONFIG_ARCH_L7200)|| defined(CONFIG_ARCH_L7200_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR L7200
/*
 * The Linkup L7205/L7210 has fixed endpoints and the bulk endpoints
 * are limited to 32 byte packets and does not support ZLP.
 *
 * To get around specific hardware problems the Linkup eliminates optional 
 * strings in the configuration, reverses the CDC data interfaces.
 * 
 * It also requires all USB packets sent and received to be padded. They
 * must be either 31 or 32 bytes in length.
 *
 * XXX The new version of the L7210 may not require padding.
 */
        #define ABS_BULK_OUT_ADDR                           1
        #define ABS_BULK_IN_ADDR                            2
        #define ABS_INT_IN_ADDR                             3

        #define MAX_BULK_OUT_PKTSIZE                        32
        #define MAX_BULK_IN_PKTSIZE                         32
        #define MAX_INT_IN_PKTSIZE                          16

        #undef CONFIG_USBD_SERIAL_OUT_ENDPOINT
        #undef CONFIG_USBD_SERIAL_IN_ENDPOINT
        #undef CONFIG_USBD_SERIAL_INT_ENDPOINT

        #define CONFIG_USBD_NO_ZLP_SUPPORT
        #define CONFIG_USBD_NETWORK_NO_STRINGS
        #define CONFIG_USBD_NETWORK_PADDED

	#undef CONFIG_USBD_NETWORK_CDC
	#undef CONFIG_USBD_NETWORK_MDLM
	#undef CONFIG_USBD_NETWORK_SAFE

	#define CONFIG_USBD_NETWORK_MDLM			    1
	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	    1

#endif

#if defined(CONFIG_USBD_SL11_BUS)|| defined(CONFIG_USBD_SL11_BUS_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR SL11
/*
 * The SL11 endpoints can be a mix of 1, 2, or 3. The SL11 endpoints have
 * fixed addresses but each can be used for Bulk IN, Bulk OUT and Interrupt.
 *
 * The default addresses can be overridden by using the kernel configuration
 * and setting the net-fd IN, OUT and INT addresses. See Config.in
 */

        #define DFL_OUT_ADDR                                 1
        #define DFL_IN_ADDR                                  2
        #define DFL_INT_ADDR                                 3

        #define MAX_BULK_OUT_ADDR                            3
        #define MAX_BULK_IN_ADDR                             3
        #define MAX_INT_IN_ADDR                              3

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64
        #define MAX_INT_IN_PKTSIZE                           16

#endif

#if defined(CONFIG_USBD_SUPERH_BUS)|| defined(CONFIG_USBD_SUPERH_BUS_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR SUPERH
/*
 * The SuperH 7727 has fixed endpoints and does not support ZLP.
 */

        #define ABS_BULK_OUT_ADDR                            1
        #define ABS_BULK_IN_ADDR                             2
        #define ABS_INT_IN_ADDR                              3

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64
        #define MAX_INT_IN_PKTSIZE                           16

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	     1
#endif

#if defined(CONFIG_USBD_PXA_BUS)|| defined(CONFIG_USBD_PXA_BUS_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR PXA
/*
 * The PXA has fixed endpoints. It also cannot function as a CDC device.
 *
 */
        #define ABS_BULK_IN_ADDR                             1
        #define ABS_BULK_OUT_ADDR                            2

	#define	ABS_ISO_IN_ADDR			             3
	#define ABS_ISO_OUT_ADDR			     4

        #define ABS_INT_IN_ADDR                              5

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64


        #define MAX_INT_IN_PKTSIZE                           8

	#define	MAX_ISO_IN_PKTSIZE			     512
	#define	MAX_ISO_OUT_PKTSIZE			     512

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER 	     1

	#undef CONFIG_USBD_NETWORK_CDC
	#undef CONFIG_USBD_SERIAL_CDC

#endif

#if defined(CONFIG_USBD_AU1X00_BUS)|| defined(CONFIG_USBD_AU1X00_BUS_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR AU1X00
/*
 * The AU1X00 has fixed endpoints 
 */
        #define ABS_BULK_IN_ADDR                             2
        #define ABS_BULK_OUT_ADDR                            4
        #define ABS_INT_IN_ADDR                              3

        #define ABS_ISO_IN_ADDR                              2
        #define ABS_ISO_OUT_ADDR                             4
        #define ABS_INT_IN_ADDR                              3

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64
        #define MAX_INT_IN_PKTSIZE                           64

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	     1
#endif

#if defined(CONFIG_USBD_LH7A400_BUS)|| defined(CONFIG_USBD_LH7A400_BUS_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR LH7A400
/*
 * The LH7A400 has fixed endpoints 
 */
        #define ABS_BULK_IN_ADDR                             1
        #define ABS_BULK_OUT_ADDR                            2
        #define ABS_INT_IN_ADDR                              3

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64
        #define MAX_INT_IN_PKTSIZE                           64

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	     1
#endif

#if defined(CONFIG_USBD_SMDK2500_BUS)|| defined(CONFIG_USBD_SMDK2500_BUS_MODULE)
//#warning
//#warning SETTING DEFAULTS FOR SMDK2500
/*
 * The SMDK2500 has bi-directional endpoints 
 */
        #define ABS_INT_IN_ADDR                            1
        #define ABS_BULK_IN_ADDR                             3
        #define ABS_BULK_OUT_ADDR                            4

        #define MAX_BULK_OUT_PKTSIZE                         64
        #define MAX_BULK_IN_PKTSIZE                          64
        #define MAX_INT_IN_PKTSIZE                         32

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1
#endif

/* ********************************************************************************************* */

/*
 * known vendor and product ids
 * Serial and network vendor ids can be overidden with kernel config.
 */

/*
 * For itsy we will use Compaq iPaq vendor ID and 0xffff for product id
 */
#if defined(CONFIG_SA1100_ITSY) || defined(CONFIG_SA1100_H3XXX) || defined(CONFIG_SA1100_ITSY_MODULE) || defined(CONFIG_SA1100_H3XXX_MODULE)

	//#warning CONFIGURING FOR IPAQ H3XXX
	
	#undef CONFIG_USBD_VENDORID
	#undef CONFIG_USBD_PRODUCTID
	#define CONFIG_USBD_VENDORID		0x049f
	#define CONFIG_USBD_PRODUCTID		0xffff

	#undef CONFIG_USBD_SELFPOWERED
	#undef	CONFIG_USBD_MANUFACTURER
	#undef CONFIG_USBD_PRODUCT_NAME

	#define CONFIG_USBD_SELFPOWERED		1
	#define	CONFIG_USBD_MANUFACTURER	"Compaq"
	#define CONFIG_USBD_PRODUCT_NAME	"iPaq"
	
	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1
#endif

/*
 * Assigned vendor and product ids for HP Calypso
 */
#if defined(CONFIG_SA1100_CALYPSO) || defined(CONFIG_SA1110_CALYPSO) || defined(CONFIG_SA1100_CALYPSO_MODULE) || defined(CONFIG_SA1110_CALYPSO_MODULE)
	
	//#warning CONFIGURING FOR CALYPSO

	#undef CONFIG_USBD_VENDORID
	#undef CONFIG_USBD_PRODUCTID
	#define CONFIG_USBD_VENDORID		0x03f0
	#define CONFIG_USBD_PRODUCTID		0x2101

	#undef CONFIG_USBD_SELFPOWERED
	#undef	CONFIG_USBD_MANUFACTURER
	#undef CONFIG_USBD_PRODUCT_NAME

	#define CONFIG_USBD_SELFPOWERED		1
	#define	CONFIG_USBD_MANUFACTURER	"HP"
	#define CONFIG_USBD_PRODUCT_NAME	"Journada X25"
#endif

/*
 * Assigned vendor and serial/network product ids for Sharp Iris
 */
#if defined(CONFIG_ARCH_L7200) && defined(CONFIG_IRIS) || defined(CONFIG_ARCH_L7200_MODULE) && defined(CONFIG_IRIS_MODULE)

	//#warning CONFIGURING FOR LINKUP

	#undef CONFIG_USBD_VENDORID
	#define CONFIG_USBD_VENDORID		0x04dd
	#undef CONFIG_USBD_SERIAL_PRODUCTID
	#define CONFIG_USBD_SERIAL_PRODUCTID	0x8001
	#undef CONFIG_USBD_NETWORK_PRODUCTID
	#define CONFIG_USBD_NETWORK_PRODUCTID	0x8003

	#undef CONFIG_USBD_SELFPOWERED
	#undef	CONFIG_USBD_MANUFACTURER
	#undef CONFIG_USBD_PRODUCT_NAME

	#define CONFIG_USBD_SELFPOWERED		1
	#define	CONFIG_USBD_MANUFACTURER	"Sharp"
	#define CONFIG_USBD_PRODUCT_NAME	"Iris"

	#undef CONFIG_USBD_NETWORK_CDC
	#undef CONFIG_USBD_NETWORK_MDLM
	#undef CONFIG_USBD_NETWORK_SAFE

	#define CONFIG_USBD_NETWORK_MDLM			1
	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1
#endif


/*
 * Assigned vendor and serial/network product ids for Sharp Collie
 */
#if defined(CONFIG_SA1100_COLLIE) || defined(CONFIG_SA1100_COLLIE_MODULE)
	
	//#warning CONFIGURING FOR COLLIE

	#undef CONFIG_USBD_VENDORID
	#define CONFIG_USBD_VENDORID		0x04dd
	#undef CONFIG_USBD_SERIAL_PRODUCT ID
	#define CONFIG_USBD_SERIAL_PRODUCT ID	0x8002
	#undef CONFIG_USBD_NETWORK_PRODUCTID
	//#define CONFIG_USBD_NETWORK_PRODUCTID	0x8004
	#define CONFIG_USBD_NETWORK_PRODUCTID	0x8003

	#undef CONFIG_USBD_SELFPOWERED
	#undef	CONFIG_USBD_MANUFACTURER
	#undef CONFIG_USBD_PRODUCT_NAME

	#define CONFIG_USBD_SELFPOWERED		1
	#define	CONFIG_USBD_MANUFACTURER	"Sharp"
	#define CONFIG_USBD_PRODUCT_NAME	"Zaurus"

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1
	
#endif


/*
 * Assigned vendor and serial/network product ids for Vercel UD1
 */
#if defined(CONFIG_SA1100_IDR)
	
	//#warning CONFIGURING FOR Vercel UD1

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1
	
#endif

#ifdef CONFIG_SABINAL_DISCOVERY

	//#warning CONFIGURING FOR DISCOVERY

	#undef CONFIG_USBD_VENDORID
	#define CONFIG_USBD_VENDORID		0x04dd
	#undef CONFIG_USBD_SERIAL_PRODUCT ID
	#define CONFIG_USBD_SERIAL_PRODUCT ID	0x8003
	#undef CONFIG_USBD_NETWORK_PRODUCTID
	//#define CONFIG_USBD_NETWORK_PRODUCTID	0x8004
	#define CONFIG_USBD_NETWORK_PRODUCTID	0x8003

	#undef CONFIG_USBD_SELFPOWERED
	#undef	CONFIG_USBD_MANUFACTURER
	#undef CONFIG_USBD_PRODUCT_NAME

	#define CONFIG_USBD_SELFPOWERED		1
	#define	CONFIG_USBD_MANUFACTURER	"Sharp"
	#define CONFIG_USBD_PRODUCT_NAME	"SL-A003"

	#define	CONFIG_USBD_MAC_AS_SERIAL_NUMBER	1

#endif


/* --------------------------------------------------------------------------------------------- */

#if     defined(ABS_BULK_OUT_ADDR) && (ABS_BULK_OUT_ADDR > 0)
        #undef BULK_OUT_ENDPOINT
        #define BULK_OUT_ENDPOINT		ABS_BULK_OUT_ADDR
	#define BULK_OUT_PKTSIZE          	MAX_BULK_OUT_PKTSIZE
#elif   defined(MAX_BULK_OUT_ADDR) && (MAX_BULK_OUT_ADDR > 0) && defined(DFL_OUT_ADDR)
        #undef BULK_OUT_ENDPOINT
        #define BULK_OUT_ENDPOINT         	DFL_OUT_ADDR
	#define BULK_OUT_PKTSIZE          	MAX_BULK_OUT_PKTSIZE
#endif 


#if     defined(ABS_BULK_IN_ADDR) && (ABS_BULK_IN_ADDR > 0)
        #undef BULK_IN_ENDPOINT
        #define BULK_IN_ENDPOINT  		ABS_BULK_IN_ADDR
	#define BULK_IN_PKTSIZE           	MAX_BULK_IN_PKTSIZE
#elif   defined(MAX_BULK_IN_ADDR) && (MAX_BULK_IN_ADDR > 0) && defined(DFL_IN_ADDR)
        #undef BULK_IN_ENDPOINT
        #define BULK_IN_ENDPOINT          	DFL_IN_ADDR
	#define BULK_IN_PKTSIZE           	MAX_BULK_IN_PKTSIZE
#endif 


#if     defined(ABS_INT_IN_ADDR) && (ABS_INT_IN_ADDR > 0)
        #define INT_IN_ENDPOINT 		ABS_INT_IN_ADDR
	#define INT_IN_PKTSIZE          	MAX_INT_IN_PKTSIZE
#elif   defined(MAX_INT_IN_ADDR) && (MAX_INT_IN_ADDR > 0) && defined(DFL_INT_ADDR)
        #undef INT_IN_ENDPOINT
        #define INT_IN_ENDPOINT         	DFL_INT_ADDR
	#define INT_IN_PKTSIZE          	MAX_INT_IN_PKTSIZE
#endif 

#if     defined(ABS_ISO_OUT_ADDR) && (ABS_ISO_OUT_ADDR > 0)
        #undef ISO_OUT_ENDPOINT
        #define ISO_OUT_ENDPOINT		ABS_ISO_OUT_ADDR
	#define ISO_IN_PKTSIZE          	MAX_ISO_IN_PKTSIZE
#elif   defined(MAX_ISO_OUT_ADDR) && (MAX_ISO_OUT_ADDR > 0) && defined(DFL_OUT_ADDR)
        #undef ISO_OUT_ENDPOINT
        #define ISO_OUT_ENDPOINT         	DFL_OUT_ADDR
	#define ISO_IN_PKTSIZE          	MAX_ISO_IN_PKTSIZE
#endif 


#if     defined(ABS_ISO_IN_ADDR) && (ABS_ISO_IN_ADDR > 0)
        #undef ISO_IN_ENDPOINT
        #define ISO_IN_ENDPOINT  		ABS_ISO_IN_ADDR
	#define ISO_OUT_PKTSIZE          	MAX_ISO_OUT_PKTSIZE
#elif   defined(MAX_ISO_IN_ADDR) && (MAX_ISO_IN_ADDR > 0) && defined(DFL_IN_ADDR)
        #undef ISO_IN_ENDPOINT
        #define ISO_IN_ENDPOINT          	DFL_IN_ADDR
	#define ISO_OUT_PKTSIZE          	MAX_ISO_OUT_PKTSIZE
#endif 

/*
 * The UUT tester specifically tests for MaxPower to be non-zero (> 0).
 */
#ifdef CONFIG_USBD_SELFPOWERED
	#define BMATTRIBUTE BMATTRIBUTE_RESERVED | BMATTRIBUTE_SELF_POWERED
	#define BMAXPOWER 1
#else
	#define BMATTRIBUTE BMATTRIBUTE_RESERVED
	#define BMAXPOWER CONFIG_USBD_MAXPOWER
#endif


