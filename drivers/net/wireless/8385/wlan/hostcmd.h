/*
 *      File: 	hostcmd.h
 *
 *      This file contains the function prototypes, data structure and defines
 *	for all the host/station commands. Please check the 802.11
 *	GUI/Driver/Station Interface Specification for detailed command
 *	information
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
 *****************************************************************************/

#ifndef __HOSTCMD__H
#define __HOSTCMD__H


/*
 * 	IEEE scan.confirm data structure borrowed from station FW code
 * 	Reference: IEEE 802.11 1999 Spec 10.3.2.2.2
 */

/*	In station FW code, enum type is 1 byte u8 */

typedef enum
{
    SSID = 0,
    SUPPORTED_RATES,
    FH_PARAM_SET,
    DS_PARAM_SET,
    CF_PARAM_SET,
    TIM,
    IBSS_PARAM_SET,
#ifdef ENABLE_802_11D
    COUNTRY_INFO = 7,
#endif

    CHALLENGE_TEXT = 16,

#ifdef ENABLE_802_11H_TPC
    POWER_CONSTRAINT = 32,
    POWER_CAPABILITY = 33,
    TPC_REQUEST  = 34,
    TPC_REPORT   = 35,
#endif

#ifdef FWVERSION3
    EXTENDED_SUPPORTED_RATES = 50,
#endif
    
    VENDOR_SPECIFIC_221 = 221,
#ifdef WMM
    WMM_IE = 221,
#endif /* WMM */
#ifdef WPA
    WPA_IE = 221,
#ifdef WPA2
    WPA2_IE = 48,
#endif  //WPA2
#endif  //WPA
    EXTRA_IE = 133,
} __ATTRIB_PACK__ IEEEtypes_ElementId_e;

#ifdef WMM
#define WMM_OUI_TYPE  2
#endif /* WMM */

#define CAPINFO_MASK    (~( W_BIT_15 | W_BIT_14 |               \
                            W_BIT_12 | W_BIT_11 | W_BIT_9) )

#ifdef BIG_ENDIAN
typedef struct IEEEtypes_CapInfo_t {
	u8	Rsrvd1:2;
	u8	DSSSOFDM:1;
	u8	Rsvrd2:2;
	u8	ShortSlotTime:1;
	u8	Rsrvd3:1;
	u8	SpectrumMgmt:1;
	u8	ChanAgility:1;
	u8	Pbcc:1;
	u8	ShortPreamble:1;
	u8	Privacy:1;
	u8	CfPollRqst:1;
	u8	CfPollable:1;
	u8	Ibss:1;
	u8	Ess:1;
} __ATTRIB_PACK__ IEEEtypes_CapInfo_t;
#else
typedef struct IEEEtypes_CapInfo_t {
	u8	Ess:1;
	u8	Ibss:1;
	u8	CfPollable:1;
	u8	CfPollRqst:1;
	u8	Privacy:1;
	u8	ShortPreamble:1;
	u8	Pbcc:1;
	u8	ChanAgility:1;
	u8	SpectrumMgmt:1;
	u8	Rsrvd3:1;
	u8	ShortSlotTime:1;
	u8	Apsd:1;
	u8	Rsvrd2:1;
	u8	DSSSOFDM:1;
	u8	Rsrvd1:2;
} __ATTRIB_PACK__ IEEEtypes_CapInfo_t;

#endif  /* BIG_ENDIAN */

typedef struct IEEEtypes_CfParamSet_t {
	u8	ElementId;
	u8	Len;
	u8	CfpCnt;
	u8	CfpPeriod;
	u16	CfpMaxDuration;
	u16	CfpDurationRemaining;
} __ATTRIB_PACK__ IEEEtypes_CfParamSet_t;

typedef struct IEEEtypes_IbssParamSet_t {
	u8	ElementId;
	u8	Len;
	u16	AtimWindow;
} __ATTRIB_PACK__ IEEEtypes_IbssParamSet_t;

typedef union IEEEtypes_SsParamSet_t {
	IEEEtypes_CfParamSet_t		CfParamSet;
	IEEEtypes_IbssParamSet_t	IbssParamSet;
} __ATTRIB_PACK__ IEEEtypes_SsParamSet_t;

typedef struct IEEEtypes_FhParamSet_t {
	u8	ElementId;
	u8	Len;
	u16	DwellTime;
	u8	HopSet;
	u8	HopPattern;
	u8	HopIndex;
} __ATTRIB_PACK__ IEEEtypes_FhParamSet_t;

typedef struct IEEEtypes_DsParamSet_t {
	u8	ElementId;
	u8	Len;
	u8	CurrentChan;
} __ATTRIB_PACK__ IEEEtypes_DsParamSet_t;

typedef union IEEEtypes_PhyParamSet_t {
	IEEEtypes_FhParamSet_t	FhParamSet;
	IEEEtypes_DsParamSet_t	DsParamSet;
} __ATTRIB_PACK__ IEEEtypes_PhyParamSet_t;

/////////////////////////////////////////////////////////////////
//
//                  802.11-related definitions
//
/////////////////////////////////////////////////////////////////


//
// Define TxPD and RxPD
//
//          TxPD descriptor
//
//          Status     : Current Tx packet transmit status
//          PktPtr     : Physical address of the Tx Packet on host PC memory
//          PktLen		: Tx packet length
//          u8 DestMACAdrHi[2]	: First 2 byte of destination MAC address
//          DestMACAdrLow[4]	: Last 4 byte of destination MAC address
//          DataRate		: Driver uses this field to specify data
//				: rate for the Tx packet
//          NextTxPDPtr            : Address to next TxPD (Used by the driver)
//

typedef struct _TxPD
{
	u32	TxStatus;
	u32	TxControl;
	u32	TxPacketLocation;
	u16	TxPacketLength;
	u8	TxDestAddrHigh[2];
	u8	TxDestAddrLow[4];
#ifdef WMM
	u8	Priority;
	u8	PowerMgmt;
	u8	Reserved[2];
#endif /* WMM */
//	struct _TxPD *NextTxPDPtr;
} __ATTRIB_PACK__ TxPD, *PTxPD;	// size = 32 bytes

//	RxPD descirptor
//                      
//	Control     : Ownership indication
//	RSSI        : FW uses this field to report last Rx packet RSSI
//	Status      : Rx packet type
//	PktLen      : FW uses this field to report last Rx packet length 
//	SQBody      : Signal quality for packet body
//	Rate        : In FW. 
//	PktPtr      : Driver uses this field to report host memory buffer size
//	NextRxPDPtr : Physical address of the next RxPD
//

typedef struct _RxPD {
	u16	Status;
	u8	SNR;
	u8	RxControl;
	u16	PktLen;
	u8	NF;	/* Noise Floor */
	u8	RxRate;
	u32	PktPtr;
	u32	NextRxPDPtr;
#ifdef WMM
	u8	Priority;
	u8	Reserved[3];
#endif /* WMM */
} __ATTRIB_PACK__ RxPD, *PRxPD;


//
//	Tx control node data structure 
//
//	Tx control node is used by the driver to keep ready to send Tx packet
//	information.
//
//	Status         :
//	NPTxPacket     : Packet reference
//	LocalTxPD       : Local TxPD reference
//	SQTxPD          : On-chip SQ memory TxPD reference
//	BufVirtualAddr : Tx staging buffer logical address
//	BufPhyAddr     : Tx staging buffer physical address
//	NextNode       : link to next Tx control node
//

#if defined(__KERNEL__) || !defined(linux)
//
//	Next              : Link to nect command control node
//	Status            : Current command execution status
//	PendingOID        : Set or query OID passed
//	PendingInfo       : Init, Reset, Set OID, Get OID, Host command, etc.
//	INTOption         : USE_INT or NO_INT
//	BytesWritten      : For async OID processing
//	BytesRead         : For async OID processing
//	BytesNeeded       : For async OID processing
//	InformationBuffer : For async OID processing
//	BufVirtualAddr    : Command buffer logical address
//	BufPhyAddr        : Command buffer physical address
//

typedef struct _CmdCtrlNode
{
	struct list_head list;
	u32	Status;
	u32	PendingOID;
	u16	PendingInfo;	// Init, Reset, Set OID,
						// Get OID, Host command, etc.
	u16 	INTOption;

	void 	*InformationBuffer; // For async OID processing
	u8 	*BufVirtualAddr;
	long long	BufPhyAddr;

	int	retcode;	/* Return status of the command */
    
	u16			CmdFlags;

	u16			CmdWaitQWoken;
	wait_queue_head_t	cmdwait_q __ATTRIB_ALIGN__;	// This has to be the last entry
} __ATTRIB_PACK__ CmdCtrlNode, *PCmdCtrlNode;

