/******************** (c) Marvell Semiconductor, Inc., 2001 ********************
 *
 *  $HEADER$
 *
 *      File: 	hostcmd.h
 *
 *      Purpose:
 *
 *      This file contains the function prototypes, data structure and defines
 *	for all the host/station commands. Please check the 802.11
 *	GUI/Driver/Station Interface Specification for detailed command
 *	information
 *
 *      Notes:
 *
 *****************************************************************************/

#ifndef __HOSTCMD__H
#define __HOSTCMD__H

#include	"ieeetypes.h"
#include 	"host.h"

#ifdef PASSTHROUGH_MODE
#include	"wlan_passthrough.h"
#endif

/*
 *	Definition of data structure for each command
 */

/* Define general data structure */
typedef struct _HostCmd_DS_GEN {
	u16	Command;
	u16	Size;
	u16	SeqNum;
	u16	Result;
} __attribute__ ((packed)) HostCmd_DS_GEN, *PHostCmd_DS_GEN
#if defined(DEEP_SLEEP_CMD)
    ,HostCmd_DS_802_11_DEEP_SLEEP, *PHostCmd_DS_802_11_DEEP_SLEEP
#endif /* DEEP_SLEEP_CMD */
	;

/* Define data structure for HostCmd_CMD_CODE_DNLD */
typedef struct _HostCmd_DS_CODE_DNLD
{
	u8	Code[4096];
} __attribute__ ((packed)) HostCmd_DS_CODE_DNLD, *PHostCmd_DS_CODE_DNLD;


/* Define data structure for HostCmd_CMD_OP_PARAM_DNLD */
typedef struct _HostCmd_DS_OP_PARAM_DNLD
{
	u8	OpParam[4096];
} __attribute__ ((packed)) HostCmd_DS_OP_PARAM_DNLD,
  *PHostCmd_DS_OP_PARAM_DNLD;

typedef struct _BSS_DESCRIPTION_SET_FIXED_FIELDS {
	u8 BSSID[MRVDRV_ETH_ADDR_LEN];
	u8 SSID[MRVDRV_MAX_SSID_LENGTH];
	u8 BSSType;
	u16 BeaconPeriod;
	u8 DTIMPeriod;
	u8 TimeStamp[8];
	u8 LocalTime[8];
} __attribute__ ((packed)) BSS_DESCRIPTION_SET_FIXED_FIELDS,
  *PBSS_DESCRIPTION_SET_FIXED_FIELDS;

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
} __attribute__ ((packed)) HostCmd_DS_GET_HW_SPEC, *PHostCmd_DS_GET_HW_SPEC;

/* Define data structure for HostCmd_CMD_EEPROM_UPDATE */
typedef struct _HostCmd_DS_EEPROM_UPDATE {
	u16	Action;                // Detailed action or option
	u32	Value;
} __attribute__ ((packed)) HostCmd_DS_EEPROM_UPDATE, *PHostCmd_DS_EEPROM_UPDATE;

/* Define data structure for HostCmd_CMD_802_11_RESET */
typedef struct _HostCmd_DS_802_11_RESET
{
	u16	Action;		// ACT_NOT_REVERT_MIB, ACT_REVERT_MIB
				// or ACT_HALT
} __attribute__ ((packed)) HostCmd_DS_802_11_RESET, *PHostCmd_DS_802_11_RESET;

#ifdef TLV_SCAN
/* 
 * The following MrvlIETypes should be defined in some other place, 
 * such as wlan_defs.h Will move later 
 */
typedef struct _MrvlIEtypesHeader_t {
	u16	Type;
	u16	Len;
} __attribute__ ((packed)) MrvlIEtypesHeader_t;

typedef struct 	_MrvlIEtypes_RatesParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	Rates[1];
} __attribute__ ((packed)) MrvlIEtypes_RatesParamSet_t;

typedef struct 	_MrvlIEtypes_SsIdParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	SsId[1];
} __attribute__ ((packed)) MrvlIEtypes_SsIdParamSet_t;

typedef struct _ChanScanParamSet_t{
	u8	RadioType;
	u8	ChanNumber;
	u8	ScanType;
	u16	MinScanTime;
	u16	ScanTime;
} __attribute__ ((packed)) ChanScanParamSet_t;

typedef struct 	_MrvlIEtypes_ChanListParamSet_t {
	MrvlIEtypesHeader_t	Header;
	ChanScanParamSet_t 	ChanScanParam[1];
} __attribute__ ((packed)) MrvlIEtypes_ChanListParamSet_t;

/* 
 * This scan handle Country Information IE(802.11d compliant) 
 * Define data structure for HostCmd_CMD_802_11_SCAN 
 */
typedef struct _HostCmd_DS_802_11_SCAN {
	u8	BSSType;
	u8	BSSID[ETH_ALEN];
	MrvlIEtypes_SsIdParamSet_t 	SsIdParamSet;
	MrvlIEtypes_ChanListParamSet_t	ChanListParamSet;
	MrvlIEtypes_RatesParamSet_t 	OpRateSet;
} __attribute__ ((packed)) HostCmd_DS_802_11_SCAN, *PHostCmd_DS_802_11_SCAN;

typedef struct _HostCmd_DS_802_11_SCAN_RSP {
	u16	BSSDescriptSize;
	u8	NumberOfSets;
//	IEEEtypes_BssDesc_t	BSSDesc[1];
} __attribute__ ((packed)) HostCmd_DS_802_11_SCAN_RSP,
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
	u8	DataRate[14];
} __attribute__ ((packed)) HostCmd_DS_802_11_SCAN, *PHostCmd_DS_802_11_SCAN;

