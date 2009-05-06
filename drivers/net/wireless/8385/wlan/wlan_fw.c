/*
 * File : wlan_fw.c 
 *
 * Motorola Confidential Proprietary
 * Copyright (C) Motorola 2005. All rights reserved.
 */

#include	"include.h"

#ifdef DEBUG
void break1(void)
{
}
#endif

#ifdef THROUGHPUT_TEST
u8	G_buf[1600];
#endif /* THROUGHPUT_TEST */

#ifdef READ_HELPER_FW
u8 *helper_name=NULL;
u8 *fw_name=NULL;

MODULE_PARM( helper_name, "s");
MODULE_PARM( fw_name, "s" );
#endif


/*
 * Initialize the Firmware
 */
int wlan_init_fw(wlan_private * priv)
{
	int             ret = 0;

	ENTER();
	
 	/* Allocate adapter structure */
        if ((ret = wlan_allocate_adapter(priv)) != WLAN_STATUS_SUCCESS) {
                goto done;
        }
	
	wlan_init_adapter(priv);

	init_sync_objects(priv);

	if ((ret = wlan_setup_station_hw(priv)) < 0) {
		goto done;
	}

#ifndef THROUGHPUT_TEST

	/*
	 * Get the supported Data Rates 
	 */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_DATA_RATE,
			HostCmd_ACT_GET_TX_RATE, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret)
		goto done;
#else
	memset(G_buf, 0xaa, 1600);
#endif /* THROUGHPUT_TEST */

	ret = 0;

#ifdef ENABLE_802_11D
	wlan_init_11d( priv );
#endif

#ifdef ENABLE_802_11H_TPC
	PRINTK2("11HTPC: init 11H TPC\n");
	wlan_802_11h_init( priv );
#endif

done:
	LEAVE();
	return ret;
}

