/*
 * File : wlan_cmd.c 
 */

#include	"include.h"

#ifdef WMM
static u8 wmm_ie[WMM_IE_LENGTH] = { 0xdd, 0x07, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00 };
#endif /* WMM */

#ifdef PS_REQUIRED
static u16 Commands_Allowed_In_PS[] = {
	HostCmd_CMD_802_11_RSSI,
#ifdef HOST_WAKEUP
	//HostCmd_CMD_802_11_HOST_WAKE_UP_CFG,
	HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM,
#endif
};

static BOOLEAN Is_Command_Allowed_In_PS(u16 Command)
{
	int count = sizeof(Commands_Allowed_In_PS) 
				/ sizeof(Commands_Allowed_In_PS[0]);
	int i;

	for (i = 0; i < count; i++) {
		if (Command == Commands_Allowed_In_PS[i]) /*  command found */
			return TRUE;
	}

	return FALSE;
}
#endif

#ifdef WPA
void DisplayTkip_MicKey(HostCmd_DS_802_11_PWK_KEY * pCmdPwk)
{
	PRINTK1("TKIP Key : "
			"0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x - "
			"0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x - \n",
		pCmdPwk->TkipEncryptKey[0],
		pCmdPwk->TkipEncryptKey[1],
		pCmdPwk->TkipEncryptKey[2],
		pCmdPwk->TkipEncryptKey[3],
		pCmdPwk->TkipEncryptKey[4],
		pCmdPwk->TkipEncryptKey[5],
		pCmdPwk->TkipEncryptKey[6],
		pCmdPwk->TkipEncryptKey[7],
		pCmdPwk->TkipEncryptKey[8],
		pCmdPwk->TkipEncryptKey[9],
		pCmdPwk->TkipEncryptKey[10],
		pCmdPwk->TkipEncryptKey[11],
		pCmdPwk->TkipEncryptKey[12],
		pCmdPwk->TkipEncryptKey[13],
		pCmdPwk->TkipEncryptKey[14], pCmdPwk->TkipEncryptKey[15]);

	PRINTK1("TxMicKey : 0x%2x 0x%2x 0x%2x 0x%2x "
			"0x%2x 0x%2x 0x%2x 0x%2x - \n",
		pCmdPwk->TkipTxMicKey[0],
		pCmdPwk->TkipTxMicKey[1],
		pCmdPwk->TkipTxMicKey[2],
		pCmdPwk->TkipTxMicKey[3],
		pCmdPwk->TkipTxMicKey[4],
		pCmdPwk->TkipTxMicKey[5],
		pCmdPwk->TkipTxMicKey[6], pCmdPwk->TkipTxMicKey[7]);

	PRINTK1("RxMicKey : 0x%2x 0x%2x 0x%2x 0x%2x "
			"0x%2x 0x%2x 0x%2x 0x%2x - \n",
		pCmdPwk->TkipRxMicKey[0],
		pCmdPwk->TkipRxMicKey[1],
		pCmdPwk->TkipRxMicKey[2],
		pCmdPwk->TkipRxMicKey[3],
		pCmdPwk->TkipRxMicKey[4],
		pCmdPwk->TkipRxMicKey[5],
		pCmdPwk->TkipRxMicKey[6], pCmdPwk->TkipRxMicKey[7]);

}
#endif				/* end of WPA */


#ifdef IWGENIE_SUPPORT
/*
**  Called from the network join command prep. routine.  If a generic IE
**    buffer has been setup by the application/supplication, the routine
**    appends the buffer as a passthrough TLV type to the request.
** 
**  ppBuffer: pointer to command buffer pointer, modified by length appended
** 
**  return: bytes added to the buffer pointer *pBuffer
*/
static int wlan_cmd_append_generic_ie( wlan_private* priv, u8** ppBuffer )
{
    wlan_adapter *Adapter = priv->adapter;
    MrvlIEtypesHeader_t ieHeader;
    int retLen = 0;

    /* Null Checks */
    if (ppBuffer == 0) return 0;
    if (*ppBuffer == 0) return 0;

    /*
    ** If there is a generic ie buffer setup, append it to the return
    **   parameter buffer pointer. 
    */
    if (Adapter->genIeBufferLen)
    {
        PRINTK1("append generic %d to %p\n", Adapter->genIeBufferLen,
                *ppBuffer);

        /* Wrap the generic IE buffer with a passthrough TLV type */
        ieHeader.Type = TLV_TYPE_PASSTHROUGH;
        ieHeader.Len  = Adapter->genIeBufferLen;
        memcpy(*ppBuffer, &ieHeader, sizeof(ieHeader));

        /* Increment the return size and the return buffer pointer param */
        *ppBuffer += sizeof(ieHeader);
        retLen    += sizeof(ieHeader);

        /* Copy the generic IE buffer to the output buffer, advance pointer */
        memcpy(*ppBuffer, Adapter->genIeBuffer, Adapter->genIeBufferLen);

        /* Increment the return size and the return buffer pointer param */
        *ppBuffer += Adapter->genIeBufferLen;
        retLen    += Adapter->genIeBufferLen;

        /* Reset the generic IE buffer */
        Adapter->genIeBufferLen = 0;
    }

    /* return the length appended to the buffer */
    return retLen;
}
#endif


#ifdef REASSOCIATION_SUPPORT
/*
**  Called from the network join command prep. routine.  If a reassociation
**   attempt is in progress (determined from flag in the wlan_priv structure),
**   a REASSOCAP TLV is added to the association request.  This causes the
**   firmware to send a reassociation request instead of an association 
**   request.  The wlan_priv structure also contains the current AP BSSID to
**   be passed in the TLV and eventually in the managment frame to the new AP.
** 
**  ppBuffer: pointer to command buffer pointer, modified by length appended
** 
**  return: bytes added to the buffer pointer *pBuffer
*/
static int wlan_cmd_append_reassoc_tlv( wlan_private* priv, u8** ppBuffer )
{
    wlan_adapter *Adapter = priv->adapter;
    int retLen = 0;
    MrvlIEtypes_ReassocAp_t reassocIe;

    /* Null Checks */
    if (ppBuffer == 0) return 0;
    if (*ppBuffer == 0) return 0;

    /*
    ** If the reassocAttempt flag is set in the adapter structure, include
    **   the appropriate TLV in the association buffer pointed to by ppBuffer
    */
    if (Adapter->reassocAttempt)
    {
        PRINTK1("Reassoc: append current AP: %#x:%#x:%#x:%#x:%#x:%#x\n",
                Adapter->reassocCurrentAp[0], Adapter->reassocCurrentAp[1],
                Adapter->reassocCurrentAp[2], Adapter->reassocCurrentAp[3],
                Adapter->reassocCurrentAp[4], Adapter->reassocCurrentAp[5]);

        /* Setup a copy of the reassocIe on the stack */
        reassocIe.Header.Type = TLV_TYPE_REASSOCAP;
        reassocIe.Header.Len  = (sizeof(MrvlIEtypes_ReassocAp_t)
                                 - sizeof(MrvlIEtypesHeader_t));
        memcpy(&reassocIe.currentAp, 
               &Adapter->reassocCurrentAp,
               sizeof(reassocIe.currentAp));

        /* Copy the stack reassocIe to the buffer pointer parameter */
        memcpy(*ppBuffer, &reassocIe, sizeof(reassocIe));

        /* Set the return length */
        retLen = sizeof(reassocIe);

        /* Advance passed buffer pointer */
        *ppBuffer += sizeof(reassocIe);
        
        /* Reset the reassocAttempt flag, only valid for a single attempt */
        Adapter->reassocAttempt = FALSE;

        /* Reset the reassociation AP address */
        memset(&Adapter->reassocCurrentAp,
               0x00, 
               sizeof(Adapter->reassocCurrentAp));
    }

    /* return the length appended to the buffer */
    return retLen;
}
#endif

int SetMacPacketFilter(wlan_private * priv)
{
	int	ret = 0;

	ENTER();

	PRINTK("SetMacPacketFilter Value = %x\n",
           priv->adapter->CurrentPacketFilter);

	/* Send MAC control command to station */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_MAC_CONTROL, 0, 
			HostCmd_OPTION_USE_INT, 0, HostCmd_PENDING_ON_NONE, 
			NULL);

	LEAVE();
	return ret;
}

/*
 * Function: wlan_cmd_hw_spec (0x03) 
 */

static inline int wlan_cmd_hw_spec(wlan_private * priv,
					HostCmd_DS_COMMAND * cmd)
{
	HostCmd_DS_GET_HW_SPEC *hwspec = &cmd->params.hwspec;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_GET_HW_SPEC);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_GET_HW_SPEC) + S_DS_GEN);
	memcpy(hwspec->PermanentAddr, priv->adapter->CurrentAddr, ETH_ALEN);

	return 0;
}

#ifdef PS_REQUIRED
/*
 * Function: wlan_cmd_802_11_ps_mode (0x21) 
 */

static inline int wlan_cmd_802_11_ps_mode(wlan_private * priv,
					HostCmd_DS_COMMAND * cmd,
					u16 get_set, 
					int ps_mode)
{
	HostCmd_DS_802_11_PS_MODE	*psm = &cmd->params.psmode;
	u16				Action = get_set;
	wlan_adapter			*Adapter = priv->adapter;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_PS_MODE);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_PS_MODE) + S_DS_GEN);
	psm->Action = wlan_cpu_to_le16(get_set);	// CmdOption; //To verify !!
	switch (Action) {
	case HostCmd_SubCmd_TxPend_PS:
		PRINTK1("PS Command:" "SubCode-TxPending\n");
		// Set number of pend pkts

		break;

	case HostCmd_SubCmd_Enter_PS:
		PRINTK1("PS Command:" "SubCode- Enter PS\n");
		PRINTK1("LocalListenInterval = %d\n", 
				Adapter->LocalListenInterval);
	
		psm->LocalListenInterval = 
			wlan_cpu_to_le16(Adapter->LocalListenInterval);
		psm->NullPktInterval =
			wlan_cpu_to_le16(Adapter->NullPktInterval);

		break;

	case HostCmd_SubCmd_ChangeSet_PS:
		PRINTK1("PS Command: SubCode- Change PS Setting\n");
		// Set power level

		break;

	case HostCmd_SubCmd_Exit_PS:
		PRINTK1("PS Command:" "SubCode- Exit PS\n");
		// No parameters
		break;

	case HostCmd_SubCmd_Sleep_Confirmed:
		PRINTK1("PS Command: SubCode - sleep acknowledged\n");
		break;

	case HostCmd_SubCmd_No_Tx_Pkt:
		PRINTK1("PS Command : SubCode - no Tx Packet ");
		break;

	default:
		break;
	}

	if(psm->Action == HostCmd_SubCmd_Enter_PS)
		psm->MultipleDtim = wlan_cpu_to_le16(priv->adapter->MultipleDtim);
	else
		psm->MultipleDtim = 0;

	LEAVE();
	return 0;
}

#ifdef FW_WAKEUP_METHOD
static inline int wlan_cmd_802_11_fw_wakeup_method(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, int cmd_option, void *InfoBuf)
{
	HostCmd_DS_802_11_FW_WAKEUP_METHOD *fwwm = &cmd->params.fwwakeupmethod;
	u16 action = (u16)cmd_option;
	u16 method = *((u16 *)InfoBuf);

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_FW_WAKEUP_METHOD);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_FW_WAKEUP_METHOD) + S_DS_GEN);
	fwwm->Action = wlan_cpu_to_le16(action);
	switch (action) {
	case HostCmd_ACT_SET:
		fwwm->Method = wlan_cpu_to_le16(method);
		break;
	case HostCmd_ACT_GET:
	default:
		fwwm->Method = 0;
		break;
	}

	return 0;
}
#endif
#endif

#ifdef HOST_WAKEUP
/*
 * Function: wlan_cmd_802_11_host_wake_up_cfg (0x0043)
 */

static inline int wlan_cmd_802_11_host_wake_up_cfg(wlan_private * priv,
			HostCmd_DS_COMMAND * cmd,
			HostCmd_DS_802_11_HOST_WAKE_UP_CFG *hwuc)
{
	HostCmd_DS_802_11_HOST_WAKE_UP_CFG *phwuc = &cmd->params.hostwakeupcfg;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_HOST_WAKE_UP_CFG);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_HOST_WAKE_UP_CFG) + S_DS_GEN);
	phwuc->conditions = wlan_cpu_to_le32(hwuc->conditions);	// HOST_WAKE_UP_CFG_UNICAST_DATA | 
								// HOST_WAKE_UP_CFG_MAC_EVENT;
	phwuc->gpio = hwuc->gpio;		// not used
	phwuc->gap = hwuc->gap;			// in mseconds

	LEAVE();
	return 0;
}

/*
 * Function: wlan_cmd_802_11_host_awake_confirm (0x0044)
 */

static inline int wlan_cmd_802_11_host_awake_confirm(wlan_private * priv,
			HostCmd_DS_COMMAND * cmd)
{
	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM);
	cmd->Size = wlan_cpu_to_le16(S_DS_GEN);

	LEAVE();
	return 0;
}
#endif

static inline int wlan_cmd_802_11_inactivity_timeout(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, u16 cmd_option, void *InfoBuf)
{
	u16	*timeout = (u16 *) InfoBuf;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_INACTIVITY_TIMEOUT);
	cmd->Size    = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_INACTIVITY_TIMEOUT) + S_DS_GEN);

	cmd->params.inactivity_timeout.Action = wlan_cpu_to_le16(cmd_option);

	if (cmd_option)
		cmd->params.inactivity_timeout.Timeout = wlan_cpu_to_le16(*timeout);
	else
		cmd->params.inactivity_timeout.Timeout = 0;
		
	return 0;
}

#ifdef BG_SCAN
static inline int wlan_cmd_802_11_bg_scan_config(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, int cmd_option, void *InfoBuf)
{
	wlan_adapter *Adapter = priv->adapter;
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgcfg = &cmd->params.bgscancfg;
	BOOLEAN enable = *((BOOLEAN *)InfoBuf);

	cmd->Command = HostCmd_CMD_802_11_BG_SCAN_CONFIG;
	cmd->Size = (priv->adapter->bgScanConfigSize) + S_DS_GEN; 

	Adapter->bgScanConfig->Enable = enable;

	memcpy(bgcfg, Adapter->bgScanConfig, 
			Adapter->bgScanConfigSize);

	return 0;
}

static inline int wlan_cmd_802_11_bg_scan_query(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, int cmd_option, void *InfoBuf)
{
	HostCmd_DS_802_11_BG_SCAN_QUERY *bgquery = &cmd->params.bgscanquery;

	cmd->Command = HostCmd_CMD_802_11_BG_SCAN_QUERY;
	cmd->Size = sizeof(HostCmd_DS_802_11_BG_SCAN_QUERY) + S_DS_GEN; 

	bgquery->Flush = 1;

	return 0;
}
#endif /* BG_SCAN */


#ifdef SUBSCRIBE_EVENT_CTRL
static int wlan_cmd_802_11_subscribe_events(wlan_private *priv,
		HostCmd_DS_COMMAND *cmd, u16 cmd_option, void *InfoBuf)
{
	HostCmd_DS_802_11_SUBSCRIBE_EVENT	*events = &cmd->params.events;
	MrvlIEtypes_RssiParamSet_t		*Rssi;
	MrvlIEtypes_BeaconsMissed_t		*BcnMiss;
	MrvlIEtypes_SnrThreshold_t		*Snr;
	MrvlIEtypes_FailureCount_t		*FailCnt;
	EventSubscribe				*SubscribeEvent =
							(EventSubscribe *)InfoBuf;
	u16		ActualPos = sizeof(HostCmd_DS_802_11_SUBSCRIBE_EVENT);

	ENTER();

	HEXDUMP("InfoBuf :", (u8 *)InfoBuf, sizeof(EventSubscribe));
	
#define TLV_PAYLOAD_SIZE	2	

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SUBSCRIBE_EVENT);
	
	events->Action = wlan_cpu_to_le16(SubscribeEvent->Action);
	events->Events = wlan_cpu_to_le16(SubscribeEvent->Events);

	Rssi = (MrvlIEtypes_RssiParamSet_t *)((char *)events + ActualPos);
	Rssi->Header.Type = wlan_cpu_to_le16(TLV_TYPE_RSSI);
	Rssi->Header.Len = wlan_cpu_to_le16(TLV_PAYLOAD_SIZE);
	Rssi->RSSIValue = SubscribeEvent->RSSIValue;
	Rssi->RSSIFreq = SubscribeEvent->RSSIFreq;
	
	ActualPos += sizeof(MrvlIEtypesHeader_t) + Rssi->Header.Len;

	Snr = (MrvlIEtypes_SnrThreshold_t *)((char *)events + ActualPos);
	Snr->Header.Type = wlan_cpu_to_le16(TLV_TYPE_SNR);
	Snr->Header.Len = wlan_cpu_to_le16(TLV_PAYLOAD_SIZE);
	Snr->SNRValue = SubscribeEvent->SNRValue;
	Snr->SNRFreq = SubscribeEvent->SNRFreq;

	ActualPos += sizeof(MrvlIEtypesHeader_t) + Snr->Header.Len;

	FailCnt = (MrvlIEtypes_FailureCount_t *)((char *)events + ActualPos);
	FailCnt->Header.Type = TLV_TYPE_FAILCOUNT;
        FailCnt->Header.Len = TLV_PAYLOAD_SIZE;
	FailCnt->FailValue = SubscribeEvent->FailValue;
	FailCnt->FailFreq = SubscribeEvent->FailFreq;
	
	ActualPos += sizeof(MrvlIEtypesHeader_t) + FailCnt->Header.Len;

	BcnMiss = (MrvlIEtypes_BeaconsMissed_t *)((char *)events + ActualPos);
	BcnMiss->Header.Type = wlan_cpu_to_le16(TLV_TYPE_BCNMISS);
	BcnMiss->Header.Len = wlan_cpu_to_le16(TLV_PAYLOAD_SIZE);
	BcnMiss->BeaconMissed = SubscribeEvent->BcnMissed;
	BcnMiss->Reserved = 0;

	ActualPos += sizeof(MrvlIEtypesHeader_t) + BcnMiss->Header.Len;

	cmd->Size = wlan_cpu_to_le16(ActualPos + S_DS_GEN);

	PRINTK("ActualPos = %d\n", ActualPos);
	
	LEAVE();
	return 0;	
}
#endif

static inline int wlan_cmd_802_11_sleep_period(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, u16 cmd_option, void *InfoBuf)
{
	cmd->Command =	wlan_cpu_to_le16(HostCmd_CMD_802_11_SLEEP_PERIOD);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_SLEEP_PERIOD) + 
								S_DS_GEN);
	memmove(&cmd->params.ps_sleeppd, InfoBuf,
			sizeof(HostCmd_DS_802_11_SLEEP_PERIOD));

	return 0;
}

static inline int wlan_cmd_802_11_sleep_params(wlan_private *priv,
					HostCmd_DS_COMMAND *cmd, u16 CmdOption)
{
	wlan_adapter *Adapter = priv->adapter;
	HostCmd_DS_802_11_SLEEP_PARAMS *sp = &cmd->params.sleep_params;

	ENTER();

	cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_802_11_SLEEP_PARAMS)) +
								S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SLEEP_PARAMS);

	if (CmdOption == HostCmd_ACT_GEN_GET) {
		memset(&Adapter->sp, 0, sizeof(SleepParams));
		memset(sp, 0, sizeof(HostCmd_DS_802_11_SLEEP_PARAMS));
		sp->Action = wlan_cpu_to_le16(CmdOption);
	} else if (CmdOption == HostCmd_ACT_GEN_SET) {
		sp->Action = wlan_cpu_to_le16(CmdOption);
		sp->Error = wlan_cpu_to_le16(Adapter->sp.sp_error);
		sp->Offset = wlan_cpu_to_le16(Adapter->sp.sp_offset);
		sp->StableTime = wlan_cpu_to_le16(Adapter->sp.sp_stabletime);
		sp->CalControl = (u8) Adapter->sp.sp_calcontrol;
		sp->ExternalSleepClk = (u8) Adapter->sp.sp_extsleepclk;
		sp->Reserved = wlan_cpu_to_le16(Adapter->sp.sp_reserved);
	}

	LEAVE();
	return 0;
}

/*
 * Function: Scan Command 
 */

#ifdef TLV_SCAN
static inline int wlan_cmd_802_11_scan(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd)
{
#ifdef	PROGRESSIVE_SCAN
	int				j = 0;
#endif
	int				i = 0;
	wlan_adapter			*Adapter = priv->adapter;
	HostCmd_DS_802_11_SCAN 		*scan = &cmd->params.scan;
	CHANNEL_FREQ_POWER		*cfp;

	MrvlIEtypes_SsIdParamSet_t	*ssid;
	MrvlIEtypes_ChanListParamSet_t  *chan;
	MrvlIEtypes_RatesParamSet_t	*rates;
	MrvlIEtypes_NumProbes_t *scanprobes;

	u8				scanType;
	int ssidPos, chanPos, ratesPos, probPos =0;
#ifdef MULTI_BANDS
	int 				ret=0;
	HostCmd_DS_802_11_BAND_CONFIG	bc;
#endif /* MULTI BANDS*/

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SCAN);
	
	ssidPos = 1+ MRVDRV_ETH_ADDR_LEN;

	scan->BSSType = Adapter->ScanMode;

	memcpy(scan->BSSID, Adapter->SpecificScanBSSID, ETH_ALEN);

	/*SetSsIdParamSet*/
	ssid = (MrvlIEtypes_SsIdParamSet_t *)( (char *)scan + ssidPos );

 	ssid->Header.Type = wlan_cpu_to_le16(TLV_TYPE_SSID); /*0x0000; */
	ssid->Header.Len = 0; 

	if (Adapter->SetSpecificScanSSID) {
		PRINTK2("Setting SpecificScanSSID to %s\n",
			Adapter->SpecificScanSSID.Ssid);

		PRINTK2("Length of SpecificScanSSID is %u\n",
		       Adapter->SpecificScanSSID.SsidLength);

		/*Setup specific Ssid TLV*/
		ssid->Header.Len = Adapter->SpecificScanSSID.SsidLength;
		memcpy(ssid->SsId, Adapter->SpecificScanSSID.Ssid,
				Adapter->SpecificScanSSID.SsidLength);
		scanType = HostCmd_SCAN_TYPE_ACTIVE;
	} else {
		scanType = Adapter->ScanType;
	}


	chanPos = ssidPos + ssid->Header.Len + sizeof( MrvlIEtypesHeader_t ); 
	chan = (MrvlIEtypes_ChanListParamSet_t *)( (char*)scan + chanPos );
	chan->Header.Type = wlan_cpu_to_le16(TLV_TYPE_CHANLIST); /*0x0101*/
	chan->Header.Len = 0;

#ifdef	PROGRESSIVE_SCAN
	{
		int k;

		if (Adapter->SetSpecificScanSSID == TRUE) 
 			k= MRVDRV_MAX_CHANNELS_PER_SCAN;
		else
			k = MRVDRV_CHANNELS_PER_SCAN;
		

		if (Adapter->ScanChannelsLeft <= 0) {
			Adapter->ScanChannelsLeft = 
				Adapter->cur_region_channel->NrCFP;
		}

		j = ((Adapter->ScanChannelsLeft - 1) / k) * k;

		cfp = Adapter->cur_region_channel->CFP;
		
		cfp = cfp + j;
		{ 
		int i;
		for ( i = 0; i < Adapter->cur_region_channel->NrCFP; i++ ) 
			PRINTK2("%x::ChNo:%x\n", i, 
				(Adapter->cur_region_channel->CFP+i)->Channel);
		}

		
		PRINTK2("============================\n");
		PRINTK2("k = %x j=%x NrCFP=%x Left=%x\n", k, j, 
				Adapter->cur_region_channel->NrCFP,
				Adapter->ScanChannelsLeft );	
		for (i = j; (((i - j) < k) && 
				(i < Adapter->cur_region_channel->NrCFP));
								i++, cfp++) {
#ifdef MULTI_BANDS
			if ( Adapter->cur_region_channel->Band == BAND_B || 
				Adapter->cur_region_channel->Band == BAND_G ) {
				chan->ChanScanParam[i-j].RadioType = 
					HostCmd_SCAN_RADIO_TYPE_BG; /*BG*/
			}
			else
				chan->ChanScanParam[i-j].RadioType = 
					HostCmd_SCAN_RADIO_TYPE_A;
#else
			chan->ChanScanParam[i-j].RadioType = 
				HostCmd_SCAN_RADIO_TYPE_BG; /*BG*/
#endif

#ifdef ENABLE_802_11D
			if( wlan_get_state_11d( priv) == ENABLE_11D ) {
				scanType = wlan_get_scan_type_11d(
						cfp->Channel, 
						&Adapter->parsed_region_chan
						);
			}

#endif
			if ( scanType == HostCmd_SCAN_TYPE_PASSIVE ) {
				chan->ChanScanParam[i-j].MaxScanTime = 
                    wlan_cpu_to_le16(HostCmd_SCAN_PASSIVE_MAX_CH_TIME);
                chan->ChanScanParam[i-j].ChanScanMode.PassiveScan = TRUE;
            } else {
                chan->ChanScanParam[i-j].MaxScanTime = 
				   wlan_cpu_to_le16(HostCmd_SCAN_MAX_CH_TIME);
                chan->ChanScanParam[i-j].ChanScanMode.PassiveScan = FALSE;
            }

            if (Adapter->SetSpecificScanSSID == TRUE) {
                chan->ChanScanParam[i-j].ChanScanMode.DisableChanFilt = TRUE;
            }
		
			chan->ChanScanParam[i-j].ChanNumber = cfp->Channel;
			
			chan->Header.Len += sizeof( ChanScanParamSet_t);

			PRINTK2("ch[%d] ScanType=%d\n", cfp->Channel, scanType);

			PRINTK2("TLV Scan: Max=%d\n", chan->ChanScanParam[i-j].MaxScanTime );
	
			PRINTK2("chan=%d len=%d(i=%d, j=%d)\n", i-j, 
						chan->Header.Len, i, j );
		}
		
		Adapter->ScanChannelsLeft = j;
	}