/* Define data structure for HostCmd_CMD_802_11_SCAN */
typedef struct _HostCmd_DS_802_11_SCAN_RSP {
	u8	BSSID[ETH_ALEN];
	u8	RSSI[MRVDRV_MAX_BSS_DESCRIPTS];
	u16	BSSDescriptSize;
	u8	NumberOfSets;
//	IEEEtypes_BssDesc_t	BSSDesc[16];
} __attribute__ ((packed)) HostCmd_DS_802_11_SCAN_RSP,
				*PHostCmd_DS_802_11_SCAN_RSP;
#endif /* TLV_SCAN */

#ifdef ENABLE_802_11D
typedef struct _MrvlIEtypes_DomainParamSet_t {
	MrvlIEtypesHeader_t	Header;
	u8	CountryCode[3];
 	IEEEtypes_SubbandSet_t		Subband[1];
} __attribute__ ((packed)) MrvlIEtypes_DomainParamSet_t;

/* Define data structure for HostCmd_CMD_802_11D_DOMAIN_INFO */
typedef struct _HostCmd_DS_802_11D_DOMAIN_INFO {
	u16	Action;
	MrvlIEtypes_DomainParamSet_t Domain;
} __attribute__ ((packed)) HostCmd_DS_802_11D_DOMAIN_INFO, *PHostCmd_DS_802_11D_DOMAIN_INFO;

/* Define data structure for HostCmd_RET_802_11D_DOMAIN_INFO */
typedef struct _HostCmd_DS_802_11D_DOMAIN_INFO_RSP {
	u16	Action;
	MrvlIEtypes_DomainParamSet_t Domain;
} __attribute__ ((packed)) HostCmd_DS_802_11D_DOMAIN_INFO_RSP, *PHostCmd_DS_802_11D_DOMIAN_INFO_RSP;
#endif

#ifdef ENABLE_802_11H_TPC
/* TLV for Local Power Constraint, CMD_802_11H_TPC_INFO */
typedef struct _MrvlIEtypes_LocalPowerConstraint_t {
	MrvlIEtypesHeader_t	Header;
	u8	Chan;
	u8	PowerConstraint;
} __attribute__ ((packed)) MrvlIEtypes_LocalPowerConstraint_t;
	
/* TLV for Local Power Capability, CMD_802_11H_TPC_INFO */
typedef struct _MrvlIEtype_PowerCapability_t {
	MrvlIEtypesHeader_t	Header;
	s8 	MinPower;
	s8	MaxPower;
} __attribute__ ((packed)) MrvlIEtypes_PowerCapability_t;

/* data structure for CMD_802_11H_TPC_INFO */
typedef struct _HostCmd_DS_802_11H_TPC_INFO {
	MrvlIEtypes_LocalPowerConstraint_t	LocalPowerConstraint;
	MrvlIEtypes_PowerCapability_t		PowerCapability;
} __attribute__ ((packed)) HostCmd_DS_802_11H_TPC_INFO;

typedef struct _HostCmd_DS_802_11H_TPC_INFO_RSP {
	MrvlIEtypes_LocalPowerConstraint_t	LocalPowerConstraint;
	MrvlIEtypes_PowerCapability_t		PowerCapability;
} __attribute__ ((packed)) HostCmd_DS_802_11H_TPC_INFO_RSP;
	
/* data structure for CMD_802_11H_TPC_REQUEST */
typedef struct _HostCmd_DS_802_11H_TPC_REQUEST {
	u8		BSSID[ETH_ALEN];
	u16		timeout;
	u8		RateIndex;
} __attribute__ ((packed)) HostCmd_DS_802_11H_TPC_REQUEST;

typedef struct _HostCmd_DS_802_11H_TPC_REQUEST_RSP {
	u8		TPCRetCode;
	s8		TxPower;
	s8		LinkMargin;
	s8		RSSI;
} __attribute__ ((packed)) HostCmd_DS_802_11H_TPC_REQUEST_RSP;
#endif /* ENABLE_802_11H_TPC */

/* Define data structure for HostCmd_CMD_802_11_QUERY_TRAFFIC */
typedef struct _HostCmd_DS_802_11_QUERY_TRAFFIC {
	u32	Traffic;		// Traffic in bps
} __attribute__ ((packed)) HostCmd_DS_802_11_QUERY_TRAFFIC,
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
} __attribute__ ((packed)) HostCmd_DS_802_11_QUERY_STATUS,
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
 