int wlan_setup_station_hw(wlan_private * priv)
{
	int 		ret = 0;
	u32           ulCnt;
	int             ixStatus;
	wlan_adapter   *adapter = priv->adapter;
	u16		dbm = MRVDRV_MAX_TX_POWER;
#ifndef THROUGHPUT_TEST
 	u8           TempAddr[MRVDRV_ETH_ADDR_LEN];
#endif /* THROUGHPUT_TEST */
	
#ifdef READ_HELPER_FW
	u8	*ptr = NULL;
	u32	len = 0;
#endif
	
    	ENTER();
    
	sbi_disable_host_int(priv, HIM_DISABLE);
#ifdef READ_HELPER_FW
	PRINTK2("helper:%s fw:%s \n", helper_name, fw_name );
	adapter->fmimage = NULL;
	adapter->fmimage_len = 0;
	adapter->helper = NULL;
	adapter->helper_len = 0;

	if ( helper_name != NULL ) {
		if( fw_read( helper_name, &ptr, &len ) != WLAN_STATUS_FAILURE ) {
			adapter->helper = ptr;
			adapter->helper_len = len;
			PRINTK2( "helper read success:%x len=%x\n", 
					(unsigned int)ptr, len);
		}
		else {
			printk( "<1>helper read fail. Default Helper is used\n");
		}
	}


	if ( fw_name != NULL ) {
		if( fw_read( fw_name, &ptr, &len ) != WLAN_STATUS_FAILURE ) {
			adapter->fmimage = ptr;
			adapter->fmimage_len = len;
			PRINTK2("fw read success:%x len=%x\n", 
					(unsigned int)ptr, len);
		}
		else {
			printk("<1>fw read fail. Default FW is used\n");
		}
	}
#endif

#ifdef	HELPER_IMAGE
	ixStatus = sbi_prog_helper(priv);
#else
	ixStatus = sbi_prog_firmware(priv);
#endif


#ifdef READ_HELPER_FW
	if ( adapter->helper != NULL ) { 
		kfree (	adapter->helper );
		PRINTK2("kfree helper\n");
	}

	if ( adapter->fmimage != NULL ) {
		kfree( adapter->fmimage );
		PRINTK2("kfree fmimage\n");
	}
#endif
	
	if (ixStatus < 0) {
		PRINTK1("Bootloader in invalid state!\n");
		/*
		 * we could do a hardware reset here on the card to try to
		 * restart the boot loader.
		 */
		return WLAN_STATUS_FAILURE;
	}
	
#ifdef	HELPER_IMAGE
	/*
	 * Download the main firmware via the helper firmware 
	 */
	if (sbi_download_wlan_fw(priv)) {
		PRINTK("Wlan Firmware download failed!\n");
		return WLAN_STATUS_FAILURE;
	}
#endif

	if (sbi_verify_fw_download(priv)) {
		PRINTK("Firmware failed to be active in time!\n");
		return WLAN_STATUS_FAILURE;
	}

#define RF_REG_OFFSET 0x07
#define RF_REG_VALUE  0xc8

	/*
	 * Attach the priv structure to mspio core to pass as argument while
	 * invoking user_isr when there are interrupts 
         */
    	// priv->wlan_dev.card->user_arg = priv->wlan_dev.netdev;

    	ulCnt = 0;

    	/*
    	 * TODO: Reduce this as far as possible 
         */

  	sbi_enable_host_int(priv, HIM_ENABLE);

#ifndef THROUGHPUT_TEST

#ifdef GSPI83xx
	g_bus_mode_reg = 0x06 \
			| B_BIT_4; /* This will be useful in B1 GSPI h/w
		       		      will not affect B0 as this bit is 
		      		      reserved. */
	g_dummy_clk_ioport = 1;
	g_dummy_clk_reg = 1;
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_GSPI_BUS_CONFIG,
			    HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT,
			    0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	PRINTK("Setting for %d %d\n", g_dummy_clk_ioport, g_dummy_clk_reg);
#endif /* GSPI83xx */

	/*
	 * Read MAC address from HW
	 * permanent address is not read first before it gets set if
	 * NetworkAddress entry exists in the registry, so need to read it
	 * first
	 * 
	 * use permanentaddr as temp variable to store currentaddr if
	 * NetworkAddress entry exists
	 */
	memmove(TempAddr, adapter->CurrentAddr, MRVDRV_ETH_ADDR_LEN);
	memset(adapter->CurrentAddr, 0xff, MRVDRV_ETH_ADDR_LEN);
	memset(adapter->PermanentAddr, 0xff, MRVDRV_ETH_ADDR_LEN);

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_GET_HW_SPEC, 
			0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	if (TempAddr[0] != 0xff) {
		/*
		 * an address was read, so need to send GET_HW_SPEC again to
		 * set the soft mac
		 */

		memmove(adapter->CurrentAddr, TempAddr, MRVDRV_ETH_ADDR_LEN);

		ret = PrepareAndSendCommand(priv, HostCmd_CMD_GET_HW_SPEC,
			0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}
	}

	SetMacPacketFilter(priv);

#ifdef PCB_REV4
	wlan_read_write_rfreg(priv);
#endif

	//set the max tx power
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RF_TX_POWER,
		HostCmd_ACT_TX_POWER_OPT_SET_LOW, HostCmd_OPTION_USE_INT, 0,
		HostCmd_PENDING_ON_GET_OID, &dbm);

	if (ret) {
		LEAVE();
		return ret;
	}

#ifdef FW_WAKEUP_METHOD
	PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_FW_WAKEUP_METHOD, HostCmd_ACT_GET,
		HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP, 0,
		HostCmd_PENDING_ON_NONE, &priv->adapter->fwWakeupMethod);
#endif

#endif /* THROUGHPUT_TEST */

	LEAVE();

	return WLAN_STATUS_SUCCESS;
}

/*
 *              ResetSingleTxDone 
 */
int ResetSingleTxDoneAck(wlan_private * priv)
{
	return 0;
}

/*
 * This function is called after a card has been probed and passed the test.
 * We then need to set up the device (mv_mspio_dev_t) data structure and
 * read/write all relevant register ports on the card.
 */

int wlan_register_dev(wlan_private * priv)
{
  	int             ret = 0;
	
	ENTER();

	/*
	 * Initialize the private structure 
	 */
	strncpy(priv->wlan_dev.name, "wlan0", sizeof(priv->wlan_dev.name));
	priv->wlan_dev.ioport = 0;
	priv->wlan_dev.upld_rcv = 0;
	priv->wlan_dev.upld_typ = 0;
	priv->wlan_dev.upld_len = 0;

	LEAVE();

	return ret;
}