#endif

	probPos = chanPos + chan->Header.Len + sizeof(MrvlIEtypesHeader_t);
	scanprobes = (MrvlIEtypes_NumProbes_t *)( (char *)scan + probPos );
  scanprobes->Header.Len = wlan_cpu_to_le16(0);

	/* Handle number of probes */

	if (Adapter->ScanProbes != 0)
  {
		scanprobes->Header.Type = wlan_cpu_to_le16(TLV_TYPE_NUMPROBES); /*0x0102*/
		scanprobes->Header.Len = wlan_cpu_to_le16(2);
		scanprobes->NumProbes = wlan_cpu_to_le16(Adapter->ScanProbes);
	}

	/* Handle the Rates */
	ratesPos = probPos+ scanprobes->Header.Len + sizeof(MrvlIEtypesHeader_t);
	rates = (MrvlIEtypes_RatesParamSet_t *)( (char *)scan + ratesPos );
	rates->Header.Type = wlan_cpu_to_le16(TLV_TYPE_RATES); /*0x0001*/

#ifdef MULTI_BANDS
	if (Adapter->cur_region_channel->Band == BAND_A) {
		memcpy(rates->Rates, SupportedRates_A, 
						sizeof(SupportedRates_A));
		rates-> Header.Len  = wlan_cpu_to_le16(sizeof ( SupportedRates_A )); 
	} else if (Adapter->cur_region_channel->Band == BAND_B) {
		memcpy(rates->Rates, SupportedRates_B, 
						sizeof(SupportedRates_B));
		rates-> Header.Len  = wlan_cpu_to_le16(sizeof ( SupportedRates_B )); 
	} else if (Adapter->cur_region_channel->Band == BAND_G) {
		memcpy(rates->Rates, SupportedRates_G, 
						sizeof(SupportedRates_G));
		rates-> Header.Len  = wlan_cpu_to_le16(sizeof ( SupportedRates_G )); 
	}

	/* We had to send BAND_CONFIG for every scan command */
	if (Adapter->is_multiband) {
		bc.BandSelection = Adapter->cur_region_channel->Band;
		
		if ((Adapter->MediaConnectStatus == WlanMediaStateConnected) &&
			(Adapter->CurBssParams.band == bc.BandSelection))
			bc.Channel = Adapter->CurBssParams.channel;
		else
			bc.Channel = Adapter->cur_region_channel->CFP->Channel;

		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_BAND_CONFIG,
					HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
					, 0, HostCmd_PENDING_ON_NONE, &bc);

		if (ret) {
			LEAVE();
			return ret;
		}
	}

#else
	memcpy(rates->Rates, SupportedRates, sizeof(SupportedRates));
	rates-> Header.Len  = wlan_cpu_to_le16(sizeof ( SupportedRates )); 
#endif /* MULTI_BAND */

	cmd->Size = wlan_cpu_to_le16(ratesPos + rates->Header.Len + 
				sizeof( MrvlIEtypesHeader_t ) + S_DS_GEN);

	PRINTK2("cmd->Size=%x\n", cmd->Size );

	Adapter->bIsScanInProgress = TRUE;

        /* Flush all the packets upto the OS before stopping */
	wlan_send_rxskbQ(priv);
	os_carrier_off(priv);

	PRINTK2("ssidPos=%d chanPos=%d Rates=%d\n", ssidPos, chanPos, ratesPos);
	HEXDUMP( "TLV Scan Cmd:", (u8 *)cmd, (int)cmd->Size );

	PRINTK1("Printing all the values\n");
	PRINTK1("Command=%x\n", cmd->Command);
	PRINTK1("Size=%x\n", cmd->Size);
	PRINTK1("Sequence Num=%x\n", cmd->SeqNum);

	PRINTK1("HWAC - SCAN command is ready\n");

	LEAVE();
	return 0;
}
#endif /*TLV_SCAN*/

#ifndef TLV_SCAN
static inline int wlan_cmd_802_11_scan(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd)
{
#ifdef	PROGRESSIVE_SCAN
	int				j = 0;
#endif
	int				i = 0;
	wlan_adapter			*Adapter = priv->adapter;
	HostCmd_DS_802_11_SCAN 		*scan = &cmd->params.scan;
	CHANNEL_FREQ_POWER		*cfp;
#ifdef MULTI_BANDS
	int 				ret=0;
	HostCmd_DS_802_11_BAND_CONFIG	bc;
#endif /* MULTI BANDS*/

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SCAN);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_SCAN) + S_DS_GEN);

#if 0 
	scan->IsAutoAssociation = wlan_cpu_to_le16((u16) (adapter->bAutoAssociation));
	PRINTK1("Is Autoassociation %u\n", scan->IsAutoAssociation);
#endif

	scan->IsAutoAssociation = 0;

	/* ScanMode to support the ScanMode private command */
	scan->BSSType = Adapter->ScanMode;

	memcpy(scan->BSSID, Adapter->SpecificScanBSSID, ETH_ALEN);

	memset(scan->SSID, 0, MRVDRV_MAX_SSID_LENGTH);

	if (Adapter->SetSpecificScanSSID) {
		PRINTK1("Setting SpecificScanSSID to %s\n",
				Adapter->SpecificScanSSID.Ssid);

		PRINTK1("Length of SpecificScanSSID is %u\n",
			       Adapter->SpecificScanSSID.SsidLength);

		memcpy(scan->SSID, Adapter->SpecificScanSSID.Ssid,
		Adapter->SpecificScanSSID.SsidLength);
		scan->ScanType = HostCmd_SCAN_TYPE_ACTIVE;
	} else {
		scan->ScanType = Adapter->ScanType;
	}

	scan->ProbeDelay = wlan_cpu_to_le16(HostCmd_SCAN_PROBE_DELAY_TIME);
	scan->MinCHTime = wlan_cpu_to_le16(HostCmd_SCAN_MIN_CH_TIME);

#ifdef MULTI_BANDS
	if ((Adapter->SetSpecificScanSSID == TRUE) && Adapter->is_multiband)
		scan->MaxCHTime = 
			wlan_cpu_to_le16(HostCmd_SCAN_MAX_CH_TIME_MULTI_BANDS);
	else
#endif
		scan->MaxCHTime = wlan_cpu_to_le16(HostCmd_SCAN_MAX_CH_TIME);

	/* Use the region code to determine channel numbers */
	memset(scan->CHList, 0, sizeof(scan->CHList));

#ifdef	PROGRESSIVE_SCAN
	{
		int k;

		if (Adapter->SetSpecificScanSSID == TRUE) 
 			k= MRVDRV_MAX_CHANNELS_PER_SCAN;
		else
			k = MRVDRV_CHANNELS_PER_SCAN;
		

		if (Adapter->ScanChannelsLeft <= 0) {
			Adapter->ScanChannelsLeft = 
				Adapter->cur_region_channel->NrCFP;
		}

		j = ((Adapter->ScanChannelsLeft - 1) / k) * k;

		cfp = Adapter->cur_region_channel->CFP;
		
		memset(scan->CHList, 0, sizeof(scan->CHList));

		cfp = cfp + j;
		
		for (i = j; (((i - j) < k) && 
				(i < Adapter->cur_region_channel->NrCFP));
								i++, cfp++) {
			scan->CHList[i - j] = cfp->Channel;
		}
		
		Adapter->ScanChannelsLeft = j;
	}
#else
	/*
	 * Old SCAN all channels with a MAX of 14 channels coming in
	 * on the scan list
	 */      
	{
		cfp = Adapter->cur_region_channel->CFP;
	
		memset(scan->CHList, 0, sizeof(scan->CHList));

		for (i = 0; i < MIN(Adapter->cur_region_channel->NrCFP, 
			MRVDRV_MAX_CHANNEL_SIZE); i++) {
			scan->CHList[i] = (cfp->Channel);
			cfp++;
		}
	}
#endif

	memset(scan->DataRate, 0, sizeof(scan->DataRate));
#ifdef MULTI_BANDS
	if (Adapter->cur_region_channel->Band == BAND_A) {
		memcpy(scan->DataRate, SupportedRates_A, 
						sizeof(SupportedRates_A));
	} else if (Adapter->cur_region_channel->Band == BAND_B) {
		memcpy(scan->DataRate, SupportedRates_B, 
						sizeof(SupportedRates_B));
	} else if (Adapter->cur_region_channel->Band == BAND_G) {
		memcpy(scan->DataRate,	SupportedRates_G, 
						sizeof(SupportedRates_G));
	} 

	/* We had to send BAND_CONFIG for every scan command */
	if (Adapter->is_multiband) {
		bc.BandSelection = Adapter->cur_region_channel->Band;

		if ((Adapter->MediaConnectStatus == WlanMediaStateConnected) &&
			(Adapter->CurBssParams.band == bc.BandSelection))
			bc.Channel = Adapter->CurBssParams.channel;
		else
			bc.Channel = Adapter->cur_region_channel->CFP->Channel;

		ret = PrepareAndSendCommand(priv, 
					HostCmd_CMD_802_11_BAND_CONFIG,
					HostCmd_ACT_SET, HostCmd_OPTION_USE_INT 
					, 0, HostCmd_PENDING_ON_NONE, &bc);

		if (ret) {
			LEAVE();
			return ret;
		}
	}

#else
	memcpy(scan->DataRate, SupportedRates, sizeof(SupportedRates));
#endif

	Adapter->bIsScanInProgress = TRUE;
			
	/* Flush all the packets upto the OS before stopping */
	wlan_send_rxskbQ(priv);
	os_carrier_off(priv);

	PRINTK1("Printing all the values\n");
	PRINTK1("Command=%x\n", cmd->Command);
	PRINTK1("Size=%x\n", cmd->Size);
	PRINTK1("Sequence Num=%x\n", cmd->SeqNum);
	PRINTK1("IsAutoAssociation %d\n", scan->ScanType);
	PRINTK1("BSS type  %d\n", scan->BSSType);
	PRINTK1("SpecificScanSSID is %s\n", (Adapter->SetSpecificScanSSID) ?
			(char *) scan->SSID : "NULL");

#ifdef DEBUG_ALL
	{
		int i;

		for (i = 0; i < Adapter->cur_region_channel->NrCFP; i++) {
			PRINTK1("Channel List %d\n", scan->CHList[i]);
		}
    	}
#endif

#ifdef	PROGRESSIVE_SCAN
	PRINTK1("Channel List %d\n", scan->CHList[j]);
#endif
	PRINTK1("Scan Type %d\n", scan->ScanType);

	PRINTK1("HWAC - SCAN command is ready\n");
	return 0;
}
#endif /*TLV_SCAN*/

/*
 * Function: Mac Control 
 */
static inline int wlan_cmd_mac_control(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd)
{
	/*
	* TODO Check For the CmdOptions 
	*/
	HostCmd_DS_MAC_CONTROL *mac = &cmd->params.macctrl;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_MAC_CONTROL);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_MAC_CONTROL) + S_DS_GEN);
	mac->Action = wlan_cpu_to_le16(priv->adapter->CurrentPacketFilter);

	//PRINTK("cmd_mac_control() Action=0x%X Size=%d\n",
	//				mac->Action,cmd->Size);

	LEAVE();
	return 0;
}

void CleanupAndInsertCmd(wlan_private * priv, CmdCtrlNode * pTempCmd)
{
	u32		flags;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	if (!pTempCmd)
		return;

	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
	CleanUpCmdCtrlNode(pTempCmd);
        list_add_tail((struct list_head *)pTempCmd, &Adapter->CmdFreeQ);
	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

	LEAVE();
}

int SetRadioControl(wlan_private *priv)
{
	int		ret = 0;

	ENTER();

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RADIO_CONTROL,
				HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT, 
				0, HostCmd_PENDING_ON_GET_OID, NULL);

	mdelay(200);

	PRINTK1("Radio = 0x%X, Preamble = 0x%X\n", 
			priv->adapter->RadioOn,	priv->adapter->Preamble);

	LEAVE();
	return ret;
}

/* This function will verify the provided rates with the card 
 * supported rates, and fills only the common rates between them 
 * in to the destination address 
 * NOTE: Setting the MSB of the basic rates need to be taken
 * 	 care, either before or after calling this function */
inline int get_common_rates(wlan_adapter *Adapter, u8 *dest, int size1, 
				u8 *card_rates, int size2)
{
	int	i;
	u8	tmp[30], *ptr = dest;

	memset(&tmp, 0, sizeof(tmp));
	memcpy(&tmp, dest, MIN(size1,sizeof(tmp)));
	memset(dest, 0, size1);

	/* Mask the top bit of the original values */
	for (i = 0; tmp[i] && i < sizeof(tmp); i++)
		tmp[i] &= 0x7F;

	for (i = 0; card_rates[i] && i < size2; i++) {
		/* Check for Card Rate in tmp, excluding the top bit */
		if (strchr(tmp, card_rates[i] & 0x7F)) {
			/* Values match, so copy the Card Rate to dest */
			*dest++ = card_rates[i];
		}
	}

	HEXDUMP("AP Rates", tmp, sizeof(tmp));
	HEXDUMP("Card Rates", card_rates, size2);
	HEXDUMP("Common Rates", ptr, size1);
	PRINTK1("Data Rate = 0x%X\n", Adapter->DataRate);

	if (!Adapter->Is_DataRate_Auto) {
		while (*ptr) {
			if ((*ptr & 0x7f) == Adapter->DataRate)
				return 0;	// fixed rate match
			ptr++;
		}
		printk(KERN_ALERT "Previously set fixed data rate %#x isn't "
			"compatible with the network.\n",Adapter->DataRate);
		return -1;			// no fixed rate match
	}

	return 0;				// auto rate
}

#ifdef NEW_ASSOCIATION
#ifdef TLV_ASSOCIATION
static inline int wlan_cmd_802_11_associate(wlan_private * priv,
			  CmdCtrlNode * pTempCmd, 
			  void *InfoBuf)
{
	int				ret = 0;
	int		           	i;
	wlan_adapter			*Adapter = priv->adapter;

	HostCmd_DS_COMMAND 		*cmd = (HostCmd_DS_COMMAND *)
						pTempCmd->BufVirtualAddr;
	HostCmd_DS_802_11_ASSOCIATE 	*pAsso = &cmd->params.associate;
	u8  				*pReqBSSID = NULL;
	WLAN_802_11_BSSID		*pBSSIDList;
	u8				*card_rates;
	int				card_rates_size;
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG	bc;
#endif /* MULTI BANDS*/
	u16				TmpCap;
	u8				*pos = (u8 *)pAsso;
	MrvlIEtypes_SsIdParamSet_t	*ssid;
	MrvlIEtypes_PhyParamSet_t	*phy;
	MrvlIEtypes_SsParamSet_t	*ss;
	MrvlIEtypes_RatesParamSet_t	*rates;
#ifdef WPA
	MrvlIEtypes_RsnParamSet_t	*rsn;
#endif
#ifdef WMM
	MrvlIEtypes_WmmParamSet_t	*wmm;
#endif

	ENTER();

	if (!Adapter)
		return -ENODEV;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_ASSOCIATE);

	if (Adapter->RequestViaBSSID)
		pReqBSSID = Adapter->RequestedBSSID;
    
	/*
	 * find out the BSSID that matches 
	 * the SSID given in InformationBuffer
	 */
	PRINTK1("NumOfBSSIDs = %u\n", Adapter->ulNumOfBSSIDs);
	PRINTK1("Wanted SSID = %s\n", ((WLAN_802_11_SSID *)InfoBuf)->Ssid);

#ifdef	DEBUG
	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		ssid = &Adapter->BSSIDList[i].Ssid;
		PRINTK1("Listed SSID = %s\n", ssid->Ssid);
	}
#endif

	i = FindSSIDInList(Adapter, InfoBuf, pReqBSSID, 
						Wlan802_11Infrastructure);

	if (i >= 0) {
		memmove(pAsso->PeerStaAddr, &(Adapter->BSSIDList[i].MacAddress),
				ETH_ALEN);
	} else {
		PRINTK1("HWAC - SSID is not in the list\n");
		return -1;
	}
	pos += sizeof(pAsso->PeerStaAddr);

#if 0
    /* 
    ** Code Disabled: Control flow of driver causes deauth before entry
    **   into this function for all cases except reassociation.  For reassoc,
    **   we disable this check.  In all other cases, it fails since the
    **   deauth will have already happened.  Remove soon if still not required.
    **   (7/2005)
    */
	/* check if the requested SSID is already associated */
	if ((Adapter->CurBssParams.ssid.SsidLength != 0)
		&& !SSIDcmp(InfoBuf, &Adapter->CurBssParams.ssid)
        && (Adapter->CurrentBSSIDDescriptor.InfrastructureMode
            == Wlan802_11Infrastructure)
        && !memcmp(Adapter->CurrentBSSID, 
                   Adapter->BSSIDList[i].MacAddress, ETH_ALEN)
        && !Adapter->reassocAttempt)
    {
        /* current associated SSID is same as the new one */
        PRINTK1("New SSID is the same as current, not "
                "attempting to re-associate\n");
        return -1;
	}
#endif

	/* Set the temporary BSSID Index */
	Adapter->ulAttemptedBSSIDIndex = i;
	pBSSIDList = &Adapter->BSSIDList[i];

	// set preamble to firmware
	if (Adapter->capInfo.ShortPreamble && pBSSIDList->Cap.ShortPreamble) {
		Adapter->Preamble = HostCmd_TYPE_SHORT_PREAMBLE;
	} else {
		Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;
	}
	SetRadioControl(priv);
	
	/* set the Capability info */
	memcpy(&TmpCap, &pBSSIDList->Cap, sizeof(pAsso->CapInfo));

	TmpCap &= CAPINFO_MASK;
	PRINTK("TmpCap=%4X CAPINFO_MASK=%4X\n", TmpCap, CAPINFO_MASK );

	TmpCap = wlan_cpu_to_le16(TmpCap);

	memcpy(&pAsso->CapInfo, &TmpCap, sizeof(pAsso->CapInfo));
	pos += sizeof(pAsso->CapInfo);

	/* set the listen interval */
	pAsso->ListenInterval = Adapter->ListenInterval;
	pos += sizeof(pAsso->ListenInterval);

	pos += sizeof(pAsso->BcnPeriod);

	pos += sizeof(pAsso->DtimPeriod);

	ssid = (MrvlIEtypes_SsIdParamSet_t *)pos;
 	ssid->Header.Type = wlan_cpu_to_le16(TLV_TYPE_SSID);
	ssid->Header.Len = pBSSIDList->Ssid.SsidLength;
	memcpy(ssid->SsId, pBSSIDList->Ssid.Ssid, ssid->Header.Len);
	pos += sizeof(ssid->Header) + ssid->Header.Len;

	phy = (MrvlIEtypes_PhyParamSet_t *)pos;
 	phy->Header.Type = wlan_cpu_to_le16(TLV_TYPE_PHY_DS);
	phy->Header.Len = sizeof(phy->fh_ds.DsParamSet);
	memcpy(&phy->fh_ds.DsParamSet, &pBSSIDList->PhyParamSet.DsParamSet.CurrentChan,
				sizeof(phy->fh_ds.DsParamSet));
	pos += sizeof(phy->Header) + phy->Header.Len;

	ss = (MrvlIEtypes_SsParamSet_t *)pos;
 	ss->Header.Type = wlan_cpu_to_le16(TLV_TYPE_CF);
	ss->Header.Len = sizeof(ss->cf_ibss.CfParamSet);
	pos += sizeof(ss->Header) + ss->Header.Len;

	rates = (MrvlIEtypes_RatesParamSet_t *)pos;
 	rates->Header.Type = wlan_cpu_to_le16(TLV_TYPE_RATES);

	memcpy(&rates->Rates,&pBSSIDList->SupportedRates, WLAN_SUPPORTED_RATES);

#ifdef	MULTI_BANDS
	if (pBSSIDList->bss_band == BAND_A) {
		card_rates = SupportedRates_A;
		card_rates_size = sizeof(SupportedRates_A);
	}
	else if (pBSSIDList->bss_band == BAND_B) {
		card_rates = SupportedRates_B;
		card_rates_size = sizeof(SupportedRates_B);
	}
	else if (pBSSIDList->bss_band == BAND_G) {
		card_rates = SupportedRates_G;
		card_rates_size = sizeof(SupportedRates_G);
	}
	else {
		return -EINVAL;
	}
#else
	card_rates = SupportedRates;
	card_rates_size = sizeof(SupportedRates);
#endif	/* MULTI_BANDS*/

	if (get_common_rates(Adapter, rates->Rates, WLAN_SUPPORTED_RATES,
						card_rates, card_rates_size)) {
		return -EINVAL;
	}

	//strlen() here may not right !!!
	rates->Header.Len = MIN(strlen(rates->Rates), WLAN_SUPPORTED_RATES);
	Adapter->CurBssParams.NumOfRates = rates->Header.Len;	

	pos += sizeof(rates->Header) + rates->Header.Len;

#ifdef WPA
	if (Adapter->SecInfo.WPAEnabled
#ifdef WPA2
		|| Adapter->SecInfo.WPA2Enabled
#endif //WPA2
		) {
		rsn = (MrvlIEtypes_RsnParamSet_t *)pos;
 		rsn->Header.Type = (u16)Adapter->Wpa_ie[0];	/* WPA_IE or WPA2_IE */
 		rsn->Header.Type = wlan_cpu_to_le16(rsn->Header.Type);
 		rsn->Header.Len = (u16)Adapter->Wpa_ie[1];
		memcpy(rsn->RsnIE, &Adapter->Wpa_ie[2], rsn->Header.Len);
		HEXDUMP("RSN IE", (u8*)rsn, sizeof(rsn->Header) + rsn->Header.Len); 
		pos += sizeof(rsn->Header) + rsn->Header.Len;
	}
#endif //WPA

#ifdef WMM
	if (Adapter->wmm.required && pBSSIDList->Wmm_IE[0] == WMM_IE) {
		Adapter->CurrentPacketFilter |= HostCmd_ACT_MAC_WMM_ENABLE;
		SetMacPacketFilter(priv);
	
		wmm = (MrvlIEtypes_WmmParamSet_t *)pos;
 		wmm->Header.Type = (u16)wmm_ie[0];
 		wmm->Header.Type = wlan_cpu_to_le16(wmm->Header.Type);
 		wmm->Header.Len = (u16)wmm_ie[1];
		memcpy(wmm->WmmIE, &wmm_ie[2], wmm->Header.Len);
#ifdef WMM_UAPSD
	if((pBSSIDList->Wmm_IE[WMM_QOS_INFO_OFFSET] & WMM_QOS_INFO_UAPSD_BIT) && ((Adapter->wmm.qosinfo & 0x0f) != 0))
	{
		memcpy((u8 *)wmm->WmmIE + wmm->Header.Len - 1, &Adapter->wmm.qosinfo, 1);
	}
#endif
		HEXDUMP("WMM IE", (u8*)wmm, sizeof(wmm->Header) + wmm->Header.Len); 
		pos += sizeof(wmm->Header) + wmm->Header.Len;
	}
	else {
		Adapter->CurrentPacketFilter &= ~HostCmd_ACT_MAC_WMM_ENABLE;
		SetMacPacketFilter(priv);
	}
#endif /* WMM */

#ifdef REASSOCIATION_SUPPORT
    wlan_cmd_append_reassoc_tlv(priv, &pos);
#endif

#ifdef IWGENIE_SUPPORT
    wlan_cmd_append_generic_ie(priv, &pos);
#endif

#ifdef CCX
#ifdef ENABLE_802_11H_TPC
    wlan_ccx_process_association_req(&pos, &pBSSIDList->ccx_bss_info,
                                     !pBSSIDList->Sensed11H);
#else
    wlan_ccx_process_association_req(&pos, &pBSSIDList->ccx_bss_info, TRUE);
