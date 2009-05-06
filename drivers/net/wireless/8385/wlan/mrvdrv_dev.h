/******************* (c) Marvell Semiconductor, Inc., 2001 ********************
 *  $Header&
 *
 * 	File:	mrvdrv_dev.h
 * 	
 *      Purpose:
 *          This file contains definitions and data structures that are specific
 *          to Marvell 802.11 NIC. It contains the Device Information
 *          structure wlan_adapter.
 *****************************************************************************/

#ifndef _MRVDRV_DEV_H_
#define _MRVDRV_DEV_H_

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__((packed))
#endif

struct tx_thr_data {
	PVOID		ctx;
	struct sk_buff *packet;
};

#ifdef QUEUE_TX_PKT

typedef struct _TX_QUEUE
{
	struct _TX_QUEUE *Next __ATTRIB_PACK__;
	PVOID _DO_NOT_USE      __ATTRIB_PACK__;
} __ATTRIB_PACK__ TX_QUEUE, *PTX_QUEUE;

typedef struct _TX_QUEUE_HEADER
{
	PTX_QUEUE Head __ATTRIB_PACK__;
	PTX_QUEUE Tail __ATTRIB_PACK__;
} __ATTRIB_PACK__ TX_QUEUE_HEADER, *PTX_QUEUE_HEADER;

#endif

#ifdef FAST_RETURN_PACKET

typedef struct _RXPDQIDX
{
	PVOID _DO_NOT_USE __ATTRIB_PACK__;
	UINT Index         __ATTRIB_PACK__;
} __ATTRIB_PACK__ RX_PDQ_IDX, *PRX_PDQ_IDX;

#endif

//#define READY_TO_SEND                   0
//#define STOP_SENDING                    1
#define	MAX_BSSID_PER_CHANNEL		16

/* For the extended Scan */
#define MAX_EXTENDED_SCAN_BSSID_LIST    MAX_BSSID_PER_CHANNEL * \
						MRVDRV_MAX_CHANNEL_SIZE + 1

typedef struct _PER_CHANNEL_BSSID_LIST_DATA {
	u8	ucStart;
	u8	ucNumEntry;
} PER_CHANNEL_BSSID_LIST_DATA, *PPER_CHANNEL_BSSID_LIST_DATA;

typedef struct _EXTENDED_SCAN_PARAMS {
	/* per channel data */
	PER_CHANNEL_BSSID_LIST_DATA PerChannelData[MRVDRV_MAX_CHANNEL_SIZE];

	/* could be made more efficient by finding out exactly how many channels
	 are there and allocate the memory accordingly */

	/* list of merged BSSID */
	WLAN_802_11_BSSID	MergedBSSIDList[MAX_EXTENDED_SCAN_BSSID_LIST];

	/* last channel scanned indexed to the channel table */
	u8			ucLastScannedChannelIndex;

	/* total number of BSSID in MergedBSSIDList */
	u8			ucNumberOfBSSID;

	/* number of valid channels in the current channel table */
	u8			ucNumValidChannel;

	spinlock_t		ListLock;

	/* perform a complete scan */
	BOOLEAN         bPerformCompleteScan;
} EXTENDED_SCAN_PARAMS, *PEXTENDED_SCAN_PARAMS;

#ifdef FWVERSION3

typedef struct _MRV_BSSID_IE_LIST {
	WLAN_802_11_FIXED_IEs   FixedIE;
	u8                   VariableIE[MRVDRV_SCAN_LIST_VAR_IE_SPACE];
} MRV_BSSID_IE_LIST, *PMRV_BSSID_IE_LIST;

#endif

#define	MAX_REGION_CHANNEL_NUM	2

typedef struct _CHANNEL_FREQ_POWER {
	u16	Channel;	/* Channel Number		*/
	u32	Freq;		/* Frequency of this Channel	*/
	u16	MaxTxPower;	/* Max allowed Tx power level	*/
} CHANNEL_FREQ_POWER;