#ifdef buildModes_INCLUDE_ALL_STATS

	u32  AuthQueued;
	u32  AssocQueued;
	u32  ReassocQueued;
	u32  DeauthQueued;
	u32  DisassocQueued;
	u32  ProbeRqstsQueued;
	u32  BcnQueued;
	u32  UnknownQueued;
	u32  TxAttempts;
	u32  TxQFull;
	u32  TxBcns;
	u32  TxProbeRsps;
	u32  TxDataPkts;
	u32  RxDataPkts;
	u32  RxDataPktsWithBadSig;
	u32  DataPktsRcvdTooLong;
	u32  DataPktsRcvdDiscard;
	u32  RxMgmtMsgs;
	u32  RxMgmtMsgsWithBadSig;
	u32  RxProbeRsps;
	u32  RxUnicasts;
	u32  RxTotalBcns;
	u32  RxJoiningBcns;
	u32  RxCurrentJoinedBcns;
	u32  RxTotalJoinedBcns;
	u32  Authentications;
	u32  Associations;
	u32  BcnsLost;
	u32  Timeouts;
	u32  ScanTimeouts;
	u32  JoinTimeouts;
	u32  AuthTimeouts;
	u32  AssocTimeouts;
	u32  ReassocTimeouts;
	u32  DisassocTimeouts;
	u32  NoActivityTimeouts;
	u32  MacIsrIn;
	u32  MacIsrOut;
	u32  Dma0IsrIn;
	u32  Dma0IsrOut;
	u32  Dma1IsrIn;
	u32  Dma1IsrOut;
	u32  HostIsrIn;
	u32  HostIsrOut;
#endif
} __attribute__((packed)) HostCmd_DS_802_11_GET_LOG,
    *PHostCmd_DS_802_11_GET_LOG;

/* Define data structure for HostCmd_CMD_MAC_CONTROL */
typedef struct _HostCmd_DS_MAC_CONTROL
{
	u16 Action;                // RX, TX, INT, WEP, LOOPBACK on/off
	u16 Reserved;
} __attribute__ ((packed)) HostCmd_DS_MAC_CONTROL, *PHostCmd_DS_MAC_CONTROL;

/* Define data structure for HostCmd_CMD_MAC_MULTICAST_ADR */
typedef struct _HostCmd_DS_MAC_MULTICAST_ADR {
	u16	Action;
	u16	NumOfAdrs;
	u8	MACList[MRVDRV_ETH_ADDR_LEN * MRVDRV_MAX_MULTICAST_LIST_SIZE];
} __attribute__ ((packed)) HostCmd_DS_MAC_MULTICAST_ADR,
  *PHostCmd_DS_MAC_MULTICAST_ADR;

/* Define data structure for HostCmd_CMD_802_11_AUTHENTICATE */
typedef struct _HostCmd_DS_802_11_AUTHENTICATE {
	u8	MacAddr[ETH_ALEN];
	u8	AuthType;
	u16	TimeOut;
	u8	Reserved[8];
} __attribute__ ((packed)) HostCmd_DS_802_11_AUTHENTICATE,
  *PHostCmd_DS_802_11_AUTHENTICATE;

/* Define data structure for HostCmd_RET_802_11_AUTHENTICATE */
typedef struct _HostCmd_DS_802_11_AUTHENTICATE_RSP
{
	u8	MacAddr[6];
	u8	AuthType;
	u8	AuthStatus;
} __attribute__ ((packed)) HostCmd_DS_802_11_AUTHENTICATE_RSP,
  *PHostCmd_DS_802_11_AUTHENTICATE_RSP;

/* Define data structure for HostCmd_CMD_802_11_DEAUTHENTICATE */
typedef struct _HostCmd_DS_802_11_DEAUTHENTICATE
{
	u8	MacAddr[6];
	u16	ReasonCode;
} __attribute__ ((packed)) HostCmd_DS_802_11_DEAUTHENTICATE,
  *PHostCmd_DS_802_11_DEAUTHENTICATE;

/* Define data structure for HostCmd_RET_802_11_DEAUTHENTICATE */
typedef struct _HostCmd_DS_802_11_DEAUTHENTICATE_RESULT
{
	u8 MacAddr[6];
	u8 AuthStatus;
	u8 Reserved;
} __attribute__ ((packed)) HostCmd_DS_802_11_DEAUTHENTICATE_RESULT,
  *PHostCmd_DS_802_11_DEAUTHENTICATE_RESULT;

#ifdef NEW_ASSOCIATION
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
	
#ifdef MULTI_BANDS
	u8				DataRates[A_SUPPORTED_RATES];
#else
#ifdef G_RATE
	u8				DataRates[G_SUPPORTED_RATES];
#else
	u8				DataRates[B_SUPPORTED_RATES];
#endif
#endif
	
#ifdef  WPA
	u8				RsnIE[24];
#endif
} __attribute__ ((packed)) HostCmd_DS_802_11_ASSOCIATE,
    *PHostCmd_DS_802_11_ASSOCIATE;
//struct IEEEtypes_AssocCmd_t IEEEtypes_AssocCmd_t, *IEEEtypes_AssocCmd_t;				
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
} __attribute__ ((packed)) HostCmd_DS_802_11_ASSOCIATE,
  *PHostCmd_DS_802_11_ASSOCIATE;

#endif /* NEW_ASSOCIATE */

/* Define data structure for HostCmd_CMD_802_11_DISASSOCIATE */
typedef struct _HostCmd_DS_802_11_DISASSOCIATE
{
	u8 DestMacAddr[6];
	u16 ReasonCode;            // Disassociation reason code
} __attribute__ ((packed)) HostCmd_DS_802_11_DISASSOCIATE,
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
	u8			ExtRates[12]; /* This was not their before */
#endif
#endif

} __attribute__ ((packed)) HostCmd_DS_802_11_ASSOCIATE_RSP,
  *PHostCmd_DS_802_11_ASSOCIATE_RSP;

