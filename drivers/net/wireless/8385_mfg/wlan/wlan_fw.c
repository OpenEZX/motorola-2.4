/*
 * File : wlan_fw.c 
 */

#ifdef	linux
#include	<linux/module.h>
#include	<linux/kernel.h>
#include	<linux/net.h>
#include	<linux/netdevice.h>
#include	<linux/skbuff.h>
#include	<linux/etherdevice.h>
#include	<asm/irq.h>
#endif

#include	"os_headers.h"
#include	"wlan.h"
#include	"wlan_fw.h"
#include	"wlan_wext.h"


#ifdef DEBUG
void break1(void)
{
}
#endif

int             wlan_setup_station_hw(wlan_private * priv);
int             wlan_allocate_adapter(wlan_private * priv);
void            wlan_init_adapter(wlan_private * priv);

#ifdef PASSTHROUGH_MODE
int             AllocatePassthroughList(wlan_private * priv);
void            FreePassthroughList(wlan_adapter * Adapter);
#endif
int             init_sync_objects(wlan_private * priv);
int             wlan_dnld_ready(wlan_private * priv);

#ifdef PS_REQUIRED
void            MrvDrvTxPktTimerFunction(void *FunctionContext);
void            MrvDrvCheckTxReadyTimerFunction(void *FunctionContext);
#endif

void            MrvDrvTimerFunction(void *FunctionContext);

#ifdef WPA
void            DisplayTkip_MicKey(HostCmd_DS_802_11_PWK_KEY * pCmdPwk);
#endif

#ifdef ENABLE_802_11H_TPC
static int  wlan_802_11h_init( wlan_private *priv );
#endif

#ifdef THROUGHPUT_TEST
u8	G_buf[1600];
#endif /* THROUGHPUT_TEST */

#ifdef GSPI8385
extern int g_bus_mode_reg;
extern int g_dummy_clk_ioport;
extern int g_dummy_clk_reg;
#endif /* GSPI8385 */

/*
 * Initialize the Firmware
 */
int wlan_init_fw(wlan_private * priv)
{
	int             ret = 0;

	ENTER();
	
	wlan_allocate_adapter(priv);
	
	wlan_init_adapter(priv);

	init_sync_objects(priv);

	if ((ret = wlan_setup_station_hw(priv)) < 0) {
		goto done;
	}

#if !defined (THROUGHPUT_TEST) && !defined(MFG_CMD_SUPPORT)

	/*
	 * Get the supported Data Rates 
	 */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_DATA_RATE,
			HostCmd_ACT_GET_TX_RATE, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret)
		goto done;
#endif // THROUGHPUT_TEST || MFG_CMD_SUPPORT

#ifdef THROUGHPUT_TEST
	memset(G_buf, 0xaa, 1600);
#endif // THROUGHPUT_TEST 

	ret = 0;

done:
#ifdef ENABLE_802_11D
	PRINTK2("Call sm_transfer with CMD_INIT_11D\n");
	wlan_802_11d_sm_transfer( priv, CMD_INIT_11D );
#endif

#ifdef ENABLE_802_11H_TPC
	PRINTK2("11HTPC: init 11H TPC\n");
	wlan_802_11h_init( priv );
#endif
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
#if !defined (THROUGHPUT_TEST) && !defined(MFG_CMD_SUPPORT) 
 	u8           TempAddr[MRVDRV_ETH_ADDR_LEN];
#endif /* THROUGHPUT_TEST && MFG_CMD_SUPPORT */
	
    	ENTER();
    
	sbi_disable_host_int(priv, HIM_DISABLE);

#ifdef	HELPER_IMAGE
	ixStatus = sbi_prog_helper(priv);
#else
	ixStatus = sbi_prog_firmware(priv);
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

//  	mdelay(500);

  	sbi_enable_host_int(priv, HIM_ENABLE);

#ifndef THROUGHPUT_TEST

#ifdef GSPI8385
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
#endif /* GSPI8385 */

