/*****************************************************************************
 *
 *  	File:	wlan_defs.h
 *
 *****************************************************************************/

#ifndef _WLAN_DEFS_H_
#define _WLAN_DEFS_H_

#ifdef linux
#ifndef MMSCONFIG
#include	<linux/version.h>
#include	<linux/param.h>
#include	<linux/delay.h>
#include	<linux/slab.h>
#include	<linux/mm.h>
#include	<asm/io.h>
#include	<linux/types.h>
#include	<linux/module.h>
#include	<linux/kernel.h>
#include	<linux/sched.h>
#include	<linux/wireless.h>
#include	<linux/netdevice.h>
#include	<linux/timer.h>
#include	<linux/skbuff.h>
#include	<linux/if_arp.h>
#include	<asm/semaphore.h>
#include	<linux/delay.h>
#include	<asm/byteorder.h>
#endif
#include	<linux/types.h>
#endif


//Commenting .. TODO
/*#include "wlan/wlan_compat.h"*/

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__ ((packed))
#endif

#include "os_defs.h"

#define ETH_LENGTH_OF_ADDRESS 6

#ifdef MMSCONFIG
#define u32 	UINT
#define u16 	USHORT //MPS
#define u8  	u8
#endif

#ifndef linux
typedef int 	s32;
#endif

typedef union _LARGE_INTEGER
{
	struct {
			u32 __LowPart __ATTRIB_PACK__;
			LONG __HighPart __ATTRIB_PACK__;
	} __ATTRIB_PACK__  _ILH;
#define LowPart   _ILH.__LowPart
#define HighPart  _ILH.__HighPart
	LONGLONG QuadPart __ATTRIB_PACK__;
} __ATTRIB_PACK__ LARGE_INTEGER;

typedef LARGE_INTEGER		*PLARGE_INTEGER;
typedef LARGE_INTEGER		PHYSICAL_ADDRESS;
typedef PHYSICAL_ADDRESS	*PPHYSICAL_ADDRESS;


typedef enum _WLAN_DEVICE_POWER_STATE
{
	WlanDeviceStateUnspecified = 0,
	WlanDeviceStateD0,
	WlanDeviceStateD1,
	WlanDeviceStateD2,
	WlanDeviceStateD3,
	WlanDeviceStateMaximum
} WLAN_DEVICE_POWER_STATE, *PWLAN_DEVICE_POWER_STATE;

#ifndef	TRUE
#define TRUE			1
#endif
#ifndef	FALSE
#define	FALSE			0
#endif

#define EXTERN			extern

#define ASSERT(cond)

#define WLAN_STATUS_SUCCESS			(0)
#define WLAN_STATUS_FAILURE			(-1)
#define WLAN_STATUS_NOT_ACCEPTED                (-2)

/* Definitions for SNR and NF */
typedef enum _SNRNF_TYPE {
	TYPE_BEACON = 0,
	TYPE_RXPD,
	MAX_TYPE_B
} SNRNF_TYPE;

typedef enum _SNRNF_DATA {
	TYPE_NOAVG = 0,
	TYPE_AVG,
	MAX_TYPE_AVG
} SNRNF_DATA;

/*
 * Defines the state of the LAN media
 */
typedef enum _WLAN_MEDIA_STATE
{
	WlanMediaStateConnected,
	WlanMediaStateDisconnected
} WLAN_MEDIA_STATE, *PWLAN_MEDIA_STATE;

/*
 * Hardware status codes (OID_GEN_HARDWARE_STATUS).
 */
typedef enum _WLAN_HARDWARE_STATUS
{
	WlanHardwareStatusReady,
	WlanHardwareStatusInitializing,
	WlanHardwareStatusReset,
	WlanHardwareStatusClosing,
	WlanHardwareStatusNotReady
} WLAN_HARDWARE_STATUS, *PWLAN_HARDWARE_STATUS;

/*
 * Wlan Packet Filter Bits (OID_GEN_CURRENT_PACKET_FILTER).
 */