#endif
#endif
	cmd->Size = wlan_cpu_to_le16((u16)(pos - (u8 *)pAsso) + S_DS_GEN);

	/* update CurBssParams */
 	Adapter->CurBssParams.channel = (pBSSIDList
                                     ->PhyParamSet.DsParamSet.CurrentChan);
#ifdef MULTI_BANDS
	Adapter->CurBssParams.band = pBSSIDList->bss_band;
#endif	/* MULTI_BANDS*/

	/* Copy the infra. association rates into Current BSS state structure */
	memcpy(&Adapter->CurBssParams.DataRates, &rates->Rates, 
           MIN(sizeof(Adapter->CurBssParams.DataRates), rates->Header.Len));

	PRINTK1("rates->Header.Len = %d\n", rates->Header.Len);

	/* set IBSS field */
	if (pBSSIDList->InfrastructureMode == Wlan802_11Infrastructure) 
    {
		pAsso->CapInfo.Ess = 1; /* HostCmd_CAPINFO_ESS */

#ifdef ENABLE_802_11D
		ret = wlan_parse_dnld_countryinfo_11d( priv );
		if ( ret ) 
        {
			LEAVE();
			return ret;
		}
#endif

#ifdef ENABLE_802_11H_TPC
	/*infra, before Assoc, send power constraint to FW first. */
		{
			PWLAN_802_11_BSSID bssid = 
				&(Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex]);

			if (Adapter->State11HTPC.Enable11HTPC==TRUE && 
							bssid->Sensed11H )
				pAsso->CapInfo.SpectrumMgmt = 1;
	
			PRINTK2( "11HTPC: Cap = 0x%x\n", *(unsigned int *)&(pAsso->CapInfo));

			PRINTK2("11HTPC:Enable11H=%s Sensed11H=%s\n", 
				(Adapter->State11HTPC.Enable11HTPC == TRUE)?"TRUE":"FALSE",
				(bssid->Sensed11H)?"TRUE":"FALSE" 
				);

			if (Adapter->State11HTPC.Enable11HTPC==TRUE && 
							bssid->Sensed11H ) {
				Adapter->State11HTPC.InfoTpc.Chan = 
					Adapter->CurBssParams.channel;
				Adapter->State11HTPC.InfoTpc.PowerConstraint = 
					bssid->PowerConstraint.LocalPowerConstraint;

				/*Enabel 11H and Dnld TPC Info to FW*/
				wlan_802_11h_tpc_enable( priv, TRUE ); 
	
				ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11H_TPC_INFO,
					HostCmd_ACT_SET, HostCmd_OPTION_USE_INT,
					0, HostCmd_PENDING_ON_NONE, NULL);
				if (ret) {
					PRINTK2("11HTPC:Err: Send TPC_INFO CMD: %d\n", ret);
					LEAVE();
					return ret;
				}
			}
			else {
				wlan_802_11h_tpc_enable( priv, FALSE ); /* Disable 11H*/
			}
		}
#endif
	}
//	else
//		pAsso->CapInfo.Ibss = 1; /* HostCmd_CAPINFO_IBSS */

	Adapter->bIsAssociationInProgress = TRUE;

	/* need to report disconnect event if currently associated */
	if (Adapter->CurBssParams.ssid.SsidLength != 0) {
		MacEventDisconnected(priv);
	}

#ifdef MULTI_BANDS
	if (Adapter->is_multiband) {
		bc.BandSelection = Adapter->CurBssParams.band;
		bc.Channel = Adapter->CurBssParams.channel;

		ret = PrepareAndSendCommand(priv,
                                    HostCmd_CMD_802_11_BAND_CONFIG,
                                    HostCmd_ACT_SET, 
                                    (HostCmd_OPTION_USE_INT 
                                     | HostCmd_OPTION_WAITFORRSP),
                                    0, HostCmd_PENDING_ON_NONE, &bc);
        
		if (ret) {
			LEAVE();
			return ret;
		}
	}
#endif /* MULTI_BANDS */

	LEAVE();
	return ret;
}

#else /* !TLV_ASSOCIATION */

static inline int wlan_cmd_802_11_associate(wlan_private * priv,
			  CmdCtrlNode * pTempCmd, 
			  void *InfoBuf)
{
	int				ret = 0;
	int		           	i;
	wlan_adapter			*Adapter = priv->adapter;

	HostCmd_DS_COMMAND 		*cmd = (HostCmd_DS_COMMAND *)
						pTempCmd->BufVirtualAddr;
	HostCmd_DS_802_11_ASSOCIATE 	*pAsso = &cmd->params.associate;
	u8  				*pReqBSSID = NULL;
	u8				*card_rates;
	int				card_rates_size;
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG	bc;
#endif /* MULTI BANDS*/
#ifdef	DEBUG
	WLAN_802_11_SSID		*ssid;
#endif
	u16				TmpCap;

	ENTER();

	if (!Adapter)
		return -ENODEV;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_ASSOCIATE);
	cmd->Size = sizeof(HostCmd_DS_802_11_ASSOCIATE) + S_DS_GEN - sizeof(pAsso->RsnIE);

	if (Adapter->RequestViaBSSID)
		pReqBSSID = Adapter->RequestedBSSID;
    
	/*
	 * find out the BSSID that matches 
	 * the SSID given in InformationBuffer
	 */
	PRINTK1("NumOfBSSIDs = %u\n", Adapter->ulNumOfBSSIDs);
	PRINTK1("Wanted SSID = %s\n", ((WLAN_802_11_SSID *)InfoBuf)->Ssid);

#ifdef	DEBUG
	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		ssid = &Adapter->BSSIDList[i].Ssid;
		PRINTK1("Listed SSID = %s\n", ssid->Ssid);
	}
#endif

	i = FindSSIDInList(Adapter, InfoBuf, pReqBSSID, 
						Wlan802_11Infrastructure);

	if (i >= 0) {
		memmove(pAsso->PeerStaAddr, &(Adapter->BSSIDList[i].MacAddress),
				ETH_ALEN);
	} else {
		PRINTK1("HWAC - SSID is not in the list\n");
		return -1;
	}

	/* check if the requested SSID is already associated */
	if ((Adapter->CurBssParams.ssid.SsidLength != 0) &&
		!SSIDcmp(InfoBuf, &Adapter->CurBssParams.ssid) && 
		!memcmp(Adapter->CurrentBSSID,
                Adapter->BSSIDList[i].MacAddress, ETH_ALEN)) {
		if ((&(Adapter->CurrentBSSIDDescriptor) != NULL)) {
			if (Adapter->CurrentBSSIDDescriptor.
				InfrastructureMode 
				== Wlan802_11Infrastructure) {
				/*
				 * current associated SSID is same 
				 * as the new one
				 */
				PRINTK1("*** new SSID is the"
					"same as current, not"
					"attempting to re-associate\n");

				return -1;
			}
		}
	}

	/* Set the temporary BSSID Index */
	Adapter->ulAttemptedBSSIDIndex = i;

	/* set failure-time-out */
	pAsso->FailTimeOut = wlan_cpu_to_le16(MRVDRV_ASSOCIATION_TIME_OUT);

	// set preamble to firmware
	if (Adapter->capInfo.ShortPreamble 
			&& Adapter->BSSIDList[i].Cap.ShortPreamble) {
		Adapter->Preamble = HostCmd_TYPE_SHORT_PREAMBLE;
	} else {
		Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;
	}
	SetRadioControl(priv);
	
	/* set the Capability info */
	memcpy(&pAsso->CapInfo, &Adapter->BSSIDList[i].Cap, 
						sizeof(pAsso->CapInfo));

	memcpy(&TmpCap, &pAsso->CapInfo, sizeof(pAsso->CapInfo));
	
	TmpCap &= CAPINFO_MASK;
	PRINTK("TmpCap=%4X CAPINFO_MASK=%4X\n", TmpCap, CAPINFO_MASK);

	TmpCap = wlan_cpu_to_le16(TmpCap);

	memcpy(&pAsso->CapInfo, &TmpCap, sizeof(pAsso->CapInfo));

	memcpy(pAsso->SsId, 
			Adapter->BSSIDList
			[Adapter->ulAttemptedBSSIDIndex].Ssid.Ssid,
			Adapter->BSSIDList
			[Adapter->ulAttemptedBSSIDIndex].Ssid.SsidLength);	

	memcpy(&pAsso->PhyParamSet.DsParamSet, &(Adapter->BSSIDList[Adapter->
		ulAttemptedBSSIDIndex].PhyParamSet.DsParamSet),
				sizeof(pAsso->PhyParamSet.DsParamSet));

	memcpy(&pAsso->DataRates,&(Adapter->BSSIDList[Adapter->
		ulAttemptedBSSIDIndex].	SupportedRates), 		
				sizeof(pAsso->DataRates));

#ifdef WPA
	if (Adapter->SecInfo.WPAEnabled
#ifdef WPA2
		|| Adapter->SecInfo.WPA2Enabled
#endif //WPA2
		) {

		memcpy(pAsso->RsnIE, Adapter->Wpa_ie, sizeof(pAsso->RsnIE));
		cmd->Size += Adapter->Wpa_ie[1] + 2; // ID and length field!
#ifdef DEBUG
		HEXDUMP("RSN IE  : ", (u8*)pAsso->RsnIE, sizeof(pAsso->RsnIE));
#endif
	}
#endif //WPA

  cmd->Size = wlan_cpu_to_le16(cmd->Size);

#ifdef	MULTI_BANDS
	if (Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band == 
								BAND_A) {
		card_rates = SupportedRates_A;
		card_rates_size = sizeof(SupportedRates_A);
	}
	else if (Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band == 
								BAND_B) {
		card_rates = SupportedRates_B;
		card_rates_size = sizeof(SupportedRates_B);
	}
	else if (Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band == 
								BAND_G) {
		card_rates = SupportedRates_G;
		card_rates_size = sizeof(SupportedRates_G);
	}
	else {
		return -EINVAL;
	}
#else
	card_rates = SupportedRates;
	card_rates_size = sizeof(SupportedRates);
#endif	/* MULTI_BANDS*/

 	Adapter->CurBssParams.channel = 
		Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].
					PhyParamSet.DsParamSet.CurrentChan;
#ifdef MULTI_BANDS
	Adapter->CurBssParams.band = 
		Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band;
#endif	/* MULTI_BANDS*/

	if (get_common_rates(Adapter, pAsso->DataRates, 
						sizeof(pAsso->DataRates),
						card_rates, card_rates_size)) {
		return -EINVAL;
	}

	for (i=0; i < MIN(sizeof(pAsso->DataRates),IW_MAX_BITRATES) 
		&& pAsso->DataRates[i]; i++) {
	}
	Adapter->CurBssParams.NumOfRates = i;

	/* Copy the infra. association rates into Current BSS state structure */
	memcpy(&Adapter->CurBssParams.DataRates, &pAsso->DataRates, 
		Adapter->CurBssParams.NumOfRates);

	/* set IBSS field */
	if ((Adapter->BSSIDList[i].InfrastructureMode) ==
				Wlan802_11Infrastructure) {
		pAsso->CapInfo.Ess = 1; /* HostCmd_CAPINFO_ESS */

#ifdef ENABLE_802_11D
		ret = wlan_parse_dnld_countryinfo_11d( priv );
		if ( ret ) {
			LEAVE();
			return ret;
		}
#endif

#ifdef ENABLE_802_11H_TPC
	/*infra, before Assoc, send power constraint to FW first. */
		{
			PWLAN_802_11_BSSID bssid = 
				&(Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex]);

			if (Adapter->State11HTPC.Enable11HTPC==TRUE && 
							bssid->Sensed11H )
				pAsso->CapInfo.SpectrumMgmt = 1;
	
			PRINTK2( "11HTPC: Cap = 0x%x\n", *(unsigned int *)&(pAsso->CapInfo));

			PRINTK2("11HTPC:Enable11H=%s Sensed11H=%s\n", 
				(Adapter->State11HTPC.Enable11HTPC == TRUE)?"TRUE":"FALSE",
				(bssid->Sensed11H)?"TRUE":"FALSE" 
				);

			if (Adapter->State11HTPC.Enable11HTPC==TRUE && 
							bssid->Sensed11H ) {
				Adapter->State11HTPC.InfoTpc.Chan = 
					Adapter->CurBssParams.channel;
				Adapter->State11HTPC.InfoTpc.PowerConstraint = 
					bssid->PowerConstraint.LocalPowerConstraint;

				/*Enabel 11H and Dnld TPC Info to FW*/
				wlan_802_11h_tpc_enable( priv, TRUE ); 
	
				ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11H_TPC_INFO,
					HostCmd_ACT_SET, HostCmd_OPTION_USE_INT,
					0, HostCmd_PENDING_ON_NONE, NULL);
				if (ret) {
					PRINTK2("11HTPC:Err: Send TPC_INFO CMD: %d\n", ret);
					LEAVE();
					return ret;
				}
			}
			else {
				wlan_802_11h_tpc_enable( priv, FALSE ); /* Disable 11H*/
			}
		}
#endif
	}
//	else
//		pAsso->CapInfo.Ibss = 1; /* HostCmd_CAPINFO_IBSS */

	/* set Privacy field */
//	if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled) {
//		PRINTK1("WEPStatus set, setting" 
//				"HostCmd_CAPINFO_PRIVACY bit\n");
//		pAsso->CapInfo.Privacy = 1; /* HostCmd_CAPINFO_PRIVACY */
//	}
	/* set the listen interval */
//	pAsso->ListenInterval = MRVDRV_DEFAULT_LISTEN_INTERVAL;
	pAsso->ListenInterval = Adapter->ListenInterval;

	Adapter->bIsAssociationInProgress = TRUE;

	/* need to report disconnect event if currently associated */
	if (Adapter->CurBssParams.ssid.SsidLength != 0) {
		MacEventDisconnected(priv);
	}

#ifdef MULTI_BANDS
	if (Adapter->is_multiband) {
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
#endif /* MULTI_BANDS */

	LEAVE();
	return ret;
}
#endif /* TLV_ASSOCIATION */

#else				/* OLD_ASSOCIATION */

static inline int wlan_cmd_802_11_associate(wlan_private * priv,
			  		CmdCtrlNode * cmdnode, void *InfoBuf)
{
	int 				ret = 0;
	int             		i;
	wlan_adapter   			*Adapter = priv->adapter;
	HostCmd_DS_COMMAND 		*cmd = (HostCmd_DS_COMMAND *)
						cmdnode->BufVirtualAddr;
	HostCmd_DS_802_11_ASSOCIATE	*asst = &cmd->params.associate;
	WLAN_802_11_SSID 		*ssid = InfoBuf;
	u8  				*pReqBSSID = NULL;
	u8				*card_rates;
	int				card_rates_size;
	u16				TmpCap;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_ASSOCIATE);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_ASSOCIATE) + S_DS_GEN);

	if (Adapter->RequestViaBSSID)
		pReqBSSID = Adapter->RequestedBSSID;
    
	/*
	 * The command structure is HostCmd_CMD_802_11_ASSOCIATE
	 * Check if BSSID Matches .. called from wext .. 
	 * find out the BSSID that matches the SSID 
	 * given in InfoBuf
	 */
	i = FindSSIDInList(Adapter, InfoBuf, pReqBSSID, 
						Wlan802_11Infrastructure);

	if (i >= 0) {
		memcpy(asst->DestMacAddr, 
				&Adapter->BSSIDList[i].MacAddress, ETH_ALEN);
	} else {
		PRINTK1("HWAC - SSID not in list\n");
		return WLAN_STATUS_FAILURE;
	}

	/* check if the requested SSID is already associated */
	if (Adapter->CurBssParams.ssid.SsidLength != 0 && 
			!SSIDcmp(ssid, &Adapter->CurBssParams.ssid) && 
				!memcmp(Adapter->CurrentBSSID,
    			                Adapter->BSSIDList[i].MacAddress,
								ETH_ALEN)) {
		PRINTK1("CurBssParams.ssid Len = %u, Required SSID Len = %u\n"
			"Current SSID = %s, Required SSID = %s\n",
			Adapter->CurBssParams.ssid.SsidLength, ssid->SsidLength,
			Adapter->CurBssParams.ssid.Ssid, ssid->Ssid);

		if (&Adapter->CurrentBSSIDDescriptor != NULL) {
			if (Adapter->CurrentBSSIDDescriptor.InfrastructureMode ==
						Wlan802_11Infrastructure) {
				PRINTK1("New SSID is the same as current, "
					"not attempting to re-associate\n");

				/*
				 * this code will be converted back to 
				 * WLAN_STATUS_SUCCESS by oid processing 
				 * function.
				 */
				return WLAN_STATUS_NOT_ACCEPTED;
			}
		}
	}

	PRINTK1("Authentication Mode = %d\n", 
					Adapter->SecInfo.AuthenticationMode);
		
	/* Set the temporary BSSID Index */
	Adapter->ulAttemptedBSSIDIndex = i;

	memcpy(Adapter->CurBssParams.DataRates, 
			&Adapter->BSSIDList[i].SupportedRates,
				sizeof(Adapter->CurBssParams.DataRates));

	card_rates = SupportedRates;
	card_rates_size = sizeof(SupportedRates);

	if (get_common_rates(Adapter, Adapter->CurBssParams.DataRates, 
				sizeof(Adapter->CurBssParams.DataRates),
					card_rates, card_rates_size)) {
		return -EINVAL;
	}

	for (i=0; i < MIN(sizeof(Adapter->CurBssParams.DataRates),IW_MAX_BITRATES) 
		&& Adapter->CurBssParams.DataRates[i]; i++) {
	}
	Adapter->CurBssParams.NumOfRates = i;

#ifdef WPA
	if ((FW_IS_WPA_ENABLED(Adapter)) && (Adapter->SecInfo.WPAEnabled) ) {
		PWLAN_802_11_ASSOCIATION_INFORMATION pAdapterAssoInfo;

		PRINTK1("Inside the WPA associate command\n");
		/*
		 * reset the cap and interval info according 
		 * to OID_802_11_ASSOCIATION_INFO
		 */
		pAdapterAssoInfo = (PWLAN_802_11_ASSOCIATION_INFORMATION)
						Adapter->AssocInfoBuffer;

		if (pAdapterAssoInfo->AvailableRequestFixedIEs &
				WLAN_802_11_AI_REQFI_LISTENINTERVAL) {
			asst->ListenInterval = 	pAdapterAssoInfo->
						RequestFixedIEs.ListenInterval;
		} else {
			pAdapterAssoInfo->AvailableRequestFixedIEs |=
					WLAN_802_11_AI_REQFI_LISTENINTERVAL;
			pAdapterAssoInfo->RequestFixedIEs.ListenInterval
						= asst->ListenInterval;
		}

		/* ToDo :not sure what to do with CurrentApAddress field */
		pAdapterAssoInfo->AvailableRequestFixedIEs |=
				WLAN_802_11_AI_REQFI_CURRENTAPADDRESS;

		memmove(pAdapterAssoInfo->RequestFixedIEs.CurrentAPAddress,
					asst->DestMacAddr, ETH_ALEN);
		/*
		 * add SSID, supported rates, 
		 * and wpa IE info associate request
		 */
		{
			u8 		tmpBuf[500];
			u32           ulReqIELen = 0;
			u32           ulCurOffset = 0;
			u8           ucElemID;
			u8           ucElemLen;
			unsigned char  	*pIEBuf,
					*pRequestIEBuf;

			memmove(tmpBuf, Adapter->AssocInfoBuffer +
					pAdapterAssoInfo->
					OffsetResponseIEs,
					pAdapterAssoInfo->
					ResponseIELength);

			pIEBuf = Adapter->IEBuffer[Adapter->
				ulAttemptedBSSIDIndex].VariableIE;
			pRequestIEBuf =	Adapter->AssocInfoBuffer +
				pAdapterAssoInfo->OffsetRequestIEs;

			PRINTK1("Variable IE Length is %d\n",
				(u32) (Adapter->BSSIDList
				[Adapter->ulAttemptedBSSIDIndex].
				IELength - MRVL_FIXED_IE_SIZE));
			
			while (ulCurOffset < (Adapter->
						BSSIDList[Adapter->
						ulAttemptedBSSIDIndex].
						IELength - MRVL_FIXED_IE_SIZE)
						&& (Adapter->BSSIDList[Adapter->
						ulAttemptedBSSIDIndex].IELength
						> 12)) {
				ucElemID = *(pIEBuf + ulCurOffset);
				ucElemLen = *(pIEBuf + ulCurOffset + 1);

				// PRINTK1("uc ElemID : %x\n", ucElemID);

				/* wpa, supported rate, or wpa */
				if ((ucElemID == SSID) ||
					(ucElemID == SUPPORTED_RATES)
					|| (ucElemID == WPA_IE)) {
					/*
					 * copy the IE to association 
					 * information buffer
					 */
					memmove(pRequestIEBuf + 
							ulReqIELen,
							pIEBuf + 
							ulCurOffset, 
							ucElemLen + 2);
					ulReqIELen += (ucElemLen + 2);

					PRINTK1("Copied elemID = "
						"0x%x, %d bytes into "
						"request ID \n",
						ucElemID, (u32) ulReqIELen);

					HEXDUMP("RequestIE: ",
							pRequestIEBuf, 32);
				}
			ulCurOffset = ulCurOffset + 2 + ucElemLen;
			}
			pAdapterAssoInfo->RequestIELength = ulReqIELen;
			pAdapterAssoInfo->OffsetResponseIEs =
			pAdapterAssoInfo->OffsetRequestIEs +
				pAdapterAssoInfo->RequestIELength;

			memmove(Adapter->AssocInfoBuffer +
				pAdapterAssoInfo->OffsetResponseIEs, tmpBuf,
				pAdapterAssoInfo->ResponseIELength);
		}
		/* Removed sending RSN_AUTH_SUITES command as
		 * it is already sent before coming here */
	}
#endif	/* WPA */

	/* set failure-time-out */
	asst->TimeOut = wlan_cpu_to_le16(MRVDRV_ASSOCIATION_TIME_OUT);	// 2 sec in TU
	/* set the Capability info */
	memcpy(&TmpCap, &Adapter->capInfo, sizeof(asst->CapInfo));
	
	TmpCap &= CAPINFO_MASK;
	PRINTK("TmpCap=%4X CAPINFO_MASK=%4X\n", TmpCap, CAPINFO_MASK);
	memcpy(&asst->CapInfo, &TmpCap, sizeof(asst->CapInfo));

	// set preamble to firmware
	if (Adapter->capInfo.ShortPreamble 
			&& Adapter->BSSIDList[i].Cap.ShortPreamble) {
		Adapter->Preamble = HostCmd_TYPE_SHORT_PREAMBLE;
	} else {
		Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;
	}
	SetRadioControl(priv);
	
	/*
	 * association was done after scanning, 
	 * possibly trying to connect to
	 * AP that sends out beacon with blank SSID
	 */
	asst->BlankSsid = wlan_cpu_to_le32(1);

	/* set IBSS field */
	if (Adapter->BSSIDList[i].InfrastructureMode == 
						Wlan802_11Infrastructure)
		asst->CapInfo.Ess = 1; /* HostCmd_CAPINFO_ESS */
	else
		asst->CapInfo.Ibss = 1; /* HostCmd_CAPINFO_IBSS */

	/* set Privacy field */
	if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled) {
		PRINTK1("WEPStatus set, setting HostCmd_CAPINFO_PRIVACY bit\n");

		asst->CapInfo.Privacy = 1; /* HostCmd_CAPINFO_PRIVACY */
	}
	/* set the listen interval */
	asst->ListenInterval = wlan_cpu_to_le16(Adapter->ListenInterval);
	Adapter->bIsAssociationInProgress = TRUE;

	/* need to report disconnect event if currently associated */
	if (Adapter->CurBssParams.ssid.SsidLength != 0) {
		MacEventDisconnected(priv);
	}

	PRINTK1("Peer Mac Address = %x:%x:%x:%x:%x:%x\n",
			asst->DestMacAddr[0], asst->DestMacAddr[1],
			asst->DestMacAddr[2], asst->DestMacAddr[3],
			asst->DestMacAddr[4], asst->DestMacAddr[5]);
	PRINTK1("CapInfo: Ess=%d Ibss=%d CfPollable=%d CfPollRqst=%d "
			"Privacy=%d ShortPreamble=%d Pbcc=%d ChanAgility=%d\n",
			asst->CapInfo.Ess, asst->CapInfo.Ibss,
			asst->CapInfo.CfPollable, asst->CapInfo.CfPollRqst,
			asst->CapInfo.Privacy, asst->CapInfo.ShortPreamble,
			asst->CapInfo.Pbcc, asst->CapInfo.ChanAgility);
	PRINTK1("Listen Interval SSID = %d\n", asst->ListenInterval);
	PRINTK1("Blank Sssid = 0x%08x\n", asst->BlankSsid);

	PRINTK1("HWAC - ASSOCIATION command is ready\n");

	LEAVE();
	return ret;
}
#endif	/* OLD ASSOCIATION */


