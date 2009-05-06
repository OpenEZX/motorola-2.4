/*
 * File : wlan_wext.c 
 *
 * Motorola Confidential Proprietary
 * Copyright (C) Motorola 2005. All rights reserved.
 */

#include	"include.h"

#define WAIT_FOR_SCAN_RRESULT_MAX_TIME (10 * HZ)	// unit is second

static int get_active_data_rates(wlan_adapter *Adapter, WLAN_802_11_RATES rates);

static inline int wlan_scan_type_ioctl(wlan_private *priv, struct iwreq *wrq);

int ascii2hex(unsigned char *d, char *s, u32 dlen)
{
	int i;
	unsigned char n;
    
	memset(d, 0x00, dlen);

	for (i = 0; i < dlen * 2; i++) {
		if ((s[i] >= 48) && (s[i] <= 57))
			n = s[i] - 48;
		else if ((s[i] >= 65) && (s[i] <= 70))
			n = s[i] - 55;
		else if ((s[i] >= 97) && (s[i] <= 102))
			n = s[i] - 87;
		else
			break;
		if ((i % 2) == 0)
			n = n * 16;
		d[i / 2] += n;
	}

	return i;
}

void *wlan_memchr(void *s, int c, int n)
{
	const unsigned char	*p = s;

	while(n-- != 0) {
		if((unsigned char)c == *p++) {
			return (void *) (p - 1);
		}
	}
	return NULL;
}


#if WIRELESS_EXT > 14
static int mw_to_dbm(int mw)
{
	if (mw < 2)
		return 0;
	else if (mw < 3)
		return 3;
	else if (mw < 4)
		return 5;
	else if (mw < 6)
		return 7;
	else if (mw < 7)
		return 8;
	else if (mw < 8)
		return 9;
	else if (mw < 10)
		return 10;
	else if (mw < 13)
		return 11;
	else if (mw < 16)
		return 12;
	else if (mw < 20)
		return 13;
	else if (mw < 25)
		return 14;
	else if (mw < 32)
		return 15;
	else if (mw < 40)
		return 16;
	else if (mw < 50)
		return 17;
	else if (mw < 63)
		return 18;
	else if (mw < 79)
		return 19;
	else if (mw < 100)
		return 20;
	else
		return 21;
}
#endif

CHANNEL_FREQ_POWER *find_cfp_by_band_and_channel(wlan_adapter *adapter, 
							u8 band, u16 channel)
{
	CHANNEL_FREQ_POWER	*cfp = NULL;
	REGION_CHANNEL		*rc;
	int	count = sizeof(adapter->region_channel) / 
				sizeof(adapter->region_channel[0]);
	int i, j;

	for (j = 0; !cfp && (j < count); j++) {
		rc = &adapter->region_channel[j];

#ifdef ENABLE_802_11D
		if( adapter->State11D.Enable11D == ENABLE_11D ) {
			rc = &adapter->universal_channel[j];
		}
#endif
		if(!rc->Valid || !rc->CFP)
			continue;
#ifdef MULTI_BANDS
		switch (rc->Band) {
			case BAND_A:
				switch (band) {
					case BAND_A:	// matching BAND_A
						break;
					default:
						continue;
				}
				break;
			case BAND_B:
			case BAND_G:
				switch (band) {
					case BAND_B:	//matching BAND_B/G
					case BAND_G:
						break;
					default:
						continue;
				}
				break;
			default:
				continue;
		}
#else
		if (rc->Band != band)
			continue;
#endif
		for (i = 0; i < rc->NrCFP; i++) {
			if (rc->CFP[i].Channel == channel) {
				cfp = &rc->CFP[i];
				break;
			}
		}
	}

	if (!cfp && channel)
		PRINTK("find_cfp_by_band_and_channel(): cannot find "
			"cfp by band %d & channel %d\n",band,channel);

	return cfp;
}
			
CHANNEL_FREQ_POWER *find_cfp_by_band_and_freq(wlan_adapter *adapter, 
							u8 band, u32 freq)
{
	CHANNEL_FREQ_POWER *cfp = NULL;
	REGION_CHANNEL *rc;
	int count = sizeof(adapter->region_channel) / 
				sizeof(adapter->region_channel[0]);
	int i, j;

	for (j = 0; !cfp && (j < count); j++) {
		rc = &adapter->region_channel[j];

#ifdef ENABLE_802_11D
		if( adapter->State11D.Enable11D == ENABLE_11D ) {
			rc = &adapter->universal_channel[j];
		}
#endif

		if(!rc->Valid || !rc->CFP)
			continue;
#ifdef MULTI_BANDS
		switch (rc->Band) {
			case BAND_A:
				switch (band) {
					case BAND_A:	// matching BAND_A
						break;
					default:
						continue;
				}
				break;
			case BAND_B:
			case BAND_G:
				switch (band) {
					case BAND_B:	//matching BAND_B/G
					case BAND_G:
						break;
					default:
						continue;
				}
				break;
			default:
				continue;
		}
#else
		if (rc->Band != band)
			continue;
#endif
		for (i = 0; i < rc->NrCFP; i++) {
			if (rc->CFP[i].Freq == freq) {
				cfp = &rc->CFP[i];
				break;
			}
		}
	}

	if(!cfp && freq)
		PRINTK("find_cfp_by_band_and_freql(): cannot find cfp by "
				"band %d & freq %d\n",band,freq);

	return cfp;
}
			
int wlan_associate(wlan_private *priv, WLAN_802_11_SSID *ssid)
{
	int ret = 0;
	wlan_adapter *Adapter = priv->adapter;

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_AUTHENTICATE,
		0, HostCmd_OPTION_USE_INT 
		| HostCmd_OPTION_WAITFORRSP
		, 0, HostCmd_PENDING_ON_NONE, &Adapter->CurrentBSSID);

	if (ret) {
		LEAVE();
		return ret;
	}

#ifdef WPA
#ifndef WPA2
	if (Adapter->SecInfo.WPAEnabled == TRUE) {
		/* send down authentication mode */
		ret = PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_RSN_AUTH_SUITES,
				HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_NONE, NULL);
		if (ret) {
			LEAVE();
			return ret;
		}

		HEXDUMP("WPA IE ", Adapter->Wpa_ie, 40);

// MS8380 f/w does not support UNICAST/MCAST CIPHER at this time
#ifndef	MS8380
		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_UNICAST_CIPHER,
				HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_SET_OID, NULL);
		if (ret) {
			LEAVE();
			return ret;
		}

		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_MCAST_CIPHER,
				HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_SET_OID, NULL);
		if (ret) {
			LEAVE();
			return ret;
		}
#endif
	}
#endif	//WPA2
#endif	//WPA

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_ASSOCIATE,
			0, HostCmd_OPTION_USE_INT 
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_SET_OID, ssid);
	return ret;
}

#ifdef MANF_CMD_SUPPORT
static int wlan_mfg_command(wlan_private * priv, void *userdata)
{
	PkHeader	*pkHdr;
	int		len, ret;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	// creating the cmdbuf
	if (Adapter->mfg_cmd == NULL) {
		PRINTK1("Creating cmdbuf\n");
		if (!(Adapter->mfg_cmd = kmalloc(2000, GFP_KERNEL))) {
			PRINTK1("kmalloc failed!\n");
			return -1;
		}
		Adapter->mfg_cmd_len = 2000;
	}
	
	// get PktHdr from userdata
	if (copy_from_user(Adapter->mfg_cmd, userdata, sizeof(PkHeader))) {
		PRINTK1("copy from user failed :PktHdr\n");
		return -1;
	}

	// get the size
	pkHdr = (PkHeader *) Adapter->mfg_cmd;
	len = pkHdr->len;

	PRINTK1("cmdlen = %d\n", (u32) len);

	while (len >= Adapter->mfg_cmd_len) {
		kfree(Adapter->mfg_cmd);

		if (!(Adapter->mfg_cmd = kmalloc(len + 256, GFP_KERNEL))) {
			PRINTK1("kmalloc failed!\n");
			return -1;
		}

		Adapter->mfg_cmd_len = len + 256;
	}

	// get the whole command from user
	if (copy_from_user(Adapter->mfg_cmd, userdata, len)) {
		PRINTK1("copy from user failed :PktHdr\n");
		return -1;
	}

	ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_MFG_COMMAND,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	// copy it back to user
	if (Adapter->mfg_cmd_resp_len > 0) {
		if (copy_to_user(userdata, Adapter->mfg_cmd, len)) {
			PRINTK1("copy to user failed \n");
			return -1;
		}
	}

	kfree(Adapter->mfg_cmd);
	Adapter->mfg_cmd = NULL;
	Adapter->mfg_cmd_len = 0;

	LEAVE();
	return ret;
}
#endif

static inline int StopAdhocNetwork(wlan_private *priv,u16 pending_info)
{
	return PrepareAndSendCommand(priv, HostCmd_CMD_802_11_AD_HOC_STOP,
			0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP
			, 0, pending_info, NULL);
}

static inline int SendDeauthentication(wlan_private *priv,u16 pending_info)
{
#ifdef REASSOCIATION_SUPPORT
	wlan_adapter *Adapter = priv->adapter;

    /* If a reassociation attempt is in progress, do not send the deauth */
    if (Adapter->reassocAttempt)
    {
        return 0;
    }
#endif
	return PrepareAndSendCommand(priv, HostCmd_CMD_802_11_DEAUTHENTICATE,
			0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP
			, 0, pending_info, NULL);
}

static int ChangeAdhocChannel(wlan_private *priv, int channel)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;
	u16		new_channel = channel;
	
	ENTER();
	
	PRINTK1("Setting Adhoc Channel to = %d\n", Adapter->AdhocChannel);

	//the channel in f/w could be out of sync. in some cases.
	//always set the channel number
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RF_CHANNEL,
			RF_SET, HostCmd_OPTION_USE_INT 
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_CMD, &new_channel);

	if (ret) {
		LEAVE();
		return ret;
	}

	if (Adapter->AdhocChannel == channel) {
		LEAVE();
		return 0;
	}
	
	Adapter->AdhocChannel = channel;

	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		int			i;
		WLAN_802_11_SSID	curAdhocSsid;
		
		PRINTK1("Channel Changed while in an IBSS\n");

		/* Copy the current ssid */
		memcpy(&curAdhocSsid, &Adapter->CurBssParams.ssid, 
				sizeof(WLAN_802_11_SSID));
		
		/* Exit Adhoc mode */
		PRINTK1("In ChangeAdhocChannel(): Sending Adhoc Stop\n");
		ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

		if (ret) {
			LEAVE();
			return ret;
		}

		SendSpecificScan(priv, &curAdhocSsid);

		// find out the BSSID that matches the current SSID 
		i = FindSSIDInList(Adapter, &curAdhocSsid, NULL, 
				Wlan802_11IBSS);

		if (i >= 0) {
			PRINTK1("SSID found at %d in List," 
					"so join\n", i);
			JoinAdhocNetwork(priv, &curAdhocSsid, i);
		} else {
			// else send START command
			PRINTK1("SSID not found in list, "
					"so creating adhoc with ssid = %s\n",
					curAdhocSsid.Ssid);
			StartAdhocNetwork(priv, &curAdhocSsid);
		}	// end of else (START command)
	}

	LEAVE();
	return 0;
}

int wlan_set_freq(struct net_device *dev, struct iw_request_info *info, 
		struct iw_freq *fwrq, char *extra)
{
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	int             rc = -EINPROGRESS;	/* Call commit handler */
	CHANNEL_FREQ_POWER	*cfp;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk(KERN_CRIT "SIOCSIWFREQ: IOCTLS called when station is "
							"in DeepSleep\n");
		return -EBUSY;
	}
#endif

#ifdef MULTI_BANDS
	if (Adapter->is_multiband) {
		printk(KERN_CRIT "SIOCSIWFREQ: IOCTLS call not supported in "
				"MultiBands mode, use private command "
				"'setadhocch' instead.\n");
		return -ENOTSUPP;
	}
#endif

	/*
	 * If setting by frequency, convert to a channel 
	 */
	if (fwrq->e == 1) {

		long	f = fwrq->m / 100000;
		int	c = 0;

		cfp = find_cfp_by_band_and_freq(Adapter, 0, f);
		if (!cfp) {
			PRINTK("Invalid freq=%ld\n", f);
			return -EINVAL;
		} 
		
		c = (int) cfp->Channel;

		if (c < 0) 
			return -EINVAL;
	
		fwrq->e = 0;
		fwrq->m = c;
	}

	/*
	 * Setting by channel number 
	 */
	if (fwrq->m > 1000 || fwrq->e > 0) {
		rc = -EOPNOTSUPP;
	} else {
		int	channel = fwrq->m;

		cfp = find_cfp_by_band_and_channel(Adapter, 0, channel);
		if (!cfp) {
			rc = -EINVAL;
		} else {
			if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
				rc = ChangeAdhocChannel(priv, channel);
				/*  If station is WEP enabled, send the 
				 *  command to set WEP in firmware
				 */
				if (Adapter->SecInfo.WEPStatus == 
							Wlan802_11WEPEnabled) {
					PRINTK1("set_freq: WEP Enabled\n");
					ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_SET_WEP, 
						0, HostCmd_OPTION_USE_INT 
						| HostCmd_OPTION_WAITFORRSP
						, OID_802_11_ADD_WEP, 
						HostCmd_PENDING_ON_CMD,
						NULL);

					if (ret) {
						LEAVE();
						return ret;
					}
						
					Adapter->CurrentPacketFilter |=	
						HostCmd_ACT_MAC_WEP_ENABLE;

					SetMacPacketFilter(priv);
				}
			} else {
				rc = -EOPNOTSUPP;
			}
		}
	}

	LEAVE();
	return rc;
}

int SendSpecificScan(wlan_private *priv, WLAN_802_11_SSID *RequestedSSID)
{
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	if (!Adapter->bIsScanInProgress) {
//		Adapter->ulNumOfBSSIDs = 0;
		
		/* store the SSID info temporarily */
		if (RequestedSSID) {
			/* Specific BSSID */
			memcpy(&(Adapter->AttemptedSSIDBeforeScan),
				RequestedSSID, sizeof(WLAN_802_11_SSID));

			memcpy(&Adapter->SpecificScanSSID, RequestedSSID,
						sizeof(WLAN_802_11_SSID));
		}

		Adapter->SetSpecificScanSSID = TRUE;
		
#ifdef PROGRESSIVE_SCAN
		Adapter->ChannelsPerScan = MRVDRV_CHANNELS_PER_ACTIVE_SCAN;
#endif
		wlan_scan_networks(priv, HostCmd_PENDING_ON_NONE);
		Adapter->SetSpecificScanSSID = FALSE;
#ifdef PROGRESSIVE_SCAN
		Adapter->ChannelsPerScan = MRVDRV_CHANNELS_PER_SCAN;
#endif
	}

	if (Adapter->SurpriseRemoved)
		return -1;

	LEAVE();
	return 0;
}

#ifdef DEEP_SLEEP_CMD
int SetDeepSleep(wlan_private *priv, BOOLEAN bDeepSleep)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef PS_REQUIRED
	// check if it is in IEEE PS mode or not
	if (Adapter->PSMode != Wlan802_11PowerModeCAM) {
		LEAVE();
		return 0;
	}
#endif

	if (bDeepSleep == TRUE) {
		// 1. return failure if it is in normal PS mode
		// 2. return directly if it is in DEELP_SLEP already
		// 3. otherwise send DEEP_SLEEP to FM/HW to put HW to deep sleep
		// 4. change the state to DEELP_SLEEP after sent cmd done in
		//    DownloadCommandToStation

		if (Adapter->IsDeepSleep != TRUE) {
			PRINTK("Deep Sleep : sleep\n");

			/* If the state is associated, turn off the
			 * network device */	
			if (Adapter->MediaConnectStatus == 
					WlanMediaStateConnected) {
				/* Flush all the packets upto the OS before stopping */
				wlan_send_rxskbQ(priv);
				netif_carrier_off(priv->wlan_dev.netdev);
			}
			
			// note: the command could be queued and executed later
			//       if there is command in prigressing.
			ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_DEEP_SLEEP, 0,
					HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP,
					0, HostCmd_PENDING_ON_NONE,
					NULL);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
	}
	else {
		// 1. return directly if it is not in DEEP_SLEEP mode
		// 2. otherwise wakeup FM/HW: CF: access any register, 
		// 	SD/SPI: set special register.
		// 3. change state to non DEEP_SLEEP after received AWAKE 
		// 	event from firmware.
		// 4. verify if it is awake or not by reading flag. to avoid 
		// 	dead lock if AWAKE is lost or not generated by FM

		if (Adapter->IsDeepSleep == TRUE) {

			PRINTK("Deep Sleep : wakeup\n");

			// add delay as gap here to gaurantee that wake up 
			// will not be sent until HW/FM is really sleeping.
			// otherwise the state could mismatch: Drive thinks 
			// HW/FM is sleeping but actually HW/FM is activ
				
			//mdelay(10);

			if (Adapter->IntCounterSaved)
				Adapter->IntCounter = Adapter->IntCounterSaved;

			if (sbi_exit_deep_sleep(priv))
				PRINTK("Deep Sleep : wakeup failed\n");
			

			if (Adapter->IsDeepSleep == TRUE) {
			
				if(interruptible_sleep_on_timeout(&Adapter->ds_awake_q,
						       	WAIT_FOR_SCAN_RRESULT_MAX_TIME) == 0)
					printk("ds_awake_q: timer expired\n");
			}

			/* If the state is associated, turn off the
			 * network device */	
			if (Adapter->MediaConnectStatus == 
					WlanMediaStateConnected) {
				netif_carrier_on(priv->wlan_dev.netdev);
			}
			
			if(Adapter->IntCounter)
				wake_up_interruptible(&priv->MainThread.waitQ);
		}
	}
	
	LEAVE();
	return 0;
}
#endif // DEEP_SLEEP_CMD

int StartAdhocNetwork(wlan_private * priv, WLAN_802_11_SSID * AdhocSSID)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
	
	if (!Adapter->capInfo.ShortPreamble) {
		PRINTK("AdhocStart: Long Preamble\n");
		Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;
	} else {
		PRINTK("AdhocStart: Short Preamble\n");
		Adapter->Preamble = HostCmd_TYPE_SHORT_PREAMBLE;
	}
		
	// set to firmware anyway
	SetRadioControl(priv);

	Adapter->AdhocCreate = TRUE;
	
	PRINTK1("Adhoc Channel = %d\n", Adapter->AdhocChannel);
	PRINTK1("CurBssParams.channel = %d\n", Adapter->CurBssParams.channel);
	PRINTK1("CurBssParams.band = %d\n", Adapter->CurBssParams.band);

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_AD_HOC_START,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_SET_OID, AdhocSSID);

	LEAVE();
	return ret;
}

int JoinAdhocNetwork(wlan_private * priv, WLAN_802_11_SSID * AdhocSSID, int i)
{
	int			ret = 0;
	wlan_adapter		*Adapter = priv->adapter;
	WLAN_802_11_BSSID	*pBSSIDList;
 
	ENTER();

	pBSSIDList = &Adapter->BSSIDList[i];

	/*Use ShortPreamble only when both creator and card supports 
	 short preamble*/	
	if (!pBSSIDList->Cap.ShortPreamble 
				|| !Adapter->capInfo.ShortPreamble) {
		PRINTK("AdhocJoin: Long Preamble\n");
		Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;
	}
	else {
		PRINTK("AdhocJoin: Short Preamble\n");
		Adapter->Preamble = HostCmd_TYPE_SHORT_PREAMBLE;
	}
		
	// set to firmware anyway
	SetRadioControl(priv);
	
	Adapter->Channel = pBSSIDList->Channel;
    
	PRINTK1("Joining on Channel %d\n", Adapter->Channel);
	PRINTK1("CurBssParams.channel = %d\n", Adapter->CurBssParams.channel);
	PRINTK1("CurBssParams.band = %c\n", Adapter->CurBssParams.band);

	Adapter->AdhocCreate = FALSE;

	// store the SSID info temporarily
	memset(&Adapter->AttemptedSSIDBeforeScan, 0, sizeof(WLAN_802_11_SSID));
	memcpy(&Adapter->AttemptedSSIDBeforeScan,
			AdhocSSID, sizeof(WLAN_802_11_SSID));

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_AD_HOC_JOIN,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, OID_802_11_SSID, HostCmd_PENDING_ON_SET_OID,
			&Adapter->AttemptedSSIDBeforeScan);

	LEAVE();
	return ret;
}

int FindBestNetworkSsid(wlan_private *priv, WLAN_802_11_SSID *pSSID)
{
	wlan_adapter	*Adapter = priv->adapter;
	int		i, ret = 0;	
	
	ENTER();
	
	memset(pSSID, 0, sizeof(WLAN_802_11_SSID));
	
	Adapter->bAutoAssociation = TRUE;

	wlan_scan_networks(priv, HostCmd_PENDING_ON_CMD);

	PRINTK1("AUTOASSOCIATE: Scan complete - doing associate...\n");

	i = FindBestSSIDInList(Adapter);
	
	if (i >= 0) {
		PWLAN_802_11_BSSID	pReqBSSID;

		pReqBSSID = &Adapter->BSSIDList[i];
		memcpy(pSSID, &pReqBSSID->Ssid, sizeof(WLAN_802_11_SSID));

		/* Make sure we are in the right mode */
		if (Adapter->InfrastructureMode == Wlan802_11AutoUnknown) {
			Adapter->InfrastructureMode =
				pReqBSSID->InfrastructureMode;

			ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_SNMP_MIB, 0,
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, OID_802_11_INFRASTRUCTURE_MODE,
					HostCmd_PENDING_ON_SET_OID, NULL);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
	} 
	
	if (!pSSID->SsidLength) {
		ret = -1;
	}

	Adapter->bAutoAssociation = FALSE;

	LEAVE();
	return ret;
}

int wlan_set_essid(struct net_device *dev, struct iw_request_info *info,
	       struct iw_point *dwrq, char *extra)
{
	int			ret = 0;
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	WLAN_802_11_SSID	ssid, *pRequestSSID;
	int			i;//, Status = 0;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWESSID: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif
    
	// cancel re-association timer if there's one
	if (Adapter->TimerIsSet == TRUE) {
		CancelTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}

	/* Check the size of the string */
	if (dwrq->length > IW_ESSID_MAX_SIZE + 1) {
		return -E2BIG ;
	}
	
	//Adapter->SetSpecificScanSSID = FALSE;

	memset(&ssid, 0, sizeof(WLAN_802_11_SSID));

	/*
	 * Check if we asked for `any' or 'particular' 
	 */
	if (!dwrq->flags) {
//		Adapter->ulNumOfBSSIDs = 0;
		if (FindBestNetworkSsid(priv, &ssid)) {
			PRINTK1("Could not find best network\n");
			return 0;
		}
	} else {
		Adapter->bAutoAssociation = TRUE;
		if (Adapter->bIsPendingReset == TRUE)
			return WLAN_STATUS_FAILURE; // Reset in Progress
	
		/* Set the SSID */
		memcpy(ssid.Ssid, extra, dwrq->length);
		ssid.SsidLength = dwrq->length - 1;
	}

	pRequestSSID = &ssid;

	PRINTK1("Requested new SSID = %s\n",
			(pRequestSSID->SsidLength > 0) ?
			(char *) pRequestSSID->Ssid : "NULL");

	if (!pRequestSSID->SsidLength || pRequestSSID->Ssid[0] < 0x20) {
		PRINTK1("Invalid SSID - aborting set_essid\n");
		return -EINVAL;
	}

	// If the requested SSID is not a NULL string, 
	// do ASSOCIATION or JOIN

	// if it's infrastructure mode
	if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
		// check for the SSID
		PRINTK1("Got the ssid for comparision %s\n",
				pRequestSSID->Ssid);

#ifdef PROGRESSIVE_SCAN
		i = -1;
#else
		i = FindSSIDInList(Adapter, pRequestSSID, NULL,
				Wlan802_11Infrastructure);
#endif
		if (i < 0) {
			// If the SSID is not found in the list, 
			// do an specific scan with the specific SSID. 
			// This will scan for AP's which have 
			// SSID broadcasting disabled.
			PRINTK("0 APs in scan list or SSID not found in"
					" scan list, trying active scan; " 
					"do scan first\n");

			SendSpecificScan(priv, pRequestSSID);

			i = FindSSIDInList(Adapter, pRequestSSID, NULL,
					Wlan802_11Infrastructure);

#ifdef	DEBUG
			if (i >= 0) {
				PRINTK("AP found in new list\n");
			}
#endif
		}

		if (i >= 0)
        {
			PRINTK("SSID found in scan list ... associating...\n");
            
			if (Adapter->MediaConnectStatus == WlanMediaStateConnected) 
            {
				PRINTK("Already Connected ..\n");
				ret = SendDeauthentication(priv, HostCmd_PENDING_ON_NONE);

				if (ret)
                {
					LEAVE();
					return ret;
				}
			}
			
			memcpy(Adapter->CurrentBSSID,
                   Adapter->BSSIDList[i].MacAddress, 
                   ETH_ALEN);
            
			Adapter->RequestViaBSSID = FALSE;
            
			ret = wlan_associate(priv, pRequestSSID);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
        else  /* i >= 0 */
        {
            return i; /* return -ENETUNREACH, passed from FindSSIDInList */
        }
	} 
    else 
    {
		// it's ad hoc mode
		/* If the requested SSID matches current SSID return */
		if (!SSIDcmp(&Adapter->CurBssParams.ssid, pRequestSSID))
			return WLAN_STATUS_SUCCESS;

		SendSpecificScan(priv, pRequestSSID);

		if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
			/*
			 * Exit Adhoc mode 
			 */
			PRINTK1("Sending Adhoc Stop\n");
			ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

			if (ret) {
				LEAVE();
				return ret;
			}

		}
		// find out the BSSID that matches the SSID given in
		// InformationBuffer
		i = FindSSIDInList(Adapter, pRequestSSID, NULL, 
				Wlan802_11IBSS);

		Adapter->RequestViaBSSID = FALSE;
		
		if (i >= 0) {
			PRINTK1("SSID found at %d in List," 
					"so join\n", i);
			JoinAdhocNetwork(priv, &ssid, i);
		} else {
			// else send START command
			PRINTK1("SSID not found in list, "
					"so creating adhoc with ssid = %s\n", 
					ssid.Ssid);

			StartAdhocNetwork(priv, &ssid);
		}	// end of else (START command)
	}		// end of else (Ad hoc mode)

	LEAVE();
	return 0;
}

u32 index_to_data_rate(u8 index)
{
	if (index >= sizeof(WlanDataRates))
		index = 0;

	return WlanDataRates[index];
}

u8 data_rate_to_index(u32 rate)
{
	u8	*ptr;

	if (rate)
		if ((ptr = wlan_memchr(WlanDataRates, (u8)rate, 
						sizeof(WlanDataRates))))
			return (ptr - WlanDataRates);

	return 0;
}

int wlan_set_rate(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	u32			data_rate;
	u16			action;
	int 			ret = 0;
	WLAN_802_11_RATES	rates;
	u8			*rate;
    
	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk(KERN_ALERT "SIOCSIWRATE: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	PRINTK1("Vwrq->value = %d\n", vwrq->value);

	if (vwrq->value == -1) {
		action = HostCmd_ACT_SET_TX_AUTO;	// Auto
		Adapter->Is_DataRate_Auto = TRUE;
		Adapter->DataRate = 0;
	} else {
		if (vwrq->value % 100000) {
			return -EINVAL;
		}

		data_rate = vwrq->value / 500000;

		memset(rates, 0, sizeof(rates));
		get_active_data_rates(Adapter, rates);
		rate = rates;
		while (*rate) {
			PRINTK("Rate=0x%X  Wanted=0x%X\n",*rate,data_rate);
			if ((*rate & 0x7f) == (data_rate & 0x7f))
				break;
			rate++;
		}
		if (!*rate) {
			printk(KERN_ALERT "The fixed data rate 0x%X is out "
					"of range.\n",data_rate);
			return -EINVAL;
		}

		Adapter->DataRate = data_rate;
		action = HostCmd_ACT_SET_TX_FIX_RATE;
		Adapter->Is_DataRate_Auto = FALSE;
	}

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_DATA_RATE, 
			action,	HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	LEAVE();
	return ret;
}

int wlan_get_rate(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	if (Adapter->Is_DataRate_Auto) {
		vwrq->fixed = 0;
	} else {
		vwrq->fixed = 1;
	}
	
	vwrq->value = Adapter->DataRate * 500000;

	LEAVE();
	return 0;
}

int wlan_set_mode(struct net_device *dev,
	      struct iw_request_info *info, u32 * uwrq, char *extra)
{
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	
	WLAN_802_11_NETWORK_INFRASTRUCTURE	WantedMode;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWMODE: IOCTLS called when station is in DeepSleep\n");
		return -EBUSY;
	}
#endif

	switch (*uwrq) {
		case IW_MODE_ADHOC:
			PRINTK1("Wanted Mode is ad-hoc: current DataRate=%#x\n",Adapter->DataRate);
			WantedMode = Wlan802_11IBSS;
#ifdef MULTI_BANDS
			if (Adapter->adhoc_start_band == BAND_A) 
				Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL_A;
			else
#endif
				Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL;
			break;

		case IW_MODE_INFRA:
			PRINTK1("Wanted Mode is Infrastructure\n");
			WantedMode = Wlan802_11Infrastructure;
			break;

		case IW_MODE_AUTO:
			PRINTK1("Wanted Mode is Auto\n");
			WantedMode = Wlan802_11AutoUnknown;
			break;

		default:
			PRINTK1("Wanted Mode is Unknown: 0x%x\n", *uwrq);
			return -EINVAL;
	}

	if (Adapter->InfrastructureMode == WantedMode ||
			WantedMode == Wlan802_11AutoUnknown) {
		PRINTK1("Already set to required mode! No change!\n");

		Adapter->InfrastructureMode = WantedMode;

		LEAVE();
		return 0;
	}

	if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
#ifdef PS_REQUIRED
		if (Adapter->PSState != PS_STATE_FULL_POWER) {
			PSWakeup(priv, HostCmd_OPTION_WAITFORRSP);
		}
		Adapter->PSMode = Wlan802_11PowerModeCAM;
#endif
	}
	
	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
			ret = SendDeauthentication(priv, 
					HostCmd_PENDING_ON_NONE);

			if (ret) {
				LEAVE();
				return ret;
			}
		} else if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
			/* If current mode is Adhoc, clean stale information */
			ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
	} 

	if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled) {
		/* If there is a key with the specified SSID, 
		 * send REMOVE WEP command, to make sure we clean up
		 * the WEP keys in firmware
		 */
		ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_802_11_SET_WEP,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, OID_802_11_REMOVE_WEP,
			HostCmd_PENDING_ON_CMD,
			NULL);

		if (ret) {
			LEAVE();
			return ret;
		}

		Adapter->CurrentPacketFilter &= ~HostCmd_ACT_MAC_WEP_ENABLE;

		SetMacPacketFilter(priv);
	}

	Adapter->SecInfo.WEPStatus = Wlan802_11WEPDisabled;
	Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeOpen;

	Adapter->InfrastructureMode = WantedMode;

	ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_802_11_SNMP_MIB,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, OID_802_11_INFRASTRUCTURE_MODE,
			HostCmd_PENDING_ON_SET_OID, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	LEAVE();
	return 0;
}