int wlan_unregister_dev(wlan_private * priv)
{
	ENTER();

	LEAVE();
	return 0;
}
int wlan_allocate_adapter(wlan_private * priv)
{
	u32           ulBufSize;
	PWLAN_802_11_BSSID pTempBSSIDList;
	wlan_adapter   *Adapter = priv->adapter;

	/* Allocate buffer to store the BSSID list */
	ulBufSize = sizeof(WLAN_802_11_BSSID) * MRVDRV_MAX_BSSID_LIST;
	if (!(pTempBSSIDList = kmalloc(ulBufSize, GFP_KERNEL))) {
		wlan_free_adapter(priv);
		return -1;
	}

	Adapter->BSSIDList = pTempBSSIDList;
	memset(Adapter->BSSIDList, 0, ulBufSize);

	spin_lock_init(&Adapter->QueueSpinLock);

	/* Create the command buffers */
	AllocateCmdBuffer(priv);

#ifdef PS_REQUIRED
	AllocatePSConfirmBuffer(priv);
#endif

#ifdef	PASSTHROUGH_MODE
	spin_lock_init(&Adapter->PTDataLock);
	INIT_LIST_HEAD(&Adapter->PTDataQ);
	INIT_LIST_HEAD(&Adapter->PTFreeDataQ);
	init_waitqueue_head(&Adapter->wait_PTData);

	if ((AllocatePassthroughList(priv))) {
		wlan_free_adapter(priv);
		return -1;
	}
#endif

	return 0;
}

void wlan_free_adapter(wlan_private * priv)
{
	wlan_adapter   *Adapter = priv->adapter;
	
	ENTER();

	if (!Adapter)
		return;

	/* Free allocated data structures */
	// FreeSingleTxBuffer(Adapter);

	FreeCmdBuffer(priv);

	PRINTK("Free CommandTimer\n");
	if (Adapter->CommandTimerIsSet) {
		CancelTimer(&Adapter->MrvDrvCommandTimer);
		FreeTimer(&Adapter->MrvDrvCommandTimer);
		Adapter->CommandTimerIsSet = FALSE;
	}

	PRINTK("Free MrvDrvTimer\n");
	if (Adapter->TimerIsSet) {
		CancelTimer(&Adapter->MrvDrvTimer);
		FreeTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}

#ifdef BG_SCAN
	if (Adapter->bgScanConfig) {
		kfree(Adapter->bgScanConfig);
		Adapter->bgScanConfig = NULL;
	}
#endif /* BG_SCAN */

#ifdef PASSTHROUGH_MODE
	FreePassthroughList(Adapter);
#endif

	/* The following is needed for Threadx and are dummy in linux */
	OS_FREE_LOCK(&Adapter->CurrentTxLock);
	OS_FREE_LOCK(&Adapter->QueueSpinLock);

	PRINTK("Free BSSIDList\n");
	if (Adapter->BSSIDList) {
		kfree(Adapter->BSSIDList);
		Adapter->BSSIDList = NULL;
	}

	PRINTK("Free PS_CMD_ConfirmSleep\n");
	if (Adapter->PSConfirmSleep) {
		kfree(Adapter->PSConfirmSleep);
		Adapter->PSConfirmSleep = NULL;
	}
	PRINTK("free Adapter\n");
	/* Free the adapter object itself */
	kfree(Adapter);
	priv->adapter = NULL;
	LEAVE();
}