/*
 * New commands - for ioctl's 
 */

static inline int wlan_cmd_802_11_deauthenticate(wlan_private * priv,
			       			HostCmd_DS_COMMAND * cmd)
{
	wlan_adapter   *Adapter = priv->adapter;

	ENTER();

	if (cmd->Command == HostCmd_CMD_802_11_DEAUTHENTICATE) {

		HostCmd_DS_802_11_DEAUTHENTICATE *dauth = &cmd->params.deauth;

		cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_DEAUTHENTICATE);
		cmd->Size = 
			wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_DEAUTHENTICATE) + S_DS_GEN);

		/* set AP MAC address */
		memmove(dauth->MacAddr, Adapter->CurrentBSSID,
							MRVDRV_ETH_ADDR_LEN);
		/* Reason code 3 = Station is leaving */
		dauth->ReasonCode = wlan_cpu_to_le16(3);
	}

	else if (cmd->Command == HostCmd_CMD_802_11_DISASSOCIATE) {
		HostCmd_DS_802_11_DISASSOCIATE *dassociate =
		&cmd->params.dassociate;

		cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_DISASSOCIATE);
		cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_DISASSOCIATE) + S_DS_GEN);
		/* set AP MAC address */
		memmove(dassociate->DestMacAddr, Adapter->CurrentBSSID,
				MRVDRV_ETH_ADDR_LEN);
		/* Reason code 0 = Station is leaving */
		dassociate->ReasonCode = wlan_cpu_to_le16(0);
	}

	// ResetRxPDQ(Adapter); 
	// TODO:Figure out we need to take care of anything

	LEAVE();
	return 0;
}

/* Needs OID,Info buffer and action to be paased */

#define WEP_40_BIT_LEN	5
#define WEP_104_BIT_LEN	13
static inline int wlan_cmd_802_11_set_wep(wlan_private * priv,
			HostCmd_DS_COMMAND * cmd, u32 PendingOID,
			void *InformationBuffer, u16 CmdOption)
{
	HostCmd_DS_802_11_SET_WEP	*wep = &cmd->params.wep;
	wlan_adapter   			*Adapter = priv->adapter;

	ENTER();

	if (PendingOID == OID_802_11_ADD_WEP) {
		cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SET_WEP);
		cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_802_11_SET_WEP)) + S_DS_GEN);
		wep->Action = wlan_cpu_to_le16(HostCmd_ACT_ADD);

		//default tx key index
		wep->KeyIndex = wlan_cpu_to_le16(Adapter->CurrentWepKeyIndex &
			       					HostCmd_WEP_KEY_INDEX_MASK);
	
		PRINTK1("Tx Key Index: %u\n", wep->KeyIndex);

		switch (Adapter->WepKey[0].KeyLength) {
			case WEP_40_BIT_LEN:
				wep->WEPTypeForKey1 = HostCmd_TYPE_WEP_40_BIT;
				memmove(wep->WEP1, Adapter->WepKey[0].KeyMaterial, Adapter->WepKey[0].KeyLength);
				break;
			case WEP_104_BIT_LEN:
				wep->WEPTypeForKey1 = HostCmd_TYPE_WEP_104_BIT;
				memmove(wep->WEP1, Adapter->WepKey[0].KeyMaterial, Adapter->WepKey[0].KeyLength);
				break;
			case 0:
				break;
			default:
				PRINTK("Key1 Length = %d is incorrect\n", Adapter->WepKey[0].KeyLength);
				LEAVE();
				return -1;
		}

		switch (Adapter->WepKey[1].KeyLength) {
			case WEP_40_BIT_LEN:
				wep->WEPTypeForKey2 = HostCmd_TYPE_WEP_40_BIT;
				memmove(wep->WEP2, Adapter->WepKey[1].KeyMaterial, Adapter->WepKey[1].KeyLength);
				break;
			case WEP_104_BIT_LEN:
				wep->WEPTypeForKey2 = HostCmd_TYPE_WEP_104_BIT;
				memmove(wep->WEP2, Adapter->WepKey[1].KeyMaterial, Adapter->WepKey[1].KeyLength);
				break;
			case 0:
				break;
			default:
				PRINTK("Key2 Length = %d is incorrect\n", Adapter->WepKey[1].KeyLength);
				LEAVE();
				return -1;
		}

		switch (Adapter->WepKey[2].KeyLength) {
			case WEP_40_BIT_LEN:
				wep->WEPTypeForKey3 = HostCmd_TYPE_WEP_40_BIT;
				memmove(wep->WEP3, Adapter->WepKey[2].KeyMaterial, Adapter->WepKey[2].KeyLength);
				break;
			case WEP_104_BIT_LEN:
				wep->WEPTypeForKey3 = HostCmd_TYPE_WEP_104_BIT;
				memmove(wep->WEP3, Adapter->WepKey[2].KeyMaterial, Adapter->WepKey[2].KeyLength);
				break;
			case 0:
				break;
			default:
				PRINTK("Key3 Length = %d is incorrect\n", Adapter->WepKey[2].KeyLength);
				LEAVE();
				return -1;
		}

		switch (Adapter->WepKey[3].KeyLength) {
			case WEP_40_BIT_LEN:
				wep->WEPTypeForKey4 = HostCmd_TYPE_WEP_40_BIT;
				memmove(wep->WEP4, Adapter->WepKey[3].KeyMaterial, Adapter->WepKey[3].KeyLength);
				break;
			case WEP_104_BIT_LEN:
				wep->WEPTypeForKey4 = HostCmd_TYPE_WEP_104_BIT;
				memmove(wep->WEP4, Adapter->WepKey[3].KeyMaterial, Adapter->WepKey[3].KeyLength);
				break;
			case 0:
				break;
			default:
				PRINTK("Key4 Length = %d is incorrect\n", Adapter->WepKey[3].KeyLength);
				LEAVE();
				return -1;
		}
	}
	else if (PendingOID == OID_802_11_REMOVE_WEP) {
		cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SET_WEP);
		cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_802_11_SET_WEP)) + S_DS_GEN);
		wep->Action = wlan_cpu_to_le16(HostCmd_ACT_REMOVE);

		//default tx key index
		wep->KeyIndex = wlan_cpu_to_le16((u16) (Adapter->CurrentWepKeyIndex & 
				(u32) HostCmd_WEP_KEY_INDEX_MASK));
	}

	LEAVE();
	return 0;
}

/*
 * Needs a infoBuf to be transferred 
 */
static inline int wlan_cmd_802_11_ad_hoc_start(wlan_private * priv,
			     CmdCtrlNode * cmdnode,
			     void *InformationBuffer)
{
	int				ret = 0;
	wlan_adapter   			*Adapter = priv->adapter;
	HostCmd_DS_COMMAND 		*cmd = 
		(HostCmd_DS_COMMAND *) cmdnode->BufVirtualAddr;
	HostCmd_DS_802_11_AD_HOC_START 	*adhs = &cmd->params.ads;
	int     	        	i, j;
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG 	bc;
#endif /* MULTI BANDS */
	u16				TmpCap;

	ENTER();

	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_AD_HOC_START) + S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_AD_HOC_START);

	if (Adapter->AdHocCreated && Adapter->ulNumOfBSSIDs)
		Adapter->ulNumOfBSSIDs--;

	i = Adapter->ulNumOfBSSIDs;

	/* add a new entry in the BSSID list */
	if (Adapter->ulNumOfBSSIDs < MRVDRV_MAX_BSSID_LIST) {
		Adapter->ulAttemptedBSSIDIndex = i;
		Adapter->ulNumOfBSSIDs++;
	}

	/*
	 * Fill in the parameters for 2 data structures:
	 * 1. HostCmd_DS_802_11_AD_HOC_START Command
	 * 2. Adapter->BSSIDList[i]
	 * Driver will fill up SSID, BSSType,IBSS param, Physical Param,
	 * probe delay, and Cap info.
	 * Firmware will fill up beacon period, DTIM, Basic rates 
	 * and operational rates. 
	 */
	for (j = 0; j < MRVDRV_MAX_SSID_LENGTH; j++) {
		if (((PWLAN_802_11_SSID) InformationBuffer)->Ssid[j] == '\0')
	break;
	}

	memset(adhs->SSID, 0, MRVDRV_MAX_SSID_LENGTH);	// with 2.0.23
							// reference 
	memcpy(adhs->SSID,((PWLAN_802_11_SSID) InformationBuffer)->Ssid,
			((PWLAN_802_11_SSID) InformationBuffer)->SsidLength);

	PRINTK1("Inside the adhoc_start command %s\n", adhs->SSID);

	memset(Adapter->BSSIDList[i].Ssid.Ssid, 0, MRVDRV_MAX_SSID_LENGTH);
	memcpy(Adapter->BSSIDList[i].Ssid.Ssid,
			((PWLAN_802_11_SSID) InformationBuffer)->Ssid,
			((PWLAN_802_11_SSID) InformationBuffer)->SsidLength);

	Adapter->BSSIDList[i].Ssid.SsidLength =
		((PWLAN_802_11_SSID) InformationBuffer)->SsidLength;

	/* Set the length of Adapter->BSSIDList[i] */
	Adapter->BSSIDList[i].Length = sizeof(WLAN_802_11_BSSID);
	/* set the length of configuration in Adapter->BSSIDList[i] */
	Adapter->BSSIDList[i].Configuration.Length =
		sizeof(WLAN_802_11_CONFIGURATION);

	/* set the BSS type */
	adhs->BSSType = HostCmd_BSS_TYPE_IBSS;
	Adapter->BSSIDList[i].InfrastructureMode = Wlan802_11IBSS;

	/* set Physical param set */
	adhs->PhyParamSet.DsParamSet.ElementId = 3;
	adhs->PhyParamSet.DsParamSet.Len = 1;

	if (Adapter->AdhocChannel == 0) {
#ifdef MULTI_BANDS
		if (Adapter->adhoc_start_band == BAND_A) 
			Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL_A;
		else
#endif
			Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL;
	}

	PRINTK1("Creating ADHOC on Channel %d\n", Adapter->AdhocChannel);

	Adapter->CurBssParams.channel = Adapter->AdhocChannel;
#ifdef MULTI_BANDS
	Adapter->CurBssParams.band = Adapter->adhoc_start_band;
#endif

	Adapter->BSSIDList[i].Channel = Adapter->AdhocChannel;
	adhs->PhyParamSet.DsParamSet.CurrentChan = Adapter->AdhocChannel;

	memcpy(&Adapter->BSSIDList[i].PhyParamSet,
			&adhs->PhyParamSet, sizeof(IEEEtypes_PhyParamSet_t));

	Adapter->BSSIDList[i].NetworkTypeInUse = Wlan802_11DS;
	Adapter->BSSIDList[i].Configuration.DSConfig =
	DSFreqList[adhs->PhyParamSet.DsParamSet.CurrentChan];

	/* set IBSS param set */
	adhs->SsParamSet.IbssParamSet.ElementId = 6;
	adhs->SsParamSet.IbssParamSet.Len = 2;
	adhs->SsParamSet.IbssParamSet.AtimWindow = Adapter->AtimWindow;
	memcpy(&Adapter->BSSIDList[i].SsParamSet,
			&(adhs->SsParamSet), sizeof(IEEEtypes_SsParamSet_t));

	/* set Capability info */
	adhs->Cap.Ess = 0;
	adhs->Cap.Ibss = 1;
	Adapter->BSSIDList[i].Cap.Ibss = 1;
#ifdef ENABLE_802_11H_TPC
	if (Adapter->State11HTPC.Enable11HTPC==TRUE )
		adhs->Cap.SpectrumMgmt = 1;
	PRINTK2("11HTPC: Start AD-HOC Cap=%x len=%d\n", *(unsigned int*)&(adhs->Cap), sizeof(adhs->Cap) );
#endif

	/* ProbeDelay */
	adhs->ProbeDelay = wlan_cpu_to_le16(HostCmd_SCAN_PROBE_DELAY_TIME);

	/* set up privacy in Adapter->BSSIDList[i] */
	if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled
#ifdef ADHOCAES
		|| Adapter->AdhocAESEnabled
#endif
		) {
		PRINTK1("WEPStatus set, BSSIDList[i].Privacy to WEP\n");
		Adapter->BSSIDList[i].Privacy = Wlan802_11PrivFilter8021xWEP;
		adhs->Cap.Privacy = 1;
	} else {
		PRINTK1("WEPStatus NOTSET, Setting"
				"BSSIDList[i].Privacy to ACCEPT ALL\n");
		Adapter->BSSIDList[i].Privacy = Wlan802_11PrivFilterAcceptAll;
	}

	memcpy(&TmpCap, &adhs->Cap, sizeof(u16));
	TmpCap = wlan_cpu_to_le16(TmpCap);
	memcpy(&adhs->Cap, &TmpCap, sizeof(u16));
	
	/* Get ready to start an ad hoc network */
	Adapter->bIsAssociationInProgress = TRUE;

	ResetSingleTxDoneAck(priv);	// TODO ?? What to do

	/* need to report disconnect event if currently associated */
	if (Adapter->CurBssParams.ssid.SsidLength != 0) {
		MacEventDisconnected(priv);
	}
		
	memset(adhs->DataRate, 0, sizeof(adhs->DataRate));
#ifdef	MULTI_BANDS
	if (Adapter->adhoc_start_band & BAND_A) 
		memcpy(adhs->DataRate, AdhocRates_A, 
				MIN(sizeof(adhs->DataRate),
				sizeof(AdhocRates_A)));
	else if (Adapter->adhoc_start_band & BAND_B) 
		memcpy(adhs->DataRate, AdhocRates_B, 
				MIN(sizeof(adhs->DataRate),
				sizeof(AdhocRates_B)));
	else if (Adapter->adhoc_start_band & BAND_G) 
		memcpy(adhs->DataRate, AdhocRates_G, 
				MIN(sizeof(adhs->DataRate),
				sizeof(AdhocRates_G)));
#else 
#ifdef ADHOC_GRATE
	if (Adapter->adhoc_grate_enabled == TRUE)
		memcpy(adhs->DataRate, AdhocRates_G, MIN(sizeof(adhs->DataRate),
							sizeof(AdhocRates_G)));
	else
		memcpy(adhs->DataRate, AdhocRates_B, MIN(sizeof(adhs->DataRate),
							sizeof(AdhocRates_B)));
#else
	memcpy(adhs->DataRate, AdhocRates_B, MIN(sizeof(adhs->DataRate),
							sizeof(AdhocRates_B)));
#endif /* ADHOC_GRATE */
#endif

	for (i=0; i < sizeof(adhs->DataRate) && adhs->DataRate[i]; i++) {
	}
	Adapter->CurBssParams.NumOfRates = i;

	/* Copy the ad-hoc creating rates into Current BSS state structure */
	memcpy(&Adapter->CurBssParams.DataRates, &adhs->DataRate, 
		Adapter->CurBssParams.NumOfRates);

/*
	for (i=0; i < Adapter->CurBssParams.NumOfRates; i++) {
		PRINTK("<1>Rates=%02x \n", adhs->DataRate[i]);
	}
*/		
	PRINTK("Rates=%02x %02x %02x %02x \n", 
			adhs->DataRate[0], adhs->DataRate[1],
			adhs->DataRate[2], adhs->DataRate[3]);

	PRINTK1("HWAC - AD HOC Start command is ready\n");

#ifdef MULTI_BANDS
	if (Adapter->is_multiband) {
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
#endif /* MULTI_BANDS */

#ifdef ENABLE_802_11D
	ret = wlan_create_dnld_countryinfo_11d( priv );
	if ( ret ) {
		LEAVE();
		return ret;
	}
#endif

#ifdef ENABLE_802_11H_TPC
	/*Before Start Ad-hoc, set power cap and constraint first*/

	PRINTK2("11HTPC:Enable11H=%s\n", 
		(Adapter->State11HTPC.Enable11HTPC==TRUE)?"TRUE":"FALSE");

	if (Adapter->State11HTPC.Enable11HTPC==TRUE ) {
		Adapter->State11HTPC.InfoTpc.Chan = Adapter->AdhocChannel;
		Adapter->State11HTPC.InfoTpc.PowerConstraint = 
				Adapter->State11HTPC.UsrDefPowerConstraint;
		ret = PrepareAndSendCommand(priv,
			HostCmd_CMD_802_11H_TPC_INFO,
			HostCmd_ACT_SET, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);
		if (ret) {
			PRINTK2("Err: Send TPC_INFO CMD: %d\n", ret);
			LEAVE();
			return ret;
		}
	}
#endif

	return ret;
}

static inline int wlan_cmd_802_11_ad_hoc_stop(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_AD_HOC_STOP);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_AD_HOC_STOP) + S_DS_GEN);

	return 0;
}

static inline int wlan_cmd_802_11_ad_hoc_join(wlan_private * priv,
			    CmdCtrlNode * cmdnode, void *InformationBuffer)
{
	int				ret = 0;

	HostCmd_DS_COMMAND 		*cmd =
		(HostCmd_DS_COMMAND *) cmdnode->BufVirtualAddr;

	HostCmd_DS_802_11_AD_HOC_JOIN 	*pAdHocJoin = &cmd->params.adj;
	wlan_adapter   			*Adapter = priv->adapter;
	int             		i;
	u8				*pReqBSSID = NULL;
	u8				*card_rates;
	int				card_rates_size;
	u16				TmpCap;
#ifdef MULTI_BANDS
	HostCmd_DS_802_11_BAND_CONFIG bc;
#endif /* MULTI BANDS*/
	int				index;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_AD_HOC_JOIN);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_AD_HOC_JOIN) + S_DS_GEN);

	if (Adapter->RequestViaBSSID)
		pReqBSSID = Adapter->RequestedBSSID;
    
	/* find out the BSSID that matches 
	 * the SSID given in InformationBuffer 
	 */
	for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
		/* if found a match on SSID, get the MAC address(BSSID) */
		if (!SSIDcmp(&Adapter->BSSIDList[i].Ssid,
					(PWLAN_802_11_SSID)InformationBuffer)) {
			if (!pReqBSSID || !memcmp(Adapter->BSSIDList[i].
						MacAddress, pReqBSSID, 
						ETH_ALEN)) {
				if (Adapter->BSSIDList[i].InfrastructureMode !=
						Wlan802_11Infrastructure) {
					break;
				}
			}
		}
	}

	/* check if the matching SSID is found, if not, return */
	if (i == Adapter->ulNumOfBSSIDs) {
		return WLAN_STATUS_FAILURE;
	}
	
//	UpdateWEPTable(Adapter, InformationBuffer);

	PRINTK1("***Adapter ssid =%s\n", Adapter->CurBssParams.ssid.Ssid);
	PRINTK1("***Adapter Length =%u\n", 
					Adapter->CurBssParams.ssid.SsidLength);
	PRINTK1("***Info Buffer ssid =%s\n",
			((PWLAN_802_11_SSID) InformationBuffer)->Ssid);
	PRINTK1("**Info buffer Length =%u\n",
			((PWLAN_802_11_SSID) InformationBuffer)->SsidLength);

	/* check if the requested SSID is already joined */
	if (Adapter->CurBssParams.ssid.SsidLength &&
        !SSIDcmp((PWLAN_802_11_SSID)InformationBuffer,
                 &Adapter->CurBssParams.ssid)) 
    {
		if (Adapter->CurrentBSSIDDescriptor.InfrastructureMode ==
            Wlan802_11IBSS) 
        {
			PRINTK1("New ad-hoc SSID is the same as current, "
					"not attempting to re-join");
            
			/*
			 * should return WLAN_STATUS_NOT_ACCEPTED in any case,
			 * not just for WHQL_FIX
			 * return WLAN_STATUS_SUCCESS;
			 * scan expects the result to be true always!!
			 * Changed to fix the issue of zero size command 
			 * being sent to pendingQ. 
			 * It would stuck there if issue the same essid to
			 * driver after it has joined already.  
			 */
			return WLAN_STATUS_NOT_ACCEPTED;
		}
	}
	/* Set the temporary BSSID Index */
	Adapter->ulAttemptedBSSIDIndex = index = i;

	pAdHocJoin->BssDescriptor.BSSType = HostCmd_BSS_TYPE_IBSS;

	pAdHocJoin->BssDescriptor.BeaconPeriod = 
		Adapter->BSSIDList[index].Configuration.BeaconPeriod;

	memcpy(&pAdHocJoin->BssDescriptor.BSSID,
           &Adapter->BSSIDList[index].MacAddress,
           MRVDRV_ETH_ADDR_LEN);

	memcpy(&pAdHocJoin->BssDescriptor.SSID,
           &Adapter->BSSIDList[index].Ssid.Ssid,
           Adapter->BSSIDList[index].Ssid.SsidLength);
    
	memcpy(&pAdHocJoin->BssDescriptor.PhyParamSet, 
           &Adapter->BSSIDList[index].PhyParamSet,
           sizeof(IEEEtypes_PhyParamSet_t));
    
	memcpy(&pAdHocJoin->BssDescriptor.SsParamSet,
           &Adapter->BSSIDList[index].SsParamSet,
					sizeof(IEEEtypes_SsParamSet_t));

	memcpy(&TmpCap, &Adapter->BSSIDList[index].Cap,
					sizeof(IEEEtypes_CapInfo_t));
	TmpCap &= CAPINFO_MASK;
	PRINTK("TmpCap=%4X CAPINFO_MASK=%4X\n", TmpCap, CAPINFO_MASK);
	memcpy(&pAdHocJoin->BssDescriptor.Cap, &TmpCap,
					sizeof(IEEEtypes_CapInfo_t));


	/* information on BSSID descriptor passed to FW */
	PRINTK1("Adhoc Join - BSSID = %2x-%2x-%2x-%2x-%2x-%2x, SSID = %s\n",
			pAdHocJoin->BssDescriptor.BSSID[0],
			pAdHocJoin->BssDescriptor.BSSID[1],
			pAdHocJoin->BssDescriptor.BSSID[2],
			pAdHocJoin->BssDescriptor.BSSID[3],
			pAdHocJoin->BssDescriptor.BSSID[4],
			pAdHocJoin->BssDescriptor.BSSID[5],
			pAdHocJoin->BssDescriptor.SSID);

	PRINTK1("Data Rate = %x\n", (u32) pAdHocJoin->BssDescriptor.DataRates);

	/* FailTimeOut */
	pAdHocJoin->FailTimeOut = wlan_cpu_to_le16(MRVDRV_ASSOCIATION_TIME_OUT);// 2 sec in TU

	/* ProbeDelay */
	pAdHocJoin->ProbeDelay = wlan_cpu_to_le16(HostCmd_SCAN_PROBE_DELAY_TIME);// 0;

	/* Copy Data Rates from the Rates recorded in scan response */
	memset(pAdHocJoin->BssDescriptor.DataRates, 0, 
           sizeof(pAdHocJoin->BssDescriptor.DataRates));
	memcpy(pAdHocJoin->BssDescriptor.DataRates, 
           Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].DataRates, 
           MIN(sizeof(pAdHocJoin->BssDescriptor.DataRates),
               sizeof(Adapter->BSSIDList[Adapter
                                         ->ulAttemptedBSSIDIndex].DataRates)));

#ifdef	MULTI_BANDS
	if (Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band == BAND_A) {
		card_rates = SupportedRates_A;
		card_rates_size = sizeof(SupportedRates_A);
	}
	else if (Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band == BAND_B) {
		card_rates = SupportedRates_B;
		card_rates_size = sizeof(SupportedRates_B);
	}
	else if (Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band == BAND_G) {
		card_rates = SupportedRates_G;
		card_rates_size = sizeof(SupportedRates_G);
	}
	else {
		return -EINVAL;
	}
#else
	card_rates = SupportedRates;
	card_rates_size = sizeof(SupportedRates);
#endif	/* MULTI_BANDS*/

	Adapter->CurBssParams.channel = 
		Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].Channel;
#ifdef	MULTI_BANDS
	Adapter->CurBssParams.band = 
		Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].bss_band;
#endif	/* MULTI_BANDS*/

	if (get_common_rates(Adapter, pAdHocJoin->BssDescriptor.DataRates, 
                         sizeof(pAdHocJoin->BssDescriptor.DataRates),
                         card_rates, card_rates_size)) {
		return -EINVAL;
	}

	for (i=0; i < sizeof(pAdHocJoin->BssDescriptor.DataRates) 
		&& pAdHocJoin->BssDescriptor.DataRates[i]; i++) {
	}
	Adapter->CurBssParams.NumOfRates = i;

	/* 
	** Copy the adhoc joining rates to Current BSS State structure 
	*/
	memcpy(Adapter->CurBssParams.DataRates, 
		pAdHocJoin->BssDescriptor.DataRates,
		Adapter->CurBssParams.NumOfRates);

	/* Get ready to join */
	Adapter->bIsAssociationInProgress = TRUE;

	pAdHocJoin->BssDescriptor.SsParamSet.IbssParamSet.AtimWindow =
	wlan_cpu_to_le16(Adapter->BSSIDList[index].Configuration.ATIMWindow);

	if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled
#ifdef ADHOCAES
		|| Adapter->AdhocAESEnabled
#endif
		) {
		pAdHocJoin->BssDescriptor.Cap.Privacy =  1;
	}

	memcpy(&TmpCap, &pAdHocJoin->BssDescriptor.Cap, sizeof(IEEEtypes_CapInfo_t));

	TmpCap = wlan_cpu_to_le16(TmpCap);

	memcpy(&pAdHocJoin->BssDescriptor.Cap, &TmpCap, sizeof(IEEEtypes_CapInfo_t));

	ResetSingleTxDoneAck(priv);

	/* need to report disconnect event if currently associated */
	if (Adapter->CurBssParams.ssid.SsidLength != 0) {
		MacEventDisconnected(priv);
	}
#ifdef PS_REQUIRED
	/*
	 * Note that firmware does not go back to power save 
	 * after association for ad hoc mode
	 */
	if (Adapter->PSMode == Wlan802_11PowerModeMAX_PSP) {
		/* wake up first */
		WLAN_802_11_POWER_MODE LocalPSMode;

		LocalPSMode = Wlan802_11PowerModeCAM;
		ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_PS_MODE,
				HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT,
				0, HostCmd_PENDING_ON_NONE, &LocalPSMode);

		if (ret) {
			LEAVE();
			return ret;
		}
	}
#endif

#ifdef MULTI_BANDS
	if (Adapter->is_multiband) {
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
#endif /* MULTI_BANDS */

#ifdef ENABLE_802_11D
		ret = wlan_parse_dnld_countryinfo_11d( priv );
		if ( ret ) {
			LEAVE();
			return ret;
		}
#endif

#ifdef ENABLE_802_11H_TPC
	/*Before Join Ad-hoc, set power cap and constraint first*/
		{
			PWLAN_802_11_BSSID bssid = 
				&(Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex]);

			PRINTK2("11HTPC: Enable11H=%s Sensed11H=%s\n", 
					(Adapter->State11HTPC.Enable11HTPC==TRUE)?"TRUE":"FALSE",
					(bssid->Sensed11H)?"TRUE":"FALSE" 
				);

			if (Adapter->State11HTPC.Enable11HTPC==TRUE && bssid->Sensed11H) {
				Adapter->State11HTPC.InfoTpc.Chan = 
						Adapter->CurBssParams.channel;

				Adapter->State11HTPC.InfoTpc.PowerConstraint = 
						bssid->PowerConstraint.LocalPowerConstraint;

				PRINTK2("11HTPC: Join Ad-Hoc: LocalPwr = %d\n", 
						Adapter->State11HTPC.InfoTpc.PowerConstraint 
					);

				wlan_802_11h_tpc_enable( priv, TRUE );

				ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11H_TPC_INFO,
					HostCmd_ACT_SET, HostCmd_OPTION_USE_INT,
					0, HostCmd_PENDING_ON_NONE, NULL);

				if (ret) {
					PRINTK2("Err: Send TPC_INFO CMD: %d\n", ret);
					LEAVE();
					return ret;
				}
			}
			else {
				wlan_802_11h_tpc_enable( priv, FALSE ); /* Disable 11H*/
			}
		}

#endif

	LEAVE();
	return ret;
}

#ifdef WPA
static inline int wlan_cmd_802_11_query_rsn_option(wlan_private * priv,
				 HostCmd_DS_COMMAND * cmd)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_QUERY_RSN_OPTION);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_QUERY_RSN_OPTION) + S_DS_GEN);

	return 0;
}

static inline int wlan_cmd_802_11_enable_rsn(wlan_private * priv,
			   HostCmd_DS_COMMAND * cmd, u16 option)
{
	HostCmd_DS_802_11_ENABLE_RSN *pEnableRSN = &cmd->params.enbrsn;
	wlan_adapter   *Adapter = priv->adapter;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_ENABLE_RSN);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_ENABLE_RSN) + S_DS_GEN);
	pEnableRSN->Action = wlan_cpu_to_le16(option);
	if (Adapter->SecInfo.WPAEnabled
#ifdef WPA2
		|| Adapter->SecInfo.WPA2Enabled
#endif
	) {
		pEnableRSN->Enable = wlan_cpu_to_le16(HostCmd_ENABLE_RSN);
	} else {
		pEnableRSN->Enable = wlan_cpu_to_le16(HostCmd_DISABLE_RSN);
	}

	return 0;
}

#ifndef WPA2
static inline int wlan_cmd_802_11_rsn_auth_suites(wlan_private * priv,
				HostCmd_DS_COMMAND * cmd, u16 option)
{
	HostCmd_DS_802_11_RSN_AUTH_SUITES *pAuth = &cmd->params.rsnauth;
	wlan_adapter   *Adapter = priv->adapter;

	ENTER();

	pAuth->Action = wlan_cpu_to_le16(option);
	/* Added to fix WPA-TKIP */
	memcpy(pAuth->AuthSuites,
			Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].
				wpa_supplicant.Wpa_ie +	20, 4);

	HEXDUMP("WPA_IE", Adapter->BSSIDList[Adapter->
			ulAttemptedBSSIDIndex].wpa_supplicant.Wpa_ie, 24);
    
	if (Adapter->SecInfo.WPAEnabled == TRUE) {
		pAuth->Enabled = HostCmd_ENABLE_RSN_SUITES;
	} else {
		pAuth->Enabled = HostCmd_DISABLE_RSN_SUITES;
	}

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RSN_AUTH_SUITES);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_RSN_AUTH_SUITES) + S_DS_GEN);

	LEAVE();
	return 0;
}
#endif	//WPA2

static inline int wlan_cmd_802_11_pwk_key(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, u16 option, void *InformationBuffer)
{
	HostCmd_DS_802_11_PWK_KEY 	*pCmdPwk = &cmd->params.pwkkey;
	PMRVL_WLAN_WPA_KEY	pKey = (PMRVL_WLAN_WPA_KEY)InformationBuffer;
	wlan_adapter   			*Adapter = priv->adapter;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_PWK_KEY);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_PWK_KEY) + S_DS_GEN);
	pCmdPwk->Action = wlan_cpu_to_le16(option);

	if (option == HostCmd_ACT_GET) {
		LEAVE();
		return 0;
	}

	if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
		memcpy(pCmdPwk->TkipTxMicKey, pKey->MICKey1, 8);
		memcpy(pCmdPwk->TkipRxMicKey, pKey->MICKey2, 8);

		memcpy(pCmdPwk->TkipEncryptKey,
				pKey->EncryptionKey, 16);
		HEXDUMP("PWK", pKey->EncryptionKey,16);
	}
#ifdef ADHOCAES
	else if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
		SetupAdhocEncryptionKey(priv, pCmdPwk, option,
				InformationBuffer);	
	}
#endif
	LEAVE();
	return 0;
}

#ifdef WPA2
static inline int wlan_cmd_802_11_key_material(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, u16 option, WLAN_OID PendingOID, void *InformationBuffer)
{
	HostCmd_DS_802_11_KEY_MATERIAL 	*pKeyMaterial = &cmd->params.keymaterial;
	PWLAN_802_11_KEY		pKey = (PWLAN_802_11_KEY)InformationBuffer;
	u16				KeyParamSet_len;

	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_KEY_MATERIAL);
	pKeyMaterial->Action = wlan_cpu_to_le16(option);

	if( option == HostCmd_ACT_GET ) {
		cmd->Size = wlan_cpu_to_le16(2 + S_DS_GEN);
		LEAVE();
		return 0;
	}

	memset(&pKeyMaterial->KeyParamSet, 0, sizeof(MrvlIEtype_KeyParamSet_t));

	if (pKey->KeyLength == WPA_AES_KEY_LEN) {	//AES
		PRINTK2("WPA_AES\n");
		pKeyMaterial->KeyParamSet.KeyTypeId = wlan_cpu_to_le16(KEY_TYPE_ID_AES);

		if ( PendingOID == (WLAN_OID)KEY_INFO_ENABLED )
			pKeyMaterial->KeyParamSet.KeyInfo = wlan_cpu_to_le16(KEY_INFO_AES_ENABLED);
		else
			pKeyMaterial->KeyParamSet.KeyInfo = !(wlan_cpu_to_le16(KEY_INFO_AES_ENABLED));

		if (pKey->KeyIndex & 0x40000000)	//AES pairwise key: unicast
			pKeyMaterial->KeyParamSet.KeyInfo |= wlan_cpu_to_le16(KEY_INFO_AES_UNICAST);
		else					//AES group key: multicast
			pKeyMaterial->KeyParamSet.KeyInfo |= wlan_cpu_to_le16(KEY_INFO_AES_MCAST);
	}
	else if (pKey->KeyLength == WPA_TKIP_KEY_LEN) {	//TKIP
		PRINTK2("WPA_TKIP\n");
		pKeyMaterial->KeyParamSet.KeyTypeId = wlan_cpu_to_le16(KEY_TYPE_ID_TKIP);
		pKeyMaterial->KeyParamSet.KeyInfo = wlan_cpu_to_le16(KEY_INFO_TKIP_ENABLED);

		if (pKey->KeyIndex & 0x40000000)	//TKIP pairwise key: unicast
			pKeyMaterial->KeyParamSet.KeyInfo |= wlan_cpu_to_le16(KEY_INFO_TKIP_UNICAST);
		else					//TKIP group key: multicast
			pKeyMaterial->KeyParamSet.KeyInfo |= wlan_cpu_to_le16(KEY_INFO_TKIP_MCAST);
	}

	if ((pKey->KeyIndex & 0x40000000) == 0)
		priv->adapter->IsGTK_SET = TRUE + 1;

	if (pKeyMaterial->KeyParamSet.KeyTypeId) {
		pKeyMaterial->KeyParamSet.Type = wlan_cpu_to_le16(TLV_TYPE_KEY_MATERIAL);
		pKeyMaterial->KeyParamSet.KeyLen = wlan_cpu_to_le16(pKey->KeyLength);
		memcpy(pKeyMaterial->KeyParamSet.Key,
			pKey->KeyMaterial, pKey->KeyLength);
		pKeyMaterial->KeyParamSet.Length = wlan_cpu_to_le16(pKey->KeyLength + 6);

		//2 bytes for Type, 2 bytes for Length
		KeyParamSet_len = pKeyMaterial->KeyParamSet.Length + 4;
		//2 bytes for Action
		cmd->Size = wlan_cpu_to_le16(KeyParamSet_len + 2 + S_DS_GEN);
	}

	LEAVE();
	return 0;
}
#endif	//WPA2

#ifndef WPA2
static inline int wlan_cmd_802_11_grp_key(wlan_private * priv,
			HostCmd_DS_COMMAND * cmd, u16 option, void *InformationBuffer)
{
	HostCmd_DS_802_11_GRP_KEY 	*pCmdPwk = &cmd->params.grpkey;
	PMRVL_WLAN_WPA_KEY		pKey = (PMRVL_WLAN_WPA_KEY)InformationBuffer;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_GRP_KEY);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_GRP_KEY) + S_DS_GEN);
	pCmdPwk->Action = wlan_cpu_to_le16(option);
	if (option == HostCmd_ACT_GET) {
		return 0;
	}

	memcpy(pCmdPwk->TkipTxMicKey, pKey->MICKey1, 8);
	memcpy(pCmdPwk->TkipRxMicKey, pKey->MICKey2, 8);
	memcpy(pCmdPwk->TkipEncryptKey, pKey->EncryptionKey, 16);

	return 0;
}
#endif	//WPA2
#endif				/* end of WPA */

static inline int wlan_cmd_802_11_reset(wlan_private * priv,
		      HostCmd_DS_COMMAND * cmd, int option)
{
	HostCmd_DS_802_11_RESET *reset = &cmd->params.reset;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RESET);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_RESET) + S_DS_GEN);
	reset->Action = wlan_cpu_to_le16(option);
	return 0;
}


static inline int wlan_cmd_802_11_query_traffic(wlan_private * priv,
			      HostCmd_DS_COMMAND * cmd)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_QUERY_TRAFFIC);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_QUERY_TRAFFIC) + S_DS_GEN);
	return 0;
}

static inline int wlan_cmd_802_11_query_status(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_QUERY_STATUS);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_QUERY_STATUS) + S_DS_GEN);
	return 0;
}

static inline int wlan_cmd_802_11_get_log(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_GET_LOG);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_GET_LOG) + S_DS_GEN);

	return 0;
}

static inline int wlan_cmd_802_11_authenticate(wlan_private * priv,
			     HostCmd_DS_COMMAND * cmd,
			     int PendingOID, void *InformationBuffer)
{
	HostCmd_DS_802_11_AUTHENTICATE 	*pAuthenticate = &cmd->params.auth;
	wlan_adapter   			*Adapter = priv->adapter;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_AUTHENTICATE);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_AUTHENTICATE) + S_DS_GEN);
	/*
	 * if the authentication type need to be determined at 
	 * initialization time
	 */
	if (PendingOID == HostCmd_PENDING_ON_NONE) {
		pAuthenticate->AuthType = Adapter->SecInfo.AuthenticationMode;

		memmove(pAuthenticate->MacAddr,
				Adapter->CurrentBSSID, MRVDRV_ETH_ADDR_LEN);
		PRINTK("Bssid shared auth: %x:%x:%x:%x:%x:%x\n",
				Adapter->CurrentBSSID[0],
				Adapter->CurrentBSSID[1],
				Adapter->CurrentBSSID[2],
				Adapter->CurrentBSSID[3],
				Adapter->CurrentBSSID[4],
				Adapter->CurrentBSSID[5]);
	} else {
		pAuthenticate->AuthType = (u8)
			(*((PWLAN_802_11_AUTHENTICATION_MODE) 
			   InformationBuffer));
		/*
		 * if the authentication type is not Open(0) or 
		 * Shared(1), set to default
		 */
		if (pAuthenticate->AuthType != 0 && 
				pAuthenticate->AuthType != 1)
			pAuthenticate->AuthType = 1;
			/* set AP MAC address */
			memmove(pAuthenticate->MacAddr,
					Adapter->CurrentBSSID, 
					MRVDRV_ETH_ADDR_LEN);
	}

	return 0;
}

static inline int wlan_cmd_802_11_get_stat(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_GET_STAT);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_GET_STAT) + S_DS_GEN);

	return 0;
}

static inline int wlan_cmd_802_3_get_stat(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd)
{
	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_3_GET_STAT);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_3_GET_STAT) + S_DS_GEN);

	LEAVE();
	return 0;
}

/* Needs PendOID And Infobuf */
static inline int wlan_cmd_802_11_snmp_mib(wlan_private * priv,
			 HostCmd_DS_COMMAND * cmd, int PendingInfo,
			 int PendingOID, void *InformationBuffer)
{
	HostCmd_DS_802_11_SNMP_MIB 	*pSNMPMIB = &cmd->params.smib;
	wlan_adapter   			*Adapter = priv->adapter;
	u8              		ucTemp;

	ENTER();

	PRINTK1("PendingOID = 0x%x\n", PendingOID);

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SNMP_MIB);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_SNMP_MIB) + S_DS_GEN);

	switch (PendingOID) {
	case OID_802_11_INFRASTRUCTURE_MODE:
		{
		pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
		// InfrastructureMode Index = 0
		pSNMPMIB->OID = wlan_cpu_to_le16((u16) DesiredBssType_i);
		pSNMPMIB->BufSize = sizeof(u8);
		if (Adapter->InfrastructureMode == Wlan802_11Infrastructure)
			ucTemp = SNMP_MIB_VALUE_INFRA;	/* Infrastructure mode */
		else
			ucTemp = SNMP_MIB_VALUE_ADHOC;	/* Ad hoc mode */


		memmove(pSNMPMIB->Value, &ucTemp, sizeof(u8));

		break;
		}
#ifdef ENABLE_802_11D
	case OID_802_11D_ENABLE:
		{
		u32 		ulTemp;

		pSNMPMIB->OID = wlan_cpu_to_le16((u16) Dot11D_i);

		if (PendingInfo == HostCmd_PENDING_ON_SET_OID) {
			pSNMPMIB->QueryType = HostCmd_ACT_GEN_SET;
			pSNMPMIB->BufSize = sizeof(u16);
			ulTemp = *(u32*) InformationBuffer;
			*((PUSHORT) (pSNMPMIB->Value)) = 
						wlan_cpu_to_le16((u16) ulTemp);
		}
		break;
		}
#endif

#ifdef ENABLE_802_11H_TPC
	case OID_802_11H_TPC_ENABLE:
		{
		u32 		ulTemp;

		pSNMPMIB->OID = wlan_cpu_to_le16((u16) Dot11HTPC_i);

		if (PendingInfo == HostCmd_PENDING_ON_SET_OID) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pSNMPMIB->BufSize = wlan_cpu_to_le16(sizeof(u16));
			ulTemp = *(u32*) InformationBuffer;
			*((PUSHORT) (pSNMPMIB->Value)) = 
						wlan_cpu_to_le16((u16) ulTemp);
		}
		break;
		}
#endif

	case OID_802_11_FRAGMENTATION_THRESHOLD:
		{
		WLAN_802_11_FRAGMENTATION_THRESHOLD ulTemp;
		u16          usTemp;

		pSNMPMIB->OID = wlan_cpu_to_le16((u16) FragThresh_i);

		if (PendingInfo == HostCmd_PENDING_ON_GET_OID) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
		} else if (PendingInfo == HostCmd_PENDING_ON_SET_OID) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pSNMPMIB->BufSize = wlan_cpu_to_le16(sizeof(u16));
			ulTemp = *((WLAN_802_11_FRAGMENTATION_THRESHOLD *)
							InformationBuffer);
			*((PUSHORT) (pSNMPMIB->Value)) = 
						wlan_cpu_to_le16((u16) ulTemp);
			
		} else if (PendingInfo == HostCmd_PENDING_ON_NONE) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pSNMPMIB->BufSize = wlan_cpu_to_le16(sizeof(u16));
			usTemp = 
				wlan_cpu_to_le16((u16)Adapter->FragThsd);
			memmove(pSNMPMIB->Value, &usTemp, sizeof(u16));
		}

		break;
	}

	case OID_802_11_RTS_THRESHOLD:
		{
		u16          		usTemp;
		WLAN_802_11_RTS_THRESHOLD 	ulTemp;
		pSNMPMIB->OID = wlan_le16_to_cpu((u16)RtsThresh_i);

		if (PendingInfo == HostCmd_PENDING_ON_GET_OID) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
		} else if (PendingInfo == HostCmd_PENDING_ON_SET_OID) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pSNMPMIB->BufSize = wlan_cpu_to_le16(sizeof(u16));
			ulTemp =
				*((WLAN_802_11_RTS_THRESHOLD *) 
						InformationBuffer);
			*(PUSHORT) (pSNMPMIB->Value) = 
						wlan_cpu_to_le16((u16) ulTemp);
			
		} else if (PendingInfo == HostCmd_PENDING_ON_NONE) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pSNMPMIB->BufSize = wlan_cpu_to_le16(sizeof(u16));
			usTemp = 
				wlan_cpu_to_le16((u16) Adapter->RTSThsd);
			memmove(pSNMPMIB->Value, &usTemp, sizeof(u16));
		}
		break;

	case OID_802_11_TX_RETRYCOUNT:
		pSNMPMIB->OID = wlan_cpu_to_le16((u16) ShortRetryLim_i);

		if (PendingInfo == HostCmd_PENDING_ON_GET_OID) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
		} else if (PendingInfo == HostCmd_PENDING_ON_SET_OID ||
			PendingInfo == HostCmd_PENDING_ON_NONE) {
			pSNMPMIB->QueryType = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pSNMPMIB->BufSize = wlan_cpu_to_le16(sizeof(u16));
			*((PUSHORT) (pSNMPMIB->Value)) = 
				wlan_cpu_to_le16((u16)Adapter->TxRetryCount);
		}

		break;
		}

	default:
		break;
	}

	PRINTK1("Command=0x%x, Size=0x%x, SeqNum=0x%x, Result=0x%x\n",
			cmd->Command, cmd->Size, cmd->SeqNum, cmd->Result);
	PRINTK1("Action=0x%x, OID=0x%x, OIDSize=0x%x, Value=0x%x\n",
			pSNMPMIB->QueryType, pSNMPMIB->OID, pSNMPMIB->BufSize, 
			*(u16 *) pSNMPMIB->Value);

	LEAVE();
	return 0;
}

static inline int wlan_cmd_802_11_radio_control(wlan_private * priv,
			      HostCmd_DS_COMMAND * cmd, int CmdOption)
{
	wlan_adapter   			*Adapter = priv->adapter;
	HostCmd_DS_802_11_RADIO_CONTROL *pRadioControl = &cmd->params.radio;

	ENTER();

	cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_802_11_RADIO_CONTROL)) + S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RADIO_CONTROL);

	pRadioControl->Action = wlan_cpu_to_le16(CmdOption);

	switch (Adapter->Preamble) {
	case HostCmd_TYPE_SHORT_PREAMBLE:
		pRadioControl->Control = wlan_cpu_to_le16(SET_SHORT_PREAMBLE);
		break;

	case HostCmd_TYPE_LONG_PREAMBLE:
		pRadioControl->Control = wlan_cpu_to_le16(SET_LONG_PREAMBLE);
		break;

	default:
	case HostCmd_TYPE_AUTO_PREAMBLE:
		pRadioControl->Control = wlan_cpu_to_le16(SET_AUTO_PREAMBLE);
		break;
	}

	if (Adapter->RadioOn)
		pRadioControl->Control |= wlan_cpu_to_le16(TURN_ON_RF);
	else
		pRadioControl->Control &= wlan_cpu_to_le16(~TURN_ON_RF);

	LEAVE();
	return 0;
}

#ifdef BCA
static inline int wlan_cmd_bca_config(wlan_private * priv,
			HostCmd_DS_COMMAND * cmd, int CmdOption)
{
	wlan_adapter   *Adapter = priv->adapter;
	HostCmd_DS_BCA_CONFIG *pBca = &cmd->params.bca_config;

	ENTER();

	cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_BCA_CONFIG))+S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_BCA_CONFIG);

	if (CmdOption == HostCmd_ACT_GEN_GET) {
		memset(&Adapter->bca, 0, sizeof(HostCmd_DS_BCA_CONFIG));
		memset(pBca, 0, sizeof(HostCmd_DS_BCA_CONFIG));
		pBca->Action = wlan_cpu_to_le16(CmdOption);
	} else if (CmdOption == HostCmd_ACT_GEN_SET) {
		pBca->Action = wlan_cpu_to_le16(CmdOption);
		pBca->Mode = wlan_cpu_to_le16(Adapter->bca.Mode);
		pBca->Antenna = wlan_cpu_to_le16(Adapter->bca.Antenna);
		pBca->BtFreq = wlan_cpu_to_le16(Adapter->bca.BtFreq);
		pBca->TxPriorityLow32 = 
				wlan_cpu_to_le32(Adapter->bca.TxPriorityLow32);
		pBca->TxPriorityHigh32 = 
				wlan_cpu_to_le32(Adapter->bca.TxPriorityHigh32);
		pBca->RxPriorityLow32 = 
				wlan_cpu_to_le32(Adapter->bca.RxPriorityLow32);
		pBca->RxPriorityHigh32 = 
				wlan_cpu_to_le32(Adapter->bca.RxPriorityHigh32);
	}

	LEAVE();
	return 0;
}

static inline int wlan_cmd_802_11_bca_timeshare(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd, u16 CmdOption, 
		HostCmd_DS_802_11_BCA_TIMESHARE *user_bca_ts)
{
	wlan_adapter 			*Adapter = priv->adapter;
	HostCmd_DS_802_11_BCA_TIMESHARE *bca_ts = &cmd->params.bca_timeshare;

	ENTER();

	cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_802_11_BCA_TIMESHARE)) +
								S_DS_GEN);
	cmd->Command = 
		wlan_cpu_to_le16(HostCmd_CMD_802_11_BCA_CONFIG_TIMESHARE);

	if (CmdOption == HostCmd_ACT_GEN_GET) {
		memset(&Adapter->bca_ts, 0, sizeof(bca_ts));
		memset(bca_ts, 0, sizeof(HostCmd_DS_802_11_BCA_TIMESHARE));
		bca_ts->Action = wlan_cpu_to_le16(CmdOption);
	} else if (CmdOption == HostCmd_ACT_GEN_SET) {
		bca_ts->Action = wlan_cpu_to_le16(CmdOption);
		bca_ts->TrafficType = 
				wlan_cpu_to_le16(user_bca_ts->TrafficType);
		bca_ts->TimeShareInterval = 
			wlan_cpu_to_le32(user_bca_ts->TimeShareInterval);
		bca_ts->BTTime = wlan_cpu_to_le32(user_bca_ts->BTTime);
	} 

	LEAVE();
	return 0;
}
#endif