#endif

typedef struct _MRVL_WEP_KEY
{	// Based on WLAN_802_11_WEP, we extend the WEP buffer length to 128 bits
	u32 Length;
	u32 KeyIndex;
	u32 KeyLength;
	u8 KeyMaterial[MRVL_KEY_BUFFER_SIZE_IN_BYTE];
} __ATTRIB_PACK__ MRVL_WEP_KEY, *PMRVL_WEP_KEY;

typedef ULONGLONG       WLAN_802_11_KEY_RSC;

typedef struct _WLAN_802_11_KEY
{
	u32	Length;
	u32   KeyIndex;
	u32   KeyLength;
	WLAN_802_11_MAC_ADDRESS BSSID;
	WLAN_802_11_KEY_RSC KeyRSC;
	u8   KeyMaterial[MRVL_MAX_KEY_WPA_KEY_LENGTH];
} __ATTRIB_PACK__ WLAN_802_11_KEY, *PWLAN_802_11_KEY;

#ifdef WPA 
typedef struct _MRVL_WPA_KEY {
	u32   KeyIndex;
	u32   KeyLength;
	u32	KeyRSC;
	u8   KeyMaterial[MRVL_MAX_KEY_WPA_KEY_LENGTH];
} MRVL_WPA_KEY, *PMRVL_WPA_KEY;

typedef struct _MRVL_WLAN_WPA_KEY {
	u8   EncryptionKey[16];
	u8   MICKey1[8];
	u8   MICKey2[8];
} MRVL_WLAN_WPA_KEY, *PMRVL_WLAN_WPA_KEY;

typedef struct _IE_WPA {
	u8	Elementid;
	u8	Len;
	u8	oui[4];
	u16	version;
} IE_WPA,*PIE_WPA;

#endif /* WPA */

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
  __ATTRIB_PACK__;

typedef struct _WLAN_802_11_CONFIGURATION
{
	u32 Length;                 // Length of structure
	u32 BeaconPeriod;           // units are Kusec
	u32 ATIMWindow;             // units are Kusec
	u32 DSConfig;               // Frequency, units are kHz
	WLAN_802_11_CONFIGURATION_FH FHConfig;
} WLAN_802_11_CONFIGURATION, *PWLAN_802_11_CONFIGURATION
  __ATTRIB_PACK__;


typedef struct _WLAN_802_11_WEP
{
	u32 Length;		// Length of this structure
	u32 KeyIndex;		// 0 is the per-client key, 1-N are the
				// global keys
	u32 KeyLength;	// length of key in bytes
	u8 KeyMaterial[1];	// variable length depending on above field
} WLAN_802_11_WEP, *PWLAN_802_11_WEP __ATTRIB_PACK__;

typedef struct _WLAN_802_11_SSID {
	u32 SsidLength;	// length of SSID field below, in bytes;
				// this can be zero.
	u8 Ssid[WLAN_MAX_SSID_LENGTH];	// SSID information field
} WLAN_802_11_SSID, *PWLAN_802_11_SSID __ATTRIB_PACK__;

typedef struct _WPA_SUPPLICANT {
	u8 	Wpa_ie[256];
	u8	Wpa_ie_len;
} WPA_SUPPLICANT, *PWPA_SUPPLICANT;

typedef struct _WLAN_802_11_BSSID {
	u32 Length;                 	// Length of this structure
	WLAN_802_11_MAC_ADDRESS MacAddress;   // BSSID
	u8 Reserved[2];
	WLAN_802_11_SSID Ssid;        	// SSID
	u32 Privacy;                	// WEP encryption requirement
	WLAN_802_11_RSSI Rssi;        	// receive signal
	u32 Channel;
	// strength in dBm
	WLAN_802_11_NETWORK_TYPE NetworkTypeInUse;
	WLAN_802_11_CONFIGURATION Configuration;
	WLAN_802_11_NETWORK_INFRASTRUCTURE InfrastructureMode;
	WLAN_802_11_RATES SupportedRates;
#ifdef WMM
	u8 Wmm_IE[WMM_PARA_IE_LENGTH + 2];
	u8 Wmm_ie_len;
#endif /* WMM */
#ifdef FWVERSION3
	u32 IELength;
	u8 IEs[1];
#endif
#ifdef MULTI_BANDS
	u16 bss_band;	/* Network band.
				   BAND_B(0x01): 'b' band   
				   BAND_G(0x02): 'g' band    
				   BAND_A(0X04): 'a' band	*/
#endif
	int extra_ie;

	u8 TimeStamp[8];
	IEEEtypes_PhyParamSet_t	PhyParamSet;
	IEEEtypes_SsParamSet_t	SsParamSet;
	IEEEtypes_CapInfo_t	Cap;
	u8 DataRates[WLAN_SUPPORTED_RATES];

#ifdef ENABLE_802_11H_TPC
	u8 Sensed11H;
	IEEEtypes_PowerConstraint_t PowerConstraint;
	IEEEtypes_PowerCapability_t PowerCapability;
	IEEEtypes_TPCReport_t TPCReport;
#endif

#ifdef ENABLE_802_11D
	IEEEtypes_CountryInfoFullSet_t CountryInfo;
#endif

#ifdef WPA
	WPA_SUPPLICANT		wpa_supplicant;
#ifdef WPA2
	WPA_SUPPLICANT		wpa2_supplicant;
#endif /* WPA2 */

#ifdef CCX
    CCX_BSS_Info_t ccx_bss_info;
#endif

#endif 

} WLAN_802_11_BSSID, *PWLAN_802_11_BSSID __ATTRIB_PACK__;

typedef u32 WLAN_802_11_FRAGMENTATION_THRESHOLD;
typedef u32 WLAN_802_11_RTS_THRESHOLD;
typedef u32 WLAN_802_11_ANTENNA;

typedef struct _wlan_offset_value {
	u32	offset;
	u32	value;
} wlan_offset_value;

#ifdef FWVERSION3
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

#endif /* FWVERSION3 */

/* Linux Dynamic Power Management,
 *  If the user has enabled the deep sleep, user has the highest privilege.
 *  The Kernel PM takes the next priority
 */
/*
 *	Definition of data structure for each command
 */

/* Define general data structure */
typedef struct _HostCmd_DS_GEN {
	u16	Command;
	u16	Size;
	u16	SeqNum;
	u16	Result;
} __ATTRIB_PACK__ HostCmd_DS_GEN, *PHostCmd_DS_GEN
#if defined(DEEP_SLEEP_CMD)
    ,HostCmd_DS_802_11_DEEP_SLEEP, *PHostCmd_DS_802_11_DEEP_SLEEP
#endif /* DEEP_SLEEP_CMD */
	;

/*
 * Define data structure for HostCmd_CMD_GET_HW_SPEC
 * This structure defines the response for the GET_HW_SPEC command
 */

typedef struct _HostCmd_DS_GET_HW_SPEC
{
	u16	HWIfVersion;		// HW Interface version number
	u16	Version;		// HW version number
	u16	NumOfTxPD;		// Max number of TxPD FW can handle
	u16	NumOfMCastAdr;		// Max no of Multicast address 
					// FW can handle
	u8	PermanentAddr[6];	// MAC address
	u16	RegionCode;		// Region Code
	u16	NumberOfAntenna;	// Number of antenna used
	u32	FWReleaseNumber;	// FW release number, 
					// example 0x1234=1.2.3.4
	u32	WcbBase; 		// Base Address of TxPD queue
	u32	RxPdRdPtr;		// Read Pointer of RxPd queue
	u32	RxPdWrPtr;		// Write Pointer of RxPd queue
#ifdef FWVERSION3
	u32	fwCapInfo;
#endif
} __ATTRIB_PACK__ HostCmd_DS_GET_HW_SPEC, *PHostCmd_DS_GET_HW_SPEC;

/* Define data structure for HostCmd_CMD_EEPROM_UPDATE */
typedef struct _HostCmd_DS_EEPROM_UPDATE {
	u16	Action;                // Detailed action or option
	u32	Value;
} __ATTRIB_PACK__ HostCmd_DS_EEPROM_UPDATE, *PHostCmd_DS_EEPROM_UPDATE;