/* Define data structure for HostCmd_RET_802_11_AD_HOC_JOIN */
typedef struct _HostCmd_DS_802_11_AD_HOC_RESULT
{
#ifndef	MS8380
  	u8 PAD[3]; //TODO: Remove this when fixed in F/W 3 
 		      //byte right shift in BSSID
#endif
	u8 BSSID[MRVDRV_ETH_ADDR_LEN];
} __attribute__ ((packed)) HostCmd_DS_802_11_AD_HOC_RESULT,
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
} __attribute__ ((packed)) HostCmd_DS_802_11_SET_WEP,
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
} __attribute__ ((packed)) HostCmd_DS_802_3_GET_STAT,
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
} __attribute__ ((packed)) HostCmd_DS_802_11_GET_STAT,
  *PHostCmd_DS_802_11_GET_STAT;

typedef struct _HostCmd_DS_802_11_AD_HOC_STOP {

} __attribute__((packed)) HostCmd_DS_802_11_AD_HOC_STOP, 
	*PHostCmd_DS_802_11_AD_HOC_STOP;

typedef struct _HostCmd_DS_802_11_BEACON_STOP {

} __attribute__((packed)) HostCmd_DS_802_11_BEACON_STOP, 
	*PHostCmd_DS_802_11_BEACON_STOP;

#ifdef DTIM_PERIOD
/* Define data structure for HostCmd_CMD_802_11_SET_DTIM_MULTIPLE */
typedef struct _HostCmd_DS_802_11_SET_DTIM_MULTIPLE {
	u16	DTIM_time_multiple;     
} __attribute__((packed)) HostCmd_DS_802_11_SET_DTIM_MULTIPLE,
			 *PHostCmd_DS_802_11_SET_DTIM_MULTIPLE;
#endif /* DTIM_PERIOD */

typedef enum {
	DesiredBssType_i = 0,
	OpRateSet_i,
	BcnPeriod_i,
	DtimPeriod_i,
	AssocRspTimeOut_i,
	RtsThresh_i,
	ShortRetryLim_i,
	LongRetryLim_i,
	FragThresh_i,
	Dot11D_i,
	Dot11HTPC_i,
	ManufId_i,
	ProdId_i,
	ManufOui_i,
	ManufName_i,
	ManufProdName_i,
	ManufProdVer_i
} SNMP_MIB_INDEX_e;

typedef enum {
	SNMP_MIB_VALUE_INFRA = 1,
	SNMP_MIB_VALUE_ADHOC
} SNMP_MIB_VALUE_e;

/* Define data structure for HostCmd_CMD_802_11_SNMP_MIB */
typedef struct _HostCmd_DS_802_11_SNMP_MIB
{
	u16 QueryType __attribute__ ((packed));	
	u16 OID __attribute__ ((packed));	
	u16 BufSize __attribute__ ((packed));	
	u8 Value[128] __attribute__ ((packed));
} __attribute__ ((packed)) HostCmd_DS_802_11_SNMP_MIB,
  *PHostCmd_DS_802_11_SNMP_MIB;

/* Define data structure for HostCmd_CMD_MAC_REG_MAP */
typedef struct _HostCmd_DS_MAC_REG_MAP
{
	u16 BufferSize;            // 128 u8s
	u8 RegMap[128];
	u16 Reserved;
} __attribute__ ((packed)) HostCmd_DS_MAC_REG_MAP, *PHostCmd_DS_MAC_REG_MAP;

/* Define data structure for HostCmd_CMD_BBP_REG_MAP */
typedef struct _HostCmd_DS_BBP_REG_MAP
{
	u16 BufferSize;            // 128 u8s
	u8 RegMap[128];
	u16 Reserved;
} __attribute__ ((packed)) HostCmd_DS_BBP_REG_MAP, *PHostCmd_DS_BBP_REG_MAP;

/* Define data structure for HostCmd_CMD_RF_REG_MAP */
typedef struct _HostCmd_DS_RF_REG_MAP
{
	u16 BufferSize;            // 64 u8s
	u8 RegMap[64];
	u16 Reserved;
} __attribute__ ((packed)) HostCmd_DS_RF_REG_MAP, *PHostCmd_DS_RF_REG_MAP;

/* Define data structure for HostCmd_CMD_MAC_REG_ACCESS */
typedef struct _HostCmd_DS_MAC_REG_ACCESS
{
	u16 Action;
	u16 Offset;
	u32 Value;
} __attribute__ ((packed)) HostCmd_DS_MAC_REG_ACCESS,
  *PHostCmd_DS_MAC_REG_ACCESS;

/* Define data structure for HostCmd_CMD_BBP_REG_ACCESS */
typedef struct _HostCmd_DS_BBP_REG_ACCESS
{
	u16 Action;
	u16 Offset;
	u8 Value;
	u8 Reserved[3];
} __attribute__ ((packed)) HostCmd_DS_BBP_REG_ACCESS,
  *PHostCmd_DS_BBP_REG_ACCESS;

/* Define data structure for HostCmd_CMD_RF_REG_ACCESS */
typedef struct _HostCmd_DS_RF_REG_ACCESS
{
	u16 Action;
	u16 Offset;
	u8 Value;
	u8 Reserved[3];
} __attribute__ ((packed)) HostCmd_DS_RF_REG_ACCESS,
  *PHostCmd_DS_RF_REG_ACCESS;