#ifdef WPA
int wlan_set_encode_wpa(struct net_device *dev, 
			struct iw_request_info *info,
				struct iw_point *dwrq, char *extra)
{
	int			ret = 0;
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	PWLAN_802_11_KEY	pKey;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>():%s IOCTLS called when station is"
					" in DeepSleep\n",__FUNCTION__);
		return -EBUSY;
	}
#endif

	pKey = (PWLAN_802_11_KEY) extra;

	HEXDUMP("Key buffer: ", extra, dwrq->length);

	HEXDUMP("KeyMaterial: ", (unsigned char *) pKey->KeyMaterial,
			pKey->KeyLength);

	// current driver only supports key length of up to 32 bytes
	if (pKey->KeyLength > MRVL_MAX_WPA_KEY_LENGTH) {
		PRINTK1(" Error in key length \n");
		return -1;
	}

#ifdef WPA2
	ret = PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_KEY_MATERIAL,
		HostCmd_ACT_SET,
		HostCmd_OPTION_USE_INT
		| HostCmd_OPTION_WAITFORRSP
		, KEY_INFO_ENABLED, HostCmd_PENDING_ON_SET_OID,
		pKey);
	if (ret) {
		LEAVE();
		return ret;
	}
#else
	// check bit 30 for pairwise key, if pairwise key, store in 
	// location 0 of the key map
	if (pKey->KeyIndex & 0x40000000) {
		/*
		 * This scheduling is needed to let the 4th EAPOL
		 * packet to be sent out before setting the PTK.
		 */
		os_sched_timeout(10);

		Adapter->EncryptionStatus =
			Wlan802_11Encryption2Enabled;

		memcpy(Adapter->WpaPwkKey.KeyMaterial,
			pKey->KeyMaterial, pKey->KeyLength);

		if (pKey->KeyLength == WPA_AES_KEY_LEN) {
	
			PRINTK1("Set PWK WPA - AES Key\n");
			
			ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_PWK_KEY,
				HostCmd_ACT_SET_AES,
				HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_SET_OID,
				&Adapter->WpaPwkKey.KeyMaterial);

			if (ret) {
				LEAVE();
				return ret;
			}
		} 
		else if (pKey->KeyLength == WPA_TKIP_KEY_LEN) {

			PRINTK1("Set PWK WPA - TKIP Key\n");
			
			ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_PWK_KEY,
					HostCmd_ACT_SET,
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_SET_OID,
					&Adapter->WpaPwkKey.KeyMaterial);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
	} else {
		PRINTK1("GROUP KEY\n");	

		Adapter->EncryptionStatus =
			Wlan802_11Encryption2Enabled;

		memcpy(Adapter->WpaGrpKey.KeyMaterial,
			pKey->KeyMaterial, pKey->KeyLength);

		if (pKey->KeyLength == WPA_AES_KEY_LEN) {
	
			PRINTK1("Set GTK WPA - AES Key\n");

			ret = PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_GRP_KEY,
				HostCmd_ACT_SET_AES, 
				HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_SET_OID,
				&Adapter->WpaGrpKey.KeyMaterial);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
		else if (pKey->KeyLength == WPA_TKIP_KEY_LEN) {
	
			PRINTK1("Set GTK WPA - TKIP Key\n");

			ret = PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_GRP_KEY,
				HostCmd_ACT_SET, 
				HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_SET_OID,
				&Adapter->WpaGrpKey.KeyMaterial);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
	}
#endif	//WPA2

	LEAVE();
	return ret;
}

#endif

/*
 *  iwconfig ethX key on:	WEPEnabled;
 *  iwconfig ethX key off:	WEPDisabled;
 *  iwconfig ethX key [x]:	CurrentWepKeyIndex = x; WEPEnabled;
 *  iwconfig ethX key [x] kstr:	WepKey[x] = kstr;
 *  iwconfig ethX key kstr:	WepKey[CurrentWepKeyIndex] = kstr;
 *
 *  all:			Send command SET_WEP;
 				SetMacPacketFilter;
 */

int wlan_set_encode_nonwpa(struct net_device *dev,
			       struct iw_request_info *info,
				       struct iw_point *dwrq, char *extra)
{
	int			ret = 0;
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	MRVL_WEP_KEY		*pWep;
	WLAN_802_11_SSID	ssid;
	int			index, PrevAuthMode;

	ENTER();

	pWep = &Adapter->WepKey[Adapter->CurrentWepKeyIndex];
	PrevAuthMode = Adapter->SecInfo.AuthenticationMode;

	index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

	if(index >= 4) {
		PRINTK("Key index #%d out of range.\n", index + 1);
		return -EINVAL;
	}

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk(KERN_ALERT "():%s IOCTLS called when station is"
					" in DeepSleep\n",__FUNCTION__);
		return -EBUSY;
	}
#endif

	PRINTK1("Flags=0x%x, Length=%d Index=%d CurrentWepKeyIndex=%d\n", 
			dwrq->flags, dwrq->length, index, Adapter->CurrentWepKeyIndex);

//	if (dwrq->flags == 0x801) /* User has to explicitely mention the
//				   * Auth mode (open/restricted)
//		dwrq->flags |= 0x2000;

	if (dwrq->length > 0) {
		/* iwconfig ethX key [n] xxxxxxxxxxx 
		 * Key has been provided by the user 
		 */

		/*
		 * Check the size of the key 
		 */

		if (dwrq->length > MAX_KEY_SIZE) {
			return -EINVAL;
		}

		/*
		 * Check the index (none -> use current) 
		 */

		if (index < 0 || index > 3)		//invalid index or no index
			index = Adapter->CurrentWepKeyIndex;
		else					//index is given & valid
			pWep = &Adapter->WepKey[index];

		/*
		 * Check if the key is not marked as invalid 
		 */
		if (!(dwrq->flags & IW_ENCODE_NOKEY)) {
			/* Cleanup */
			memset(pWep, 0, sizeof(MRVL_WEP_KEY));

			/* Copy the key in the driver */
			memcpy(pWep->KeyMaterial, extra, dwrq->length);

			/* Set the length */
			if (dwrq->length > MIN_KEY_SIZE) {
				pWep->KeyLength = MAX_KEY_SIZE;
			} else {
				if (dwrq->length > 0) {
					pWep->KeyLength = MIN_KEY_SIZE;
				} else {
					/* Disable the key */
					pWep->KeyLength = 0;
				}
			}
			pWep->KeyIndex = index;

			if(Adapter->SecInfo.WEPStatus != Wlan802_11WEPEnabled) {
				/*
				 * The status is set as Key Absent 
				 * so as to make sure we display the 
				 * keys when iwlist ethX key is 
				 * used - MPS 
				 */

				Adapter->SecInfo.WEPStatus = 
					Wlan802_11WEPKeyAbsent;
			}

			PRINTK1("KeyIndex=%u KeyLength=%u\n", 
					pWep->KeyIndex,
					pWep->KeyLength);
			HEXDUMP("WepKey", (u8 *) pWep->KeyMaterial, 
					pWep->KeyLength);
		}
	} else {
		/*
		 * No key provided so it is either enable key, 
		 * on or off */
		if (dwrq->flags & IW_ENCODE_DISABLED) {
			PRINTK1("******** iwconfig ethX key off **********\n");

			Adapter->SecInfo.WEPStatus = Wlan802_11WEPDisabled;
			if (Adapter->SecInfo.AuthenticationMode == Wlan802_11AuthModeShared)
				Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeOpen;
		} else {
			/* iwconfig ethX key [n]
			 * iwconfig ethX key on 
			 * Do we want to just set the transmit key index ? 
			 */

			if (index < 0 || index > 3) {
				PRINTK1("******** iwconfig ethX key on **********\n");
				index = Adapter->CurrentWepKeyIndex;
			}
			else {
				PRINTK1("******** iwconfig ethX key [x=%d] **********\n",index);
				Adapter->CurrentWepKeyIndex = index;
			}

			/* Copy the required key as the current key */
			pWep = &Adapter->WepKey[index];

			if (!pWep->KeyLength) {
				PRINTK1("Key not set,so cannot enable it\n");
				return -EPERM;
			}
	    
			Adapter->SecInfo.WEPStatus = Wlan802_11WEPEnabled;

			HEXDUMP("KeyMaterial", (u8 *) pWep->KeyMaterial,
					pWep->KeyLength);
		}
	}

	if(pWep->KeyLength) {
		ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_802_11_SET_WEP,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, OID_802_11_ADD_WEP,
			HostCmd_PENDING_ON_SET_OID, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}
	}

	if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled) {
		Adapter->CurrentPacketFilter |= HostCmd_ACT_MAC_WEP_ENABLE;
	} else {
		Adapter->CurrentPacketFilter &= ~HostCmd_ACT_MAC_WEP_ENABLE;
	}

	SetMacPacketFilter(priv);

	// PRINTK ("Flags %x\n", dwrq->flags);
	if (dwrq->flags & IW_ENCODE_RESTRICTED) {
		/* iwconfig ethX restricted key [1] */
		Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeShared;
		PRINTK1("Authentication mode restricted!\n");
	} else if (dwrq->flags & IW_ENCODE_OPEN) {
		/* iwconfig ethX key [2] open */
		Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeOpen;
		PRINTK1("Authentication mode open!\n");
	}

	/*
	 * If authentication mode changed - de-authenticate, set authentication
	 * method and re-associate if we were previously associated.
	 */
	if (Adapter->SecInfo.AuthenticationMode != PrevAuthMode) {
		if (Adapter->MediaConnectStatus == WlanMediaStateConnected &&
				Adapter->InfrastructureMode == 
				Wlan802_11Infrastructure) {

			/* keep a copy of the ssid associated with */
			memcpy(&ssid, &Adapter->CurBssParams.ssid, sizeof(ssid));

			/*
			 * De-authenticate from AP 
			 */
				
			ret = SendDeauthentication(priv, 
						HostCmd_PENDING_ON_SET_OID);

			if (ret) {
				LEAVE();
				return ret;
			}

		} else {
			/* reset ssid */
			memset(&ssid, 0, sizeof(ssid));
		}
	}

	LEAVE();
	return 0;
}

int wlan_set_encode(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *dwrq, char *extra)
{

	PWLAN_802_11_KEY pKey = NULL;

	ENTER();
	
	if (dwrq->length > MAX_KEY_SIZE) {
		pKey = (PWLAN_802_11_KEY) extra;

		if(pKey->KeyLength <= MAX_KEY_SIZE) {
			//dynamic WEP
			dwrq->length = pKey->KeyLength;
			dwrq->flags = pKey->KeyIndex + 1;
			return wlan_set_encode_nonwpa(dev, info, dwrq, 
					pKey->KeyMaterial);
		}
		else {
#ifdef	WPA
			//WPA
			return wlan_set_encode_wpa(dev, info, dwrq, extra);
#endif
		}
	}
	else {
		//static WEP
		PRINTK1("Setting WEP\n");
		return wlan_set_encode_nonwpa(dev, info, dwrq, extra);
	}

	return -EINVAL;
}

int wlan_set_txpow(struct net_device *dev, struct iw_request_info *info,
			   		struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	wlan_private *priv= dev->priv;
	wlan_adapter *Adapter= priv->adapter;

	int wlan_radio_ioctl(wlan_private *priv, u8 option);

	u16	dbm;
	
	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWTXPOW: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif
	
	if(vwrq->disabled) {
		wlan_radio_ioctl(priv, RADIO_OFF);
		return 0;
	}

	Adapter->Preamble = HostCmd_TYPE_AUTO_PREAMBLE;

	wlan_radio_ioctl(priv, RADIO_ON);

#if WIRELESS_EXT > 14
	if ((vwrq->flags & IW_TXPOW_TYPE) == IW_TXPOW_MWATT) {
		dbm = (u16) mw_to_dbm(vwrq->value);
	}
	else
#endif
		dbm = (u16) vwrq->value;


	/* auto tx power control */

	if (vwrq->fixed == 0)
		dbm = 0xffff;

  	PRINTK1("<1>TXPOWER SET %d dbm.\n", dbm);

	ret = PrepareAndSendCommand(priv, 
		HostCmd_CMD_802_11_RF_TX_POWER,
		HostCmd_ACT_TX_POWER_OPT_SET_LOW,
		HostCmd_OPTION_USE_INT, 0,
		HostCmd_PENDING_ON_GET_OID,
		(void *) &dbm);

	LEAVE();
	return ret;
}

int wlan_scan_networks(wlan_private *priv, u16 pending_info)
{
	wlan_adapter	*Adapter = priv->adapter;
	int 		i;
	int		ret = 0;
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG bc;
#endif /* MULTI BANDS*/

	ENTER();

	Adapter->ulNumOfBSSIDs = 0;

#ifdef BG_SCAN
	if(priv->adapter->bgScanConfig->Enable == TRUE) {
		wlan_bg_scan_enable(priv, FALSE);
	}
#endif /* BG_SCAN */

	for (i = 0; i < sizeof(Adapter->region_channel) / 
			sizeof(Adapter->region_channel[0]); i++) {

#ifdef ENABLE_802_11D
		if( wlan_get_state_11d( priv) == ENABLE_11D &&
			Adapter->MediaConnectStatus != WlanMediaStateConnected) {
/* Scan all the supported chan for the first scan */
			if (!Adapter->universal_channel[i].Valid)
				continue;
			Adapter->cur_region_channel = 
				&(Adapter->universal_channel[i]);

/* clear the parsed_region_chan for the first scan  */
			memset( &Adapter->parsed_region_chan, 0,
					sizeof(parsed_region_chan_11d_t) );
			
		}
		else
#endif
		{
			if (!Adapter->region_channel[i].Valid)
				continue;
			Adapter->cur_region_channel = 
					&(Adapter->region_channel[i]);
		}

		PRINTK2("cur_region_channel: NrCFP=%d\n", 
				Adapter->cur_region_channel->NrCFP );

		ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SCAN,
				0, HostCmd_OPTION_USE_INT, 0,
					pending_info, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}

                do{
			if (interruptible_sleep_on_timeout(&Adapter->scan_q,WAIT_FOR_SCAN_RRESULT_MAX_TIME) == 0)
				printk("Adapter->scan_q: timer expired\n");

                }while(Adapter->ScanChannelsLeft != 0);
	}

#ifdef MULTI_BANDS
	/* To set the band, back to the current operating band */
	/* Need to remove it once the new firmware can handle this */
	if ((Adapter->MediaConnectStatus == WlanMediaStateConnected) &&
							Adapter->is_multiband) {
		bc.BandSelection = Adapter->CurBssParams.band;
		bc.Channel = Adapter->CurBssParams.channel;

		ret = PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_BAND_CONFIG,
				HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_NONE, &bc);

		if (ret) {
			LEAVE();
			return ret;
		}

	}
#endif /* MULTI_BANDS*/

#ifdef BG_SCAN
	if(priv->adapter->bgScanConfig->Enable == TRUE) {
		wlan_bg_scan_enable(priv, TRUE);
	}
#endif /* BG_SCAN */

	LEAVE();
	return 0;
}

/*
 * Wireless Handler: Initiate scan 
 */
int wlan_set_scan(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	union iwreq_data	wrqu;

	ENTER();

#ifdef BG_SCAN
	/* Return immediately if BG_SCAN is enabled */
	if(priv->adapter->bgScanConfig->Enable) 
		return 0;
#endif /* BG_SCAN */

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWSCAN: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	if (!Adapter->bIsScanInProgress) {
//		Adapter->ulNumOfBSSIDs = 0;

		memset(Adapter->BSSIDList, 0,
			sizeof(WLAN_802_11_BSSID) * 
			MRVDRV_MAX_BSSID_LIST);

		Adapter->bAutoAssociation = FALSE;
		if (!wlan_scan_networks(priv, HostCmd_PENDING_ON_NONE)) {
			memset(&wrqu, 0, sizeof(union iwreq_data));
#ifdef linux
			wireless_send_event(priv->wlan_dev.netdev, SIOCGIWSCAN, &wrqu , NULL);
#endif
		}
	}

	if (Adapter->SurpriseRemoved)
		return -1;

	LEAVE();
	return 0;
}

/* Added for Subcommand Implementation MPS 2004/08/16. */

int SetRxAntenna(wlan_private *priv, int Mode)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	if (Mode != RF_ANTENNA_1 && Mode != RF_ANTENNA_2
					&& Mode != RF_ANTENNA_AUTO) {
		return -EINVAL;
	}

	Adapter->RxAntennaMode = Mode;

	PRINTK1("SET RX Antenna Mode to 0x%04x\n", Adapter->RxAntennaMode);
			
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RF_ANTENNA,
			HostCmd_ACT_SET_RX, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, &Adapter->RxAntennaMode);
	return ret;
}

int SetTxAntenna(wlan_private *priv, int Mode)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	if ((Mode != RF_ANTENNA_1) && (Mode != RF_ANTENNA_2)
			&& (Mode != RF_ANTENNA_AUTO)) {
		return -EINVAL;
	}
		
	Adapter->TxAntennaMode = Mode; 

	PRINTK1("SET TX Antenna Mode to 0x%04x\n", Adapter->TxAntennaMode);

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RF_ANTENNA,
			HostCmd_ACT_SET_TX, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, &Adapter->TxAntennaMode);

	return ret;
}

int GetRxAntenna(wlan_private *priv, char *buf)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	// clear it, so we will know if the value 
	// returned below is correct or not.
	Adapter->RxAntennaMode = 0;	

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RF_ANTENNA,
			HostCmd_ACT_GET_RX, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	PRINTK1("Get Rx Antenna Mode:0x%04x\n", Adapter->RxAntennaMode);

	LEAVE();
			
	return sprintf(buf, "0x%04x", Adapter->RxAntennaMode) + 1;
}

int GetTxAntenna(wlan_private *priv, char *buf)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
	
	// clear it, so we will know if the value 
	// returned below is correct or not.
	Adapter->TxAntennaMode = 0;	

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RF_ANTENNA,
			HostCmd_ACT_GET_TX, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);		

	if (ret) {
		LEAVE();
		return ret;
	}

	PRINTK1("Get Tx Antenna Mode:0x%04x\n", Adapter->TxAntennaMode);

	LEAVE();

	return sprintf(buf, "0x%04x", Adapter->TxAntennaMode) + 1;
}

/* End of Addition for Subcommand Implementation  2004/08/16 MPS */		

int wlan_send_deauth(wlan_private *priv)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;
	
	ENTER();
	
    	if (Adapter->InfrastructureMode == Wlan802_11Infrastructure &&
		Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		
		ret = SendDeauthentication(priv, HostCmd_PENDING_ON_NONE);

	} else {
		LEAVE();
		return -ENOTSUPP;
	}

	LEAVE();
	return ret;
}

int wlan_do_adhocstop_ioctl(wlan_private *priv)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	if (Adapter->InfrastructureMode == Wlan802_11IBSS && 
		Adapter->MediaConnectStatus == WlanMediaStateConnected) {

		ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

	} else {
		LEAVE();
		return -ENOTSUPP;
	}

	LEAVE();
	
	return 0;
		
}

int wlan_radio_ioctl(wlan_private *priv, u8 option)
{
	int		ret = 0;
	wlan_adapter	*Adapter	= priv->adapter;

	ENTER();

	if (Adapter->RadioOn != option) {
		PRINTK1("Switching %s the Radio\n", option ? "On" : "Off");
		Adapter->RadioOn = option;
		
		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_RADIO_CONTROL,
				HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
			      	, 0, HostCmd_PENDING_ON_GET_OID, NULL);
	}

	LEAVE();
	return ret;
}

int reassociation_on(wlan_private *priv)
{
	wlan_adapter	*Adapter	= priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>():%s IOCTLS called when station is"
					" in DeepSleep\n",__FUNCTION__);
		return -EBUSY;
	}
#endif

	Adapter->Reassoc_on = TRUE;

	LEAVE();
	return 0;
}

int reassociation_off(wlan_private *priv)
{
	wlan_adapter		*Adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>():%s IOCTLS called when station is"
					" in DeepSleep\n",__FUNCTION__);
		return -EBUSY;
	}
#endif
	
	if (Adapter->TimerIsSet == TRUE) {
		CancelTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}	

	Adapter->Reassoc_on = FALSE;
	
	LEAVE();
	return 0;
}

/* wlanidle is off */
int wlanidle_off(wlan_private *priv)
{
	int			ret = 0;
	wlan_adapter		*Adapter = priv->adapter;
	WLAN_802_11_SSID	*AdhocSsid;

	ENTER();

	if (Adapter->MediaConnectStatus == WlanMediaStateDisconnected 
			&& Adapter->PreviousSSID.Ssid[0] != '\0') {
		if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
			PRINTK1("Previous SSID = %s\n", 
					Adapter->PreviousSSID.Ssid);
			
			ret = wlan_associate(priv, &Adapter->PreviousSSID);

		} else if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
	
			/* Copy the current ssid */
			memcpy(&AdhocSsid, &Adapter->PreviousSSID, 
						sizeof(WLAN_802_11_SSID));
			ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_AD_HOC_START,
				0, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_SET_OID, AdhocSsid);
		}
	}
	/* else it is connected */
	
	PRINTK("\nwlanidle is off");
	LEAVE();
	return ret;
}

/* wlanidle is on */
int wlanidle_on(wlan_private *priv)
{
	int			ret = 0;
	wlan_adapter		*Adapter = priv->adapter;

	ENTER();

	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
			PRINTK1("Previous SSID = %s\n", 
					Adapter->PreviousSSID.Ssid);
			memmove(&Adapter->PreviousSSID, 
					&Adapter->CurBssParams.ssid,
						sizeof(WLAN_802_11_SSID));
 			wlan_send_deauth(priv);
		
		} else if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
			
			ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

		}

	}

	if (Adapter->TimerIsSet == TRUE) {
		CancelTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}	
	PRINTK("\nwlanidle is on");

	LEAVE();
	return ret;
}

int wlan_rfi_txmode(wlan_private *priv, struct iwreq *wrq)
{
	int			ret = 0;
	HostCmd_802_11_RFI	rfi;
		
	ENTER();

	if (wrq->u.data.pointer){
		
		switch ((int)*wrq->u.data.pointer) {

			case TX_MODE_NORMAL :
				rfi.Mode	= TX_MODE_NORMAL;
				ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_TX_MODE,
						0, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP
						, 0, HostCmd_PENDING_ON_NONE,
						&rfi);
				
				break;
				
			case TX_MODE_CONT :	/* continuous modulated data */
				rfi.Mode	= TX_MODE_CONT;
				ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_TX_MODE,
						0, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP
						, 0, HostCmd_PENDING_ON_NONE,
						&rfi);
				
				break;
				
			case TX_MODE_CW :	/* contention window */
				rfi.Mode	= TX_MODE_CW;
				ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_TX_MODE,
						0, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP
						, 0, HostCmd_PENDING_ON_NONE,
						&rfi);
				break;

			default:
				return -ENOTSUPP;
		}

	} else
		return -EINVAL;

	LEAVE();
	return ret;

}

int wlan_rfi_txcontrolmode(wlan_private *priv, struct iwreq *wrq)
{
	int			ret = 0;
	HostCmd_802_11_RFI	rfi;
	int			mode;

	ENTER();

	if (wrq->u.data.pointer){ 
		mode = (int)*wrq->u.data.pointer;
		switch (mode) {
			case TX_MODE_EXIT :	/* exit test mode */
				rfi.Mode	= mode;
				ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_TX_CONTROL_MODE,
						0, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP
						, 0, HostCmd_PENDING_ON_NONE,
						&rfi);
				break;
				
			case TX_MODE_ENTER :	/* enter test mode */
				rfi.Mode	= mode;
				ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_TX_CONTROL_MODE,
						0, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP
						, 0, HostCmd_PENDING_ON_NONE,
						&rfi);
				break;

			default:
				return -ENOTSUPP;
		}

	} else
		return -EINVAL;

	LEAVE();
	return ret;
}

int wlan_set_region(wlan_private *priv, u16 region_code)
{
	int i;

	ENTER();
	
	for (i = 0; i < MRVDRV_MAX_REGION_CODE; i++) {
		// use the region code to search for the index
		if (region_code == RegionCodeToIndex[i]) {
			priv->adapter->RegionTableIndex = (u16) i;
			priv->adapter->RegionCode = region_code;
			break;
		}
	}

	// if it's unidentified region code
	if (i >= MRVDRV_MAX_REGION_CODE) {
		PRINTK1("Region Code not identified\n");
		LEAVE();
		return -1;
	}

#ifdef MULTI_BANDS
	if (wlan_set_regiontable(priv, priv->adapter->RegionCode, 
					priv->adapter->config_bands)) {
#else
	if (wlan_set_regiontable(priv, priv->adapter->RegionCode, 0)) {
#endif
		LEAVE();
		return -EINVAL;
	}

	LEAVE();
	return 0;
}
int wlan_get_essid(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *dwrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
	/*
	 * Note : if dwrq->flags != 0, we should get the relevant SSID from
	 * the SSID list... 
	 */

	/*
	 * Get the current SSID 
	 */
	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		memcpy(extra, Adapter->CurBssParams.ssid.Ssid,
				Adapter->CurBssParams.ssid.SsidLength);
		extra[Adapter->CurBssParams.ssid.SsidLength] = '\0';
	} else {
		memset(extra, 0, 32);
		extra[Adapter->CurBssParams.ssid.SsidLength] = '\0';
	}
	/*
	 * If none, we may want to get the one that was set 
	 */

	/* To make the driver backward compatible with WPA supplicant v0.2.4 */
	if (dwrq->length == 32)  /* check with WPA supplicant buffer size */
		dwrq->length = MIN(Adapter->CurBssParams.ssid.SsidLength, 
							IW_ESSID_MAX_SIZE);
	else 		
		dwrq->length = Adapter->CurBssParams.ssid.SsidLength + 1;

	dwrq->flags = 1;		/* active */

	LEAVE1();
	return 0;
}

static inline int CopyRates(u8 *dest, int pos, u8 *src, int len)
{
	int	i;

	for (i = 0; i < len && src[i]; i++, pos++) {
		if (pos >= sizeof(WLAN_802_11_RATES))
			break;
		dest[pos] = src[i];
	}

	return pos;
}

static int get_active_data_rates(wlan_adapter *Adapter, WLAN_802_11_RATES rates)
{
	int	k = 0;

	ENTER();
	
	if (Adapter->MediaConnectStatus != WlanMediaStateConnected) {
		if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {	
			//Infra. mode
#ifdef	MULTI_BANDS
			if (Adapter->config_bands & BAND_G) {
				PRINTK("Infra mBAND_G=%#x\n", BAND_G);
				k = CopyRates(rates, k, SupportedRates_G, 
						sizeof(SupportedRates_G));
			} else {
				if (Adapter->config_bands & BAND_B) {
					PRINTK("Infra mBAND_B=%#x\n", BAND_B);
					k = CopyRates(rates, k, 
						SupportedRates_B, 
						sizeof(SupportedRates_B));
				}
				if (Adapter->config_bands & BAND_A) {
					PRINTK("Infra mBAND_A=%#x\n", BAND_A);
					k = CopyRates(rates, k, 
						SupportedRates_A, 
						sizeof(SupportedRates_A));
				}
			}
#else //MULTI_BANDS
			PRINTK("Infra\n");
			k = CopyRates(rates, k, SupportedRates, 
						sizeof(SupportedRates));
#endif //MULTI_BANDS
		} else {
			//ad-hoc mode
#ifdef	MULTI_BANDS
			if (Adapter->config_bands & BAND_G) {
				PRINTK("Adhoc mBAND_G=%#x\n", BAND_G);
				k = CopyRates(rates, k, AdhocRates_G, 
							sizeof(AdhocRates_G));
			} else if (Adapter->config_bands & BAND_B) {
				PRINTK("Adhoc mBAND_B=%#x\n", BAND_B);
				k = CopyRates(rates, k, AdhocRates_B, 
							sizeof(AdhocRates_B));
			}
			if (Adapter->config_bands & BAND_A) {
				PRINTK("Adhoc mBAND_A=%#x\n", BAND_A);
				k = CopyRates(rates, k, AdhocRates_A, 
							sizeof(AdhocRates_A));
			}
#else //MULTI_BANDS
#ifdef G_RATE
			PRINTK("Adhoc G\n");
			k = CopyRates(rates, k, AdhocRates_G, 
							sizeof(AdhocRates_G));
#else //G_RATE
			PRINTK("Adhoc B\n");
			k = CopyRates(rates, k, AdhocRates_B, 
							sizeof(AdhocRates_B));
#endif //G_RATE
#endif //MULTI_BANDS
		}
	} else {
		k = CopyRates(rates, 0, Adapter->CurBssParams.DataRates, 
					Adapter->CurBssParams.NumOfRates);
	}

	LEAVE();

	return k;
}

