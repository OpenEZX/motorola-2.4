/*
 *	File	: wlan_fw.h
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

#ifndef	_WLAN_FW_H_
#define	_WLAN_FW_H_

#ifndef DEV_NAME_LEN
#define DEV_NAME_LEN			32
#endif

#define MAXKEYLEN			13

/* The number of times to try when waiting for downloaded firmware to */
/* become active. (polling the scratch register). */

#define MAX_FIRMWARE_POLL_TRIES		100

#define FIRMWARE_TRANSFER_BLOCK_SIZE	1536 /* bytes */

int wlan_init_fw(wlan_private *priv);
int wlan_disable_host_int(wlan_private *priv,u8 reg);
int wlan_enable_host_int(wlan_private *priv, u8 mask);
int wlan_free_cmd_buffers(wlan_private *priv);
int wlan_is_tx_download_ready(wlan_private * priv);

#if 1	/* New Implementation of finding SSID and BSSID */

/*
	Compare two SSID's
	Returns: non-zero value if mismatch 0 on match
*/

static inline int SSIDcmp(WLAN_802_11_SSID *ssid1, WLAN_802_11_SSID *ssid2)
{
	if (!ssid1 || !ssid2)
		return -1;

	if (ssid1->SsidLength != ssid2->SsidLength)
		return -1;

	return memcmp(ssid1->Ssid, ssid2->Ssid, ssid1->SsidLength);
}

/*
 *    WEP      WPA      WPA2    ad-hoc  encrypt                       Network
 *  enabled  enabled  enabled    AES     mode    Privacy  WPA  WPA2  Compatible
 *     0        0        0        0      NONE       0      0    0	yes	No security
 *     1        0        0        0      NONE       1      0    0	yes	Static WEP
 *     0        1        0        0       x         1x     1    x	yes	WPA
 *     0        0        1        0       x         1x     x    1	yes	WPA2
 *     0        0        0        1      NONE       1      0    0	yes	Ad-hoc AES
 *     0        0        0        0     !=NONE      1      0    0	yes	Dynamic WEP
 */
static inline int IsNetworkCompatible(wlan_adapter *Adapter, 
						int index, int mode)
{
	ENTER();
	
#ifdef WMM
	if(Adapter->wmm.required
		&& Adapter->BSSIDList[index].Wmm_IE[0] == WMM_IE) {
//		&& Adapter->BSSIDList[index].Wmm_IE[5] == WMM_OUI_TYPE) { 
		Adapter->wmm.enabled = 1; 
	} else {
		Adapter->wmm.enabled = 0; 
	}
#endif /* WMM */

	if (Adapter->BSSIDList[index].InfrastructureMode == mode) {
		if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPDisabled
#ifdef WPA
			&& !Adapter->SecInfo.WPAEnabled
#ifdef WPA2
			&& !Adapter->SecInfo.WPA2Enabled
#endif	//WPA2
			&& Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0] != WPA_IE
#ifdef WPA2
			&& Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0] != WPA2_IE
#endif	//WPA2
#endif	//WPA
#ifdef ADHOCAES
			&& !Adapter->AdhocAESEnabled
#endif
			&& Adapter->SecInfo.EncryptionMode == CIPHER_NONE
			&& !Adapter->BSSIDList[index].Privacy
		) {
			/* no security */
			LEAVE();
			return index;
		}
		else if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled
#ifdef WPA
			&& !Adapter->SecInfo.WPAEnabled
#ifdef WPA2
			&& !Adapter->SecInfo.WPA2Enabled
#endif	//WPA2
//			&& Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0] != WPA_IE
#ifdef WPA2
//			&& Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0] != WPA2_IE
#endif	//WPA2
#endif	//WPA
#ifdef ADHOCAES
			&& !Adapter->AdhocAESEnabled
#endif
//			&& Adapter->SecInfo.EncryptionMode == CIPHER_NONE
			&& Adapter->BSSIDList[index].Privacy
		) {
			/* static WEP enabled */
			LEAVE();
			return index;
		}
#ifdef WPA
		else if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPDisabled
			&& Adapter->SecInfo.WPAEnabled
#ifdef WPA2
			&& !Adapter->SecInfo.WPA2Enabled
#endif	//WPA2
			&& Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0] == WPA_IE
#ifdef ADHOCAES
			&& !Adapter->AdhocAESEnabled