#if !defined (THROUGHPUT_TEST) && !defined(MFG_CMD_SUPPORT)
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
			    0, HostCmd_OPTION_USE_INT, (WLAN_OID) 0,
			    HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	/* wait for command to come back */
	wlan_sched_timeout(100);

	if (TempAddr[0] != 0xff) {
		/*
		 * an address was read, so need to send GET_HW_SPEC again to
		 * set the soft mac
		 */

		memmove(adapter->CurrentAddr, TempAddr, MRVDRV_ETH_ADDR_LEN);

		ret = PrepareAndSendCommand(priv, HostCmd_CMD_GET_HW_SPEC,
				0, HostCmd_OPTION_USE_INT, 0,
				HostCmd_PENDING_ON_NONE, NULL);

		if (ret) {
			LEAVE();
			return ret;
		}

		/* wait for command to come back */
		wlan_sched_timeout(10);
	}

	SetMacPacketFilter(priv);
#endif /* THROUGHPUT_TEST && MFG_CMD_SUPPORT */

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
#ifdef ENABLE_802_11H_TPC

int wlan_802_11h_tpc_enabled( wlan_private * priv )
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11h_tpc_t		*state = &Adapter->State11HTPC;
	return ( (state->Enable11HTPC == TRUE)? 1:0 );
}

int wlan_802_11h_tpc_enable( wlan_private * priv, BOOLEAN flag )
{
/* flag== TRUE, 11H_TPC is enable, otherwise, disable */

	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11h_tpc_t		*state = &Adapter->State11HTPC;
	int ret;
	int enable = (flag == TRUE )? 1:0;
	
	ENTER();

	/* send cmd to FW to enable/disable 11H TPC fucntion in FW */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
		0, HostCmd_OPTION_USE_INT, OID_802_11H_TPC_ENABLE, 
		HostCmd_PENDING_ON_SET_OID, &enable);

	if (ret) {
		LEAVE();
		return ret;
	}

	state->Enable11HTPC = flag;
	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

static int  wlan_802_11h_init( wlan_private *priv )
/*Init the 802.11h tpc related parameter*/
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11h_tpc_t		*state = &Adapter->State11HTPC;
	int ret;

	ENTER();

	state -> UsrDefPowerConstraint= MRV_11H_TPC_POWERCONSTAINT;
	state -> MinTxPowerCapability = MRV_11H_TPC_POWERCAPABILITY_MIN;
	state -> MaxTxPowerCapability = MRV_11H_TPC_POWERCAPABILITY_MAX;

	ret = wlan_802_11h_tpc_enable( priv, TRUE );  /* enable 802.11H TPC */
	if (ret) {
		LEAVE();
		return ret;
	}

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

#endif /*ENABLE_802_11H_TPC*/

#ifdef ENABLE_802_11D
/*Functions for handling the state-machine to support 802.11d*/
extern int wlan_set_domainreg( wlan_private *priv, u8 region, u8 band );

/*SM(State Machine) const is used for StateMachine const*/
#define SM_802_11D_ENABLE	1
#define SM_802_11D_DISABLE	0

static int wlan_802_11d_sm_enable( wlan_private * priv, int flag )
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11d_state_t		*state = &Adapter->State11D;
	int ret;
	int enable = flag;
	
	ENTER();

	state->Enable11D 	= (flag==SM_802_11D_ENABLE)?TRUE:FALSE;

	/* send cmd to FW to enable/disable 11D fucntion in FW */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
		0, HostCmd_OPTION_USE_INT, OID_802_11D_ENABLE, 
		HostCmd_PENDING_ON_SET_OID, &enable);

	if (ret) {
		LEAVE();
		return ret;
	}

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

/*Reset Domain Regulatory Data Structure*/
static void wlan_802_11d_sm_domain_reg_init( wlan_private * priv )
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11d_state_t		*state = &Adapter->State11D;
	
	ENTER();

	memset( &Adapter->DomainReg, 0, sizeof(wlan_802_11d_domain_reg_t) );

	state->DomainRegValid 	= FALSE; /*valid DomainReg Data*/
	LEAVE();
}