#define WLAN_PACKET_TYPE_DIRECTED               0x00000001
#define WLAN_PACKET_TYPE_MULTICAST              0x00000002
#define WLAN_PACKET_TYPE_ALL_MULTICAST          0x00000004
#define WLAN_PACKET_TYPE_BROADCAST              0x00000008
#define WLAN_PACKET_TYPE_SOURCE_ROUTING         0x00000010
#define WLAN_PACKET_TYPE_PROMISCUOUS            0x00000020
#define WLAN_PACKET_TYPE_SMT                    0x00000040
#define WLAN_PACKET_TYPE_ALL_LOCAL              0x00000080
#define WLAN_PACKET_TYPE_GROUP                  0x00001000
#define WLAN_PACKET_TYPE_ALL_FUNCTIONAL         0x00002000
#define WLAN_PACKET_TYPE_FUNCTIONAL             0x00004000
#define WLAN_PACKET_TYPE_MAC_FRAME              0x00008000

/*
 * Timer related functions
 */ 

#ifdef linux
#include <linux/if_ether.h>
#endif

/////////////////////////////////////////////////////////////////
//
//                  802.11-related definitions
//
/////////////////////////////////////////////////////////////////

#define MAX_SIZE_ARRAY	64

typedef struct _WEP_INFO
{
	u32 KeyLength;              //MAX 13 keys        
	u8 Key[MAX_SIZE_ARRAY];
} WEP_INFO __attribute__ ((packed));

typedef struct _DESIRED_RATE
{
	u32 Length;                 //cannot be more than 8;        
	u8 DesiredRate[MAX_SIZE_ARRAY];    //max. 32bytes
} DESIRED_RATE __attribute__ ((packed));

//
//          IEEE 802.11 Structures and definitions
//

// jeff.spurgat 2003-04-02
// these definitions already provided in WinCE provided headers

typedef enum _WLAN_802_11_NETWORK_TYPE
{
	Wlan802_11FH,
	Wlan802_11DS,
	Wlan802_11NetworkTypeMax	// not a real type, 
					//defined as an upper bound
} WLAN_802_11_NETWORK_TYPE, *PWLAN_802_11_NETWORK_TYPE
  __attribute__ ((packed));

typedef struct _WLAN_802_11_NETWORK_TYPE_LIST
{
	u32 NumberOfItems;          // in list below, at least 1
	WLAN_802_11_NETWORK_TYPE NetworkType[1];
} WLAN_802_11_NETWORK_TYPE_LIST, *PWLAN_802_11_NETWORK_TYPE_LIST
  __attribute__ ((packed));

/*
 *          Received Signal Strength Indication
 */
typedef LONG WLAN_802_11_RSSI;  // in dBm

typedef struct _WLAN_802_11_CONFIGURATION_FH
{
	u32 Length;                 // Length of structure
	u32 HopPattern;             // As defined by 802.11, MSB set
	u32 HopSet;                 // to one if non-802.11
	u32 DwellTime;              // units are Kusec
} WLAN_802_11_CONFIGURATION_FH, *PWLAN_802_11_CONFIGURATION_FH
  __attribute__ ((packed));

typedef struct _WLAN_802_11_CONFIGURATION
{
	u32 Length;                 // Length of structure
	u32 BeaconPeriod;           // units are Kusec
	u32 ATIMWindow;             // units are Kusec
	u32 DSConfig;               // Frequency, units are kHz
	WLAN_802_11_CONFIGURATION_FH FHConfig;
} WLAN_802_11_CONFIGURATION, *PWLAN_802_11_CONFIGURATION
  __attribute__ ((packed));

typedef struct _WLAN_802_11_STATISTICS
{
	u32 Length;                 // Length of structure
	LARGE_INTEGER TransmittedFragmentCount;
	LARGE_INTEGER MulticastTransmittedFrameCount;
	LARGE_INTEGER FailedCount;
	LARGE_INTEGER RetryCount;
	LARGE_INTEGER MultipleRetryCount;
	LARGE_INTEGER RTSSuccessCount;
	LARGE_INTEGER RTSFailureCount;
	LARGE_INTEGER ACKFailureCount;
	LARGE_INTEGER FrameDuplicateCount;
	LARGE_INTEGER ReceivedFragmentCount;
	LARGE_INTEGER MulticastReceivedFrameCount;
	LARGE_INTEGER FCSErrorCount;
} WLAN_802_11_STATISTICS, *PWLAN_802_11_STATISTICS __attribute__ ((packed));

typedef u32 WLAN_802_11_KEY_INDEX;

typedef struct _WLAN_802_11_WEP
{
	u32 Length;		// Length of this structure
	u32 KeyIndex;		// 0 is the per-client key, 1-N are the
				// global keys
	u32 KeyLength;	// length of key in bytes
	u8 KeyMaterial[1];	// variable length depending on above field
} WLAN_802_11_WEP, *PWLAN_802_11_WEP __attribute__ ((packed));


