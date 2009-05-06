/**
 * File : wlan_wext.h
 *
 * Motorola Confidential Proprietary
 * Copyright (C) Motorola 2005. All rights reserved.
 */

#ifndef	_WLAN_WEXT_H_
#define	_WLAN_WEXT_H_

#define	WLANIOCTL			SIOCIWFIRSTPRIV

#define WLANSETWPAIE			(WLANIOCTL + 0)  
#define WLANCISDUMP 			(WLANIOCTL + 1)
#define	WLANMANFCMD			(WLANIOCTL + 2)
#define	WLANREGRDWR			(WLANIOCTL + 3)
#define MAX_EEPROM_DATA	256
#define	WLANSTDCMD			(WLANIOCTL + 4)

#ifdef DTIM_PERIOD
#define	WLANSETDTIM			(WLANIOCTL + 5)
#endif

#ifdef HOST_WAKEUP
#define WLANHOSTWAKEUPCFG		(WLANIOCTL + 6)
#endif

#define WLAN_SETNONE_GETBYTE		(WLANIOCTL + 7)
#define WLANREASSOCIATIONAUTO			1
#define WLANREASSOCIATIONUSER			2
#define WLANWLANIDLEON				3
#define WLANWLANIDLEOFF				4


#define WLAN_SETNONE_GETNONE 		(WLANIOCTL + 8)
#define WLANDEAUTH				1
#define WLANRADIOON				2
#define WLANRADIOOFF				3
#define WLANREMOVEADHOCAES			4
#define WLANADHOCSTOP				5
#define WLANCIPHERTEST      6

#define WLANGETLOG			(WLANIOCTL + 9)

#define WLAN_SETCONF_GETCONF		(WLANIOCTL + 10)
#ifdef CAL_DATA
#define	CAL_DATA_CONFIG				0	
#define WLANGET_CAL_DATA			0
#define WLANSET_CAL_DATA			1
#endif

#ifdef BG_SCAN
#define BG_SCAN_CONFIG				1
#endif /* BG_SCAN */

#ifdef SUBSCRIBE_EVENT_CTRL
#define SUBSCRIBE_EVENT				6
#endif

#ifdef WMM
#define WMM_TSPEC				2
#define WMM_ACK_POLICY				3
#define WMM_PARA_IE				4
#define WMM_ACK_POLICY_PRIO			4
#endif /* WMM */

#ifdef CAL_DATA
#define	CAL_DATA_EXT_CONFIG			5		
#define WLANGET_CAL_DATA_EXT			0
#define WLANSET_CAL_DATA_EXT			1
#endif
#define WLANSCAN_TYPE			(WLANIOCTL + 11)

#ifdef PASSTHROUGH_MODE
#define WLANPASSTHROUGH			(WLANIOCTL + 13)
#define	WLANPTDATA_RCVE			(WLANIOCTL + 14)
#endif

#define WLAN_SETNONE_GETONEINT		(WLANIOCTL + 15)
#define WLANGETREGION				1
#define WLAN_GET_LISTEN_INTERVAL		2
#define WLAN_GET_MULTIPLE_DTIM			3
#define	WLANGETBCNAVG				5

#define WLAN_SETTENCHAR_GETNONE		(WLANIOCTL + 16)
#ifdef MULTI_BANDS
#define WLAN_SET_BAND				1
#define WLAN_SET_ADHOC_CH			2
#endif

#define WLAN_SETNONE_GETTENCHAR		(WLANIOCTL + 17)
#ifdef MULTI_BANDS
#define WLAN_GET_BAND				1
#define WLAN_GET_ADHOC_CH			2
#endif

#define WLANREGCFRDWR			(WLANIOCTL + 18)

#define WLAN_SETNONE_GETTWELVE_CHAR	(WLANIOCTL + 19)
#define WLAN_SUBCMD_GETRXANTENNA		1
#define WLAN_SUBCMD_GETTXANTENNA		2

#define WLAN_SETWORDCHAR_GETNONE	(WLANIOCTL + 20)
#define WLANSETADHOCAES				1	

#define WLAN_SETNONE_GETWORDCHAR	(WLANIOCTL + 21)
#define WLANGETADHOCAES				1	
#define WLANVERSION				2	

/* Sub commands for RSSI and NF releated stuffs 
 * With this we got holes for 23,24 and 25
 */
#define WLAN_SETONEINT_GETONEINT	(WLANIOCTL + 23)
#define WLANNF					1
#define WLANRSSI				2