/* Define data structure for HostCmd_CMD_802_11_RESET */
typedef struct _HostCmd_DS_802_11_RESET
{
	u16	Action;		// ACT_NOT_REVERT_MIB, ACT_REVERT_MIB
				// or ACT_HALT
} __ATTRIB_PACK__ HostCmd_DS_802_11_RESET, *PHostCmd_DS_802_11_RESET;

#ifdef SUBSCRIBE_EVENT_CTRL
typedef struct _HostCmd_DS_802_11_SUBSCRIBE_EVENT {
	u16	Action;
	u16	Events;
} __ATTRIB_PACK__ HostCmd_DS_802_11_SUBSCRIBE_EVENT;
#endif

#ifdef TLV_SCAN

/* 
 * This scan handle Country Information IE(802.11d compliant) 
 * Define data structure for HostCmd_CMD_802_11_SCAN 
 */
typedef struct _HostCmd_DS_802_11_SCAN {
	u8	BSSType;
	u8	BSSID[ETH_ALEN];
//	MrvlIEtypes_SsIdParamSet_t 	SsIdParamSet;
//	MrvlIEtypes_ChanListParamSet_t	ChanListParamSet;
//	MrvlIEtypes_RatesParamSet_t 	OpRateSet;
} __ATTRIB_PACK__ HostCmd_DS_802_11_SCAN, *PHostCmd_DS_802_11_SCAN;

typedef struct _HostCmd_DS_802_11_SCAN_RSP {
	u16	BSSDescriptSize;
	u8	NumberOfSets;
} __ATTRIB_PACK__ HostCmd_DS_802_11_SCAN_RSP,
				*PHostCmd_DS_802_11_SCAN_RSP;
#else

/* Define data structure for HostCmd_CMD_802_11_SCAN */
typedef struct _HostCmd_DS_802_11_SCAN {
	u16	IsAutoAssociation;
	u8	BSSType;
	u8	BSSID[ETH_ALEN];
	u8	SSID[MRVDRV_MAX_SSID_LENGTH];
	u8	ScanType;
	u16	ProbeDelay;
	u8	CHList[MRVDRV_MAX_CHANNEL_SIZE];
	u16	MinCHTime;
	u16	MaxCHTime;
	u16	Reserved;
	u8	DataRate[HOSTCMD_SUPPORTED_RATES];
} __ATTRIB_PACK__ HostCmd_DS_802_11_SCAN, *PHostCmd_DS_802_11_SCAN;

/* Define data structure for HostCmd_CMD_802_11_SCAN */
typedef struct _HostCmd_DS_802_11_SCAN_RSP {
	u8	BSSID[ETH_ALEN];
	u8	RSSI[MRVDRV_MAX_BSS_DESCRIPTS];
	u16	BSSDescriptSize;
	u8	NumberOfSets;
} __ATTRIB_PACK__ HostCmd_DS_802_11_SCAN_RSP,
				*PHostCmd_DS_802_11_SCAN_RSP;
#endif /* TLV_SCAN */

/* Define data structure for HostCmd_CMD_802_11_QUERY_TRAFFIC */
typedef struct _HostCmd_DS_802_11_QUERY_TRAFFIC {
	u32	Traffic;		// Traffic in bps
} __ATTRIB_PACK__ HostCmd_DS_802_11_QUERY_TRAFFIC,
  *PHostCmd_DS_802_11_QUERY_TRAFFIC;

/* Define data structure for HostCmd_CMD_802_11_QUERY_STATUS */
typedef struct _HostCmd_DS_802_11_QUERY_STATUS {
	u16	FWStatus;
	u16	MACStatus;
	u16	RFStatus;
	u16	CurrentChannel;		// 1..99
	u8	APMACAdr[6];		// Associated AP MAC address
	u16	Reserved;
	u32	MaxLinkSpeed;		// Allowable max.link speed in unit
					// of 100bps
} __ATTRIB_PACK__ HostCmd_DS_802_11_QUERY_STATUS,
  *PHostCmd_DS_802_11_QUERY_STATUS;

/* Define data structure for HostCmd_CMD_802_11_GET_LOG */
typedef struct _HostCmd_DS_802_11_GET_LOG {
	u32   mcasttxframe;
	u32   failed;
	u32   retry;
	u32   multiretry;
	u32   framedup;
	u32   rtssuccess;
	u32   rtsfailure;
	u32   ackfailure;
	u32   rxfrag;
	u32   mcastrxframe;
	u32   fcserror;
	u32   txframe;
	u32   wepundecryptable;
} __ATTRIB_PACK__ HostCmd_DS_802_11_GET_LOG,
    *PHostCmd_DS_802_11_GET_LOG;

/* Define data structure for HostCmd_CMD_MAC_CONTROL */
typedef struct _HostCmd_DS_MAC_CONTROL
{
	u16 Action;                // RX, TX, INT, WEP, LOOPBACK on/off
	u16 Reserved;
} __ATTRIB_PACK__ HostCmd_DS_MAC_CONTROL, *PHostCmd_DS_MAC_CONTROL;

/* Define data structure for HostCmd_CMD_MAC_MULTICAST_ADR */
typedef struct _HostCmd_DS_MAC_MULTICAST_ADR {
	u16	Action;
	u16	NumOfAdrs;
	u8	MACList[MRVDRV_ETH_ADDR_LEN * MRVDRV_MAX_MULTICAST_LIST_SIZE];
} __ATTRIB_PACK__ HostCmd_DS_MAC_MULTICAST_ADR,
  *PHostCmd_DS_MAC_MULTICAST_ADR;

/* Define data structure for HostCmd_CMD_802_11_AUTHENTICATE */
typedef struct _HostCmd_DS_802_11_AUTHENTICATE {
	u8	MacAddr[ETH_ALEN];
	u8	AuthType;
	u8	Reserved[10];
} __ATTRIB_PACK__ HostCmd_DS_802_11_AUTHENTICATE,
  *PHostCmd_DS_802_11_AUTHENTICATE;

/* Define data structure for HostCmd_RET_802_11_AUTHENTICATE */
typedef struct _HostCmd_DS_802_11_AUTHENTICATE_RSP
{
	u8	MacAddr[6];
	u8	AuthType;
	u8	AuthStatus;
} __ATTRIB_PACK__ HostCmd_DS_802_11_AUTHENTICATE_RSP,
  *PHostCmd_DS_802_11_AUTHENTICATE_RSP;

/* Define data structure for HostCmd_CMD_802_11_DEAUTHENTICATE */
typedef struct _HostCmd_DS_802_11_DEAUTHENTICATE
{
	u8	MacAddr[6];
	u16	ReasonCode;
} __ATTRIB_PACK__ HostCmd_DS_802_11_DEAUTHENTICATE,
  *PHostCmd_DS_802_11_DEAUTHENTICATE;

#ifdef NEW_ASSOCIATION
#ifdef TLV_ASSOCIATION

typedef struct _HostCmd_DS_802_11_ASSOCIATE 
{
	u8				PeerStaAddr[6];
	IEEEtypes_CapInfo_t		CapInfo;
	u16			 	ListenInterval;
	u16				BcnPeriod;
	u8				DtimPeriod;
//	MrvlIEtypes_SsIdParamSet_t 	SsIdParamSet;
//	MrvlIEtypes_PhyParamSet_t	PhyParamSet;
//	MrvlIEtypes_SsParamSet_t	SsParamSet;
//	MrvlIEtypes_RatesParamSet_t	RatesParamSet;
//#ifdef  WPA
//	MrvlIEtypes_RsnParamSet_t	RsnParamSet;
//#endif
//#ifdef WMM
//	MrvlIEtypes_WmmParamSet_t	WmmParamSet;
//#endif /* WMM */
} __ATTRIB_PACK__ HostCmd_DS_802_11_ASSOCIATE,
    *PHostCmd_DS_802_11_ASSOCIATE;
#else
typedef struct _HostCmd_DS_802_11_ASSOCIATE 
{
	u8				PeerStaAddr[6];
	u16				FailTimeOut;
	IEEEtypes_CapInfo_t		CapInfo;
	u16			 	ListenInterval;
	u8				SsId[MRVDRV_MAX_SSID_LENGTH];
	u8				Reserved;
	u16				Reserved1;
	u8				Reserved2;
	IEEEtypes_PhyParamSet_t		PhyParamSet;
	IEEEtypes_SsParamSet_t		SsParamSet;
	u8				DataRates[HOSTCMD_SUPPORTED_RATES];
	u8				RsnIE[256];
} __ATTRIB_PACK__ HostCmd_DS_802_11_ASSOCIATE,
    *PHostCmd_DS_802_11_ASSOCIATE;