static inline int wlan_cmd_802_11_rf_tx_power(wlan_private * priv,
			    HostCmd_DS_COMMAND * cmd, u16 CmdOption,
			    void *infobuf)
{
	
	HostCmd_DS_802_11_RF_TX_POWER *pRTP = &cmd->params.txp;
	
	ENTER();
	
	cmd->Size = wlan_cpu_to_le16((sizeof(HostCmd_DS_802_11_RF_TX_POWER)) + S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RF_TX_POWER);
	pRTP->Action = CmdOption;
	
	PRINTK1("Size:%d Cmd:0x%x Act:%d\n", cmd->Size, cmd->Command,
			pRTP->Action);

	switch (CmdOption) {
		case HostCmd_ACT_TX_POWER_OPT_GET:
			pRTP->Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
			pRTP->CurrentLevel = 0;
			break;

		case HostCmd_ACT_TX_POWER_OPT_SET_HIGH:
			pRTP->Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pRTP->CurrentLevel =
			   	 wlan_cpu_to_le16(HostCmd_ACT_TX_POWER_INDEX_HIGH);
			break;

		case HostCmd_ACT_TX_POWER_OPT_SET_MID:
			pRTP->Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pRTP->CurrentLevel =
			    	wlan_cpu_to_le16(HostCmd_ACT_TX_POWER_INDEX_MID);
			break;

		case HostCmd_ACT_TX_POWER_OPT_SET_LOW:
			pRTP->Action = wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pRTP->CurrentLevel = wlan_cpu_to_le16(*((u16 *)infobuf));
			break;
	}
	LEAVE();
	return 0;
}

static inline int wlan_cmd_802_11_rf_antenna(wlan_private * priv,
			   HostCmd_DS_COMMAND * cmd, u16 CmdOption,
			   void *InformationBuffer)
{
	HostCmd_DS_802_11_RF_ANTENNA *rant = &cmd->params.rant;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RF_ANTENNA);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_RF_ANTENNA) + S_DS_GEN);

	rant->Action = wlan_cpu_to_le16(CmdOption);
	if ((CmdOption == HostCmd_ACT_SET_RX) || 
			(CmdOption == HostCmd_ACT_SET_TX))
		rant->AntennaMode =
		wlan_cpu_to_le16((u16) (*(WLAN_802_11_ANTENNA *) InformationBuffer));

	return 0;
}

static int wlan_cmd_802_11_rate_adapt_rateset(wlan_private *priv, 
		HostCmd_DS_COMMAND *cmd, u16 cmdoption, void *InfoBuf)
{
	HostCmd_DS_802_11_RATE_ADAPT_RATESET	*rateadapt = 
							&cmd->params.rateset;
	wlan_adapter				*Adapter = priv->adapter;

	cmd->Size = 
	wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_RATE_ADAPT_RATESET) + 
								S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RATE_ADAPT_RATESET);

	ENTER();
	
	rateadapt->Action = cmdoption;
	rateadapt->EnableHwAuto = Adapter->EnableHwAuto;
	rateadapt->Bitmap = Adapter->RateBitmap;

	LEAVE();
	return 0;
}

/* Needs PendingOID and InfoBuffer */
static inline int wlan_cmd_802_11_data_rate(wlan_private * priv,
			  HostCmd_DS_COMMAND * cmd, u16 cmdoption,
			  void *InformationBuffer)
{
	HostCmd_DS_802_11_DATA_RATE *pDataRate = &cmd->params.drate;
	wlan_adapter   *Adapter = priv->adapter;
	u16		Action	= cmdoption;

	ENTER();

	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_DATA_RATE) + S_DS_GEN);

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_DATA_RATE);

	memset(pDataRate, 0, sizeof(HostCmd_DS_802_11_DATA_RATE));

	pDataRate->Action = wlan_cpu_to_le16(cmdoption);

	if (Action == HostCmd_ACT_SET_TX_FIX_RATE) {
		pDataRate->DataRate[0] = data_rate_to_index(Adapter->DataRate);
		PRINTK("Setting FW for fixed rate 0x%02X\n", Adapter->DataRate);
	}
	else if(Action == HostCmd_ACT_SET_TX_AUTO) {
		PRINTK("Setting FW for AUTO rate\n");
	}

	LEAVE();
	return 0;
}

static inline int wlan_cmd_mac_multicast_adr(wlan_private * priv,
			   HostCmd_DS_COMMAND * cmd, u16 CmdOption)
{
	HostCmd_DS_MAC_MULTICAST_ADR *pMCastAdr = &cmd->params.madr;
	wlan_adapter   *Adapter = priv->adapter;

	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_MAC_MULTICAST_ADR) + S_DS_GEN);
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_MAC_MULTICAST_ADR);

	pMCastAdr->Action = wlan_cpu_to_le16(CmdOption);
	pMCastAdr->NumOfAdrs = wlan_cpu_to_le16((u16) Adapter->NumOfMulticastMACAddr);
	memcpy(pMCastAdr->MACList, Adapter->MulticastList,
			Adapter->NumOfMulticastMACAddr * ETH_ALEN);

	return 0;
}

static inline int wlan_cmd_802_11_rf_channel(wlan_private * priv,
			   HostCmd_DS_COMMAND * cmd, int option,
			   void *InfoBuf)
{
	HostCmd_DS_802_11_RF_CHANNEL *rfchan = &cmd->params.rfchannel;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RF_CHANNEL);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_RF_CHANNEL) + S_DS_GEN);

	if (option == RF_GET) {
		rfchan->Action = wlan_cpu_to_le16(option);
	} else if (option == RF_SET) {
		rfchan->Action = wlan_cpu_to_le16(option);
		rfchan->CurrentChannel = wlan_cpu_to_le16(*((u16 *)InfoBuf));
	}

	return 0;
}

static inline int wlan_cmd_802_11_rssi(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd)
{
	wlan_adapter   *Adapter = priv->adapter;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_RSSI);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_RSSI) + S_DS_GEN);
	cmd->params.rssi.N = priv->adapter->bcn_avg_factor;

	/* reset Beacon SNR/NF/RSSI values */
	Adapter->SNR[TYPE_BEACON][TYPE_NOAVG] = 0;
	Adapter->SNR[TYPE_BEACON][TYPE_AVG] = 0;
	Adapter->NF[TYPE_BEACON][TYPE_NOAVG] = 0;
	Adapter->NF[TYPE_BEACON][TYPE_AVG] = 0;
	Adapter->RSSI[TYPE_BEACON][TYPE_NOAVG] = 0;
	Adapter->RSSI[TYPE_BEACON][TYPE_AVG] = 0;

	return 0;
}

#ifdef DTIM_PERIOD

static inline int wlan_cmd_set_dtim_multiple(wlan_private * priv,
			   HostCmd_DS_COMMAND * cmd, u16 * multiple)
{
	HostCmd_DS_802_11_SET_DTIM_MULTIPLE *pDTIM = &cmd->params.mdtim;

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_SET_DTIM_MULTIPLE);
	cmd->Size = 
	wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_SET_DTIM_MULTIPLE) + S_DS_GEN);

	pDTIM->DTIM_time_multiple = wlan_cpu_to_le16(*multiple);

	PRINTK1("DTIM Multiple :%d\n", pDTIM->DTIM_time_multiple);

	return 0;
}

#endif				/* DTIM_PERIOD */

static inline int wlan_cmd_reg_access(wlan_private * priv,
		    HostCmd_DS_COMMAND * CmdPtr, u8 CmdOption,
		    void *InfoBuf)
{
	wlan_offset_value *offval;

	ENTER();

	offval = (wlan_offset_value *) InfoBuf;

	switch (CmdPtr->Command) {
	case HostCmd_CMD_MAC_REG_ACCESS:	/* 0x0019 */
		{
		HostCmd_DS_MAC_REG_ACCESS *macreg;

		CmdPtr->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_MAC_REG_ACCESS) + S_DS_GEN);
		macreg = (PHostCmd_DS_MAC_REG_ACCESS) & CmdPtr->params.macreg;

		macreg->Action = wlan_cpu_to_le16(CmdOption);	/* rd / wr */
		macreg->Offset = wlan_cpu_to_le16((u16) offval->offset);
		macreg->Value = wlan_cpu_to_le32(offval->value);

		break;
		}

	case HostCmd_CMD_BBP_REG_ACCESS:	/* 0x001a */
		{
		HostCmd_DS_BBP_REG_ACCESS *bbpreg;

		CmdPtr->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_BBP_REG_ACCESS) + S_DS_GEN);
		bbpreg = (PHostCmd_DS_BBP_REG_ACCESS) & CmdPtr->params.bbpreg;

		bbpreg->Action = wlan_cpu_to_le16(CmdOption);
		bbpreg->Offset = wlan_cpu_to_le16((u16) offval->offset);
		bbpreg->Value = (u8) offval->value;

		break;
		}

	case HostCmd_CMD_RF_REG_ACCESS:	/* 0x001b */
		{
		HostCmd_DS_RF_REG_ACCESS *rfreg;

		CmdPtr->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_RF_REG_ACCESS) + S_DS_GEN);
		rfreg = (PHostCmd_DS_RF_REG_ACCESS) & CmdPtr->params.rfreg;

		rfreg->Action = wlan_cpu_to_le16(CmdOption);
		rfreg->Offset = wlan_cpu_to_le16((u16) offval->offset);
		rfreg->Value = (u8) offval->value;

		break;
		}

	default:
		return -1;
	}

	LEAVE();
	return 0;
}

static inline int wlan_cmd_802_11_beacon_stop(wlan_private * priv,
		HostCmd_DS_COMMAND * cmd)
{
	ENTER();

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_BEACON_STOP);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_BEACON_STOP) + S_DS_GEN);
	cmd->Result = 0;

	LEAVE();
	return 0;
}

#ifdef PASSTHROUGH_MODE
static inline int
wlan_cmd_passthrough(wlan_private * priv,
		     HostCmd_DS_COMMAND * cmd, int CmdOption,
		     wlan_passthrough_config * pt_config)
{
	PHostCmd_DS_802_11_PASSTHROUGH pPass = &cmd->params.passthrough;

	ENTER();

	PRINTK1("CMD Passthrough Mode\n");

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_PASSTHROUGH);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_PASSTHROUGH) + S_DS_GEN);
	cmd->Result = 0;

	/*
	 * start filling the command buffer 
	 */
	memmove(pPass, pt_config, sizeof(pt_config));
	PRINTK1("Action=0x%04x, Enable=0x%02x, Channel=0x%02x\n",
			pPass->Action, pPass->Enable, pPass->Channel);
	PRINTK1("Filter1=0x%08x, Filter2=0x%08x\n", pPass->Filter1,
			pPass->Filter2);
	PRINTK1("MatchBssid=%02x:%02x:%02x:%02x:%02x:%02x",
			pPass->MatchBssid[0], pPass->MatchBssid[1],
			pPass->MatchBssid[2], pPass->MatchBssid[3],
			pPass->MatchBssid[4], pPass->MatchBssid[5]);
	if (pPass->MatchSsid[0]) {
		PRINTK1("MatchSsid=%s\n", pPass->MatchSsid);
	} else {
		PRINTK1("MatchSsid is Blank\n");
	}

	LEAVE();
	return 0;
}
#endif

static inline int
wlan_cmd_802_11_tx_mode(wlan_private * priv, HostCmd_DS_COMMAND * cmd, 
		PHostCmd_DS_802_11_RFI rfi)
{
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_TX_MODE);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_802_11_RFI) + S_DS_GEN);
	cmd->params.rfi.Mode	= wlan_cpu_to_le16(rfi->Mode);
	return 0;
}

static inline int wlan_cmd_802_11_tx_control_mode(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd, PHostCmd_DS_802_11_RFI rfi)
{
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_TX_CONTROL_MODE);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_802_11_RFI) + S_DS_GEN);
	cmd->params.rfi.Mode	= wlan_cpu_to_le16(rfi->Mode);
	return 0;
}

#ifdef PS_REQUIRED
static inline int wlan_cmd_802_11_set_pre_tbtt(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 option, void *InfoBuf)
{
	PHostCmd_DS_802_11_PRE_TBTT	pPreTbtt = 
		(PHostCmd_DS_802_11_PRE_TBTT)&cmd->params.pretbtt; 

	ENTER();
	
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_PRE_TBTT);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_PRE_TBTT) + S_DS_GEN);
	cmd->Result = 0;

	cmd->params.pretbtt.Action = wlan_cpu_to_le16(option);
	if (option == HostCmd_ACT_SET) 
		pPreTbtt->Value = wlan_cpu_to_le32(*((int *)InfoBuf));

	LEAVE();
	return 0;
}
#endif

static inline int wlan_cmd_802_11_mac_address(wlan_private * priv, 
		HostCmd_DS_COMMAND * cmd, 
		PHostCmd_DS_802_11_MAC_ADDRESS macAdd)
{
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_MAC_ADDRESS);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_MAC_ADDRESS) +
				S_DS_GEN);
	cmd->Result 	= 0;
	return 0;
}

#ifdef MULTI_BANDS
static inline int wlan_cmd_802_11_band_config(wlan_private *priv,
				HostCmd_DS_COMMAND *cmd, u16  cmd_option,
		    		void *InfoBuf)
{
	HostCmd_DS_802_11_BAND_CONFIG *bc =
		(pHostCmd_DS_802_11_BAND_CONFIG)InfoBuf;

	ENTER();
	
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_BAND_CONFIG);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_BAND_CONFIG) + S_DS_GEN);
	cmd->Result 	= 0;

	cmd->params.band.Action		= wlan_cpu_to_le16(cmd_option);
	cmd->params.band.Channel	= wlan_cpu_to_le16(bc->Channel);

	/* We cannot assign 'bc->BandSelection' directly to 
	 * 'cmd->params.band.BandSelection', since the value present in 
	 * 'bc->BandSelection' is BAND_A(0x04) or BAND_G(0x02) or BAND_B(0x01).
	 * But the value that has to be filled in 'cmd->params.band.BandSelection'
	 * is 0/1/2.
	 */
	
	if (bc->BandSelection == BAND_A)
		cmd->params.band.BandSelection = wlan_cpu_to_le16(BS_802_11A);
	else if (bc->BandSelection == BAND_G)
		cmd->params.band.BandSelection = wlan_cpu_to_le16(BS_802_11G);
	else 
		cmd->params.band.BandSelection = wlan_cpu_to_le16(BS_802_11B);
	
	HEXDUMP("802_11a Cmd", (char *) cmd, cmd->Size);

	LEAVE();
	return 0;
}

#endif

#ifdef CAL_DATA
static inline int wlan_cmd_802_11_cal_data(wlan_private *priv, 
		HostCmd_DS_COMMAND *cmd, u16 cmd_option, void *CalData)
{
	HostCmd_DS_COMMAND *pDat = (HostCmd_DS_COMMAND *) CalData;
	pHostCmd_DS_802_11_CAL_DATA pCalData = 
		(pHostCmd_DS_802_11_CAL_DATA) &pDat->params.caldata;
	pHostCmd_DS_802_11_CAL_DATA pCmdCalData = 
		(pHostCmd_DS_802_11_CAL_DATA) &cmd->params.caldata;
	
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_CAL_DATA);	
	cmd->Size 	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_CAL_DATA)+ S_DS_GEN);
	cmd->Result 	= 0;
	
	switch (cmd_option) {
		case HostCmd_ACT_GEN_SET:
		
			pCmdCalData->Action	= wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pCmdCalData->PAOption	= pCalData->PAOption;
			pCmdCalData->ExtPA	= pCalData->ExtPA;
			pCmdCalData->Ant	= pCalData->Ant;
			pCmdCalData->Domain	= wlan_cpu_to_le16(pCalData->Domain);
			pCmdCalData->ECO	= pCalData->ECO;
			pCmdCalData->LCT_cal	= pCalData->LCT_cal;
		
			/*TODO Check Big endian conversion for IntPA it is 
			 * array of u16 
			 */	
			memcpy(&pCmdCalData->IntPA, &pCalData->IntPA,
					sizeof(pCmdCalData->IntPA));
			memcpy(&pCmdCalData->PAConfig, 
					&pCalData->PAConfig,
					sizeof(pCmdCalData->PAConfig));
		
			break;

		case HostCmd_ACT_GEN_GET:			
			
			pCmdCalData->Action	= wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
			pCmdCalData->PAOption	= pCalData->PAOption;
			pCmdCalData->ExtPA	= pCalData->ExtPA;
			pCmdCalData->Ant	= pCalData->Ant;
			pCmdCalData->Domain	= wlan_cpu_to_le16(pCalData->Domain);
			pCmdCalData->ECO	= pCalData->ECO;
			pCmdCalData->LCT_cal	= pCalData->LCT_cal;
			
			/*TODO Check Big endian conversion for IntPA it is 
			 * array of u16 
			 */	
			memcpy(&pCmdCalData->IntPA, &pCalData->IntPA,
					sizeof(pCmdCalData->IntPA));
			memcpy(&pCmdCalData->PAConfig,
					&pCalData->PAConfig,
					sizeof(pCmdCalData->PAConfig));

			break;
	}
	return 0;
}
#endif /* End of CAL_DATA */

#ifdef CAL_DATA
#if 0
static inline int wlan_cmd_802_11_cal_data_ext(wlan_private *priv, 
		HostCmd_DS_COMMAND *cmd, u16 cmd_option, void *CalData)
{
	ENTER(); 
	HostCmd_DS_COMMAND *pDat = (HostCmd_DS_COMMAND *)(CalData + 2); // adjust for Type
	pHostCmd_DS_802_11_CAL_DATA_EXT pCalData = 
		(pHostCmd_DS_802_11_CAL_DATA_EXT) &pDat->params.caldataext;

	pHostCmd_DS_802_11_CAL_DATA_EXT pCmdCalData = 
		(pHostCmd_DS_802_11_CAL_DATA_EXT) &cmd->params.caldataext;
	
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_CAL_DATA_EXT);	
	PRINTK1("pCalData->Revision = 0x%04x\n", pCalData->Revision);
	if (pCalData->Revision == 0x0005) 
		cmd->Size 	= wlan_cpu_to_le16(64+ 6 + S_DS_GEN);//TO DO
	else if (pCalData->Revision == 0x000A || pCalData->Revision == 0x000a) {
		cmd->Size 	= wlan_cpu_to_le16(248+ 6 + S_DS_GEN);//TO DO
		PRINTK1("cmd->Size = %d(d)\n", cmd->Size);
	}
	else if (pCalData->Revision == 0x0007)
		cmd->Size 	= wlan_cpu_to_le16(256+ 6 + S_DS_GEN);//TO DO
	
	cmd->Result 	= 0;
	PRINTK1("cmd->Size = %d(d)\n",cmd->Size); 
	
	switch (cmd_option) {
		case HostCmd_ACT_GEN_SET:
		
			pCmdCalData->Action	= wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			pCmdCalData->Revision	= pCalData->Revision;
			pCmdCalData->CalDataLen	= pCalData->CalDataLen;
			PRINTK1("Action=0x%x, CalDataLen=0x%x\n", 
				pCmdCalData->Action,pCmdCalData->CalDataLen);
			/*TODO Check Big endian conversion for IntPA it is 
			 * array of u16 
			 */	
			memcpy(&pCmdCalData->CalData, &pCalData->CalData,
				pCmdCalData->CalDataLen);
		
			break;

		case HostCmd_ACT_GEN_GET:			
			
			pCmdCalData->Action	= wlan_cpu_to_le16(HostCmd_ACT_GEN_GET);
			pCmdCalData->Revision	= pCalData->Revision;
			pCmdCalData->CalDataLen	= pCalData->CalDataLen;
			
			/*TODO Check Big endian conversion for IntPA it is 
			 * array of u16 
			 */	
			memcpy(&pCmdCalData->CalData, &pCalData->CalData,
				pCmdCalData->CalDataLen);

			break;
	}
	LEAVE(); 
	return 0;
}
#endif
static inline int wlan_cmd_802_11_cal_data_ext(wlan_private *priv, 
		HostCmd_DS_COMMAND *cmd, u16 cmd_option, void *CalDataext)
{
	HostCmd_DS_802_11_CAL_DATA_EXT  *PCalDataext = CalDataext; 
	ENTER(); 

	pHostCmd_DS_802_11_CAL_DATA_EXT pCmdCalData = 
		(pHostCmd_DS_802_11_CAL_DATA_EXT) &cmd->params.caldataext;

	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_CAL_DATA_EXT);
	
	PRINTK1("CalDataLen = %d(d)\n",PCalDataext->CalDataLen);
	if(PCalDataext->CalDataLen > 1024) {
		printk("Error, Cal data lenght too large!\n");
		return -1;
	}
	
	memcpy(pCmdCalData, CalDataext, PCalDataext->CalDataLen + 6 + S_DS_GEN);
	
	cmd->Size = wlan_cpu_to_le16(PCalDataext->CalDataLen + 6 + S_DS_GEN);//chg for Rev A eeprom
	PRINTK1("cmd->Size = %d(d)\n", cmd->Size);
	
	cmd->Result 	= 0;
	
	LEAVE(); 
	return 0;
}
#endif /* End of CAL_DATA */

#ifdef WPA
#ifndef WPA2
static inline int  wlan_cmd_802_11_config_rsn(wlan_private *priv,
		HostCmd_DS_COMMAND *cmd, u16 cmd_option)
{
    	wlan_adapter  *Adapter = priv->adapter;
	PHostCmd_DS_802_11_CONFIG_RSN pRsn = 
		(PHostCmd_DS_802_11_CONFIG_RSN)&cmd->params.rsn;

	ENTER();

	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_CONFIG_RSN);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_CONFIG_RSN) +
				S_DS_GEN);
	cmd->Result 	= 0;
	
	pRsn->Action 	= wlan_cpu_to_le16(cmd_option);
	
	pRsn->Version 	= Adapter->Wpa_ie[6];
	
	pRsn->PairWiseKeysSupported = TRUE;
	
	memcpy(pRsn->MulticastCipher,
			Adapter->Wpa_ie + 8, 4); 
	
	pRsn->TsnEnabled = FALSE;

	LEAVE();
	return 0;
}

static inline int  wlan_cmd_802_11_unicast_cipher(wlan_private *priv,
		HostCmd_DS_COMMAND *cmd, u16 cmd_option)
{	
    	wlan_adapter  *Adapter = priv->adapter;
	PHostCmd_DS_802_11_UNICAST_CIPHER pCipher = 
	 (PHostCmd_DS_802_11_UNICAST_CIPHER)&cmd->params.cipher;

	ENTER();

	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_UNICAST_CIPHER);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_UNICAST_CIPHER) +
				S_DS_GEN);
	cmd->Result 	= 0;
	
	pCipher->Action = wlan_cpu_to_le16(cmd_option);
	
	memcpy(pCipher->UnicastCipher, 
				Adapter->Wpa_ie + 14, 4); 
	
	pCipher->Enabled = wlan_cpu_to_le16(TRUE);

	LEAVE();
	return 0;
}

static inline int  wlan_cmd_802_11_mcast_cipher(wlan_private *priv,
		HostCmd_DS_COMMAND *cmd, u16 cmd_option)
{	
    	wlan_adapter  *Adapter = priv->adapter;
	PHostCmd_DS_802_11_MCAST_CIPHER pCipher = 
	 (PHostCmd_DS_802_11_MCAST_CIPHER)&cmd->params.cipher;

	ENTER();

	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_MCAST_CIPHER);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_MCAST_CIPHER) +
				S_DS_GEN);
	cmd->Result 	= 0;
	
	pCipher->Action = wlan_cpu_to_le16(cmd_option);
	
	memcpy(pCipher->McastCipher, 
				Adapter->Wpa_ie + 8, 4); 
	
	LEAVE();
	return 0;
}
#endif /* end of WPA2 */
#endif /* end of WPA */

/*
 * TODO: Fix the issue when DownloadCommandToStation is being called the
 * second time when the command timesout. All the CmdPtr->xxx are in little
 * endian and therefore all the comparissions will fail.
 * For now - we are not performing the endian conversion the second time - but
 * for PS and DEEP_SLEEP we need to worry
 */