#endif
			// Privacy bit may NOT be set in some APs like LinkSys WRT54G
			//&& Adapter->BSSIDList[index].Privacy
		) {
			/* WPA enabled */
#ifdef WPA2
PRINTK("IsNetworkCompatible() WPA: index=%d wpa_ie=%#x wpa2_ie=%#x WEP=%s WPA=%s WPA2=%s EncMode=%#x privacy=%#x\n",index,Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0],Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0],(Adapter->SecInfo.WEPStatus==Wlan802_11WEPEnabled)?"e":"d",(Adapter->SecInfo.WPAEnabled)?"e":"d",(Adapter->SecInfo.WPA2Enabled)?"e":"d",Adapter->SecInfo.EncryptionMode,Adapter->BSSIDList[index].Privacy);
#endif
			LEAVE();
			return index;
		}
#ifdef WPA2
		else if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPDisabled
			&& !Adapter->SecInfo.WPAEnabled
			&& Adapter->SecInfo.WPA2Enabled
			&& Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0] == WPA2_IE
#ifdef ADHOCAES
			&& !Adapter->AdhocAESEnabled
#endif
			// Privacy bit may NOT be set in some APs like LinkSys WRT54G
			//&& Adapter->BSSIDList[index].Privacy
		) {
			/* WPA2 enabled */
PRINTK("IsNetworkCompatible() WPA2: index=%d wpa_ie=%#x wpa2_ie=%#x WEP=%s WPA=%s WPA2=%s EncMode=%#x privacy=%#x\n",index,Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0],Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0],(Adapter->SecInfo.WEPStatus==Wlan802_11WEPEnabled)?"e":"d",(Adapter->SecInfo.WPAEnabled)?"e":"d",(Adapter->SecInfo.WPA2Enabled)?"e":"d",Adapter->SecInfo.EncryptionMode,Adapter->BSSIDList[index].Privacy);
			LEAVE();
			return index;
		}
#endif	//WPA2
#endif	//WPA
#ifdef ADHOCAES
		else if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPDisabled
#ifdef WPA
			&& !Adapter->SecInfo.WPAEnabled
#ifdef WPA2
			&& !Adapter->SecInfo.WPA2Enabled
#endif	//WPA2
			&& Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0] != WPA_IE
#ifdef WPA2
			&& Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0] != WPA2_IE
#endif	//WPA2
#endif	//WPA
			&& Adapter->AdhocAESEnabled
			&& Adapter->SecInfo.EncryptionMode == CIPHER_NONE
			&& Adapter->BSSIDList[index].Privacy
		) {
			/* Ad-hoc AES enabled */
			LEAVE();
			return index;
		}
#endif
		else if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPDisabled
#ifdef WPA
			&& !Adapter->SecInfo.WPAEnabled
#ifdef WPA2
			&& !Adapter->SecInfo.WPA2Enabled
#endif	//WPA2
			&& Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0] != WPA_IE
#ifdef WPA2
			&& Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0] != WPA2_IE
#endif	//WPA2
#endif	//WPA
#ifdef ADHOCAES
			&& !Adapter->AdhocAESEnabled
#endif
			&& Adapter->SecInfo.EncryptionMode != CIPHER_NONE
			&& Adapter->BSSIDList[index].Privacy
		) {
			/* dynamic WEP enabled */
#ifdef WPA
#ifdef WPA2
PRINTK("IsNetworkCompatible() dynamic WEP: index=%d wpa_ie=%#x wpa2_ie=%#x EncMode=%#x privacy=%#x\n",index,Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0],Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0],Adapter->SecInfo.EncryptionMode,Adapter->BSSIDList[index].Privacy);
#else
PRINTK("IsNetworkCompatible() dynamic WEP: index=%d wpa_ie=%#x EncMode=%#x privacy=%#x\n",index,Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0],Adapter->SecInfo.EncryptionMode,Adapter->BSSIDList[index].Privacy);
#endif
#endif
			LEAVE();
			return index;
		}

		/* security doesn't match */
#ifdef WPA
#ifdef WPA2
PRINTK("IsNetworkCompatible() FAILED: index=%d wpa_ie=%#x wpa2_ie=%#x WEP=%s WPA=%s WPA2=%s EncMode=%#x privacy=%#x\n",index,Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0],Adapter->BSSIDList[index].wpa2_supplicant.Wpa_ie[0],(Adapter->SecInfo.WEPStatus==Wlan802_11WEPEnabled)?"e":"d",(Adapter->SecInfo.WPAEnabled)?"e":"d",(Adapter->SecInfo.WPA2Enabled)?"e":"d",Adapter->SecInfo.EncryptionMode,Adapter->BSSIDList[index].Privacy);
#else
PRINTK("IsNetworkCompatible() FAILED: index=%d wpa_ie=%#x WEP=%s WPA=%s EncMode=%#x privacy=%#x\n",index,Adapter->BSSIDList[index].wpa_supplicant.Wpa_ie[0],(Adapter->SecInfo.WEPStatus==Wlan802_11WEPEnabled)?"e":"d",(Adapter->SecInfo.WPAEnabled)?"e":"d",Adapter->SecInfo.EncryptionMode,Adapter->BSSIDList[index].Privacy);
#endif
#endif
		LEAVE();
		return -ECONNREFUSED;
	}

	/* mode doesn't match */
	LEAVE();
	return -ENETUNREACH;
}