#endif /* TLV_ASSOCIATION */
#else

/* 
 * Define data structure for HostCmd_CMD_802_11_ASSOCIATE and 
 * HostCmd_CMD_802_11_REASSOCIATE
 */
typedef struct _HostCmd_DS_802_11_ASSOCIATE {
	u8			DestMacAddr[ETH_ALEN];
	u16			TimeOut;        // Association failure timeout
	IEEEtypes_CapInfo_t	CapInfo;        // Capability information
	u16			ListenInterval; // Listen interval
	u32			BlankSsid;
} __ATTRIB_PACK__ HostCmd_DS_802_11_ASSOCIATE,
  *PHostCmd_DS_802_11_ASSOCIATE;

#endif /* NEW_ASSOCIATE */

/* Define data structure for HostCmd_CMD_802_11_DISASSOCIATE */
typedef struct _HostCmd_DS_802_11_DISASSOCIATE
{
	u8 DestMacAddr[6];
	u16 ReasonCode;            // Disassociation reason code
} __ATTRIB_PACK__ HostCmd_DS_802_11_DISASSOCIATE,
  *PHostCmd_DS_802_11_DISASSOCIATE;

/* Define data structure for HostCmd_RET_802_11_ASSOCIATE */
typedef struct _HostCmd_DS_802_11_ASSOCIATE_RSP
{
#ifdef FWVERSION3
	IEEEtypes_CapInfo_t	CapInfo;
	u16			ResultCode;
	u16			AssociationID;
	u16			IELength;
#ifdef NEW_ASSOCIATION_RSP
	u8			RateID;
	u8			RateLen;
	u8			Rates[8];
	u8			ExtRateID;
	u8			ExtRateLen;
	u8			ExtRates[8];
#else
	u8			IE[8];        /* The old member was IE[1] */
	u8			ExtRates[12]; /* This was not there before */
#endif
#endif

} __ATTRIB_PACK__ HostCmd_DS_802_11_ASSOCIATE_RSP,
  *PHostCmd_DS_802_11_ASSOCIATE_RSP;

/* Define data structure for HostCmd_RET_802_11_AD_HOC_JOIN */
typedef struct _HostCmd_DS_802_11_AD_HOC_RESULT
{
#ifndef	MS8380
  	u8 PAD[3]; //TODO: Remove this when fixed in F/W 3 
 		      //byte right shift in BSSID
#endif
	u8 BSSID[MRVDRV_ETH_ADDR_LEN];
} __ATTRIB_PACK__ HostCmd_DS_802_11_AD_HOC_RESULT,
  *PHostCmd_DS_802_11_AD_HOC_RESULT;

/* Define data structure for HostCmd_CMD_802_11_SET_WEP */
typedef struct _HostCmd_DS_802_11_SET_WEP
{
	u16 Action;                  // ACT_ADD, ACT_REMOVE or ACT_ENABLE 
	u16 KeyIndex;		// Key Index selected for Tx
	u8 WEPTypeForKey1;           // 40, 128bit or TXWEP 
	u8 WEPTypeForKey2;
	u8 WEPTypeForKey3;
	u8 WEPTypeForKey4;
	u8 WEP1[16];                 // WEP Key itself
	u8 WEP2[16];
	u8 WEP3[16];
	u8 WEP4[16];
} __ATTRIB_PACK__ HostCmd_DS_802_11_SET_WEP,
  *PHostCmd_DS_802_11_SET_WEP;

/* Define data structure for HostCmd_CMD_802_3_GET_STAT */
typedef struct _HostCmd_DS_802_3_GET_STAT
{
	u32 XmitOK;
	u32 RcvOK;
	u32 XmitError;
	u32 RcvError;
	u32 RcvNoBuffer;
	u32 RcvCRCError;
} __ATTRIB_PACK__ HostCmd_DS_802_3_GET_STAT,
  *PHostCmd_DS_802_3_GET_STAT;

/* Define data structure for HostCmd_CMD_802_11_GET_STAT */
typedef struct _HostCmd_DS_802_11_GET_STAT
{
	u32 TXFragmentCnt;
	u32 MCastTXFrameCnt;
	u32 FailedCnt;
	u32 RetryCnt;
	u32 MultipleRetryCnt;
	u32 RTSSuccessCnt;
	u32 RTSFailureCnt;
	u32 ACKFailureCnt;
	u32 FrameDuplicateCnt;
	u32 RXFragmentCnt;
	u32 MCastRXFrameCnt;
	u32 FCSErrorCnt;
	u32 BCastTXFrameCnt;
	u32 BCastRXFrameCnt;
	u32 TXBeacon;
	u32 RXBeacon;
	u32 WEPUndecryptable;
} __ATTRIB_PACK__ HostCmd_DS_802_11_GET_STAT,
  *PHostCmd_DS_802_11_GET_STAT;

typedef struct _HostCmd_DS_802_11_AD_HOC_STOP {

} __ATTRIB_PACK__ HostCmd_DS_802_11_AD_HOC_STOP, 
	*PHostCmd_DS_802_11_AD_HOC_STOP;

typedef struct _HostCmd_DS_802_11_BEACON_STOP {

} __ATTRIB_PACK__ HostCmd_DS_802_11_BEACON_STOP, 
	*PHostCmd_DS_802_11_BEACON_STOP;

#ifdef DTIM_PERIOD
/* Define data structure for HostCmd_CMD_802_11_SET_DTIM_MULTIPLE */
typedef struct _HostCmd_DS_802_11_SET_DTIM_MULTIPLE {
	u16	DTIM_time_multiple;     
} __ATTRIB_PACK__ HostCmd_DS_802_11_SET_DTIM_MULTIPLE,
			 *PHostCmd_DS_802_11_SET_DTIM_MULTIPLE;
#endif /* DTIM_PERIOD */

/* Define data structure for HostCmd_CMD_802_11_SNMP_MIB */
typedef struct _HostCmd_DS_802_11_SNMP_MIB
{
	u16 QueryType;	
	u16 OID;	
	u16 BufSize;	
	u8 Value[128];
} __ATTRIB_PACK__ HostCmd_DS_802_11_SNMP_MIB,
  *PHostCmd_DS_802_11_SNMP_MIB;

/* Define data structure for HostCmd_CMD_MAC_REG_MAP */
typedef struct _HostCmd_DS_MAC_REG_MAP
{
	u16 BufferSize;            // 128 u8s
	u8 RegMap[128];
	u16 Reserved;
} __ATTRIB_PACK__ HostCmd_DS_MAC_REG_MAP, *PHostCmd_DS_MAC_REG_MAP;

/* Define data structure for HostCmd_CMD_BBP_REG_MAP */
typedef struct _HostCmd_DS_BBP_REG_MAP
{
	u16 BufferSize;            // 128 u8s
	u8 RegMap[128];
	u16 Reserved;
} __ATTRIB_PACK__ HostCmd_DS_BBP_REG_MAP, *PHostCmd_DS_BBP_REG_MAP;

/* Define data structure for HostCmd_CMD_RF_REG_MAP */
typedef struct _HostCmd_DS_RF_REG_MAP
{
	u16 BufferSize;            // 64 u8s
	u8 RegMap[64];
	u16 Reserved;
} __ATTRIB_PACK__ HostCmd_DS_RF_REG_MAP, *PHostCmd_DS_RF_REG_MAP;

/* Define data structure for HostCmd_CMD_MAC_REG_ACCESS */
typedef struct _HostCmd_DS_MAC_REG_ACCESS
{
	u16 Action;
	u16 Offset;
	u32 Value;
} __ATTRIB_PACK__ HostCmd_DS_MAC_REG_ACCESS,
  *PHostCmd_DS_MAC_REG_ACCESS;

/* Define data structure for HostCmd_CMD_BBP_REG_ACCESS */
typedef struct _HostCmd_DS_BBP_REG_ACCESS
{
	u16 Action;
	u16 Offset;
	u8 Value;
	u8 Reserved[3];
} __ATTRIB_PACK__ HostCmd_DS_BBP_REG_ACCESS,
  *PHostCmd_DS_BBP_REG_ACCESS;

/* Define data structure for HostCmd_CMD_RF_REG_ACCESS */
typedef struct _HostCmd_DS_RF_REG_ACCESS
{
	u16 Action;
	u16 Offset;
	u8 Value;
	u8 Reserved[3];
} __ATTRIB_PACK__ HostCmd_DS_RF_REG_ACCESS,
  *PHostCmd_DS_RF_REG_ACCESS;