typedef enum _WLAN_802_11_NETWORK_INFRASTRUCTURE
{
	Wlan802_11IBSS,
	Wlan802_11Infrastructure,
	Wlan802_11AutoUnknown,
	Wlan802_11InfrastructureMax	// Not a real value, 
					// defined as upper bound
} WLAN_802_11_NETWORK_INFRASTRUCTURE, *PWLAN_802_11_NETWORK_INFRASTRUCTURE;

typedef enum _WLAN_802_11_AUTHENTICATION_MODE
{
	Wlan802_11AuthModeOpen = 0x00,
	Wlan802_11AuthModeShared = 0x01,
	Wlan802_11AuthModeNetworkEAP = 0x80,
} WLAN_802_11_AUTHENTICATION_MODE, *PWLAN_802_11_AUTHENTICATION_MODE;

typedef u8 WLAN_802_11_RATES[14] __attribute__ ((packed));    // Set of 8 data rates
typedef u8 WLAN_802_11_MAC_ADDRESS[6] __attribute__ ((packed));

#define B_SUPPORTED_RATES		8
#define G_SUPPORTED_RATES		14

#ifdef MULTI_BANDS
#define A_SUPPORTED_RATES		14 /* TODO: Check max support rates */
#endif

#define	WLAN_SUPPORTED_RATES		14

#define WLAN_MAX_SSID_LENGTH		32

typedef struct _WLAN_802_11_SSID
{
	u32 SsidLength;	// length of SSID field below, in bytes;
				// this can be zero.
	u8 Ssid[WLAN_MAX_SSID_LENGTH];	// SSID information field
} WLAN_802_11_SSID, *PWLAN_802_11_SSID __attribute__ ((packed));

typedef struct _WLAN_802_11_BSSID
{
	u32 Length;                 	// Length of this structure
	WLAN_802_11_MAC_ADDRESS MacAddress;   // BSSID
	u8 Reserved[2];
	WLAN_802_11_SSID Ssid;        	// SSID
	u32 Privacy;                	// WEP encryption requirement
	WLAN_802_11_RSSI Rssi;        	// receive signal
	u32			Channel;
	// strength in dBm
	WLAN_802_11_NETWORK_TYPE NetworkTypeInUse;
	WLAN_802_11_CONFIGURATION Configuration;
	WLAN_802_11_NETWORK_INFRASTRUCTURE InfrastructureMode;
	WLAN_802_11_RATES SupportedRates;
#ifdef FWVERSION3
	u32		IELength;
	u8		IEs[1];
#endif
#ifdef MULTI_BANDS
	u16 bss_band;	/* Network band.
				   BAND_B(0x01): 'b' band   
				   BAND_G(0x02): 'g' band    
				   BAND_A(0X04): 'a' band	*/
#endif
	int extra_ie;
} WLAN_802_11_BSSID, *PWLAN_802_11_BSSID __attribute__ ((packed));

typedef struct _WLAN_802_11_BSSID_LIST
{
	u32 NumberOfItems;          // in list below, at least 1
	WLAN_802_11_BSSID Bssid[1];
} WLAN_802_11_BSSID_LIST, *PWLAN_802_11_BSSID_LIST __attribute__ ((packed));

typedef u32 WLAN_802_11_FRAGMENTATION_THRESHOLD;

typedef u32 WLAN_802_11_RTS_THRESHOLD;

typedef u32 WLAN_802_11_ANTENNA;

typedef enum _WLAN_802_11_PRIVACY_FILTER
{
	Wlan802_11PrivFilterAcceptAll,
	Wlan802_11PrivFilter8021xWEP
} WLAN_802_11_PRIVACY_FILTER, *PWLAN_802_11_PRIVACY_FILTER;

typedef enum _WLAN_802_11_WEP_STATUS
{
	Wlan802_11WEPEnabled,
	Wlan802_11WEPDisabled,
	Wlan802_11WEPKeyAbsent,
	Wlan802_11WEPNotSupported
#ifdef WPA
	,Wlan802_11Encryption2Enabled,
	Wlan802_11Encryption2KeyAbsent,
	Wlan802_11Encryption3Enabled,
	Wlan802_11Encryption3KeyAbsent
#endif
} WLAN_802_11_WEP_STATUS, *PWLAN_802_11_WEP_STATUS
#ifdef WPA
,WLAN_802_11_ENCRYPTION_STATUS, *PWLAN_802_11_ENCRYPTION_STATUS;
#else
;
#endif
 

