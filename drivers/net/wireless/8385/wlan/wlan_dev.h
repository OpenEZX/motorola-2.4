/******************* (c) Marvell Semiconductor, Inc. 2005 ********************
 * 	File:	wlan_dev.h
 * 	
 *      Purpose:
 *          This file contains definitions and data structures that are specific
 *          to Marvell 802.11 NIC. It contains the Device Information
 *          structure wlan_adapter.
 *****************************************************************************/

#ifndef _WLAN_DEV_H_
#define _WLAN_DEV_H_

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
	BOOLEAN	Unsupported;	/* TRUE:channel unsupported;  FLASE:supported*/
} CHANNEL_FREQ_POWER;

typedef	struct	_REGION_CHANNEL {
	BOOLEAN	Valid;		/* TRUE if this entry is valid		     */
	u8	Region;		/* Region code for US, Japan ...	     */
	u8	Band;		/* B, G, or A, used for BAND_CONFIG cmd	     */
	u8	NrCFP;		/* Actual No. of elements in the array below */
	CHANNEL_FREQ_POWER	*CFP;
} REGION_CHANNEL;


typedef struct  _wlan_802_11_security_t
{
	BOOLEAN				WPAEnabled;
#ifdef WPA2
	BOOLEAN				WPA2Enabled;
#endif // WPA2
	WLAN_802_11_WEP_STATUS 		WEPStatus;
	WLAN_802_11_AUTHENTICATION_MODE AuthenticationMode;
	WLAN_802_1X_AUTH_ALG 		Auth1XAlg;
	WLAN_802_11_ENCRYPTION_MODE 	EncryptionMode;
} wlan_802_11_security_t;

