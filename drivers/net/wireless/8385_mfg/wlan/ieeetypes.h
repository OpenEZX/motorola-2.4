/****************** (c) Marvell Semiconductor, Inc., 2004 ********************
 *
 *	$HEADER$
 *
 *      File:		ieeetypes.h
 *
 *      Purpose:	This file contains the data structure and defines 
 *      		required for the IEEE 802.11 implementation.
 *
 ***************************************************************************/

#ifndef _IEEETYPES_H_
#define _IEEETYPES_H_


#ifndef	ETH_ALEN
#define ETH_ALEN	6
#endif


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

	CHALLENGE_TEXT = 16

#ifdef ENABLE_802_11H_TPC
	,POWER_CONSTRAINT = 32,
	POWER_CAPABILITY = 33,
	TPC_REQUEST	 = 34,
	TPC_REPORT	 = 35
#endif

#ifdef FWVERSION3
	,EXTENDED_SUPPORTED_RATES = 50
#endif

#ifdef WPA
	,WPA_IE = 221
#ifdef WPA2
	,WPA2_IE = 48
#endif	//WPA2
#endif	//WPA
        ,EXTRA_IE = 133
} __attribute__ ((packed)) IEEEtypes_ElementId_e;

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
} __attribute__ ((packed)) IEEEtypes_CapInfo_t;
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
	u8	Rsvrd2:2;
	u8	DSSSOFDM:1;
	u8	Rsrvd1:2;
} __attribute__ ((packed)) IEEEtypes_CapInfo_t;
#endif  /* BIG_ENDIAN */

typedef struct IEEEtypes_CfParamSet_t {
	u8	ElementId;
	u8	Len;
	u8	CfpCnt;
	u8	CfpPeriod;
	u16	CfpMaxDuration;
	u16	CfpDurationRemaining;
} __attribute__ ((packed)) IEEEtypes_CfParamSet_t;

typedef struct IEEEtypes_IbssParamSet_t {
	u8	ElementId;
	u8	Len;
	u16	AtimWindow;
} __attribute__ ((packed)) IEEEtypes_IbssParamSet_t;

typedef union IEEEtypes_SsParamSet_t {
	IEEEtypes_CfParamSet_t		CfParamSet;
	IEEEtypes_IbssParamSet_t	IbssParamSet;
} __attribute__ ((packed)) IEEEtypes_SsParamSet_t;

typedef struct IEEEtypes_FhParamSet_t {
	u8	ElementId;
	u8	Len;
	u16	DwellTime;
	u8	HopSet;
	u8	HopPattern;
	u8	HopIndex;
} __attribute__ ((packed)) IEEEtypes_FhParamSet_t;

typedef struct IEEEtypes_DsParamSet_t {
	u8	ElementId;
	u8	Len;
	u8	CurrentChan;
} __attribute__ ((packed)) IEEEtypes_DsParamSet_t;

typedef union IEEEtypes_PhyParamSet_t {
	IEEEtypes_FhParamSet_t	FhParamSet;
	IEEEtypes_DsParamSet_t	DsParamSet;
} __attribute__ ((packed)) IEEEtypes_PhyParamSet_t;

#ifdef ENABLE_802_11D
typedef struct IEEEtypes_SubbandSet_t {
	u8	FirstChan;
	u8	NoOfChan;
	u8	MaxTxPwr;	
} __attribute__ ((packed)) IEEEtypes_SubbandSet_t;

typedef struct IEEEtypes_CountryInfoSet_t {
	u8	ElementId;
	u8	Len;
	u8	CountryCode[3];
	IEEEtypes_SubbandSet_t	Subband[1];
} __attribute__ ((packed)) IEEEtypes_CountryInfoSet_t;

typedef struct IEEEtypes_CountryInfoFullSet_t {
	u8	ElementId;
	u8	Len;
	u8	CountryCode[3];
	IEEEtypes_SubbandSet_t	Subband[MRVDRV_MAX_SUBBAND_802_11D];
} __attribute__ ((packed)) IEEEtypes_CountryInfoFullSet_t;
#endif

#ifdef ENABLE_802_11H_TPC
typedef struct IEEEtypes_PowerConstraint_t {
	u8	ElementId;
	u8	Len;
	u8	LocalPowerConstraint;
} __attribute__ ((packed)) IEEEtypes_PowerConstraint_t;

typedef struct IEEEtypes_PowerCapability_t {
	u8	ElementId;
	u8	Len;
	s8	MinTxPowerCapability;
	s8	MaxTxPowerCapability;
} __attribute__ ((packed)) IEEEtypes_PowerCapability_t;

typedef struct IEEEtypes_TPCRequest_t {
	u8	ElementId;
	u8	Len;
} __attribute__ ((packed)) IEEEtypes_TPCRequest_t;

typedef struct IEEEtypes_TPCReport_t {
	u8	ElementId;
	u8	Len;
	s8	TxPower;
	s8	Linkmargin;
} __attribute__ ((packed)) IEEEtypes_TPCReport_t;
#endif /* ENABLE_802_11H_TPC */

typedef struct IEEEtypes_BssDesc_t {
	u8	BSSID[ETH_ALEN];
	u8	SSID[MRVDRV_MAX_SSID_LENGTH];
	u8	BSSType;
	u16	BeaconPeriod;
	u8	DTIMPeriod;
	u8	TimeStamp[8];
	u8	LocalTime[8];
	IEEEtypes_PhyParamSet_t	PhyParamSet;
	IEEEtypes_SsParamSet_t	SsParamSet;
	IEEEtypes_CapInfo_t	Cap;
	u8	DataRates[WLAN_SUPPORTED_RATES];
#ifdef ENABLE_802_11H_TPC
	u8	Sensed11H;
	IEEEtypes_PowerConstraint_t	PowerConstraint;
	IEEEtypes_PowerCapability_t	PowerCapability;
	IEEEtypes_TPCReport_t		TPCReport;
#endif
#ifdef ENABLE_802_11D
	IEEEtypes_CountryInfoFullSet_t	CountryInfo;
#endif
} __attribute__ ((packed)) IEEEtypes_BssDesc_t,
 BSS_DESCRIPTION_SET_ALL_FIELDS, *PBSS_DESCRIPTION_SET_ALL_FIELDS;

#endif	/* _IEEETYPES_H_ */