#ifdef THROUGHPUT_TEST
#define WLANTHRUPUT				4
#endif /* THROUGHTPUT_TEST */

#ifdef BG_SCAN
#define WLANBGSCAN				5
#endif /* BG_SCAN */

#ifdef ENABLE_802_11D
/*Private com id for enable/disable 11d*/
#define WLANENABLE11D				6
#endif

#ifdef ADHOC_GRATE
#define WLANADHOCGRATE				7
#endif

#ifdef ENABLE_802_11H_TPC
/*Private cmd enable11htpc*/
#define WLANENABLE11HTPC			8
#define WLANSETLOCALPOWER			9	
#endif

#ifdef BULVERDE_SDIO
#define WLANSDIOCLOCK				10
#endif

#ifdef WMM
#define WLANWMM_ENABLE				11
#endif /* WMM */

#ifdef WMM_UAPSD
#define WLAN_WMM_QOSINFO			12
#endif /* WMM_UAPSD */

#ifdef PS_REQUIRED
#define	WLAN_LISTENINTRVL			13
#ifdef FW_WAKEUP_METHOD
#define WLAN_FW_WAKEUP_METHOD			15
#define WAKEUP_FW_UNCHANGED				0
#define WAKEUP_FW_THRU_INTERFACE			1
#define WAKEUP_FW_THRU_GPIO				2
#endif
#endif

/* adds for iwpriv txcontrol ioctl */
#define WLAN_TXCONTROL				16

#ifdef ATIMGEN
#define WLANATIMGEN				17
#endif

#ifdef WMM_UAPSD
#define WLANNULLGEN				18
#endif

#define WLAN_NULLPKTINTERVAL			19

#ifdef WPRM_DRV
#define WLANMOTOTM				30
#endif /* WPRM_DRV */

#define WLAN_SETONEINT_GETNONE		(WLANIOCTL + 24)
#define WLAN_SUBCMD_SETRXANTENNA		1
#define WLAN_SUBCMD_SETTXANTENNA		2
#ifdef PS_REQUIRED
#define WLANSETPRETBTT				3
#endif
#define WLANSETAUTHALG				5
#define WLANSET8021XAUTHALG			6
#define WLANSETENCRYPTIONMODE			7
#define WLANSETREGION				8
#define WLAN_SET_LISTEN_INTERVAL		9

#ifdef PS_REQUIRED
#define WLAN_SET_MULTIPLE_DTIM			10
#endif
#define WLAN_SET_ATIM_WINDOW			11
#define WLANSETBCNAVG				13
#define WLANSETDATAAVG				14

#define WLAN_SET64CHAR_GET64CHAR	(WLANIOCTL + 25)	
#ifdef BCA
#define WLANBCA 				1
#endif
#define WLANSLEEPPARAMS 			2

#ifdef ENABLE_802_11H_TPC
#define WLANREQUESTTPC				3
#endif

#ifdef BCA
#define	WLAN_BCA_TIMESHARE			4
#endif

#ifdef ENABLE_802_11H_TPC
#define WLANSETPOWERCAP				5	
#endif
#define WLANSCAN_MODE			        6 	
#define	WLANSCAN_BSSID	 			7	
#define WLAN_GET_RATE				8
#define WLAN_GET_ADHOC_STATUS			9

#if defined(IWGENIE_SUPPORT) && (WIRELESS_EXT < 18)
#define WLAN_SET_GEN_IE                 10
#define WLAN_GET_GEN_IE                 11
#endif
#if defined(REASSOCIATION_SUPPORT)
#define WLAN_REASSOCIATE                12
#endif

#define WLANEXTSCAN			(WLANIOCTL + 26)
#ifdef DEEP_SLEEP
#define WLANDEEPSLEEP			(WLANIOCTL + 27)
#define DEEP_SLEEP_ENABLE			1
#define DEEP_SLEEP_DISABLE  			0	
#endif

#define WLAN_RFI			(WLANIOCTL + 28)
#define WLAN_TX_MODE				1
#define WLAN_TX_CONTROL_MODE			2

#define WLAN_SET_GET_SIXTEEN_INT       (WLANIOCTL + 29)
#define WLAN_TPCCFG                             1
#define WLAN_POWERCFG                           2

#ifdef WPRM_DRV
#define WLAN_SESSIONTYPE                        11
#endif