/* Get/Set NULL Package generation interval */
int wlan_null_pkt_interval(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter	*Adapter = priv->adapter;
	int data;	
	ENTER();

	if((int) wrq->u.data.length == 0) {
	    data = Adapter->NullPktInterval;
	    
	    if(copy_to_user(wrq->u.data.pointer, &data, sizeof(int))) {
    		PRINTK1("copy_to_user failed!\n");
	    	return -EFAULT;
	    }
	} else {
	     if((int) wrq->u.data.length > 1) {
	  	    PRINTK1("ioctl too many args!\n");
	   	    return -EFAULT;
	     }
	     copy_from_user(&data,wrq->u.data.pointer,sizeof(int));
	     Adapter->NullPktInterval = data;
	}

	wrq->u.data.length = 1;
			
	LEAVE();
	return WLAN_STATUS_SUCCESS;
}


#ifdef	linux
#include	"wlan_version.h"

#define VALID_SRANGE	(1 * HZ)
#define GETLOG_BUFSIZE  300

#define MAX_SCAN_CELL_SIZE      (IW_EV_ADDR_LEN + \
				MRVDRV_MAX_SSID_LENGTH + \
				IW_EV_UINT_LEN + IW_EV_FREQ_LEN + \
				IW_EV_QUAL_LEN + MRVDRV_MAX_SSID_LENGTH + \
				IW_EV_PARAM_LEN + 40) /* 40 for WPAIE */

void get_version(wlan_adapter *adapter, char *version, int maxlen)
{
	union {
		u32	l;
		u8	c[4];
	} ver;
	char	fwver[32];

	ver.l = adapter->FWReleaseNumber;
	if(ver.c[3] == 0)
		sprintf(fwver, "%u.%u.%u",
			ver.c[2], ver.c[1], ver.c[0]);
	else
		sprintf(fwver, "%u.%u.%u.p%u",
			ver.c[2], ver.c[1], ver.c[0],ver.c[3]);

	snprintf(version, maxlen, driver_version, fwver);
}

/*
 * Commit handler: called after a bunch of SET operations 
 */
static int wlan_config_commit(struct net_device *dev, 
		struct iw_request_info *info, 
		char *cwrq, char *extra)
{
	ENTER();

	LEAVE();
	return 0;
}

/*
 * Wireless Handler: get protocol name 
 */
static int wlan_get_name(struct net_device *dev, struct iw_request_info *info, 
		char *cwrq, char *extra)
{
	const char *cp;
	char comm[6] = {"COMM-"};
	char mrvl[6] = {"MRVL-"};
	int cnt;

	ENTER();

//	strcpy(cwrq, "IEEE802.11-DS");

	strcpy(cwrq, mrvl);

	cp = strstr(driver_version, comm);
	if (cp == driver_version)	//skip leading "COMM-"
		cp = driver_version + strlen(comm);
	else
		cp = driver_version;

	cnt = strlen(mrvl);
	cwrq += cnt;
	while (cnt < 16 && (*cp != '-')) {
		*cwrq++ = toupper(*cp++);
		cnt++;
	}
	*cwrq = '\0';

	LEAVE();

	return 0;
}

static int wlan_get_freq(struct net_device *dev, struct iw_request_info *info, 
		struct iw_freq *fwrq, char *extra)
{
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	CHANNEL_FREQ_POWER	*cfp;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCGIWFREQ: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

#ifdef MULTI_BANDS
	cfp = find_cfp_by_band_and_channel(Adapter, Adapter->CurBssParams.band, 
						Adapter->CurBssParams.channel);
#else
	cfp = find_cfp_by_band_and_channel(Adapter, 0, 
						Adapter->CurBssParams.channel);
#endif
	
	if (!cfp) {
		if (Adapter->CurBssParams.channel)
			PRINTK("Invalid channel=%d\n", 
					Adapter->CurBssParams.channel);
		return -EINVAL;
	} 
	
	fwrq->m = (long) cfp->Freq * 100000;
	fwrq->e = 1;

	PRINTK("freq=%u\n",fwrq->m);

	LEAVE1();
	return 0;
}


static int wlan_set_wap(struct net_device *dev,	struct iw_request_info *info,
		struct sockaddr *awrq, char *extra)
{
	int			ret = 0;
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	static const u8 	bcast[ETH_ALEN] = 
					{ 255, 255, 255, 255, 255, 255 };
	WLAN_802_11_SSID	SSID;
	u8			reqBSSID[ETH_ALEN];	
	int 			i = 0;
	
	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWAP: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	//TODO: Check if the card is still there.

	if (awrq->sa_family != ARPHRD_ETHER)
		return -EINVAL;			

	PRINTK1("sa_data: %02x:%02x:%02x:%02x:%02x:%02x\n",
			awrq->sa_data[0], awrq->sa_data[1],
			awrq->sa_data[2], awrq->sa_data[3],
			awrq->sa_data[4], awrq->sa_data[5]);
	
	// cancel re-association timer if there's one
	if (Adapter->TimerIsSet == TRUE) {
		CancelTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}

	memset(&SSID, 0, sizeof(WLAN_802_11_SSID));
	
//	wlan_scan_networks(priv, HostCmd_PENDING_ON_SET_OID);

	if (!memcmp(bcast, awrq->sa_data, ETH_ALEN)) {
		i = FindBestSSIDInList(Adapter);
		if (i >= 0) {
			memcpy(reqBSSID, Adapter->BSSIDList[i].MacAddress, 
					ETH_ALEN);
		}
	} else {
		memcpy(reqBSSID, awrq->sa_data, ETH_ALEN);

		PRINTK1("Required bssid = %x:%x:%x:%x:%x:%x\n", 
				reqBSSID[0], reqBSSID[1],
				reqBSSID[2], reqBSSID[3],
				reqBSSID[4], reqBSSID[5]);
		
		// Search for index position in list for requested MAC
		i = FindBSSIDInList(Adapter, reqBSSID, 
				Adapter->InfrastructureMode);
  
	// If BSSID not found, do scan one more time.

		if (i < 0)
		{
			wlan_scan_networks(priv, HostCmd_PENDING_ON_SET_OID);

  			i = FindBSSIDInList(Adapter, reqBSSID, 
				Adapter->InfrastructureMode);
		}
	}	
	
	if (i >= 0) {
		memcpy(&SSID, &Adapter->BSSIDList[i].Ssid, sizeof(SSID));
	} else {
		PRINTK1("MAC address not found in BSSID List\n");
		return -ENETUNREACH;
	}

	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		ret = SendDeauthentication(priv, HostCmd_PENDING_ON_NONE);

		if (ret) {
			LEAVE();
			return ret;
		}
	}
	
	Adapter->RequestViaBSSID = TRUE;
	memcpy(Adapter->RequestedBSSID, reqBSSID, ETH_ALEN);
	
	if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
		ret = wlan_associate(priv, &SSID);

		if (ret) {
			LEAVE();
			return ret;
		}
	} else {
		if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
			/* Exit Adhoc mode */
			ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

			if (ret) {
				LEAVE();
				return ret;
			}
		}

		JoinAdhocNetwork(priv, &SSID, i);
	}

	LEAVE();
	return 0;
}

static int wlan_get_wap(struct net_device *dev, struct iw_request_info *info, 
		struct sockaddr *awrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		memcpy(awrq->sa_data, Adapter->CurrentBSSID, ETH_ALEN);
	} else {
		memset(awrq->sa_data, 0, ETH_ALEN);
	}
	awrq->sa_family = ARPHRD_ETHER;

	LEAVE1();
	return 0;
}

static int wlan_set_nick(struct net_device *dev, struct iw_request_info *info, 
		struct iw_point *dwrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	/*
	 * Check the size of the string 
	 */
	
	if (dwrq->length > 16) {
		return -E2BIG;
	}

	memset(Adapter->nodeName, 0, sizeof(Adapter->nodeName));
	memcpy(Adapter->nodeName, extra, dwrq->length);

	LEAVE();
	return 0;
}

static int wlan_get_nick(struct net_device *dev, struct iw_request_info *info, 
		struct iw_point *dwrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	/*
	 * Get the Nick Name saved 
	 */

	strncpy(extra, Adapter->nodeName, 16);

	extra[16] = '\0';

	/*
	 * If none, we may want to get the one that was set 
	 */

	/*
	 * Push it out ! 
	 */
	dwrq->length = strlen(extra) + 1;

	LEAVE1();
	return 0;
}

static int wlan_set_rts(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	int		rthr = vwrq->value;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWRTS: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	if (vwrq->disabled) {
		Adapter->RTSThsd = rthr = MRVDRV_RTS_MAX_VALUE;
	} else {
		if (rthr < MRVDRV_RTS_MIN_VALUE || rthr > MRVDRV_RTS_MAX_VALUE)
			return -EINVAL;
		Adapter->RTSThsd = rthr;
	}

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, OID_802_11_RTS_THRESHOLD,
			HostCmd_PENDING_ON_SET_OID, &rthr);

	LEAVE();
	return ret;
}

static int wlan_get_rts(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCGIWRTS: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	if (!Adapter->RTSThsd) {
		ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
				0, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, OID_802_11_RTS_THRESHOLD,
				HostCmd_PENDING_ON_GET_OID, NULL);
		if (ret) {
			LEAVE();
			return ret;
		}
	}

	vwrq->value = Adapter->RTSThsd;
	vwrq->disabled = ((vwrq->value < MRVDRV_RTS_MIN_VALUE)
			 || (vwrq->value > MRVDRV_RTS_MAX_VALUE));
	vwrq->fixed = 1;

	LEAVE();
	return 0;
}

static int wlan_set_frag(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	int		fthr = vwrq->value;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWFRAG: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	if (vwrq->disabled) {
		Adapter->FragThsd = fthr = MRVDRV_FRAG_MAX_VALUE;
	} else {
		if (fthr < MRVDRV_FRAG_MIN_VALUE || fthr > MRVDRV_FRAG_MAX_VALUE)
			return -EINVAL;
		Adapter->FragThsd = fthr;
	}

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, OID_802_11_FRAGMENTATION_THRESHOLD,
			HostCmd_PENDING_ON_SET_OID, &fthr);
	LEAVE();
	return ret;
}

static int wlan_get_frag(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCGIWFRAG: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	if (!Adapter->FragThsd) {
		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_SNMP_MIB, 0,
				HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, OID_802_11_FRAGMENTATION_THRESHOLD,
				HostCmd_PENDING_ON_GET_OID, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}
	}

	vwrq->value = Adapter->FragThsd;
	vwrq->disabled = ((vwrq->value < MRVDRV_FRAG_MIN_VALUE)
			 || (vwrq->value > MRVDRV_FRAG_MAX_VALUE));
	vwrq->fixed = 1;

	LEAVE();
	return ret;
}

static int wlan_get_mode(struct net_device *dev,
	      struct iw_request_info *info, u32 * uwrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*adapter = priv->adapter;

	ENTER();

	switch (adapter->InfrastructureMode) {
		case Wlan802_11IBSS:
			*uwrq = IW_MODE_ADHOC;
			break;

		case Wlan802_11Infrastructure:
			*uwrq = IW_MODE_INFRA;
			break;

		default:
		case Wlan802_11AutoUnknown:
			*uwrq = IW_MODE_AUTO;
			break;
	}

	LEAVE1();
	return 0;
}

static int wlan_get_encode(struct net_device *dev,
			struct iw_request_info *info,
				struct iw_point *dwrq, u8 * extra)
{

	wlan_private	*priv = dev->priv;
	wlan_adapter	*adapter = priv->adapter;
	int		index = (dwrq->flags & IW_ENCODE_INDEX);

	ENTER();

	PRINTK1("flags=0x%x index=%d length=%d CurrentWepKeyIndex=%d\n", 
			dwrq->flags, index, dwrq->length, 
			adapter->CurrentWepKeyIndex);

	dwrq->flags = 0;

	/*
	 * Check encryption mode 
	 */

	switch (adapter->SecInfo.AuthenticationMode) {
		case Wlan802_11AuthModeOpen:
			dwrq->flags = IW_ENCODE_OPEN;
			break;

		case Wlan802_11AuthModeShared:
		case Wlan802_11AuthModeNetworkEAP:
			dwrq->flags = IW_ENCODE_RESTRICTED;
			break;
		default:
			dwrq->flags = IW_ENCODE_DISABLED | IW_ENCODE_OPEN;
			break;
	}

	if ((adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled)
		|| (adapter->SecInfo.WEPStatus == Wlan802_11WEPKeyAbsent)
#ifdef WPA
		|| adapter->SecInfo.WPAEnabled
#endif
#ifdef WPA2
		|| adapter->SecInfo.WPA2Enabled
#endif
		) {
		dwrq->flags &= ~IW_ENCODE_DISABLED;
	} else {
		dwrq->flags |= IW_ENCODE_DISABLED;
	}

	memset(extra, 0, 16);

	if (!index) {
		/* Handle current key request	*/
		if ((adapter->WepKey[adapter->CurrentWepKeyIndex].KeyLength) &&
				(adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled)) {
			index = adapter->WepKey[adapter->CurrentWepKeyIndex].KeyIndex;
			memcpy(extra, adapter->WepKey[index].KeyMaterial,
					adapter->WepKey[index].KeyLength);
			dwrq->length = adapter->WepKey[index].KeyLength;
			/* return current key */
			dwrq->flags |= (index + 1);
			/* return WEP enabled */
			dwrq->flags &= ~IW_ENCODE_DISABLED;
#ifdef WPA
		} else if ((adapter->SecInfo.WPAEnabled)
#ifdef WPA2
		|| (adapter->SecInfo.WPA2Enabled)
#endif
		) {
			/* return WPA enabled */
			dwrq->flags &= ~IW_ENCODE_DISABLED;
#endif
		} else {
			dwrq->flags |= IW_ENCODE_DISABLED;
		}
	} else {
		/* Handle specific key requests */
		index--;
		if (adapter->WepKey[index].KeyLength) {
			memcpy(extra, adapter->WepKey[index].KeyMaterial,
					adapter->WepKey[index].KeyLength);
			dwrq->length = adapter->WepKey[index].KeyLength;
			/* return current key */
			dwrq->flags |= (index + 1);
			/* return WEP enabled */
			dwrq->flags &= ~IW_ENCODE_DISABLED;
#ifdef WPA
		} else if ((adapter->SecInfo.WPAEnabled)
#ifdef WPA2
		|| (adapter->SecInfo.WPA2Enabled)
#endif
		) {
			/* return WPA enabled */
			dwrq->flags &= ~IW_ENCODE_DISABLED;
#endif
		} else {
			dwrq->flags |= IW_ENCODE_DISABLED;
		}
	}

	dwrq->flags |= IW_ENCODE_NOKEY;

	PRINTK1("Key:%02x:%02x:%02x:%02x:%02x:%02x KeyLen=%d\n",
			extra[0], extra[1], extra[2],
			extra[3], extra[4], extra[5], dwrq->length);

#ifdef WPA
	if (adapter->EncryptionStatus == Wlan802_11Encryption2Enabled
			&& !dwrq->length) {
		dwrq->length = MAX_KEY_SIZE;
	}
#endif

//	if (dwrq->length > 16) {
//		dwrq->length = 0;
//	}

	PRINTK1("Return flags=0x%x\n", dwrq->flags);

	LEAVE();
	return 0;
}

static int wlan_get_txpow(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCGIWTXPOW: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

//  We want the real value every time.
//	if (!Adapter->TxPowerLevel) {
		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_RF_TX_POWER,
					HostCmd_ACT_TX_POWER_OPT_GET,
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
				      	, 0, HostCmd_PENDING_ON_GET_OID, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}
//	}

  	PRINTK1("TXPOWER GET %d dbm.\n", Adapter->TxPowerLevel);
	vwrq->value = Adapter->TxPowerLevel;
	vwrq->fixed = 1;	/* No power control  Need to verify */
	if (Adapter->RadioOn) {
		vwrq->disabled = 0;
		vwrq->flags = IW_TXPOW_DBM;
	} else {
		vwrq->disabled = 1;
	}

	LEAVE();
	return 0;
}

static int wlan_set_retry(struct net_device *dev, struct iw_request_info *info,
	       struct iw_param *vwrq, char *extra)
{
	int		ret = 0;
	wlan_private   *priv = dev->priv;
	wlan_adapter   *adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWRETRY: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	if (vwrq->flags == IW_RETRY_LIMIT) {
		if (vwrq->value < 1 || vwrq->value > 16)
			return -EINVAL;

		/* Adding 1 to convert retry count to try count */
		adapter->TxRetryCount = vwrq->value + 1;

		ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB, 
				0, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, OID_802_11_TX_RETRYCOUNT,
				HostCmd_PENDING_ON_SET_OID, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}
	} else {
		return -EOPNOTSUPP;
	}

	LEAVE();
	return 0;
}

static int wlan_get_retry(struct net_device *dev, struct iw_request_info *info,
		struct iw_param *vwrq, char *extra)
{
	wlan_private   *priv = dev->priv;
	wlan_adapter   *adapter = priv->adapter;

	ENTER();

	vwrq->disabled = 0;
	if (!vwrq->flags) {
		vwrq->flags = IW_RETRY_LIMIT;
		/* Subtract 1 to convert try count to retry count */
		vwrq->value = adapter->TxRetryCount - 1;
	}

	LEAVE1();
	return 0;
}

static inline void sort_channels(struct iw_freq *freq, int num)
{
	int i, j;
	struct iw_freq temp;

	for (i = 0; i < num; i++)
		for (j = i + 1; j < num; j++)
			if (freq[i].i > freq[j].i) {
				temp.i = freq[i].i;
				temp.m = freq[i].m;
				
				freq[i].i = freq[j].i;
				freq[i].m = freq[j].m;
				
				freq[j].i = temp.i;	
				freq[j].m = temp.m;	
			}
}



/* data rate listing
	MULTI_BANDS:
		abg		a	b	b/g
   Infra 	G(12)		A(8)	B(4)	G(12)
   Adhoc 	A+B(12)		A(8)	B(4)	B(4)

	non-MULTI_BANDS:
		   		 	b	b/g
   Infra 	     		    	B(4)	G(12)
   Adhoc 	      		    	B(4)	B(4)
 */
static int wlan_get_range(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *dwrq, char *extra)
{
	int			i, j;
	wlan_private		*priv = dev->priv;
	wlan_adapter		*Adapter = priv->adapter;
	struct iw_range		*range = (struct iw_range *) extra;
	CHANNEL_FREQ_POWER	*cfp;
	WLAN_802_11_RATES	rates;

#ifdef ENABLE_802_11D
	BOOLEAN		flag = FALSE;
#endif

	ENTER();

	dwrq->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->min_nwid = 0;
	range->max_nwid = 0;

	memset(rates, 0, sizeof(rates));
	range->num_bitrates = get_active_data_rates(Adapter, rates);

	for (i=0; i < MIN(range->num_bitrates,IW_MAX_BITRATES) && rates[i]; i++) {
		range->bitrate[i] = (rates[i] & 0x7f) * 500000;
	}
	range->num_bitrates = i;
	PRINTK("IW_MAX_BITRATES=%d num_bitrates=%d\n",IW_MAX_BITRATES,range->num_bitrates);

	range->num_frequency = 0;
#ifdef ENABLE_802_11D
	if( wlan_get_state_11d( priv) == ENABLE_11D &&
			Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		u16 chan_no;
		u8 band;
		
		parsed_region_chan_11d_t *parsed_region_chan = &Adapter->parsed_region_chan;

		if( parsed_region_chan == NULL ) {
			PRINTK2("11D:parsed_region_chan is NULL\n");
			LEAVE();
			return 0;
		}
		band = parsed_region_chan->band;
		PRINTK2("band=%d NoOfChan=%d\n", band, parsed_region_chan->NoOfChan);

		for (i = 0; (range->num_frequency < IW_MAX_FREQUENCIES) 
				&& (i < parsed_region_chan->NoOfChan); i++) {
			chan_no = parsed_region_chan->chanPwr[i].chan;
			PRINTK2("chan_no=%d\n", chan_no );
			range->freq[range->num_frequency].i = (long) chan_no;
			range->freq[range->num_frequency].m = 
					(long) chan_2_freq(chan_no, band) * 100000;
			range->freq[range->num_frequency].e = 1;
			range->num_frequency++;
		}
		flag = TRUE;
	}
	if ( !flag )
#endif
	{
		for (j = 0; (range->num_frequency < IW_MAX_FREQUENCIES) 
			&& (j < sizeof(Adapter->region_channel) 
				/ sizeof(Adapter->region_channel[0]));
				j++) {
			cfp = Adapter->region_channel[j].CFP;
			for (i = 0; (range->num_frequency < IW_MAX_FREQUENCIES) 
				&& Adapter->region_channel[j].Valid 
				&& cfp 
				&& (i < Adapter->region_channel[j].NrCFP);
					i++) {
				range->freq[range->num_frequency].i = (long) cfp->Channel;
				range->freq[range->num_frequency].m = (long) cfp->Freq * 100000;
				range->freq[range->num_frequency].e = 1;
				cfp++;
				range->num_frequency++;
			}
		}
	}

	PRINTK("IW_MAX_FREQUENCIES=%d num_frequency=%d\n",IW_MAX_FREQUENCIES,
							range->num_frequency);

	range->num_channels = range->num_frequency;
	
	sort_channels(&range->freq[0], range->num_frequency);
	
	/*
	 * Set an indication of the max TCP throughput in bit/s that we can
	 * expect using this interface 
	 */
	if (i > 2)
		range->throughput = 5000 * 1000;
	else
		range->throughput = 1500 * 1000;

	range->min_rts = MRVDRV_RTS_MIN_VALUE;
	range->max_rts = MRVDRV_RTS_MAX_VALUE;
	range->min_frag = MRVDRV_FRAG_MIN_VALUE;
	range->max_frag = MRVDRV_FRAG_MAX_VALUE;

	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;
	range->num_encoding_sizes = 2;
	range->max_encoding_tokens = 4;

	range->min_pmp = 1000000;
	range->max_pmp = 120000000;	
	range->min_pmt = 1000;		
	range->max_pmt = 1000000;	
	range->pmp_flags = IW_POWER_PERIOD;
	range->pmt_flags = IW_POWER_TIMEOUT;
	range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_ALL_R;

	/*
	 * Minimum version we recommend 
	 */
	range->we_version_source = 15;

	/*
	 * Version we are compiled with 
	 */
	range->we_version_compiled = WIRELESS_EXT;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT | IW_RETRY_MAX;

	range->min_retry = 1;
	range->max_retry = 16;	// 65535

	/*
	 * Set the qual, level and noise range values 
	 */
	/*
	 * need to put the right values here 
	 */
	range->max_qual.qual = 10;
	range->max_qual.level = 0;	//156;
	range->max_qual.noise = 0;	//156;
	range->sensitivity = 0;
	// range->sensitivity = 65535;

	/*
	 * Setup the supported power level ranges 
	 */
	memset(range->txpower, 0, sizeof(range->txpower));
	range->txpower[0] = 5;
	range->txpower[1] = 7;
	range->txpower[2] = 9;
	range->txpower[3] = 11;
	range->txpower[4] = 13;
	range->txpower[5] = 15;
	range->txpower[6] = 17;
	range->txpower[7] = 19;

	range->num_txpower = 8;
	range->txpower_capa = IW_TXPOW_DBM;
#ifdef WPA
	range->txpower_capa |= IW_TXPOW_RANGE;
#endif

	LEAVE1();
	return 0;
}

#ifdef PS_REQUIRED
/*
 * Set power management 
 */
#ifdef WPRM_DRV
int wlan_set_power(struct net_device *dev, struct iw_request_info *info,
                struct iw_param *vwrq, char *extra)
#else
static int wlan_set_power(struct net_device *dev, struct iw_request_info *info,
		struct iw_param *vwrq, char *extra)
#endif
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCSIWPOWER: IOCTLS called when station is"
							" in DeepSleep\n");
		return -EBUSY;
	}
#endif

	/* PS is currently supported only in Infrastructure Mode 
	 * Remove this check if it is to be supported in IBSS mode also 
	 */
#ifndef AD_HOC_PS
	if (Adapter->InfrastructureMode != Wlan802_11Infrastructure)
		return -EINVAL;
#endif

	if (vwrq->disabled) {
		Adapter->PSMode = Wlan802_11PowerModeCAM;
		if (Adapter->PSState != PS_STATE_FULL_POWER) {
			PSWakeup(priv, HostCmd_OPTION_WAITFORRSP);
		}

		return 0;
	}

	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		PRINTK("Setting power timeout command is not supported\n");
		return -EINVAL;
	} else if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_PERIOD) {
		PRINTK("Setting power period command is not supported\n");
		return -EINVAL;
	}

	if (Adapter->PSMode != Wlan802_11PowerModeCAM) {
		return 0;
	}
     
	Adapter->PSMode = Wlan802_11PowerModeMAX_PSP;
     
	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
		PSSleep(priv, Adapter->PSMode, HostCmd_OPTION_WAITFORRSP);
	}

	LEAVE();
	return 0;
}

static int wlan_get_power(struct net_device *dev, struct iw_request_info *info,
		struct iw_param *vwrq, char *extra)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	int		mode;

	ENTER();

	mode = Adapter->PSMode;

	if ((vwrq->disabled = (mode == Wlan802_11PowerModeCAM))
			|| Adapter->MediaConnectStatus == 
					WlanMediaStateDisconnected) {
		LEAVE();
		return 0;
	}

		vwrq->value = 0;

#if 0 
	if ((Adapter->rmode & 0xff) == 
			MRVDRV_RXPD_STATUS_MULTICAST_RX) {
		vwrq->flags |= IW_POWER_UNICAST_R;
	} else {
		vwrq->flags |= IW_POWER_ALL_R;
	}
#endif

	LEAVE1();
	return 0;
}
#endif

static int wlan_set_sens(struct net_device *dev, struct iw_request_info *info, 
		struct iw_param *vwrq, char *extra)
{
	ENTER();
	LEAVE();
	return 0;
}

static int wlan_get_sens(struct net_device *dev,
	      struct iw_request_info *info, struct iw_param *vwrq,
	      char *extra)
{
	ENTER();
	LEAVE();
	return -1;
}

#if 0
/*
 * Wireless Handler: get AP List Note: this is deprecated in favour of
 * IWSCAN if WIRELESS_EXT > 14 
 */
static int wlan_get_aplist(struct net_device *dev,
		struct iw_request_info *info,
		struct iw_point *dwrq, char *extra)
{
	int			i;
	wlan_private		*priv = dev->priv;
	struct sockaddr		*address = (struct sockaddr *) extra;
	wlan_adapter		*adapter = priv->adapter;
	struct iw_quality	qual[IW_MAX_AP];
				// Max Number of AP's to return

	ENTER();

#define HAS_QUALITY_INFO 1
	
	PRINTK1("Number of AP's: %d\n", adapter->ulNumOfBSSIDs);

#ifdef	DEBUG
	for (i = 0; i < adapter->ulNumOfBSSIDs; i++) {
		PRINTK1("\t%2d:\t%32s - %2x-%2x-%2x-%2x-%2x-%2x "
				"RSSI=%d\n", i,
				adapter->BSSIDList[i].Ssid.Ssid,
				adapter->BSSIDList[i].MacAddress[0],
				adapter->BSSIDList[i].MacAddress[1],
				adapter->BSSIDList[i].MacAddress[2],
				adapter->BSSIDList[i].MacAddress[3],
				adapter->BSSIDList[i].MacAddress[4],
				adapter->BSSIDList[i].MacAddress[5],
				(int) adapter->BSSIDList[i].Rssi);
	}
#endif

	for (i = 0; i < MIN(IW_MAX_AP,adapter->ulNumOfBSSIDs); i++) {
		memcpy(address[i].sa_data, adapter->
				BSSIDList[i].MacAddress, ETH_ALEN);
		address[i].sa_family = ARPHRD_ETHER;
		qual[i].level = SCAN_RSSI(adapter->BSSIDList[i].Rssi);
		qual[i].qual = 0;
		if (adapter->NF[TYPE_BEACON][TYPE_NOAVG] == 0)
			qual[i].noise = MRVDRV_NF_DEFAULT_SCAN_VALUE;
		else
			qual[i].noise = CAL_NF(adapter->NF[TYPE_BEACON][TYPE_NOAVG]);
	}

	dwrq->flags = HAS_QUALITY_INFO;	
	
	memcpy(extra + sizeof(struct sockaddr) * i,
			&qual, sizeof(struct iw_quality) * i);

	dwrq->length = i;

	LEAVE1();
	return 0;
}
#endif