/* Define data structure for HostCmd_CMD_802_11_RADIO_CONTROL */
typedef struct _HostCmd_DS_802_11_RADIO_CONTROL
{
	u16 Action;
	u16 Control;
//	u8 Control;	// @bit0: 1/0,on/off, @bit1: 1/0, 
  			// long/short @bit2: 1/0,auto/fix
//	u8 RadioOn;
} __ATTRIB_PACK__ HostCmd_DS_802_11_RADIO_CONTROL,
  *PHostCmd_DS_802_11_RADIO_CONTROL;

#ifdef BCA
/* Define data structure for HostCmd_CMD_BCA_CONFIG & HostCmd_RET_BCA_CONFIG */
typedef struct _HostCmd_DS_BCA_CONFIG
{
	u16 Action;
	u16 Mode;
	u16 Antenna;
	u16 BtFreq;
	u32 TxPriorityLow32;
	u32 TxPriorityHigh32;
	u32 RxPriorityLow32;
	u32 RxPriorityHigh32;
} __ATTRIB_PACK__ HostCmd_DS_BCA_CONFIG,
  *PHostCmd_DS_BCA_CONFIG;
#endif

/* SLEEP PARAMS REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_SLEEP_PARAMS
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	Error;		/* Sleep clock error in ppm */
	u16 	Offset; 	/* Wakeup offset in usec */
	u16 	StableTime; 	/* Clock stabilization time in usec */
	u8 	CalControl; 	/* Control periodic calibration */
	u8 	ExternalSleepClk; /* Control the use of external sleep clock */	
	u16 	Reserved; 	/* Reserved field, should be set to zero */
} __ATTRIB_PACK__ HostCmd_DS_802_11_SLEEP_PARAMS,
  *PHostCmd_DS_802_11_SLEEP_PARAMS;

/* SLEEP PERIOD REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_SLEEP_PERIOD
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	Period; 	/* Sleep Period in msec */
} __ATTRIB_PACK__ HostCmd_DS_802_11_SLEEP_PERIOD,
  *PHostCmd_DS_802_11_SLEEP_PERIOD;

/* BCA CONFIG TIMESHARE REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_BCA_TIMESHARE
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	TrafficType; 	/* Type: WLAN, BT */
	u32 	TimeShareInterval; /* 20msec - 60000msec */
	u32 	BTTime; 	/* PTA arbiter time in msec */	
} __ATTRIB_PACK__ HostCmd_DS_802_11_BCA_TIMESHARE,
  *PHostCmd_DS_802_11_BCA_TIMESHARE;

/* INACTIVITY TIMEOUT REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_INACTIVITY_TIMEOUT
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	Timeout; 	/* Inactivity timeout in msec */
} __ATTRIB_PACK__ HostCmd_DS_802_11_INACTIVITY_TIMEOUT,
  *PHostCmd_DS_802_11_INACTIVITY_TIMEOUT;

/* Define data structure for HostCmd_CMD_802_11_RF_CHANNEL */
typedef struct _HostCmd_DS_802_11_RF_CHANNEL
{
	u16	Action;
#ifdef FWVERSION3
	u16	CurrentChannel;
	u16	RFType; 	// Not Used
	u16	Reserved;	// Not used
	u8		ChannelList[32];// Not used
#endif
} __ATTRIB_PACK__ HostCmd_DS_802_11_RF_CHANNEL,
  *PHostCmd_DS_802_11_RF_CHANNEL;

/* Define data structure for HostCmd_CMD_802_11_RSSI */
typedef struct _HostCmd_DS_802_11_RSSI {
	u16	N;  // N is the weighting factor
	u16	Reserved_0;
	u16	Reserved_1;
	u16	Reserved_2;
} __ATTRIB_PACK__ HostCmd_DS_802_11_RSSI, *PHostCmd_DS_802_11_RSSI;

typedef struct _HostCmd_DS_802_11_RSSI_RSP {
	u16	SNR;
	u16	NoiseFloor;
	u16	AvgSNR;
	u16	AvgNoiseFloor;
} __ATTRIB_PACK__ HostCmd_DS_802_11_RSSI_RSP, *PHostCmd_DS_802_11_RSSI_RSP;

/* mac address */
typedef struct _HostCmd_DS_802_11_MAC_ADDRESS {
	u16	Action;
	u8	MacAdd[ETH_ALEN];
} __ATTRIB_PACK__ HostCmd_DS_802_11_MAC_ADDRESS,
	*PHostCmd_DS_802_11_MAC_ADDRESS;

/* PRE TBTT */
typedef struct _HostCmd_DS_802_11_PRE_TBTT
{
	u16 Action;
	u16 Value;
} __ATTRIB_PACK__ HostCmd_DS_802_11_PRE_TBTT,
  *PHostCmd_DS_802_11_PRE_TBTT;

/* RFI Tx and Tx_Control modes */
typedef struct _HostCmd_DS_802_11_RFI {
	u16      Mode;
} __ATTRIB_PACK__ HostCmd_802_11_RFI, *PHostCmd_DS_802_11_RFI;


/* Define data structure for HostCmd_CMD_802_11_RF_TX_POWER */
typedef struct _HostCmd_DS_802_11_RF_TX_POWER {
	u16 Action;
	u16 CurrentLevel;
} __ATTRIB_PACK__ HostCmd_DS_802_11_RF_TX_POWER,
    *PHostCmd_DS_802_11_RF_TX_POWER;

/* Define data structure for HostCmd_CMD_802_11_RF_ANTENNA */
typedef struct _HostCmd_DS_802_11_RF_ANTENNA {
	u16	Action;
	u16	AntennaMode;           // Number of antennas or 0xffff(diversity)
} __ATTRIB_PACK__ HostCmd_DS_802_11_RF_ANTENNA,
	*PHostCmd_DS_802_11_RF_ANTENNA;

#ifdef PS_REQUIRED
/* Define data structure for HostCmd_CMD_802_11_PS_MODE */
typedef struct _HostCmd_DS_802_11_PS_MODE
{
	u16 Action;
	u16 NullPktInterval;
	u16 MultipleDtim;
	u16 Reserved;
	u16 LocalListenInterval;
} __ATTRIB_PACK__ HostCmd_DS_802_11_PS_MODE,
  *PHostCmd_DS_802_11_PS_MODE;

typedef struct _PS_CMD_ConfirmSleep {
	u16	Command;
	u16	Size;
	u16	SeqNum;
	u16	Result;

	u16	Action;
	u16	PowerMode;
	u16	MultipleDtim;
	u16	Reserved;
	u16 	LocalListenInterval;
} __ATTRIB_PACK__ PS_CMD_ConfirmSleep, *PPS_CMD_ConfirmSleep;
#endif

#ifdef FW_WAKEUP_METHOD
/* Define data structure for HostCmd_CMD_802_11_FW_WAKEUP_METHOD */
typedef struct _HostCmd_DS_802_11_FW_WAKEUP_METHOD
{
	u16 Action;
	u16 Method;
} __ATTRIB_PACK__ HostCmd_DS_802_11_FW_WAKEUP_METHOD,
  *PHostCmd_DS_802_11_FW_WAKEUP_METHOD;
#endif

/* Define data structure for HostCmd_CMD_802_11_DATA_RATE */
typedef struct _HostCmd_DS_802_11_DATA_RATE {
	u16	Action;
	u16	Reserverd;
	u8	DataRate[HOSTCMD_SUPPORTED_RATES];
} __ATTRIB_PACK__ HostCmd_DS_802_11_DATA_RATE,
	*PHostCmd_DS_802_11_DATA_RATE;

typedef struct _HostCmd_DS_802_11_RATE_ADAPT_RATESET {
	u16	Action;
	u16	EnableHwAuto;
	u16	Bitmap;
} __ATTRIB_PACK__ HostCmd_DS_802_11_RATE_ADAPT_RATESET,
	*PHostCmd_DS_802_11_RATE_ADAPT_RATESET;

/* Define data structure for start Command in Ad Hoc mode */
typedef struct _HostCmd_DS_802_11_AD_HOC_START
{
	u8 SSID[MRVDRV_MAX_SSID_LENGTH];
	u8 BSSType;
	u16 BeaconPeriod;
	u8 DTIMPeriod;
	IEEEtypes_SsParamSet_t SsParamSet;
	IEEEtypes_PhyParamSet_t PhyParamSet;
	u16 ProbeDelay;
	IEEEtypes_CapInfo_t Cap;
	u8 DataRate[HOSTCMD_SUPPORTED_RATES];
    /*
    ** New fields after this point should be MRVL TLV types
    */

} __ATTRIB_PACK__ HostCmd_DS_802_11_AD_HOC_START,
  *PHostCmd_DS_802_11_AD_HOC_START;