/* Load DomainReg with Default RegionCode */
extern  int wlan_set_regionreg( wlan_private * priv, u8 region, u8 band );
static void wlan_802_11d_sm_set_usr_domain_reg( wlan_private * priv )
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11d_state_t		*state = &Adapter->State11D;
	
	ENTER();

	memset( &Adapter->DomainReg, 0, sizeof(wlan_802_11d_domain_reg_t) );

	PRINTK2("11D: RegionCode=%d\n", Adapter->RegionCode );
	wlan_set_domainreg(priv, Adapter->RegionCode, 
#ifdef MULTI_BANDS
	Adapter->config_bands
#else
	0	/*G_RATE*/
#endif
	);

	state->DomainRegValid 	= TRUE;
	/* valid DomainReg Data */
	LEAVE();
}

/*State machine init: Set the initial value for State Machine*/
static int wlan_802_11d_sm_init( wlan_private * priv )
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11d_state_t		*state = &Adapter->State11D;
	int ret;

	ENTER();
	
	wlan_802_11d_sm_domain_reg_init( priv );
	memset( state, 0, sizeof( wlan_802_11d_state_t ) );
	state->Status = UNKNOWN_11D;
	state->HandleNon11D = TRUE;

	state->AssocAdhocAllowed = FALSE;
	
	wlan_802_11d_sm_set_usr_domain_reg( priv );

	ret = wlan_802_11d_sm_enable( priv, SM_802_11D_ENABLE );
	if (ret) {
		LEAVE();
		return ret;
	}
	return WLAN_STATUS_SUCCESS;
}