static int wlan_get_scan(struct net_device *dev, struct iw_request_info *info, 
                         struct iw_point *dwrq, char *extra)
{
	int			ret = 0;
	char			*current_ev = extra;
	char			*end_buf = extra + IW_SCAN_MAX_DATA;
	wlan_private		*priv = dev->priv;
	struct iw_event		iwe;	/* Temporary buffer */
	char			*current_val;	/* For rates */
	int			i, j;
	wlan_adapter		*Adapter = priv->adapter;
	WLAN_802_11_BSSID	*pBSSIDList;
#if defined(WPA) || (WIRELESS_EXT > 14)
	unsigned char		buf[16+256*2], *ptr;
#endif
	CHANNEL_FREQ_POWER	*cfp;

	ENTER();

	PRINTK("ulNumOfBSSIDs = %d\n", Adapter->ulNumOfBSSIDs);

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)) {
		printk("<1>SIOCGIWSCAN: IOCTLS called when station is"
               " in DeepSleep\n");
		return -EBUSY;
	}
#endif
    
#if WIRELESS_EXT > 13
	/* The old API using SIOCGIWAPLIST had a hard limit of IW_MAX_AP. 
	 * The new API using SIOCGIWSCAN is only limited by buffer size
	 * WE-14 -> WE-16 the buffer is limited to IW_SCAN_MAX_DATA bytes
	 * which is 4096 :-).
	 */ 
	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		if ((current_ev + MAX_SCAN_CELL_SIZE) >= end_buf) {
			PRINTK("i=%d break out: current_ev=%p end_buf=%p "
				"MAX_SCAN_CELL_SIZE=%d\n",
				i,current_ev,end_buf,MAX_SCAN_CELL_SIZE);
			break;
		}

		pBSSIDList = &Adapter->BSSIDList[i];
		
		PRINTK("Current Ssid: %s\n", Adapter->CurBssParams.ssid.Ssid);

#ifdef MULTI_BANDS
		cfp = find_cfp_by_band_and_channel(Adapter, 
				pBSSIDList->bss_band, pBSSIDList->Channel);
#else
		cfp = find_cfp_by_band_and_channel(Adapter, 0, 
						pBSSIDList->Channel);
#endif
		if (!cfp) {
			PRINTK("Invalid channel number %d\n",
						pBSSIDList->Channel);
			continue;
		}

		/* First entry *MUST* be the AP MAC address */
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(iwe.u.ap_addr.sa_data, &Adapter->
				BSSIDList[i].MacAddress,
				ETH_ALEN);
		PRINTK1("Before current_ev\n");
		iwe.len = IW_EV_ADDR_LEN;
		current_ev = iwe_stream_add_event(current_ev,
				end_buf, &iwe, iwe.len);
		PRINTK1("After SIOCGIWAP\n");

		//Add the ESSID 
		iwe.u.data.length = Adapter->
			BSSIDList[i].Ssid.SsidLength;

		if (iwe.u.data.length > 32)
			iwe.u.data.length = 32;

		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.flags = 1;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(current_ev,
				end_buf, &iwe,
				Adapter->BSSIDList[i].Ssid.Ssid);
		PRINTK1("After SIOCGIWAP1\n");

		//Add mode 
		iwe.cmd = SIOCGIWMODE;
		iwe.u.mode = Adapter->BSSIDList[i].
			InfrastructureMode + 1;
		iwe.len = IW_EV_UINT_LEN;
		current_ev = iwe_stream_add_event(current_ev,
				end_buf, &iwe, iwe.len);
		PRINTK1("After SIOCGIWAP2\n");

		//frequency 
		iwe.cmd = SIOCGIWFREQ;

		iwe.u.freq.m = (long) cfp->Freq * 100000;

		iwe.u.freq.e = 1;

		iwe.len = IW_EV_FREQ_LEN;
		current_ev = iwe_stream_add_event(current_ev,
					  end_buf, &iwe, iwe.len);

		/* Add quality statistics */
		iwe.cmd = IWEVQUAL;
		iwe.u.qual.level = SCAN_RSSI(Adapter->BSSIDList[i].Rssi);
		iwe.u.qual.qual = 0;
		if (Adapter->NF[TYPE_BEACON][TYPE_NOAVG] == 0)
			iwe.u.qual.noise = MRVDRV_NF_DEFAULT_SCAN_VALUE;
		else
			iwe.u.qual.noise = CAL_NF(Adapter->NF[TYPE_BEACON][TYPE_NOAVG]);

		if ((Adapter->InfrastructureMode == Wlan802_11IBSS) &&
				!SSIDcmp(&Adapter->CurBssParams.ssid,
					&Adapter->BSSIDList[i].Ssid)
				&& Adapter->AdhocCreate) {
			ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_RSSI,
					0, HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE, NULL);

			if (ret) {
				LEAVE();
				return ret;
			}
			iwe.u.qual.level =
				CAL_RSSI(Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE, 
					Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE);
		}

		iwe.len = IW_EV_QUAL_LEN;
		current_ev = iwe_stream_add_event(current_ev,
				end_buf, &iwe, iwe.len);

		/* Add encryption capability */
		iwe.cmd = SIOCGIWENCODE;
		if (Adapter->BSSIDList[i].Privacy)
			iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		else
			iwe.u.data.flags = IW_ENCODE_DISABLED;

		iwe.u.data.length = 0;

		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(current_ev,
				end_buf, &iwe,
				Adapter->BSSIDList->Ssid.Ssid);

		/*
		 * Rate : stuffing multiple values in a single event require a
		 * bit more of magic - Jean II 
		 */
		current_val = current_ev + IW_EV_LCP_LEN;

		iwe.cmd = SIOCGIWRATE;
		/* Those two flags are ignored... */
		iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = 0;
		iwe.u.bitrate.value = 0;
		/* Bit rate given in 500 kb/s units (+ 0x80) */
		for (j = 0; j < sizeof(Adapter->BSSIDList[i].SupportedRates);
				j++){
			int	rate;
			if (Adapter->BSSIDList[i].SupportedRates[j] == 0) {
				break;
			}
			rate = (Adapter->BSSIDList[i].SupportedRates[j] &
					0x7F) * 500000;
			if (rate > iwe.u.bitrate.value) {
				iwe.u.bitrate.value = rate;
			}
	
			iwe.u.bitrate.value = (Adapter->BSSIDList[i].
					SupportedRates[j] & 0x7f) * 500000;
			iwe.len = IW_EV_PARAM_LEN;
			current_ev = iwe_stream_add_value(current_ev, 
					current_val, end_buf,
					&iwe, iwe.len);

		}
		if ((Adapter->BSSIDList[i].InfrastructureMode ==
				Wlan802_11IBSS) &&
				!SSIDcmp(&Adapter->CurBssParams.ssid,
				&Adapter->BSSIDList[i].Ssid) &&
				Adapter->AdhocCreate) {
			iwe.u.bitrate.value = 22 * 500000;
		}
		iwe.len = IW_EV_PARAM_LEN;
		current_ev = iwe_stream_add_value(current_ev,
				current_val, end_buf,
				&iwe, iwe.len);

		/* Add new value to event */
		current_val = current_ev + IW_EV_LCP_LEN;

#ifdef WPA
#ifdef WPA2
		if (Adapter->BSSIDList[i].wpa2_supplicant.Wpa_ie[0] == WPA2_IE) {
			memset(&iwe, 0, sizeof(iwe));
			memset(buf, 0, sizeof(buf));
			ptr = buf;
			ptr += sprintf(ptr, "rsn_ie=");

			for (j = 0; j < Adapter->BSSIDList[i].wpa2_supplicant.Wpa_ie_len; 
					j++) {
				ptr += sprintf(ptr, "%02x",
				Adapter->BSSIDList[i].wpa2_supplicant.Wpa_ie[j]);
			}
			iwe.u.data.length = strlen(buf);

			PRINTK1("iwe.u.data.length %d\n",iwe.u.data.length);
			PRINTK1("WPA2 BUF: %s \n",buf);

			iwe.cmd = IWEVCUSTOM;
			iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
			current_ev = iwe_stream_add_point(current_ev, end_buf, 
				&iwe, buf);
		}
#endif	//WPA2
		if (Adapter->BSSIDList[i].wpa_supplicant.Wpa_ie[0] == WPA_IE) {
			memset(&iwe, 0, sizeof(iwe));
			memset(buf, 0, sizeof(buf));
			ptr = buf;
			ptr += sprintf(ptr, "wpa_ie=");

			for (j = 0; j < Adapter->BSSIDList[i].wpa_supplicant.Wpa_ie_len; 
					j++) {
				ptr += sprintf(ptr, "%02x",
				Adapter->BSSIDList[i].wpa_supplicant.Wpa_ie[j]);
			}
			iwe.u.data.length = strlen(buf);

			PRINTK1("iwe.u.data.length %d\n",iwe.u.data.length);
			PRINTK1("WPA BUF: %s \n",buf);

			iwe.cmd = IWEVCUSTOM;
			iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
			current_ev = iwe_stream_add_point(current_ev, end_buf, 
				&iwe, buf);
		}

#endif	//WPA

#ifdef WMM
		if (Adapter->BSSIDList[i].Wmm_IE[0] == WMM_IE) {
			memset(&iwe, 0, sizeof(iwe));
			memset(buf, 0, sizeof(buf));
			ptr = buf;
			ptr += sprintf(ptr, "wmm_ie=");

			for (j = 0; j < Adapter->BSSIDList[i].Wmm_ie_len; j++) {
				ptr += sprintf(ptr, "%02x",
				Adapter->BSSIDList[i].Wmm_IE[j]);
			}
			iwe.u.data.length = strlen(buf);

			PRINTK1("iwe.u.data.length %d\n",iwe.u.data.length);
			PRINTK1("WMM BUF: %s \n",buf);

			iwe.cmd = IWEVCUSTOM;
			iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
			current_ev = iwe_stream_add_point(current_ev, end_buf, 
				&iwe, buf);
		}
#endif	//WMM

#if WIRELESS_EXT > 14
#ifdef MULTI_BANDS
		memset(&iwe, 0, sizeof(iwe));
		memset(buf, 0, sizeof(buf));
		ptr = buf;
		ptr += sprintf(ptr, "band=");

		memset(&iwe, 0, sizeof(iwe));
		
		if (Adapter->BSSIDList[i].bss_band == BAND_A) {
			ptr += sprintf(ptr, "a");
		} else {
			ptr += sprintf(ptr, "bg");
		}
		
		iwe.u.data.length = strlen(buf);

		PRINTK1("iwe.u.data.length %d\n",iwe.u.data.length);
		PRINTK1("BUF: %s \n",buf);

		iwe.cmd = IWEVCUSTOM;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(current_ev, end_buf, 
                                          &iwe, buf);
#endif

        if(Adapter->BSSIDList[i].extra_ie != 0)
        {
            memset(&iwe, 0, sizeof(iwe));
            memset(buf, 0, sizeof(buf));
            ptr = buf;
            ptr += sprintf(ptr, "extra_ie");
            iwe.u.data.length = strlen(buf);

            PRINTK1("iwe.u.data.length %d\n",iwe.u.data.length);
            PRINTK1("BUF: %s \n",buf);
            
            iwe.cmd = IWEVCUSTOM;
            iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
            current_ev = iwe_stream_add_point(current_ev, end_buf, 
                                              &iwe, buf);
        }

#ifdef TIMESTAMP_SCAN_RESULTS
        /*
        **
        ** Add the TSF/Timestamp of the received scan as a custom field
        **
        */
		memset(&iwe, 0, sizeof(iwe));
		memset(buf, 0, sizeof(buf));
		ptr = buf;
        ptr += sprintf(ptr, "timestamp=");
        for (j = 0; j < sizeof(Adapter->BSSIDList[i].TimeStamp); j++)
        {
            ptr += sprintf(ptr, "%02x", Adapter->BSSIDList[i].TimeStamp[j]);
        }
		memset(&iwe, 0, sizeof(iwe));
		iwe.u.data.length = strlen(buf);
        
		PRINTK1("timestamp scan buf: %s\n", buf);
        
		iwe.cmd = IWEVCUSTOM;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(current_ev, end_buf, 
                                          &iwe, buf);
#endif
#endif
        
		current_val = current_ev + IW_EV_LCP_LEN;
		/*
		 * Check if we added any event 
		 */
		if ((current_val - current_ev) > IW_EV_LCP_LEN)
			current_ev = current_val;
	}
    
	dwrq->length = (current_ev - extra);
	dwrq->flags = 0;
    
	LEAVE1();
#endif
	return 0;
}


#ifdef IWGENIE_SUPPORT
#if (WIRELESS_EXT >= 18)                    
static int wlan_set_gen_ie(struct net_device *dev, 
                           struct iw_request_info *info, 
                           struct iw_param *vwrq, 
                           char *extra)
{
    /*
    ** Functionality to be added later
    */
    return 0;
}

static int wlan_get_gen_ie(struct net_device *dev, 
                           struct iw_request_info *info, 
                           struct iw_point *dwrq,
                           char *extra)
{
    /*
    ** Functionality to be added later
    */
    return 0;
}
#else
/*
**  wlan_set_gen_ioctl is used to pass an opaque block of data, expected to be
**    IEEE TLVs, to the driver for eventual passthrough to the firmware in
**    an associate/join (and potentially start) command. 
**
**  Data is appended to an existing buffer and then wrapped in a passthrough
**    TLV in the command API to the firmware.  The firmware treats the data
**    as a transparent passthrough to the transmitted management frame.
**
**  A zero size length in the ioctl resets the buffer.  
**
**  Return: 0 for success, -1 for error
*/
static int wlan_set_gen_ie_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter			*Adapter = priv->adapter;
    int ret = 0;
    
    /* If the passed length is zero, reset the buffer */
    if (wrq->u.data.length == 0)
    {
        Adapter->genIeBufferLen = 0;
    }
    else
    {
        /* Verify that the passed length is not larger than the available 
        **   space remaining in the buffer
        */
        if (wrq->u.data.length < 
            (sizeof(Adapter->genIeBuffer) - Adapter->genIeBufferLen))
        {
            /* Append the passed data to the end of the genIeBuffer */
            memcpy(Adapter->genIeBuffer + Adapter->genIeBufferLen, 
                   wrq->u.data.pointer, wrq->u.data.length);
            
            /* Increment the stored buffer length by the size passed */
            Adapter->genIeBufferLen += wrq->u.data.length;
        }
        else
        {
            /* Passed data does not fit in the remaining buffer space */
            ret = -1;
        }
    }

    /* Return 0 for success, -1 for error case */
    return ret;
}

/*
**  wlan_get_gen_ioctl is used to pass an opaque block of data, expected to be
**    IEEE TLVs, back to the application.  Currently the data block passed
**    back to the application is the saved association response retrieved 
**    from the firmware.
**
**  Return: 0 for success; no error condition
*/
static int wlan_get_gen_ie_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter			*Adapter = priv->adapter;
    WLAN_802_11_ASSOCIATION_INFORMATION* pAssocInfo 
        = (WLAN_802_11_ASSOCIATION_INFORMATION*)Adapter->AssocInfoBuffer;
    int copySize; 
    
    /*
    ** Set the amount to copy back to the application as the minimum of the 
    **   available data or the buffer provided by the application
    */
    copySize = MIN(pAssocInfo->ResponseIELength, wrq->u.data.length);

    if (copySize)
    {
        /*
        ** If the copy size is non-zero, copy the IEEE TLVs in the association
        **   response back to the application
        */
        memcpy(wrq->u.data.pointer, 
               (u8*)pAssocInfo + pAssocInfo->OffsetResponseIEs, 
               copySize);
    }

    /* Returned copy length */
    wrq->u.data.length = copySize;

    /* No error on return */
    return 0;
}
#endif
#endif

#ifdef REASSOCIATION_SUPPORT
static int wlan_reassociate_ioctl(struct net_device *dev, struct iwreq *wrq)
{
    wlan_private    *priv    = dev->priv;
    wlan_adapter    *Adapter = priv->adapter;
    wlan_ioctl_reassociation_info reassocInfo;
    u8 zeroMac[] = {0, 0, 0, 0, 0, 0};
    struct sockaddr bssidSockAddr;
    struct iw_point dwrq;

    int attemptBSSID = 0;
    int attemptSSID  = 0;
    int retcode      = 0;

    ENTER();

    Adapter->reassocAttempt = TRUE;

    /* Fail if the data passed is not what we expect */
    if (wrq->u.data.length != sizeof(reassocInfo))
    {
        retcode = -EINVAL;
    }
    else if (copy_from_user(&reassocInfo, wrq->u.data.pointer,
                            sizeof(reassocInfo)) != 0)
    {
        /* copy_from_user failed  */
        PRINTK1("Reassoc: copy from user failed\n");
        retcode = -EFAULT;
    }
    else
    {
        /*
        **  Copy the passed CurrentBSSID into the driver adapter structure.
        **    This is later used in a TLV in the associate command to 
        **    trigger the reassociation in firmware.
        */
        memcpy(Adapter->reassocCurrentAp, reassocInfo.CurrentBSSID,
               sizeof(Adapter->reassocCurrentAp));

        /* Check if the desired BSSID is non-zero */
        if (memcmp(reassocInfo.DesiredBSSID, zeroMac, sizeof(zeroMac)) != 0)
        {
            /* Set a flag for later processing */
            attemptBSSID = TRUE;
        }

        /* Check if the desired SSID is set */
        if (reassocInfo.DesiredSSID[0])
        {
            /* Make sure there is a null at the end of the SSID string */
            reassocInfo.DesiredSSID[sizeof(reassocInfo.DesiredSSID) - 1] = 0;

            /* Set a flag for later processing */
            attemptSSID = TRUE;
        }

        /* 
        **  Debug prints
        */
        PRINTK1("Reassoc: BSSID(%d), SSID(%d)\n",
                attemptBSSID, attemptSSID);
        PRINTK1("Reassoc: CBSSID(%02x:%02x:%02x:%02x:%02x:%02x)\n",
                reassocInfo.CurrentBSSID[0],
                reassocInfo.CurrentBSSID[1],
                reassocInfo.CurrentBSSID[2],
                reassocInfo.CurrentBSSID[3],
                reassocInfo.CurrentBSSID[4],
                reassocInfo.CurrentBSSID[5]);
        PRINTK1("Reassoc: DBSSID(%02x:%02x:%02x:%02x:%02x:%02x)\n",
                reassocInfo.DesiredBSSID[0],
                reassocInfo.DesiredBSSID[1],
                reassocInfo.DesiredBSSID[2],
                reassocInfo.DesiredBSSID[3],
                reassocInfo.DesiredBSSID[4],
                reassocInfo.DesiredBSSID[5]);
        PRINTK1("Reassoc: DSSID = %s\n", reassocInfo.DesiredSSID);


        /*
        **  If a BSSID was provided in the reassociation request, attempt
        **    to reassociate to it first 
        */
        if (attemptBSSID)
        {
            memset(bssidSockAddr.sa_data, 0x00, sizeof(bssidSockAddr.sa_data));
            memcpy(bssidSockAddr.sa_data, reassocInfo.DesiredBSSID,
                   sizeof(bssidSockAddr.sa_data));
            bssidSockAddr.sa_family = ARPHRD_ETHER;

            /* Attempt to reassociate to the BSSID passed */        
            retcode = wlan_set_wap(dev, NULL, &bssidSockAddr, NULL);
        }

        /*
        **  If a SSID was provided, and either the BSSID failed or a BSSID
        **    was not provided, attemtp to reassociate to the SSID
        */
        if (attemptSSID && (retcode != 0 || !attemptBSSID))
        {
            reassocInfo.DesiredSSID[sizeof(reassocInfo.DesiredSSID) - 1] = 0;
            dwrq.length = strlen(reassocInfo.DesiredSSID) + 1;
            dwrq.flags = 1; /* set for a specific association */

            /* Attempt to reassociate to the SSID passed */        
            retcode = wlan_set_essid(dev, NULL, &dwrq,
                                     reassocInfo.DesiredSSID);
        }


        /* Reset the adapter control flag and current AP to zero */
        memset(Adapter->reassocCurrentAp, 0x00, 
               sizeof(Adapter->reassocCurrentAp));
        Adapter->reassocAttempt = FALSE;
    }

    return retcode;
}
#endif

/*
 * iwconfig settable callbacks 
 */
static const iw_handler wlan_handler[] = {
    (iw_handler) wlan_config_commit,        /* SIOCSIWCOMMIT */
    (iw_handler) wlan_get_name,             /* SIOCGIWNAME */
    (iw_handler) NULL,                      /* SIOCSIWNWID */
    (iw_handler) NULL,                      /* SIOCGIWNWID */
    (iw_handler) wlan_set_freq,             /* SIOCSIWFREQ */
    (iw_handler) wlan_get_freq,             /* SIOCGIWFREQ */
    (iw_handler) wlan_set_mode,             /* SIOCSIWMODE */
    (iw_handler) wlan_get_mode,             /* SIOCGIWMODE */
    (iw_handler) wlan_set_sens,             /* SIOCSIWSENS */
    (iw_handler) wlan_get_sens,             /* SIOCGIWSENS */
    (iw_handler) NULL,                      /* SIOCSIWRANGE */
    (iw_handler) wlan_get_range,            /* SIOCGIWRANGE */
    (iw_handler) NULL,                      /* SIOCSIWPRIV */
    (iw_handler) NULL,                      /* SIOCGIWPRIV */
    (iw_handler) NULL,                      /* SIOCSIWSTATS */
    (iw_handler) NULL,                      /* SIOCGIWSTATS */
#if WIRELESS_EXT > 15
    iw_handler_set_spy,                     /* SIOCSIWSPY */
    iw_handler_get_spy,                     /* SIOCGIWSPY */
    iw_handler_set_thrspy,                  /* SIOCSIWTHRSPY */
    iw_handler_get_thrspy,                  /* SIOCGIWTHRSPY */
#else  /* WIRELESS_EXT > 15 */
#ifdef WIRELESS_SPY
    (iw_handler) wlan_set_spy,              /* SIOCSIWSPY */
    (iw_handler) wlan_get_spy,              /* SIOCGIWSPY */
#else  /* WIRELESS_SPY */
    (iw_handler) NULL,                      /* SIOCSIWSPY */
    (iw_handler) NULL,                      /* SIOCGIWSPY */
#endif /* WIRELESS_SPY */
    (iw_handler) NULL,                      /* -- hole -- */
    (iw_handler) NULL,                      /* -- hole -- */
#endif /* WIRELESS_EXT > 15 */
    (iw_handler) wlan_set_wap,              /* SIOCSIWAP */
    (iw_handler) wlan_get_wap,              /* SIOCGIWAP */
    (iw_handler) NULL,                      /* -- hole -- */
    //(iw_handler) wlan_get_aplist,         /* SIOCGIWAPLIST */
    NULL,                                   /* SIOCGIWAPLIST */
#if WIRELESS_EXT > 13
    (iw_handler) wlan_set_scan,             /* SIOCSIWSCAN */
    (iw_handler) wlan_get_scan,             /* SIOCGIWSCAN */
#else  /* WIRELESS_EXT > 13 */
    (iw_handler) NULL,                      /* SIOCSIWSCAN */
    (iw_handler) NULL,                      /* SIOCGIWSCAN */
#endif /* WIRELESS_EXT > 13 */
    (iw_handler) wlan_set_essid,            /* SIOCSIWESSID */
    (iw_handler) wlan_get_essid,            /* SIOCGIWESSID */
    (iw_handler) wlan_set_nick,             /* SIOCSIWNICKN */
    (iw_handler) wlan_get_nick,             /* SIOCGIWNICKN */
    (iw_handler) NULL,                      /* -- hole -- */
    (iw_handler) NULL,                      /* -- hole -- */
    (iw_handler) wlan_set_rate,             /* SIOCSIWRATE */
    (iw_handler) wlan_get_rate,             /* SIOCGIWRATE */
    (iw_handler) wlan_set_rts,              /* SIOCSIWRTS */
    (iw_handler) wlan_get_rts,              /* SIOCGIWRTS */
    (iw_handler) wlan_set_frag,             /* SIOCSIWFRAG */
    (iw_handler) wlan_get_frag,             /* SIOCGIWFRAG */
    (iw_handler) wlan_set_txpow,            /* SIOCSIWTXPOW */
    (iw_handler) wlan_get_txpow,            /* SIOCGIWTXPOW */
    (iw_handler) wlan_set_retry,            /* SIOCSIWRETRY */
    (iw_handler) wlan_get_retry,            /* SIOCGIWRETRY */
#ifdef WPA                                  
    (iw_handler) NULL,                      /* -- hole -- */
#else  /* WPA */
    (iw_handler) wlan_set_encode,           /* SIOCSIWENCODE */
#endif                                      
    (iw_handler) wlan_get_encode,           /* SIOCGIWENCODE */
#ifdef PS_REQUIRED                          
    (iw_handler) wlan_set_power,            /* SIOCSIWPOWER */
    (iw_handler) wlan_get_power,            /* SIOCGIWPOWER */
#endif /* PS_REQUIRED */                    
#if (WIRELESS_EXT >= 18)                    
    (iw_handler) NULL,                      /* -- hole -- */
    (iw_handler) NULL,                      /* -- hole -- */
#if defined(IWGENIE_SUPPORT)                
    (iw_handler) wlan_set_gen_ie,           /* SIOCSIWGENIE */
    (iw_handler) wlan_get_gen_ie,           /* SIOCGIWGENIE */
#else /* IWGENIE_SUPPORT */                 
    (iw_handler) NULL,                      /* -- hole -- */
    (iw_handler) NULL,                      /* -- hole -- */
#endif /* IWGENIE_SUPPORT */
#endif /* WIRELESSS_EXT >= 18 */
};

/*
 * iwpriv settable callbacks 
 */

static const iw_handler wlan_private_handler[] = {
	NULL,			/* SIOCIWFIRSTPRIV */
};