typedef enum _WLAN_802_11_RELOAD_DEFAULTS
{
	Wlan802_11ReloadWEPKeys
} WLAN_802_11_RELOAD_DEFAULTS, *PWLAN_802_11_RELOAD_DEFAULTS;

typedef struct _wlan_offset_value {
	u32	offset;
	u32	value;
} wlan_offset_value;

#ifdef FWVERSION3
#define WLAN_802_11_AI_REQFI_CAPABILITIES 	1
#define WLAN_802_11_AI_REQFI_LISTENINTERVAL 	2
#define WLAN_802_11_AI_REQFI_CURRENTAPADDRESS 	4

#define WLAN_802_11_AI_RESFI_CAPABILITIES 	1
#define WLAN_802_11_AI_RESFI_STATUSCODE 	2
#define WLAN_802_11_AI_RESFI_ASSOCIATIONID 	4

typedef struct _WLAN_802_11_FIXED_IEs {
	u8	Timestamp[8];
	u16	BeaconInterval;
	u16	Capabilities;
} WLAN_802_11_FIXED_IEs, *PWLAN_802_11_FIXED_IEs;

typedef struct _WLAN_802_11_VARIABLE_IEs {
	u8	ElementID;
	u8	Length;
	u8	data[1];
} WLAN_802_11_VARIABLE_IEs, *PWLAN_802_11_VARIABLE_IEs;

typedef struct _WLAN_802_11_AI_RESFI {
	u16	Capabilities;
	u16	StatusCode;
	u16	AssociationId;
} WLAN_802_11_AI_RESFI, *PWLAN_802_11_AI_RESFI;

typedef struct _WLAN_802_11_AI_REQFI {
	u16	Capabilities;
	u16	ListenInterval;
	WLAN_802_11_MAC_ADDRESS	CurrentAPAddress;
} WLAN_802_11_AI_REQFI, *PWLAN_802_11_AI_REQFI;

typedef struct _WLAN_802_11_ASSOCIATION_INFORMATION {
	u32			Length;
	u16			AvailableRequestFixedIEs;
	WLAN_802_11_AI_REQFI	RequestFixedIEs;
	u32			RequestIELength;
	u32			OffsetRequestIEs;
	u16			AvailableResponseFixedIEs;
	WLAN_802_11_AI_RESFI	ResponseFixedIEs;
	u32			ResponseIELength;
	u32			OffsetResponseIEs;
} WLAN_802_11_ASSOCIATION_INFORMATION, *PWLAN_802_11_ASSOCIATION_INFORMATION;

typedef enum _WLAN_802_11_STATUS_TYPE {
	Wlan802_11StatusType_Authentication,
	Wlan802_11StatusTypeMax
} WLAN_802_11_STATUS_TYPE, *PWLAN_802_11_STATUS_TYPE;

typedef struct _WLAN_802_11_STATUS_INDICATION {
	WLAN_802_11_STATUS_TYPE StatusType;
} WLAN_802_11_STATUS_INDICATION, *PWLAN_802_11_STATUS_INDICATION;

typedef struct _WLAN_802_11_AUTHENTICATION_REQUEST {
	u32	Length;
	WLAN_802_11_MAC_ADDRESS	Bssid;
	u32	Flags;
} WLAN_802_11_AUTHENTICATION_REQUEST, *PWLAN_802_11_AUTHENTICATION_REQUEST;

typedef struct _WLAN_802_11_AUTHENTICATION_EVENT {
	WLAN_802_11_STATUS_INDICATION 	Status;
	WLAN_802_11_AUTHENTICATION_REQUEST	Request[1];
}WLAN_802_11_AUTHENTICATION_EVENT, *PWLAN_802_11_AUTHENTICATION_EVENT;
#endif /* FWVERSION3 */

/* Linux Dynamic Power Management,
 *  If the user has enabled the deep sleep, user has the highest privilege.
 *  The Kernel PM takes the next priority
 */
enum {
	DEEP_SLEEP_DISABLED,
	DEEP_SLEEP_ENABLED,
	DEEP_SLEEP_OS_ENABLED,
	DEEP_SLEEP_USER_ENABLED
};
enum {
	PM_DISABLED,
	PM_ENABLED
};

#endif