/*State machine state change*/
int wlan_802_11d_sm_transfer( wlan_private *priv, wlan_802_11d_action init_action )
{
	wlan_adapter	*Adapter = priv->adapter;
	wlan_802_11d_state_t	*state = &Adapter->State11D;
	wlan_802_11d_status_t	*status = &state->Status;
	int ret = 0;
	wlan_802_11d_action 	action = init_action;

	ENTER();
	while ( action != CMD_YIELD_11D ) {
		switch ( action ) {
			case CMD_INIT_11D:  /*This is the first sm cmd. set sm to initial state*/
				action = CMD_YIELD_11D;
				wlan_802_11d_sm_init(priv);
				PRINTK2("CMD_INIT_11D:Status=%d\n", *status);
				break;
			case CMD_RESET_11D: /* Return back to UNKNOWN state: used to */
				action = CMD_YIELD_11D;
				PRINTK1("CMD_RESET_11D\n");
				*status = UNKNOWN_11D;
				break;
			case CMD_SET_DOMAIN_11D:
				action = CMD_YIELD_11D;
				PRINTK2("CMD_SET_DOMAIN\n");
//				if ( *status == KNOWN_UNSET_11D ) {
					PRINTK2("CMD_SET_DOMAIN_11D::KNOWN_UNSET\n");
					if ( state->DomainRegValid == TRUE ) {
					 	ret = PrepareAndSendCommand(priv, 
							HostCmd_CMD_802_11D_DOMAIN_INFO,
			    				HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT,
			    				0, HostCmd_PENDING_ON_NONE, NULL);
						if ( ret ) {
							printk("Err:CMD_SET_DOMAIN KNOWN_UNSET:set domainInfo to FW\n");
							action = CMD_RESET_11D;
						}
						else {
							*status = KNOWN_SET_11D;
						}
					}

//				}
				break;
			case CMD_DISABLE_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_DISABLE_11D\n");
				ret = wlan_802_11d_sm_enable( priv, SM_802_11D_DISABLE);
				if (ret) {
					PRINTK1("Err:CMD_DISABLE_11D\n");
					action = CMD_RESET_11D;
				}
				else {
					*status = DISABLED_11D;
				}
				break;
			case CMD_ENABLE_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_ENABLE_11D\n");
				wlan_802_11d_sm_domain_reg_init(priv);
				ret = wlan_802_11d_sm_enable( priv, SM_802_11D_ENABLE);
				if ( ret ) {
					PRINTK1("Err:CMD_DISABLE_11D\n");
					action = CMD_RESET_11D;
				}
				else {
					*status = UNKNOWN_11D;
				}
				break;
			case CMD_ALLOW_USR_REGION_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_ALLOW_USR_REGION_11D\n");
				state->HandleNon11D = TRUE;
				*status = UNKNOWN_11D;
				wlan_802_11d_sm_set_usr_domain_reg(priv);
				break;
			case CMD_DISALLOW_USR_REGION_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_DISALLOW_USR_REGION_11D\n");
				state->HandleNon11D = FALSE;
				*status = UNKNOWN_11D;
				wlan_802_11d_sm_domain_reg_init( priv );
				break;
			case CMD_SET_REGION_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_SET_REGION_11D\n");

/* 				ret = wlan_802_11d_sm_enable( priv, SM_802_11D_DISABLE );
				if ( ret ) {
					action = CMD_RESET_11D;
					break;
				}

*/
				wlan_802_11d_sm_set_usr_domain_reg(priv);
				
				/*Dnld to FW*/
/*				ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11D_DOMAIN_INFO,
					HostCmd_ACT_SET, HostCmd_OPTION_USE_INT,
					0, HostCmd_PENDING_ON_SET_OID, NULL);
			    	
				if ( ret ) {
					printk("Err:CMD_SET_REGION, Dnld to FW\n");
					action = CMD_RESET_11D;	
				}
				else {
					*status = DISABLED_11D;
					state->DomainRegValid = TRUE;
				}
*/
				break;
			case CMD_SCAN_RESP_COMPLIANT_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_SCAN_RESP_COMPLIANT_11D::status=%d\n", *status);
				if ( *status != DISABLED_11D ) {
					state->DomainRegValid = TRUE;
					*status = KNOWN_UNSET_11D;
					action = CMD_SET_DOMAIN_11D;
				}
				break;

			case CMD_SCAN_RESP_NON_COMPLIANT_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_SCAN_RESP_NON_COMPLIANT_11D\n");
				if ( *status == UNKNOWN_11D  ) {
					PRINTK1("CMD_SCAN_RESP_NON_COMPLIANT_11D:UNKNOW_11D\n");
					if ( state->HandleNon11D == TRUE ) {
						PRINTK1("CMD_SCAN_RSP_NON_COMPLIANT:UNKNOW:HandleNon11D[T]\n");
						wlan_802_11d_sm_set_usr_domain_reg(priv);
						*status = KNOWN_UNSET_11D;
						action = CMD_SET_DOMAIN_11D;
					}
					else {
						PRINTK1("CMD_SCAN_RESP_NON_COMPLIANT_11D:UNKNOW_11D:HandleNon11D[F]\n");
					}
				}
				break;
			case CMD_DISCONNECT_11D:
				action = CMD_YIELD_11D;
				PRINTK1("CMD_DISCONNECT_11D\n");
				wlan_802_11d_sm_set_usr_domain_reg(priv);
				action = CMD_RESET_11D;
				break;
			default:
				action = CMD_YIELD_11D;
				printk("Wrong Action Number: %d!\n", action);
				ret = -1;
				break;
		}
	}
	
	state->AssocAdhocAllowed = FALSE;
	if ( *status == DISABLED_11D || *status == KNOWN_SET_11D )
		state->AssocAdhocAllowed = TRUE;

	PRINTK2("AssocAdhocAllowed=%s status=%d \n", (state->AssocAdhocAllowed==TRUE)?"TRUE":"FALSE",*status);
	PRINTK2("DomainRegValid=%s\n", (state->DomainRegValid==TRUE)?"TRUE":"FALSE");
	PRINTK2("HandleNon11D=%s\n", (state->HandleNon11D==TRUE)?"TRUE":"FALSE");
	PRINTK2("Enable11D=%s\n", (state->Enable11D==TRUE)?"TRUE":"FALSE");
	PRINTK2("DomainReg CountryStr=%c%c\n", Adapter->DomainReg.CountryCode[0], Adapter->DomainReg.CountryCode[1]);
	PRINTK2("**********************After Transfer ************************\n");
	LEAVE();
	return ret;
}