static const struct iw_priv_args wlan_private_args[] = {
	/*
	 * { cmd, set_args, get_args, name } 
	 */
	{
	WLANEXTSCAN, /* IOCTL: 26 */
	IW_PRIV_TYPE_INT,
	IW_PRIV_TYPE_CHAR | 2,
	"extscan"
	},

	{
	WLANCISDUMP, /* IOCTL: 1 */
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_BYTE | 512,
	"getcis"
	},

     	{
	WLANSCAN_TYPE, /* IOCTL: 11 */
	IW_PRIV_TYPE_CHAR | 8,
	IW_PRIV_TYPE_CHAR | 8,
	"scantype"
	},

    	// added for setrxant, settxant, getrxant, gettxant
	/* ADDED ON 2004/08/16 MPS. */
	{ 
	WLAN_SETONEINT_GETONEINT,  /* IOCTL: 23 */
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	""
	},
	{ 
	WLANNF, 
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"getNF"
	},
	{ 
	WLANRSSI, 
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"getRSSI"
	},
#ifdef THROUGHPUT_TEST
	{ 
	WLANTHRUPUT, 
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"thruput"
	},
#endif /* THROUGHPUT_TEST */

#ifdef WMM
	{ 
	WLANWMM_ENABLE,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"wmm"
	},
#endif /* WMM */

#ifdef BG_SCAN 
	{ 
	WLANBGSCAN, 
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"bgscan"
	},
#endif /* BG_SCAN */

#ifdef BULVERDE_SDIO
	{ 
	WLANSDIOCLOCK,
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"sdioclock"
	},
#endif /* BULVERDE_SDIO */

#ifdef ENABLE_802_11D
	{
	WLANENABLE11D,		
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"enable11d"
	},
#endif

#ifdef ADHOC_GRATE
	{ 
	WLANADHOCGRATE,
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"adhocgrate"
	},
#endif

#ifdef ENABLE_802_11H_TPC
	{
	WLANENABLE11HTPC,		
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"enable11htpc"
	},

	{
	WLANSETLOCALPOWER,		
	IW_PRIV_TYPE_INT | 1, 
	IW_PRIV_TYPE_INT | 1, 
	"setpowercons"
	},
#endif
#ifdef WMM_UAPSD
	{
	WLAN_WMM_QOSINFO,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"wmm_qosinfo"
	},
#endif
#ifdef PS_REQUIRED
	{
	WLAN_LISTENINTRVL,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"lolisteninter"
	},
#ifdef FW_WAKEUP_METHOD
	{
	WLAN_FW_WAKEUP_METHOD,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"fwwakeupmethod"
	},
#endif
#endif

/* adds for iwpriv txcontrol ioctl */
	{
	WLAN_TXCONTROL,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"txcontrol"
	},
#ifdef ATIMGEN			
	{
	WLANATIMGEN,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"atimgen"
	},
#endif

#ifdef WMM_UAPSD
	{
	WLANNULLGEN,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"uapsdnullgen"
	},
#endif

	{
	WLAN_NULLPKTINTERVAL,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"psnullinterval"
	},

#ifdef WPRM_DRV
	{
	WLANMOTOTM,
	IW_PRIV_TYPE_INT | 1,
	IW_PRIV_TYPE_INT | 1,
	"mototm"
	},
#endif /* WPRM_DRV */

// added for setrxant, settxant, getrxant, gettxant
	/* Using iwpriv sub-command feature */
	{ 
	WLAN_SETONEINT_GETNONE,  /* IOCTL: 24 */
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE, 
	""
	},
	{ 
	WLAN_SUBCMD_SETRXANTENNA, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"setrxant"
	},
	{ 
	WLAN_SUBCMD_SETTXANTENNA, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE, 
	"settxant"
	},
	{
	WLANSETPRETBTT,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"pre-TBTT"
	},
	{
	WLANSETAUTHALG,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"authalgs",
	},
	{
	WLANSET8021XAUTHALG,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"8021xauthalgs",
	},
	{
	WLANSETENCRYPTIONMODE,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"encryptionmode",
	},
	{ 
	WLANSETREGION,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"setregioncode"
	},
	{ 
	WLAN_SET_LISTEN_INTERVAL,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"setlisteninter"
	},
#ifdef PS_REQUIRED
	{ 
	WLAN_SET_MULTIPLE_DTIM,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"setmultipledtim"
	},
#endif
	{ 
	WLAN_SET_ATIM_WINDOW,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"atimwindow"
	},
	{ 
	WLANSETBCNAVG, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"setbcnavg"
	},
	{ 
	WLANSETDATAAVG, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_NONE,
	"setdataavg"
	},
	{ 
	WLAN_SETNONE_GETONEINT, 
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	""
	},
	{ 
	WLANGETREGION,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"getregioncode"
	},
	{ 
	WLAN_GET_LISTEN_INTERVAL,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"getlisteninter"
	},
	{ 
	WLAN_GET_MULTIPLE_DTIM,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"getmultipledtim"
	},
	{ 
	WLANGETBCNAVG,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"getbcnavg"
	},
	{ 
	WLAN_SETNONE_GETTWELVE_CHAR,  /* IOCTL: 19 */
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_CHAR | 12,
	""
	},
	{ 
	WLAN_SUBCMD_GETRXANTENNA, 
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_CHAR | 12, 
	"getrxant"
	},
	{ 
	WLAN_SUBCMD_SETTXANTENNA, 
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_CHAR | 12, 
	"gettxant"
	},
#ifdef DEEP_SLEEP
	{
	WLANDEEPSLEEP, /* IOCTL: 27 */
#ifdef DEEP_SLEEP_CMD
	IW_PRIV_TYPE_CHAR | 1, 
	IW_PRIV_TYPE_CHAR | 6, 
#else
	IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
#endif
	"deepsleep"
	},
#endif

#ifdef HOST_WAKEUP
	{
	WLANHOSTWAKEUPCFG, /* IOCTL: 6 */
	IW_PRIV_TYPE_CHAR | 15,
	IW_PRIV_TYPE_NONE,
	"hostwakeupcfg"
	},
#endif

	{
	WLAN_SETNONE_GETNONE, /* IOCTL: 8 */
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	""
	},
	{
	WLANDEAUTH,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	"deauth"
	},
	{
	WLANADHOCSTOP,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	"adhocstop"
	},
	{
	WLANRADIOON,		
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	"radioon"
	},
	{
	WLANRADIOOFF,		
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	"radiooff"
	},
#ifdef ADHOCAES
	{
	WLANREMOVEADHOCAES,		
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	"rmaeskey"
	},
#endif
#ifdef CIPHER_TEST
	{
	WLANCIPHERTEST,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_NONE,
	"cipher_test"
	},
#endif
	{
	WLAN_SETNONE_GETBYTE, /* IOCTL: 7 */
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_BYTE,
	""
	},
	{
	WLANREASSOCIATIONAUTO,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_BYTE,
	"reasso-on"
	},
	{
	WLANREASSOCIATIONUSER,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_BYTE,
	"reasso-off"
	},
	{
	WLANWLANIDLEON,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_BYTE,
	"wlanidle-on"
	},
	{
	WLANWLANIDLEOFF,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_BYTE,
	"wlanidle-off"
	},
	/* RFI TX/TX_CONTROL MODE */
	{
	WLAN_RFI, /* IOCTL: 28 */
	IW_PRIV_TYPE_BYTE | 1, 
	IW_PRIV_TYPE_NONE,
	""
	},
	{
	WLAN_TX_MODE,
	IW_PRIV_TYPE_BYTE | 1, 
	IW_PRIV_TYPE_NONE,
	"tx-mode"
	},
	{
	WLAN_TX_CONTROL_MODE,
	IW_PRIV_TYPE_BYTE | 1, 
	IW_PRIV_TYPE_NONE,
	"txcontrol-mode"
	},
	{
	WLAN_SET64CHAR_GET64CHAR, /* IOCTL: 25 */	
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	""
	},
#ifdef BCA
	{
	WLANBCA,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"bca"
	},
#endif
	{
	WLANSLEEPPARAMS,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"sleepparams"
	},

#ifdef ENABLE_802_11H_TPC
	{
	WLANREQUESTTPC,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"requesttpc"
	},
	{
	WLANSETPOWERCAP,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"setpowercap"
	},
#endif
#ifdef BCA
	{
	WLAN_BCA_TIMESHARE,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"bca-ts"
	},
#endif
	{
	WLANSCAN_MODE,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"scanmode"
	},
	{ 
	WLANSCAN_BSSID,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"scanbssid"
	},
	{ 
	WLAN_GET_RATE,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"getrate"
	},
	{ 
	WLAN_GET_ADHOC_STATUS,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"getadhocstatus"
	},
#if defined(IWGENIE_SUPPORT) && (WIRELESS_EXT < 18)
	{
    WLAN_SET_GEN_IE,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"setgenie"
	},
    {
	WLAN_GET_GEN_IE,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"getgenie"
	},
#endif
#ifdef REASSOCIATION_SUPPORT
    {
    WLAN_REASSOCIATE,
	IW_PRIV_TYPE_CHAR | 64,
	IW_PRIV_TYPE_CHAR | 64,
	"reassociate"
	},
#endif
	{
	WLAN_SETWORDCHAR_GETNONE, /* IOCTL: 20 */	
	IW_PRIV_TYPE_CHAR | 32,
	IW_PRIV_TYPE_NONE,
	""
	},
#ifdef ADHOCAES
	{
	WLANSETADHOCAES,
	IW_PRIV_TYPE_CHAR | 32,
	IW_PRIV_TYPE_NONE,
	"setaeskey"
	},
#endif /* end of ADHOCAES */ 
	{
	WLAN_SETNONE_GETWORDCHAR, /* IOCTL: 21 */	
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_CHAR | 128,
	""
	},
#ifdef ADHOCAES
	{
	WLANGETADHOCAES,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_CHAR | 128,
	"getaeskey"
	},
#endif /* end of ADHOCAES */ 
	{
	WLANVERSION,
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_CHAR | 128,
	"version"
	},
	{
	WLANSETWPAIE, /* IOCTL: 0 */
	IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 24,
	IW_PRIV_TYPE_NONE,
	"setwpaie"
	},
	{ 
	WLAN_SETTENCHAR_GETNONE, /* IOCTL: 16 */
	IW_PRIV_TYPE_CHAR | 10, 
	IW_PRIV_TYPE_NONE,
	""
	},
#ifdef MULTI_BANDS
	{ 
	WLAN_SET_BAND,
	IW_PRIV_TYPE_CHAR | 10, 
	IW_PRIV_TYPE_NONE,
	"setband"
	},
	{ 
	WLAN_SET_ADHOC_CH,
	IW_PRIV_TYPE_CHAR | 10, 
	IW_PRIV_TYPE_NONE,
	"setadhocch"
	},
#endif
	{ 
	WLAN_SETNONE_GETTENCHAR,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_CHAR | 10, 
	""
	},
#ifdef MULTI_BANDS
	{ 
	WLAN_GET_BAND,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_CHAR | 10, 
	"getband"
	},
	{ 
	WLAN_GET_ADHOC_CH,
	IW_PRIV_TYPE_NONE, 
	IW_PRIV_TYPE_CHAR | 10, 
	"getadhocch"
	},
#endif
	{
	WLANGETLOG, /* IOCTL: 9 */
	IW_PRIV_TYPE_NONE,
	IW_PRIV_TYPE_CHAR | GETLOG_BUFSIZE,
	"getlog"
	},
        {
        WLAN_SET_GET_SIXTEEN_INT,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
        ""
        },
        {
        WLAN_TPCCFG,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
        "tpccfg"
        },
        {
        WLAN_POWERCFG,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
        "powercfg"
        },
#ifdef WPRM_DRV
        {
        WLAN_SESSIONTYPE,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
        "sessiontype"
        },
#endif
#ifdef AUTO_FREQ_CTRL
	{
	WLAN_AUTO_FREQ_SET,
	IW_PRIV_TYPE_INT | 16,
	IW_PRIV_TYPE_INT | 16,
	"setafc"
	},
	{
	WLAN_AUTO_FREQ_GET,
	IW_PRIV_TYPE_INT | 16,
	IW_PRIV_TYPE_INT | 16,
	"getafc"
	},
#endif
	{
	WLAN_SCANPROBES,
	IW_PRIV_TYPE_INT | 16,
	IW_PRIV_TYPE_INT | 16,
	"scanprobes"
	},
#ifdef LED_GPIO_CTRL	
        {
        WLAN_LED_GPIO_CTRL,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
        "ledgpio"
        },
#endif	
	{ 
	WLAN_SLEEP_PERIOD,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
	"sleeppd"
	},
	{ 
	WLAN_ADAPT_RATESET,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
	"rateadapt"
	},

	{ 
	WLAN_INACTIVITY_TIMEOUT,
        IW_PRIV_TYPE_INT | 16,
        IW_PRIV_TYPE_INT | 16,
	"inactivityto"
	},
	{ 
	WLANSNR, 
	IW_PRIV_TYPE_INT | 16, 
	IW_PRIV_TYPE_INT | 16, 
	"getSNR"
	},
	// end of addition
};

struct iw_handler_def wlan_handler_def = {
	num_standard:sizeof(wlan_handler) / sizeof(iw_handler),
	num_private:sizeof(wlan_private_handler) / sizeof(iw_handler),
	num_private_args:sizeof(wlan_private_args) /
	sizeof(struct iw_priv_args),
	standard:(iw_handler *) wlan_handler,
	private:(iw_handler *) wlan_private_handler,
	private_args:(struct iw_priv_args *) wlan_private_args,
#if 0
#if WIRELESS_EXT > 15
	spy_offset:((void *) (&((struct wlan_info *) NULL)->spy_data) -
		(void *) NULL),
#endif				/* WIRELESS_EXT > 15 */
#endif
};

#ifdef PASSTHROUGH_MODE
int wlan_passthrough_mode_ioctl(wlan_private * priv, struct ifreq *req)
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_passthrough_config		pt_config;
	int				ret;

	ENTER();

	memset(&pt_config, 0, sizeof(wlan_passthrough_config));

	copy_from_user(&pt_config, req->ifr_data,
			sizeof(wlan_passthrough_config));

	PRINTK1("Action=0x%04x, Enable=0x%02x, Channel=0x%02x\n",
			pt_config.Action, pt_config.Enable, pt_config.Channel);
	PRINTK1("Filter1=0x%08x, Filter2=0x%08x\n",
			pt_config.Filter1, pt_config.Filter2);
	PRINTK1("MatchBssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
			pt_config.MatchBssid[0],
			pt_config.MatchBssid[1],
			pt_config.MatchBssid[2],
			pt_config.MatchBssid[3],
			pt_config.MatchBssid[4], 
			pt_config.MatchBssid[5]);
	if (pt_config.MatchSsid[0]) {
		PRINTK1("MatchSsid=%s\n", pt_config.MatchSsid);
	} else {
		PRINTK1("MatchSsid is Blank\n");
	}

	if (pt_config.Enable == 1) {
		PRINTK1("Enabling Passtrough Mode\n");
		if (!Adapter->passthrough_enabled) {
#ifdef PS_REQUIRED
			if (Adapter->PSState != PS_STATE_FULL_POWER)
				PRINTK1("In Power Save Mode\n");
			PRINTK1("Exiting PS Mode\n");
			Adapter->PSMode = Wlan802_11PowerModeCAM;
			PSWakeup(priv, 0);
		}
#endif
		PRINTK1("Stopping Network Layer\n");

		/* Flush all the packets upto the OS before stopping */
		wlan_send_rxskbQ(priv);
		netif_stop_queue(priv->wlan_dev.netdev);

		if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
			ret = StopAdhocNetwork(priv, HostCmd_PENDING_ON_NONE);

			if (ret) {
				LEAVE();
				return ret;
			}
		}
	} else {
		PRINTK1("Disabling Passthrough Mode\n");
	}

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_PASSTHROUGH,
			HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, &pt_config);

	if (ret) {
		LEAVE();
		return ret;
	}

	Adapter->PassthroughCmdStatus = PT_MODE_TIMEOUT;

	// wait for the command response 
	mdelay(4000);

	if (Adapter->PassthroughCmdStatus == PT_MODE_SUCCESS) {
		PRINTK1("Command Success\n");
		memmove(&(Adapter->passthrough_config), &pt_config,
				sizeof(wlan_passthrough_config));
		Adapter->passthrough_enabled = pt_config.Enable;
		PRINTK1("Passthrough Mode %s\n",
				(Adapter->passthrough_enabled) ? 
				"enabled" : "disabled");

		if (!pt_config.Enable) {
			if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
				Adapter->MediaConnectStatus = 
					WlanMediaStateConnected;
			}

			Adapter->bAutoAssociation = FALSE;

			wlan_scan_networks(priv, HostCmd_PENDING_ON_NONE);

			netif_wake_queue(priv->wlan_dev.netdev);
		}

	} else if (Adapter->PassthroughCmdStatus == PT_MODE_FAILURE) {
		PRINTK1("Command Failure\n");
		return -1;		/* return appro err code */
	} else if (Adapter->PassthroughCmdStatus == PT_MODE_TIMEOUT) {
		PRINTK1("Command Timeout\n");
		return -1;		/* return appro err code */
	}
	// Copy the data to user space.
	copy_to_user(req->ifr_data, &Adapter->passthrough_config,
				sizeof(wlan_passthrough_config));

	return 0;
}

int wlan_passthrough_data_ioctl(wlan_private * priv, struct ifreq *req)
{
	wlan_adapter	*Adapter = priv->adapter;
	u8		*data = NULL;
	int		ret = 0, size;

	ENTER();

#ifndef PASSTHROUGH_NONBLOCK
get_pt_pkt:
#endif
	if ((ret = GetPassthroughPacket(Adapter, &data, &size)) < 0) {
#ifdef PASSTHROUGH_NONBLOCK
		PRINTK1("Returning %d to app\n", ret);
		return ret;
#else
		interruptible_sleep_on_timeout(&Adapter->wait_PTData, 2 * HZ);
		if (Adapter->SurpriseRemoved)
			return -1;
		/*
		 * get data if available 
		 */
		if (!list_empty(&Adapter->PTDataQ))
			goto get_pt_pkt;
#endif
	} else {
		if (!data) {
			PRINTK1("Returning %d to app: " "data ptr is null\n",
					ret);
			return -1;
		}

		PRINTK1("Size of PT Packet = %d\n", size);
		HEXDUMP("Data to App", data, size);
		/*
		 * copy the contents to user space 
		 */
		if (copy_to_user(req->ifr_data, data, size)) {
			PRINTK1("Copy to user failed \n");
			return -1;
		}
	}
	
	LEAVE();
	return 0;
}
#endif

#ifdef STDCMD

int wlan_stdcmd_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
	u8		*tempResponseBuffer;
	CmdCtrlNode	*pCmdNode;
	HostCmd_DS_GEN	gencmd, *pCmdPtr;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	u16		intoption = HostCmd_OPTION_USE_INT;

	ENTER();

	/*
	 * Get a free command control node 
	 */
	if (!(pCmdNode = GetFreeCmdCtrlNode(priv))) {
		PRINTK1("Failed GetFreeCmdCtrlNode\n");
		return -ENOMEM;
	}

	if (!(tempResponseBuffer = kmalloc(3000, GFP_KERNEL))) {
		PRINTK1("ERROR: Failed to allocate response buffer!\n");
		return -ENOMEM;
	}

	intoption |= HostCmd_OPTION_WAITFORRSP;
	
	SetCmdCtrlNode(priv, pCmdNode, 0, 0, intoption, NULL);

	pCmdPtr = (HostCmd_DS_GEN *) pCmdNode->BufVirtualAddr;

	/*
	 * Read to header to get the size 
	 */
	copy_from_user(&gencmd, req->ifr_data, sizeof(gencmd));

	/*
	 * Copy the whole command into the command buffer 
	 */
	copy_from_user(pCmdPtr, req->ifr_data, gencmd.Size);

	pCmdNode->InformationBuffer = tempResponseBuffer;
	pCmdNode->CmdFlags |= CMD_F_STDCMD;

	pCmdPtr->SeqNum = ++priv->adapter->SeqNum;
	pCmdPtr->Result = 0;

	PRINTK1("STDCMD Command: 0x%04x Size: %d SeqNum: %d\n",
			pCmdPtr->Command, pCmdPtr->Size, pCmdPtr->SeqNum);
	HEXDUMP("Command Data", (u8 *) (pCmdPtr), MIN(32, pCmdPtr->Size));
	PRINTK1("Copying data from : (user)0x%p -> 0x%p(driver)\n",
			req->ifr_data, pCmdPtr);

	QueueCmd(Adapter, pCmdNode, TRUE);
	wake_up_interruptible(&priv->MainThread.waitQ);

	if (intoption & HostCmd_OPTION_WAITFORRSP) {
		/* Sleep until response is generated by FW */
		wait_event_interruptible(pCmdNode->cmdwait_q, 
						pCmdNode->CmdWaitQWoken);
	}
		
	/* Copy the response back to user space */
	pCmdPtr = (HostCmd_DS_GEN *) tempResponseBuffer;

	if (copy_to_user(req->ifr_data, tempResponseBuffer, pCmdPtr->Size))
		PRINTK1("ERROR: copy_to_user failed!\n");

	kfree(tempResponseBuffer);

	LEAVE();
	return 0;
}

#endif	/* STDCMD */

int wlan_get_snr(wlan_private *priv, struct iwreq *wrq)
{
  int ret = 0;
  wlan_adapter *Adapter = priv->adapter;
  int *data = (int *)wrq->u.data.pointer;

  ENTER();

  if ((wrq->u.data.length == 0) || (data[0] == 0) || (data[0] == 1))
  { 
    ret = PrepareAndSendCommand(priv, 
          HostCmd_CMD_802_11_RSSI,
          0, HostCmd_OPTION_USE_INT
          | HostCmd_OPTION_WAITFORRSP
          , 0, HostCmd_PENDING_ON_NONE, NULL);

    if (ret) 
    {
      LEAVE();
      return ret;
    }
  }

  if (wrq->u.data.length == 0)
  {
    data[0] = Adapter->SNR[TYPE_BEACON][TYPE_NOAVG];
    data[1] = Adapter->SNR[TYPE_BEACON][TYPE_AVG];

    if ((jiffies - Adapter->RxPDSNRAge) <= VALID_SRANGE) 
      data[2] = Adapter->SNR[TYPE_RXPD][TYPE_NOAVG];
    else 
      data[2] = 0;

    data[3] = Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE;
    wrq->u.data.length = 4;
  }
  else if (data[0] == 0)
  {
    data[0] = Adapter->SNR[TYPE_BEACON][TYPE_NOAVG];
    wrq->u.data.length = 1;
  }
  else if (data[0] == 1)
  {
    data[0] = Adapter->SNR[TYPE_BEACON][TYPE_AVG];
    wrq->u.data.length = 1;
  }
  else if (data[0] == 2)
  {
    if ((jiffies - Adapter->RxPDSNRAge) <= VALID_SRANGE) {
      data[0] = Adapter->SNR[TYPE_RXPD][TYPE_NOAVG];
    } 
    else 
    {
      data[0] = 0;
    }
    wrq->u.data.length = 1;
  }
  else if (data[0] == 3)
  {
    data[0] = Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE;
    wrq->u.data.length = 1;
  }
  else
  {
    return -ENOTSUPP;
  }
    
  LEAVE();
  return 0;
}

#ifdef BG_SCAN 
int wlan_bg_scan_enable(wlan_private *priv, BOOLEAN enable)
{
	wlan_adapter    *Adapter = priv->adapter;
	int		ret;
#ifdef WPRM_DRV
	static BOOLEAN	irqRegistered = FALSE;
#endif 
	/* Clear the previous scan result */
	Adapter->ulNumOfBSSIDs = 0;
	memset(Adapter->BSSIDList, 0, 
		sizeof(WLAN_802_11_BSSID) * MRVDRV_MAX_BSSID_LIST);
#ifdef WPRM_DRV
        if((enable == FALSE) && (irqRegistered == TRUE)) {
		/* free WLAN_HOST_WAKEB irq */
	        free_irq(IRQ_GPIO(GPIO_WLAN_HOST_WAKEB), (void *)priv);
#if 1 
	        printk("wprm: free IRQ WLAN_HOST_WAKEB.\n");
#endif
		irqRegistered = FALSE;
	}
	else if((enable == TRUE) && (irqRegistered == FALSE)) {
	        /* register IRQ for WLAN_HOST_WAKEB */
	        set_GPIO_IRQ_edge(GPIO_WLAN_HOST_WAKEB, GPIO_RISING_EDGE);
#if 1   /* debug info */
	        printk("wprm: request IRQ on WLAN_HOST_WAKEB.\n");
        	/* get WLAN_HOST_WKEB LEVEL */
	        if(GPIO_is_high(GPIO_WLAN_HOST_WAKEB))
	                printk("wprm: WLAN_HOST_WAKEB is high.\n");
	        else
	                printk("wprm: WLAN_HOST_WAKEB is low.\n");
#endif
        	if (request_irq(IRQ_GPIO(GPIO_WLAN_HOST_WAKEB), wprm_wlan_host_wakeb_handler, \
                	SA_INTERRUPT, "wlan_host_wakeb", (void*) priv)) {
	                printk("wprm: request irq for wlan_host_wakeb fails.\n");
        	}
		irqRegistered = TRUE;
	}
#endif
	ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_802_11_BG_SCAN_CONFIG,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, &enable);
	return ret;
}
#endif /* BG_SCAN */

#ifdef BULVERDE_SDIO
int wlan_sdio_clock(wlan_private *priv, BOOLEAN on)
{
	if (on)
		return start_bus_clock(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
	else
		return stop_bus_clock_2(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
}
#endif /* BULVERDE_SDIO */

#ifdef THROUGHPUT_TEST
int wlan_thruput(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter    *Adapter = priv->adapter;
	static u32	startJiffi;
	static u32	startT, endT, TXthruput, RXthruput;

	ENTER();
	
	if (wrq->u.data.pointer) {

		PRINTK("The value is %d\n", (int)*wrq->u.data.pointer);		

		Adapter->ThruputTest = (int)*wrq->u.data.pointer;
		switch((int)*wrq->u.data.pointer) {
		case 0:
			endT = get_utimeofday();
			printk("StartJiffi %lu, EndJiffi %lu, Num times Tx %d,"
				       "Num time Rx %d, diff %lu\n",
					startJiffi, jiffies, Adapter->NumTransfersTx,
					Adapter->NumTransfersRx, (jiffies - startJiffi));
			TXthruput = (Adapter->NumTransfersTx * 1530 * 8) / ((endT - startT) / 1000);
			RXthruput = (Adapter->NumTransfersRx * 1530 * 8) / ((endT - startT) / 1000);
			printk("StartT=%lu.%lu.%lu, EndT=%lu.%lu.%lu, Num times Tx %d, "
				       "Num time Rx %d, diff %lu.%lu.%lu, "
					"TXthruput=%lu.%lu Mbps, RXthruput=%lu.%lu Mbps\n",
					startT/1000000,(startT%1000000)/1000,(startT%1000000)%1000,
					endT/1000000,(endT%1000000)/1000,(endT%1000000)%1000,
 					Adapter->NumTransfersTx,Adapter->NumTransfersRx,
					(endT-startT)/1000000,((endT-startT)%1000000)/1000,((endT-startT)%1000000)%1000,
					TXthruput/1000,TXthruput%1000,
					RXthruput/1000,RXthruput%1000
					);
			break;
		case 1:
		case 3:
			startJiffi = jiffies;
			startT = get_utimeofday();
			priv->adapter->NumTransfersRx = 0;
			priv->adapter->NumTransfersTx = 0;
			priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
			priv->adapter->IntCounter = 1;
			wake_up_interruptible(&priv->MainThread.waitQ);
			break;
		case 2:
			startJiffi = jiffies;
			startT = get_utimeofday();
			priv->adapter->NumTransfersRx = 0;
			priv->adapter->NumTransfersTx = 0;
			priv->adapter->IntCounter = 1;
			wake_up_interruptible(&priv->MainThread.waitQ);
			break;
		default:
			break;
		}
	}

	return 0;
}
#endif /* THROUGHPUT */

int wlan_get_rssi(wlan_private *priv, struct iwreq *wrq)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;
	int		temp;
	
	ENTER();
			
	if (wrq->u.data.pointer) {
		if (((int)*wrq->u.data.pointer == 0) ||
				((int)*wrq->u.data.pointer == 1)) { 
			ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_RSSI,
					0, HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE, NULL);

			if (ret) {
				LEAVE();
				return ret;
			}
		}

		switch ((int)*wrq->u.data.pointer) {
		case 0:
		temp = CAL_RSSI(Adapter->SNR[TYPE_BEACON][TYPE_NOAVG],
					Adapter->NF[TYPE_BEACON][TYPE_NOAVG]);
				break;
		case 1:
		temp = CAL_RSSI(Adapter->SNR[TYPE_BEACON][TYPE_AVG],
				Adapter->NF[TYPE_BEACON][TYPE_AVG]);
				break;
		case 2:
		temp = CAL_RSSI(Adapter->SNR[TYPE_RXPD][TYPE_NOAVG],
					Adapter->NF[TYPE_RXPD][TYPE_NOAVG]);
				break;
		case 3:
		temp = CAL_RSSI(Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE,
				Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE);
				break;
		default:
			return -ENOTSUPP;
		}
		
		if (copy_to_user(wrq->u.data.pointer, 
					&temp, sizeof(int))) {
			PRINTK1("copy_to_user failed!\n");
			return -EFAULT;
		}
		wrq->u.data.length = 1;
	} else
		return -EINVAL;
 
	LEAVE();
	return 0;
}

int wlan_get_nf(wlan_private *priv, struct iwreq *wrq)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;
	int		temp;

	ENTER();

	if (wrq->u.data.pointer) {
		if (((int)*wrq->u.data.pointer == 0) ||
				((int)*wrq->u.data.pointer == 1) ) { 
			ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_RSSI,
					0, HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE, NULL);

			if (ret) {
				LEAVE();
				return ret;
			}
		}

		switch ((int)*wrq->u.data.pointer) {
			case 0:
				temp = Adapter->NF[TYPE_BEACON][TYPE_NOAVG];
				break;
			case 1:
				temp = Adapter->NF[TYPE_BEACON][TYPE_AVG];
				break;
			case 2:
				temp = Adapter->NF[TYPE_RXPD][TYPE_NOAVG];
				if ((jiffies - Adapter->RxPDSNRAge) <= 
								VALID_SRANGE) {
				temp = Adapter->NF[TYPE_RXPD][TYPE_NOAVG];
				} else {
					temp		= 0;
				}
				break;
			case 3:
				temp = Adapter->
					NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE;
				break;
			default:
				return -ENOTSUPP;
		}

		temp = CAL_NF(temp);
		
		PRINTK1("***temp = %d\n", temp);		
		if (copy_to_user(wrq->u.data.pointer, 
					&temp, sizeof(int))) {
			PRINTK1("copy_to_user failed!\n");
			return -EFAULT;
		}
		wrq->u.data.length = 1;
	} else
		return -EINVAL;
		
	LEAVE();
	return 0;
}

#ifdef ADHOCAES
int wlan_remove_aes(wlan_private *priv)
{
	wlan_adapter	*Adapter = priv->adapter;	

#ifdef WPA2
	WLAN_802_11_KEY	key;
#else
	u8	temp[16];
#endif

	int 	ret;


	ENTER();

	if(Adapter->InfrastructureMode != Wlan802_11IBSS ||
			Adapter->MediaConnectStatus == 
					WlanMediaStateConnected)
		return -EOPNOTSUPP;
					
	Adapter->AdhocAESEnabled = FALSE;

#ifdef WPA2
	memset(&key, 0, sizeof(WLAN_802_11_KEY));
	PRINTK2("WPA2: DISABLE AES_KEY\n");
	key.KeyLength = WPA_AES_KEY_LEN;
	key.KeyIndex = 0x40000000;
/*	memcpy(key.KeyMaterial, key_hex, key.KeyLength );
*/

	ret = PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_KEY_MATERIAL,
		HostCmd_ACT_SET,
		HostCmd_OPTION_USE_INT
		| HostCmd_OPTION_WAITFORRSP
		, !(KEY_INFO_ENABLED), HostCmd_PENDING_ON_SET_OID,
		&key);

#else
	ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_PWK_KEY,	
					HostCmd_ACT_REMOVE_AES, 
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_SET_OID,
					&temp);
#endif
	
	LEAVE();
	
	return ret;
}
#endif

static int wlan_extscan_ioctl(wlan_private *priv, struct ifreq *req)
{
	WLAN_802_11_SSID Ext_Scan_SSID;
	union iwreq_data wrqu;

	ENTER();

	if (copy_from_user(&Ext_Scan_SSID, req->ifr_data,
				sizeof(Ext_Scan_SSID))) {
		PRINTK1("copy of SSID for ext scan from user failed \n");
		LEAVE();
 		return -EFAULT;
	}
	SendSpecificScan(priv, &Ext_Scan_SSID);

	memset(&wrqu, 0, sizeof(union iwreq_data));
#ifdef linux
	wireless_send_event(priv->wlan_dev.netdev, SIOCGIWSCAN, &wrqu , NULL);
#endif

	LEAVE();
	return 0;
}

