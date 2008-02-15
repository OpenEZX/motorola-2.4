/* 
 * File : wlan_types.h
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the license.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
*/

#ifndef _WLAN_TYPES_
#define _WLAN_TYPES_

#define PROPRIETARY_TLV_BASE_ID		0x0100

/* Terminating TLV Type */
#define MRVL_TERMINATE_TLV_ID		0xffff

/* TLV  type ID definition */
#define TLV_TYPE_SSID				0x0000
#define TLV_TYPE_RATES				0x0001
#define TLV_TYPE_PHY_FH				0x0002
#define TLV_TYPE_PHY_DS				0x0003
#define TLV_TYPE_CF				    0x0004
#define TLV_TYPE_IBSS				0x0006

#ifdef ENABLE_802_11D
#define TLV_TYPE_DOMAIN				0x0007
#endif

#ifdef ENABLE_802_11H_TPC
#define TLV_TYPE_POWER_CONSTRAINT	0x0020
#endif
#define TLV_TYPE_POWER_CAPABILITY	0x0021

#define TLV_TYPE_KEY_MATERIAL			(PROPRIETARY_TLV_BASE_ID + 0)
#define TLV_TYPE_CHANLIST			(PROPRIETARY_TLV_BASE_ID + 1)
#define TLV_TYPE_NUMPROBES			(PROPRIETARY_TLV_BASE_ID + 2)
#define TLV_TYPE_RSSI				(PROPRIETARY_TLV_BASE_ID + 4)
#define TLV_TYPE_SNR				(PROPRIETARY_TLV_BASE_ID + 5)
#define TLV_TYPE_FAILCOUNT			(PROPRIETARY_TLV_BASE_ID + 6)
#define TLV_TYPE_BCNMISS			(PROPRIETARY_TLV_BASE_ID + 7)
#define TLV_TYPE_LED_GPIO			(PROPRIETARY_TLV_BASE_ID + 8)
#define TLV_TYPE_LEDBEHAVIOR			(PROPRIETARY_TLV_BASE_ID + 9)
#define TLV_TYPE_PASSTHROUGH			(PROPRIETARY_TLV_BASE_ID + 10)
#define TLV_TYPE_REASSOCAP			(PROPRIETARY_TLV_BASE_ID + 11)
#define TLV_TYPE_POWER_TBL_2_4GHZ		(PROPRIETARY_TLV_BASE_ID + 12)
#define TLV_TYPE_POWER_TBL_5GHZ			(PROPRIETARY_TLV_BASE_ID + 13)
#define TLV_TYPE_BCASTPROBE			(PROPRIETARY_TLV_BASE_ID + 14)
#define TLV_TYPE_NUMSSID_PROBE			(PROPRIETARY_TLV_BASE_ID + 15)
#define TLV_TYPE_WMMQSTATUS			(PROPRIETARY_TLV_BASE_ID + 16)
#define TLV_TYPE_CRYPTO_DATA	        	(PROPRIETARY_TLV_BASE_ID + 17)
#define TLV_TYPE_WILDCARDSSID	    		(PROPRIETARY_TLV_BASE_ID + 18)
#ifdef AUTO_TX
#define TLV_TYPE_AUTO_TX		    	(PROPRIETARY_TLV_BASE_ID + 24)
#endif

typedef struct _MrvlIEtypesHeader {
	u16	Type;
	u16	Len;
} __ATTRIB_PACK__ MrvlIEtypesHeader_t;


typedef struct _MrvlIEtypesGeneric {
	MrvlIEtypesHeader_t Header;
	u8 Data[1];
} __ATTRIB_PACK__ MrvlIEtypesGeneric_t;


typedef struct 	_MrvlIEtypes_RatesParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	Rates[1];
} __ATTRIB_PACK__ MrvlIEtypes_RatesParamSet_t;

typedef struct 	_MrvlIEtypes_SsIdParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	SsId[1];
} __ATTRIB_PACK__ MrvlIEtypes_SsIdParamSet_t;

typedef struct 	_MrvlIEtypes_WildCardSsIdParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8  MaxSsidLength;
	u8	SsId[1];
} __ATTRIB_PACK__ MrvlIEtypes_WildCardSsIdParamSet_t;

/** ChanScanMode_t */
typedef struct {
#ifdef BIG_ENDIAN
	u8 Reserved_2_7    : 6;
	u8 DisableChanFilt : 1;
	u8 PassiveScan     : 1;
#else
	u8 PassiveScan     : 1;
	u8 DisableChanFilt : 1;
	u8 Reserved_2_7    : 6;
#endif
} __ATTRIB_PACK__ ChanScanMode_t;

/** ChanScanParamSet_t */
typedef struct _ChanScanParamSet_t{
	u8	           RadioType;
	u8	           ChanNumber;
	ChanScanMode_t ChanScanMode;
	u16            MinScanTime;
	u16            MaxScanTime;
} __ATTRIB_PACK__ ChanScanParamSet_t;

typedef struct 	_MrvlIEtypes_ChanListParamSet_t {
	MrvlIEtypesHeader_t	Header;
	ChanScanParamSet_t 	ChanScanParam[1];
} __ATTRIB_PACK__ MrvlIEtypes_ChanListParamSet_t;

#ifdef TLV_ASSOCIATION
typedef struct _CfParamSet_t {
	u8	CfpCnt;
	u8	CfpPeriod;
	u16	CfpMaxDuration;
	u16	CfpDurationRemaining;
} __ATTRIB_PACK__ CfParamSet_t;

typedef struct _IbssParamSet_t {
	u16	AtimWindow;
} __ATTRIB_PACK__ IbssParamSet_t;