typedef struct AdHoc_BssDesc_t {
	u8	BSSID[6];
	u8	SSID[32];
	u8	BSSType;
	u16	BeaconPeriod;
	u8	DTIMPeriod;
	u8	TimeStamp[8];
	u8	LocalTime[8];
	IEEEtypes_PhyParamSet_t	PhyParamSet;
	IEEEtypes_SsParamSet_t	SsParamSet;
	IEEEtypes_CapInfo_t	Cap;
	u8  DataRates[HOSTCMD_SUPPORTED_RATES];
    /*
    ** DO NOT ADD ANY FIELDS TO THIS STRUCTURE.  It is used below in the
    **   Adhoc join command and will cause a binary layout mismatch with 
    **   the firmware
    */
} __ATTRIB_PACK__ AdHoc_BssDesc_t, pAdHoc_BssDesc_t;


/* Define data structure for Join Command in Ad Hoc mode */
typedef struct _HostCmd_DS_802_11_AD_HOC_JOIN
{
	AdHoc_BssDesc_t BssDescriptor;
	u16 FailTimeOut;
	u16 ProbeDelay;
    /*
    ** New fields after this point should be MRVL TLV types
    */
    
} __ATTRIB_PACK__ HostCmd_DS_802_11_AD_HOC_JOIN,
  *PHostCmd_DS_802_11_AD_HOC_JOIN;

#ifdef WPA
typedef struct _HostCmd_DS_802_11_QUERY_RSN_OPTION {
	u8                   RSN_Capable;
	u8                   Reserved[3];
}__ATTRIB_PACK__ HostCmd_DS_802_11_QUERY_RSN_OPTION, 
*PHostCmd_DS_802_11_QUERY_RSN_OPTION;

typedef struct _HostCmd_DS_802_11_ENABLE_RSN {
	u16                  Action;	
	u16                  Enable;
}__ATTRIB_PACK__ HostCmd_DS_802_11_ENABLE_RSN,
*PHostCmd_DS_802_11_ENABLE_RSN;

typedef struct _HostCmd_DS_802_11_QUERY_TKIP_REPLY_CNTRS {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
	u32                   NumTkipCntrs;
}__ATTRIB_PACK__ HostCmd_DS_802_11_QUERY_TKIP_REPLY_CNTRS,
*PHostCmd_DS_802_11_QUERY_TKIP_REPLY_CNTRS;

#ifndef WPA2
typedef struct _HostCmd_DS_802_11_CONFIG_RSN {
	u16	             Action;
	u8                   Version;
	u8                   PairWiseKeysSupported;
	u8                   MulticastCipher[4];
	u8                   GroupKeyMethod;
	u32                  GroupReKeyTime;
	u32                  GroupReKeyPkts;
	u8                   GroupReKeyStrict;
	u8                   TsnEnabled;
	u32                  GroupMasterReKeyTime;
	u32                  GroupUpdateTmo;
	u32		     GroupUpdateCnt;
	u32 		     PairWiseUpdateTmo;
       	u32                  PairWiseUpdateCnt;
}__ATTRIB_PACK__ HostCmd_DS_802_11_CONFIG_RSN , 
*PHostCmd_DS_802_11_CONFIG_RSN;

typedef struct _HostCmd_DS_802_11_UNICAST_CIPHER {
	u16                  Action;
	u8                   UnicastCipher[4];
	u16                  Enabled;
}__ATTRIB_PACK__ HostCmd_DS_802_11_UNICAST_CIPHER,
*PHostCmd_DS_802_11_UNICAST_CIPHER;

typedef struct _HostCmd_DS_802_11_MCAST_CIPHER {
	u16                  Action;
	u8                   McastCipher[4];
}__ATTRIB_PACK__ HostCmd_DS_802_11_MCAST_CIPHER,
*PHostCmd_DS_802_11_MCAST_CIPHER;

typedef struct _HostCmd_DS_802_11_RSN_AUTH_SUITES {
	u16                  Action;
	u8                   AuthSuites[4];
	u16                  Enabled;
}__ATTRIB_PACK__ HostCmd_DS_802_11_RSN_AUTH_SUITES,
*PHostCmd_DS_802_11_RSN_AUTH_SUITES;
#endif

typedef struct _HostCmd_DS_802_11_PWK_KEY {
	u16                  Action;
	u8                   TkipEncryptKey[16];
	u8                   TkipTxMicKey[8];
	u8                   TkipRxMicKey[8];
}__ATTRIB_PACK__ HostCmd_DS_802_11_PWK_KEY, 
*PHostCmd_DS_802_11_PWK_KEY;

#ifndef WPA2
typedef struct _HostCmd_DS_802_11_GRP_KEY {
	u16                  Action;
	u8                   TkipEncryptKey[16];
	u8                   TkipTxMicKey[8];
	u8                   TkipRxMicKey[8];
}__ATTRIB_PACK__ HostCmd_DS_802_11_GRP_KEY, *PHostCmd_DS_802_11_GRP_KEY;
#endif

typedef struct _HostCmd_DS_802_11_PAIRWISE_TSC {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
        u16                  Action;
        u32                   Txlv32;
        u16                  Txlv16;
}__ATTRIB_PACK__ HostCmd_DS_802_11_PAIRWISE_TSC, 
	*PHostCmd_DS_802_11_PAIRWISE_TSC;

typedef struct _HostCmd_DS_802_11_GROUP_TSC {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
	u16                  Action;
	u32                   Txlv32;
	u16                  Txlv16;
}__ATTRIB_PACK__ HostCmd_DS_802_11_GROUP_TSC, 
		*PHostCmd_DS_802_11_GROUP_TSC;

#ifdef WPA2

typedef union _KeyInfo_WEP_t {
	u8	Reserved;		/* bits 5-15: Reserved */
	u8	WepKeyIndex;		/* bits 1-4: Specifies the index of key */
	u8	isWepDefaultKey;	/* bit 0: Specifies that this key is to be used as the default for TX data packets */
} __ATTRIB_PACK__ KeyInfo_WEP_t;

typedef union _KeyInfo_TKIP_t {
	u8	Reserved;		/* bits 3-15: Reserved */
	u8	isKeyEnabled;		/* bit 2: Specifies that this key is enabled and valid to use */
	u8	isUnicastKey;		/* bit 1: Specifies that this key is to be used as the unicast key */
	u8	isMulticastKey;		/* bit 0: Specifies that this key is to be used as the multicast key */
} __ATTRIB_PACK__ KeyInfo_TKIP_t;

typedef union _KeyInfo_AES_t {
	u8	Reserved;		/* bits 3-15: Reserved */
	u8	isKeyEnabled;		/* bit 2: Specifies that this key is enabled and valid to use */
	u8	isUnicastKey;		/* bit 1: Specifies that this key is to be used as the unicast key */
	u8	isMulticastKey;		/* bit 0: Specifies that this key is to be used as the multicast key */
} __ATTRIB_PACK__ KeyInfo_AES_t;

typedef struct _KeyMaterial_TKIP_t {
	u8	TkipKey[16];			/* TKIP encryption/decryption key */
	u8	TkipTxMicKey[16];		/* TKIP TX MIC Key */
	u8	TkipRxMicKey[16];		/* TKIP RX MIC Key */
} __ATTRIB_PACK__ KeyMaterial_TKIP_t,
		*PKeyMaterial_TKIP_t;

typedef struct _KeyMaterial_AES_t {
	u8	AesKey[16];			/* AES encryption/decryption key */
} __ATTRIB_PACK__ KeyMaterial_AES_t,
		*PKeyMaterial_AES_t;

typedef struct _MrvlIEtype_KeyParamSet_t {
	u16	Type;		/* Type ID */
	u16	Length;		/* Length of Payload */
	u16	KeyTypeId;	/* Type of Key: WEP=0, TKIP=1, AES=2 */
	u16	KeyInfo;	/* Key Control Info specific to a KeyTypeId */
	u16	KeyLen;		/* Length of key */
	u8	Key[32];	/* Key material of size KeyLen */
} __ATTRIB_PACK__ MrvlIEtype_KeyParamSet_t,
		*PMrvlIEtype_KeyParamSet_t;

