/**
 * 	File	: wlan_config.h
 */

#ifndef _WLANCONFIG_H_
#define _WLANCONFIG_H_

#ifdef EXTSCAN

#define NULLBSSID		"\x00\x00\x00\x00\x00\x00"
#define HWA_ARG(x)		*(((u8 *)x + 0)), *(((u8 *)x + 1)), \
				*(((u8 *)x + 2)), *(((u8 *)x + 3)), \
				*(((u8 *)x + 4)), *(((u8 *)x + 5))

#define WCON_ENC_DISABLED	0
#define WCON_ENC_ENABLED	1	

#define WCON_WPA_DISABLED	0
#define WCON_WPA_ENABLED	1	

#define WCON_WMM_DISABLED	0
#define WCON_WMM_ENABLED	1	

typedef struct _WCON_SSID {
	unsigned int	ssid_len;
	unsigned char	ssid[IW_ESSID_MAX_SIZE + 1];
} WCON_SSID;

typedef unsigned char WCON_BSSID[ETH_ALEN];

typedef struct _WCON_NET_INFO {
	WCON_SSID	Ssid;
	WCON_BSSID	Bssid;
	unsigned int	Rssi;
	int		NetMode;
	int		Privacy;
	int		WpaAP;
	int		Wmm;
} WCON_NET_INFO;

typedef struct _WCON_HANDLE {
	WCON_NET_INFO	ScanList[IW_MAX_AP];
	int		ApNum; 
} WCON_HANDLE;

#endif
#endif /* _WLANCONFIG_H_ */