/* Define data structure for HostCmd_CMD_802_11_RADIO_CONTROL */
typedef struct _HostCmd_DS_802_11_RADIO_CONTROL
{
	u16 Action;
	u16 Control;
//	u8 Control;	// @bit0: 1/0,on/off, @bit1: 1/0, 
  			// long/short @bit2: 1/0,auto/fix
//	u8 RadioOn;
} __attribute__ ((packed)) HostCmd_DS_802_11_RADIO_CONTROL,
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
} __attribute__ ((packed)) HostCmd_DS_BCA_CONFIG,
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
} __attribute__ ((packed)) HostCmd_DS_802_11_SLEEP_PARAMS,
  *PHostCmd_DS_802_11_SLEEP_PARAMS;

/* SLEEP PERIOD REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_SLEEP_PERIOD
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	Period; 	/* Sleep Period in msec */
	u16 	Direction; 	/* Uplink/Downlink/bidirectional */
} __attribute__ ((packed)) HostCmd_DS_802_11_SLEEP_PERIOD,
  *PHostCmd_DS_802_11_SLEEP_PERIOD;

/* BCA CONFIG TIMESHARE REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_BCA_TIMESHARE
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	TrafficType; 	/* Type: WLAN, BT */
	u32 	TimeShareInterval; /* 20msec - 60000msec */
	u32 	BTTime; 	/* PTA arbiter time in msec */	
} __attribute__ ((packed)) HostCmd_DS_802_11_BCA_TIMESHARE,
  *PHostCmd_DS_802_11_BCA_TIMESHARE;

#ifdef V4_CMDS
/* INACTIVITY TIMEOUT REQUEST & RESPONSE */
typedef struct _HostCmd_DS_802_11_INACTIVITY_TIMEOUT
{
	u16 	Action;		/* ACT_GET/ACT_SET */	
	u16 	Timeout; 	/* Inactivity timeout in msec */
} __attribute__ ((packed)) HostCmd_DS_802_11_INACTIVITY_TIMEOUT,
  *PHostCmd_DS_802_11_INACTIVITY_TIMEOUT;
#endif

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
} __attribute__ ((packed)) HostCmd_DS_802_11_RF_CHANNEL,
  *PHostCmd_DS_802_11_RF_CHANNEL;

/* Define data structure for HostCmd_CMD_802_11_RSSI */
typedef struct _HostCmd_DS_802_11_RSSI {
	u16	SNR;		// signal noise ratio
	u16	NoiseFloor;
	u16	Rsvd1;		// reserved - Host driver should ignore this field
	u16	Rsvd2;		// reserved - Host driver should ignore this field
} __attribute__ ((packed)) HostCmd_DS_802_11_RSSI, *PHostCmd_DS_802_11_RSSI;

/* mac address */
typedef struct _HostCmd_DS_802_11_MAC_ADDRESS {
	u16	Action;
	u8	MacAdd[ETH_ALEN];
} __attribute__((packed)) HostCmd_DS_802_11_MAC_ADDRESS,
	*PHostCmd_DS_802_11_MAC_ADDRESS;

/* PRE TBTT */
typedef struct _HostCmd_DS_802_11_PRE_TBTT
{
	u16 Action;
	u16 Value;
} __attribute__((packed))HostCmd_DS_802_11_PRE_TBTT,
  *PHostCmd_DS_802_11_PRE_TBTT;

/* RFI Tx and Rx modes */
typedef struct _HostCmd_DS_802_11_RFI {
	u16      Mode;
} __attribute__ ((packed)) HostCmd_802_11_RFI, *PHostCmd_DS_802_11_RFI;


/* Define data structure for HostCmd_CMD_802_11_RF_TX_POWER */
typedef struct _HostCmd_DS_802_11_RF_TX_POWER {
	u16 Action;
	u16 CurrentLevel;
} __attribute__ ((packed)) HostCmd_DS_802_11_RF_TX_POWER,
    *PHostCmd_DS_802_11_RF_TX_POWER;

/* Define data structure for HostCmd_CMD_802_11_RF_ANTENNA */
typedef struct _HostCmd_DS_802_11_RF_ANTENNA {
	u16	Action;
	u16	AntennaMode;           // Number of antennas or 0xffff(diversity)
} __attribute__ ((packed)) HostCmd_DS_802_11_RF_ANTENNA,
	*PHostCmd_DS_802_11_RF_ANTENNA;

#ifdef PS_REQUIRED
/* Define data structure for HostCmd_CMD_802_11_PS_MODE */
typedef struct _HostCmd_DS_802_11_PS_MODE
{
	u16 Action;
	u16 PowerMode;             // CAM:PS disable , Max.PSP: PS enable
	u16 MultipleDtim;
} __attribute__ ((packed)) HostCmd_DS_802_11_PS_MODE,
  *PHostCmd_DS_802_11_PS_MODE;

typedef struct _PS_CMD_ConfirmSleep {
	u16	Command;
	u16	Size;
	u16	SeqNum;
	u16	Result;

	u16	Action;
	u16	PowerMode;
	u16	MultipleDtim;
} __attribute__((packed)) PS_CMD_ConfirmSleep, *PPS_CMD_ConfirmSleep;
#endif

/* Define data structure for HostCmd_CMD_802_11_DATA_RATE */
typedef struct _HostCmd_DS_802_11_DATA_RATE {
	u16	Action;
	u16	Reserverd;
#ifdef MULTI_BANDS
	u8	DataRate[A_SUPPORTED_RATES];	// Supported data rate list
#else
#ifdef G_RATE
	u8	DataRate[G_SUPPORTED_RATES];	// Supported data rate list
#else
	u8	DataRate[B_SUPPORTED_RATES];	// Supported data rate list
#endif
#endif
} __attribute__ ((packed)) HostCmd_DS_802_11_DATA_RATE,
	*PHostCmd_DS_802_11_DATA_RATE;