typedef struct _HostCmd_DS_802_11_KEY_MATERIAL {
	u16				Action;		/* ACT_SET
					   ACT_GET: return all the available keys
							 */
	MrvlIEtype_KeyParamSet_t	KeyParamSet;	/* Request:
					   Specified only with ACT_SET,
					   as many key parameter set as needed.
					   Response:
					   Return valid only for ACT_GET,
					   return as many as needed.
							 */
} __ATTRIB_PACK__ HostCmd_DS_802_11_KEY_MATERIAL,
		*PHostCmd_DS_802_11_KEY_MATERIAL;
#endif	/* WPA2 */

#endif	/* WPA */

#ifdef HOST_WAKEUP
typedef struct _HostCmd_DS_HOST_802_11_WAKE_UP_CFG {
	u32	conditions;		/* bit0=1: non-unicast data
					   bit1=1: unicast data
					   bit2=1: mac events
					   bit3=1: magic packet */
	u8	gpio;			/* 0xff if not uesed */
	u8	gap;			/* in milliseconds */
} __ATTRIB_PACK__ HostCmd_DS_802_11_HOST_WAKE_UP_CFG;
#endif		

#ifdef CAL_DATA
typedef struct _HostCmd_DS_802_11_CAL_DATA {
	u16	Action;
	u8	Reserved1[9];
	u8	PAOption;		/* PA calibration options */
	u8	ExtPA;			/* type of external PA */
	u8	Ant;			/* Antenna selection */
	u16	IntPA[14];		/* channel calibration */
	u8	PAConfig[4];		/* RF register calibration */
	u8	Reserved2[4];
	u16	Domain;			/* Regulatory Domain */
	u8	ECO;			/* ECO present or not */
	u8	LCT_cal;		/* VGA capacitor calibration */
	u8	Reserved3[12];		
} __ATTRIB_PACK__ HostCmd_DS_802_11_CAL_DATA,
	*pHostCmd_DS_802_11_CAL_DATA;

typedef struct _HostCmd_DS_802_11_CAL_DATA_EXT {
	u16	Action;
	u16	Revision;
	u16	CalDataLen;
	u8	CalData[1024]; 
} __ATTRIB_PACK__ HostCmd_DS_802_11_CAL_DATA_EXT,
	*pHostCmd_DS_802_11_CAL_DATA_EXT;
#endif /* CAL_DATA */

#ifdef MULTI_BANDS
typedef struct _HostCmd_DS_802_11_BAND_CONFIG {
	u16		Action;		/* 0: GET;
					   1: SET */
	u16		BandSelection; 	/* Select 802.11b = 0 (2.4GHz) */
					/* Select 802.11g = 1 (2.4GHz) */
					/* Select 802.11a = 2 (5 GHz ) */
	u16		Channel;
} __ATTRIB_PACK__ HostCmd_DS_802_11_BAND_CONFIG,
	*pHostCmd_DS_802_11_BAND_CONFIG;
#endif

typedef	struct _HostCmd_DS_802_11_EEPROM_ACCESS {
	u16	Action; /* ACT_GET / ACT_SET */
	u16	Offset; /* Multiple of 4. Example: 0, 4, 8 ... */
	u16	ByteCount; /* Multiple of 4. Example: 4, 8, 12 ... */
	u8	Value; /* The caller must provide a buffer of atleast 
			  byte count size starting from here */
} __ATTRIB_PACK__ HostCmd_DS_802_11_EEPROM_ACCESS,
	*pHostCmd_DS_802_11_EEPROM_ACCESS;

#ifdef GSPI83xx
typedef struct _HostCmd_DS_CMD_GSPI_BUS_CONFIG {
	u16	Action;  /* ACT_GET / ACT_SET */
	u16	BusDelayMode; /* Data format Bit[1:0], Delay method Bit[2] */
	u16	HostTimeDelayToReadPort; /* Number of dummy clocks 
					    to wait for read r/w port */
	u16	HostTimeDelayToReadregister; /* Number of dummy 
						clocks to wait for read reg */
} __ATTRIB_PACK__ HostCmd_DS_CMD_GSPI_BUS_CONFIG,
	*pHostCmd_DS_CMD_GSPI_BUS_CONFIG;
#endif /* GSPI83xx */

#ifdef BG_SCAN

typedef struct _HostCmd_DS_802_11_BG_SCAN_CONFIG  {
	u16	Action;
	u8	Enable;			/* 0 - Disable
					   1 - Enable */
	u8	BssType;		/* 1 - Infrastructure
					   2 - IBSS
					   3 - any */
	u8	ChannelsPerScan;	/* No of channels to scan at
					   one scan */
	u8	DiscardWhenFull;	/* 0 - Discard old scan results
					   1 - Discard new scan 
					   	results */
	u16 	Reserved;
	u32	ScanInterval;		/* Interval b/w consecutive 
					   scan */
	u32	StoreCondition;		/* - SSID Match
				   	   - Exceed RSSI threshold
					   - SSID Match & Exceed RSSI
					     Threshold 
					   - Always */	   
	u32	ReportConditions;	/* - SSID Match
					   - Exceed RSSI threshold
					   - SSID Match & Exceed RSSI
					     Threshold
					   - Exceed MaxScanResults
					   - Entire channel list 
					     scanned once 
					   - Domain Mismatch in 
					     country IE */
	u16	MaxScanResults; 	/* Max scan results that will
					   trigger a scn completion
					   event */
//	attach TLV based parameters as needed, e.g.
//	MrvlIEtypes_SsIdParamSet_t 	SsIdParamSet;
//	MrvlIEtypes_ChanListParamSet_t	ChanListParamSet;
					/* List Channel of in TLV scan
					   format */
//	MrvlIEtypes_NumProbes_t		NumProbes;
} __ATTRIB_PACK__ HostCmd_DS_802_11_BG_SCAN_CONFIG,
	*pHostCmd_DS_802_11_BG_SCAN_CONFIG;

typedef struct _HostCmd_DS_802_11_BG_SCAN_QUERY {
	u8	Flush;
} __ATTRIB_PACK__ HostCmd_DS_802_11_BG_SCAN_QUERY,
	*pHostCmd_DS_802_11_BG_SCAN_QUERY;

typedef struct _HostCmd_DS_802_11_BG_SCAN_QUERY_RSP {
	u32				ReportCondition;
	HostCmd_DS_802_11_SCAN_RSP	scanresp;
} __ATTRIB_PACK__ HostCmd_DS_802_11_BG_SCAN_QUERY_RSP,
				*PHostCmd_DS_802_11_BG_SCAN_QUERY_RSP;
#endif /* BG_SCAN */

#ifdef ATIMGEN			
typedef struct _HostCmd_DS_802_11_GENERATE_ATIM {
	u16	Action;
	u16	Enable;
} __ATTRIB_PACK__ HostCmd_DS_802_11_GENERATE_ATIM,
	*PHostCmd_DS_802_11_GENERATE_ATIM;
#endif

typedef struct _HostCmd_DS_802_11_TPC_CFG {
	u16   Action;
	u8    Enable;
	char  P0;
	char  P1;
	char  P2;
	u8    UseSNR;
} __ATTRIB_PACK__ HostCmd_DS_802_11_TPC_CFG;

#ifdef LED_GPIO_CTRL	
typedef struct _HostCmd_DS_802_11_LED_CTRL {
  u16 Action;
  u16 NumLed;
  u8 data[256];
} __ATTRIB_PACK__ HostCmd_DS_802_11_LED_CTRL;
#endif

typedef struct _HostCmd_DS_802_11_PWR_CFG {
	u16   Action;
	u8    Enable;
	char  PA_P0;
	char  PA_P1;
	char  PA_P2;
} __ATTRIB_PACK__ HostCmd_DS_802_11_PWR_CFG;


#ifdef AUTO_FREQ_CTRL
typedef struct _HostCmd_DS_802_11_AFC {
	u16 afc_auto;
	union {
		struct {
			u16 threshold;
			u16 period;
		} auto_mode;

		struct {
			s16 timing_offset;
			s16 carrier_offset;
		} manual_mode;
	} b;
} __ATTRIB_PACK__ HostCmd_DS_802_11_AFC;

#define afc_data    b.data
#define afc_thre    b.auto_mode.threshold
#define afc_period  b.auto_mode.period
#define afc_toff    b.manual_mode.timing_offset
#define afc_foff    b.manual_mode.carrier_offset
#endif /* AUTO_FREQ_CTRL */