typedef	struct	_REGION_CHANNEL {
	BOOLEAN	Valid;		/* TRUE if this entry is valid		     */
	u8	Region;		/* Region code for US, Japan ...	     */
	u8	Band;		/* B, G, or A, used for BAND_CONFIG cmd	     */
	u8	NrCFP;		/* Actual No. of elements in the array below */
	CHANNEL_FREQ_POWER	*CFP;
} REGION_CHANNEL;

typedef enum _WLAN_802_11_AUTH_ALG
{
	AUTH_ALG_OPEN_SYSTEM = 1,
	AUTH_ALG_SHARED_KEY = 2,
	AUTH_ALG_NETWORK_EAP = 8,
} WLAN_802_11_AUTH_ALG;

typedef enum _WLAN_802_1X_AUTH_ALG
{
	WLAN_1X_AUTH_ALG_NONE = 1,
	WLAN_1X_AUTH_ALG_LEAP = 2,
	WLAN_1X_AUTH_ALG_TLS = 4,
	WLAN_1X_AUTH_ALG_TTLS = 8,
	WLAN_1X_AUTH_ALG_MD5 = 16,
} WLAN_802_1X_AUTH_ALG;

typedef enum _WLAN_802_11_ENCRYPTION_MODE
{
	CIPHER_NONE,
	CIPHER_WEP40,
	CIPHER_TKIP,
	CIPHER_CCMP,
	CIPHER_WEP104,
} WLAN_802_11_ENCRYPTION_MODE;

typedef struct  _wlan_802_11_security_t
{
	BOOLEAN				WPAEnabled;
#ifdef WPA2
	BOOLEAN				WPA2Enabled;
#endif // WPA2
	WLAN_802_11_WEP_STATUS 		WEPStatus __ATTRIB_PACK__;
	WLAN_802_11_AUTHENTICATION_MODE AuthenticationMode __ATTRIB_PACK__;
	WLAN_802_1X_AUTH_ALG 		Auth1XAlg __ATTRIB_PACK__;
	WLAN_802_11_ENCRYPTION_MODE 	EncryptionMode __ATTRIB_PACK__;
} wlan_802_11_security_t;



#ifdef ENABLE_802_11D
/******************************************************
    Data structure definition to support 802.11d
********************************************************/
/* domain regulatory information */
typedef struct _wlan_802_11d_domain_reg_t
{
	u8			CountryCode[3];	/*country Code*/
	u8			NoOfSubband;	/*No. of subband*/
	IEEEtypes_SubbandSet_t	Subband[MRVDRV_MAX_SUBBAND_802_11D]; 
} wlan_802_11d_domain_reg_t;
	
typedef enum _wlan_802_11d_status_t
{
	UNKNOWN_11D,
	KNOWN_UNSET_11D,
	KNOWN_SET_11D,
	DISABLED_11D,
} wlan_802_11d_status_t;

/* Data for state machine, refer to . ... */
typedef struct _wlan_802_11d_state_t
{
	BOOLEAN				DomainRegValid; /*True for Valid; False for unvalid*/ 	
	BOOLEAN 			HandleNon11D;   
	/* Specify how to handle non-11d compliant AP. True for get default 
	 * Domain INfo from EEPROM; False for waiting until 11d compliant AP is 
	 * sensed */
	BOOLEAN				Enable11D; /* True for Enabling  11D*/
	BOOLEAN				CountryInfoIEFound; /* TRUE: Found CountryIE in this Scan*/
	BOOLEAN				CountryInfoChanged; /* TRUE: New CountryIE Found in the Scan resp*/
	BOOLEAN				AssocAdhocAllowed;
	/* True: Allowed to association or Adhoc join */
	wlan_802_11d_status_t  		Status;	
} wlan_802_11d_state_t;
#endif

#ifdef ENABLE_802_11H_TPC
typedef struct _wlan_802_11h_tpc_request_param_t {
/*request Parameter*/
	u8	mac[ETH_ALEN];
	u16	timeout;
	u8	index;
/*result*/
	u8	RetCode;
	s8	TxPower;
	s8	LinkMargin;
	s8	RSSI;
} wlan_802_11h_tpc_request_param_t;

typedef struct _wlan_802_11h_tpc_info_param_t {
	u8		Chan;
	u8		PowerConstraint;
} wlan_802_11h_tpc_info_param_t;