int DownloadCommandToStation(wlan_private * priv, CmdCtrlNode * CmdNode)
{
	u32             	flags;
	HostCmd_DS_COMMAND 	*CmdPtr;
    	wlan_adapter   		*Adapter = priv->adapter;
	int	             	ret = 0;
	u16			CmdSize;
	u16			Command;
	
	OS_INTERRUPT_SAVE_AREA;	/* Needed for threadx */

	ENTER();

	if (!Adapter || !CmdNode) {
		PRINTK1("Adapter or CmdNode is NULL\n");
		return -1;
   	}

	CmdPtr = (HostCmd_DS_COMMAND *) CmdNode->BufVirtualAddr;
	
 	if (!CmdPtr || !CmdPtr->Size) {
		PRINTK1("ERROR: CmdPtr is Null or Cmd Size is Zero, "
							"Not sending\n");
		CleanupAndInsertCmd(priv, CmdNode);
		return -EINVAL;
    	}

	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
	Adapter->CurCmd = CmdNode;
    	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);
	PRINTK1("Before download Size of Cmdptr = %d\n", CmdPtr->Size);

	CmdSize = CmdPtr->Size;

	Command = wlan_cpu_to_le16(CmdPtr->Command);
	
	CmdNode->CmdWaitQWoken = FALSE;
	CmdSize = wlan_cpu_to_le16(CmdSize);

	ret = sbi_host_to_card(priv, MVMS_CMD, (u8 *) CmdPtr, CmdSize);

	// clear TxDone interrupt bit
	OS_INT_DISABLE;
	Adapter->HisRegCpy &= ~HIS_TxDnLdRdy;
	OS_INT_RESTORE;
	
	if (ret != 0) {
		PRINTK1("Host to Card Failed\n");
		// Scan in progress flag should be cleared when scan command failed and dropped.
		if (Command == HostCmd_CMD_802_11_SCAN)
		{
			PRINTK("Clear scan in progress flag.\n");
			Adapter->bIsScanInProgress = FALSE;
		}
		/* should we free Adapter->CurCmd here? */
		CleanupAndInsertCmd(priv, Adapter->CurCmd);
		spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
		Adapter->CurCmd = NULL;
		spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);
		return -1;
    	}

    	PRINTK("Sent command 0x%x @ %lu\n", Command, os_time_get());
    	HEXDUMP("Command", CmdNode->BufVirtualAddr, CmdSize);

    	/* Setup the timer after transmit command */
    	Adapter->isCommandTimerExpired = FALSE;

    	if (Command == HostCmd_CMD_802_11_SCAN 
			|| Command == HostCmd_CMD_802_11_AUTHENTICATE
			|| Command == HostCmd_CMD_802_11_ASSOCIATE)
		ModTimer(&Adapter->MrvDrvCommandTimer, MRVDRV_TIMER_10S);
	else
		ModTimer(&Adapter->MrvDrvCommandTimer, MRVDRV_TIMER_5S);
	Adapter->CommandTimerIsSet = TRUE;

#ifdef	DEEP_SLEEP_CMD
	if (Command == HostCmd_CMD_802_11_DEEP_SLEEP) {
		priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
		OS_INT_DISABLE;
		Adapter->HisRegCpy |= HIS_TxDnLdRdy;
		OS_INT_RESTORE;
		if(Adapter->IntCounter || Adapter->CurrentTxSkb)
			PRINTK(" DS: IntCounter=%d CurrentTxSkb=%p\n",
				Adapter->IntCounter,Adapter->CurrentTxSkb);
		if(Adapter->IntCounter) {
			OS_INT_DISABLE;
			Adapter->IntCounterSaved = Adapter->IntCounter;
			Adapter->IntCounter = 0;
			OS_INT_RESTORE;
		}
#ifdef linux
		if (Adapter->CurrentTxSkb) {
			kfree_skb(Adapter->CurrentTxSkb);
			OS_INT_DISABLE;
			Adapter->CurrentTxSkb = NULL;
			OS_INT_RESTORE;
			priv->stats.tx_dropped++;
		}
#endif
		/* 1. change the PS state to DEEP_SLEEP
		 * 2. since there is no response for this command, so do 
		 * 	following: idel timer, free Node, turn flag of 
		 * 	CmdIn Progress.
		 * 3. stop netif_queue. */

		if (!netif_queue_stopped(priv->wlan_dev.netdev)) {
		/* Flush all the packets upto the OS before stopping */
			wlan_send_rxskbQ(priv);
			netif_stop_queue(priv->wlan_dev.netdev);
		}

		Adapter->IsDeepSleep = TRUE;

		Adapter->IsDeepSleepRequired = FALSE;

		CleanupAndInsertCmd(priv, CmdNode);
		spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
		Adapter->CurCmd = NULL;
		spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

		if (Adapter->CommandTimerIsSet) {
			CancelTimer(&Adapter->MrvDrvCommandTimer);
			Adapter->CommandTimerIsSet = FALSE;
		}

#ifdef BULVERDE_SDIO
		stop_bus_clock_2(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
#endif

#ifdef HOST_WAKEUP
		if (Adapter->bHostWakeupConfigured) {
#if WIRELESS_EXT > 14
			send_iwevcustom_event(priv, "HWM_CFG_DONE.indication ");
			PRINTK1("Deep Sleep: HWM_CFG_DONE event sent\n");
#endif //WIRELESS_EXT
		}
#endif //HOST_WAKEUP

	}
#endif // DEEP_SLEEP_CMD 

	LEAVE();
	return 0;
}

static inline int wlan_cmd_802_11_eeprom_access(wlan_private *priv,
				HostCmd_DS_COMMAND *cmd, int  cmd_option,
		    		void *InfoBuf)
{
	wlan_ioctl_regrdwr *ea = InfoBuf;

	ENTER();
	
	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_802_11_EEPROM_ACCESS);
	cmd->Size	= wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_EEPROM_ACCESS) + S_DS_GEN);
	cmd->Result 	= 0;

	cmd->params.rdeeprom.Action = wlan_cpu_to_le16(ea -> Action);
	cmd->params.rdeeprom.Offset = wlan_cpu_to_le16(ea -> Offset);
	cmd->params.rdeeprom.ByteCount = wlan_cpu_to_le16(ea -> NOB);
	cmd->params.rdeeprom.Value    = 0;

	return 0;
}

#ifdef GSPI83xx
static inline int wlan_cmd_gspi_bus_config(wlan_private *priv,
				HostCmd_DS_COMMAND *cmd, u16  cmd_option,
		    		void *GspiConfig)
{
	pHostCmd_DS_CMD_GSPI_BUS_CONFIG pCmdGspiConfig = 
				(HostCmd_DS_CMD_GSPI_BUS_CONFIG *) &cmd->params.gspicfg;

	cmd->Command	= wlan_cpu_to_le16(HostCmd_CMD_GSPI_BUS_CONFIG);	
	cmd->Size 	= wlan_cpu_to_le16(sizeof(HostCmd_DS_CMD_GSPI_BUS_CONFIG)+ S_DS_GEN);
	cmd->Result 	= 0;

	pCmdGspiConfig -> Action = wlan_cpu_to_le16(cmd_option);
	pCmdGspiConfig -> BusDelayMode = wlan_cpu_to_le16(g_bus_mode_reg);
	pCmdGspiConfig -> HostTimeDelayToReadPort = wlan_cpu_to_le16(g_dummy_clk_ioport * 16);
	pCmdGspiConfig -> HostTimeDelayToReadregister = wlan_cpu_to_le16(g_dummy_clk_reg * 16);

	return 0;
}
#endif /* GSPI83xx */

#ifdef ATIMGEN			
static inline int wlan_cmd_802_11_generate_atim(wlan_private *priv,
				HostCmd_DS_COMMAND *cmd, u16  cmd_option)
{
	PHostCmd_DS_802_11_GENERATE_ATIM pAtim =
		(PHostCmd_DS_802_11_GENERATE_ATIM)&cmd->params.genatim;
    	wlan_adapter  *Adapter = priv->adapter;
	
	ENTER();
	
	PRINTK("ENABLE/DISABLE ATIM\n");

	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_GENERATE_ATIM);
	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_GENERATE_ATIM) + S_DS_GEN);
	cmd->Result = 0;
	pAtim->Action = wlan_cpu_to_le16(cmd_option);

	if (Adapter->ATIMEnabled == TRUE) {
		pAtim->Enable = wlan_cpu_to_le16(HostCmd_ENABLE_GENERATE_ATIM);
	} else {
		pAtim->Enable = wlan_cpu_to_le16(HostCmd_DISABLE_GENERATE_ATIM);
	}

	LEAVE();
	return 0;
}
#endif

void QueueCmd(wlan_adapter *Adapter, CmdCtrlNode *CmdNode, BOOLEAN addtail)
{
	u32             	flags;
	HostCmd_DS_COMMAND 	*CmdPtr;

	ENTER();
	
 	if (!CmdNode) {
		PRINTK1("ERROR: CmdNode is NULL\n");
		return;
    	}
	CmdPtr = (HostCmd_DS_COMMAND *)CmdNode->BufVirtualAddr;
 	if (!CmdPtr) {
		PRINTK1("ERROR: CmdPtr is NULL\n");
		return;
    	}

#ifdef PS_REQUIRED
	if (CmdPtr->Command == HostCmd_CMD_802_11_PS_MODE) {
		HostCmd_DS_802_11_PS_MODE *psm = &CmdPtr->params.psmode;
		if (psm->Action == HostCmd_SubCmd_Exit_PS) {
			if (Adapter->PSState != PS_STATE_FULL_POWER)
				addtail = FALSE;
		}
	}
#endif

#ifdef HOST_WAKEUP
	if (CmdPtr->Command == HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM) {
		addtail = FALSE;
	}
#endif

	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);

	if (addtail)
		list_add_tail((struct list_head *)CmdNode,
						&Adapter->CmdPendingQ);
	else
		list_add((struct list_head *)CmdNode,
						&Adapter->CmdPendingQ);

	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

	PRINTK1("Inserted node=0x%x, cmd=0x%x in CmdPendingQ\n", (u32)CmdNode, 
			((HostCmd_DS_GEN *)CmdNode->BufVirtualAddr)->Command);

	LEAVE();
}

#ifdef MANF_CMD_SUPPORT
static inline int SendMfgCommand(wlan_private *priv, CmdCtrlNode *cmdnode)
{
	HostCmd_DS_GEN	*pCmdPtr;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
			
	pCmdPtr = (PHostCmd_DS_GEN) Adapter->mfg_cmd;

	pCmdPtr->Command = wlan_cpu_to_le16(HostCmd_CMD_MFG_COMMAND);

	SetCmdCtrlNode(priv, cmdnode, OID_MRVL_MFG_COMMAND,
			HostCmd_PENDING_ON_GET_OID, HostCmd_OPTION_USE_INT 
			| HostCmd_OPTION_WAITFORRSP
			, pCmdPtr);

	// Assign new sequence number
	pCmdPtr->SeqNum = wlan_cpu_to_le16(priv->adapter->SeqNum);

	PRINTK1("Sizeof CmdPtr->size %d\n", (u32) pCmdPtr->Size);

	// copy the command from information buffer to command queue
	memcpy((void *) cmdnode->BufVirtualAddr, (void *) pCmdPtr,
							pCmdPtr->Size);
	Adapter->mfg_cmd_flag = 1;

	LEAVE();
	return 0;
}
#endif