/* Define data structure for start Command in Ad Hoc mode */
typedef struct _HostCmd_DS_802_11_AD_HOC_START
{
	u8 SSID[MRVDRV_MAX_SSID_LENGTH] __attribute__ ((packed));
	u8 BSSType __attribute__ ((packed));
	u16 BeaconPeriod __attribute__ ((packed));
	u8 DTIMPeriod __attribute__ ((packed));
	IEEEtypes_SsParamSet_t SsParamSet __attribute__ ((packed));
	IEEEtypes_PhyParamSet_t PhyParamSet __attribute__ ((packed));
	u16 ProbeDelay __attribute__ ((packed));
	IEEEtypes_CapInfo_t Cap __attribute__ ((packed));
	u8 DataRate[14] __attribute__ ((packed));
	u8 Reserved[14] __attribute__ ((packed));
} __attribute__ ((packed)) HostCmd_DS_802_11_AD_HOC_START,
  *PHostCmd_DS_802_11_AD_HOC_START;

/* Define data structure for Join Command in Ad Hoc mode */
typedef struct _HostCmd_DS_802_11_AD_HOC_JOIN
{
	BSS_DESCRIPTION_SET_ALL_FIELDS BssDescriptor;
	u16 FailTimeOut;
	u16 ProbeDelay;
	u8 DataRates[14];
} __attribute__ ((packed)) HostCmd_DS_802_11_AD_HOC_JOIN,
  *PHostCmd_DS_802_11_AD_HOC_JOIN;

#ifdef WPA
typedef struct _HostCmd_DS_802_11_QUERY_RSN_OPTION {
	u8                   RSN_Capable;
	u8                   Reserved[3];
}__attribute__((packed)) HostCmd_DS_802_11_QUERY_RSN_OPTION, 
*PHostCmd_DS_802_11_QUERY_RSN_OPTION;

typedef struct _HostCmd_DS_802_11_ENABLE_RSN {
	u16                  Action;	
	u16                  Enable;
}__attribute__((packed)) HostCmd_DS_802_11_ENABLE_RSN,
*PHostCmd_DS_802_11_ENABLE_RSN;

typedef struct _HostCmd_DS_802_11_QUERY_TKIP_REPLY_CNTRS {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
	u32                   NumTkipCntrs;
}__attribute__((packed)) HostCmd_DS_802_11_QUERY_TKIP_REPLY_CNTRS,
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
}__attribute__((packed)) HostCmd_DS_802_11_CONFIG_RSN , 
*PHostCmd_DS_802_11_CONFIG_RSN;

typedef struct _HostCmd_DS_802_11_UNICAST_CIPHER {
	u16                  Action;
	u8                   UnicastCipher[4];
	u16                  Enabled;
}__attribute__((packed)) HostCmd_DS_802_11_UNICAST_CIPHER,
*PHostCmd_DS_802_11_UNICAST_CIPHER;

typedef struct _HostCmd_DS_802_11_MCAST_CIPHER {
	u16                  Action;
	u8                   McastCipher[4];
}__attribute__((packed)) HostCmd_DS_802_11_MCAST_CIPHER,
*PHostCmd_DS_802_11_MCAST_CIPHER;

typedef struct _HostCmd_DS_802_11_RSN_AUTH_SUITES {
	u16                  Action;
	u8                   AuthSuites[4];
	u16                  Enabled;
}__attribute__((packed)) HostCmd_DS_802_11_RSN_AUTH_SUITES,
*PHostCmd_DS_802_11_RSN_AUTH_SUITES;
#endif

typedef struct _HostCmd_DS_802_11_RSN_STATS {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
	u8                   MacAddr[ETH_ALEN];
	u8                   Version;
	u8                   SelUnicastCipher[4];
	u32                   TkipIcvErrors;
	u32                   TkipLocalMicFailures;
	u32                   TkipRemoteMicFailures;
	u32                   TkipCounterMeasureInvoked;
	u32                   WrapFormatErrors;
	u32                   WrapReplays;
	u32                   WrapDecryptErrors;
	u32                   CcmpFormatErrors;
	u32                   CcmpReplays;
	u32                   CcmpDecryptErrors;
}__attribute__((packed)) HostCmd_DS_802_11_RSN_STATS,
*PHostCmd_DS_802_11_RSN_STATS;

typedef struct _HostCmd_DS_802_11_PWK_KEY {
	u16                  Action;
	u8                   TkipEncryptKey[16];
	u8                   TkipTxMicKey[8];
	u8                   TkipRxMicKey[8];
}__attribute__((packed)) HostCmd_DS_802_11_PWK_KEY, 
*PHostCmd_DS_802_11_PWK_KEY;

#ifndef WPA2
typedef struct _HostCmd_DS_802_11_GRP_KEY {
	u16                  Action;
	u8                   TkipEncryptKey[16];
	u8                   TkipTxMicKey[8];
	u8                   TkipRxMicKey[8];
}__attribute__((packed)) HostCmd_DS_802_11_GRP_KEY, *PHostCmd_DS_802_11_GRP_KEY;
#endif