typedef struct _MrvlIEtypes_SsParamSet_t {
	MrvlIEtypesHeader_t	Header;
	union {
		CfParamSet_t	CfParamSet[1];
		IbssParamSet_t	IbssParamSet[1];
	} cf_ibss;
} __ATTRIB_PACK__ MrvlIEtypes_SsParamSet_t;

typedef struct _FhParamSet_t {
	u16	DwellTime;
	u8	HopSet;
	u8	HopPattern;
	u8	HopIndex;
} __ATTRIB_PACK__ FhParamSet_t;

typedef struct _DsParamSet_t {
	u8	CurrentChan;
} __ATTRIB_PACK__ DsParamSet_t;

typedef struct _MrvlIEtypes_PhyParamSet_t {
	MrvlIEtypesHeader_t	Header;
	union {
		FhParamSet_t	FhParamSet[1];
		DsParamSet_t	DsParamSet[1];
	} fh_ds;
} __ATTRIB_PACK__ MrvlIEtypes_PhyParamSet_t;

/*  Local Power Capability */
typedef struct _MrvlIEtypes_PowerCapability_t {
	MrvlIEtypesHeader_t	Header;
	s8 	MinPower;
	s8	MaxPower;
} __ATTRIB_PACK__ MrvlIEtypes_PowerCapability_t;

#ifdef REASSOCIATION_SUPPORT
typedef struct _MrvlIEtypes_ReassocAp_t
{
	MrvlIEtypesHeader_t	Header;
    WLAN_802_11_MAC_ADDRESS currentAp;

} __ATTRIB_PACK__ MrvlIEtypes_ReassocAp_t;
#endif

#ifdef  WPA
typedef struct 	_MrvlIEtypes_RsnParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	RsnIE[1];
} __ATTRIB_PACK__ MrvlIEtypes_RsnParamSet_t;
#endif

#ifdef  WMM
typedef struct 	_MrvlIEtypes_WmmParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	WmmIE[1];
} __ATTRIB_PACK__ MrvlIEtypes_WmmParamSet_t;
#endif
#endif /* TLV_ASSOCIATION */

#ifdef  WMM
typedef struct {
	MrvlIEtypesHeader_t	Header;
    u8  QueueIndex;
	u8	Disabled;
	u8	TriggeredPS;
	u8	FlowDirection;
	u8	FlowRequired;
	u8	FlowCreated;
	u32	MediumTime;
} __ATTRIB_PACK__ MrvlIEtypes_WmmQueueStatus_t;
#endif

typedef struct _MrvlIEtypes_RssiThreshold_t {
	MrvlIEtypesHeader_t	Header;
	u8			RSSIValue;
	u8			RSSIFreq;
} __ATTRIB_PACK__ MrvlIEtypes_RssiParamSet_t;

typedef struct _MrvlIEtypes_SnrThreshold_t {
	MrvlIEtypesHeader_t	Header;
	u8			SNRValue;
	u8			SNRFreq;
} __ATTRIB_PACK__ MrvlIEtypes_SnrThreshold_t;

typedef struct _MrvlIEtypes_FailureCount_t {
	MrvlIEtypesHeader_t	Header;
	u8			FailValue;
	u8			FailFreq;
} __ATTRIB_PACK__ MrvlIEtypes_FailureCount_t;

typedef struct _MrvlIEtypes_BeaconsMissed_t {
	MrvlIEtypesHeader_t	Header;
	u8			BeaconMissed;
	u8			Reserved;
} __ATTRIB_PACK__ MrvlIEtypes_BeaconsMissed_t;

#ifdef TLV_SCAN
typedef struct 	_MrvlIEtypes_NumProbes_t {
	MrvlIEtypesHeader_t	Header;
	u16 			NumProbes;
} __ATTRIB_PACK__ MrvlIEtypes_NumProbes_t;

typedef struct 	_MrvlIEtypes_BcastProbe_t {
	MrvlIEtypesHeader_t	Header;
	u16 			BcastProbe;
} __ATTRIB_PACK__ MrvlIEtypes_BcastProbe_t;

typedef struct 	_MrvlIEtypes_NumSSIDProbe_t {
	MrvlIEtypesHeader_t	Header;
	u16 			NumSSIDProbe;
} __ATTRIB_PACK__ MrvlIEtypes_NumSSIDProbe_t;
#endif /* TLV_SCAN */

#ifdef LED_GPIO_CTRL
typedef struct {
  u8 Led;
  u8 Pin;
} __ATTRIB_PACK__ Led_Pin;

typedef struct 	_MrvlIEtypes_LedGpio_t {
	MrvlIEtypesHeader_t	Header;
	Led_Pin			LedPin[1];
} __ATTRIB_PACK__ MrvlIEtypes_LedGpio_t;
#endif

#ifdef AUTO_TX
typedef struct 	_AutoTx_MacFrame_t {
	u16			Interval;	/* in seconds */
	u8			Priority;	/* User Priority: 0~7, ignored if non-WMM */
	u8			Reserved;	/* set to 0 */
	u16			FrameLen;	/* Length of MAC frame payload */
	u8			DestMacAddr[ETH_ALEN];
	u8			SrcMacAddr[ETH_ALEN];
	u8			Payload[];
} __ATTRIB_PACK__ AutoTx_MacFrame_t;

/** MrvlIEtypes_PA_Group_t */
typedef struct 	_MrvlIEtypes_AutoTx_t {
	MrvlIEtypesHeader_t	Header;
	AutoTx_MacFrame_t	AutoTx_MacFrame;
} __ATTRIB_PACK__ MrvlIEtypes_AutoTx_t;
#endif

#endif /* _WLAN_TYPES_ */