typedef struct _wlan_802_11h_tpc_t { 
	BOOLEAN		Enable11HTPC;	/* True for Enable 11H */
	s8		MinTxPowerCapability;
	s8		MaxTxPowerCapability;
	s8		UsrDefPowerConstraint;
	wlan_802_11h_tpc_info_param_t		InfoTpc;
	wlan_802_11h_tpc_request_param_t	RequestTpc;
} wlan_802_11h_tpc_t;
#endif /* ENABLE_802_11H_TPC*/

/* Current Basic Service Set State Structure */
typedef struct CurrentBSSParams
{
	WLAN_802_11_SSID 	ssid;
	WLAN_802_11_BSSID 	bssid;
	u8			band;
	u8			channel;
	u8			DataRates[WLAN_SUPPORTED_RATES] __ATTRIB_PACK__;
} CurrentBSSParams;

/* sleep_params */	
typedef struct SleepParams
{
	u16 			sp_error;
	u16 			sp_offset;
	u16 			sp_stabletime;
	u8 			sp_calcontrol;	
	u8 			sp_extsleepclk;	
	u16 			sp_reserved; /* for hardware debugging */
} SleepParams;

/////////////////////////////////////////////////////////////////
//
//			Adapter information
//
/////////////////////////////////////////////////////////////////