/* Listen to a Specific MAC Address */
static int wlan_scan_bssid_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter	*Adapter = priv->adapter;
	u8  		mac[ETH_ALEN];

	ENTER();

	if (copy_from_user(mac, wrq->u.data.pointer, sizeof(mac))) {
		PRINTK("Copy from User space failed\n");
		return -EFAULT;
	}

	memcpy(Adapter->SpecificScanBSSID, mac, ETH_ALEN);
	HEXDUMP("SpecificScanBSSID:", Adapter->SpecificScanBSSID, ETH_ALEN);
	
	LEAVE();
	return 0;
}

static int wlan_getrate_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter		*Adapter = priv->adapter;
	WLAN_802_11_RATES	rates;
	int			i;

	ENTER();

	memset(rates, 0, sizeof(rates));
	wrq->u.data.length = get_active_data_rates(Adapter, rates);

	for (i = 0; i < wrq->u.data.length; i++)
		rates[i] &= ~0x80;

	if (copy_to_user(wrq->u.data.pointer, rates, wrq->u.data.length)) {
		PRINTK("Copy to user failed\n");
		return -EFAULT;
	}

	LEAVE();
	return 0;
}

static int wlan_get_adhoc_status_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	char 		status[64];
	wlan_adapter	*Adapter = priv->adapter;
	
	memset(status, 0, sizeof(status));

	switch (Adapter->InfrastructureMode) {
	case Wlan802_11IBSS:
		if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
			if (Adapter->AdhocCreate == TRUE)
				memcpy(&status, "AdhocStarted", sizeof(status));
			else
				memcpy(&status, "AdhocJoined", sizeof(status));
		} else {
			memcpy(&status, "AdhocIdle", sizeof(status));
		}
		break;
	case Wlan802_11Infrastructure:	
		memcpy(&status, "InfraMode", sizeof(status));
		break;
	default:	
		memcpy(&status, "AutoUnknownMode", sizeof(status));
		break;
	}

	PRINTK("Status = %s\n", status);
	wrq->u.data.length = strlen(status) + 1;

	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, 
					&status, wrq->u.data.length))
			return -EFAULT;
	}

	LEAVE();
	return 0;
}

#ifdef	DTIM_PERIOD
static int wlan_setdtim_ioctl(wlan_private *priv, struct ifreq *req)
{
	int	DTIM_multiple = 0, ret;

	ENTER();

	DTIM_multiple = (int) req->ifr_data;
	ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_SET_DTIM_MULTIPLE,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, 
			&DTIM_multiple);

	LEAVE();
	return ret;
}
#endif

static int wlan_version_ioctl(wlan_private *priv, struct ifreq *req)
{
	int	len;
	char	buf[128];
	struct iwreq	*wrq = (struct iwreq *) req;

	ENTER();

	get_version(priv->adapter, buf, sizeof(buf) - 1);

	len = strlen(buf);
	if (wrq->u.data.pointer) {
		if(copy_to_user(wrq->u.data.pointer, buf, len)){
			PRINTK1("CopyToUser failed\n");
			return -EFAULT;
		}
		wrq->u.data.length = len;
	}

	PRINTK1("wlan version: %s\n", buf);
/*
#ifdef BULVERDE_SDIO
	sdio_print_imask(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
	PRINTK("ps_sleep_confirmed=%d, psstate=%ld\n",
				ps_sleep_confirmed, priv->adapter->PSState);
	sdio_clear_imask(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
	PRINTK("ps_sleep_confirmed=%d, psstate=%ld\n",
				ps_sleep_confirmed,priv->adapter->PSState);
#endif
*/

#ifdef	WLAN_MEM_CHECK
	/*
	 * TODO: Move the display of this into a proc file 
	 */
	DisplayMemCounters();
#endif

	LEAVE();
	return 0;
}

static int wlan_regrdwr_ioctl(wlan_private *priv, struct ifreq *req)
{
	wlan_ioctl_regrdwr	regrdwr;
	wlan_offset_value	offval;
	wlan_adapter		*Adapter = priv->adapter;
	int			ret = 0;

	ENTER();

	if(copy_from_user(&regrdwr, req->ifr_data,	sizeof(regrdwr))){
		PRINTK1("copy of regrdwr for wlan_regrdwr_ioctl from user failed \n");
		LEAVE();
 		return -EFAULT;	
	}

	if(regrdwr.WhichReg == REG_EEPROM) {
		printk("Inside RDEEPROM\n");
		Adapter->pRdeeprom = (char *) kmalloc(regrdwr.NOB, GFP_KERNEL);
		if(!Adapter -> pRdeeprom)
			return -ENOMEM;
		/* +14 is for Action, Offset, and NOB in 
		 * response */
			PRINTK1("Action:%d Offset: %lx NOB: %02x\n",
						regrdwr.Action, 
						regrdwr.Offset, regrdwr.NOB);

			ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_EEPROM_ACCESS,
					regrdwr.Action,	HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE, &regrdwr);
	
			if (ret) {
				if(Adapter -> pRdeeprom)
					kfree(Adapter -> pRdeeprom);
				LEAVE();
				return ret;
			}

			mdelay(10);

			/*
			 * Return the result back to the user 
			 */

			memcpy(&regrdwr.Value, Adapter->pRdeeprom, regrdwr.NOB);

			if (regrdwr.Action == HostCmd_ACT_GEN_READ) {
				if(copy_to_user(req->ifr_data, &regrdwr, 
				sizeof(regrdwr) + regrdwr.NOB - 1))
				{
					PRINTK1("copy of regrdwr for wlan_regrdwr_ioctl to user failed \n");
					if(Adapter -> pRdeeprom)
						kfree(Adapter -> pRdeeprom);
					LEAVE();
 					return -EFAULT;	
				}
			}
			
			if(Adapter -> pRdeeprom)
				kfree(Adapter -> pRdeeprom);

			return 0;
	}

	offval.offset = regrdwr.Offset;
	offval.value = (regrdwr.Action) ? regrdwr.Value : 0x00;

	PRINTK1("RegAccess: %02x Action:%d "
			"Offset: %04x Value: %04x\n",
			regrdwr.WhichReg, regrdwr.Action, 
			offval.offset, offval.value);

	/*
	 * regrdwr.WhichReg should contain the command that
	 * corresponds to which register access is to be 
	 * performed HostCmd_CMD_MAC_REG_ACCESS 0x0019
	 * HostCmd_CMD_BBP_REG_ACCESS 0x001a 
	 * HostCmd_CMD_RF_REG_ACCESS 0x001b 
	 */
	ret = PrepareAndSendCommand(priv, regrdwr.WhichReg, 
			regrdwr.Action, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, &offval);

	if (ret) {
		LEAVE();
		return ret;
	}

	mdelay(10);

	/*
	 * Return the result back to the user 
	 */
	regrdwr.Value = Adapter->OffsetValue.value;
	if (regrdwr.Action == HostCmd_ACT_GEN_READ) {
		if(copy_to_user(req->ifr_data, &regrdwr, sizeof(regrdwr))){
			PRINTK1("copy of regrdwr for wlan_regrdwr_ioctl to user failed \n");
			LEAVE();
 			return -EFAULT;	
		}
	}

	LEAVE();
	return 0;
}
		
#if (defined SD83xx) || (defined SPI83xx)
static int wlan_cmd52rdwr_ioctl(wlan_private *priv, struct ifreq *req)
{
	u8 buf[7];
	u8 rw, func, dat = 0xff;
	u32 reg;

	ENTER();

	copy_from_user(buf, req->ifr_data, sizeof(buf));

	rw = buf[0];
	func = buf[1];
	reg = buf[5];
	reg = (reg << 8) + buf[4];
	reg = (reg << 8) + buf[3];
	reg = (reg << 8) + buf[2];

	if(rw != 0)
		dat = buf[6];
	
	PRINTK("rw=%d func=%d reg=0x%08X dat=0x%02X\n",rw,func,	reg,dat);

	if(rw == 0) {
		if (sbi_read_ioreg(priv, func, reg, &dat) < 0) {
			PRINTK("sdio_read_ioreg: reading register 0x%X failed\n"
									,reg);
			dat = 0xff;
		}
	}
	else {
		if (sbi_write_ioreg(priv, func, reg, dat) < 0) {
			PRINTK("sdio_read_ioreg: writing register 0x%X failed\n"
									,reg);
			dat = 0xff;
		}
	}
	copy_to_user(req->ifr_data, &dat, sizeof(dat));

	LEAVE();
	return 0;
}

static int wlan_cmd53rdwr_ioctl(wlan_private *priv, struct ifreq *req)
{
	return -EINVAL;
#if 0
	char buf[CMD53BUFLEN];
	int i;
	int ret = 0;
	int type = -1, nb = -1;

	ENTER();
	if(copy_from_user(buf, req->ifr_data, sizeof(buf))){
		PRINTK1("wlan_cmd53rdwr_ioctl copy from user failed \n");
		LEAVE();
 		return -EFAULT;	
	}
	for(i=0; i < sizeof(buf); i++)
		buf[i] ^= 0xff;

	ret = sbi_host_to_card(priv, MVMS_DAT, (u8 *) buf, sizeof(buf));
	if(ret < 0)
		PRINTK("CMD53 sbi_host_to_card failed: ret=%d\n",ret);

	for(i=0; i < sizeof(buf); i++)
		buf[i] ^= 0xff;

	ret = mv_sdio_card_to_host(priv, &type, &nb, buf, sizeof(buf));

	if(ret < 0)
		PRINTK("CMD53 mv_sdio_card_to_host failed: ret=%d\n",ret);
	else
		PRINTK("type=%d nb=%d\n",type,nb);

	for(i=0; i < sizeof(buf); i++)
		buf[i] ^= 0xff;

	if(copy_to_user(req->ifr_data, buf, sizeof(buf))){
		PRINTK1("wlan_cmd53rdwr_ioctl copy to user failed \n");
		LEAVE();
 		return -EFAULT;	
	}

	ret = 0;

	LEAVE();
	return ret;
#endif	
}
#endif


#ifdef CIPHER_TEST
static int wlan_cipher_test(wlan_private *priv, struct iwreq *wrq)
{
  int ret = 0;
  HostCmd_DS_802_11_KEY_ENCRYPT cmd;
	ENTER();

  memset(&cmd, 0, sizeof(cmd));
	copy_from_user(&cmd, wrq->u.data.pointer, sizeof(cmd));
	HEXDUMP("key_encrypt", (u8 *)&cmd, sizeof(cmd));

	ret = PrepareAndSendCommand(
			priv,
			HostCmd_CMD_802_11_KEY_ENCRYPT,
			HostCmd_ACT_SET,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0,
			HostCmd_PENDING_ON_NONE,
			&cmd
			);

  copy_to_user(wrq->u.data.pointer, &cmd, sizeof(cmd));
	LEAVE();
	return ret;
}
#endif

#ifdef ADHOCAES
static int wlan_setadhocaes_ioctl(wlan_private *priv, struct ifreq *req)
{
	u8  key_ascii[32];
	u8  key_hex[16];
	int ret;
	struct iwreq *wrq = (struct iwreq *) req;
	wlan_adapter *Adapter = priv->adapter;

#ifdef WPA2
	WLAN_802_11_KEY		key;
#endif

	ENTER();

	if (Adapter->InfrastructureMode != Wlan802_11IBSS)
		return -EOPNOTSUPP;

	if (Adapter->MediaConnectStatus == WlanMediaStateConnected)
		return -EOPNOTSUPP;

	if(copy_from_user(key_ascii, wrq->u.data.pointer, sizeof(key_ascii))){
		PRINTK1("wlan_setadhocaes_ioctl copy from user failed \n");
		LEAVE();
 		return -EFAULT;	
	}
  
	Adapter->AdhocAESEnabled = TRUE;
	ascii2hex(key_hex, key_ascii, sizeof(key_hex));

	HEXDUMP("wlan_setadhocaes_ioctl", key_hex, sizeof(key_hex));

#ifdef WPA2
	PRINTK2("WPA2: ENABLE AES_KEY\n");
	key.KeyLength = WPA_AES_KEY_LEN;
	key.KeyIndex = 0x40000000;
	memcpy(key.KeyMaterial, key_hex, key.KeyLength );

	ret = PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_KEY_MATERIAL,
		HostCmd_ACT_SET,
		HostCmd_OPTION_USE_INT
		| HostCmd_OPTION_WAITFORRSP
 		, KEY_INFO_ENABLED, HostCmd_PENDING_ON_SET_OID,
		&key);

#else
	ret = PrepareAndSendCommand(
			priv,
			HostCmd_CMD_802_11_PWK_KEY, 
			HostCmd_ACT_SET_AES, /* 3 */
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, 
			HostCmd_PENDING_ON_SET_OID,
			key_hex
			);
#endif

	LEAVE();
	return ret;
}
        
static int wlan_getadhocaes_ioctl(wlan_private *priv, struct ifreq *req)
{
	u8 *tmp;
	u8 key_ascii[33];
	u8 key_hex[16];
	int i, ret;
	struct iwreq *wrq = (struct iwreq *) req;
	wlan_adapter *Adapter = priv->adapter;

#ifdef WPA2
	WLAN_802_11_KEY		key;
#endif

	ENTER();

	memset(key_hex, 0x00, sizeof(key_hex));

#ifdef WPA2
	PRINTK2("WPA2: ENABLE AES_KEY\n");
	key.KeyLength = WPA_AES_KEY_LEN;
	key.KeyIndex = 0x40000000;
	memcpy(key.KeyMaterial, key_hex, key.KeyLength );

	ret = PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_KEY_MATERIAL,
		HostCmd_ACT_GET,
		HostCmd_OPTION_USE_INT
		| HostCmd_OPTION_WAITFORRSP
 		, KEY_INFO_ENABLED, HostCmd_PENDING_ON_SET_OID,
		&key);
	

#else
	ret = PrepareAndSendCommand(
			priv,
			HostCmd_CMD_802_11_PWK_KEY,
			HostCmd_ACT_GET + 2,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, 
			HostCmd_PENDING_ON_SET_OID,
			key_hex
			);

#endif
	if (ret) {
		LEAVE();
		return ret;
	}

#ifdef WPA2
	memcpy(key_hex, Adapter->aeskey.KeyParamSet.Key, sizeof(key_hex));

#else
	memcpy(key_hex, Adapter->pwkkey.TkipEncryptKey, sizeof(key_hex));
#endif

	HEXDUMP("wlan_getadhocaes_ioctl", key_hex, sizeof(key_hex));

	wrq->u.data.length = sizeof(key_ascii) + 1;

	memset(key_ascii, 0x00, sizeof(key_ascii));
	tmp = key_ascii;
            
	for (i = 0; i < sizeof(key_hex); i++) 
		tmp += sprintf(tmp, "%02x", key_hex[i]);

	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, &key_ascii, 
					sizeof(key_ascii))) {
			PRINTK1("copy_to_user failed\n");
			return -EFAULT;
		}
	}

	LEAVE();
	return ret;
}
#endif

#ifdef WPA
static int wlan_setwpaie_ioctl(wlan_private *priv, struct ifreq *req)
{
	struct iwreq	*wrq = (struct iwreq *) req;
	wlan_adapter	*Adapter = priv->adapter;
	int		ret = 0;

	ENTER();

	if (wrq->u.data.pointer != NULL) {
		if (wrq->u.data.length > sizeof(Adapter->Wpa_ie)){
			PRINTK1("failed to copy WPA IE, too big \n");
			return -EFAULT;
		}
		if (copy_from_user(Adapter->Wpa_ie, wrq->u.data.pointer,
							wrq->u.data.length)) {
			PRINTK1("failed to copy WPA IE \n");
			return -EFAULT;
		}
		Adapter->Wpa_ie_len = wrq->u.data.length;
		PRINTK("Set Wpa_ie_len=%d IE=%#x\n", Adapter->Wpa_ie_len, 
							Adapter->Wpa_ie[0]);
		HEXDUMP("Wpa_ie", Adapter->Wpa_ie, Adapter->Wpa_ie_len);
		if (Adapter->Wpa_ie[0] == WPA_IE)
			Adapter->SecInfo.WPAEnabled = TRUE;
#ifdef WPA2
		else if (Adapter->Wpa_ie[0] == WPA2_IE)
			Adapter->SecInfo.WPA2Enabled = TRUE;
#endif //WPA2
		else {
			Adapter->SecInfo.WPAEnabled = FALSE;
#ifdef WPA2
			Adapter->SecInfo.WPA2Enabled = FALSE;
#endif //WPA2
		}
	} else {
		memset(Adapter->Wpa_ie, 0, sizeof(Adapter->Wpa_ie));
		Adapter->Wpa_ie_len = wrq->u.data.length;
		PRINTK("Reset Wpa_ie_len=%d IE=%#x\n", Adapter->Wpa_ie_len,
							Adapter->Wpa_ie[0]);
		Adapter->SecInfo.WPAEnabled = FALSE;
#ifdef WPA2
		Adapter->SecInfo.WPA2Enabled = FALSE;
#endif //WPA2
	}

	// enable/disable RSN in firmware if WPA is enabled/disabled
	// depending on variable Adapter->SecInfo.WPAEnabled is set or not
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_ENABLE_RSN,
			HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	LEAVE();
	return ret;
}
#endif //WPA

#ifdef PS_REQUIRED
static int wlan_setpretbtt_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		preTbttTime, ret;
	struct	ifreq	*treq;

	ENTER();
	
	PRINTK("WLANSET_PRETBTT\n");
	treq = (struct ifreq *)((u8 *)req + 4);

	preTbttTime = (int)treq->ifr_data;

	if(preTbttTime <= 0) {
		PRINTK1("Invalid value\n");
		return -EINVAL;
	}
			
	PRINTK1("Set Pre-TBTT Time to %d\n", preTbttTime);
	ret = PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_PRE_TBTT,
		HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
		| HostCmd_OPTION_WAITFORRSP
		, 0, HostCmd_PENDING_ON_NONE,
		&preTbttTime);
	
	LEAVE();
	return ret;
}
#endif

static int wlan_set_atim_window_ioctl(wlan_private *priv, struct ifreq *req)
{
	wlan_adapter	*Adapter = priv->adapter;
	struct	ifreq	*treq;

	ENTER();
	
	PRINTK1("WLAN_SET_ATIM_WINDOW\n");
	treq = (struct ifreq *)((u8 *)req + 4);
	Adapter->AtimWindow = (int)treq->ifr_data;
	Adapter->AtimWindow = MIN(Adapter->AtimWindow, 50);
	PRINTK("AtimWindow is set to %d\n", Adapter->AtimWindow);

	LEAVE();
	return 0;
}

static int wlan_setregion_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		region_code;
	struct	ifreq	*treq;

	ENTER();

	treq = (struct ifreq *)((u8 *)req + 4);
	region_code = (int) treq->ifr_data;

	if (wlan_set_region(priv, (u16) region_code))
		return -EFAULT;
	
	LEAVE();
	return 0;
}

static int wlan_set_listen_interval_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		interval;
	struct	ifreq	*treq;

	ENTER();
					
	treq = (struct ifreq *)((u8 *)req + 4);
	interval = (int) treq->ifr_data;
					
	priv->adapter->ListenInterval = (u16) interval;
		
	LEAVE();
	return 0;
}

static int wlan_subcmd_setrxantenna_ioctl(wlan_private *priv, struct ifreq *req)
{
	struct	ifreq	*treq;
	PRINTK1("WLAN_SUBCMD_SETRXANTENNA\n");
	treq = (struct ifreq *)((u8 *)req + 4);
	return SetRxAntenna(priv, (int)treq->ifr_data);
}

static int wlan_subcmd_settxantenna_ioctl(wlan_private *priv, struct ifreq *req)
{
	struct	ifreq	*treq;
	PRINTK1("WLAN_SUBCMD_SETTXANTENNA\n");
	treq = (struct ifreq *)((u8 *)req + 4);
	return SetTxAntenna(priv, (int)treq->ifr_data);
}

#ifdef PS_REQUIRED
static int wlan_set_multiple_dtim_ioctl(wlan_private *priv, struct ifreq *req)
{
	struct	ifreq	*treq;
	u32		mdtim;
	int		ret = -EINVAL;

	ENTER();
	
	treq = (struct ifreq *)((u8 *)req + 4);
	if (treq) {
		mdtim = (u32)treq->ifr_data;
		if (((mdtim >= MRVDRV_MIN_MULTIPLE_DTIM) &&
			(mdtim <= MRVDRV_MAX_MULTIPLE_DTIM))
			|| (mdtim == MRVDRV_IGNORE_MULTIPLE_DTIM)
		) {
			priv->adapter->MultipleDtim = mdtim;
			ret = 0;
		}
	}

	if (ret)
		PRINTK("Invalid parameter, MultipleDtim not changed.\n");

	LEAVE();
	return ret;
}
#endif

static int wlan_setauthalg_ioctl(wlan_private *priv, struct ifreq *req)
{
	u16		alg;
	struct	ifreq	*treq;
	struct iwreq	*wrq = (struct iwreq *) req;
	wlan_adapter	*Adapter = priv->adapter;
	
	ENTER();

	if (wrq->u.data.flags == 0) {
		//from iwpriv subcmd
		treq = (struct ifreq *)((u8 *)req + 4);
		alg = (int)treq->ifr_data;
	} else {	
		//from wpa_supplicant subcmd
		alg = (int)*wrq->u.data.pointer;
	}
	
	PRINTK("auth alg is %#x\n",alg);

	switch (alg) {
		case AUTH_ALG_SHARED_KEY:
			Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeShared;
			break;
		case AUTH_ALG_NETWORK_EAP:
			Adapter->SecInfo.AuthenticationMode = 
						Wlan802_11AuthModeNetworkEAP;
			break;
		case AUTH_ALG_OPEN_SYSTEM:
		default:
			Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeOpen;
			break;
	}
	
	LEAVE();
	return 0;
}

static int wlan_set8021xauthalg_ioctl(wlan_private *priv, struct ifreq *req)
{
	u16		alg;
	struct	ifreq	*treq;
	struct iwreq	*wrq = (struct iwreq *) req;
	
	ENTER();
	
	if (wrq->u.data.flags == 0) {
		//from iwpriv subcmd
		treq = (struct ifreq *)((u8 *)req + 4);
		alg = (int)treq->ifr_data;
	} else {	
		//from wpa_supplicant subcmd
		alg = (int)*wrq->u.data.pointer;
	}
	PRINTK("802.1x auth alg is %#x\n",alg);
	priv->adapter->SecInfo.Auth1XAlg = alg;

	LEAVE();
	return 0;
}

static int wlan_setencryptionmode_ioctl(wlan_private *priv, struct ifreq *req)
{
	u16		mode;
	struct	ifreq	*treq;
	struct iwreq	*wrq = (struct iwreq *) req;

	ENTER();
	
	if (wrq->u.data.flags == 0) {
		//from iwpriv subcmd
		treq = (struct ifreq *)((u8 *)req + 4);
		mode = (int)treq->ifr_data;
	}
	else {	
		//from wpa_supplicant subcmd
		mode = (int)*wrq->u.data.pointer;
	}
	PRINTK("encryption mode is %#x\n",mode);
	priv->adapter->SecInfo.EncryptionMode = mode;

	LEAVE();
	return 0;
}

static int wlan_subcmd_getrxantenna_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		len;
	char		buf[8];
	struct iwreq	*wrq = (struct iwreq *) req;

	ENTER();

	PRINTK1("WLAN_SUBCMD_GETRXANTENNA\n");
	len = GetRxAntenna(priv, buf);

	wrq->u.data.length = len;
	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, &buf, len)) {
			PRINTK1("CopyToUser failed\n");
			return -EFAULT;
		}
	}

	LEAVE();
	return 0;
}

static int wlan_subcmd_gettxantenna_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		len;
	char		buf[8];
	struct iwreq	*wrq = (struct iwreq *) req;

	ENTER();

	PRINTK1("WLAN_SUBCMD_GETTXANTENNA\n");
	len = GetTxAntenna(priv, buf);

	wrq->u.data.length = len;
	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, &buf, len)) {
			PRINTK1("CopyToUser failed\n");
			return -EFAULT;
		}
	}

	LEAVE();
	return 0;
}

#ifdef DEEP_SLEEP
static int wlan_deepsleep_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		ret = 0;
#if defined(DEEP_SLEEP_CMD)
	char		status[128];
	struct iwreq	*wrq = (struct iwreq *) req;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
	
#ifdef PS_REQUIRED
	// check if it is in IEEE PS mode or not
	if (Adapter->PSMode != Wlan802_11PowerModeCAM) {
		printk("Cannot process Deep Sleep command in IEEE Power"
							" Save mode.\n");
		return -EINVAL;
	}
#endif

	if (req->ifr_data[0] == '0') {
		PRINTK("Exit Deep Sleep Mode.\n");
		sprintf(status, "setting to off ");
		SetDeepSleep(priv, FALSE);	
	} else if (req->ifr_data[0] == '1') {
		PRINTK("Enter Deep Sleep Mode.\n");
		sprintf(status, "setting to on ");
		SetDeepSleep(priv, TRUE);
	} else if (req->ifr_data[0] == '2') {
		PRINTK("Get Deep Sleep Mode.\n");
		if (Adapter->IsDeepSleep == TRUE) {
			sprintf(status, "on ");
		} else {
			sprintf(status, "off ");
		}
	} else {
		PRINTK("unknown option = %d\n",req->ifr_data[0]);
		return  -EINVAL;
	}

	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, &status, strlen(status)))
			return -EFAULT;
		wrq->u.data.length = strlen(status);
	}

#else	 // DEEP_SLEEP_CMD
	ENTER();
	ret = wlan_deep_sleep_ioctl(priv, req);
#endif // DEELP_SLEEP

	LEAVE();
	return ret;
}
#endif

#ifdef HOST_WAKEUP
int wlan_do_hostwakeupcfg_ioctl(wlan_private *priv, struct iwreq *wrq) 
{
	wlan_adapter	*Adapter = priv->adapter;
	char		buf[16];
	int		ret = 0;
	int 		gpio, gap;
	

	memset(buf, 0, sizeof(buf));
	if (copy_from_user(buf, wrq->u.data.pointer, 
				MIN(sizeof(buf)-1,
				wrq->u.data.length))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}
	buf[15] = 0;

	sscanf(buf,"%x %x %x",&Adapter->HostWakeupCfgParam.conditions,
		&gpio, &gap );
	Adapter->HostWakeupCfgParam.gpio = (u8)gpio;
	Adapter->HostWakeupCfgParam.gap	 = (u8)gap;

	printk("conditions=0x%x gpio=%d gap=%d\n",
		Adapter->HostWakeupCfgParam.conditions,
		Adapter->HostWakeupCfgParam.gpio,
		Adapter->HostWakeupCfgParam.gap);

	ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_HOST_WAKE_UP_CFG,
				0, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
				, 0, HostCmd_PENDING_ON_NONE,
				&Adapter->HostWakeupCfgParam);

	return ret;
}
#endif

int wlan_do_rfi_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int ret;

	ENTER();
	
	switch(wrq->u.data.flags)
	{
		case WLAN_TX_MODE :
			ret = wlan_rfi_txmode(priv, wrq);
			break;
					
		case WLAN_TX_CONTROL_MODE :		   
			ret  = wlan_rfi_txcontrolmode(priv, wrq);
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
	}
	
	LEAVE();

	return ret;
	
}

#ifdef SUBSCRIBE_EVENT_CTRL
int wlan_do_subscribe_event(wlan_private *priv, struct ifreq *req)
{
	EventSubscribe		*Events;
	wlan_adapter		*Adapter = priv->adapter;
	int			ret;
	
	ENTER();

	Events = kmalloc(sizeof(EventSubscribe), GFP_KERNEL);
	if(!Events){
		printk("kmalloc no memory !!\n");
		return -ENOMEM;
		
	}
	
	if (copy_from_user(Events, req->ifr_data + SKIP_CMDNUM,
			       sizeof(EventSubscribe))) {
			PRINTK1("Copy from user failed\n");
			kfree(Events);
			return -EFAULT;
	}
	HEXDUMP("SubscribeEvents: ", req->ifr_data + SKIP_CMDNUM,
					sizeof(EventSubscribe));

	ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_SUBSCRIBE_EVENT,
				0, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP,
			       	0, HostCmd_PENDING_ON_NONE, Events);

	if (Events->Action == HostCmd_ACT_GET) {
		PRINTK("Suscribed Events = 0x%X\n", Adapter->GetEvents);
		if (copy_to_user(req->ifr_data + SKIP_CMDNUM, 
					&Adapter->GetEvents, sizeof(u16))) {
			PRINTK1("Copy to user Failed\n");
			kfree(Events);
			return -EFAULT;
		}
	}

	kfree(Events);

	LEAVE();
	return ret;
}
#endif