typedef struct _HostCmd_DS_802_11_PAIRWISE_TSC {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
        u16                  Action;
        u32                   Txlv32;
        u16                  Txlv16;
}__attribute__((packed)) HostCmd_DS_802_11_PAIRWISE_TSC, 
	*PHostCmd_DS_802_11_PAIRWISE_TSC;

typedef struct _HostCmd_DS_802_11_GROUP_TSC {
	u16                  CmdCode;
	u16                  Size;
	u16                  SeqNum;
	u16                  Result;
	u16                  Action;
	u32                   Txlv32;
	u16                  Txlv16;
}__attribute__((packed)) HostCmd_DS_802_11_GROUP_TSC, 
		*PHostCmd_DS_802_11_GROUP_TSC;

#ifdef WPA2
typedef enum {
	KEY_TYPE_ID_WEP = 0,
	KEY_TYPE_ID_TKIP,
	KEY_TYPE_ID_AES
} KEY_TYPE_ID;

typedef enum {
	KEY_INFO_WEP_DEFAULT_KEY = 0x01
} KEY_INFO_WEP;

typedef enum {
	KEY_INFO_TKIP_MCAST = 0x01,
	KEY_INFO_TKIP_UNICAST = 0x02,
	KEY_INFO_TKIP_ENABLED = 0x04
} KEY_INFO_TKIP;

typedef enum {
	KEY_INFO_AES_MCAST = 0x01,
	KEY_INFO_AES_UNICAST = 0x02,
	KEY_INFO_AES_ENABLED = 0x04
} KEY_INFO_AES;

typedef union _KeyInfo_WEP_t {
	u8	Reserved;		/* bits 5-15: Reserved */
	u8	WepKeyIndex;		/* bits 1-4: Specifies the index of key */
	u8	isWepDefaultKey;	/* bit 0: Specifies that this key is to be used as the default for TX data packets */
} __attribute__ ((packed)) KeyInfo_WEP_t;

typedef union _KeyInfo_TKIP_t {
	u8	Reserved;		/* bits 3-15: Reserved */
	u8	isKeyEnabled;		/* bit 2: Specifies that this key is enabled and valid to use */
	u8	isUnicastKey;		/* bit 1: Specifies that this key is to be used as the unicast key */
	u8	isMulticastKey;		/* bit 0: Specifies that this key is to be used as the multicast key */
} __attribute__ ((packed)) KeyInfo_TKIP_t;

typedef union _KeyInfo_AES_t {
	u8	Reserved;		/* bits 3-15: Reserved */
	u8	isKeyEnabled;		/* bit 2: Specifies that this key is enabled and valid to use */
	u8	isUnicastKey;		/* bit 1: Specifies that this key is to be used as the unicast key */
	u8	isMulticastKey;		/* bit 0: Specifies that this key is to be used as the multicast key */
} __attribute__ ((packed)) KeyInfo_AES_t;

typedef struct _KeyMaterial_TKIP_t {
	u8	TkipKey[16];			/* TKIP encryption/decryption key */
	u8	TkipTxMicKey[16];		/* TKIP TX MIC Key */
	u8	TkipRxMicKey[16];		/* TKIP RX MIC Key */
} __attribute__((packed)) KeyMaterial_TKIP_t,
		*PKeyMaterial_TKIP_t;

typedef struct _KeyMaterial_AES_t {
	u8	AesKey[16];			/* AES encryption/decryption key */
} __attribute__((packed)) KeyMaterial_AES_t,
		*PKeyMaterial_AES_t;

typedef struct _MrvlIEtype_KeyParamSet_t {
	u16	Type;		/* Type ID */
	u16	Length;		/* Length of Payload */
	u16	KeyTypeId;	/* Type of Key: WEP=0, TKIP=1, AES=2 */
	u16	KeyInfo;	/* Key Control Info specific to a KeyTypeId */
	u16	KeyLen;		/* Length of key */
	u8	Key[32];	/* Key material of size KeyLen */
} __attribute__((packed)) MrvlIEtype_KeyParamSet_t,
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
} __attribute__((packed)) HostCmd_DS_802_11_KEY_MATERIAL,
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
} __attribute__ ((packed)) HostCmd_DS_802_11_HOST_WAKE_UP_CFG;
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
} __attribute__((packed)) HostCmd_DS_802_11_CAL_DATA,
	*pHostCmd_DS_802_11_CAL_DATA;
#endif /* CAL_DATA */


#ifdef MULTI_BANDS
#define BS_802_11B	0
#define BS_802_11G	1
#define BS_802_11A	2
typedef struct _HostCmd_DS_802_11_BAND_CONFIG {
	u16		Action;		/* 0: GET;
					   1: SET */
	u16		BandSelection; 	/* Select 802.11b = 0 (2.4GHz) */
					/* Select 802.11g = 1 (2.4GHz) */
					/* Select 802.11a = 2 (5 GHz ) */
	u16		Channel;
} __attribute__((packed)) HostCmd_DS_802_11_BAND_CONFIG,
	*pHostCmd_DS_802_11_BAND_CONFIG;
#endif

typedef	struct _HostCmd_DS_802_11_EEPROM_ACCESS {
	u16	Action; /* ACT_GET / ACT_SET */
	u16	Offset; /* Multiple of 4. Example: 0, 4, 8 ... */
	u16	ByteCount; /* Multiple of 4. Example: 4, 8, 12 ... */
	u8	Value; /* The caller must provide a buffer of atleast 
			  byte count size starting from here */
} __attribute__((packed)) HostCmd_DS_802_11_EEPROM_ACCESS,
	*pHostCmd_DS_802_11_EEPROM_ACCESS;