void wlan_init_adapter(wlan_private * priv)
{
	wlan_adapter	*Adapter = priv->adapter;
	int		i;
#ifdef BG_SCAN
	static u8 dfltBgScanCfg[] = {
		0x01, 0x00,                                  	//Action
 		0x00,						//Enable
		0x03,						//BssType
		0x0E,						//ChannelsPerScan
		0x00,						//DiscardWhenFull
		0x00, 0x00,                                     //Reserved
		0x64, 0x00, 0x00, 0x00,				//Scan Interval				
		0x01, 0x00, 0x00, 0x00,		                //StoreCondition
		0x01, 0x00, 0x00, 0x00,                         //ReportConditions
		0x0E, 0x00,					//MaxScanResults

		0x01, 0x01, 0x4d, 0x00,                         //ChannelList  
		0x00, 0x0b, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x01, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x02, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x03, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x04, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x05, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x06, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x07, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x08, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x09, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x00, 0x0a, 0x00, 0x06, 0x00, 0x64, 0x00
/*
		0x01, 0x01, 0x5b, 0x00,
		0x01, 0x24, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x28, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x2c, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x30, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x34, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x38, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x3c, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x40, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x95, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x99, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0x9d, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0xa1, 0x00, 0x06, 0x00, 0x64, 0x00,
		0x01, 0xa5, 0x00, 0x06, 0x00, 0x64, 0x00
*/
	};
#endif /* BG_SCAN */

	Adapter->ScanProbes = 0;

	Adapter->bcn_avg_factor  = DEFAULT_BCN_AVG_FACTOR;
	Adapter->data_avg_factor = DEFAULT_DATA_AVG_FACTOR;

	/* Timer variables */
	Adapter->TimerInterval = MRVDRV_DEFAULT_TIMER_INTERVAL;
	Adapter->isCommandTimerExpired = FALSE;

	/* ATIM params */
  	Adapter->AtimWindow = 0;
	Adapter->ATIMEnabled = FALSE;

	/* Operation characteristics */
	/* Remove: CurrentLookAhead doesn't seem to be used anywhere. ???? */
    	Adapter->CurrentLookAhead = (u32) MRVDRV_MAXIMUM_ETH_PACKET_SIZE -
							MRVDRV_ETH_HEADER_SIZE;
	Adapter->MediaConnectStatus = WlanMediaStateDisconnected;
#ifdef WPRM_DRV
	WPRM_DRV_TRACING_PRINT();
	wprm_traffic_meter_exit(priv);
#endif
	Adapter->LinkSpeed = MRVDRV_LINK_SPEED_1mbps;
	memset(Adapter->CurrentAddr, 0xff, MRVDRV_ETH_ADDR_LEN);

	/* Status variables */
	Adapter->HardwareStatus = WlanHardwareStatusInitializing;

	/* scan type */
    	Adapter->ScanType = HostCmd_SCAN_TYPE_ACTIVE;

	/* scan mode */
	Adapter->ScanMode = HostCmd_BSS_TYPE_ANY;

    	/* 802.11 specific */
	Adapter->SecInfo.WEPStatus = Wlan802_11WEPDisabled;
	for(i = 0; i < sizeof(Adapter->WepKey)/sizeof(Adapter->WepKey[0]); i++)
		memset(&Adapter->WepKey[i], 0, sizeof(MRVL_WEP_KEY));
	Adapter->CurrentWepKeyIndex = 0;
	Adapter->SecInfo.AuthenticationMode = Wlan802_11AuthModeOpen;
	Adapter->SecInfo.Auth1XAlg = WLAN_1X_AUTH_ALG_NONE;
	Adapter->SecInfo.EncryptionMode = CIPHER_NONE;
#ifdef ADHOCAES
	Adapter->AdhocAESEnabled = FALSE;
#endif
	Adapter->InfrastructureMode = Wlan802_11Infrastructure;
	Adapter->ulNumOfBSSIDs = 0;
	Adapter->ulCurrentBSSIDIndex = 0;
	Adapter->ulAttemptedBSSIDIndex = 0;
	Adapter->bAutoAssociation = FALSE;
	// Adapter->LastRSSI = MRVDRV_RSSI_DEFAULT_NOISE_VALUE;

	Adapter->bIsScanInProgress = FALSE;
	Adapter->bIsAssociationInProgress = FALSE;
	Adapter->bIsPendingReset = FALSE;

	/* SDCF: MPS/20041012: Is this really required */
	Adapter->HisRegCpy |= HIS_TxDnLdRdy; // MPS

	memset(&Adapter->CurBssParams, 0, sizeof(Adapter->CurBssParams));

	Adapter->TxRetryCount = 4;

	/* PnP and power profile */
	Adapter->SurpriseRemoved = FALSE;

	Adapter->CurrentPacketFilter =
		HostCmd_ACT_MAC_RX_ON | HostCmd_ACT_MAC_TX_ON;

	Adapter->SetSpecificScanSSID = FALSE;	// default

	/*Default: broadcast*/
	memset( Adapter->SpecificScanBSSID, 0xff, ETH_ALEN );

#ifdef PROGRESSIVE_SCAN
	Adapter->ChannelsPerScan = MRVDRV_CHANNELS_PER_SCAN;
#endif

	Adapter->RadioOn = RADIO_ON;
	/* This is added for reassociation on and off */
	Adapter->Reassoc_on = TRUE;
	Adapter->TxAntenna = RF_ANTENNA_2;
	Adapter->RxAntenna = RF_ANTENNA_AUTO;

	Adapter->Is_DataRate_Auto = TRUE;
	
	Adapter->FragThsd = MRVDRV_FRAG_MAX_VALUE;
	Adapter->RTSThsd = MRVDRV_RTS_MAX_VALUE;
#define SHORT_PREAMBLE_ALLOWED		1
	//Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;

	// set default value of capInfo.
	memset(&Adapter->capInfo, 0, sizeof(Adapter->capInfo));
	Adapter->capInfo.ShortPreamble = SHORT_PREAMBLE_ALLOWED;

	Adapter->Channel = DEFAULT_CHANNEL;	// default
	Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL;	// default

#ifdef	PS_REQUIRED
	/* **** POWER_MODE ***** */
	Adapter->PSMode = Wlan802_11PowerModeCAM;
	Adapter->MultipleDtim = MRVDRV_DEFAULT_MULTIPLE_DTIM;

	Adapter->ListenInterval = MRVDRV_DEFAULT_LISTEN_INTERVAL;
	
	Adapter->PSState = PS_STATE_FULL_POWER;
	Adapter->NeedToWakeup = FALSE;
	Adapter->LocalListenInterval = 0;	/* default value in firmware will be used */
#ifdef FW_WAKEUP_METHOD
	Adapter->fwWakeupMethod = WAKEUP_FW_UNCHANGED;
#endif
#endif

#ifdef DEEP_SLEEP
	Adapter->IsDeepSleep = FALSE;
	Adapter->IsDeepSleepRequired = FALSE;
#endif // DEEP_SLEEP_CMD

#ifdef HOST_WAKEUP
	Adapter->bHostWakeupDevRequired = FALSE;
	Adapter->bHostWakeupConfigured = FALSE;
	Adapter->WakeupTries = 0;
#endif // HOST_WAKEUP

	/* **** Extended Scan ***** */
	Adapter->ExtendedScan = 0;	// Off by default
	Adapter->pExtendedScanParams = NULL;
	Adapter->BeaconPeriod = 100;

#ifdef PS_REQUIRED
	Adapter->PreTbttTime = 0;
#endif

	Adapter->DataRate = 0; // Initially indicate the rate as auto 

#ifdef ADHOC_GRATE
	Adapter->adhoc_grate_enabled=FALSE;
#endif

	Adapter->IntCounter = Adapter->IntCounterSaved = 0;
#ifdef WMM
	memset(&Adapter->wmm, 0, sizeof(WMM_DESC));
	for (i = 0; i < MAX_AC_QUEUES; i++)
		INIT_LIST_HEAD(&Adapter->wmm.TxSkbQ[i]);
#ifdef WMM_UAPSD
	Adapter->wmm.gen_null_pkg = TRUE; /*Enable NULL Pkg generation*/
#endif

#endif /* WMM */
	INIT_LIST_HEAD(&Adapter->RxSkbQ);
#ifdef BG_SCAN
	Adapter->bgScanConfig = kmalloc(sizeof(dfltBgScanCfg), GFP_KERNEL);
	memcpy(Adapter->bgScanConfig, dfltBgScanCfg, sizeof(dfltBgScanCfg));
	Adapter->bgScanConfigSize = sizeof(dfltBgScanCfg);
#endif /* BG_SCAN */

#ifdef ADHOCAES
	init_waitqueue_head(&Adapter->cmd_EncKey);
#endif

#ifdef WPA
    	Adapter->EncryptionStatus = Wlan802_11WEPDisabled;

	/* setup association information buffer */
	{
		PWLAN_802_11_ASSOCIATION_INFORMATION pAssoInfo;

		pAssoInfo = (PWLAN_802_11_ASSOCIATION_INFORMATION)
			Adapter->AssocInfoBuffer;

		// assume the buffer has already been zero-ed
		// no variable IE, so both request and response IE are
		// pointed to the end of the buffer, 4 byte aligned

		pAssoInfo->OffsetRequestIEs =
			pAssoInfo->OffsetResponseIEs =
			pAssoInfo->Length =
			((sizeof(WLAN_802_11_ASSOCIATION_INFORMATION) + 3) / 4)
			* 4;
    	}
#endif				// #ifdef WPA
	
	spin_lock_init(&Adapter->CurrentTxLock);

	Adapter->CurrentTxSkb = NULL;
	Adapter->PktTxCtrl = 0; //init value for TxControl ioctl
	
	return;
}