#ifdef BG_SCAN 
int wlan_do_bg_scan_config_ioctl(wlan_private *priv, struct ifreq *req)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;
	u8		*buf;

	ENTER();
	
	PRINTK("Action = %d\n", 
		(u8) *(req->ifr_data + SKIP_CMDNUM + SKIP_SIZE));

	switch((u8) *(req->ifr_data + SKIP_CMDNUM + SKIP_SIZE)) {
	case HostCmd_ACT_GEN_GET:
		buf = kmalloc(Adapter->bgScanConfigSize + SKIP_SIZE, GFP_KERNEL);
		if(!buf){
			printk("kmalloc no memory !!\n");
			return -ENOMEM;
		}
		memcpy(buf, &Adapter->bgScanConfigSize, SKIP_SIZE);
		memcpy(buf + SKIP_TYPE_SIZE, Adapter->bgScanConfig, 
						Adapter->bgScanConfigSize);

		if (copy_to_user(req->ifr_data, buf, 
					Adapter->bgScanConfigSize + SKIP_SIZE)) {
			PRINTK1("Copy from user failed\n");
			kfree(buf);
			return -EFAULT;
		}

		kfree(buf);

		break;
	case HostCmd_ACT_GEN_SET:
		Adapter->bgScanConfigSize = (u16) *(req->ifr_data + SKIP_CMDNUM);
		PRINTK("bgscanConfigSize = %d\n", Adapter->bgScanConfigSize);
		
		if(Adapter->bgScanConfig)
			kfree(Adapter->bgScanConfig);

		if(!(Adapter->bgScanConfig = 
				kmalloc(Adapter->bgScanConfigSize, GFP_KERNEL))) {
			printk("kmalloc no memory !!\n");
			Adapter->bgScanConfigSize = 0;
			return -ENOMEM;
		}

		HEXDUMP("treq", req->ifr_data + SKIP_CMDNUM + SKIP_SIZE,
						Adapter->bgScanConfigSize);
		
		if (copy_from_user(Adapter->bgScanConfig, req->ifr_data + 
					SKIP_CMDNUM + SKIP_SIZE, 
					Adapter->bgScanConfigSize)) {
			PRINTK1("Copy from user failed\n");
			return -EFAULT;
		}

		break;
	}

	LEAVE();

	return ret;
}
#endif /* BG_SCAN */

#ifdef CAL_DATA
int wlan_do_caldata_ioctl(wlan_private *priv, struct ifreq *req)
{
	int			ret = 0;
	u8			buf[8];
	wlan_ioctl_cal_data	CalData;
	
	memset(&CalData, 0, sizeof(wlan_ioctl_cal_data));
	
	if (copy_from_user(&CalData, req->ifr_data + SKIP_TYPE,
						sizeof(wlan_ioctl_cal_data))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}
	memset(&priv->adapter->caldata, 0, sizeof(HostCmd_DS_COMMAND));
	switch (CalData.Action) {
	case WLANSET_CAL_DATA:
		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_CAL_DATA,
					HostCmd_ACT_GEN_SET,	
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE,
					&CalData);

		if (ret) {
			LEAVE();
			return ret;
		}
					
		if (copy_to_user(req->ifr_data + SKIP_TYPE, &priv->adapter->caldata,
					sizeof(HostCmd_DS_802_11_CAL_DATA) + 
					sizeof(HostCmd_DS_GEN))) {
			PRINTK1("Copy to user failed\n");
			return -EFAULT;
		}

		buf[0] = 0x01;
		buf[1] = 0x00;
		memcpy(buf + 2, CalData.MacAddr, ETH_ALEN);
		wlan_set_mac_address(priv->wlan_dev.netdev, (void *) buf);
		break;
			
	case WLANGET_CAL_DATA:
		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_CAL_DATA,	
					HostCmd_ACT_GEN_GET,
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE,
					&CalData);

		if (ret) {
			LEAVE();
			return ret;
		}

		if (copy_to_user(req->ifr_data + SKIP_TYPE,
					&priv->adapter->caldata,
					sizeof(HostCmd_DS_802_11_CAL_DATA) +
					sizeof(HostCmd_DS_GEN))) {
			PRINTK1("Copy to user failed\n");
			return -EFAULT;
		}
			break;
	}

	LEAVE();
	return ret;
}
#endif	/* CAL_DATA */

#ifdef CAL_DATA
int wlan_do_caldata_ext_ioctl(wlan_private *priv, struct ifreq *req)
{
	int			ret = 0;
	u8			buf[8];
	HostCmd_DS_802_11_CAL_DATA_EXT	CalData;
	
	ENTER();
	
	memset(&CalData, 0, sizeof(HostCmd_DS_802_11_CAL_DATA_EXT));
			
	if (copy_from_user(&CalData.Action, req->ifr_data + SKIP_CMDNUM,
						sizeof(HostCmd_DS_802_11_CAL_DATA_EXT))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}

	HEXDUMP("Cal data ext", (u8 *)&CalData.Action, 
					sizeof(HostCmd_DS_802_11_CAL_DATA_EXT));

	PRINTK("CalData.Action = 0x%0X\n", CalData.Action);
	
	switch (CalData.Action) {
	case WLANSET_CAL_DATA_EXT:
		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_CAL_DATA_EXT,
					HostCmd_ACT_GEN_SET,	
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE,
					&CalData);

		if (ret) {
			LEAVE();
			return ret;
		}
					
		if (copy_to_user(req->ifr_data, &priv->adapter->caldata,
					sizeof(HostCmd_DS_802_11_CAL_DATA_EXT) + 
					sizeof(HostCmd_DS_GEN))) {
			PRINTK1("Copy from user failed\n");
			return -EFAULT;
		}

		buf[0] = 0x01;
		buf[1] = 0x00;
		//memcpy(buf + 2, CalData.MacAddr, ETH_ALEN);
		// wlan_set_mac_address(priv->wlan_dev.netdev, (void *) buf);
		break;
			
	case WLANGET_CAL_DATA_EXT:
		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_CAL_DATA_EXT,	
					HostCmd_ACT_GEN_GET,
					HostCmd_OPTION_USE_INT
					| HostCmd_OPTION_WAITFORRSP
					, 0, HostCmd_PENDING_ON_NONE,
					&CalData);

		if (ret) {
			LEAVE();
			return ret;
		}

		if (copy_to_user(req->ifr_data, &priv->adapter->caldata,
					sizeof(HostCmd_DS_802_11_CAL_DATA_EXT) +
					sizeof(HostCmd_DS_GEN))) {
			PRINTK1("Copy from user failed\n");
			return -EFAULT;
		}
			break;
	}

	LEAVE();
	return ret;
}
#endif	/* CAL_DATA */

#ifdef MULTI_BANDS
int wlan_do_setband_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int		cnt, i;
	char		buf[16];
	u16		tmp = 0;
	wlan_adapter	*Adapter = priv->adapter;
				
	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) 
		return -EOPNOTSUPP;
				
	memset(buf, '\0', sizeof(buf));
	if(copy_from_user(buf, wrq->u.data.pointer, 
				MIN(wrq->u.data.length, sizeof(buf)-1))){
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}

	cnt = strlen(buf);

	if (!cnt || cnt > 3)
		return -EINVAL;

	for (i = 0; i < cnt; i++) {
		switch (buf[i]) {
			case 'B' :
			case 'b' :
				tmp |= BAND_B; 
				break;
			case 'G' :
			case 'g' :
				tmp |= BAND_G; 
				break;
			case 'A' :
			case 'a' :
				tmp |= BAND_A; 
				break;
			default:
			return -EINVAL;
		}
	}
		
	/* Validate against FW support */
	if ((tmp | Adapter->fw_bands) &
					~Adapter->fw_bands)
		return -EINVAL;
	
	/* To support only <a/b/bg/abg> */
	if (!tmp || (tmp == BAND_G) || (tmp == (BAND_A | BAND_B)) ||
					(tmp == (BAND_A | BAND_G)))
		return -EINVAL;
	
	if (wlan_set_regiontable(priv, Adapter->RegionCode, tmp)) {
		return -EINVAL;
		}

	Adapter->config_bands = tmp;

	if (Adapter->config_bands & BAND_A) {
		Adapter->adhoc_start_band = BAND_A; 
		Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL_A;
	}
	else if (Adapter->config_bands & BAND_G) {
		Adapter->adhoc_start_band = BAND_G; 
		Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL;
	}
	else {
		Adapter->adhoc_start_band = BAND_B; 
		Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL;
	}

	LEAVE();

	return 0;
}

int wlan_do_setadhocch_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	/* iwpriv ethX setaddocchannel "a nnn" */
	u32	vals[2];
	u16	band;
	u8	chan;
	wlan_adapter	*Adapter = priv->adapter;
				
	if(copy_from_user(vals, wrq->u.data.pointer, 
		MIN(wrq->u.data.length, sizeof(vals)))){
		PRINTK1("Copy from user failed\n");
		return -EFAULT;			

	}
	switch (vals[0]) { /* 1st byte is band */
		case 'B' :
		case 'b' :
			band = BAND_B; 
			break;
		case 'G' :
		case 'g' :
			band = BAND_G; 
			break;
		case 'A' :
		case 'a' :
			band = BAND_A; 
			break;
		default:
			return -EINVAL;
	}

	/* Validate against FW support */
	if ((band | Adapter->fw_bands) & ~Adapter->fw_bands)
		return -EINVAL;
	
	chan = vals[1];
	
	if (!find_cfp_by_band_and_channel(Adapter, band, chan)) {
		PRINTK("Invalid channel number %d\n", chan);
		return -EINVAL;
	}
		
	Adapter->adhoc_start_band = band; 
	Adapter->AdhocChannel   = chan;

	return 0;
}
				
int wlan_do_get_band_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	char	status[10], *ptr;
	u16	val = priv->adapter->config_bands;

	ENTER();

	memset(status, 0, sizeof(status));
	ptr = status;

	if (val & BAND_A)
	*ptr++ = 'a'; 

	if (val & BAND_B)
	*ptr++ = 'b';

	if (val & BAND_G)
	*ptr++ = 'g';

	*ptr = '\0';
				
	PRINTK("Status = %s\n", status);
	wrq->u.data.length = strlen(status) + 1;

	if (wrq->u.data.pointer) {
		if (copy_to_user(wrq->u.data.pointer, 
					&status, wrq->u.data.length))
			return -EFAULT;
		}

	LEAVE();

	return 0;
}

int wlan_do_get_adhocch_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	u32		status[2];
	wlan_adapter	*Adapter = priv->adapter;
	u16		band = Adapter->adhoc_start_band;

	ENTER();
	
	if (band & BAND_A) {
		status[0] = 'a';
	} else if (band & BAND_B) {
		status[0] = 'b';
	} else if (band & BAND_G) { 
		status[0] = 'g';
	}

	status[1] = Adapter->AdhocChannel;

	if (wrq->u.data.pointer) {
		wrq->u.data.length = sizeof(status);

		if (copy_to_user(wrq->u.data.pointer, status,
							wrq->u.data.length))
			return -EFAULT;
	}

	LEAVE();
	return 0;
}

#endif 

int wlan_sleep_period(wlan_private *priv, struct iwreq *wrq)
{
	int ret;		
        int *data = (int *)wrq->u.data.pointer;
	wlan_adapter *Adapter = priv->adapter;
	HostCmd_DS_802_11_SLEEP_PERIOD	sleeppd;
       
	ENTER();

	if (data[1])
		return -ENOTSUPP;
	
	memset(&sleeppd, 0, sizeof(sleeppd));
	memset(&Adapter->sleep_period, 0, sizeof(SleepPeriod));

        if (wrq->u.data.length == 0) {
		sleeppd.Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
        } else {
		/* sleep period is between 0 or 10 ... 60 */
		if ((data[0] <= 60 && data[0] >= 10) || (data[0] == 0)){
			sleeppd.Action = 
				wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			sleeppd.Period = data[0];
		} else
			return -EINVAL;
        }

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SLEEP_PERIOD,
			0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, (void *) &sleeppd);
	
	data[0] = (int) Adapter->sleep_period.period;

        wrq->u.data.length = 1;

	LEAVE();
	return ret;
}

int wlan_adapt_rateset(wlan_private *priv, struct iwreq *wrq)
{
	int			ret;
	wlan_adapter		*Adapter = priv->adapter;

	ENTER();

	if (!wrq->u.data.length) {
		PRINTK1("Get ADAPT RATE SET\n");
		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_RATE_ADAPT_RATESET,
				HostCmd_ACT_GEN_GET, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
			      	, 0, 0, NULL);
		*(int *)wrq->u.data.pointer = Adapter->EnableHwAuto;
		*(int *)((char *)wrq->u.data.pointer + sizeof(int)) =
							Adapter->RateBitmap;
#define GET_TWO_INT	2
		wrq->u.data.length = GET_TWO_INT;
	} else {
		PRINTK1("Set ADAPT RATE SET\n");
		Adapter->EnableHwAuto = *(int *)wrq->u.data.pointer;
		Adapter->RateBitmap = *(int *)((char *)wrq->u.data.pointer + 
								sizeof(int));
		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_RATE_ADAPT_RATESET,
				HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP
			      	, 0, 0, NULL);
	}

	LEAVE();
	return ret;	
}
int wlan_inactivity_timeout(wlan_private *priv, struct iwreq *wrq)
{
	int ret;		
        int *data = (int *)wrq->u.data.pointer;
	u16		timeout = 0;
	ENTER();
	if (data[1])
		return -ENOTSUPP;
	
        if (wrq->u.data.length == 0) {
		/* Get*/
		ret = PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_INACTIVITY_TIMEOUT,
				HostCmd_ACT_GET, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP,
				0, HostCmd_PENDING_ON_NONE, &timeout);
		data[0] = timeout;
        } else {
		/* Set */
		timeout =data[0];
		ret = PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_INACTIVITY_TIMEOUT,
				HostCmd_ACT_SET, HostCmd_OPTION_USE_INT
						| HostCmd_OPTION_WAITFORRSP,
				0, HostCmd_PENDING_ON_NONE, &timeout);
	}

        wrq->u.data.length = 1;

	LEAVE();
	return ret;
}

       
int wlan_do_getlog_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int		ret;
	char		buf[GETLOG_BUFSIZE-1];
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	PRINTK(" GET STATS\n"); 

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_GET_LOG,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	if (wrq->u.data.pointer) {
		sprintf(buf, "\n  mcasttxframe %u failed %u retry %u " 
				"multiretry %u framedup %u "
				"rtssuccess %u rtsfailure %u ackfailure %u\n"
				"rxfrag %u mcastrxframe %u fcserror %u "
				"txframe %u wepundecryptable %u ",
				Adapter->LogMsg.mcasttxframe,
				Adapter->LogMsg.failed,
				Adapter->LogMsg.retry,
				Adapter->LogMsg.multiretry,
				Adapter->LogMsg.framedup,
				Adapter->LogMsg.rtssuccess,
				Adapter->LogMsg.rtsfailure,
				Adapter->LogMsg.ackfailure,
				Adapter->LogMsg.rxfrag,
				Adapter->LogMsg.mcastrxframe,
				Adapter->LogMsg.fcserror,
				Adapter->LogMsg.txframe,
				Adapter->LogMsg.wepundecryptable);
		wrq->u.data.length = strlen(buf) + 1;
		copy_to_user(wrq->u.data.pointer, buf, wrq->u.data.length);
	}

	LEAVE();
	return 0;
}


//#if defined (CF8385) || defined (CF8381) || defined (CF8305)
#if defined (CF83xx)
int wlan_do_regcfrdwr_ioctl(wlan_private *priv, struct ifreq *req)
{
	wlan_ioctl_cfregrdwr regrdwr;

	ENTER();

	copy_from_user(&regrdwr, req->ifr_data, 
					sizeof(regrdwr));

	if (regrdwr.Action) {
		PRINTK("Write value into CF register\n");
		sbi_write_cfreg(priv, regrdwr.Offset, regrdwr.Value);
	} else {
		PRINTK("Read value from CF register\n");
		regrdwr.Value = sbi_read_cfreg(priv, 
		regrdwr.Offset);
		copy_to_user(req->ifr_data, &regrdwr, 
						sizeof(regrdwr));
	}

	LEAVE();
	return 0;
}
#endif

/*
 * PMU state machine oscillator and the PLL parameters
 */

int wlan_sleep_params_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int ret;
	wlan_adapter 	*Adapter = priv->adapter;
	wlan_ioctl_sleep_params_config  sp;

	ENTER();

	memset(&sp, 0, sizeof(sp));
	
	if (copy_from_user(&sp, wrq->u.data.pointer,
				MIN(sizeof(sp), wrq->u.data.length))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}
	
	memcpy(&Adapter->sp, &sp.Error, sizeof(SleepParams));
	
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SLEEP_PARAMS,
			sp.Action, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if (!ret && !sp.Action && wrq->u.data.pointer) {
		memcpy(&sp.Error, &Adapter->sp, sizeof(SleepParams));
		copy_to_user(wrq->u.data.pointer, &sp, sizeof(sp));
		wrq->u.data.length = sizeof(sp);
	}

	LEAVE();
	return ret;
}

/*
 *	Read the CIS Table
 */

int wlan_do_getcis_ioctl(wlan_private *priv, struct ifreq *req)
{
	int 		ret = 0;
	struct iwreq	*wrq = (struct iwreq *) req;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	if (wrq->u.data.pointer) {
		copy_to_user(wrq->u.data.pointer, Adapter->CisInfoBuf, 
							Adapter->CisInfoLen);
		wrq->u.data.length = Adapter->CisInfoLen;
	}

	LEAVE();
	return ret;
}

/*
 * We put our ioctl function - entry point over here so that it can use
 * all the above wireless extensions without clutering the name space. 
 */

int wlan_do_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
	int		subcmd = 0;
	int		ret = 0;
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	struct iwreq	*wrq = (struct iwreq *) req;

	ENTER();

#ifdef DEEP_SLEEP
	if ((Adapter->IsDeepSleep == TRUE)
			&& (cmd != WLANDEEPSLEEP)
			/* here we exclude all sub-cmds under SET1INT_GET1INT cmd
			   in order to get SDIOCLOCK sub-cmd to work; need a fix later */
			&& (cmd != WLAN_SETONEINT_GETONEINT)
		) {
		printk("<1>():%s IOCTLS called when station is"
					" in DeepSleep\n",__FUNCTION__);
		return -EBUSY;
	}
