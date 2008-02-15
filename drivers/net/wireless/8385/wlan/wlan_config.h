/*
 * 	File	: wlan_config.h
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