/* Current Basic Service Set State Structure */
typedef struct CurrentBSSParams
{
	WLAN_802_11_SSID 	ssid;
	WLAN_802_11_BSSID 	bssid;
	u8			band;
	u8			channel;
	int			NumOfRates;
	u8			DataRates[WLAN_SUPPORTED_RATES];
#ifdef WMM
	u8			wmm_enabled;
	u8			wmm_queue_prio[MAX_AC_QUEUES];
#endif /* WMM */
#ifdef WMM_UAPSD
	u8			wmm_uapsd_enabled;
#endif
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

/* sleep_period */	
typedef struct SleepPeriod
{
	u16 			period;
	u16 			reserved;
} SleepPeriod;

/**
 * Data structure for the Marvell WLAN device.
 * The comments in parenthesis are in relation to the user of this
 * structure.
 */
typedef struct _wlan_dev {
	char 			name[DEV_NAME_LEN];  /* device name (RO) */
	void			*card;  	/* card pointer (RO) */
	u32 			ioport;  	/* IO port (RO) */
	/* Remove: just initialized in if, but not used anywhwere ??? */
	u32  			upld_rcv;  	/* Upload received (RW) */
	u32  			upld_typ;  	/* Upload type (RO) */
	u32 			upld_len;  	/* Upload length (RO) */
	struct net_device	*netdev;  	/* Linux netdev (WO) */
	u8  			upld_buf[WLAN_UPLD_SIZE];  /* Upload buffer (RO) */
	u8			dnld_sent;	/* Download sent: bit0 1/0=
						data_sent/data_tx_done, bit1
						1/0=cmd_sent/cmd_tx_done, all
						other bits reserved 0 */
} wlan_dev_t, *pwlan_dev_t;

/* Data structure defined for the tx packet transmission */
typedef struct {
	struct semaphore sem;		/* semaphore */
	u8  tx_started;			/* tx started? */
	u32 tx_pktlen;			/* tx packet len  */
	u32 tx_pktcnt;			/* tx packet count */
	u8  tx_buf[MAX_WLAN_TX_SIZE];	/* tx buffer */
} WLAN_OBJECT, *PWLAN_OBJECT;

/**
 * Private structure for the MV device.
 * The comments in parenthesis are in relation to the user of this
 * structure.
 */

struct _wlan_private {
	int			open;

	wlan_adapter		*adapter;
	WLAN_OBJECT     	iface; 
	wlan_dev_t		wlan_dev;

	struct net_device_stats	stats;

#ifdef linux
	struct iw_statistics	wstats;
	struct proc_dir_entry	*proc_entry;
	struct proc_dir_entry	*proc_dev;
#endif

	/* thread to service interrupts */
	wlan_thread 		MainThread;

	/* thread to service mac events */
	wlan_thread 		ReassocThread;	
	
};

/////////////////////////////////////////////////////////////////
//
//			Adapter information
//
/////////////////////////////////////////////////////////////////

struct _wlan_adapter
{
	/* 
	 * 	STATUS variables
	 */

	WLAN_HARDWARE_STATUS 	HardwareStatus;
	u16 			FWStatus;
	u16 			MACStatus;
	u16 			RFStatus;
	u16 			CurrentChannel;
	u32 			FWReleaseNumber;
#ifdef FWVERSION3
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

	/* Variables brought in from private structure */
	int			irq;
	int			TxCount;

	/* Async and Sync Event variables */
	u32			IntCounter;
	u32			IntCounterSaved;	/* save int for DS/PS */
	u32			EventCause;
	u32			EventCounter;
	u8			nodeName[16]; 		/* for the nickname */
#ifdef SUBSCRIBE_EVENT_CTRL
	u16			GetEvents;
#endif

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
	wait_queue_head_t	ds_awake_q __ATTRIB_ALIGN__;

	u8			HisRegCpy;

/* Do we really need MANF code in production driver ??? If not remove
 * everything releated to MANF code */
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
	WLAN_802_11_BSSID 	CurrentBSSIDDescriptor;
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

	/* Capability Info used in Association, start, join */
	IEEEtypes_CapInfo_t	capInfo;

	/* Reassociation on and off */
	BOOLEAN 		Reassoc_on;
	BOOLEAN			ATIMEnabled;

	/* MAC address information */
	u8 			PermanentAddr[MRVDRV_ETH_ADDR_LEN];
	u8 			CurrentAddr[MRVDRV_ETH_ADDR_LEN];
	u8 			MulticastList[MRVDRV_MAX_MULTICAST_LIST_SIZE]
						[MRVDRV_ETH_ADDR_LEN];
	u32 			NumOfMulticastMACAddr;

	/* 802.3 Statistics */
	/* TODO: Enable this if the firmware supports */
	//HostCmd_DS_802_3_GET_STAT wlan802_3Stat;
	
	/* 802.11 statistics */
	HostCmd_DS_802_11_GET_STAT wlan802_11Stat;

	BOOLEAN 		AdHocCreated;
	BOOLEAN 		AdHocFailed;

	u16			EnableHwAuto;
	u16			RateBitmap;
	/* control G Rates */
#ifdef ADHOC_GRATE
	BOOLEAN 		adhoc_grate_enabled;
#endif

	WLAN_802_11_ANTENNA 	TxAntenna;
	WLAN_802_11_ANTENNA 	RxAntenna;
	
	u32 			Channel;
	u8 			AdhocChannel;
	
	WLAN_802_11_FRAGMENTATION_THRESHOLD FragThsd;
	WLAN_802_11_RTS_THRESHOLD RTSThsd;
	
	/* 0:auto, 1:1 Mbps, 2:2 Mbps, 3:5.5 Mbps,4:11 Mbps, 5:22 Mbps */
	u32 			DataRate; 
	BOOLEAN			Is_DataRate_Auto;

	u32			ExtendedScan;
	PEXTENDED_SCAN_PARAMS	pExtendedScanParams;

	/* number of association attempts for the current SSID cmd */
	u32 			m_NumAssociationAttemp;
	u16			ListenInterval; 
	u8			TxRetryCount;

	/* Tx-related variables (for single packet tx) */
	TxPD 			LocalTxPD;

	u32 			SentFlags;
	spinlock_t 		TxSpinLock  __ATTRIB_ALIGN__;
	struct sk_buff 		*CurrentTxSkb;
	struct sk_buff		RxSkbQ;
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

	BOOLEAN 		SurpriseRemoved;
	u16 			AtimWindow;

#ifdef PS_REQUIRED
	u16 			PSMode;	/* Wlan802_11PowerModeCAM=disable
					   Wlan802_11PowerModeMAX_PSP=enable */
	u16			MultipleDtim;
	u32 			PSState;
	BOOLEAN			NeedToWakeup;

	PS_CMD_ConfirmSleep	*PSConfirmSleep;
	u16			PreTbttTime;
	u16			LocalListenInterval;
	u16			NullPktInterval;
#ifdef FW_WAKEUP_METHOD
	u16			fwWakeupMethod;
#endif
#endif

#ifdef DEEP_SLEEP
	BOOLEAN			IsDeepSleep;
	BOOLEAN			IsDeepSleepRequired;
#endif

#ifdef HOST_WAKEUP
	BOOLEAN			bHostWakeupDevRequired;
	BOOLEAN			bHostWakeupConfigured;
	HostCmd_DS_802_11_HOST_WAKE_UP_CFG HostWakeupCfgParam;
	u32			WakeupTries;
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

#if defined(WPA) || defined(SAVE_ASSOC_RSP)
	/* buffer to store data from OID_802_22_ASSOCIATION_INFORMATION */
	u8 		AssocInfoBuffer[MRVDRV_ASSOCIATE_INFO_BUFFER_SIZE];
#endif

#if defined(IWGENIE_SUPPORT)
    u8 genIeBuffer[256];
    u8 genIeBufferLen;
#endif

#ifdef WPA
	WLAN_802_11_ENCRYPTION_STATUS EncryptionStatus;
	int			IsGTK_SET; /* TRUE (1), FALSE (0), or (TRUE + 1), meaning setting GTK */
	
	u8			Wpa_ie[256];
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

	u16			bcn_avg_factor;
	u16			data_avg_factor;
	int			AvgPacketCount;
	u8			SNRNF[MAX_SNR][2];
	u16			SNR[MAX_TYPE_B][MAX_TYPE_AVG];
	u16			NF[MAX_TYPE_B][MAX_TYPE_AVG];
	u8			RSSI[MAX_TYPE_B][MAX_TYPE_AVG];
#ifdef SLIDING_WIN_AVG
	u8			rawSNR[DEFAULT_DATA_AVG_FACTOR];
	u8		        rawNF[DEFAULT_DATA_AVG_FACTOR];
	u16			nextSNRNF;
	u16			numSNRNF;
#endif //SLIDING_WIN_AVG
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
	HostCmd_DS_BCA_CONFIG	bca;
	HostCmd_DS_802_11_BCA_TIMESHARE bca_ts;
#endif

	/* sleep_params */	
	SleepParams 		sp;

	/* sleep_period (Enhanced Power Save) */	
	SleepPeriod 		sleep_period;

	/* RF calibration data */
#ifdef CAL_DATA
	HostCmd_DS_COMMAND	caldata;
	wait_queue_head_t	wait_cal_dataResponse;
#endif

	REGION_CHANNEL		region_channel[MAX_REGION_CHANNEL_NUM];
	REGION_CHANNEL		*cur_region_channel;

#ifdef ENABLE_802_11D
	REGION_CHANNEL		universal_channel[MAX_REGION_CHANNEL_NUM];

	wlan_802_11d_domain_reg_t DomainReg;	/* Domain Regulatory Data */
	parsed_region_chan_11d_t  parsed_region_chan;
	
	wlan_802_11d_state_t	State11D; /* FSM variable for 11d support */
#endif

	/* 802.11H */
#ifdef ENABLE_802_11H_TPC
	wlan_802_11h_tpc_t	State11HTPC;
#endif

#ifdef REASSOCIATION_SUPPORT
    int reassocAttempt;
    WLAN_802_11_MAC_ADDRESS reassocCurrentAp;
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
	wait_queue_head_t	wait_PTData;
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
  u16 ScanProbes;

	u32			PktTxCtrl; // adds for TxControl ioctl cmd

#ifdef READ_HELPER_FW
	u8 	*helper;
	u32   	helper_len;
	u8 	*fmimage;
	u32   	fmimage_len;
#endif

};

#endif /* _WLAN_DEV_H_ */