void MrvDrvCommandTimerFunction(void *FunctionContext)
{
	// get the adapter context
	wlan_private	*priv = (wlan_private *)FunctionContext;
	wlan_adapter	*Adapter = priv->adapter;
	CmdCtrlNode	*pTempNode;
	u32		flags;

	ENTER();

	PRINTK1("No response CommandTimerFunction ON\n");
	Adapter->CommandTimerIsSet = FALSE;

	pTempNode = Adapter->CurCmd;

	if(pTempNode == NULL){
		PRINTK("PTempnode Empty\n");
		return ;
	}

	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
	Adapter->CurCmd = NULL;
	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

//#ifdef BULVERDE_SDIO
//	sdio_enable_SDIO_INT();
//#endif

	PRINTK1("Re-sending same command as it timeout...!\n");
	QueueCmd(Adapter,pTempNode, FALSE);

	wake_up_interruptible(&priv->MainThread.waitQ);

	LEAVE();
	return;
}

int init_sync_objects(wlan_private * priv)
{
	wlan_adapter   *Adapter = priv->adapter;

	InitializeTimer(&Adapter->MrvDrvCommandTimer,
			MrvDrvCommandTimerFunction, priv);
	Adapter->CommandTimerIsSet = FALSE;

	/* 
	 * Initialize the timer for packet sending 
	 * For the reassociation
	 */
	InitializeTimer(&Adapter->MrvDrvTimer, MrvDrvTimerFunction, priv);
 	Adapter->TimerIsSet = FALSE;

	return WLAN_STATUS_SUCCESS;
}