int PrepareAndSendCommand(wlan_private * priv, u16 cmdno, u16 cmd_option,
		      u16 intoption, WLAN_OID PendingOID,
		      u16 PendingInfo, 
		      void *InfoBuf)
{
	static int      ret;
	wlan_adapter   *Adapter = priv->adapter;
	CmdCtrlNode    *CmdNode;
	HostCmd_DS_COMMAND *CmdPtr;

	ENTER();

	if (!Adapter) {
		PRINTK1("Adapter is Null\n");
		return -ENODEV;
	}

#ifdef DEEP_SLEEP
	if (Adapter->IsDeepSleep == TRUE) {
		PRINTK1("Deep sleep enabled\n");
		LEAVE();
		return -EBUSY;
	}
#endif

	if (Adapter->SurpriseRemoved) {
		PRINTK("Card is Removed\n");
		LEAVE();
		return -1;
	}
	
	CmdNode = GetFreeCmdCtrlNode(priv);

	if (CmdNode == NULL) {
		PRINTK1("No free CmdNode\n");
		/* Wake up main thread to execute next command */
		wake_up_interruptible(&priv->MainThread.waitQ);
		LEAVE();
		return -1;
    	}
	
	if (CmdNode->retcode) {
		PRINTK("Command retcode not yet delivered\n");
		CleanupAndInsertCmd(priv, CmdNode);
		LEAVE();
		return ret;
	}

	SetCmdCtrlNode(priv, CmdNode, PendingOID, PendingInfo, intoption,
			InfoBuf);
	CmdPtr = (HostCmd_DS_COMMAND *) CmdNode->BufVirtualAddr;

	PRINTK1("Val of Cmd ptr = %x, command=0x%X\n", (u32) CmdPtr,cmdno);

	if (!CmdPtr) {
		PRINTK1("BufVirtualAddr of CmdNode is NULL\n");
		CleanupAndInsertCmd(priv, CmdNode);
		LEAVE();
		return -1;
	}
	/* Set sequence number, command and INT option */
	Adapter->SeqNum++;	// TODO: is anybody checking for this?
	CmdPtr->SeqNum = wlan_cpu_to_le16(Adapter->SeqNum);

	CmdPtr->Command = cmdno;
	CmdPtr->Result = 0; 

	TX_EVENT_FLAGS_SET(&CmdNode->cmdwait_q, 0, TX_AND);
	switch (cmdno) {
#ifdef DTIM_PERIOD
		case HostCmd_CMD_SET_DTIM_MULTIPLE:
			ret = wlan_cmd_set_dtim_multiple(priv, CmdPtr, InfoBuf);
			break;
#endif

#ifdef PASSTHROUGH_MODE
		case HostCmd_CMD_802_11_PASSTHROUGH:
			ret = wlan_cmd_passthrough(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
#endif
		case HostCmd_CMD_GET_HW_SPEC:	/* 0x03 - */
			ret = wlan_cmd_hw_spec(priv, CmdPtr);
			break;
#ifdef PS_REQUIRED
		case HostCmd_CMD_802_11_PS_MODE:	/* 0x21 - */
			ret = wlan_cmd_802_11_ps_mode(priv, CmdPtr, cmd_option, 
				*((u16 *) InfoBuf));
			break;
#endif

		case HostCmd_CMD_802_11_SCAN:	/* 0x06 - */
			ret = wlan_cmd_802_11_scan(priv, CmdPtr);
			break;

		case HostCmd_CMD_MAC_CONTROL:	/* 0x28 - */
			ret = wlan_cmd_mac_control(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_11_ASSOCIATE:	/* 0x12 - */
		case HostCmd_CMD_802_11_REASSOCIATE:	/* 0x25- */
			ret = wlan_cmd_802_11_associate(priv, CmdNode, InfoBuf);
			break;

		case HostCmd_CMD_802_11_DEAUTHENTICATE:	/* 0x24 */
			ret = wlan_cmd_802_11_deauthenticate(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_11_SET_WEP:	/* 0x13 */
			ret = wlan_cmd_802_11_set_wep(priv, CmdPtr, PendingOID,
					InfoBuf, cmd_option);
			break;

    		case HostCmd_CMD_802_11_AD_HOC_START:	/* 0x2b */
			ret = wlan_cmd_802_11_ad_hoc_start(priv, CmdNode, 
					InfoBuf);
			break;
		case HostCmd_CMD_CODE_DNLD:	/* 0x02 */// TODO 
			break;

		case HostCmd_CMD_802_11_RESET:	/* 0x05 */
			ret = wlan_cmd_802_11_reset(priv, CmdPtr, cmd_option);
			break;

		case HostCmd_CMD_802_11_QUERY_TRAFFIC:	/* 0x09 */
			ret = wlan_cmd_802_11_query_traffic(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_11_QUERY_STATUS:	/* 0xa */
			ret = wlan_cmd_802_11_query_status(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_11_GET_LOG:	/* 0xb */
			ret = wlan_cmd_802_11_get_log(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_11_AUTHENTICATE:	/* 0x11 */
			ret = wlan_cmd_802_11_authenticate(priv, CmdPtr,
						PendingOID, InfoBuf);
			break;

		case HostCmd_CMD_802_11_GET_STAT:	/* 0x14 */
			ret = wlan_cmd_802_11_get_stat(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_3_GET_STAT:	/* 0x15 */
			ret = wlan_cmd_802_3_get_stat(priv, CmdPtr);
			break;

		case HostCmd_CMD_802_11_SNMP_MIB:	/* 0x16 */
			ret = wlan_cmd_802_11_snmp_mib(priv, CmdPtr, 
					PendingInfo, PendingOID, InfoBuf);
			break;

		case HostCmd_CMD_MAC_REG_ACCESS:	/* 0x0019 */
		case HostCmd_CMD_BBP_REG_ACCESS:	/* 0x001a */
		case HostCmd_CMD_RF_REG_ACCESS:	/* 0x001b */
			ret = wlan_cmd_reg_access(priv, CmdPtr, cmd_option, 
					InfoBuf);
			break;

		case HostCmd_CMD_802_11_RF_CHANNEL:	/* 0x1D */
			ret = wlan_cmd_802_11_rf_channel(priv, CmdPtr,
					cmd_option, InfoBuf);
			break;

		case HostCmd_CMD_802_11_RF_TX_POWER:	/* 0x1e */ 
			ret = wlan_cmd_802_11_rf_tx_power(priv, CmdPtr, 
					cmd_option, InfoBuf);
			break;

		case HostCmd_CMD_802_11_RADIO_CONTROL:	/* 0x1c */
			ret = wlan_cmd_802_11_radio_control(priv, CmdPtr, 
					cmd_option);
			break;

#ifdef BCA
		case HostCmd_CMD_BCA_CONFIG:		/* 0x51 */
			ret = wlan_cmd_bca_config(priv, CmdPtr, cmd_option);
			break;
#endif
		
		case HostCmd_CMD_802_11_RF_ANTENNA:	/* 0x20 */
			ret = wlan_cmd_802_11_rf_antenna(priv, CmdPtr, 
					cmd_option, InfoBuf);
			break;

		case HostCmd_CMD_802_11_DATA_RATE:	/* 0x22 */
			ret = wlan_cmd_802_11_data_rate(priv, CmdPtr, 
					cmd_option, InfoBuf);
			break;
		case HostCmd_CMD_802_11_RATE_ADAPT_RATESET:
			ret = wlan_cmd_802_11_rate_adapt_rateset(priv,
				       CmdPtr, cmd_option, InfoBuf);
			break;

		case HostCmd_CMD_MAC_MULTICAST_ADR:	/* 0x10 */
			ret = wlan_cmd_mac_multicast_adr(priv, CmdPtr, 
					cmd_option);
			break;

		case HostCmd_CMD_802_11_AD_HOC_JOIN:	/* 0x2c */
			ret = wlan_cmd_802_11_ad_hoc_join(priv, CmdNode, 
					InfoBuf);
			break;

		case HostCmd_CMD_802_11_RSSI:	/* 0x1f */
			ret = wlan_cmd_802_11_rssi(priv, CmdPtr);
			break;
		
		case HostCmd_CMD_802_11_DISASSOCIATE:	/* 0x26 */
			ret = wlan_cmd_802_11_deauthenticate(priv, CmdPtr);
			break;
		
		case HostCmd_CMD_802_11_AD_HOC_STOP:	/* 0x40 */
			ret = wlan_cmd_802_11_ad_hoc_stop(priv, CmdPtr);
			break;

#ifdef WPA
		case HostCmd_CMD_802_11_QUERY_RSN_OPTION:
			ret = wlan_cmd_802_11_query_rsn_option(priv, 
					CmdPtr);
			break;
		case HostCmd_CMD_802_11_ENABLE_RSN:
			ret = wlan_cmd_802_11_enable_rsn(priv, CmdPtr, 
					cmd_option);
			break;

#ifndef WPA2
		case HostCmd_CMD_802_11_CONFIG_RSN:
			ret = wlan_cmd_802_11_config_rsn(priv, CmdPtr, 
					cmd_option);
			break;
		case HostCmd_CMD_802_11_UNICAST_CIPHER:
			ret = wlan_cmd_802_11_unicast_cipher(priv, CmdPtr, 
					cmd_option);
			break;
		case HostCmd_CMD_802_11_MCAST_CIPHER:
			ret = wlan_cmd_802_11_mcast_cipher(priv, CmdPtr, 
					cmd_option);
			break;
		case HostCmd_CMD_802_11_RSN_AUTH_SUITES:
			ret = wlan_cmd_802_11_rsn_auth_suites(priv, 
					CmdPtr, cmd_option);
			break;
		case HostCmd_CMD_802_11_PWK_KEY:
			ret = wlan_cmd_802_11_pwk_key(priv, CmdPtr,
					cmd_option, InfoBuf);
			break;
		case HostCmd_CMD_802_11_GRP_KEY:
			ret = wlan_cmd_802_11_grp_key(priv, CmdPtr,
					cmd_option, InfoBuf);
			break;
#endif //WPA2

#ifdef WPA2
		case HostCmd_CMD_802_11_KEY_MATERIAL:
			ret = wlan_cmd_802_11_key_material(priv, CmdPtr,
					cmd_option, PendingOID, InfoBuf);
			break;
#endif //WPA2

		case HostCmd_CMD_802_11_PAIRWISE_TSC:
			break;
		case HostCmd_CMD_802_11_GROUP_TSC:
			break;
#endif /* end of WPA */

		case HostCmd_CMD_802_11_BEACON_STOP:
			ret = wlan_cmd_802_11_beacon_stop(priv, CmdPtr);
			break;
		case HostCmd_CMD_802_11_TX_MODE:
			ret = wlan_cmd_802_11_tx_mode(priv, CmdPtr,
					InfoBuf);
			break;

		case HostCmd_CMD_802_11_TX_CONTROL_MODE:
			ret = wlan_cmd_802_11_tx_control_mode(priv, CmdPtr,
					InfoBuf);
			break;

#ifdef PS_REQUIRED
		case HostCmd_CMD_802_11_PRE_TBTT: /* 0x0047 */
			ret = wlan_cmd_802_11_set_pre_tbtt(priv,
					CmdPtr, cmd_option, InfoBuf);
			break;
#endif
		
		case HostCmd_CMD_802_11_MAC_ADDRESS: /* 0x004D */
			ret = wlan_cmd_802_11_mac_address(priv,
					CmdPtr, InfoBuf);
			CmdPtr->params.macadd.Action = wlan_cpu_to_le16(cmd_option);
			if (cmd_option == HostCmd_ACT_SET){
				memcpy(CmdPtr->params.macadd.MacAdd,
						Adapter->CurrentAddr,
						ETH_ALEN);
				HEXDUMP("MAC ADDRESS:", 
						Adapter->CurrentAddr, 6);
			}
			break;
#ifdef CAL_DATA
		case HostCmd_CMD_802_11_CAL_DATA:
			ret = wlan_cmd_802_11_cal_data(priv, CmdPtr, 
					cmd_option, InfoBuf);
			break;
#endif /* CAL_DATA */

#ifdef CAL_DATA
		case HostCmd_CMD_802_11_CAL_DATA_EXT:
			ret = wlan_cmd_802_11_cal_data_ext(priv, CmdPtr, 
					cmd_option, InfoBuf);
			break;
#endif /* CAL_DATA */

#ifdef	DEEP_SLEEP_CMD
		case HostCmd_CMD_802_11_DEEP_SLEEP:
			CmdPtr->Size = wlan_cpu_to_le16((u16)
				(sizeof(HostCmd_DS_802_11_DEEP_SLEEP)));
			break;
#endif // DEEP_SLEEP_CMD

#ifdef HOST_WAKEUP
		case HostCmd_CMD_802_11_HOST_WAKE_UP_CFG:	/* 0x0043 */
			ret = wlan_cmd_802_11_host_wake_up_cfg(priv, CmdPtr,
					InfoBuf);
			break;
		case HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM:	/* 0x0044 */
			ret = wlan_cmd_802_11_host_awake_confirm(priv, CmdPtr);
			break;
#endif
#ifdef MULTI_BANDS
		case HostCmd_CMD_802_11_BAND_CONFIG: /* 0x0058 */
			ret = wlan_cmd_802_11_band_config(priv, CmdPtr, 
					cmd_option, InfoBuf);
			break;
#endif
		case HostCmd_CMD_802_11_EEPROM_ACCESS: /* 0x0059 */
			ret = wlan_cmd_802_11_eeprom_access(priv, CmdPtr,
					cmd_option, InfoBuf);
			break;
#ifdef GSPI83xx
		case HostCmd_CMD_GSPI_BUS_CONFIG: /* 0x5A */
			ret = wlan_cmd_gspi_bus_config(priv, CmdPtr,
					cmd_option, InfoBuf);
			break;
#endif /* GSPI83xx */

#ifdef ATIMGEN			
		case HostCmd_CMD_802_11_GENERATE_ATIM:
			ret = wlan_cmd_802_11_generate_atim(priv, CmdPtr,
					cmd_option);
			break;
#endif

#ifdef MANF_CMD_SUPPORT
		case HostCmd_CMD_MFG_COMMAND:
			ret = SendMfgCommand(priv, CmdNode);
			break;
#endif

#ifdef AUTO_FREQ_CTRL
		case HostCmd_CMD_802_11_SET_AFC:
		case HostCmd_CMD_802_11_GET_AFC:

                  CmdPtr->Command = wlan_cpu_to_le16(cmdno);
                  CmdPtr->Size    = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_AFC) + S_DS_GEN);

                  memmove(
                    &CmdPtr->params.afc, 
                    InfoBuf, 
                    sizeof(HostCmd_DS_802_11_AFC)
                    );

                  ret = 0;
                  break;
#endif

#ifdef ENABLE_802_11D
		case HostCmd_CMD_802_11D_DOMAIN_INFO:	/* Cmd ID Number  005b?? - */
			ret = wlan_cmd_802_11d_domain_info(priv, CmdPtr, cmdno, cmd_option );
			break;
#endif		

#ifdef ENABLE_802_11H_TPC
		case HostCmd_CMD_802_11H_TPC_REQUEST:
			ret = wlan_cmd_802_11h_tpc_request(priv, CmdPtr, cmdno);
			break;
		case HostCmd_CMD_802_11H_TPC_INFO:
			ret = wlan_cmd_802_11h_tpc_info(priv, CmdPtr, cmdno);
			break;
#endif	

		case HostCmd_CMD_802_11_SLEEP_PARAMS: /* 0x0066 */
			ret = wlan_cmd_802_11_sleep_params(priv, CmdPtr, 
								cmd_option);
			break;
#ifdef BCA		
		case HostCmd_CMD_802_11_BCA_CONFIG_TIMESHARE:
			ret = wlan_cmd_802_11_bca_timeshare(priv, CmdPtr,
								cmd_option, InfoBuf);
			break;
#endif
		case HostCmd_CMD_802_11_INACTIVITY_TIMEOUT:
			ret = wlan_cmd_802_11_inactivity_timeout(priv, CmdPtr,
							cmd_option, InfoBuf);
			SetCmdCtrlNode(priv, CmdNode, 0, 0,
					HostCmd_OPTION_USE_INT, InfoBuf);
			break;
#ifdef BG_SCAN
		case HostCmd_CMD_802_11_BG_SCAN_CONFIG:
			ret = wlan_cmd_802_11_bg_scan_config(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;

		case HostCmd_CMD_802_11_BG_SCAN_QUERY:
			ret = wlan_cmd_802_11_bg_scan_query(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
#endif /* BG_SCAN */

#ifdef PS_REQUIRED
#ifdef FW_WAKEUP_METHOD
		case HostCmd_CMD_802_11_FW_WAKEUP_METHOD:
			ret = wlan_cmd_802_11_fw_wakeup_method(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
#endif
#endif

#ifdef SUBSCRIBE_EVENT_CTRL
		case HostCmd_CMD_802_11_SUBSCRIBE_EVENT:
			ret = wlan_cmd_802_11_subscribe_events(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
#endif
			
#ifdef WMM
		case HostCmd_CMD_802_11_WMM_GET_TSPEC:
		case HostCmd_CMD_802_11_WMM_ADD_TSPEC:
		case HostCmd_CMD_802_11_WMM_REMOVE_TSPEC:
			ret = wlan_cmd_802_11_wmm_tspec(priv, CmdPtr,
							cmdno, InfoBuf);
			break;
		case HostCmd_CMD_802_11_WMM_ACK_POLICY:
			ret = wlan_cmd_802_11_wmm_ack_policy(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
		case HostCmd_CMD_802_11_WMM_GET_STATUS:
			ret = wlan_cmd_802_11_wmm_get_status(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
		case HostCmd_CMD_802_11_WMM_PRIO_PKT_AVAIL:
			ret = wlan_cmd_802_11_wmm_prio_pkt_avail(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
#endif /* WMM */

                case HostCmd_CMD_802_11_TPC_CFG:

                  CmdPtr->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_TPC_CFG);
                  CmdPtr->Size    = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_TPC_CFG) + S_DS_GEN);

                  memmove(
                    &CmdPtr->params.tpccfg, 
                    InfoBuf, sizeof(HostCmd_DS_802_11_TPC_CFG)
                    );

                  ret = 0;
                  break;
#ifdef  LED_GPIO_CTRL
                case HostCmd_CMD_802_11_LED_GPIO_CTRL:
                {
                  MrvlIEtypes_LedGpio_t *gpio = (MrvlIEtypes_LedGpio_t *)CmdPtr->params.ledgpio.data;

                  memmove(
                    &CmdPtr->params.ledgpio, 
                    InfoBuf,
                    sizeof(HostCmd_DS_802_11_LED_CTRL)
                    );

                  CmdPtr->Command = 
                    wlan_cpu_to_le16(HostCmd_CMD_802_11_LED_GPIO_CTRL);

                  CmdPtr->Size = 
                    wlan_cpu_to_le16(gpio->Header.Len + S_DS_GEN + 8); //Action + NumLed + TLV Type + Length = 8

                  gpio->Header.Len = wlan_cpu_to_le16(gpio->Header.Len);
                                                    
                  ret = 0;
                  break;
                }
#endif /* LED_GPIO_CTRL */
                case HostCmd_CMD_802_11_PWR_CFG:

                  CmdPtr->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_PWR_CFG);
                  CmdPtr->Size    = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_PWR_CFG) + S_DS_GEN);

                  memmove(
                    &CmdPtr->params.pwrcfg, 
                    InfoBuf, 
                    sizeof(HostCmd_DS_802_11_PWR_CFG)
                    );

                  ret = 0;
                  break;
		case HostCmd_CMD_802_11_SLEEP_PERIOD:
			ret = wlan_cmd_802_11_sleep_period(priv, CmdPtr,
							cmd_option, InfoBuf);
			break;
#ifdef CIPHER_TEST
    case HostCmd_CMD_802_11_KEY_ENCRYPT:

      CmdPtr->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_KEY_ENCRYPT);
      CmdPtr->Size    = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11_KEY_ENCRYPT) + S_DS_GEN);

      memmove(
        &CmdPtr->params.key_encrypt,
        InfoBuf,
        sizeof(HostCmd_DS_802_11_KEY_ENCRYPT)
        );
      ret = 0;
      break;
#endif      
		default:
			/*
			 * TODO: Replace with unknown command error 
			 */
			ret = -1;
	}

	/* return error, since the command preparation failed */
	if (ret < 0) {
		PRINTK("Command preparation failed\n");
		CleanupAndInsertCmd(priv, CmdNode);
		LEAVE();
		return ret;
	}
	
	CmdNode->CmdWaitQWoken = FALSE;
	
	QueueCmd(Adapter, CmdNode, TRUE);
	wake_up_interruptible(&priv->MainThread.waitQ);

	sbi_reenable_host_interrupt(priv, 0x00);/* To enable low level
						 * SDIO interrupt, passing
						 * 0x00 as a unused arg */

	if (intoption & HostCmd_OPTION_WAITFORRSP) {
		wait_event_interruptible(CmdNode->cmdwait_q, 
							CmdNode->CmdWaitQWoken);
	}
		
	ret = CmdNode->retcode;

	/* Now reset the retcode to zero as we are done with it */
	CmdNode->retcode = 0;
	
	if (ret)	
		PRINTK("Command failed with retcode=%d\n", ret);

	return ret;
}

int AllocateCmdBuffer(wlan_private * priv)
{
 	int             Status = WLAN_STATUS_SUCCESS;
 	u32             ulBufSize;
 	u32             i;
 	CmdCtrlNode    *TempCmdArray;
 	u8             *pTempVirtualAddr;
 	wlan_adapter   *Adapter = priv->adapter;

 	ENTER();

 	/* Allocate and initialize CmdCtrlNode */
 	INIT_LIST_HEAD(&Adapter->CmdFreeQ);
        INIT_LIST_HEAD(&Adapter->CmdPendingQ);

 	ulBufSize = sizeof(CmdCtrlNode) * MRVDRV_NUM_OF_CMD_BUFFER;
 	if (!(TempCmdArray = kmalloc(ulBufSize, GFP_KERNEL))) {
		PRINTK1("Failed to allocate TempCmdArray\n");
		return -1;
	}

	Adapter->CmdArray = TempCmdArray;
	memset(Adapter->CmdArray, 0, ulBufSize);

	/* Allocate and initialize command buffers */
	ulBufSize = MRVDRV_SIZE_OF_CMD_BUFFER;
	for (i = 0; i < MRVDRV_NUM_OF_CMD_BUFFER; i++) {
		if (!(pTempVirtualAddr = kmalloc(ulBufSize, GFP_KERNEL))) {
			PRINTK1("pTempVirtualAddr: out of memory\n");
			return -1;
		}

		memset(pTempVirtualAddr, 0, ulBufSize);

		/* Update command buffer virtual */
		TempCmdArray[i].BufVirtualAddr = pTempVirtualAddr;
	}

	for (i = 0; i < MRVDRV_NUM_OF_CMD_BUFFER; i++) {
		init_waitqueue_head(&TempCmdArray[i].cmdwait_q);
                CleanupAndInsertCmd(priv, &TempCmdArray[i]);
    	}

    	return Status;
}

int FreeCmdBuffer(wlan_private * priv)
{
    	u32           ulBufSize;
    	UINT            i;
    	CmdCtrlNode    *TempCmdArray;
    	wlan_adapter   *Adapter = priv->adapter;

    	ENTER();

    	/* need to check if cmd array is allocated or not */
    	if (Adapter->CmdArray == NULL) {
		PRINTK("CmdArray is Null\n");
		return WLAN_STATUS_SUCCESS;
	}

	TempCmdArray = Adapter->CmdArray;

	/* Release shared memory buffers */
	ulBufSize = MRVDRV_SIZE_OF_CMD_BUFFER;
	for (i = 0; i < MRVDRV_NUM_OF_CMD_BUFFER; i++) {
		if (TempCmdArray[i].BufVirtualAddr) {
			PRINTK("Free all the array\n");
			kfree(TempCmdArray[i].BufVirtualAddr);
			TempCmdArray[i].BufVirtualAddr = NULL;
		}
	}

	/* Release CmdCtrlNode */
	if (Adapter->CmdArray) {
		PRINTK("Free CmdArray\n");
		kfree(Adapter->CmdArray);
		Adapter->CmdArray = NULL;
	}

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

CmdCtrlNode    *GetFreeCmdCtrlNode(wlan_private * priv)
{
    	CmdCtrlNode	*TempNode;
	wlan_adapter	*Adapter = priv->adapter;
	u32		flags;

    	ENTER();

    	if (!Adapter)
		return NULL;

	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
	
	if (!list_empty(&Adapter->CmdFreeQ)) {
		TempNode = (CmdCtrlNode *)Adapter->CmdFreeQ.next;
		list_del((struct list_head *)TempNode);
	} else {
		PRINTK1("CmdCtrlNode is not available\n");
		TempNode = NULL;
	}

	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

	if (TempNode) {
		PRINTK1("CmdCtrlNode available\n");
		PRINTK1("CmdCtrlNode Address = %p\n", TempNode);
		CleanUpCmdCtrlNode(TempNode);
	}
	
	LEAVE();
	return TempNode;
}

void CleanUpCmdCtrlNode(CmdCtrlNode * pTempNode)
{
	ENTER();

	if (!pTempNode)
		return;
	pTempNode->CmdWaitQWoken = TRUE;
	wake_up_interruptible(&pTempNode->cmdwait_q);
	pTempNode->Status = 0;
	pTempNode->PendingOID = (WLAN_OID) 0;
	pTempNode->PendingInfo = HostCmd_PENDING_ON_NONE;
	pTempNode->INTOption = HostCmd_OPTION_USE_INT;	// Default
	pTempNode->InformationBuffer = NULL;
	pTempNode->retcode = 0;

	if (pTempNode->BufVirtualAddr != NULL)
		memset(pTempNode->BufVirtualAddr, 0, MRVDRV_SIZE_OF_CMD_BUFFER);

	LEAVE();
	return;
}

void SetCmdCtrlNode(wlan_private * priv, CmdCtrlNode * pTempNode,
	       WLAN_OID PendingOID, u16 PendingInfo,
	       u16 INTOption, 
	       void *InformationBuffer)
{
	ENTER();

	if (!pTempNode)
		return;

	pTempNode->PendingOID = PendingOID;
	pTempNode->PendingInfo = PendingInfo;
	pTempNode->INTOption = INTOption;
	pTempNode->InformationBuffer = InformationBuffer;

	LEAVE();
}

/*
 * Executes the next command pushed by rx_ret calls 
 */
int ExecuteNextCommand(wlan_private * priv)
{
	wlan_adapter   		*Adapter = priv->adapter;
	CmdCtrlNode		*CmdNode = NULL;
	HostCmd_DS_COMMAND	*CmdPtr;
	u32			flags;

	ENTER();

	if (!Adapter) {
		PRINTK1("Adapter is NULL\n");
		return -1;
   	}
	

#ifdef DEEP_SLEEP_CMD
	if (Adapter->IsDeepSleep == TRUE) {
		PRINTK("Device is in deep sleep mode.\n");
		LEAVE();
		return 0;
	}
#endif /* DEEP_SLEEP_CMD */

	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);

	if (Adapter->CurCmd) {
		spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);
		return WLAN_STATUS_SUCCESS;
	}

	if (!list_empty(&Adapter->CmdPendingQ)) {
		CmdNode = (CmdCtrlNode *)
				Adapter->CmdPendingQ.next;
	}
	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);
	
	if (CmdNode) {
		PRINTK("Got next command from CmdPendingQ\n");
		CmdPtr = (HostCmd_DS_COMMAND *) CmdNode->BufVirtualAddr;

#ifdef	PS_REQUIRED
		if (Is_Command_Allowed_In_PS(CmdPtr->Command)) {
			if ((Adapter->PSState == PS_STATE_SLEEP)
#ifdef PS_PRESLEEP
				|| (Adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
			) {
				PRINTK("CANNOT send cmd 0x%x in PSState %d\n",
					CmdPtr->Command, Adapter->PSState);
				return 0;
			}
			PRINTK("DownloadCommand: OK to send command "
					"0x%x in PSState %d\n", 
					CmdPtr->Command,Adapter->PSState);
		} else if (Adapter->PSState != PS_STATE_FULL_POWER) {
			/*
			 * 1. Non-PS command: 
			 * Queue it. set NeedToWakeup to TRUE if current state 
			 * is SLEEP, otherwise call PSWakeup to send Exit_PS.
			 * 2. PS command but not Exit_PS: 
			 * Ignore it.
			 * 3. PS command Exit_PS:
			 * Set NeedToWakeup to TRUE if current state is SLEEP, 
			 * otherwise send this command down to firmware
			 * immediately.
			 */
			if (CmdPtr->Command != HostCmd_CMD_802_11_PS_MODE) {
				/*  Prepare to send Exit PS,
				 *  this non PS command will be sent later */
				if ((Adapter->PSState == PS_STATE_SLEEP)
#ifdef PS_PRESLEEP
					|| (Adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
				) {
					Adapter->NeedToWakeup = TRUE;
				}
				else
					PSWakeup(priv, 0);
				LEAVE();
				return 0;
			} else {
				/*
				 * PS command. Ignore it if it is not Exit_PS. 
				 * otherwise send it down immediately.
				 */
				HostCmd_DS_802_11_PS_MODE *psm = 
					&CmdPtr->params.psmode;

				PRINTK1("PS cmd: Action=0x%x\n",psm->Action);

				if (psm->Action != HostCmd_SubCmd_Exit_PS) {
					PRINTK1("Ignore Enter PS cmd\n");
					list_del((struct list_head *) CmdNode);
					CleanupAndInsertCmd(priv,CmdNode);
					return 0;
				}
			
				if ((Adapter->PSState == PS_STATE_SLEEP)
#ifdef PS_PRESLEEP
					|| (Adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
				) {
					PRINTK1("Ignore ExitPS cmd in sleep\n");
					list_del((struct list_head *) CmdNode);
					CleanupAndInsertCmd(priv,CmdNode);
					Adapter->NeedToWakeup = TRUE; 
					return 0;
				}

				PRINTK1("Sending Exit_PS down...\n");
			} 
		}
#endif
		list_del((struct list_head *) CmdNode);
		PRINTK1("Sending 0x%04X Command\n", CmdPtr->Command);
		DownloadCommandToStation(priv, CmdNode);
	}
#ifdef	PS_REQUIRED
	else {
		/*
		 * check if in power save mode, if yes, put the device back
		 * to PS mode
		 */
		if ((Adapter->PSMode != Wlan802_11PowerModeCAM) &&
				(Adapter->PSState == PS_STATE_FULL_POWER) &&
				(Adapter->MediaConnectStatus == 
				 WlanMediaStateConnected)) {
#ifdef WPA
			if(Adapter->SecInfo.WPAEnabled
#ifdef WPA2
				|| Adapter->SecInfo.WPA2Enabled
#endif
			) {
				if(Adapter->IsGTK_SET == TRUE) {
					PRINTK("WPA enabled and GTK_SET"
						" go back to PS_SLEEP");
					PSSleep(priv,
						Wlan802_11PowerModeMAX_PSP, 0);
				}
			} else
#endif
			{
				PRINTK("Command PendQ is empty," 
					" go back to PS_SLEEP");
				PSSleep(priv, Wlan802_11PowerModeMAX_PSP, 0);
			}
		}
	} 
#endif

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

#ifdef PS_REQUIRED
int AllocatePSConfirmBuffer(wlan_private * priv)
{
	wlan_adapter   *Adapter = priv->adapter;

	ENTER();

	if (!(Adapter->PSConfirmSleep =
				kmalloc(sizeof(PS_CMD_ConfirmSleep),
					GFP_KERNEL))) {
			PRINTK1("Allocate ConfirmSleep out of memory \n");
			return -1;
    	}

    	memset(Adapter->PSConfirmSleep, 0, sizeof(PS_CMD_ConfirmSleep));

    	Adapter->PSConfirmSleep->SeqNum = ++Adapter->SeqNum;
    	Adapter->PSConfirmSleep->Command = HostCmd_CMD_802_11_PS_MODE;
    	Adapter->PSConfirmSleep->Size = sizeof(PS_CMD_ConfirmSleep);
    	Adapter->PSConfirmSleep->Result = 0;
    	Adapter->PSConfirmSleep->Action = HostCmd_SubCmd_Sleep_Confirmed;

    	LEAVE();
    	return 0;
}

#if WIRELESS_EXT > 14
void send_iwevcustom_event(wlan_private *priv, char *str)
{
	union iwreq_data iwrq;
	unsigned char buf[50];
  
	ENTER();

	memset(&iwrq, 0, sizeof(union iwreq_data));
	memset(buf, 0, sizeof(buf));
  
	snprintf(buf, sizeof(buf)-1, "%s", str);
  
	iwrq.data.pointer = buf; 
	iwrq.data.length = strlen(buf) + 1 + IW_EV_LCP_LEN;

	/* Send Event to upper layer */
	PRINTK1("Event Indication string = %s\n", iwrq.data.pointer);
	PRINTK1("Event Indication String Length = %d\n", iwrq.data.length);

	PRINTK1("Sending wireless event IWEVCUSTOM for %s\n", str);
	wireless_send_event(priv->wlan_dev.netdev, IWEVCUSTOM, &iwrq, buf);

	LEAVE();
	return;
}
#endif

int SendConfirmSleep(wlan_private * priv, u8 * CmdPtr, u16 size)
{
	wlan_adapter *Adapter = priv->adapter;
	int             ret = 0;
#ifndef DUMMY_PACKET
//	int             timeout = 0;
#endif
	ENTER();

	PRINTK1("Before download Size of ConfirmCmdptr = %d\n", size);

	HEXDUMP("Confirm Sleep Command", CmdPtr, size);

	ret = sbi_host_to_card(priv, MVMS_CMD, CmdPtr, size);
	priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
#ifndef BULVERDE_SDIO
	if(Adapter->IntCounter || Adapter->CurrentTxSkb)
		PRINTK("!!! PS: IntCounter=%d CurrentTxSkb=%p\n",
				Adapter->IntCounter,Adapter->CurrentTxSkb);
#endif

	if (ret) {
		printk("Host to Card Failed for Confirm Sleep\n");
	} else {
		//if (Adapter->PSMode != Wlan802_11PowerModeCAM)
		{
#ifdef BULVERDE_SDIO
			sdio_clear_imask(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
#endif
			OS_INT_DISABLE;
			if(!Adapter->IntCounter) {
#ifdef BULVERDE_SDIO
				ps_sleep_confirmed = 1;
#endif
				Adapter->PSState = PS_STATE_SLEEP;
#ifdef HOST_WAKEUP
				if (Adapter->bHostWakeupConfigured) {
					Adapter->bHostWakeupDevRequired = TRUE;
#if WIRELESS_EXT > 14
					send_iwevcustom_event(priv, "HWM_CFG_DONE.indication ");
#endif //WIRELESS_EXT
				}
#endif //HOST_WAKEUP
			}
			else
				PRINTK("After Sleep_Confirm:IntC=%d\n", Adapter->IntCounter);
			OS_INT_RESTORE;
		}
		PRINTK1("Sent Confirm Sleep command\n");
		PRINTK("+");
	}
	
	LEAVE();
	return ret;
}

void PSSleep(wlan_private *priv, u16 PSMode, int wait_option)
{
#ifndef AD_HOC_PS
	wlan_adapter   *Adapter = priv->adapter;
#endif

	ENTER();

	/* 
	 * PS is currently supported only in Infrastructure Mode 
	 * Remove this check if it is to be supported in IBSS mode also 
	 */
#ifndef AD_HOC_PS
	if (Adapter->InfrastructureMode != Wlan802_11Infrastructure) {
		PRINTK("PSSleep(): PS mode not supported in Ad-Hoc mode\n");
		LEAVE();
		return;
	}
#endif

	PrepareAndSendCommand(priv, HostCmd_CMD_802_11_PS_MODE,
			HostCmd_SubCmd_Enter_PS,
			HostCmd_OPTION_USE_INT | wait_option,
			0, HostCmd_PENDING_ON_NONE, &PSMode);

	LEAVE();
	return;
}

void PSWakeup(wlan_private *priv, int wait_option)
{
	WLAN_802_11_POWER_MODE LocalPSMode;

	ENTER();

	LocalPSMode = Wlan802_11PowerModeCAM;

	PRINTK1("PSWakeup() LocalPSMode = %d\n", LocalPSMode);

	PrepareAndSendCommand(priv, HostCmd_CMD_802_11_PS_MODE,
			HostCmd_SubCmd_Exit_PS,
			HostCmd_OPTION_USE_INT | wait_option,
			0, HostCmd_PENDING_ON_NONE, &LocalPSMode);

	LEAVE();
	return;
}

void PSConfirmSleep(wlan_private * priv, u16 PSMode)
{
	wlan_adapter   	*Adapter = priv->adapter;
#ifndef PS_PRESLEEP
#ifdef BULVERDE_SDIO
	int 		state = 0;
#endif
#endif
	int 		reject = 0;
 
	ENTER();

	if(priv->wlan_dev.dnld_sent) {
		reject = 1;
		PRINTK("D");
	}
	else if(Adapter->CurCmd) {
		reject = 1;
		PRINTK("C");
	}
#ifdef PS_PRESLEEP
	else if(Adapter->IntCounter > 0) {
#else
	// We should check IntCounter > 0 here after we solve the extra interrupt issue.
	else if(Adapter->IntCounter > 1) {
#endif
		reject = 1;
		PRINTK("I%d",Adapter->IntCounter);
	}
        else if (Adapter->CurrentTxSkb != NULL) {
#ifndef PS_PRESLEEP
		reject = 1;
#endif
		PRINTK("X%d",Adapter->IntCounter);
	}
#ifndef PS_PRESLEEP
#ifdef BULVERDE_SDIO
	else {
		state = sdio_check_idle_state(((mmc_card_t)
					((priv->wlan_dev).card))->ctrlr);
			if(state) {
				reject = 1;
				PRINTK("(%d)",state);
			}
	}
#endif
#endif

	if(reject) {
		PRINTK("-Sleep Request has been ignored\n");
		LEAVE();
		return;
	}

	{
		PRINTK1("In Power Save Mode, Sending PSConfirmSleep\n");

		if(SendConfirmSleep(priv, (u8 *) Adapter->PSConfirmSleep,
					sizeof(PS_CMD_ConfirmSleep)) == 0) {
		}
	}

#ifdef DEBUG
	Adapter->ConfirmCounter++;
#endif

	LEAVE();
}

void PSTxPending(wlan_private * priv, unsigned short NumOfPkts)
{
	PrepareAndSendCommand(priv, HostCmd_CMD_802_11_PS_MODE,
			HostCmd_SubCmd_TxPend_PS, HostCmd_OPTION_USE_INT, 0,
			HostCmd_PENDING_ON_NONE, &NumOfPkts);
}
#endif

#ifdef ADHOCAES
int SetupAdhocEncryptionKey(wlan_private *priv, PHostCmd_DS_802_11_PWK_KEY pEnc,
						int CmdOption, char *keyptr)
{
	ENTER();

	memset(pEnc->TkipEncryptKey, 0, sizeof(pEnc->TkipEncryptKey));

	pEnc->Action = CmdOption;	

	if (CmdOption == HostCmd_ACT_SET_AES) {
		memcpy(pEnc->TkipEncryptKey, keyptr, 
						sizeof(pEnc->TkipEncryptKey));
	}

	LEAVE();
	return 0;

}
#endif