#endif /*End of ENABLE_802_11D*/

int wlan_allocate_adapter(wlan_private * priv)
{
	u32           ulBufSize;
	PWLAN_802_11_BSSID pTempBSSIDList;
	wlan_adapter   *Adapter = priv->adapter;
	PBSS_DESCRIPTION_SET_ALL_FIELDS pTempBssDescList;

	if (AllocateSingleTx(priv) != WLAN_STATUS_SUCCESS) {
		PRINTK1("Failed in Allocate Single TX\n");
		wlan_free_adapter(priv);
		return -1;
	}
	
	/* Allocate buffer to store the BSSID list */
	ulBufSize = sizeof(WLAN_802_11_BSSID) * MRVDRV_MAX_BSSID_LIST;
	if (!(pTempBSSIDList = kmalloc(ulBufSize, GFP_KERNEL))) {
		wlan_free_adapter(priv);
		return -1;
	}

	Adapter->BSSIDList = pTempBSSIDList;
	memset(Adapter->BSSIDList, 0, ulBufSize);

	/*
	 * Create the command buffers 
	 */
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
    /* Allocate buffer to store IEEE BSS Descriptor list */
	ulBufSize =
		sizeof(BSS_DESCRIPTION_SET_ALL_FIELDS) * MRVDRV_MAX_BSSID_LIST;
	if (!(pTempBssDescList = kmalloc(ulBufSize, GFP_KERNEL))) {
		wlan_free_adapter(priv);
		return -1;
	}

	Adapter->BssDescList =
		(PBSS_DESCRIPTION_SET_ALL_FIELDS) pTempBssDescList;
	memset(Adapter->BssDescList, 0, ulBufSize);

	return 0;
}

void wlan_free_adapter(wlan_private * priv)
{
	wlan_adapter   *Adapter = priv->adapter;

	if (!Adapter)
		return;

	/* Free allocated data structures */
	// FreeSingleTxBuffer(Adapter);

	FreeCmdBuffer(priv);

	if (Adapter->CommandTimerIsSet) {
		FreeTimer(&Adapter->MrvDrvCommandTimer);
		Adapter->CommandTimerIsSet = FALSE;
	}

	if (Adapter->TimerIsSet) {
		FreeTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}

#ifdef PS_REQUIRED
    	if (Adapter->TxPktTimerIsSet) {
		FreeTimer(&Adapter->MrvDrvTxPktTimer);
		Adapter->TxPktTimerIsSet = FALSE;
	}
#endif

	if (Adapter->BssDescList) {
		kfree(Adapter->BssDescList);
	}

#ifdef BG_SCAN
	if(Adapter->bgScanConfig)
		kfree(Adapter->bgScanConfig);
#endif /* BG_SCAN */

#ifdef PASSTHROUGH_MODE
	FreePassthroughList(Adapter);
#endif

	/* The following is needed for Threadx and are dummy in linux */
	OS_FREE_LOCK(&Adapter->CurrentTxLock);
	OS_FREE_LOCK(&Adapter->QueueSpinLock);

	/* Free the adapter object itself */
	kfree(Adapter);
}

