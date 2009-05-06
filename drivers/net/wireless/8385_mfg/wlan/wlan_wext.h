/*
	File	: wlan_wext.h
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

#define WLANSCAN_TYPE			(WLANIOCTL + 11)

#ifdef PASSTHROUGH_MODE
#define WLANPASSTHROUGH			(WLANIOCTL + 13)
#define	WLANPTDATA_RCVE			(WLANIOCTL + 14)
#endif

#define WLAN_SETNONE_GETONEINT		(WLANIOCTL + 15)
#define WLANGETREGION				1
#define WLAN_GET_LISTEN_INTERVAL		2
#define WLAN_GET_MULTIPLE_DTIM			3
#ifdef V4_CMDS
#define	WLAN_GET_INACTIVITY_TIMEOUT		4
#endif

#define WLAN_SETTENCHAR_GETNONE		(WLANIOCTL + 16)
#ifdef MULTI_BANDS
#define WLAN_SET_BAND				1
#define WLAN_SET_ADHOC_CH			2
#endif
#define	WLAN_SET_SLEEP_PERIOD			3

#define WLAN_SETNONE_GETTENCHAR		(WLANIOCTL + 17)
#ifdef MULTI_BANDS
#define WLAN_GET_BAND				1
#define WLAN_GET_ADHOC_CH			2
#endif
#define	WLAN_GET_SLEEP_PERIOD			3

#define WLANREGCFRDWR			(WLANIOCTL + 18)

#define WLAN_SETNONE_GETTWELVE_CHAR	(WLANIOCTL + 19)
#define WLAN_SUBCMD_GETRXANTENNA		1
#define WLAN_SUBCMD_GETTXANTENNA		2

#define WLAN_SETWORDCHAR_GETNONE	(WLANIOCTL + 20)
#define WLANSETADHOCAES				1	
#define WLANVERSION				2	
#ifdef AUTO_FREQ_CTRL
#define WLAN_AUTO_FREQ_SET			3
#endif

#define WLAN_SETNONE_GETWORDCHAR	(WLANIOCTL + 21)
#define WLANGETADHOCAES				1	
#ifdef AUTO_FREQ_CTRL
#define WLAN_AUTO_FREQ_GET			2
#endif

/* Sub commands for RSSI and NF releated stuffs 
 * With this we got holes for 23,24 and 25
 */
#define WLAN_SETONEINT_GETONEINT	(WLANIOCTL + 23)
#define WLANSNR					1 
#define WLANNF					2
#define WLANRSSI				3

#ifdef THROUGHPUT_TEST
#define WLANTHRUPUT				4
#endif /* THROUGHTPUT_TEST */

#ifdef BG_SCAN
#define WLANBGSCAN				5
#endif /* BG_SCAN */

#ifdef ENABLE_802_11D
/*Private com id for enable/disable 11d*/
#define WLANENABLE11D				5	
#define WLANEEPROMREGION11D			6
#endif

#ifdef ENABLE_802_11H_TPC
/*Private cmd enable11htpc*/
#define WLANENABLE11HTPC			7
#define WLANSETLOCALPOWER			8	
#endif

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
#ifdef V4_CMDS
#define	WLAN_SET_INACTIVITY_TIMEOUT		12
#endif

#define WLAN_SET64CHAR_GET64CHAR	(WLANIOCTL + 25)	
#ifdef BCA
#define WLANBCA 				1
#endif
#define WLANSLEEPPARAMS 			2

#ifdef ENABLE_802_11H_TPC
#define WLANREQUESTTPC				3
#endif

#ifdef BCA
#define	WLAN_SET_BCA_TIMESHARE			4
#define	WLAN_GET_BCA_TIMESHARE			5
#endif

#ifdef ENABLE_802_11H_TPC
#define WLANSETPOWERCAP				6	
#endif
#define WLANSCAN_MODE			        7 	
#define	WLANSCAN_BSSID	 			8	

#define WLANEXTSCAN			(WLANIOCTL + 26)
#ifdef DEEP_SLEEP
#define WLANDEEPSLEEP			(WLANIOCTL + 27)
#define DEEP_SLEEP_ENABLE			1
#define DEEP_SLEEP_DISABLE  			0	
#endif

#define WLAN_RFI			(WLANIOCTL + 28)
#define WLAN_TX_MODE				1
#define WLAN_RX_MODE				2

#define WLANCMD52RDWR			(WLANIOCTL + 30)
#define WLANCMD53RDWR			(WLANIOCTL + 31)
#define CMD53BUFLEN	32

#define WLANGENATIM 			(WLANIOCTL + 32)

#define	REG_MAC		0x19
#define	REG_BBP		0x1a
#define	REG_RF		0x1b
#define	REG_EEPROM	0x59

#define	CMD_DISABLED	0
#define	CMD_ENABLED	1
#define	CMD_GET		2

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
} __attribute__((packed)) wlan_ioctl_cal_data,
	*pwlan_ioctl_cal_data;

typedef struct _wlan_ioctl_bg_scan_config  {
	unsigned char	Type;
	unsigned short  Size;
	unsigned short	Action;
	unsigned char  Reserved;
	unsigned char	BssType;		/* Infrastructure / IBSS */
	unsigned char	ChannelIsPerScan;	/* No of channels to scan at
						   one scan */
	unsigned char	DiscardWhenFull;	/* 0 - Discard old scan results
						   1 - Discard new scan 
						   	results */
	unsigned short	ScanInternal;		/* Interval b/w consecutive 
						   scan */
	unsigned int	StoreCondition;		/* - SSID Match
					   	   - Exceed RSSI threshold
						   - SSID Match & Exceed RSSI
						     Threshold 
						   - Always */	   
	unsigned int	ReportConditions;	/* - SSID Match
						   - Exceed RSSI threshold
						   - SSID Match & Exceed RSSI
						     Threshold
						   - Exceed MaxScanResults
						   - Entire channel list 
						     scanned once 
						   - Domain Mismatch in 
						     country IE */
	unsigned short	MaxScanResults; 	/* Max scan results that will
						   trigger a scn completion
						   event */
	unsigned char 	ChannelList[4];		/* List Channel of in TLV scan
						   format */
} __attribute__((packed)) wlan_ioctl_bg_scan_config,
	*pwlan_ioctl_bg_scan_config;


#endif /* _WLAN_WEXT_H_ */