#ifdef AUTO_FREQ_CTRL
#define WLAN_AUTO_FREQ_SET			3
#define WLAN_AUTO_FREQ_GET			4
#endif
#ifdef LED_GPIO_CTRL	
#define WLAN_LED_GPIO_CTRL			5
#endif
#define WLAN_SCANPROBES 			6
#define WLAN_SLEEP_PERIOD			7
#define	WLAN_ADAPT_RATESET			8
#define	WLAN_INACTIVITY_TIMEOUT			9
#define WLANSNR					10

#define WLANCMD52RDWR			(WLANIOCTL + 30)
#define WLANCMD53RDWR			(WLANIOCTL + 31)
#define CMD53BUFLEN	32

/*#define WLANGENATIM 			(WLANIOCTL + 32)*/

#define	REG_MAC		0x19
#define	REG_BBP		0x1a
#define	REG_RF		0x1b
#define	REG_EEPROM	0x59

#define	CMD_DISABLED	0
#define	CMD_ENABLED	1
#define	CMD_GET		2
#define SKIP_CMDNUM	4
#define SKIP_TYPE	1
#define SKIP_SIZE	2
#define SKIP_ACTION	2
#define SKIP_TYPE_SIZE		(SKIP_TYPE + SKIP_SIZE)
#define SKIP_TYPE_ACTION	(SKIP_TYPE + SKIP_ACTION)

typedef struct wlan_ioctl {
	unsigned short	command;	// what to do
	unsigned short	len;		// length of data
	unsigned char	*data;		// the data
} wlan_ioctl;

typedef struct _wlan_ioctl_rfantenna {
	unsigned short Action;
	unsigned short AntennaMode;
} wlan_ioctl_rfantenna;

typedef struct _wlan_ioctl_regrdwr {
	unsigned short	WhichReg;	/* Which register to access */
	unsigned short	Action;		/* Read or Write	*/
	unsigned long	Offset;
	unsigned short	NOB;
	unsigned long	Value;
} wlan_ioctl_regrdwr;

typedef struct _wlan_ioctl_cfregrdwr {
	unsigned char	Action;		/* Read or Write	*/
	unsigned short	Offset;
	unsigned short	Value;
} wlan_ioctl_cfregrdwr;

typedef struct _wlan_ioctl_rdeeprom {
	unsigned short	WhichReg;	/* Which register to access */
	unsigned short	Action;		/* Read or Write	*/
	unsigned short	Offset;
	unsigned short	NOB;
	unsigned char	Value;
} wlan_ioctl_rdeeprom;

typedef struct _wlan_ioctl_adhoc_key_info {
	unsigned short	action;
	unsigned char	key[16];
	unsigned char 	tkiptxmickey[16];
	unsigned char	tkiprxmickey[16];
}wlan_ioctl_adhoc_key_info;			

#ifdef __KERNEL__
extern  struct iw_handler_def wlan_handler_def;
struct iw_statistics *wlan_get_wireless_stats(struct net_device *dev);
int wlan_do_ioctl(struct net_device *dev, struct ifreq *req, int i);
#endif

typedef struct _wlan_ioctl_cal_data {
	unsigned short  Type;
	unsigned short	Command;
	unsigned short	Size;
	unsigned short	SeqNum;
	unsigned short	Result;
	unsigned short	Action;
	unsigned char	Reserved1[9];
	unsigned char	PAOption;		/* Power Amplifier calibration 
						   options */
	unsigned char	ExtPA;			/* type of external Power 
						   Amplifier */
	unsigned char	Ant;			/* Antenna selection */
	unsigned short	IntPA[14];		/* channel calibration */
	unsigned char	PAConfig[4];		/* RF register calibration */
	unsigned char	Reserved2[4];
	unsigned short	Domain;			/* Regulatory Domain */
	unsigned char	ECO;			/* ECO present or not */
	unsigned char	LCT_cal;		/* VGA capacitor calibration */
	unsigned char	Reserved3[12];		
	unsigned char	MacAddr[6];		/* This is an extra field 
						   required by the application 
						   to send the MAC address to 
						   the driver, which has to be 
						   set after sending the cal 
						   data */
} __ATTRIB_PACK__ wlan_ioctl_cal_data,
	*pwlan_ioctl_cal_data;