void wlan_init_adapter(wlan_private * priv)
{
	wlan_adapter	*Adapter = priv->adapter;
	int		i;
#ifdef BG_SCAN
	static u8 dfltBgScanCfg[] = {
		0x01, 0x00,
		0x00,
		0x01,
		0x02,
		0x01,
		0xF4, 0x01,
		0x03, 0x00, 0x00, 0x00,
		0x03, 0x00, 0x00, 0x00,
		0x0E, 0x00,
		0x01, 0x01, 0x00, 0x00
	};
#endif /* BG_SCAN */

	/* Timer variables */
	Adapter->TimerInterval = MRVDRV_DEFAULT_TIMER_INTERVAL;
	Adapter->isCommandTimerExpired = FALSE;

	/* ATIM params */
  	Adapter->AtimWindow = 0;
	Adapter->ATIMEnabled = FALSE;

	/* Operation characteristics */
    	Adapter->CurrentLookAhead = (u32) MRVDRV_MAXIMUM_ETH_PACKET_SIZE -
							MRVDRV_ETH_HEADER_SIZE;
	Adapter->MediaConnectStatus = WlanMediaStateDisconnected;
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
	for(i = 0; i < sizeof(Adapter->WepKey) / sizeof(Adapter->WepKey[0]); i++)
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

#ifdef CONFIG_MARVELL_PM
	Adapter->DeepSleep_State = DEEP_SLEEP_DISABLED;
	Adapter->PM_State = PM_DISABLED;
#endif
		
/* SDCF: MPS/20041012: Is this really required */
	Adapter->HisRegCpy |= HIS_TxDnLdRdy; // MPS

	memset(&(Adapter->CurBssParams.ssid), 0, sizeof(WLAN_802_11_SSID));

	/* Initialize RSSI-related variables */
	// Adapter->RSSITriggerValue = MRVDRV_RSSI_TRIGGER_DEFAULT;
	// Adapter->RSSITriggerCounter = 0;

	/*
	 * TryCount = TxRetryCount + 1 
	 */
	Adapter->TxRetryCount = 4;

	/* Power management state */
	Adapter->CurPowerState = WlanDeviceStateD0;

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
	Adapter->Reassociate = TRUE;
	Adapter->TxAntenna = RF_ANTENNA_2;
	Adapter->RxAntenna = RF_ANTENNA_AUTO;

	Adapter->Is_DataRate_Auto = 1; // Auto rate
	
	Adapter->FragThsd = MRVDRV_FRAG_MAX_VALUE;
	Adapter->RTSThsd = MRVDRV_RTS_MAX_VALUE;
#define SHORT_PREAMBLE_ALLOWED		1
	//Adapter->Preamble = HostCmd_TYPE_LONG_PREAMBLE;
#ifndef NEW_ASSOCIATION
	// set default value of infraCapInfo.
	memset(&Adapter->infraCapInfo, 0, sizeof(Adapter->infraCapInfo));
	Adapter->infraCapInfo.ShortPreamble = SHORT_PREAMBLE_ALLOWED;
#endif
	Adapter->Channel = DEFAULT_CHANNEL;	// default
	Adapter->AdhocChannel = DEFAULT_AD_HOC_CHANNEL;	// default

#ifdef	PS_REQUIRED
	/* **** POWER_MODE ***** */
	Adapter->PSMode = Wlan802_11PowerModeCAM;
	Adapter->MultipleDtim = MRVDRV_DEFAULT_MULTIPLE_DTIM;

	Adapter->ListenInterval = MRVDRV_DEFAULT_LISTEN_INTERVAL;
	
	Adapter->PSState = PS_STATE_FULL_POWER;
	Adapter->NeedToWakeup = FALSE;
	/* ********************* */
#endif

#ifdef DEEP_SLEEP
	Adapter->IsDeepSleep = FALSE;
	Adapter->IsDeepSleepRequired = FALSE;
#endif // DEEP_SLEEP_CMD

#ifdef HOST_WAKEUP
	Adapter->bHostWakeupDevRequired = FALSE;
#endif // HOST_WAKEUP

	/* **** Extended Scan ***** */
	Adapter->ExtendedScan = 0;	// Off by default
	Adapter->pExtendedScanParams = NULL;
	Adapter->BeaconPeriod = 100;

#ifdef PS_REQUIRED
	Adapter->PreTbttTime = 0;
#endif

	Adapter->DataRate = 0; // Initially indicate the rate as auto 

	Adapter->IntCounter = Adapter->IntCounterSaved = 0;
#ifdef BG_SCAN
	Adapter->bgScanConfig = kmalloc(sizeof(dfltBgScanCfg), GFP_KERNEL);
	memcpy(Adapter->bgScanConfig, dfltBgScanCfg, sizeof(dfltBgScanCfg));
	Adapter->bgScanConfigSize = sizeof(dfltBgScanCfg);
#endif /* BG_SCAN */

#ifdef ADHOCAES
	init_waitqueue_head(&Adapter->cmd_EncKey);
#endif

#ifdef STDCMD
	init_waitqueue_head(&Adapter->std_cmd_q);
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
	spin_lock_init(&Adapter->QueueSpinLock);

	Adapter->CurrentTxSkb = NULL;

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

//#ifdef BULVERDE
//	sdio_enable_SDIO_INT();
//#endif

	PRINTK1("Re-sending same command as it timeout...!\n");
	QueueCmd(Adapter,pTempNode);

	wake_up_interruptible(&priv->intThread.waitQ);

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

#ifdef PS_REQUIRED
 	InitializeTimer(&Adapter->MrvDrvTxPktTimer, MrvDrvTxPktTimerFunction,
			priv);
   	Adapter->TxPktTimerIsSet = FALSE;
#endif

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

	wake_up_interruptible(&priv->eventThread.waitQ);

	LEAVE();
	return;
}