int wlan_is_tx_download_ready(wlan_private * priv)
{
	int             rval;
	u8              cs;

	ENTER1();
	if ((rval = sbi_get_int_status(priv, &cs)) < 0)
		goto done;

	rval = (cs & HIS_TxDnLdRdy) ? 0 : -EBUSY;

	if (rval)
		//sbi_reenable_host_interrupt(priv, HIS_TxDnLdRdy);
		sbi_reenable_host_interrupt(priv, 0);
done:
	LEAVE1();
	return rval;
}

void MrvDrvTimerFunction(void *FunctionContext)
{
	/* get the adapter context */
	wlan_private   *priv = (wlan_private *) FunctionContext;
	wlan_adapter   *Adapter = priv->adapter;
	OS_INTERRUPT_SAVE_AREA;	/* Needed for Threadx; Dummy in linux */

	ENTER();

	PRINTK1("Timer Function triggered\n");

    	PRINTK1("Media Status = 0x%x\n", Adapter->MediaConnectStatus);

#ifdef PS_REQUIRED
	if (Adapter->PSState != PS_STATE_FULL_POWER) {
		Adapter->TimerIsSet = TRUE;
		ModTimer(&Adapter->MrvDrvTimer, (1 * HZ));
		PRINTK1("MrvDrvTimerFunction(PSState=%d) waiting" 
				"for Exit_PS done\n",Adapter->PSState);
		LEAVE();
		return;
	}
#endif

	PRINTK1("Waking Up the Event Thread\n");
	OS_INT_DISABLE;	
	Adapter->EventCounter++;
	OS_INT_RESTORE;	

	wake_up_interruptible(&priv->ReassocThread.waitQ);

	LEAVE();
	return;
}

#ifdef PCB_REV4
void wlan_read_write_rfreg(wlan_private *priv)
{
	wlan_offset_value offval;
	offval.offset = RF_REG_OFFSET;
	offval.value  = RF_REG_VALUE;

	PrepareAndSendCommand( priv, HostCmd_CMD_RF_REG_ACCESS,
			HostCmd_ACT_GEN_GET,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, &offval);

	offval.value |= 0x1;

	PrepareAndSendCommand(priv, HostCmd_CMD_RF_REG_ACCESS,
			HostCmd_ACT_GEN_SET,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, &offval);
}
#endif