/*
	Find an SSID in the Adapter->BSSIDList
	mode : ad-hoc, infrastructure or any
*/

static inline int FindSSIDInList(wlan_adapter *Adapter,
			WLAN_802_11_SSID *ssid, u8 *bssid, int mode)
{
	int	i, j;
	int	net = -ENETUNREACH;
	u8	bestrssi = 0;

	PRINTK1("Num of BSSIDs = %d\n", Adapter->ulNumOfBSSIDs);

	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		if (!SSIDcmp(&Adapter->BSSIDList[i].Ssid, ssid) &&
				(!bssid || 
				 !memcmp(Adapter->BSSIDList[i].
					MacAddress, bssid, ETH_ALEN))) {
			switch (mode) {
			case Wlan802_11Infrastructure :
			case Wlan802_11IBSS :
				j = IsNetworkCompatible(Adapter, i, mode);
				
				if (j >= 0) {
					if (bssid) {
						return i;
					}

					if (SCAN_RSSI(Adapter->BSSIDList[i].Rssi)
							> bestrssi) {
						bestrssi = 
							SCAN_RSSI(Adapter->BSSIDList[i].Rssi);
						net = i;
					}
				} else {
					if (net == -ENETUNREACH) {
						net = j;
					}
				}
				break;
			case Wlan802_11AutoUnknown :
			default :
				if (SCAN_RSSI(Adapter->BSSIDList[i].Rssi)
							> bestrssi) {
					bestrssi = 
						SCAN_RSSI(Adapter->BSSIDList[i].Rssi);
					net = i;
				}
				break;
			}
		}
	}

	return net;
}

static inline int FindBSSIDInList(wlan_adapter *Adapter, u8 *bssid, int mode)
{
	int	i;

	if (!bssid)
		return -EFAULT;

	PRINTK1("Num of BSSIDs = %d\n", Adapter->ulNumOfBSSIDs);

	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		if (!memcmp(Adapter->BSSIDList[i].MacAddress, bssid, 
								ETH_ALEN)) {
			switch (mode) {
			case Wlan802_11Infrastructure :
			case Wlan802_11IBSS :
				return IsNetworkCompatible(Adapter, i, mode);
			default :
				return i;
			}
		}
	}

	return -ENETUNREACH;
}

/*
	Find the best SSID in the Scan List depending on the current
	mode. This is called when we are trying to do an associate to any
*/

static inline int FindBestSSIDInList(wlan_adapter *Adapter)
{
	int	i, mode = Adapter->InfrastructureMode;
	int	bestnet = -ENETUNREACH;
	u8	bestrssi = 0;
	
	ENTER();

	PRINTK1("Num of BSSIDs = %d\n", Adapter->ulNumOfBSSIDs);

	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		switch (mode) {
		case Wlan802_11Infrastructure :
		case Wlan802_11IBSS :
			if (IsNetworkCompatible(Adapter, i, mode) >= 0) {
				if (SCAN_RSSI(Adapter->BSSIDList[i].Rssi) > bestrssi) {
					bestrssi = SCAN_RSSI(Adapter->BSSIDList[i].Rssi);
					bestnet = i;
				}
			}
			break;
		case Wlan802_11AutoUnknown :
		default :
			if (SCAN_RSSI(Adapter->BSSIDList[i].Rssi) > bestrssi) {
				bestrssi = SCAN_RSSI(Adapter->BSSIDList[i].Rssi);
				bestnet = i;
			}
			break;
		}
	}

	LEAVE();
	return bestnet;
}


#endif	/* End of New Implementation of finding SSID and BSSID */ 

#ifdef ENABLE_802_11H_TPC
#define MRV_11H_TPC_POWERCONSTAINT 	0	
	/* Disable constraint. Transfer Power = Min(MaxPower-Constraint, maxCapability) */
#define MRV_11H_TPC_POWERCAPABILITY_MIN		5
#define MRV_11H_TPC_POWERCAPABILITY_MAX		20

int wlan_802_11h_tpc_enable( wlan_private * priv, BOOLEAN flag );
/* Send SNMP CMD to FW to enable/Disable 11H function */

int wlan_802_11h_tpc_enabled( wlan_private * priv );
/*if 11H function enabled: 0 disabled; non 0 enabled */
#endif

#endif	/* _WLAN_FW_H_ */