#ifdef PS_REQUIRED
void MrvDrvTxPktTimerFunction(void *FunctionContext)
{
	wlan_private   *priv = (wlan_private *) FunctionContext;
	wlan_adapter   *Adapter = priv->adapter;

	PRINTK1("- TX Pkt timeout, Tx Done does not response \n");

	Adapter->TxPktTimerIsSet = FALSE;

	if (Adapter->SentPacket != NULL) {
		if (Adapter->TxRetryCount < 3) {
			PRINTK1("Retry to send the last pkt, retry cnt"
					" = %d\n", Adapter->TxRetryCount);
			// TODO: retrigger the send, 
			// FW may have missed the interrupt
			
			/* reschedule the timer */
			ModTimer(&Adapter->MrvDrvTxPktTimer,
					MRVDRV_DEFAULT_TX_PKT_TIME_OUT);
			Adapter->TxPktTimerIsSet = TRUE;

			Adapter->TxRetryCount++;
		} else {
			/* clean up the timed out pkt */
			ResetSingleTxDoneAck(priv);
		}
	}

	return;
}
#endif

#ifdef	DEBUG_LEVEL0
void HexDump(char *prompt, u8 * buf, int len)
{
	int             i = 0;

	/* 
	 * Here dont change these 'printk's to 'PRINTK'. Using 'PRINTK' 
	 * macro will make the hexdump to add '<7>' before each byte 
	 */
	printk(KERN_DEBUG "%s: ", prompt);
	for (i = 1; i <= len; i++) {
		printk("%02x ", (u8) * buf);
		buf++;
	}

	printk("\n");
}
#endif

#ifdef PCB_REV4
void wlan_read_write_rfreg(wlan_private *priv)
{

	wlan_offset_value offval;
	offval.offset = RF_REG_OFFSET;
	offval.value  = RF_REG_VALUE;

	PrepareAndSendCommand( priv, HostCmd_CMD_RF_REG_ACCESS,
				HostCmd_ACT_GEN_GET, HostCmd_OPTION_USE_INT, 
				0, HostCmd_PENDING_ON_NONE, &offval);

	wlan_sched_timeout(100);
	
	offval.value |= 0x1;

	PrepareAndSendCommand(priv, HostCmd_CMD_RF_REG_ACCESS,
				HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT, 
				0, HostCmd_PENDING_ON_NONE, &offval);
	wlan_sched_timeout(100);
}
#endif