#ifdef CIPHER_TEST

  typedef struct _HostCmd_DS_802_11_KEY_ENCRYPT {
    u16 Action;
    u16 EncType;
    u8  KeyIV[16];
    u8  KeyEncKey[16];
    u16 KeyDataLen;
    u8  KeyData[512];
  } __ATTRIB_PACK__ HostCmd_DS_802_11_KEY_ENCRYPT;

#define CIPHER_TEST_RC4 (1)
#define CIPHER_TEST_AES (2)

#endif

#ifdef AUTO_TX
/** HostCmd_DS_802_11_AUTO_TX */
typedef struct _HostCmd_DS_802_11_AUTO_TX {
	/** Action */
	u16			Action;	/* 0 = ACT_GET; 1 = ACT_SET; */
	MrvlIEtypes_AutoTx_t 	AutoTx;
} __ATTRIB_PACK__ HostCmd_DS_802_11_AUTO_TX,
	*PHostCmd_DS_802_11_AUTO_TX;
#endif

struct _HostCmd_DS_COMMAND {
	u16	Command;
	u16	Size;
	u16	SeqNum;
	u16	Result;

	union {
	HostCmd_DS_GET_HW_SPEC			hwspec;
#ifdef PS_REQUIRED
	HostCmd_DS_802_11_PS_MODE		psmode;
#endif
	HostCmd_DS_802_11_SCAN			scan;
	HostCmd_DS_802_11_SCAN_RSP		scanresp;
	HostCmd_DS_MAC_CONTROL			macctrl;
	HostCmd_DS_802_11_ASSOCIATE		associate;
	HostCmd_DS_802_11_ASSOCIATE_RSP		associatersp;
	HostCmd_DS_802_11_DEAUTHENTICATE  	deauth;	
	HostCmd_DS_802_11_SET_WEP		wep;
	HostCmd_DS_802_11_AD_HOC_START		ads;
	HostCmd_DS_802_11_RESET			reset;
	HostCmd_DS_802_11_QUERY_TRAFFIC		traffic;
	HostCmd_DS_802_11_QUERY_STATUS		qstatus;
	HostCmd_DS_802_11_AD_HOC_RESULT		result;
	HostCmd_DS_802_11_GET_LOG		glog;
	HostCmd_DS_802_11_AUTHENTICATE		auth;
	HostCmd_DS_802_11_AUTHENTICATE_RSP	rauth;
	HostCmd_DS_802_11_GET_STAT		gstat;
	HostCmd_DS_802_3_GET_STAT		gstat_8023;
	HostCmd_DS_802_11_SNMP_MIB		smib;
	HostCmd_DS_802_11_RF_TX_POWER		txp;
	HostCmd_DS_802_11_RF_ANTENNA		rant;
	HostCmd_DS_802_11_DATA_RATE		drate;
	HostCmd_DS_802_11_RATE_ADAPT_RATESET	rateset;
	HostCmd_DS_MAC_MULTICAST_ADR		madr;
	HostCmd_DS_802_11_AD_HOC_JOIN		adj;
	HostCmd_DS_802_11_RADIO_CONTROL		radio;
	HostCmd_DS_802_11_RF_CHANNEL		rfchannel;
	HostCmd_DS_802_11_RSSI			rssi;
	HostCmd_DS_802_11_RSSI_RSP		rssirsp;
	HostCmd_DS_802_11_DISASSOCIATE		dassociate;
	HostCmd_DS_802_11_AD_HOC_STOP		adhoc_stop;
	HostCmd_802_11_RFI			rfi;
	HostCmd_DS_802_11_PRE_TBTT		pretbtt;
	HostCmd_DS_802_11_MAC_ADDRESS           macadd;
#ifdef BCA
	HostCmd_DS_BCA_CONFIG			bca_config;
#endif
#ifdef WPA
        HostCmd_DS_802_11_QUERY_RSN_OPTION     	qryrsn;
	HostCmd_DS_802_11_ENABLE_RSN           	enbrsn;
#ifndef WPA2
	HostCmd_DS_802_11_UNICAST_CIPHER	cipher;
	HostCmd_DS_802_11_RSN_AUTH_SUITES      	rsnauth;
#endif
#ifdef WPA2
	HostCmd_DS_802_11_KEY_MATERIAL         	keymaterial;
#endif	//WPA2
	HostCmd_DS_802_11_PWK_KEY	       	pwkkey;
#ifndef WPA2
       	HostCmd_DS_802_11_GRP_KEY     	       	grpkey;
	HostCmd_DS_802_11_CONFIG_RSN		rsn;
#endif
#endif	//WPA
#ifdef	DTIM_PERIOD
	HostCmd_DS_802_11_SET_DTIM_MULTIPLE	mdtim;
#endif
	HostCmd_DS_MAC_REG_ACCESS		macreg;
	HostCmd_DS_BBP_REG_ACCESS		bbpreg;
	HostCmd_DS_RF_REG_ACCESS		rfreg;
#ifdef PASSTHROUGH_MODE
	HostCmd_DS_802_11_PASSTHROUGH		passthrough;
#endif
	HostCmd_DS_802_11_BEACON_STOP		beacon_stop;
#ifdef CAL_DATA
	HostCmd_DS_802_11_CAL_DATA		caldata;
	HostCmd_DS_802_11_CAL_DATA_EXT		caldataext;
#endif
#ifdef HOST_WAKEUP
	HostCmd_DS_802_11_HOST_WAKE_UP_CFG	hostwakeupcfg;
#endif
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG 		band;
#endif
	HostCmd_DS_802_11_EEPROM_ACCESS		rdeeprom;
#ifdef GSPI83xx
	HostCmd_DS_CMD_GSPI_BUS_CONFIG		gspicfg;
#endif /* GSPI83xx */

#ifdef ATIMGEN			
	HostCmd_DS_802_11_GENERATE_ATIM 	genatim;
#endif
	
#ifdef ENABLE_802_11D
	HostCmd_DS_802_11D_DOMAIN_INFO		domaininfo;
	HostCmd_DS_802_11D_DOMAIN_INFO_RSP	domaininforesp;
#endif
#ifdef ENABLE_802_11H_TPC
	HostCmd_DS_802_11H_TPC_REQUEST		tpcrequest;
	HostCmd_DS_802_11H_TPC_REQUEST_RSP	tpcrequestresp;
	HostCmd_DS_802_11H_TPC_INFO		tpcinfo;
	HostCmd_DS_802_11H_TPC_INFO_RSP		tpcinforesp;
#endif
#ifdef BG_SCAN
	HostCmd_DS_802_11_BG_SCAN_CONFIG	bgscancfg;
	HostCmd_DS_802_11_BG_SCAN_QUERY		bgscanquery;
	HostCmd_DS_802_11_BG_SCAN_QUERY_RSP	bgscanqueryresp;
#endif /* BG_SCAN */
#ifdef WMM
	HostCmd_DS_802_11_WMM_TSPEC		tspec;
	HostCmd_DS_802_11_WMM_ACK_POLICY	ackpolicy;
	HostCmd_DS_802_11_WMM_GET_STATUS	getstatus;
	HostCmd_DS_802_11_WMM_PRIO_PKT_AVAIL	priopktavail;
#endif /* WMM */
	HostCmd_DS_802_11_SLEEP_PARAMS 		sleep_params;
	HostCmd_DS_802_11_BCA_TIMESHARE		bca_timeshare;
	HostCmd_DS_802_11_INACTIVITY_TIMEOUT	inactivity_timeout;
	HostCmd_DS_802_11_SLEEP_PERIOD		ps_sleeppd;
        HostCmd_DS_802_11_TPC_CFG tpccfg;
        HostCmd_DS_802_11_PWR_CFG pwrcfg;
#ifdef AUTO_FREQ_CTRL
	HostCmd_DS_802_11_AFC 			afc;
#endif
#ifdef LED_GPIO_CTRL	
	HostCmd_DS_802_11_LED_CTRL 		ledgpio;
#endif
#ifdef FW_WAKEUP_METHOD
	HostCmd_DS_802_11_FW_WAKEUP_METHOD	fwwakeupmethod;
#endif
#ifdef SUBSCRIBE_EVENT_CTRL
	HostCmd_DS_802_11_SUBSCRIBE_EVENT	events;
#endif

#ifdef CIPHER_TEST
  HostCmd_DS_802_11_KEY_ENCRYPT key_encrypt;
#endif
	} params;

} __ATTRIB_PACK__;

#define	S_DS_GEN	sizeof(HostCmd_DS_GEN)
#endif