#ifdef GSPI8385
typedef struct _HostCmd_DS_CMD_GSPI_BUS_CONFIG {
	u16	Action;  /* ACT_GET / ACT_SET */
	u16	BusDelayMode; /* Data format Bit[1:0], Delay method Bit[2] */
	u16	HostTimeDelayToReadPort; /* Number of dummy clocks 
					    to wait for read r/w port */
	u16	HostTimeDelayToReadregister; /* Number of dummy 
						clocks to wait for read reg */
} __attribute__((packed)) HostCmd_DS_CMD_GSPI_BUS_CONFIG,
	*pHostCmd_DS_CMD_GSPI_BUS_CONFIG;
#endif /* GSPI8385 */

#ifdef BG_SCAN

#ifndef TLV_SCAN

typedef struct _MrvlIEtypesHeader_t {
	u16	Type;
	u16	Len;
} __attribute__ ((packed)) MrvlIEtypesHeader_t;

typedef struct _ChanScanParamSet_t{
	u8	RadioType;
	u8	ChanNumber;
	u8	ScanType;
	u16	MinScanTime;
	u16	ScanTime;
} __attribute__ ((packed)) ChanScanParamSet_t;

typedef struct 	_MrvlIEtypes_ChanListParamSet_t {
	MrvlIEtypesHeader_t	Header;
	ChanScanParamSet_t 	ChanScanParam[1];
} __attribute__ ((packed)) MrvlIEtypes_ChanListParamSet_t;

#endif /* TLV_SCAN */

typedef struct _HostCmd_DS_802_11_BG_SCAN_CONFIG  {
	u16	Action;
	u8	Enable;
	u8	BssType;		/* Infrastructure / IBSS */
	u8	ChannelIsPerScan;	/* No of channels to scan at
					   one scan */
	u8	DiscardWhenFull;	/* 0 - Discard old scan results
					   1 - Discard new scan 
					   	results */
	u16	ScanInternal;		/* Interval b/w consecutive 
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
	u8 	ChannelList[0];		/* List Channel of in TLV scan
					   format */
} __attribute__((packed)) HostCmd_DS_802_11_BG_SCAN_CONFIG,
	*pHostCmd_DS_802_11_BG_SCAN_CONFIG;

typedef struct _HostCmd_DS_802_11_BG_SCAN_QUERY {
	u8	Flush;
} __attribute__((packed)) HostCmd_DS_802_11_BG_SCAN_QUERY,
	*pHostCmd_DS_802_11_BG_SCAN_QUERY;

typedef struct _HostCmd_DS_802_11_BG_SCAN_QUERY_RSP {
	u32				ReportCondition;
	HostCmd_DS_802_11_SCAN_RSP	scanresp;
//	IEEEtypes_BssDesc_t	BSSDesc[16];
} __attribute__ ((packed)) HostCmd_DS_802_11_BG_SCAN_QUERY_RSP,
				*PHostCmd_DS_802_11_BG_SCAN_QUERY_RSP;
#endif /* BG_SCAN */

typedef struct _HostCmd_DS_802_11_GENERATE_ATIM {
	u16	Action;
	u16	Enable;
} __attribute__((packed)) HostCmd_DS_802_11_GENERATE_ATIM,
	*PHostCmd_DS_802_11_GENERATE_ATIM;

#ifdef AUTO_FREQ_CTRL
typedef struct {
	u16 CmdCode;
	u16 Size;
	u16 SeqNum;
	u16 Result;

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
} __attribute__ ((packed)) afc_cmd;

#define afc_thre    b.auto_mode.threshold
#define afc_period  b.auto_mode.period
#define afc_toff    b.manual_mode.timing_offset
#define afc_foff    b.manual_mode.carrier_offset

#endif /* AUTO_FREQ_CTRL */

typedef struct _HostCmd_DS_COMMAND {
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
	HostCmd_DS_MAC_MULTICAST_ADR		madr;
	HostCmd_DS_802_11_AD_HOC_JOIN		adj;
	HostCmd_DS_802_11_RADIO_CONTROL		radio;
	HostCmd_DS_802_11_RF_CHANNEL		rfchannel;
	HostCmd_DS_802_11_RSSI			rssi;
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
#endif
#ifdef HOST_WAKEUP
	HostCmd_DS_802_11_HOST_WAKE_UP_CFG	hostwakeupcfg;
#endif
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG 		band;
#endif
	HostCmd_DS_802_11_EEPROM_ACCESS		rdeeprom;
#ifdef GSPI8385
	HostCmd_DS_CMD_GSPI_BUS_CONFIG		gspicfg;
#endif /* GSPI8385 */
	HostCmd_DS_802_11_GENERATE_ATIM 	genatim;
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
	HostCmd_DS_802_11_SLEEP_PARAMS 		sleep_params;
	HostCmd_DS_802_11_BCA_TIMESHARE		bca_timeshare;
#ifdef V4_CMDS
	HostCmd_DS_802_11_INACTIVITY_TIMEOUT	inactivity_timeout;
#endif
	HostCmd_DS_802_11_SLEEP_PERIOD		ps_sleeppd;
	} params;

} __attribute__ ((packed)) HostCmd_DS_COMMAND;

#define	S_DS_GEN	sizeof(HostCmd_DS_GEN)
#endif