#endif

	PRINTK1("wlan_do_ioctl: ioctl cmd = 0x%x\n", cmd);
	switch (cmd) {
	case WLANEXTSCAN: 
		ret = wlan_extscan_ioctl(priv, req);
		break;
#ifdef STDCMD
	case WLANSTDCMD:
		ret = wlan_stdcmd_ioctl(dev, req, cmd);
		break;
#endif

#ifdef	DTIM_PERIOD
	case WLANSETDTIM:
		ret = wlan_setdtim_ioctl(priv, req);
		break;
#endif
			
	case WLANCISDUMP:	/* Read CIS Table  */
		ret = wlan_do_getcis_ioctl(priv, req);
		break;
		
	case WLANSCAN_TYPE :
		PRINTK1("Scan Type Ioctl\n");
		ret = wlan_scan_type_ioctl(priv, wrq);
		break;

#ifdef MANF_CMD_SUPPORT
	case WLANMANFCMD:
		PRINTK1("Entering the Manufacturing ioctl SIOCCFMFG\n");
		ret = wlan_mfg_command(priv, (void *) req->ifr_data);
		
		PRINTK1("Manufacturing Ioctl %s\n", (ret) ? "failed":"success");
		break;
#endif

#ifdef PASSTHROUGH_MODE
	case WLANPASSTHROUGH:	/* Passthrough mode command */
		PRINTK1("Passthrough Mode Ioctl\n");
		ret = wlan_passthrough_mode_ioctl(priv, req);
		break;

	case WLANPTDATA_RCVE:	/* Passthrough data */
		PRINTK1("Passthrough Data Rcve ioctl\n");
		ret = wlan_passthrough_data_ioctl(priv, req);
		break;
#endif

	case WLANREGRDWR:	/* Register read write command */
		ret = wlan_regrdwr_ioctl(priv, req);
		break;

#if (defined SD83xx) || (defined SPI83xx)
	case WLANCMD52RDWR:	/* CMD52 read/write command */
		ret = wlan_cmd52rdwr_ioctl(priv, req);
		break;

	case WLANCMD53RDWR:	/* CMD53 read/write command */
		ret = wlan_cmd53rdwr_ioctl(priv, req);
		break;
#endif

#ifdef WPA
	case SIOCSIWENCODE:	/* set encoding token & mode for WPA */
		ret = wlan_set_encode(dev, NULL, &(wrq->u.data),
							wrq->u.data.pointer);
		break;
#endif
	case WLAN_SETNONE_GETNONE : /* set WPA mode on/off ioctl #20 */
		switch(wrq->u.data.flags) {
		case WLANDEAUTH :
			PRINTK1("Deauth\n");
			wlan_send_deauth(priv);
			break;
			
		case WLANADHOCSTOP:
			PRINTK("Adhoc stop\n");
			ret = wlan_do_adhocstop_ioctl(priv);
			break;

		case WLANRADIOON:
			wlan_radio_ioctl(priv, RADIO_ON);
			break;
			
		case WLANRADIOOFF:
			wlan_radio_ioctl(priv, RADIO_OFF);
			break;
#ifdef ADHOCAES
		case WLANREMOVEADHOCAES:
			ret = wlan_remove_aes(priv);
			break;
#endif
#ifdef CIPHER_TEST
    case WLANCIPHERTEST:
      ret = wlan_cipher_test(priv, wrq);
      break;
#endif
		}	/* End of switch */
		break;

	case WLAN_SETWORDCHAR_GETNONE :
		switch(wrq->u.data.flags) {
#ifdef ADHOCAES
		case WLANSETADHOCAES:
			ret = wlan_setadhocaes_ioctl(priv, req);
			break;
#endif /* end of ADHOCAES */
		}
		break;

	case WLAN_SETNONE_GETWORDCHAR:
		switch(wrq->u.data.flags) {
#ifdef ADHOCAES
		case WLANGETADHOCAES:
			ret = wlan_getadhocaes_ioctl(priv, req);
			break;
#endif /* end of ADHOCAES */
		case WLANVERSION:	/* Get driver version */
			ret = wlan_version_ioctl(priv, req);
			break;
		}
		break;
#ifdef WPA
	case WLANSETWPAIE:
		ret = wlan_setwpaie_ioctl(priv, req);
		break;
#endif

	case WLAN_SETONEINT_GETONEINT:
		switch(wrq->u.data.flags) {
		case WLANNF :		   
			ret  = wlan_get_nf(priv, wrq);
			break;
				
		case WLANRSSI :		
			ret = wlan_get_rssi(priv, wrq);
			break;
#ifdef WMM
		case WLANWMM_ENABLE:
			ret = wlan_wmm_enable_ioctl(priv, wrq);
			break;
#endif /* WMM */
#ifdef THROUGHPUT_TEST
		case WLANTHRUPUT:
			ret = wlan_thruput(priv, wrq);
			break;
#endif /* THROUGHTPUT_TEST */

#ifdef BG_SCAN
		case WLANBGSCAN:
			if (wrq->u.data.pointer) {
				int data;
				switch (*wrq->u.data.pointer) {
					case CMD_DISABLED:
						PRINTK("Background scan is set to disable\n");
						ret = wlan_bg_scan_enable(priv, FALSE);
						break;
					case CMD_ENABLED:
						PRINTK("Background scan is set to enable\n");
						ret = wlan_bg_scan_enable(priv, TRUE);
						break;
					case CMD_GET:
						data = (Adapter->bgScanConfig->Enable == TRUE) ? 
							CMD_ENABLED : CMD_DISABLED;
						wrq->u.data.length = sizeof(data);
						copy_to_user(wrq->u.data.pointer, &data, wrq->u.data.length);
						break;
					default:			
						ret = -EINVAL;
						PRINTK("Background scan: wrong parameter\n");
						break;
				}
			}
			else {
				ret = -EINVAL;
				PRINTK("Background scan: wrong parameter\n");
			}
			break;
#endif /* BG_SCAN */

#ifdef WPRM_DRV
		case WLANMOTOTM:
			if (wrq->u.data.pointer) {
				int data;
				switch (*wrq->u.data.pointer) {
					case CMD_DISABLED:
						printk("Disable automatic traffic meter feature.\n");
						ret = wlan_wprm_tm_enable(FALSE);
						break;
					case CMD_ENABLED:
						printk("Enable automatic traffic meter feature.\n");
						ret = wlan_wprm_tm_enable(TRUE);
						break;
					case CMD_GET:
						data = wlan_wprm_tm_is_enabled();
						wrq->u.data.length = sizeof(data);
						copy_to_user(wrq->u.data.pointer, &data, wrq->u.data.length);
						break;
					default:
						ret = -EINVAL;
						PRINTK("mototm: wrong parameter.\n");
						break;
				}
			}
			else {
				ret = -EINVAL;
				PRINTK("mototm: wrong parameter.\n");
			}
			break;
#endif /* WPRM_DRV */

#ifdef BULVERDE_SDIO
		case WLANSDIOCLOCK:
			if (wrq->u.data.pointer) {
				switch (*wrq->u.data.pointer) {
					case CMD_DISABLED:
						PRINTK("Bulverde SDIO clock is turned off\n");
						ret = wlan_sdio_clock(priv, FALSE);
						break;
					case CMD_ENABLED:
						PRINTK("Bulverde SDIO clock is turned on\n");
						ret = wlan_sdio_clock(priv, TRUE);
						break;
					case CMD_GET: /* need an API in sdio.c to get STRPCL */
					default:			
						ret = -EINVAL;
						PRINTK("sdioclock: wrong parameter\n");
						break;
				}
			}
			else {
				ret = -EINVAL;
				printk("sdioclock scan: wrong parameter\n");
			}
			break;
#endif /* BULVERDE_SDIO */
#ifdef ENABLE_802_11D
		case WLANENABLE11D:
			ret = wlan_cmd_enable_11d( priv, wrq );
			break;
#endif

#ifdef ADHOC_GRATE
		case WLANADHOCGRATE:
			ret = wlan_do_set_grate_ioctl(priv, wrq);
			break;
#endif			

#ifdef ENABLE_802_11H_TPC
		case WLANENABLE11HTPC:
			ret = wlan_cmd_enable_11h_tpc( priv, wrq );
			break;
		
		case WLANSETLOCALPOWER:
			ret = wlan_cmd_get_local_power_11h_tpc( priv, wrq );
			break;
#endif

#ifdef WMM_UAPSD
		case WLAN_WMM_QOSINFO:
			if (wrq->u.data.length == 1)
			{
				Adapter->wmm.qosinfo = (u8) wrq->u.data.pointer[0]; 
			}
			else
			{
				wrq->u.data.pointer[0] = (int) Adapter->wmm.qosinfo;
				wrq->u.data.length = 1;
			}
			break;
#endif
#ifdef PS_REQUIRED
		case WLAN_LISTENINTRVL:
			if(!wrq->u.data.length) {				
				PRINTK1("Get LocalListenInterval Value\n");
#define GET_ONE_INT	1				
				*(int *)wrq->u.data.pointer = 
						Adapter->LocalListenInterval;
				wrq->u.data.length = GET_ONE_INT;
			} else {
				PRINTK("Set LocalListenInterval = %d\n",
						*((int *)wrq->u.data.pointer));
#define MAX_U16_VAL	65535
				if(*(int *)wrq->u.data.pointer > MAX_U16_VAL) {
					PRINTK1("Exceeds U16 value\n");
					return -EINVAL;
				}
				Adapter->LocalListenInterval = 
						*(u16 *)wrq->u.data.pointer;
			}
			break;
#ifdef FW_WAKEUP_METHOD
		case WLAN_FW_WAKEUP_METHOD:
			ret = wlan_cmd_fw_wakeup_method( priv, wrq );
			break;		
#endif
#endif
		case WLAN_TXCONTROL:
			ret = wlan_txcontrol(priv, wrq ); //adds for txcontrol ioctl
			break;

#ifdef ATIMGEN			
		case  WLANATIMGEN:
			ret = wlan_atimgen(priv, wrq );
			/* enable/disable atim generation*/
			break;
#endif

#ifdef WMM_UAPSD
		case  WLANNULLGEN:
			ret = wlan_null_pkg_gen(priv, wrq );
			/* enable/disable null pkg generation*/
			break;
#endif

		case WLAN_NULLPKTINTERVAL:
			ret = wlan_null_pkt_interval(priv, wrq);
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
		}
		break;	
	
	case WLAN_SETONEINT_GETNONE:
		/* The first 4 bytes of req->ifr_data is sub-ioctl number
		 * after 4 bytes sits the payload.
		 */
		subcmd = wrq->u.data.flags;	//from wpa_supplicant subcmd
		
		if(!subcmd)
			subcmd = (int)req->ifr_data;	//from iwpriv subcmd
		
		switch (subcmd) {
		case WLAN_SUBCMD_SETRXANTENNA : /* SETRXANTENNA */
			ret = wlan_subcmd_setrxantenna_ioctl(priv, req);
			break;
				
		case WLAN_SUBCMD_SETTXANTENNA : /* SETTXANTENNA */
			ret = wlan_subcmd_settxantenna_ioctl(priv, req);
			break;
				
#ifdef PS_REQUIRED
		case WLANSETPRETBTT:
			ret = wlan_setpretbtt_ioctl(priv, req);
			break;
#endif

		case WLAN_SET_ATIM_WINDOW:
			ret = wlan_set_atim_window_ioctl(priv, req);
			break;
		case WLANSETBCNAVG:
			priv->adapter->bcn_avg_factor = 
				(int) ((struct ifreq *)((u8 *)req + 4))->ifr_data;
			if(priv->adapter->bcn_avg_factor == 0)
				priv->adapter->bcn_avg_factor = DEFAULT_BCN_AVG_FACTOR;
#ifdef SLIDING_WIN_AVG			
			if(priv->adapter->bcn_avg_factor > DEFAULT_BCN_AVG_FACTOR)
				priv->adapter->bcn_avg_factor = DEFAULT_BCN_AVG_FACTOR;
#endif			
			ret = 0;
			break;
		case WLANSETDATAAVG:
			priv->adapter->data_avg_factor = 
				(int) ((struct ifreq *)((u8 *)req + 4))->ifr_data;
			if(priv->adapter->data_avg_factor == 0)
				priv->adapter->data_avg_factor = DEFAULT_DATA_AVG_FACTOR;
#ifdef SLIDING_WIN_AVG			
			if(priv->adapter->data_avg_factor > DEFAULT_DATA_AVG_FACTOR)
				priv->adapter->data_avg_factor = DEFAULT_DATA_AVG_FACTOR;
#endif			
			ret = 0;
			break;
		case WLANSETREGION:
			ret = wlan_setregion_ioctl(priv, req);
			break;
				
		case WLAN_SET_LISTEN_INTERVAL:
			ret = wlan_set_listen_interval_ioctl(priv, req);
			break;
				
#ifdef PS_REQUIRED
		case WLAN_SET_MULTIPLE_DTIM:
			ret = wlan_set_multiple_dtim_ioctl(priv, req);
			break;
#endif

		case WLANSETAUTHALG:
			ret = wlan_setauthalg_ioctl(priv, req);
			break;
		
		case WLANSET8021XAUTHALG:
			ret = wlan_set8021xauthalg_ioctl(priv, req);
			break;

		case WLANSETENCRYPTIONMODE:
			ret = wlan_setencryptionmode_ioctl(priv, req);
			break;
		default:
			ret = -EOPNOTSUPP;
			break;
		}
		
		break;

	case WLAN_SETNONE_GETTWELVE_CHAR: /* Get Antenna settings */
		/* 
		 * We've not used IW_PRIV_TYPE_FIXED so sub-ioctl number is
		 * in flags of iwreq structure, otherwise it will be in
		 * mode member of iwreq structure.
		 */
		switch ((int)wrq->u.data.flags) {
		case WLAN_SUBCMD_GETRXANTENNA: /* Get Rx Antenna */
			ret = wlan_subcmd_getrxantenna_ioctl(priv, req);
			break;

		case WLAN_SUBCMD_GETTXANTENNA:  /* Get Tx Antenna */
			ret = wlan_subcmd_gettxantenna_ioctl(priv, req);
			break;
		}
		break;
		
	case WLAN_SETNONE_GETBYTE:
		switch(wrq->u.data.flags) {
		case WLANREASSOCIATIONAUTO:
			reassociation_on(priv);
			break;
		case WLANREASSOCIATIONUSER:
			reassociation_off(priv);
			break;
		case WLANWLANIDLEON:
			wlanidle_on(priv);
			break;
		case WLANWLANIDLEOFF:
			wlanidle_off(priv);
			break;
		}	
		
		break;		
		
#ifdef DEEP_SLEEP
	case WLANDEEPSLEEP:
		ret = wlan_deepsleep_ioctl(priv, req);
		break;
#endif

#ifdef HOST_WAKEUP
	case WLANHOSTWAKEUPCFG:
		ret = wlan_do_hostwakeupcfg_ioctl(priv, wrq); 
		break;
#endif
			
	case WLAN_RFI :
		ret = wlan_do_rfi_ioctl(priv, wrq);
		break;	

	case WLAN_SET64CHAR_GET64CHAR:
		switch ((int)wrq->u.data.flags) {
#ifdef BCA
		case WLANBCA:
			ret = wlan_bca_ioctl(priv, wrq);
			break;
#endif
#ifdef ENABLE_802_11H_TPC
		case WLANREQUESTTPC:
			ret = wlan_do_request_tpc_ioctl(priv, wrq);
			break;
		case WLANSETPOWERCAP:
			ret = wlan_do_power_cap_ioctl(priv, wrq);
			break;
#endif

		case WLANSLEEPPARAMS:
			ret = wlan_sleep_params_ioctl(priv, wrq);
			break;

#ifdef BCA
		case WLAN_BCA_TIMESHARE:
			ret = wlan_bca_timeshare_ioctl(priv, wrq);
			break;
#endif
		case WLANSCAN_MODE :
			PRINTK1("Scan Mode Ioctl\n");
			ret = wlan_scan_mode_ioctl(priv, wrq);
			break;
		
		case WLANSCAN_BSSID:
			ret = wlan_scan_bssid_ioctl(priv, wrq);
			break;
		case WLAN_GET_RATE:
			ret = wlan_getrate_ioctl(priv, wrq);
			break;
		case WLAN_GET_ADHOC_STATUS:
			ret = wlan_get_adhoc_status_ioctl(priv, wrq);
			break;
#if defined(IWGENIE_SUPPORT) && (WIRELESS_EXT < 18)                    
        case WLAN_SET_GEN_IE:
            ret = wlan_set_gen_ie_ioctl(priv, wrq);
            break;
        case WLAN_GET_GEN_IE:
            ret = wlan_get_gen_ie_ioctl(priv, wrq);
            break;
#endif
#ifdef REASSOCIATION_SUPPORT
        case WLAN_REASSOCIATE:
            ret = wlan_reassociate_ioctl(dev, wrq);
            break;
#endif
        }
        break;
	case WLAN_SETCONF_GETCONF:
		PRINTK1("The WLAN_SETCONF_GETCONF=0x%x is %d\n",WLAN_SETCONF_GETCONF, *(req -> ifr_data));
		switch ((int)*(req -> ifr_data)) {
#ifdef CAL_DATA
		case CAL_DATA_CONFIG:
		ret = wlan_do_caldata_ioctl(priv, req);
		break;
#endif /* CAL_DATA */
#ifdef CAL_DATA
		case CAL_DATA_EXT_CONFIG:
		ret = wlan_do_caldata_ext_ioctl(priv, req);
		break;
#endif /* CAL_DATA */
#ifdef BG_SCAN
		case BG_SCAN_CONFIG:
			ret = wlan_do_bg_scan_config_ioctl(priv, req);
			break;
#endif /* BG_SCAN */

#ifdef SUBSCRIBE_EVENT_CTRL
		case SUBSCRIBE_EVENT:
			ret = wlan_do_subscribe_event(priv, req);
			break;
#endif

#ifdef WMM
		case WMM_TSPEC:
			ret = wlan_do_wmm_tspec_ioctl(priv, req);
			break;
		case WMM_ACK_POLICY:
			ret = wlan_do_wmm_ack_policy_ioctl(priv, req);
			break;	
		case WMM_PARA_IE:
			ret = wlan_do_wmm_para_ie_ioctl(priv, req);
			break;
#endif /* WMM */
		}
		break;

	case WLAN_SETNONE_GETONEINT:
		switch((int)req->ifr_data) {
		case WLANGETBCNAVG:
			req->ifr_data = (char *)((u32)Adapter->bcn_avg_factor);
			break;

		case WLANGETREGION:
			req->ifr_data = (char *)((u32)Adapter->RegionCode);
			break;

		case WLAN_GET_LISTEN_INTERVAL:
			req->ifr_data = (char *)((u32)Adapter->ListenInterval);
			break;

#ifdef PS_REQUIRED
		case WLAN_GET_MULTIPLE_DTIM:
			req->ifr_data = (char *)((u32)Adapter->MultipleDtim);
			break;
#endif
		default:
			ret = -EOPNOTSUPP;

		}

		break;
		
	case WLAN_SETTENCHAR_GETNONE:
		switch ((int)wrq->u.data.flags) {
#ifdef MULTI_BANDS
		case WLAN_SET_BAND:
			ret = wlan_do_setband_ioctl(priv, wrq);
			break;

		case WLAN_SET_ADHOC_CH:
			ret = wlan_do_setadhocch_ioctl(priv, wrq);
			break;
#endif
		}
		break;

	case WLAN_SETNONE_GETTENCHAR:
		switch ((int)wrq->u.data.flags) {
#ifdef MULTI_BANDS
		case WLAN_GET_BAND:
			ret = wlan_do_get_band_ioctl(priv, wrq);
			break;

		case WLAN_GET_ADHOC_CH:
			ret = wlan_do_get_adhocch_ioctl(priv, wrq);
			break;
#endif
		}
		break;

	case WLANGETLOG:
		ret = wlan_do_getlog_ioctl(priv, wrq);
		break;

//#if defined (CF8385) || defined (CF8381) || defined (CF8305)
#if defined (CF83xx)
	case WLANREGCFRDWR:
		ret = wlan_do_regcfrdwr_ioctl(priv, req);
		break;
#endif

  case WLAN_SET_GET_SIXTEEN_INT:
    switch ((int)wrq->u.data.flags) {
      case WLAN_TPCCFG:
      {
        int *data = (int *)wrq->u.data.pointer;
        HostCmd_DS_802_11_TPC_CFG cfg;
        memset(&cfg, 0, sizeof(cfg));

        if ((wrq->u.data.length > 1) && (wrq->u.data.length != 5))
          return -1;

        if (wrq->u.data.length == 0)
        {
          cfg.Action  = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
        }
        else
        {
          cfg.Action  = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
          cfg.Enable  = data[0];
          cfg.UseSNR  = data[1];
          cfg.P0      = data[2];
          cfg.P1      = data[3];
          cfg.P2      = data[4];
        }

        ret = 
          PrepareAndSendCommand(
          priv,
          HostCmd_CMD_802_11_TPC_CFG,
          0, 
          HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
          0,
          HostCmd_PENDING_ON_NONE, 
          (void *) &cfg
          );

        data[0] = cfg.Enable;
        data[1] = cfg.UseSNR;
        data[2] = cfg.P0;
        data[3] = cfg.P1;
        data[4] = cfg.P2;
        wrq->u.data.length = 5;
      }

      break;
  
      case WLAN_POWERCFG:
      {
        int *data = (int *)wrq->u.data.pointer;
        HostCmd_DS_802_11_PWR_CFG cfg;
        memset(&cfg, 0, sizeof(cfg));

        if ((wrq->u.data.length > 1) && (wrq->u.data.length != 4))
          return -1;

        if (wrq->u.data.length == 0)
        {
          cfg.Action  = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
        }
        else
        {
          cfg.Action  = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
          cfg.Enable  = data[0];
          cfg.PA_P0   = data[1];
          cfg.PA_P1   = data[2];
          cfg.PA_P2   = data[3];
        }

        ret = 
          PrepareAndSendCommand(
          priv,
          HostCmd_CMD_802_11_PWR_CFG,
          0, 
          HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
          0,
          HostCmd_PENDING_ON_NONE, 
          (void *) &cfg
          );

        data[0] = cfg.Enable;
        data[1] = cfg.PA_P0;
        data[2] = cfg.PA_P1;
        data[3] = cfg.PA_P2; 
        wrq->u.data.length = 4;
      }

      break;

#ifdef WPRM_DRV
      case WLAN_SESSIONTYPE:
      {
        int *data = (int *)wrq->u.data.pointer;
        WPRM_SESSION_SPEC_S session;
        memset(&session, 0, sizeof(session));

        if ((wrq->u.data.length != 3))
          return -1;

        session.pid  = current->pid;
        session.sid  = data[0];
        session.type = data[1];
        session.act  = data[2];

        ret = wprm_session_type_action(&session);
      }
      break;
#endif

#ifdef AUTO_FREQ_CTRL
  	case WLAN_AUTO_FREQ_SET: 
      {
        int *data = (int *)wrq->u.data.pointer;
        HostCmd_DS_802_11_AFC afc;
        memset(&afc, 0, sizeof(afc));

        afc.afc_auto    = data[0];

        if (afc.afc_auto != 0) {
          afc.afc_thre    = data[1];
          afc.afc_period  = data[2];
        } else {
          afc.afc_toff    = data[1];
          afc.afc_foff    = data[2];
        }

        ret = 
          PrepareAndSendCommand(
          priv,
          HostCmd_CMD_802_11_SET_AFC,
          0, 
          HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
          0,
          HostCmd_PENDING_ON_NONE, 
          (void *) &afc
          );
      }
      break;

	case WLAN_AUTO_FREQ_GET: 
      {
        int *data = (int *)wrq->u.data.pointer;
        HostCmd_DS_802_11_AFC afc;
        memset(&afc, 0, sizeof(afc));

        ret = 
          PrepareAndSendCommand(
          priv,
          HostCmd_CMD_802_11_GET_AFC,
          0, 
          HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
          0,
          HostCmd_PENDING_ON_NONE, 
          (void *) &afc
          );

        data[0] = afc.afc_auto;
        data[1] = afc.afc_toff;
        data[2] = afc.afc_foff;
        wrq->u.data.length = 3;
      }
      break;
#endif
	case WLAN_SCANPROBES: 
      {
        int *data = (int *)wrq->u.data.pointer;

        if (wrq->u.data.length > 0)
          Adapter->ScanProbes = data[0];
        else
          data[0] = Adapter->ScanProbes;

        wrq->u.data.length = 1;
      }
      break;
#ifdef LED_GPIO_CTRL
    case WLAN_LED_GPIO_CTRL:
    {
      int i;
      int *data = (int *)wrq->u.data.pointer;
      HostCmd_DS_802_11_LED_CTRL ctrl;

      MrvlIEtypes_LedGpio_t *gpio 
        = (MrvlIEtypes_LedGpio_t *) ctrl.data;

      memset(&ctrl, 0, sizeof(ctrl));
  
      if (wrq->u.data.length > MAX_LEDS*2)
        return -ENOTSUPP;

      if ((wrq->u.data.length % 2) != 0)
        return -ENOTSUPP;

      if (wrq->u.data.length == 0) {
        ctrl.Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
      } else {
        ctrl.Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
        ctrl.NumLed = wlan_cpu_to_le16(0);

        gpio->Header.Type = wlan_cpu_to_le16(TLV_TYPE_LED_GPIO);
        gpio->Header.Len  = wrq->u.data.length;

        for (i=0; i < wrq->u.data.length; i+=2) {
          gpio->LedPin[i/2].Led = data[i];
          gpio->LedPin[i/2].Pin = data[i+1];
        }
      }

      ret = PrepareAndSendCommand( priv, HostCmd_CMD_802_11_LED_GPIO_CTRL,
          0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
          0, HostCmd_PENDING_ON_NONE, (void *) &ctrl);
  
      for (i=0; i < gpio->Header.Len; i+=2) {
        data[i] = gpio->LedPin[i/2].Led;
        data[i+1] = gpio->LedPin[i/2].Pin;
      }

      wrq->u.data.length = gpio->Header.Len;
    }
    break;
#endif

	case WLAN_SLEEP_PERIOD:
		ret = wlan_sleep_period(priv, wrq);
	break;
	case WLAN_ADAPT_RATESET:
		ret = wlan_adapt_rateset(priv, wrq);
	break;

	case WLAN_INACTIVITY_TIMEOUT:
		ret = wlan_inactivity_timeout(priv, wrq); 
		break;

	case WLANSNR :
		ret = wlan_get_snr(priv, wrq);
		break;
  }
  break;
					
	default:
 		ret = -EINVAL;
		break;
	}

	LEAVE();
	return ret;
}

/*
 * Wireless statistics 
 */
struct iw_statistics *wlan_get_wireless_stats(struct net_device *dev)
{
	wlan_private	*priv = dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	int		ret = 0;

	ENTER();

	priv->wstats.status = Adapter->InfrastructureMode;
	priv->wstats.discard.retries = priv->stats.tx_errors;

	/* send RSSI command to get beacon RSSI/NF, valid only if associated */
	ret = PrepareAndSendCommand(priv, 
			HostCmd_CMD_802_11_RSSI,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	priv->wstats.qual.level =
		CAL_RSSI(Adapter->SNR[TYPE_BEACON][TYPE_NOAVG],
		Adapter->NF[TYPE_BEACON][TYPE_NOAVG]);
	priv->wstats.qual.noise = CAL_NF(Adapter->NF[TYPE_BEACON][TYPE_NOAVG]);
	if (Adapter->NF[TYPE_BEACON][TYPE_NOAVG] == 0
		&& Adapter->MediaConnectStatus == WlanMediaStateConnected)
		priv->wstats.qual.noise = MRVDRV_NF_DEFAULT_SCAN_VALUE;
	else
		priv->wstats.qual.noise = 
				CAL_NF(Adapter->NF[TYPE_BEACON][TYPE_NOAVG]);
	priv->wstats.qual.qual = 0;

	PRINTK1("Signal Level = %#x\n", priv->wstats.qual.level);
	PRINTK1("Noise = %#x\n", priv->wstats.qual.noise);

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_GET_LOG,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if (!ret) {
		priv->wstats.discard.code = Adapter->LogMsg.wepundecryptable;
		priv->wstats.discard.fragment = Adapter->LogMsg.fcserror;
		priv->wstats.discard.retries = Adapter->LogMsg.retry;
		priv->wstats.discard.misc = Adapter->LogMsg.ackfailure;

		/* TODO: Update wstats.miss.beacon once firware gives it. */
	}

	return &priv->wstats;
}

#ifdef DEEP_SLEEP
int wlan_deep_sleep_ioctl(wlan_private *priv, struct ifreq *rq)
{
#ifdef PS_REQUIRED
	wlan_adapter	*Adapter = priv->adapter;
#endif
	int		option = (int)rq->ifr_data;
        int		ret = 0;

	ENTER();

	switch(option) {
		case DEEP_SLEEP_ENABLE: /* Enter deep sleep mode */

			ret = sbi_enter_deep_sleep(priv);
			if (!ret) {
#ifdef PS_REQUIRED
#ifdef BULVERDE_SDIO
				ps_sleep_confirmed = 1;
#endif
#endif
				PRINTK("Enter Deep Sleep Successfully\n");
#ifdef PS_REQUIRED
				Adapter->PSState = PS_STATE_SLEEP;
#endif
				Adapter->IsDeepSleep = TRUE;

			/* Flush all the packets upto the OS before stopping */
				wlan_send_rxskbQ(priv);
				netif_carrier_off(priv->wlan_dev.netdev);
			}
			else
				PRINTK("Deep Sleep Enter failed\n");
                break;

		case DEEP_SLEEP_DISABLE: /* Exit deep sleep mode */

#ifdef BULVERDE_SDIO
			ps_sleep_confirmed = 0;
#endif
			ret = sbi_exit_deep_sleep(priv);
			if (!ret) {
				PRINTK("Exit Deep Sleep Successfully\n");
#ifdef PS_REQUIRED
				Adapter->PSState = PS_STATE_FULL_POWER;
#endif
				Adapter->IsDeepSleep = FALSE;
				netif_carrier_on(priv->wlan_dev.netdev);
			}
			else
				PRINTK("Deep Sleep Exit failed\n");
			break;
		default:
			PRINTK("unknown option = 0x%x\n",option);
			ret = -EINVAL;
			break;
	}
	
	LEAVE();
        return ret;
}
#endif /* end of DEEP_SLEEP */

#ifdef BCA
int wlan_bca_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int 				ret = 0;
	wlan_adapter 			*Adapter = priv->adapter;
	wlan_ioctl_bca_params_config 	bca;

	ENTER();

	memset(&bca, 0, sizeof(HostCmd_DS_BCA_CONFIG));

	if (copy_from_user(&bca, wrq->u.data.pointer,
				MIN(sizeof(bca), wrq->u.data.length))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}
	
	memcpy(&Adapter->bca, &bca, sizeof(HostCmd_DS_BCA_CONFIG));	
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_BCA_CONFIG,
			bca.Action, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if (!ret && !bca.Action && wrq->u.data.pointer) {
		copy_to_user(wrq->u.data.pointer, &Adapter->bca, 
				sizeof(HostCmd_DS_BCA_CONFIG));
		wrq->u.data.length = sizeof(HostCmd_DS_BCA_CONFIG);
	}

	LEAVE();
	return ret;
}

int wlan_bca_timeshare_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int 				ret;
	wlan_adapter 			*Adapter = priv->adapter;
	wlan_ioctl_bca_timeshare_config bca_ts;

	ENTER();

	memset(&bca_ts, 0, sizeof(HostCmd_DS_802_11_BCA_TIMESHARE));
	
	if (copy_from_user(&bca_ts, wrq->u.data.pointer,
				MIN(sizeof(bca_ts), wrq->u.data.length))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}

	PRINTK("TrafficType=%x TimeShareInterva=%x BTTime=%x\n", 
			bca_ts.TrafficType, bca_ts.TimeShareInterval, 
			bca_ts.BTTime);

	ret = PrepareAndSendCommand(priv, 
			HostCmd_CMD_802_11_BCA_CONFIG_TIMESHARE,
			bca_ts.Action, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, &bca_ts);

	if (!ret && !bca_ts.Action && wrq->u.data.pointer) {
		copy_to_user(wrq->u.data.pointer, &Adapter->bca_ts, 
								sizeof(bca_ts));
		wrq->u.data.length = sizeof(HostCmd_DS_802_11_BCA_TIMESHARE);
	}

	LEAVE();
	return ret;
}
#endif /* BCA */

static inline int wlan_scan_type_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	u8	buf[12];
	u8	*option[] = { "active", "passive", "get", };
	int	i, max_options = (sizeof(option)/sizeof(option[0]));
	int	ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

#ifdef ENABLE_802_11D
	if( wlan_get_state_11d( priv) == ENABLE_11D ) {
		PRINTK1("11D: Cannot set scantype when 11D enabled\n");
		return -EFAULT;
	}
#endif

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, wrq->u.data.pointer, MIN(sizeof(buf), 
					wrq->u.data.length))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}


	PRINTK1("Scan Type Option = %s\n", buf);

	buf[sizeof(buf)-1] = '\0';

	for (i = 0; i < max_options; i++) {
		if (!strcmp(buf, option[i]))\
			break;
	}

	switch (i) {
		case 0:
			Adapter->ScanType = HostCmd_SCAN_TYPE_ACTIVE;
			break;
		case 1:
			Adapter->ScanType = HostCmd_SCAN_TYPE_PASSIVE;
			break;
		case 2:
			wrq->u.data.length = 
				strlen(option[Adapter->ScanType]) + 1;

			if (copy_to_user(wrq->u.data.pointer, 
						option[Adapter->ScanType],
						wrq->u.data.length)) {
				PRINTK1("Copy to user failed\n");
				ret = -EFAULT;
			}

			break;
		default:
			PRINTK1("Invalid Scan Type Ioctl Option\n");
			ret = -EINVAL;
			break;
	}

	LEAVE();
	return ret;
}

int wlan_scan_mode_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter *Adapter = priv->adapter;
	u8	buf[12];
	u8	*option[] = { "bss", "ibss", "any","get"};
	int	i, max_options = (sizeof(option)/sizeof(option[0]));
	int	ret = 0;

	ENTER();

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, wrq->u.data.pointer, MIN(sizeof(buf), 
					wrq->u.data.length))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}

	PRINTK1("Scan Mode Option = %s\n", buf);

	buf[sizeof(buf)-1] = '\0';

	for (i = 0; i < max_options; i++) {
		if (!strcmp(buf, option[i]))
			break;
	}

	switch (i) {

		case 0:
			Adapter->ScanMode = HostCmd_BSS_TYPE_BSS;
			break;
		case 1:
			Adapter->ScanMode = HostCmd_BSS_TYPE_IBSS;
			break;
		case 2:
			Adapter->ScanMode = HostCmd_BSS_TYPE_ANY;
			break;
		case 3:

			wrq->u.data.length = 
				strlen(option[Adapter->ScanMode-1]) + 1;

			PRINTK1("Get Scan Mode Option = %s\n", 
					option[Adapter->ScanMode-1]);

			PRINTK1("Scan Mode Length %d\n", wrq->u.data.length);

			if (copy_to_user(wrq->u.data.pointer, 
						option[Adapter->ScanMode-1],
							wrq->u.data.length)) {
				PRINTK1("Copy to user failed\n");
				ret = -EFAULT;
			}
			PRINTK1("GET Scan Type Option after copy = %s\n", 
					wrq->u.data.pointer);

			break;

		default:
			PRINTK1("Invalid Scan Mode Ioctl Option\n");
			ret = -EINVAL;
			break;
	}

	LEAVE();
	return ret;
}

#ifdef ADHOC_GRATE
int wlan_do_set_grate_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter	*Adapter = priv->adapter;
	int 		data;
	
	ENTER();

	switch ( *((int *) wrq->u.data.pointer) ) {
		case 0:
			Adapter->adhoc_grate_enabled = FALSE;
			break;
		case 1:
			Adapter->adhoc_grate_enabled = TRUE;
			break;
		case 2:
			data = Adapter->adhoc_grate_enabled;
			copy_to_user(wrq->u.data.pointer, &data, sizeof(int) );
			wrq->u.data.length = 1;
			break;
		default:
			return -EINVAL;
	}

	LEAVE();
	return 0;
}
#endif /* ADHOC_GRATE */

#ifdef PS_REQUIRED
#ifdef FW_WAKEUP_METHOD
int wlan_cmd_fw_wakeup_method( wlan_private *priv, struct iwreq *wrq )
{
	wlan_adapter	*Adapter = priv->adapter;
	u16		action;
	u16 		method;
	int		ret;
	
	ENTER();

	if (wrq->u.data.length == 0 || !wrq->u.data.pointer) {
		action = HostCmd_ACT_GET;
	}
	else {
		action = HostCmd_ACT_SET;
		switch ( *((int *) wrq->u.data.pointer) ) {
		case 0:
			Adapter->fwWakeupMethod = WAKEUP_FW_UNCHANGED;
			break;
		case 1:
			Adapter->fwWakeupMethod = WAKEUP_FW_THRU_INTERFACE;
			break;
		case 2:
			Adapter->fwWakeupMethod = WAKEUP_FW_THRU_GPIO;
			break;
		default:
			return -EINVAL;
		}
	}

	ret = PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_FW_WAKEUP_METHOD, action,
		HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP, 0,
		HostCmd_PENDING_ON_NONE, &Adapter->fwWakeupMethod);

	if (action == HostCmd_ACT_GET) {
		method = Adapter->fwWakeupMethod;
		copy_to_user(wrq->u.data.pointer, &method, sizeof(method));
		wrq->u.data.length = 1;
	}

	LEAVE();
	return ret;
}
#endif
#endif
#endif //linux
/* adds wlan_txcontrol() for iwpriv txcontrol */
int wlan_txcontrol(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter	*Adapter = priv->adapter;
	
	ENTER();

	if((int) wrq->u.data.length == 0) {
	    if(copy_to_user(wrq->u.data.pointer, &Adapter->PktTxCtrl, sizeof(u32))) {
		printk("copy_to_user failed!\n");
		return -EFAULT;
	    }	    
	} else{
	     if((int) wrq->u.data.length > 1) {
	  	printk("ioctl too many args!\n");
	   	return -EFAULT;
	     }
	     (u32)Adapter->PktTxCtrl = *((u32*)wrq->u.data.pointer);
	}

	wrq->u.data.length = 1;
			
	LEAVE();
	return 0;
}

#ifdef ATIMGEN			
/*Function for enabling/disabling atim generation*/
int wlan_atimgen( wlan_private *priv, struct iwreq *wrq )
{
	int data = *((int *)wrq->u.data.pointer);
	wlan_adapter   *Adapter = priv->adapter;
	
	ENTER();

	if ( wrq->u.data.length == 0 ) {
		return -EINVAL;
	}

	PRINTK2("Enable ATIMGEN: %s\n", (data==1)?"Enable":"Disable");
	switch ( data ) {
		case CMD_ENABLED:
			Adapter->ATIMEnabled = TRUE;
			PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_GENERATE_ATIM,
				HostCmd_ACT_SET,
				HostCmd_OPTION_USE_INT | 
				HostCmd_OPTION_WAITFORRSP,
				0,
				HostCmd_PENDING_ON_NONE,
				NULL);
			break;
		case CMD_DISABLED:
			Adapter->ATIMEnabled = FALSE;
			PrepareAndSendCommand(priv,
				HostCmd_CMD_802_11_GENERATE_ATIM,
				HostCmd_ACT_SET,
				HostCmd_OPTION_USE_INT | 
				HostCmd_OPTION_WAITFORRSP,
				0,
				HostCmd_PENDING_ON_NONE,
				NULL);
			break;
		default:			
			PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_GENERATE_ATIM,
				HostCmd_ACT_GET,
				HostCmd_OPTION_USE_INT | 
				HostCmd_OPTION_WAITFORRSP,
				0,
				HostCmd_PENDING_ON_NONE,
				NULL);
		
			break;
	}

	data = (Adapter->ATIMEnabled==TRUE)?CMD_ENABLED:CMD_DISABLED;
	copy_to_user(wrq->u.data.pointer, &data, sizeof(int) );
	wrq->u.data.length = 1;

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}
#endif

#ifdef WMM_UAPSD
/*Function for enabling/disabling uapsd null package generation*/
int wlan_null_pkg_gen( wlan_private *priv, struct iwreq *wrq )
{
	int data = *((int *)wrq->u.data.pointer);
	wlan_adapter   *Adapter = priv->adapter;
	
	ENTER();

	if ( wrq->u.data.length == 0 ) {
		return -EINVAL;
	}

	PRINTK2("Enable UAPSD NULL PKG: %s\n", 
			(data==CMD_ENABLED)?"Enable":"Disable");
	
	switch ( data ) {
		case CMD_ENABLED:
			Adapter->wmm.gen_null_pkg = TRUE;
			break;
		case CMD_DISABLED:
			Adapter->wmm.gen_null_pkg = FALSE;
			break;
		default:			
			break;
	}

	data = (Adapter->wmm.gen_null_pkg ==TRUE)?CMD_ENABLED:CMD_DISABLED;
	copy_to_user(wrq->u.data.pointer, &data, sizeof(int) );
	wrq->u.data.length = 1;

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}
#endif