#ifdef SUBSCRIBE_EVENT_CTRL
typedef struct _EventSubscribe {
	unsigned short	Action;
	unsigned short	Events;
	unsigned char	RSSIValue;
	unsigned char	RSSIFreq;
	unsigned char	SNRValue;
	unsigned char	SNRFreq;
	unsigned char	FailValue;
	unsigned char	FailFreq;
	unsigned char	BcnMissed;
} EventSubscribe;
#endif

#ifdef WMM
struct _wlan_ioctl_wmm_para_ie {
	unsigned char 	Type;
	unsigned short	Action;
	unsigned char	Para_IE[26];		/* WMM Parameter IE */
} __ATTRIB_PACK__;

struct _wlan_ioctl_wmm_tspec {
	unsigned char 	Type;
	unsigned short	Action;			/* 0 - Get TSPEC
						   1 - Set TSPEC
						   3 - Remove TSPEC */
	unsigned char	UserPriority; 		/* 0 - TSPEC for AC_BE
						   3 - TSPEC for AC_BE
						   1 - TSPEC for AC_BK
						   2 - TSPEC for AC_BK
						   4 - TSPEC for AC_VI
						   5 - TSPEC for AC_VI
						   6 - TSPEC for AC_VO
						   7 - TSPEC for AC_VO */
	unsigned char	PSMethod;		/* 0 - WMM_PS_LEGACY
						   1 - WMM_PS_TRIGGERED */
	unsigned char	FixedSizeMSDU;		/* 0 - Size of MSDU not fixed
						   1 - Size of MSDU is fixed */ 
	unsigned short	MSDUSize;		/* Nominal MSDU size in bytes */
	unsigned int	MeanDataRate;		/* Average data rate in 
						   bits/sec */
	unsigned int	MinPhyRate;		/* Minimum PHY rate in 
						   bits/sec */
	unsigned short	ExtraBandwidth;		/* Extra bandwidth factor to
						   account for retries */
} __ATTRIB_PACK__;

struct _wlan_ioctl_wmm_ack_policy {
	unsigned char	Type;
	unsigned short	Action; 		/* 0 - ACT_GET
						   1 - ACT_SET */
	unsigned char	AC;	 		/* 0 - AC_BE
						   1 - AC_BK
						   2 - AC_VI
						   3 - AC_VO */
	unsigned char	AckPolicy;		/* 0 - WMM_ACK_POLICY_IMM_ACK
						   1 - WMM_ACK_POLICY_NO_ACK */
} __ATTRIB_PACK__;

#endif /* WMM */

/* sleep_params */	
typedef struct _wlan_ioctl_sleep_params_config  {
	unsigned short		Action;
	unsigned short 		Error;
	unsigned short 		Offset;
	unsigned short 		StableTime;
	unsigned char		CalControl;	
	unsigned char		ExtSleepClk;	
	unsigned short		Reserved; /* for hardware debugging */
} __ATTRIB_PACK__ wlan_ioctl_sleep_params_config,
	*pwlan_ioctl_sleep_params_config;

/* BCA params */	
typedef struct _wlan_ioctl_bca_params_config {
	unsigned short		Action;
	unsigned short		Mode;
	unsigned short		Antenna;
	unsigned short		BtFreq;
	unsigned int		Txpl32; /* Tx Priority Settings Low 32 bit */
	unsigned int		Txph32; /* Tx Priority Settings High 32 bit */
	unsigned int		Rxpl32; /* Rx Priority Settings Low 32 bit */
	unsigned int		Rxph32; /* Rx Priority Settings High 32 bit */
} __ATTRIB_PACK__ wlan_ioctl_bca_params_config,
	*pwlan_ioctl_bca_params_config;
    
/* BCA TIME SHARE */
typedef struct _wlan_ioctl_bca_timeshare_config {
	unsigned short	 	Action;		/* ACT_GET/ACT_SET */	
	unsigned short	 	TrafficType; 	/* Type: WLAN, BT */
	unsigned int 	 	TimeShareInterval; /* 20msec - 60000msec */
	unsigned int	 	BTTime; 	/* PTA arbiter time in msec */	
} __ATTRIB_PACK__ wlan_ioctl_bca_timeshare_config,
	*pwlan_ioctl_bca_timeshare_config;


typedef struct
{
    unsigned char CurrentBSSID[6];
    unsigned char DesiredBSSID[6];
    char DesiredSSID[IW_ESSID_MAX_SIZE + 1];

} __ATTRIB_PACK__ wlan_ioctl_reassociation_info;

#endif /* _WLAN_WEXT_H_ */