typedef struct _wlan_adapter
{
	/* 
	 * 	STATUS variables
	 */

	WLAN_HARDWARE_STATUS 	HardwareStatus;
	u16 			FWStatus;
	u16 			MACStatus;
	u16 			RFStatus;
	u16 			CurrentChannel;

	/*
	 *	FIRMWARE 
	 */

	u32 			FWReleaseNumber;

#ifdef FWVERSION3
	char 			Buffer[20][100];

	/* store the BSSID's information element */
	MRV_BSSID_IE_LIST       IEBuffer[MRVDRV_MAX_BSSID_LIST];
	u32			fwCapInfo;
#endif

	u8			TmpTxBuf[WLAN_UPLD_SIZE];
	u8			chip_rev;

	/* Command-related variables */
	u16 			SeqNum;
	CmdCtrlNode 		*CmdArray;
	CmdCtrlNode 		*CurCmd;	/* Current Command */

	/* Command Queues */
	struct list_head 	CmdFreeQ;	/* Free command buffers */
	struct list_head 	CmdPendingQ;	/* Pending command buffers */	

	struct sk_buff 		*SentPacket;

	/* Variables brought in from private structure */
	int			irq;
	int			TxCount;

	/* Async and Sync Event variables */
	u32			IntCounter;
	u32			IntCounterSaved;	/* save int for DS/PS */
	u32			EventCause;
	u32			EventCounter;
	u8			nodeName[16]; 		/* for the nickname */
	
	/* spin locks */
	spinlock_t 		QueueSpinLock  __ATTRIB_ALIGN__;

	/* Timers */ 
	WLAN_DRV_TIMER 		MrvDrvCommandTimer  __ATTRIB_ALIGN__;
	BOOLEAN 		CommandTimerIsSet;
	BOOLEAN 		isCommandTimerExpired;
	BOOLEAN 		TimerIsSet;
	u32 			TimerInterval;
	WLAN_DRV_TIMER		MrvDrvTimer __ATTRIB_ALIGN__;

	/* Event Queues */
	wait_queue_head_t	scan_q __ATTRIB_ALIGN__;	
	wait_queue_head_t	association_q __ATTRIB_ALIGN__;
	u8			HisRegCpy;

#ifdef MANF_CMD_SUPPORT
	u32			mfg_cmd_len;
	int			mfg_cmd_flag; 
	u32			mfg_cmd_resp_len;
	u8			*mfg_cmd;
	wait_queue_head_t	mfg_cmd_q;
#endif

#ifdef THROUGHPUT_TEST
	u8			ThruputTest;
	u32			NumTransfersTx;
	u32			NumTransfersRx;
#endif /* THROUGHPUT_TEST */

#ifdef BG_SCAN
	pHostCmd_DS_802_11_BG_SCAN_CONFIG 	bgScanConfig;
	u32 					bgScanConfigSize;
#endif /* BG_SCAN */

#ifdef WMM
	WMM_DESC				wmm;
	HostCmd_DS_802_11_WMM_TSPEC 		tspec;
	HostCmd_DS_802_11_WMM_ACK_POLICY 	ackpolicy;
	HostCmd_DS_802_11_WMM_GET_STATUS	getstatus;
	HostCmd_DS_802_11_WMM_PRIO_PKT_AVAIL	priopktavail;
#endif /* WMM */

	/* 
	 * 	MAC 
	 */

	CurrentBSSParams 	CurBssParams;

	WLAN_802_11_NETWORK_INFRASTRUCTURE InfrastructureMode;
	u32 			ulAttemptedBSSIDIndex;
	u32 			ulCurrentBSSIDIndex;
	u8 			CurrentBSSID[MRVDRV_ETH_ADDR_LEN];
	WLAN_802_11_CONFIGURATION CurrentConfiguration;
	WLAN_802_11_BSSID 	CurrentBSSIDDesciptor;
	WLAN_802_11_SSID 	AttemptedSSIDBeforeScan;

	WLAN_802_11_SSID 	PreviousSSID;
	u8 			PreviousBSSID[MRVDRV_ETH_ADDR_LEN];

	BOOLEAN 		RequestViaBSSID;
        u8   			RequestedBSSID[ETH_ALEN];

	PWLAN_802_11_BSSID 	BSSIDList;
	
	u32 			ulNumOfBSSIDs;
	BOOLEAN 		bAutoAssociation;
	BOOLEAN 		bIsScanInProgress;
	BOOLEAN 		bIsAssociationInProgress;
	BOOLEAN 		bIsPendingReset;
	u32 			ulAttemptedSSIDIndex;
	
	u8			ScanType;
	u32			ScanMode;

	/* Specific scan using ssid */
	WLAN_802_11_SSID 	SpecificScanSSID;
	BOOLEAN 		SetSpecificScanSSID;

	/* Specific scan using bssid */
	WLAN_802_11_MAC_ADDRESS SpecificScanBSSID;

#ifdef	PROGRESSIVE_SCAN
	u32			ScanChannelsLeft;
	u16 			ChannelsPerScan;
#endif

	u16			BeaconPeriod;

	u8			AdhocCreate;

#ifndef NEW_ASSOCIATION
	/* Capability Info used in Association request  in infrastructure */
	IEEEtypes_CapInfo_t	infraCapInfo;
#endif

	/* Reassociation on and off */
	BOOLEAN 		Reassoc_on;
	BOOLEAN			ATIMEnabled;

	/* MAC address information */
	u8 			PermanentAddr[MRVDRV_ETH_ADDR_LEN];
	u8 			CurrentAddr[MRVDRV_ETH_ADDR_LEN];
	u8 MulticastList[MRVDRV_MAX_MULTICAST_LIST_SIZE][MRVDRV_ETH_ADDR_LEN];
	u32 			NumOfMulticastMACAddr;

	/* 802.3 Statistics */
	/* TODO: Enable this if the firmware supports */
	//HostCmd_DS_802_3_GET_STAT wlan802_3Stat;
	
	/* 802.11 statistics */
	HostCmd_DS_802_11_GET_STAT wlan802_11Stat;

	BOOLEAN 		AdHocCreated;
	BOOLEAN 		AdHocFailed;

	WLAN_802_11_ANTENNA 	TxAntenna;
	WLAN_802_11_ANTENNA 	RxAntenna;
	
	u32 			Channel;
	u8 			AdhocChannel;
	
	WLAN_802_11_FRAGMENTATION_THRESHOLD FragThsd;
	WLAN_802_11_RTS_THRESHOLD RTSThsd;
	
	/* 0:auto, 1:1 Mbps, 2:2 Mbps, 3:5.5 Mbps,4:11 Mbps, 5:22 Mbps */
	u32 			DataRate; 
	u32 			Is_DataRate_Auto;

	u32			ExtendedScan;
	PEXTENDED_SCAN_PARAMS	pExtendedScanParams;

	/* number of association attempts for the current SSID cmd */
	u32 			m_NumAssociationAttemp;
	u16			ListenInterval; 
	u8			TxRetryCount;

	/* Tx-related variables (for single packet tx) */
	TxPD 			Wcb;

	TxCtrlNode 		TxNode;
	u32 			SentFlags;
	spinlock_t 		TxSpinLock  __ATTRIB_ALIGN__;
	
	struct sk_buff 		*CurrentTxSkb;
	spinlock_t 		CurrentTxLock  __ATTRIB_ALIGN__;

	/* Operation characteristics */
	u32 			LinkSpeed;
	u16 			CurrentPacketFilter;
	u32 			CurrentLookAhead;
	u32 			MediaConnectStatus;
	u16 			RegionCode;
	u16 			RegionTableIndex;
	u16 			TxPowerLevel;
	

	/* 
	 * 	POWER MANAGEMENT AND PnP SUPPORT 
	 */

	WLAN_DEVICE_POWER_STATE CurPowerState;
	BOOLEAN 		SurpriseRemoved;
	u16 			AtimWindow;

#ifdef PS_REQUIRED
	u16 			PSMode;	/* Wlan802_11PowerModeCAM=disable
					   Wlan802_11PowerModeMAX_PSP=enable */
	u16			MultipleDtim;
	u32 			PSState;
	BOOLEAN			NeedToWakeup;

	WLAN_DRV_TIMER		MrvDrvTxPktTimer  __ATTRIB_ALIGN__;
	BOOLEAN 		TxPktTimerIsSet;
	PS_CMD_ConfirmSleep	*PSConfirmSleep;
	u16			PreTbttTime;
#endif

#ifdef DEEP_SLEEP
	BOOLEAN			IsDeepSleep;
	BOOLEAN			IsDeepSleepRequired;
#endif

#ifdef CONFIG_MARVELL_PM
	BOOLEAN			PM_State;
	int			DeepSleep_State;
#endif

#ifdef HOST_WAKEUP
	BOOLEAN			bHostWakeupDevRequired;
#endif
		
#ifdef	DEBUG
	u32			SleepCounter;
	u32			ConfirmCounter;
	u32			ConsecutiveAwake;
	u32			NumConsecutiveAwakes;
#endif

	/* 
	 * 	ADVANCED ENCRYPTION SUPPORT 
	 */

	wlan_802_11_security_t  SecInfo;

	MRVL_WEP_KEY 		WepKey[4];
	u16 			CurrentWepKeyIndex; 

#ifdef WPA
	WLAN_802_11_ENCRYPTION_STATUS EncryptionStatus;
	BOOLEAN			IsGTK_SET;
	
	/* buffer to store data from OID_802_22_ASSOCIATION_INFORMATION */
	u8 		AssocInfoBuffer[MRVDRV_ASSOCIATE_INFO_BUFFER_SIZE];

	u8			Wpa_ie[40];
	u8			Wpa_ie_len;

	MRVL_WPA_KEY		WpaPwkKey, WpaGrpKey;

#ifdef WPA2
	HostCmd_DS_802_11_KEY_MATERIAL		aeskey;
#endif

	HostCmd_DS_802_11_PWK_KEY pwkkey;
#endif /* WPA */

	/* Advanced Encryption Standard */
#ifdef ADHOCAES
	BOOLEAN 		AdhocAESEnabled;
	wait_queue_head_t	cmd_EncKey __ATTRIB_ALIGN__;
#endif

	/*
	 *	RF 
	 */

	u16			RxAntennaMode;
	u16			TxAntennaMode;

	/* Requested Signal Strength Indicator */
	int			AvgPacketCount;
	u8			SNRNF[MAX_SNR][2];
	u16			SNR[MAX_TYPE_B][MAX_TYPE_AVG];
	u16			NF[MAX_TYPE_B][MAX_TYPE_AVG];
	u8			RSSI[MAX_TYPE_B][MAX_TYPE_AVG];
	u32			RxPDSNRAge;


	BOOLEAN			RadioOn;
	u32			Preamble;

	/* Multi Bands Support */
#ifdef MULTI_BANDS
	u8			SupportedRates[A_SUPPORTED_RATES];
#else
#ifdef G_RATE
	u8			SupportedRates[G_SUPPORTED_RATES];
#else
	u8			SupportedRates[B_SUPPORTED_RATES];
#endif
#endif

#ifdef MULTI_BANDS
	u16			is_multiband;	/* F/W supports multiband? */
	u16			fw_bands;	/* F/W supported bands */
	u16			config_bands;	/* User selected bands
						  		(a/b/bg/abg) */
	u16			adhoc_start_band;/* User selected bands (a/b/g) 
					     	       to start adhoc network */
#endif

	/* Blue Tooth Co-existence Arbitration */	
#ifdef BCA
	u16			bca_mode;
	u16			bca_antenna;
	u16			bca_btfreq;
	u32			bca_txpl32; /*Tx Priority Settings Low 32 bit*/
	u32			bca_txph32; /*Tx Priority Settings High 32 bit*/
	u32			bca_rxpl32; /*Rx Priority Settings Low 32 bit*/
	u32			bca_rxph32; /*Rx Priority Settings High 32 bit*/
#endif

	/* sleep_params */	
	SleepParams 		sp;
	
#ifdef AUTO_FREQ_CTRL
	afc_cmd			*freq_ctrl_info;
#endif

	/* RF calibration data */
#ifdef CAL_DATA
	HostCmd_DS_COMMAND	caldata;
	wait_queue_head_t	wait_cal_dataResponse;
#endif

	REGION_CHANNEL		region_channel[MAX_REGION_CHANNEL_NUM];
	REGION_CHANNEL		*cur_region_channel;

#ifdef ENABLE_802_11D
	wlan_802_11d_domain_reg_t DomainReg;	/* Domain Regulatory Data */
	wlan_802_11d_state_t	State11D; /* FSM variable for 11d support */
#endif

	/* 802.11H */
#ifdef ENABLE_802_11H_TPC
	wlan_802_11h_tpc_t	State11HTPC;
#endif

	/*
	 *	MISCELLANEOUS
	 */

#ifdef PASSTHROUGH_MODE
	wlan_passthrough_config	passthrough_config;
	u8			PassthroughCmdStatus;
	u8			passthrough_enabled;
	spinlock_t		PTDataLock;
	PTDataPacketList	*PTDataList;
	PTDataPacket		*PTDataPkt;
	struct list_head	PTDataQ;
	struct list_head	PTFreeDataQ;
	wait_queue_head_	wait_PTData;
#endif
	/* Card Information Structure */
	u8 			CisInfoBuf[512];
	u16 			CisInfoLen;

	u8			*pRdeeprom;
	wlan_offset_value	OffsetValue;

#ifdef linux
	wait_queue_head_t	cmd_get_log;
#endif
	
	HostCmd_DS_802_11_GET_LOG LogMsg;

} wlan_adapter;

#ifdef PS_REQUIRED	
typedef enum _WLAN_802_11_POWER_MODE
{
	Wlan802_11PowerModeCAM,
	Wlan802_11PowerModeMAX_PSP,
	Wlan802_11PowerModeFast_PSP,
	Wlan802_11PowerModeMax		// not a real mode, defined as 
					//an upper bound
} WLAN_802_11_POWER_MODE, *PWLAN_802_11_POWER_MODE;

typedef enum {
	PS_STATE_FULL_POWER,
	PS_STATE_AWAKE,
	PS_STATE_SLEEP
} PS_STATE;
#endif

/* Command or Data download status */
typedef enum {
	DNLD_RES_RECEIVED,
	DNLD_DATA_SENT,
	DNLD_CMD_SENT
} DNLD_STATE;

#define CHECK_TX_READY_TIME_OUT				10
//#define IX_STATUS_SUCCESS  				1

// for Adapter->Initialization Status flags
#define MRVDRV_INIT_STATUS_CMDTIMER_INITALIZED 		0x1
#define MRVDRV_INIT_STATUS_MAP_REGISTER_ALLOCATED 	0x2

#endif

