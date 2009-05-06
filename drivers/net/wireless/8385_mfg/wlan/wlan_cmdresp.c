/*
 * File : wlan_cmdresp.c 
 */

#ifdef		linux
#include	<linux/module.h>
#include	<linux/kernel.h>
#include	<linux/net.h>
#include	<linux/netdevice.h>
#include	<linux/skbuff.h>
#include	<linux/etherdevice.h>
#include	<asm/irq.h>
#include	<net/iw_handler.h>
#endif	// linux

#include	"os_headers.h"
#include	"wlan.h"
#include	"wlan_fw.h"
#include	"wlan_wext.h"
#include	"macro_defs.h"

extern void CleanupAndInsertCmd(wlan_private * priv, CmdCtrlNode * pTempCmd);

#ifdef WPA
static void send_bad_auth_event(wlan_private *priv)
{
  union iwreq_data iwrq;
  unsigned char buf[50];
  
  ENTER();

  memset(&iwrq, 0, sizeof(union iwreq_data));
  memset(buf, 0, sizeof(buf));
  
  sprintf(buf, "%s", "BAD-AUTH.indication ");
  
  iwrq.data.pointer = buf; 
  iwrq.data.length = strlen(buf) + 1 + IW_EV_LCP_LEN;

  PRINTK1("Sending wireless event IWEVCUSTOM for Net EAP failure\n");
  wireless_send_event(priv->wlan_dev.netdev, IWEVCUSTOM, &iwrq, buf);

  LEAVE();
}
#endif

#ifdef ADHOCAES
int HandleAdhocEncryptResponse(wlan_private *priv, 
					PHostCmd_DS_802_11_PWK_KEY pEncKey);
#endif

extern int wlan_set_regiontable(wlan_private *priv, u8 region, u8 band);

static void StartReassociation(wlan_private * priv)
{
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
	
	if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {

		MacEventDisconnected(priv);

			if (Adapter->Reassociate == TRUE ) {
				Adapter->TimerIsSet = TRUE;
				PRINTK1("Going to trigger the timer\n");
				ModTimer(&Adapter->MrvDrvTimer, 20);
			}
	}
}

#ifdef WPA
static inline void HandleMICFailureEvent(wlan_private *priv, int event)
{
	union iwreq_data	iwrq;
	unsigned char		buf[50];
	
	ENTER();

	memset(&iwrq, 0, sizeof(union iwreq_data));
	memset(buf, 0, sizeof(buf));
	
	sprintf(buf, "%s", "MLME-MICHAELMICFAILURE.indication ");
	
	if (event == MACREG_INT_CODE_MIC_ERR_UNICAST) {
		strcat(buf, "unicast ");
	} else {
		strcat(buf, "multicast ");
	}
	
	iwrq.data.pointer = buf; 
	iwrq.data.length = strlen(buf) + 1 + IW_EV_LCP_LEN;

	/* Send Event to upper layer	*/

	PRINTK1("Event Indication string = %s\n", iwrq.data.pointer);
	PRINTK1("Event Indication String Length = %d\n", iwrq.data.length);
	
	PRINTK1("Sending Wireless event IWEVCUSTOM for MIC Error\n");
	wireless_send_event(priv->wlan_dev.netdev, IWEVCUSTOM, &iwrq, buf);

	LEAVE();
}
#endif

#ifdef BG_SCAN
static inline int sendBgScanQueryCmd(wlan_private *priv)
{
	int	ret = 0;

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_BG_SCAN_QUERY,
			0, HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	return ret;
}
#endif /* BG_SCAN */


int wlan_process_event(wlan_private * priv)
{
	int		ret = 0;
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();

	/*
	* The Device scratch register 2 tells us the event! 
	* shifting them by 8 to reach there ??
	*/

	PRINTK1("EVENT Cause %x\n", Adapter->EventCause);

	switch (Adapter->EventCause >> SBI_EVENT_CAUSE_SHIFT) {	// refer
								// 2.1.1.3
		case MACREG_INT_CODE_TX_PPA_FREE:
			PRINTK1("EVENT: MACREG_INT_CODE_TX_PPA_FREE: \n");
			break;

		case MACREG_INT_CODE_TX_DMA_DONE:	// TX DMA finished
			PRINTK1("EVENT: MACREG_INT_CODE_TX_DMA_DONE:"
							" Unimplemented");
			break;

		case MACREG_INT_CODE_LINK_SENSED:	// Link found
			PRINTK1("EVENT: MACREG_INT_CODE_LINK_SENSED\n");
			break;

		case MACREG_INT_CODE_DEAUTHENTICATED:
			PRINTK1("EVENT: HWAC - Deauthenticated Actually\n");
			StartReassociation(priv);
			break;

		// TODO Take care of this event gracefully
		case MACREG_INT_CODE_DISASSOCIATED:
			PRINTK1("EVENT: HWAC - Disassociated\n");
			StartReassociation(priv);
			break;

		case MACREG_INT_CODE_LINK_LOSE_W_SCAN:	// Link lost
			PRINTK1("EVENT: HWAC - Link lost WITH SCAN\n");
			StartReassociation(priv);
			break;

		case MACREG_INT_CODE_LINK_LOSE_NO_SCAN:
			PRINTK("EVENT:  HWAC - Link lost ***\n");
			StartReassociation(priv);
			break;

//#ifdef DUMMY_PACKET
		case MACREG_INT_CODE_DUMMY_PKT:
			priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
			TXRX_DEBUG_GET_ALL(0x40, 0xff, 0xff);
			TXRX_DEBUG_GET_TIME(6);

			PRINTK1("EVENT: recieved tx done interrupt"
						" calling Send Single\n");
			//netif_wake_queue(priv->wlan_dev.netdev);
			break;
//#endif

		// POWER MODE 
		/*
		 * Before going to sleep 
		 * 	a) Test if any packets are there to
		 * 		send, since we have only a single buffer 
		 * 		just wake up the thread which pushes the 
		 * 		data out (if the data is not pushed)
		 * 	b) When waking up , check if there is any data to 
		 * 		be pushed out! Else go back to sleep again 
		 */
#ifdef PS_REQUIRED
		case MACREG_INT_CODE_PS_SLEEP:
			PRINTK1("EVENT: SLEEP\n");
			PRINTK("_");
#ifdef	DEBUG
			Adapter->SleepCounter++;

			/* Reset consecutive WAKE Counter */
			Adapter->ConsecutiveAwake = 0;
#endif

			/* handle unexpected PS SLEEP event */
			if (Adapter->PSState == PS_STATE_FULL_POWER) {
				PRINTK1("In FULL POWER mode - ignore PS SLEEP\n");
				break;
			}

			/*
			 * Make sure the previous command responded before 
			 * confirming sleep. 
			 */
			if ((!Adapter->CurCmd)) {
				PRINTK1("Sending Sleep Confirm\n");
				PSConfirmSleep(priv, (u16) Adapter->PSMode);
			} else { 
				PRINTK1("Command Pending - NOT"
							" confirming sleep!\n");
			}

#ifdef	DEBUG
			PRINTK1("SleepCounter: %u ConfirmCounter: %u\n",
			Adapter->SleepCounter, Adapter->ConfirmCounter);
#endif
			break;

		case MACREG_INT_CODE_PS_AWAKE:
			PRINTK1("EVENT: AWAKE \n");
			PRINTK("|");

#ifdef	DEBUG
			/*
			 * Did we get 2 consecutive AWAKE events 
			 */
			if (++Adapter->ConsecutiveAwake > 1) {
				Adapter->NumConsecutiveAwakes++;
				PRINTK1("ERROR: Received %u consecutive"
						" AWAKE events (Total:%u)!",
						Adapter->ConsecutiveAwake, 
						Adapter->NumConsecutiveAwakes);
			}
#endif

			/* handle unexpected PS AWAKE event */
			if (Adapter->PSState == PS_STATE_FULL_POWER) {
				PRINTK1("In FULL POWER mode - ignore PS AWAKE\n");
				break;
			}

			/*
			 * Normal power save 
			 */
			Adapter->PSState = PS_STATE_AWAKE;

			if (Adapter->NeedToWakeup == TRUE) {
				// wait for the command processing to finish
				// before resuming sending 
				// Adapter->NeedToWakeup will be set to FALSE 
				// in PSWakup()
				PRINTK1("Waking up...\n");
				PSWakeup(priv);
			}
			break;
#endif	/* PS_REQURIED */

#ifdef DEEP_SLEEP
		case MACREG_INT_CODE_DEEP_SLEEP_AWAKE:
			PRINTK1("DEEP SLEEP AWAKE Event!\n");
		        sbi_reset_deepsleep_wakeup(priv);

			if (Adapter->IsDeepSleep == TRUE) {
				Adapter->IsDeepSleep = FALSE;
				// call function below to run next command or put it back 
				// to IEEE_PS if needed.
				//if (!list_empty(&Adapter->CmdPendingQ))
				if (netif_queue_stopped(priv->wlan_dev.netdev))
					netif_wake_queue(priv->wlan_dev.netdev);
			}
			break;
#endif // DEEP_SLEEP

#ifdef HOST_WAKEUP
		case MACREG_INT_CODE_HOST_WAKE_UP:
			printk("HOST WAKE UP Event!\n");
			Adapter->bHostWakeupDevRequired = FALSE;
#ifdef DEEP_SLEEP
		        sbi_reset_deepsleep_wakeup(priv);
#endif // DEEP_SLEEP
			ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM,
					0, HostCmd_OPTION_USE_INT, 0,
					HostCmd_PENDING_ON_NONE, NULL);
			mdelay(1);
			break;
#endif // HOST_WAKEUP

#ifdef WPA
		case MACREG_INT_CODE_MIC_ERR_UNICAST:
			PRINTK1("EVENT: UNICAST MIC ERROR\n");
			HandleMICFailureEvent(priv, 
					MACREG_INT_CODE_MIC_ERR_UNICAST);
			break;
		
		case MACREG_INT_CODE_MIC_ERR_MULTICAST:
			PRINTK1("EVENT: MULTICAST MIC ERROR\n");
			HandleMICFailureEvent(priv, 
					MACREG_INT_CODE_MIC_ERR_MULTICAST);
			break;
#endif
		case MACREG_INT_CODE_MIB_CHANGED:
		case MACREG_INT_CODE_INIT_DONE:
			break;

		case MACREG_INT_CODE_ADHOC_BCN_LOST:	// Adhoc beacon lost
			PRINTK1("EVENT: HWAC - ADHOC BCN LOST\n");		
			break;
#ifdef BG_SCAN
		case MACREG_INT_CODE_BG_SCAN_REPORT:
			PRINTK1("EVENT: Background SCAN Report\n");		
			ret = sendBgScanQueryCmd(priv);
			break;
#endif /* BG_SCAN */ 
		default:
			// not implemented in the firmware. 
			// (in original fw from marvell)
			PRINTK("EVENT: unknown event id: %#x\n",
				Adapter->EventCause >> SBI_EVENT_CAUSE_SHIFT);
			break;
	}

	Adapter->EventCause = 0;
	LEAVE();
	return ret;
}

#ifdef FWVERSION3

/*The function needs to update for Ver4 new Scan Resp format*/
int InterpretBSSDescriptionWithIE(wlan_private * priv,
			      void *RetCommandBuffer,
			      u32 ulBSSDescriptionListSize,
			      int next_bssid_index)
{

	wlan_adapter			*Adapter = priv->adapter;
	u8				*pCurrentPtr, *pNextBeacon;
	u32				nBytesLeft;
	u32				ulBSSIDIndex = next_bssid_index;

	PWLAN_802_11_BSSID		pBSSIDList;
	PBSS_DESCRIPTION_SET_ALL_FIELDS	pBssDescList;
	IEEEtypes_ElementId_e		nElemID;
	IEEEtypes_FhParamSet_t		*pFH;
	IEEEtypes_DsParamSet_t		*pDS;
	IEEEtypes_CfParamSet_t		*pCF;
	IEEEtypes_IbssParamSet_t	*pIbss;
	IEEEtypes_CapInfo_t		*pCap;

	WLAN_802_11_FIXED_IEs		*pFixedIE;
	WLAN_802_11_VARIABLE_IEs	*pVariableIE;

	u16				usBeaconSize;
	u16				usBytesLeftForCurrentBeacon;
	u8				ucElemLen, ucRateSize = 0;
	BOOLEAN				bFoundDataRateIE;
	u16				usNumLineToPrint;

#ifdef WPA
	PWPA_SUPPLICANT			pwpa_supplicant;
#ifdef WPA2
	PWPA_SUPPLICANT			pwpa2_supplicant;
#endif //WPA2
	IE_WPA				*pIeWpa;
	u8				oui01[4] = { 0x00, 0x50, 0xf2, 0x01 };
#endif //WPA

#ifdef ENABLE_802_11D
	IEEEtypes_CountryInfoSet_t 	*pcountryinfo;
	IEEEtypes_SubbandSet_t		*pbandIE; /*subband of IE*/

	IEEEtypes_SubbandSet_t		*pbandDR; /*subband of DomainReg*/
	int j, cnt; /*number of subband */
#endif
	
	ENTER();

	// Initialize the current pointer to the Command buffer
	pNextBeacon = pCurrentPtr = (u8 *) RetCommandBuffer;

	if (!next_bssid_index) {
		// Clean up the current BSSID List
		memset(Adapter->BSSIDList, 0,
		sizeof(WLAN_802_11_BSSID) * MRVDRV_MAX_BSSID_LIST);
	
		// Clean up the current BSS Descriptor List
		memset(Adapter->BssDescList, 0,
		sizeof(BSS_DESCRIPTION_SET_ALL_FIELDS) * MRVDRV_MAX_BSSID_LIST);
#ifdef WPA
		memset(Adapter->wpa_supplicant, 0,
		sizeof(WPA_SUPPLICANT) * MRVDRV_MAX_BSSID_LIST);
#ifdef WPA2
		memset(Adapter->wpa2_supplicant, 0,
		sizeof(WPA_SUPPLICANT) * MRVDRV_MAX_BSSID_LIST);
#endif //WPA2
#endif //WPA
	}
    
	// At the beginning, nBytesLeft is the total BSSDescription List
	nBytesLeft = (LONG) ulBSSDescriptionListSize;

	// expected format :

	// Length - 2 bytes
	// BSSId - 6 bytes
	// IE - variable length

	while (nBytesLeft > 0 && ulBSSIDIndex < Adapter->ulNumOfBSSIDs) {

		pCurrentPtr = pNextBeacon;

		memcpy(&usBeaconSize, pCurrentPtr, sizeof(u16));

		usBeaconSize = wlan_le16_to_cpu(usBeaconSize);
		
		usBytesLeftForCurrentBeacon = usBeaconSize;

		// PRINTK1("Ap : %d Size : %d\n", 
		// (u16)ulBSSIDIndex, (u16)usBeaconSize);

		if (usBeaconSize > nBytesLeft) {
			Adapter->ulNumOfBSSIDs = ulBSSIDIndex;
			return -1;
		}
		// print the beacon buffer
		usNumLineToPrint = (usBeaconSize + 15) >> 4;

		pCurrentPtr += sizeof(usBeaconSize);

		pNextBeacon = pCurrentPtr + usBeaconSize;

		pBSSIDList = &Adapter->BSSIDList[ulBSSIDIndex];
		pFixedIE = &Adapter->IEBuffer[ulBSSIDIndex].FixedIE;
		
#ifdef FWVERSION3
		pBSSIDList->IELength = wlan_le32_to_cpu(pBSSIDList->IELength);
#endif
		PRINTK1("(In WithIE fun)pFixedIE->Capabilities=0x%X\n", pFixedIE->Capabilities);

		pVariableIE = (PWLAN_802_11_VARIABLE_IEs)
				Adapter->IEBuffer[ulBSSIDIndex].VariableIE;

		pBssDescList = &Adapter->BssDescList[ulBSSIDIndex];
		
		pBssDescList->BeaconPeriod = 
				//wlan_le16_to_cpu(pBssDescList->BeaconPeriod);
				pBssDescList->BeaconPeriod;

#ifdef WPA
		pwpa_supplicant = &Adapter->wpa_supplicant[ulBSSIDIndex];
#ifdef WPA2
		pwpa2_supplicant = &Adapter->wpa2_supplicant[ulBSSIDIndex];
		memcpy(pwpa2_supplicant->BSSID, pCurrentPtr, 
							MRVDRV_ETH_ADDR_LEN);
#endif //WPA2
		memcpy(pwpa_supplicant->BSSID, pCurrentPtr, 
							MRVDRV_ETH_ADDR_LEN);
#endif //WPA
		memcpy(pBSSIDList->MacAddress, pCurrentPtr, 
							MRVDRV_ETH_ADDR_LEN);
		PRINTK1("AP MAC Addr: %x:%x:%x:%x:%x:%x\n",
			pBSSIDList->MacAddress[0], pBSSIDList->MacAddress[1],
			pBSSIDList->MacAddress[2], pBSSIDList->MacAddress[3],
			pBSSIDList->MacAddress[4], pBSSIDList->MacAddress[5]);

		memcpy(pBssDescList->BSSID, pCurrentPtr, MRVDRV_ETH_ADDR_LEN);

		pCurrentPtr += MRVDRV_ETH_ADDR_LEN;
		usBytesLeftForCurrentBeacon -= MRVDRV_ETH_ADDR_LEN;
		pBSSIDList->Configuration.Length =
					sizeof(WLAN_802_11_CONFIGURATION);

		if (usBytesLeftForCurrentBeacon < 12) {
			PRINTK1("Not enough bytes left\n");
			Adapter->ulNumOfBSSIDs = ulBSSIDIndex;
			return -1;
		}
		
		// rest of the current buffer are IE's
		pBSSIDList->IELength = usBytesLeftForCurrentBeacon;
		PRINTK1("IELength for this AP = %d\n", 
						(u32) pBSSIDList->IELength);

#ifdef TLV_SCAN
		pBSSIDList->Rssi = wlan_le32_to_cpu((LONG) (*pCurrentPtr)) + 
						MRVDRV_RSSI_DEFAULT_NOISE_VALUE;
		PRINTK("Enable RSSI(1byte): %02X\n", *pCurrentPtr );
		pCurrentPtr +=1;
		usBytesLeftForCurrentBeacon -= 1;
#endif /*TLV_SCAN*/
		// next 3 fields are time stamp, beacon interval, 
		// and capability information
		// time stamp is 8 byte long
		memcpy(pFixedIE->Timestamp, pCurrentPtr, 8);
		memcpy(pBssDescList->TimeStamp, pCurrentPtr, 8);
		pCurrentPtr += 8;
		usBytesLeftForCurrentBeacon -= 8;

		// beacon interval is 2 byte long
		memcpy(&(pFixedIE->BeaconInterval), pCurrentPtr, 2);
		pBSSIDList->Configuration.BeaconPeriod =
		pBssDescList->BeaconPeriod = wlan_le16_to_cpu(pFixedIE->BeaconInterval);
		pCurrentPtr += 2;
		usBytesLeftForCurrentBeacon -= 2;

		// capability information is 2 byte long
		memcpy(&(pFixedIE->Capabilities), pCurrentPtr, 2);
		
		PRINTK1("(In WithIE fun1)pFixedIE->Capabilities=0x%X\n", pFixedIE->Capabilities);
		
		pBSSIDList->Configuration.BeaconPeriod =
		pBssDescList->BeaconPeriod = wlan_le16_to_cpu(pFixedIE->BeaconInterval);
		PRINTK1("(In WithIE fun2)pFixedIE->Capabilities=0x%X\n", pFixedIE->Capabilities);	
		
		pFixedIE->Capabilities = wlan_le16_to_cpu(pFixedIE->Capabilities);
			
		pCap = (IEEEtypes_CapInfo_t *) & (pFixedIE->Capabilities);
		
		if (pCap->Privacy) {
			PRINTK1("AP WEP enabled\n");
			pBSSIDList->Privacy = Wlan802_11PrivFilter8021xWEP;
		} else {
			pBSSIDList->Privacy = Wlan802_11PrivFilterAcceptAll;
		}

		if (pCap->Ibss == 1) {
			pBSSIDList->InfrastructureMode = Wlan802_11IBSS;
			pBssDescList->BSSType = HostCmd_BSS_TYPE_IBSS;
		} else {
			pBSSIDList->InfrastructureMode = 
						Wlan802_11Infrastructure;
			pBssDescList->BSSType = HostCmd_BSS_TYPE_BSS;
		}

		memcpy(&pBssDescList->Cap, pCap, sizeof(IEEEtypes_CapInfo_t));
#ifdef ENABLE_802_11H_TPC
		PRINTK2("Capability=0x%X  SpectrumMgmt=%d\n",pFixedIE->Capabilities, pCap->SpectrumMgmt ); 
		if( pCap->SpectrumMgmt == 1 ) {
/*			Adapter->State11HTPC.Sensed11H = TRUE;
*/
			pBssDescList->Sensed11H = 1;
		}
		/* Set Sensed==TRUE, 
			if Cap.SpectrumMgmt==1 or 
				one of the TC_REQUEST, TP_REQUEST, POWERCAP, POWERCONSTRAINT IE 
				is found in Beacon or Probe-Resp*/
					
#endif
		pCurrentPtr += 2;
		usBytesLeftForCurrentBeacon -= 2;

		if (usBytesLeftForCurrentBeacon > 
					MRVDRV_SCAN_LIST_VAR_IE_SPACE) {
			// not enough space to copy the IE for the current AP
			// just skip it for now
			pCurrentPtr += usBytesLeftForCurrentBeacon;
			nBytesLeft -= usBeaconSize;
			Adapter->ulNumOfBSSIDs--;
			PRINTK1("Variable IE size too big to fit into buffer, "
								"skip\n");
			 continue;
		}
		// reset are variable IE
		// copy the entire variable IE to IE buffer
		memcpy(pVariableIE, pCurrentPtr, usBytesLeftForCurrentBeacon);

		HEXDUMP("IE info", (u8 *) pVariableIE,
						usBytesLeftForCurrentBeacon);

		bFoundDataRateIE = FALSE;

		// process variable IE
		while (usBytesLeftForCurrentBeacon > 2) {
			nElemID = (IEEEtypes_ElementId_e)
					(*((unsigned char *) pCurrentPtr));
			ucElemLen = *((unsigned char *) pCurrentPtr + 1);

			usNumLineToPrint = (ucElemLen + 15) >> 4;
			
			if (usBytesLeftForCurrentBeacon < ucElemLen) {
				nBytesLeft -= usBeaconSize;
				Adapter->ulNumOfBSSIDs--;
				PRINTK1("Error in processing IE, "
						"bytes left < IE length\n");
				usBytesLeftForCurrentBeacon = 0;
				continue;
			}

			switch (nElemID) {
			
				case SSID:
					memset(pBssDescList->SSID, 0, 
							MRVDRV_MAX_SSID_LENGTH);
					memmove(pBssDescList->SSID, 
							(pCurrentPtr + 2), 
							ucElemLen);
					pBSSIDList->Ssid.SsidLength = ucElemLen;
					memcpy(pBSSIDList->Ssid.Ssid, 
							(pCurrentPtr + 2),
							ucElemLen);
#ifdef WPA
					pwpa_supplicant->SSID_Len = ucElemLen;
					memcpy(pwpa_supplicant->SSID, 
							(pCurrentPtr + 2),
							ucElemLen);
#endif

					break;
				
				case SUPPORTED_RATES:
					memcpy(pBssDescList->DataRates, 
							(pCurrentPtr + 2),
							ucElemLen);
					memmove(pBSSIDList->SupportedRates,
					       		(pCurrentPtr + 2),
							ucElemLen);
					ucRateSize = ucElemLen;
					bFoundDataRateIE = TRUE;
					break;

                                case EXTRA_IE:
                                        PRINTK1("EXTRA_IE Found!\n" );
                                        pBSSIDList->extra_ie = 1;
                                        break;

				case FH_PARAM_SET:
					pFH = (IEEEtypes_FhParamSet_t *) 
								pCurrentPtr;
					pBSSIDList->NetworkTypeInUse = 
								Wlan802_11FH;
					pBSSIDList->Configuration.DSConfig = 0;
					
					pBSSIDList->
					Configuration.FHConfig.Length =
					sizeof(WLAN_802_11_CONFIGURATION_FH);

					pBSSIDList->
					Configuration.FHConfig.HopPattern =
								pFH->HopSet;

					pBSSIDList->
					Configuration.FHConfig.DwellTime =
					pFH->DwellTime;

					memmove(&pBssDescList->
						PhyParamSet.FhParamSet, pFH, 
						sizeof(IEEEtypes_FhParamSet_t));
					
					pBssDescList->PhyParamSet.FhParamSet.
						DwellTime = 
						wlan_le16_to_cpu(pBssDescList->
							PhyParamSet.FhParamSet.
							DwellTime);
					break;

				case DS_PARAM_SET:
					// PRINTK1("DS_PARAM_SET\n");
					pDS = (IEEEtypes_DsParamSet_t *) 
								pCurrentPtr;

					pBSSIDList->NetworkTypeInUse = 
								Wlan802_11DS;

					pBSSIDList->Configuration.DSConfig =
						DSFreqList[pDS->CurrentChan];

					pBSSIDList->Channel = pDS->CurrentChan;

					// Copy IEEEtypes_DsParamSet_t 
					// info to pBssDescList
					memcpy(&pBssDescList->
					PhyParamSet.DsParamSet,	pDS, 
					sizeof(IEEEtypes_DsParamSet_t));

					break;

				case CF_PARAM_SET:
					pCF = (IEEEtypes_CfParamSet_t *)
					       			pCurrentPtr;
					
					// Copy IEEEtypes_CfParamSet_t 
					// info to pBssDescList
					memcpy(&(pBssDescList->
					SsParamSet.CfParamSet),	pCF, 
					sizeof(IEEEtypes_CfParamSet_t));

					pBssDescList->SsParamSet.CfParamSet.
						CfpMaxDuration = 
							pBssDescList->
							SsParamSet.CfParamSet.
							CfpMaxDuration;
					pBssDescList->SsParamSet.CfParamSet.
						CfpDurationRemaining = 
							pBssDescList->
							SsParamSet.CfParamSet.
							CfpDurationRemaining;

					break;

				case IBSS_PARAM_SET:
					pIbss = (IEEEtypes_IbssParamSet_t *) 
							pCurrentPtr;
					pBSSIDList->Configuration.ATIMWindow =
						wlan_le32_to_cpu(pIbss->AtimWindow);

					// Copy IEEEtypes_IbssParamSet_t 
					// info to pBssDescList
					memmove(&pBssDescList->
					SsParamSet.IbssParamSet, pIbss, 
					sizeof(IEEEtypes_IbssParamSet_t));

					pBssDescList->SsParamSet.IbssParamSet.
						AtimWindow = 
						wlan_le16_to_cpu(pBssDescList->
							SsParamSet.IbssParamSet.
							AtimWindow);
				
					break;

#ifdef ENABLE_802_11D
				case COUNTRY_INFO: /*Handle Country Info IE*/
					pcountryinfo = (IEEEtypes_CountryInfoSet_t *)
							pCurrentPtr;
					pbandIE = pcountryinfo->Subband;
					pbandDR = Adapter->DomainReg.Subband;
					if ( pcountryinfo->Len < sizeof(pcountryinfo->CountryCode) ||
					     pcountryinfo->Len > 254	) {
						PRINTK1("Err: CountryInfo len =%d min=%d max=254\n", 
							pcountryinfo->Len,
							sizeof(pcountryinfo->CountryCode) );
						return -1;
					}
					
					PRINTK2("11D: Org.CountryCode=%c%c\n", Adapter->DomainReg.CountryCode[0],
									   Adapter->DomainReg.CountryCode[1] );
					
					if ( memcmp( Adapter->DomainReg.CountryCode,
						pcountryinfo->CountryCode, 
						sizeof(Adapter->DomainReg.CountryCode) )) { 
					/*CountryCode is used for Country Info changed check*/

						cnt = (pcountryinfo->Len - 
							    sizeof(pcountryinfo->CountryCode) )
								/ sizeof( IEEEtypes_SubbandSet_t );

						memcpy( Adapter->DomainReg.CountryCode,
							pcountryinfo->CountryCode, 
							sizeof(Adapter->DomainReg.CountryCode ) );

						/*cp countryinfo to DomainReg*/
						for ( j=0; j< cnt; j++ ) {
							pbandDR[j].FirstChan = pbandIE[j].FirstChan;
							pbandDR[j].NoOfChan = pbandIE[j].NoOfChan;
							pbandDR[j].MaxTxPwr = pbandIE[j].MaxTxPwr;				
						}
						Adapter->DomainReg.NoOfSubband = cnt;
						Adapter->State11D.CountryInfoChanged = TRUE;
						PRINTK1("CountryInfo IE Found. Changed Cnt=%d\n", cnt );
						PRINTK2("11D: New CountryCode=%c%c\n", Adapter->DomainReg.CountryCode[0],
									   Adapter->DomainReg.CountryCode[1] );
					}
					else
						PRINTK1("CountryInfo IE Found. UnChanged\n" );

					Adapter->State11D.CountryInfoIEFound = TRUE;

					memcpy( &(pBssDescList->CountryInfo),  pcountryinfo, 
							pcountryinfo->Len + 2 );
					HEXDUMP("11HTPC CountryInfo:", (u8 *)pcountryinfo, (int)(pcountryinfo->Len+2) );


					break;
#endif

#ifdef ENABLE_802_11H_TPC
			case POWER_CONSTRAINT:
					PRINTK("11HTPC: POWER_CONSTAINT IE Found\n");
					pBssDescList->Sensed11H = 1;
					{
						IEEEtypes_PowerConstraint_t *ppwrCons 
							= (IEEEtypes_PowerConstraint_t *)pCurrentPtr;

						/* Copy IEEEtypes_PowerConstraint_t info to pBssDescList */
						memmove(&pBssDescList->PowerConstraint, ppwrCons, 
								sizeof(IEEEtypes_PowerConstraint_t));
					}
					break;

				case POWER_CAPABILITY:
					PRINTK("11HTPC: POWER_CAPABILITY IE Found\n");
					pBssDescList->Sensed11H = 1;
					{
						IEEEtypes_PowerCapability_t *ppwrCap =
							(IEEEtypes_PowerCapability_t *)pCurrentPtr;
						
						memmove(&pBssDescList->PowerCapability, ppwrCap, 
							sizeof(IEEEtypes_PowerCapability_t));

					}
					break;

				case TPC_REQUEST:
					PRINTK("11HTPC: TPC_REQUEST IE Found\n");
					pBssDescList->Sensed11H = 1;
					break;

				case TPC_REPORT:
					PRINTK("11HTPC: TPC_REPORT IE Found\n");
					pBssDescList->Sensed11H = 1;
					{
						IEEEtypes_TPCReport_t *ptpcreport =
							(IEEEtypes_TPCReport_t *)pCurrentPtr;
						
						memmove(&pBssDescList->TPCReport, ptpcreport, 
							sizeof(IEEEtypes_TPCReport_t));
					}
					break;
#endif /* ENABLE_802_11H_TPC */

				case EXTENDED_SUPPORTED_RATES:
					// only process extended supported rate
					// if data rate is already found.
					// data rate IE should come before
					// extended supported rate IE
					if (bFoundDataRateIE) {
						unsigned char	*pRate;
						u8		ucBytesToCopy;
						if ((ucElemLen + ucRateSize) > 
							WLAN_SUPPORTED_RATES)
						
							ucBytesToCopy =
							(WLAN_SUPPORTED_RATES 
							 	- ucRateSize);
						else
							ucBytesToCopy = 
								ucElemLen;

						pRate = (unsigned char *) 
							pBssDescList->DataRates;
						pRate += ucRateSize;
						memmove(pRate,(pCurrentPtr +2), 
								ucBytesToCopy);

						pRate = (unsigned char *) 
						pBSSIDList->SupportedRates;

						pRate += ucRateSize;
						memmove(pRate,(pCurrentPtr +2), 
								ucBytesToCopy);
					}
					break;
#ifdef WPA
#define IE_ID_LEN_FIELDS_BYTES 2
				case WPA_IE:
					pIeWpa = (IE_WPA *) pCurrentPtr;
					if (!memcmp(pIeWpa->oui, oui01, 4)) {
						memcpy(pwpa_supplicant->Wpa_ie, 
							pCurrentPtr, ucElemLen + IE_ID_LEN_FIELDS_BYTES);
						pwpa_supplicant->Wpa_ie_len 
								= ucElemLen + IE_ID_LEN_FIELDS_BYTES;
					}
					HEXDUMP("Resp WPA_IE",
					pwpa_supplicant->Wpa_ie, ucElemLen);
					break;
#ifdef WPA2
				case WPA2_IE:
					pIeWpa = (IE_WPA *) pCurrentPtr;
						pwpa2_supplicant->Wpa_ie_len 
								= ucElemLen + IE_ID_LEN_FIELDS_BYTES;
						memcpy(pwpa2_supplicant->Wpa_ie, 
							pCurrentPtr, ucElemLen + IE_ID_LEN_FIELDS_BYTES);
					HEXDUMP("Resp WPA2_IE",
					pwpa2_supplicant->Wpa_ie, ucElemLen);
					break;
#endif //WPA2
#endif //WPA
				case TIM:
					break;
				
				case CHALLENGE_TEXT:
					break;

			}

			pCurrentPtr += ucElemLen + 2;
			// need to account for IE ID and IE Len
			usBytesLeftForCurrentBeacon =
				usBytesLeftForCurrentBeacon - ucElemLen - 2;
		} // while ( usBytesLeftForCurrrentBeacon > 2 )
	       // - process variable IE

		// update the length field
		pBSSIDList->Length = sizeof(WLAN_802_11_BSSID) 
							+ pBSSIDList->IELength;

		// need to be 4 byte aligned
		pBSSIDList->Length = ((pBSSIDList->Length + 3) >> 2) << 2;

		nBytesLeft -= usBeaconSize;
		ulBSSIDIndex++;

	} // while (nBytesLeft > 0 && ulBSSIDIndex <  MRVDRV_MAX_BSSID_LIST)

	PRINTK1("-InterpretBSSDescription: normal exit\n");
	return 0;
}
#endif	// end of FWVERSION3

int InterpretBSSDescription(wlan_private * priv, void *RetCommandBuffer,
		u32 ulBSSDescriptionListSize, int next_bssid_index)
{
#ifdef FWVERSION3
	wlan_adapter	*Adapter = priv->adapter;

	if (FW_IS_WPA_ENABLED(Adapter)) {
		PRINTK1("WPA Enabled in Firmware\n");
		return InterpretBSSDescriptionWithIE(priv, RetCommandBuffer,
				ulBSSDescriptionListSize, next_bssid_index);
	}
	else
		PRINTK1("WPA NOT Enabled in Firmware\n");
#endif
	return WLAN_STATUS_FAILURE;
}


int wlan_ret_reg_access(wlan_private * priv, u16 type, 
						HostCmd_DS_COMMAND * resp)
{
	wlan_adapter   *Adapter = priv->adapter;
	
	ENTER();

    	switch (type) {
	    	case HostCmd_RET_MAC_REG_ACCESS:	/* 0x8019 */
			{
				HostCmd_DS_MAC_REG_ACCESS *reg;

				reg = (PHostCmd_DS_MAC_REG_ACCESS) & 
				resp->params.macreg;

				Adapter->OffsetValue.offset = reg->Offset;
				Adapter->OffsetValue.value = reg->Value;
				break;
			}
	
	    	case HostCmd_RET_BBP_REG_ACCESS:	/* 0x801a */
			{
				HostCmd_DS_BBP_REG_ACCESS *reg;
				reg = (PHostCmd_DS_BBP_REG_ACCESS) & 
					resp->params.bbpreg;

				Adapter->OffsetValue.offset = reg->Offset;
				Adapter->OffsetValue.value = reg->Value;
				break;
			}

	    	case HostCmd_RET_RF_REG_ACCESS:	/* 0x801b */
			{
				HostCmd_DS_RF_REG_ACCESS *reg;
				reg = (PHostCmd_DS_RF_REG_ACCESS) & 
					resp->params.rfreg;
				
				Adapter->OffsetValue.offset = reg->Offset;
				Adapter->OffsetValue.value = reg->Value;
	    			break;
			}

		default:
			return -1;
	}

    	LEAVE();
    	return 0;
}
 
void MacEventDisconnected(wlan_private * priv)
{
	wlan_adapter	*Adapter = priv->adapter;
	union iwreq_data wrqu;

	ENTER();

	if (Adapter->MediaConnectStatus != WlanMediaStateConnected)
		return;
	
	memset(wrqu.ap_addr.sa_data, 0x00, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	/*
	 * Send Event to upper layer 
	 */
	wireless_send_event(priv->wlan_dev.netdev, SIOCGIWAP, &wrqu, NULL);

	netif_carrier_off(priv->wlan_dev.netdev);

	// need to clean up Rx and Tx first
	CleanUpSingleTxBuffer(priv);
	// ResetRxPDQ(priv);	//TODO
	
	PRINTK1("Current SSID=%s, Ssid Length=%u\n",
			Adapter->CurBssParams.ssid.Ssid, 
			Adapter->CurBssParams.ssid.SsidLength);
	PRINTK1("Previous SSID=%s, Ssid Length=%u\n",
			Adapter->PreviousSSID.Ssid, 
			Adapter->PreviousSSID.SsidLength);

	Adapter->CurBssParams.band = 0;
	Adapter->CurBssParams.channel = 0;
	Adapter->AdHocCreated = FALSE;

	PRINTK1("CurBssParams.ssid Length = %u\n",
			Adapter->CurBssParams.ssid.SsidLength);
	PRINTK1("WlanMediaStateDisconnected\n");
		
	// indicate to the OS that we are disconnected
	Adapter->MediaConnectStatus = WlanMediaStateDisconnected;
	Adapter->LinkSpeed = MRVDRV_LINK_SPEED_1mbps;
		
	// memorize the previous SSID and BSSID
	memcpy(&(Adapter->PreviousSSID), &(Adapter->CurBssParams.ssid),
			sizeof(WLAN_802_11_SSID));
	memcpy(Adapter->PreviousBSSID, Adapter->CurrentBSSID,
			MRVDRV_ETH_ADDR_LEN);

	memcpy(&Adapter->SpecificScanSSID, &Adapter->PreviousSSID,
		sizeof(WLAN_802_11_SSID));

	// need to erase the current SSID and BSSID info
	Adapter->ulCurrentBSSIDIndex = 0;
	memset(&(Adapter->CurBssParams.ssid), 0, sizeof(WLAN_802_11_SSID));
	memset(Adapter->CurrentBSSID, 0, MRVDRV_ETH_ADDR_LEN);
	memset(&(Adapter->CurrentBSSIDDesciptor), 0,
			sizeof(WLAN_802_11_BSSID));
	PRINTK1("CurBssParams.ssid Length = %u\n",
			Adapter->CurBssParams.ssid.SsidLength);

#ifdef	PS_REQUIRED
	//if ((Adapter->PSMode != Wlan802_11PowerModeCAM) 
	if (Adapter->PSState != PS_STATE_FULL_POWER) {
		// need to wake up the firmware
		PRINTK1("Going to invoke PSWakeup\n");
		PSWakeup(priv);
	}
#endif

#ifdef USER_DOES_PS_AFTER_REASSOCIATION
	/* If the automatic reassociation is turned off 
	 * ( i.e, user does the reassociation, the station is 
	 *  pushed out of the power save ) */
	if (Adapter->Reassociate == FALSE) {
		Adapter->PSMode = Wlan802_11PowerModeCAM;
	}
#endif
	
#ifdef ENABLE_802_11D
	wlan_802_11d_sm_transfer( priv, CMD_DISCONNECT_11D );
#endif 	
	LEAVE();
}

/*
 * Command Response Proceesing 
 */
static inline int wlan_ret_get_hw_spec(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	int			i;
	HostCmd_DS_GET_HW_SPEC	*hwspec = &resp->params.hwspec;
	wlan_adapter		*Adapter = priv->adapter;

	ENTER();

	Adapter->HardwareStatus = WlanHardwareStatusReady;
#ifdef FWVERSION3
	Adapter->fwCapInfo = wlan_le32_to_cpu(hwspec->fwCapInfo);
#endif

#ifdef MULTI_BANDS
	if (IS_SUPPORT_MULTI_BANDS(Adapter)) {
		Adapter->fw_bands		= GET_FW_DEFAULT_BANDS(Adapter);
		Adapter->is_multiband		= 1;
	} else {
		Adapter->adhoc_start_band	= BAND_B;
		Adapter->fw_bands		= BAND_B;
		Adapter->is_multiband		= 0;
	}

	Adapter->config_bands = Adapter->fw_bands; 

	memset(&SupportedRates, 0, sizeof(SupportedRates));

	if (Adapter->fw_bands & BAND_A) {
		Adapter->Channel = DEFAULT_CHANNEL_A;	// default
		Adapter->adhoc_start_band = BAND_A;
		Adapter->AdhocChannel	  = DEFAULT_AD_HOC_CHANNEL_A;
//		memcpy(&SupportedRates, &SupportedRates_A,
//					sizeof(SupportedRates_A));
	} else if (Adapter->fw_bands & BAND_G) {
		Adapter->Channel = DEFAULT_CHANNEL;	// default
		Adapter->adhoc_start_band = BAND_G;
		Adapter->AdhocChannel	  = DEFAULT_AD_HOC_CHANNEL;
//		memcpy(&SupportedRates, &SupportedRates_G,
//					sizeof(SupportedRates_G));
	} else if (Adapter->fw_bands & BAND_B) {
		Adapter->Channel = DEFAULT_CHANNEL;	// default
		Adapter->adhoc_start_band = BAND_B;
		Adapter->AdhocChannel	  = DEFAULT_AD_HOC_CHANNEL;
//		memcpy(&SupportedRates, &SupportedRates_B,
//					sizeof(SupportedRates_B));
	}
#endif

	// permanent address should only be set once at start up
	if (Adapter->PermanentAddr[0] == 0xff) {
		// permanent address has not been set yet, set it
		memcpy(Adapter->PermanentAddr, hwspec->PermanentAddr,
							MRVDRV_ETH_ADDR_LEN);
	}

	memcpy(priv->wlan_dev.netdev->dev_addr, hwspec->PermanentAddr,
								ETH_ALEN);
	Adapter->FWReleaseNumber = hwspec->FWReleaseNumber;

	PRINTK1("FWReleaseVersion: 0x%X\n", Adapter->FWReleaseNumber);
	PRINTK1("Permanent addr: %2x:%2x:%2x:%2x:%2x:%2x\n",
			hwspec->PermanentAddr[0], hwspec->PermanentAddr[1],
			hwspec->PermanentAddr[2], hwspec->PermanentAddr[3],
			hwspec->PermanentAddr[4], hwspec->PermanentAddr[5]);
	PRINTK("HWIfVersion=0x%X  Version=0x%X\n",hwspec->HWIfVersion,
							hwspec->Version);

#ifdef HARDCODED_REGION_CODE
	Adapter->RegionTableIndex = 0;
	Adapter->RegionCode = 0x10;
#else
	// Get the region code 
	Adapter->RegionCode = wlan_le16_to_cpu(hwspec->RegionCode) >> 8;
#endif

	for (i = 0; i < MRVDRV_MAX_REGION_CODE; i++) {
		// use the region code to search for the index
		if (Adapter->RegionCode == RegionCodeToIndex[i]) {
			Adapter->RegionTableIndex = (u16) i;
			break;
		}
	}

	// if it's unidentified region code, use the default (USA)
	if (i >= MRVDRV_MAX_REGION_CODE) {
		Adapter->RegionCode = 0x10;
		Adapter->RegionTableIndex = 0;
	}

	if (Adapter->CurrentAddr[0] == 0xff) {
		memmove(Adapter->CurrentAddr, hwspec->PermanentAddr,
				MRVDRV_ETH_ADDR_LEN);
	}

#ifdef MULTI_BANDS
	if (wlan_set_regiontable(priv, Adapter->RegionCode, 
							Adapter->fw_bands)) {
		LEAVE();
		return -EINVAL;
	}
#else
	if (wlan_set_regiontable(priv, Adapter->RegionCode, 0)) {
		LEAVE();
		return -EINVAL;
	}
#endif

	// Config for extended scan
	if (Adapter->ExtendedScan) {
		Adapter->pExtendedScanParams->ucNumValidChannel = 
			(u8)	(Adapter->cur_region_channel->NrCFP - 1);
		PRINTK1("ExtendedScan: there are %d valid channels\n",
				Adapter->pExtendedScanParams->
				ucNumValidChannel);
	}

	LEAVE();
	return 0;
}

#ifdef PS_REQUIRED
static inline int wlan_ret_802_11_ps_mode(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	ENTER();
	
	LEAVE();
	return 0;
}
#endif

static inline void AppendCurrentSSID(wlan_private *priv, int i)
{
	wlan_adapter	*Adapter = priv->adapter;

	if (Adapter->SetSpecificScanSSID != TRUE) {
		
		PRINTK1("HWAC-Append current SSID to " "SCAN list\n");

		if (i < MRVDRV_MAX_BSSID_LIST) {
			
			Adapter->ulCurrentBSSIDIndex = i;
			Adapter->ulNumOfBSSIDs++;
			memcpy(&Adapter->BSSIDList[i],
					&Adapter->CurrentBSSIDDesciptor,
					sizeof(WLAN_802_11_BSSID));

#ifdef MULTI_BANDS
			Adapter->BSSIDList[i].bss_band = Adapter->CurBssParams.band;
#endif
			Adapter->BSSIDList[i].Channel = Adapter->CurBssParams.channel;

			if (Adapter->SecInfo.WEPStatus == Wlan802_11WEPEnabled) {
				Adapter->BSSIDList[i].Privacy = 
					Wlan802_11PrivFilter8021xWEP;
			} else {
				Adapter->BSSIDList[i].Privacy =
					Wlan802_11PrivFilterAcceptAll;
			}

		}
	}
} // end of AppendCurrentSSID

#ifdef ENABLE_802_11D
/*Domain Info response process*/
static inline int wlan_ret_802_11d_domain_info(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11D_DOMAIN_INFO_RSP	*domaininfo = &resp->params.domaininforesp;
	MrvlIEtypes_DomainParamSet_t	*domain = &domaininfo->Domain;
	u16				Action	= wlan_le16_to_cpu(domaininfo->Action);
	int	ret=0;
	int NoOfSubband = 0;

	ENTER();

	HEXDUMP( "11D DOMAIN Info Rsp Data:", (u8 *)resp, 
					(int)wlan_le16_to_cpu(resp->Size) ); 	

	NoOfSubband = ( domain->Header.Len - 3 ) / sizeof(IEEEtypes_SubbandSet_t);
	/* countrycode 3 bytes */	

	PRINTK2("11D Domain Info Resp: NoOfSubband=%d\n", NoOfSubband);

	if (NoOfSubband > MRVDRV_MAX_SUBBAND_802_11D ) {
		PRINTK2("Invalid Numrer of Subband returned!!\n");
		return -1;
	}

	switch (Action) {
	case HostCmd_ACT_SET: /*Proc Set Action */
		break;
	
	case HostCmd_ACT_GET: 
	        break;
	default:
		PRINTK2("Invalid Action:%d\n", domaininfo->Action );
		ret = -1;
		break;
	}

	LEAVE();
	return ret;	
}

#endif

#ifdef ENABLE_802_11H_TPC
/* processinf the TPC Request return */
static inline int  wlan_ret_802_11h_tpc_request( wlan_private *priv, HostCmd_DS_COMMAND *resp)
{
	HostCmd_DS_802_11H_TPC_REQUEST_RSP	*ptpcret = &resp->params.tpcrequestresp;
	wlan_adapter   *Adapter = priv->adapter;
	wlan_802_11h_tpc_request_param_t	*preqParam = &(Adapter->State11HTPC.RequestTpc); 

	ENTER();

	HEXDUMP( "11HTPC: 11H TPC REQUEST Rsp Data:", (u8 *)resp, (int)resp->Size ); 	

	PRINTK(" TPCRetCode=0x%x, TxPower=0x%x, LinkMargin=0x%x, RSSI=0x%x\n",
		ptpcret-> TPCRetCode, ptpcret-> TxPower, ptpcret-> LinkMargin, ptpcret->RSSI);

	preqParam ->RetCode 	= ptpcret-> TPCRetCode;
	preqParam ->TxPower 	= ptpcret-> TxPower;
	preqParam ->LinkMargin	= ptpcret-> LinkMargin;
	preqParam ->RSSI	= ptpcret->RSSI;

	LEAVE();
	return 0;	
}

/* processinf the TPC Request return */
static inline int  wlan_ret_802_11h_tpc_info( wlan_private *priv, HostCmd_DS_COMMAND *resp)
{
/*Ignore the return resule. May need to check the return codeNo any process*/
	ENTER();

	HEXDUMP( "11HTPC: 11H TPC INFO Rsp Data:", (u8 *)resp, (int)resp->Size ); 	

	LEAVE();
	return 0;	
}
#endif /* ENABLE_802_11H_TPC */


static int wlan_ret_802_11_sleep_params(wlan_private * priv,
			      			HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_SLEEP_PARAMS 	*sp = &resp->params.sleep_params;
	wlan_adapter   			*Adapter = priv->adapter;

	ENTER();

	PRINTK("error=%x offset=%x stabletime=%x calcontrol=%x\n" 
			" extsleepclk=%x\n", sp->Error, sp->Offset, 
			sp->StableTime, sp->CalControl, sp->ExternalSleepClk);
	Adapter->sp_error = wlan_le16_to_cpu(sp->Error);
	Adapter->sp_offset = wlan_le16_to_cpu(sp->Offset);
	Adapter->sp_stabletime = wlan_le16_to_cpu(sp->StableTime);
	Adapter->sp_calcontrol = wlan_le16_to_cpu(sp->CalControl);
	Adapter->sp_extsleepclk = (sp->ExternalSleepClk);

	LEAVE();
	return 0;
}

static inline int wlan_ret_802_11_scan(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	int				i, j, k, TempV;
	int				ndStat;
	// union iwreq_data		wrqu;

	HostCmd_DS_802_11_SCAN_RSP	*scan; 

	wlan_adapter			*Adapter = priv->adapter;
	u16				Flag = 0;
	u16				BSSDescriptSize;
	u16				IncrementSize = 8;

	ENTER();

#ifdef BG_SCAN
	if(priv->adapter->bgScanConfig->Enable) {
		scan = (HostCmd_DS_802_11_SCAN_RSP *) &resp->params.
					bgscanqueryresp.scanresp;
        } else {
#endif /* BG_SCAN */
                scan = &resp->params.scanresp;
#ifdef BG_SCAN
	}
#endif /* BG_SCAN */

	BSSDescriptSize = wlan_le16_to_cpu(scan->BSSDescriptSize);
	IncrementSize += sizeof(HostCmd_DS_802_11_SCAN_RSP);

	PRINTK1("BSSDescriptSize %d\n", BSSDescriptSize);
	PRINTK1("Scan returned %d AP before parsing\n", scan->NumberOfSets);

	if (scan->NumberOfSets > MRVDRV_MAX_BSSID_LIST) {
		PRINTK1("Invalid number of AP returned!!\n");
		return -1;
	}
	
	/* Newly added to send an event to supplicant after scan done */
	//wireless_send_event(priv->wlan_dev.netdev, SIOCGIWSCAN, &wrqu , NULL);
	//PRINTK1("Sending event to the supplicant");

	// Get number of BSS Descriptors
	i = Adapter->ulNumOfBSSIDs;
	Adapter->ulNumOfBSSIDs += scan->NumberOfSets;
	ndStat = InterpretBSSDescription(priv,
			(Adapter->CurCmd->BufVirtualAddr + IncrementSize),
			BSSDescriptSize, i);

	if (ndStat != WLAN_STATUS_SUCCESS) {
		PRINTK1("ERROR: InterpretBSSDescription returned ERROR");
	}
	
#ifndef TLV_SCAN 
	// Parse RSSI array
	for (j = 0; i < Adapter->ulNumOfBSSIDs; j++,i++) {
		Adapter->BSSIDList[i].Rssi =
			(LONG) (scan->RSSI[j]) + 
			MRVDRV_RSSI_DEFAULT_NOISE_VALUE;
#ifdef MULTI_BANDS
		Adapter->BSSIDList[i].bss_band = Adapter->cur_region_channel->Band;
#endif
	}
#endif /*TLV_SCAN*/

#ifdef TLV_SCAN
	for (j = 0; j < scan->NumberOfSets; j++, i++) {
#ifdef MULTI_BANDS
		Adapter->BSSIDList[i].bss_band = Adapter->cur_region_channel->Band;
#endif
	}
#endif /*TLV_SCAN*/

	PRINTK1("Scan Results: number of BSSID: %d\n", 
			Adapter->ulNumOfBSSIDs);
    
#if DEBUG
#ifdef PROGRESSIVE_SCAN
	if (Adapter->ScanChannelsLeft <= 0) {
#endif
		for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
			PRINTK1("%2d:%32s-%02x:%02x:%02x:%02x:%02x:%02x "
					"RSSI=%d SR=%x\n", i,
					Adapter->BSSIDList[i].Ssid.Ssid,
					Adapter->BSSIDList[i].MacAddress[0],
					Adapter->BSSIDList[i].MacAddress[1],
					Adapter->BSSIDList[i].MacAddress[2],
					Adapter->BSSIDList[i].MacAddress[3],
					Adapter->BSSIDList[i].MacAddress[4],
					Adapter->BSSIDList[i].MacAddress[5],
					(int) Adapter->BSSIDList[i].Rssi,
					Adapter->
					BSSIDList[i].SupportedRates[i]);
			// Adapter->BSSIDList[i].Privacy ? "On" : "Off"); 
		}
#ifdef PROGRESSIVE_SCAN
	}
#endif
#endif

	// Check whole BSSID list string if there's invalid character.
	/*
	 * If there is remove the entry from the list 
	 */
	for (i = 0; i < Adapter->ulNumOfBSSIDs || Flag; i++) {
		// We'll re-check the first one when the previous first
		// one has been discard.
		if (Flag == 1) {
			Flag = 0;
			i--;
		}
		// Check if there's invalid data rate.
		for (k = 0, TempV = 0; k < 8; k++) {
			TempV |= Adapter->BSSIDList[i].SupportedRates[k];
		}

		// Check the invalid SSID.
		for (j = 0; j < Adapter->BSSIDList[i].Ssid.SsidLength; j++) {
			if ((Adapter->BSSIDList[i].Ssid.Ssid[j] < 0x20)) {
				PRINTK1("INVALID BSSID DISCARDING\n");
				Flag = 1;
				break;
			}
		}

		if (Flag == 1) {
			if ((i + 1) == Adapter->ulNumOfBSSIDs) {
				PRINTK1("INVALID BSSID DISCARDING"
				 " LAST(%d) %s\n", i, Adapter->
				 BSSIDList[i].Ssid.Ssid);
				memset(&(Adapter->BSSIDList[i]), 0,
						sizeof(WLAN_802_11_BSSID));
				Adapter->ulNumOfBSSIDs -= 1;
				Flag = 0;
				break;
			} else {
				// We start replace the current invalid SSID
				// with last SSID in array.
				PRINTK1("INVALID BSSID DISCARDING CURRENT(%d) "
						"%s\n", i, Adapter->
						BSSIDList[i].Ssid.Ssid);
				memcpy(&Adapter->BSSIDList[i],
						&Adapter->BSSIDList[Adapter->
						ulNumOfBSSIDs - 1],
						sizeof(WLAN_802_11_BSSID));
				memset(&Adapter->BSSIDList[Adapter->
						ulNumOfBSSIDs - 1], 0,
						sizeof(WLAN_802_11_BSSID));
				if (i) {
					Adapter->ulNumOfBSSIDs --;
					i--;
					Flag = 0;
				}
			}
		}
	}
	
	// if currently connected to an AP or an Ad Hoc network
	if (Adapter->CurBssParams.ssid.SsidLength != 0 &&
			Adapter->CurBssParams.ssid.Ssid[0] > 0x20) {
		// try to find the current SSID in the new scan list
		for (i = 0; i < Adapter->ulNumOfBSSIDs; i++) {
			if (!SSIDcmp(&Adapter->BSSIDList[i].Ssid,
						&Adapter->CurBssParams.ssid) &&
					!memcmp(Adapter->CurrentBSSID,
						Adapter->BSSIDList[i].
						MacAddress,
						MRVDRV_ETH_ADDR_LEN)) {
				break;
			}
		}
		
		// if we found matching SSID, update the index
		if (i < Adapter->ulNumOfBSSIDs) {
			// Set the attempted BSSID Index to current
			Adapter->ulCurrentBSSIDIndex = i;
			// Set the new BSSID (AP's MAC address) to current
			// BSSID
			memcpy(Adapter->CurrentBSSID,
					&(Adapter->BSSIDList[Adapter->
						ulCurrentBSSIDIndex].MacAddress
						), MRVDRV_ETH_ADDR_LEN);
			
			// Make a copy of current BSSID descriptor
			memcpy(&Adapter->CurrentBSSIDDesciptor,
					&Adapter->BSSIDList[Adapter->
					ulCurrentBSSIDIndex],
					sizeof(WLAN_802_11_BSSID));
			// Set the new configuration to the current config
			memcpy(&Adapter->CurrentConfiguration,
					&Adapter->BSSIDList[Adapter->
					ulCurrentBSSIDIndex].Configuration, 
					sizeof(WLAN_802_11_CONFIGURATION));
		}
		// if the current associated SSID is not contained in the
		// list, append it
		else {
#ifdef PROGRESSIVE_SCAN
			if (Adapter->ScanChannelsLeft == 0) {
#endif
				AppendCurrentSSID(priv, i);
#ifdef PROGRESSIVE_SCAN				
			}
#endif
		}
	}

	PRINTK1("HWAC - Scanned %2d APs\n", Adapter->ulNumOfBSSIDs);

	LEAVE();
	return 0;
}

static int wlan_ret_mac_control(wlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	return 0;
}

static int wlan_ret_802_11_associate(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_ASSOCIATE_RSP	*rasst = &resp->params.associatersp;
	wlan_adapter			*Adapter = priv->adapter;
	union iwreq_data		wrqu;
	u16				ResultCode = wlan_le16_to_cpu(rasst->ResultCode);

	ENTER();

	Adapter->bIsAssociationInProgress = FALSE;

    	// if there is a reset pending
    	if (Adapter->bIsPendingReset == TRUE) {
		Adapter->bIsPendingReset = FALSE;
		
		PRINTK1("HWAC - Sending ResetComplete\n");
		Adapter->HardwareStatus = WlanHardwareStatusReady;
		
		SetMacPacketFilter(priv);
    	}

    	// if association result code != 0, will return Failure later
    	if (ResultCode) {
		PRINTK1("HWAC - Association Failed, code = %d\n",
				rasst->ResultCode);
		if (Adapter->MediaConnectStatus == 
				WlanMediaStateConnected) {
			MacEventDisconnected(priv);
		}
	
		ResetSingleTxDoneAck(priv);
#ifdef WPA  
        if (ResultCode == 4)
          send_bad_auth_event(priv);
#endif
		wake_up_interruptible(&Adapter->association_q); 
		return 0;
	}

#ifdef WPA  //added on 4 July 2004
	if (FW_IS_WPA_ENABLED(Adapter)) {
		//copy the association result info to the 
		//ASSOCIATION_INFO buffer
		PWLAN_802_11_ASSOCIATION_INFORMATION pInfo;
	
		pInfo = (PWLAN_802_11_ASSOCIATION_INFORMATION)
			Adapter->AssocInfoBuffer;

		//only currently copy the fixed IE
		//Todo : copy the entire IE once it is available
		memcpy(&pInfo->ResponseFixedIEs.Capabilities, 
				&rasst->CapInfo, sizeof(rasst->CapInfo));
		pInfo->ResponseFixedIEs.StatusCode =
			ResultCode;
		pInfo->ResponseFixedIEs.AssociationId =
			wlan_le16_to_cpu(rasst->AssociationID);
		pInfo->AvailableResponseFixedIEs |=
				WLAN_802_11_AI_RESFI_CAPABILITIES;
		pInfo->AvailableResponseFixedIEs |=
				WLAN_802_11_AI_RESFI_STATUSCODE;
		pInfo->AvailableResponseFixedIEs |=
				WLAN_802_11_AI_RESFI_ASSOCIATIONID;
		pInfo->OffsetResponseIEs = pInfo->OffsetRequestIEs + 
				pInfo->RequestIELength;
		pInfo->ResponseIELength = wlan_le16_to_cpu(rasst->IELength);
#ifdef NEW_ASSOCIATION_RSP
		memmove(Adapter->AssocInfoBuffer + pInfo->OffsetResponseIEs, 
				(u8 *)&(rasst->RateID), wlan_le16_to_cpu(rasst->IELength));
#else
		memmove(Adapter->AssocInfoBuffer + pInfo->OffsetResponseIEs, 
				rasst->IE, wlan_le16_to_cpu(rasst->IELength));
#endif
		PRINTK1("Association Result IE: CapInfo = 0x%x,"
				"StatusCode = 0x%x,AssociationID = 0x%x\n",
				pInfo->ResponseFixedIEs.Capabilities,
				pInfo->ResponseFixedIEs.StatusCode,
				pInfo->ResponseFixedIEs.AssociationId);
	}
#endif	// WPA
	
	HEXDUMP("Association response:", 
			(void*)rasst, sizeof(HostCmd_DS_802_11_ASSOCIATE_RSP));
	
	// Send a Media Connected event, according to the Spec
	Adapter->MediaConnectStatus = WlanMediaStateConnected;
	Adapter->LinkSpeed = MRVDRV_LINK_SPEED_11mbps;
	
	// Set the attempted BSSID Index to current
	Adapter->ulCurrentBSSIDIndex = Adapter->ulAttemptedBSSIDIndex;
	
	PRINTK1("Associated BSSIDList[%d]=%s\n", Adapter->ulCurrentBSSIDIndex,
			Adapter->BSSIDList[Adapter->
			ulCurrentBSSIDIndex].Ssid.Ssid);
	
	// Set the new SSID to current SSID
	memcpy(&Adapter->CurBssParams.ssid, &Adapter->
			BSSIDList[Adapter-> ulCurrentBSSIDIndex].Ssid,
			sizeof(WLAN_802_11_SSID));
	
	// Set the new BSSID (AP's MAC address) to current BSSID
	memcpy(Adapter->CurrentBSSID, &Adapter->BSSIDList
			[Adapter->ulCurrentBSSIDIndex].MacAddress,
			MRVDRV_ETH_ADDR_LEN);
	
	// Make a copy of current BSSID descriptor
	memcpy(&Adapter->CurrentBSSIDDesciptor,
			&Adapter->BSSIDList[Adapter->ulCurrentBSSIDIndex],
			sizeof(WLAN_802_11_BSSID));

	/* Copy associate'd bssid into Current BSS State struct */
	memcpy(&Adapter->CurBssParams.bssid, 
			&Adapter->BSSIDList[Adapter-> ulCurrentBSSIDIndex],
			sizeof(WLAN_802_11_BSSID));
	
	if (Adapter->ulCurrentBSSIDIndex < MRVDRV_MAX_BSSID_LIST) {
		// Set the new configuration to the current config
		memcpy(&Adapter->CurrentConfiguration,
				&Adapter->BSSIDList[Adapter->
				ulCurrentBSSIDIndex].
				Configuration, 
				sizeof(WLAN_802_11_CONFIGURATION));
	}
	
	// if the periodic timer is on, cancel it
	if (Adapter->TimerIsSet == TRUE) {
		CancelTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}
	
	// Association causes the MAC to lose the flag such as
	// promiscuous mode on, need to set the flag again
	if (Adapter->CurBssParams.ssid.Ssid[0] == '\0' ||
			!Adapter->CurBssParams.ssid.SsidLength) {
		memcpy(&Adapter->CurBssParams.ssid, &Adapter->PreviousSSID,
				sizeof(WLAN_802_11_SSID));
	
		memcpy(&Adapter->CurrentBSSID, &Adapter->PreviousBSSID,
				ETH_ALEN);
	}	
	
	PRINTK1("Value of the mac controller-> %x\n",
			Adapter->CurrentPacketFilter);
	
	Adapter->MediaConnectStatus = WlanMediaStateConnected;
	
#ifdef WPA
	if (Adapter->SecInfo.WPAEnabled
#ifdef WPA2
		|| Adapter->SecInfo.WPA2Enabled
#endif
	)
		Adapter->IsGTK_SET = FALSE;
#endif
	
	wake_up_interruptible(&Adapter->association_q); 
	
	// Do NOT SetMacPacketFilter after association 7/23/04
	// SetMacPacketFilter(priv);
	
	// Adapter->RxDataSNR = Adapter->RxDataNF = 0;
	Adapter->SNR[TYPE_RXPD][TYPE_AVG] = 
		Adapter->NF[TYPE_RXPD][TYPE_AVG] = 0;

	Adapter->AvgPacketCount = 0;

	netif_carrier_on(priv->wlan_dev.netdev);
	
	PRINTK1("HWAC - Associated \n");

	memcpy(wrqu.ap_addr.sa_data, Adapter->CurrentBSSID, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(priv->wlan_dev.netdev, SIOCGIWAP, &wrqu, NULL); 

	LEAVE();
	return 0;
}

static int wlan_ret_802_11_disassociate(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	ENTER();
	
	MacEventDisconnected(priv);
	
	LEAVE();
	return 0;
}

static int wlan_ret_802_11_ad_hoc_stop(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	ENTER();
	
	MacEventDisconnected(priv);
	
	LEAVE();
	return 0;
}

static int wlan_ret_802_11_set_wep(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	/*
	 * Nothing to be done here 
	 */
	return 0;
}

static int wlan_ret_802_11_ad_hoc_start(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	int				ret = 0;
	wlan_adapter			*Adapter = priv->adapter;
	u16				Command = wlan_le16_to_cpu(resp->Command);
	HostCmd_DS_802_11_AD_HOC_RESULT	*pAdHocResult = &resp->params.result;
	u16				Result = wlan_le16_to_cpu(resp->Result);

	ENTER();

	PRINTK1("Size = %d\n", wlan_le16_to_cpu(resp->Size));
	PRINTK1("Command = %x\n", Command);
	PRINTK1("Result = %x\n", Result);

	Adapter->bIsAssociationInProgress = FALSE;

	// Send the setmacfilter here 
	// if there is a reset pending
	if (Adapter->bIsPendingReset == TRUE) {
		Adapter->bIsPendingReset = FALSE;
		PRINTK1("HWAC - Sending ResetComplete\n");
		Adapter->HardwareStatus = WlanHardwareStatusReady;
		SetMacPacketFilter(priv);
	}

	// Join result code 0 --> SUCCESS
	// if join result code != 0, will return Failure later
	if (Result) {
		PRINTK1("HWAC Join or Start Command Failed\n");
		if (Adapter->MediaConnectStatus == WlanMediaStateConnected) {
			MacEventDisconnected(priv);
		}

		if (Command == HostCmd_RET_802_11_AD_HOC_JOIN) {
			Adapter->m_NumAssociationAttemp++;
			if (Adapter->m_NumAssociationAttemp >= 2) {
				// do not attemp any more
				return 0;
			}

			ret = PrepareAndSendCommand(priv,
					HostCmd_CMD_802_11_AD_HOC_JOIN, 0,
					HostCmd_OPTION_USE_INT, (WLAN_OID) 0,
					HostCmd_PENDING_ON_CMD,
					&(Adapter->AttemptedSSIDBeforeScan));

			if (ret) {
				LEAVE();
				return ret;
			}
		}
		// If it's a START command and it fails, 
		// remove the entry on BSSIDList nad BssDescList
		else if (Command == HostCmd_RET_802_11_AD_HOC_START) {
			memset(&(Adapter->BSSIDList[Adapter->
						ulAttemptedBSSIDIndex]), 0,
					sizeof(WLAN_802_11_BSSID));

			memset(&(Adapter->BssDescList[Adapter->
						ulAttemptedBSSIDIndex]), 0,
					sizeof(BSS_DESCRIPTION_SET_ALL_FIELDS));
			Adapter->ulNumOfBSSIDs--;
		}

		Adapter->AdHocFailed = TRUE;

		return 0;
	}
	// Now the join cmd should be successful
	// If BSSID has changed use SSID to compare instead of BSSID
	PRINTK1("Associated with %s\n",
			Adapter->BSSIDList[Adapter->
			ulAttemptedBSSIDIndex].Ssid.Ssid);

	if (memcmp(Adapter->CurBssParams.ssid.Ssid,
				Adapter->BSSIDList[Adapter->
				ulAttemptedBSSIDIndex].Ssid.Ssid,
				Adapter->BSSIDList[Adapter->
				ulAttemptedBSSIDIndex].Ssid.SsidLength)) {
		// Send a Media Connected event, according to the Spec
		Adapter->MediaConnectStatus = WlanMediaStateConnected;
		Adapter->LinkSpeed = MRVDRV_LINK_SPEED_11mbps;

		// NOTE: According to spec, WLAN_STATUS_MEDIA_CONNECT should
		// be indicated only when the first client joins, not
		// when the network is started.  Therefore, CONNECT
		// should not be indicated if the command was AD_HOC_START
		// Set the attempted BSSID Index to current
		Adapter->ulCurrentBSSIDIndex = Adapter->ulAttemptedBSSIDIndex;

		// Set the new SSID to current SSID
		memmove(&(Adapter->CurBssParams.ssid), 
				&(Adapter->BSSIDList[Adapter->
					ulCurrentBSSIDIndex].Ssid),
				sizeof(WLAN_802_11_SSID));

		// If it's a Start command, 
		// set the BSSID to the returned value
		if (Command == HostCmd_RET_802_11_AD_HOC_START) {
			memmove(&(Adapter->BSSIDList[Adapter->
						ulCurrentBSSIDIndex].
						MacAddress),
					pAdHocResult->BSSID, 
					MRVDRV_ETH_ADDR_LEN);

			Adapter->AdHocCreated = TRUE;
		}

		if (Adapter->ulCurrentBSSIDIndex < MRVDRV_MAX_BSSID_LIST) {
			// Set the new BSSID (AP's MAC address) to current BSSID
			memmove(Adapter->CurrentBSSID,
					&(Adapter->BSSIDList[Adapter->
						ulCurrentBSSIDIndex].
						MacAddress),
					MRVDRV_ETH_ADDR_LEN);

			// Make a copy of current BSSID descriptor
			memmove(&(Adapter->CurrentBSSIDDesciptor),
					&(Adapter->BSSIDList[Adapter->
						ulCurrentBSSIDIndex]),
					sizeof(WLAN_802_11_BSSID));

			/* Copy adhoc'd bssid into Current BSS State struct */
			memcpy(&Adapter->CurBssParams.bssid, &Adapter->
				BSSIDList[Adapter-> ulCurrentBSSIDIndex],
				sizeof(WLAN_802_11_BSSID));

			// Set the new configuration to the current config
			memmove(&Adapter->CurrentConfiguration,
					&(Adapter->BSSIDList[Adapter->
						ulCurrentBSSIDIndex].
						Configuration), 
					sizeof(WLAN_802_11_CONFIGURATION));
		}
	}
	// if the periodic timer is on, cancel it
	if (Adapter->TimerIsSet == TRUE) {
		CancelTimer(&Adapter->MrvDrvTimer);
		Adapter->TimerIsSet = FALSE;
	}

	Adapter->m_NumAssociationAttemp = 0;

	netif_carrier_on(priv->wlan_dev.netdev);

	PRINTK1("HWAC - Joined/Started Ad Hoc\n");
	PRINTK1("Ad-Hoc Channel = %d\n", Adapter->AdhocChannel);
	PRINTK1("BSSID  %x:%x:%x:%x:%x:%x\n",
			pAdHocResult->BSSID[0], pAdHocResult->BSSID[1],
			pAdHocResult->BSSID[2], pAdHocResult->BSSID[3],
			pAdHocResult->BSSID[4], pAdHocResult->BSSID[5]);

	LEAVE();
	return ret;
}

static int wlan_ret_802_11_reset(wlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	wlan_adapter   *Adapter = priv->adapter;

    	ENTER();
    	PRINTK1("HWAC - Reset command successful\n");

    	// We are going to rely on PreviousSSID to do association again,
    	// so if it's zero, we need to return ResetComplete instead of 
    	// doing more association 

	if (!Adapter->PreviousSSID.SsidLength) {
		// if there is a reset pending
		if (Adapter->bIsPendingReset == TRUE) {
			Adapter->bIsPendingReset = FALSE;
			PRINTK1("HWAC - Sending ResetComplete\n");
			Adapter->HardwareStatus = WlanHardwareStatusReady;
		}
	}

	return 0;
}

static int wlan_ret_802_11_query_traffic(wlan_private * priv,
		HostCmd_DS_COMMAND * resp)
{
	/*
	 * There is nothing to be done here !! 
	 */
	return 0;
}

static int wlan_ret_802_11_status_info(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_QUERY_STATUS	*pStatus = &resp->params.qstatus;
	wlan_adapter			*Adapter = priv->adapter;

	Adapter->HardwareStatus		= WlanHardwareStatusReady;
	Adapter->LinkSpeed		= wlan_le32_to_cpu(pStatus->MaxLinkSpeed);
	Adapter->FWStatus		= wlan_le16_to_cpu(pStatus->FWStatus);
	Adapter->MACStatus		= wlan_le16_to_cpu(pStatus->MACStatus);
	Adapter->RFStatus		= wlan_le16_to_cpu(pStatus->RFStatus);
	Adapter->CurrentChannel		= wlan_le16_to_cpu(pStatus->CurrentChannel);

	return 0;
}

static int wlan_ret_802_11_authenticate(wlan_private * priv,
			     HostCmd_DS_COMMAND * resp)
{
	/*
	 * TODO Further processing of data 
	 */

	return 0;
}

static int wlan_ret_802_11_stat(wlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_GET_STAT	*p11Stat = &resp->params.gstat;
	wlan_adapter			*Adapter = priv->adapter;
	
	/* TODO Convert it to Big endian befor copy */
	memcpy(&Adapter->wlan802_11Stat, p11Stat, 
					sizeof(HostCmd_DS_802_11_GET_STAT));
	return 0;
}

static int wlan_ret_802_3_stat(wlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_3_GET_STAT	*p3Stat = &resp->params.gstat_8023;
	wlan_adapter			*Adapter = priv->adapter;

	ENTER();

	/* TODO Convert it to Big endian befor copy */
	memcpy(&Adapter->wlan802_3Stat, p3Stat, 
			sizeof(HostCmd_DS_802_3_GET_STAT));
	
	PRINTK1("Xmitok = %d\n Rcvok = %d\n XmitErrot %d\n RcvError %d\n" 
			" RcvNoBuffer %d\n RcvCRCError %d\n", 
			Adapter->wlan802_3Stat.XmitOK, 
			Adapter->wlan802_3Stat.RcvOK,        
			Adapter->wlan802_3Stat.XmitError, 
			Adapter->wlan802_3Stat.RcvError,
			Adapter->wlan802_3Stat.RcvNoBuffer, 
			Adapter->wlan802_3Stat.RcvCRCError);
	LEAVE();
	return 0;
}

static int wlan_ret_802_11_snmp_mib(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
#ifdef DEBUG_LEVEL1
	HostCmd_DS_802_11_SNMP_MIB	*smib = &resp->params.smib;
	wlan_adapter			*Adapter = priv->adapter;
	u16				OID = wlan_le16_to_cpu(smib->OID);
#endif

	ENTER();

	PRINTK1("value of the OID = %x\n", OID);
	PRINTK1("Buf size  = %x\n", wlan_le16_to_cpu(smib->BufSize));

#ifdef DEBUG_LEVEL1
	if (OID == OID_802_11_RTS_THRESHOLD) {
		PRINTK1("Adapter->RTSThsd =%u\n", Adapter->RTSThsd);
	} else if (OID == OID_802_11_FRAGMENTATION_THRESHOLD) {
		PRINTK1("Adapter->FragThsd =%u\n", Adapter->FragThsd);
	} else if (OID == OID_802_11_INFRASTRUCTURE_MODE) {
		PRINTK1("Adapter->InfrastructureMode = %x\n",
				Adapter->InfrastructureMode);
		PRINTK1("set to adhoc /infra mode\n");
	}
#endif

	LEAVE();
	return 0;
}

static int wlan_ret_802_11_radio_control(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	ENTER();

	LEAVE();
	return 0;
}

#ifdef BCA
static int wlan_ret_bca_config(wlan_private * priv,
			      HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_BCA_CONFIG 	*pBca = &resp->params.bca_config;
	wlan_adapter   		*Adapter = priv->adapter;

	ENTER();

	Adapter->bca_mode = wlan_le16_to_cpu(pBca->Mode);
	Adapter->bca_antenna = wlan_le16_to_cpu(pBca->Antenna);
	Adapter->bca_btfreq = wlan_le16_to_cpu(pBca->BtFreq);
	Adapter->bca_txpl32 = wlan_le32_to_cpu(pBca->TxPriorityLow32);
	Adapter->bca_txph32 = wlan_le32_to_cpu(pBca->TxPriorityHigh32);
	Adapter->bca_rxpl32 = wlan_le32_to_cpu(pBca->RxPriorityLow32);
	Adapter->bca_rxph32 = wlan_le32_to_cpu(pBca->RxPriorityHigh32);

	PRINTK("ret_bca: mode=%X ant=%X btfreq=%X TxL=%X TxH=%X"
			" RxL=%X RxH=%X\n",pBca->Mode,pBca->Antenna,
			pBca->BtFreq,pBca->TxPriorityLow32,
			pBca->TxPriorityHigh32,pBca->RxPriorityLow32,
			pBca->RxPriorityHigh32);
	LEAVE();
	return 0;
}
#endif

#ifdef	PS_REQUIRED
static int wlan_ret_set_pre_tbtt(wlan_private *priv, 
					HostCmd_DS_COMMAND *resp)
{
#ifdef	DEBUG_LEVEL1
	HostCmd_DS_802_11_PRE_TBTT	*pTbtt = &resp->params.pretbtt;
#endif

	ENTER();

	PRINTK1("PS Wakeup Time %d\n", wlan_le16_to_cpu(pTbtt->Value));

	LEAVE();
	return 0;
}
#endif 

#ifdef ADHOCAES
int HandleAdhocEncryptResponse(wlan_private *priv, 
					PHostCmd_DS_802_11_PWK_KEY pEncKey)
{
	ENTER();

	wlan_adapter	*Adapter	= priv->adapter;

	Adapter->pwkkey.Action = pEncKey->Action;

	memcpy(Adapter->pwkkey.TkipEncryptKey, pEncKey->TkipEncryptKey,
					sizeof(pEncKey->TkipEncryptKey));
	memcpy(Adapter->pwkkey.TkipTxMicKey, pEncKey->TkipTxMicKey,
						sizeof(pEncKey->TkipTxMicKey));
	memcpy(Adapter->pwkkey.TkipRxMicKey, pEncKey->TkipRxMicKey,
						sizeof(pEncKey->TkipRxMicKey));

	HEXDUMP("ADHOC KEY", (u8*)Adapter->pwkkey.TkipEncryptKey, 
			sizeof(pEncKey->TkipEncryptKey));

	LEAVE();
	return 0;
}
#endif /* end of ADHOCAES */

#ifdef WPA
static int wlan_ret_802_11_pwk_key(wlan_private * priv,
						HostCmd_DS_COMMAND * resp) 
{
	HostCmd_DS_802_11_PWK_KEY 	*pPwkKey = &resp->params.pwkkey;
	wlan_adapter			*Adapter = priv->adapter;
#ifdef	ADHOCAES	
	u16				Action = wlan_le16_to_cpu(pPwkKey->Action);
	int ret = 0;
#endif
	
	if (Adapter->InfrastructureMode == Wlan802_11Infrastructure) {
		memcpy(Adapter->pwkkey.TkipEncryptKey, pPwkKey->TkipEncryptKey,
				sizeof(pPwkKey->TkipEncryptKey));
		memcpy(Adapter->pwkkey.TkipTxMicKey, pPwkKey->TkipTxMicKey,
				sizeof(pPwkKey->TkipTxMicKey));
		memcpy(Adapter->pwkkey.TkipRxMicKey, pPwkKey->TkipRxMicKey,
				sizeof(pPwkKey->TkipRxMicKey));
	}

#ifdef	ADHOCAES	
	if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
		ret = 	HandleAdhocEncryptResponse(priv, pPwkKey);
					
		if ((Action == HostCmd_ACT_GET) ||
				(Action == HostCmd_ACT_GET + 2))
				wake_up_interruptible(&Adapter->cmd_EncKey);
	}

#endif
	LEAVE();
	return 0;
}
#endif

static int wlan_ret_802_11_mac_address(wlan_private * priv,
						HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_MAC_ADDRESS	*MacAdd = &resp->params.macadd;
	wlan_adapter			*Adapter = priv->adapter;
	
	ENTER();
	
	memcpy(Adapter->CurrentAddr, MacAdd->MacAdd, ETH_ALEN);
	
	LEAVE();
	return 0;
}

static int wlan_ret_802_11_rf_tx_power(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_RF_TX_POWER	*rtp = &resp->params.txp;
	wlan_adapter			*Adapter = priv->adapter;

	ENTER();

	Adapter->TxPowerLevel = wlan_le16_to_cpu(rtp->CurrentLevel); 
 
	PRINTK1("Current TxPower Level = %d\n", Adapter->TxPowerLevel);

	LEAVE();
	return 0;
}

static int wlan_ret_802_11_rf_antenna(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	PHostCmd_DS_802_11_RF_ANTENNA	pAntenna = &resp->params.rant;
	wlan_adapter			*Adapter = priv->adapter;
	u16				Action = wlan_le16_to_cpu(pAntenna->Action);

	if (Action == HostCmd_ACT_GET_RX)
		Adapter->RxAntennaMode = wlan_le16_to_cpu(pAntenna->AntennaMode);

	if (Action == HostCmd_ACT_GET_TX)
		Adapter->TxAntennaMode = wlan_le16_to_cpu(pAntenna->AntennaMode);

	PRINTK1("RET_802_11_RF_ANTENNA: Action = 0x%x, Mode = 0x%04x\n",
			Action, wlan_le16_to_cpu(pAntenna->AntennaMode));

	return 0;
}

static int wlan_ret_mac_multicast_adr(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	/*
	 * TODO: enable multicast handling 
	 */
	return 0;
}

static int wlan_ret_802_11_data_rate(wlan_private * priv, 
						HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_DATA_RATE	*pDataRate = &resp->params.drate;
	wlan_adapter			*Adapter = priv->adapter;
	u8				Dot11DataRate;

	ENTER();

	HEXDUMP("data_rate: ",
			(u8 *)pDataRate,sizeof(HostCmd_DS_802_11_DATA_RATE));

	Dot11DataRate = pDataRate->DataRate[0];
	if (pDataRate->Action == HostCmd_ACT_GET_TX_RATE) 
	{
		memcpy(Adapter->SupportedRates, pDataRate->DataRate,
				sizeof(Adapter->SupportedRates));
	}
	Adapter->DataRate = index_to_data_rate(Dot11DataRate);

	PRINTK("Data Rate = %d\n", Adapter->DataRate);
    
	LEAVE();
	return 0;
}

static int wlan_ret_802_11_rf_channel(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_RF_CHANNEL	*rfchannel = &resp->params.rfchannel;
	wlan_adapter			*Adapter = priv->adapter;
	u16				Action = wlan_le16_to_cpu(rfchannel->Action);

	ENTER();

	if (Action == RF_GET) {
		Adapter->Channel = wlan_le16_to_cpu(rfchannel->CurrentChannel);
	}

	LEAVE();
	return 0;
}

static int wlan_ret_802_11_rssi(wlan_private * priv, 
		HostCmd_DS_COMMAND * resp)
{
	HostCmd_DS_802_11_RSSI	*rssi = &resp->params.rssi;
	wlan_adapter		*Adapter = priv->adapter;

	/* store the non average value */
	Adapter->SNR[TYPE_BEACON][TYPE_NOAVG]	= wlan_le16_to_cpu(rssi->SNR);
	Adapter->NF[TYPE_BEACON][TYPE_NOAVG]	= wlan_le16_to_cpu(rssi->NoiseFloor);
	    
	Adapter->SNR[TYPE_BEACON][TYPE_AVG] = 
		CAL_AVG_SNR_NF(Adapter->SNR[TYPE_BEACON][TYPE_AVG], wlan_le16_to_cpu(rssi->SNR));
	Adapter->NF[TYPE_BEACON][TYPE_AVG] = 
		CAL_AVG_SNR_NF(Adapter->NF[TYPE_BEACON][TYPE_AVG], wlan_le16_to_cpu(rssi->NoiseFloor));

	/* store the beacon values in SNRNF variable, which 
	 * is present in adapter structure */

	Adapter->SNRNF[SNR_BEACON][TYPE_NOAVG]	= 
		Adapter->SNR[TYPE_BEACON][TYPE_NOAVG];
	Adapter->SNRNF[SNR_BEACON][TYPE_AVG]	=
		Adapter->SNR[TYPE_BEACON][TYPE_AVG] / AVG_SCALE;

	Adapter->SNRNF[NF_BEACON][TYPE_NOAVG]	= 
		Adapter->NF[TYPE_BEACON][TYPE_NOAVG];
	Adapter->SNRNF[NF_BEACON][TYPE_AVG]		=
		Adapter->NF[TYPE_BEACON][TYPE_AVG] / AVG_SCALE;
   
	// NOTE: Adapter->SNRNF[SNR_RXPD][X] - represents RXPD for SNR
	//       Adapter->SNRNF[NF_RXPD][X] - represents RXPD for NF
    
	Adapter->RSSI[TYPE_BEACON][TYPE_NOAVG] = 
		CAL_RSSI(Adapter->SNR[TYPE_BEACON][TYPE_NOAVG],
				Adapter->NF[TYPE_BEACON][TYPE_NOAVG]);

	Adapter->RSSI[TYPE_BEACON][TYPE_AVG] = 
		CAL_RSSI(Adapter->SNR[TYPE_BEACON][TYPE_AVG] / AVG_SCALE,
			Adapter->NF[TYPE_BEACON][TYPE_AVG] / AVG_SCALE);
    
	PRINTK1("Beacon RSSI value = 0x%x\n", 
		Adapter->RSSI[TYPE_BEACON][TYPE_AVG]);

	return 0;
}

#ifdef	MFG_CMD_SUPPORT
static int wlan_ret_mfg_cmd(wlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	wlan_adapter	*Adapter = priv->adapter;
	PRINTK("wlan_ret_mfg_cmd\n");
	ENTER();

	if (Adapter->mfg_cmd_flag == 1) {
		// enough buffer for the response
		if (priv->wlan_dev.upld_len <= Adapter->mfg_cmd_len) {
			memcpy(Adapter->mfg_cmd,
					Adapter->CurCmd->BufVirtualAddr,
					priv->wlan_dev.upld_len);
			Adapter->mfg_cmd_resp_len = priv->wlan_dev.upld_len;
		} else {
			memset(Adapter->mfg_cmd, 0, Adapter->mfg_cmd_len);
			Adapter->mfg_cmd_resp_len = -1;
		}
		wake_up_interruptible(&(Adapter->mfg_cmd_q));
		Adapter->mfg_cmd_flag = 0;
	}

	LEAVE();
	return 0;
}
#endif

#ifdef MULTI_BANDS
static int wlan_ret_802_11_band_config(wlan_private *priv,
			      HostCmd_DS_COMMAND *resp) 
{
	ENTER();
	
	HEXDUMP("802_11a CmdResp",(char *) resp, wlan_le16_to_cpu(resp->Size));

	LEAVE();
	return 0;
}
#endif

int wlan_ret_802_11_eeprom_access(wlan_private *priv,
	       			HostCmd_DS_COMMAND *resp)	
{
	wlan_adapter    *Adapter = priv->adapter;

//	resp -> params.rdeeprom.ByteCount = 5; /* For testing should remove */

	memcpy(Adapter -> pRdeeprom, (u8 *) &resp -> params.rdeeprom.Value, 
				wlan_le16_to_cpu(resp -> params.rdeeprom.ByteCount));
	HEXDUMP("Adapter", Adapter -> pRdeeprom, wlan_le16_to_cpu(resp -> params.rdeeprom.ByteCount));

	return 0;
}

#ifdef GSPI8385
static int wlan_ret_cmd_gspi_bus_config(wlan_private *priv,
			      HostCmd_DS_COMMAND *resp) 
{
	ENTER();
	
	HEXDUMP("802_11a CmdResp",(char *) resp, wlan_le16_to_cpu(resp->Size));

	LEAVE();
	return 0;
}
#endif /* GSPI8385 */

int wlan_ret_802_11_generate_atim(wlan_private *priv,
	       			HostCmd_DS_COMMAND *resp)	
{
	HostCmd_DS_802_11_GENERATE_ATIM *pAtim = &resp->params.genatim;
	wlan_adapter *Adapter = priv->adapter;

	ENTER();
	
	if (wlan_le16_to_cpu(pAtim->Enable))
		Adapter->ATIMEnabled = wlan_le16_to_cpu(pAtim->Enable);
	
	LEAVE();
	return 0;
}

/* Handle get_log response */
int HandleGetLogResponse(wlan_private *priv, HostCmd_DS_COMMAND *resp)
{
	PHostCmd_DS_802_11_GET_LOG  LogMessage = 
			(PHostCmd_DS_802_11_GET_LOG)&resp->params.glog;
	wlan_adapter 	*Adapter = priv->adapter;

	ENTER();

	/* TODO Convert it to Big Endian before copy */
	memcpy(&Adapter->LogMsg, LogMessage, 
					sizeof(HostCmd_DS_802_11_GET_LOG));
#ifdef DEBUG	
	HEXDUMP(" Get Log Response", (u8*)&Adapter->LogMsg, sizeof (Adapter->LogMsg));
#endif

	LEAVE();
	return 0;
}

#ifdef PASSTHROUGH_MODE
static int wlan_ret_passthrough(wlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	wlan_adapter	*Adapter = priv->adapter;

	ENTER();
	HEXDUMP("Passthrough Response", (u8 *) resp, resp->Size);

	LEAVE();
	return 0;
}

#endif	/* PASSTHROUGH_MODE */

/*
 * This function is called whenever a command response is received.
 */
int wlan_process_rx_command(wlan_private * priv)
{
	u16			RespCmd;
	HostCmd_DS_COMMAND	*resp;
	wlan_adapter		*Adapter = priv->adapter;
	int			ret = 0;
	u32			flags;
	u16			Result;

	ENTER();

	PRINTK1("Cmd Resp @ %lu\n", os_time_get());

	// Now we got response from FW, cancel the command timer
	if (Adapter->CommandTimerIsSet) {
		CancelTimer(&Adapter->MrvDrvCommandTimer);
		Adapter->CommandTimerIsSet = FALSE;
		Adapter->isCommandTimerExpired = FALSE;
	}

	if (!Adapter->CurCmd) {
		PRINTK1("Wrong Cmd response since Adapter->CurCmd is NULL\n");
		return -1;
	}

	memcpy(Adapter->CurCmd->BufVirtualAddr, priv->wlan_dev.upld_buf,
			priv->wlan_dev.upld_len);

	resp = (HostCmd_DS_COMMAND *) (Adapter->CurCmd->BufVirtualAddr);

	HEXDUMP("Cmd Rsp", Adapter->CurCmd->BufVirtualAddr,
			priv->wlan_dev.upld_len);

	RespCmd = wlan_le16_to_cpu(resp->Command);

	Result = wlan_le16_to_cpu(resp->Result);

	PRINTK("Received Command: %x Result: %d Length: %d\n", RespCmd,
			Result, priv->wlan_dev.upld_len);
	
	PRINTK1("Jiffies = %lu\n", os_time_get());

	if (!(RespCmd & 0x8000)) {
		PRINTK("Invalid response to command!");
		Adapter->CurCmd->retcode = WLAN_STATUS_FAILURE;
		CleanupAndInsertCmd(priv, Adapter->CurCmd);
		spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
		Adapter->CurCmd = NULL;
		spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);
		return -1;
	}

	/* Store the response code in the CmdNode. 
	 * Note: CmdNode->retcode should not be reset in CleanUpCmdCtrlNode()*/
	Adapter->CurCmd->retcode = wlan_le16_to_cpu(resp->Result);

#ifdef  PS_REQUIRED
	if (RespCmd == HostCmd_RET_802_11_PS_MODE) {
	HostCmd_DS_802_11_PS_MODE *psmode;

	psmode = &resp->params.psmode;
	PRINTK1("hwacproc: PS_MODE command reply result=%#x action=0x%X\n",
						resp->Result, psmode->Action);

	if (Result) {
		PRINTK1("wlan_process_rx_command() PS command failed: %#x \n", 
								resp->Result);
		if (Adapter->InfrastructureMode == Wlan802_11IBSS) {
			// We should not re-try enter-power-save command in 
			// ad-hoc mode.
			// It takes place in ExecuteNextCommand().
			if (psmode->Action == HostCmd_SubCmd_Enter_PS)
				Adapter->PSMode = Wlan802_11PowerModeCAM;
		}
	} else if (psmode->Action == HostCmd_SubCmd_Enter_PS) {
		Adapter->NeedToWakeup = FALSE;
		Adapter->PSState = PS_STATE_AWAKE;
		PRINTK1("Enter_PS command response\n");
	} else if (psmode->Action == HostCmd_SubCmd_Exit_PS) {
		Adapter->NeedToWakeup = FALSE;
		Adapter->PSState = PS_STATE_FULL_POWER;
		PRINTK1("Exit_PS command response\n");
	} else if (psmode->Action == HostCmd_SubCmd_Sleep_Confirmed) {
		//you should never get the Sleep_Confirmed_PS command 
		//response 
		PRINTK1("Sleep_Confirmed_PS command response\n");
	} else {
		PRINTK1("PS: Action=0x%X\n",psmode->Action);
	}

	CleanupAndInsertCmd(priv, Adapter->CurCmd);
	spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
	Adapter->CurCmd = NULL;
	spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

	return 0;
	}
#endif

#ifdef STDCMD
	if (Adapter->CurCmd->CmdFlags & CMD_F_STDCMD) {
		/* Copy the response back to response buffer */
		memcpy(Adapter->CurCmd->InformationBuffer,
				resp, resp->Size);

		Adapter->CurCmd->CmdFlags &= ~CMD_F_STDCMD;
		
		wake_up_interruptible(&Adapter->std_cmd_q);
	}
#endif
    
	Adapter->CurCmd->CmdWaitQWoken = TRUE;
	wake_up_interruptible(&Adapter->CurCmd->cmdwait_q);

	// If the command is not successful, return failure and clean-up
	// the command and exit
	//
	/* The Last condition must be removed. For test only */
	if ((Result != HostCmd_RESULT_OK ||
				!(RespCmd & 0x8000))) 
	//			&& (resp->Command != 0x8059)) /* This
	//			 condition must be removed. For test only */ 
	{
		printk("rx_command: command reply %#x result=%#x\n",
				resp->Command, resp->Result);
		// Handling errors here
		// In some cases we need to report hardware status
		switch (RespCmd) {
			
#ifdef PASSTHROUGH_MODE
			case HostCmd_RET_802_11_PASSTHROUGH:
				Adapter->PassthroughCmdStatus = PT_MODE_FAILURE;
				break;
#endif
			case HostCmd_RET_MAC_REG_ACCESS:
			case HostCmd_RET_BBP_REG_ACCESS:
			case HostCmd_RET_RF_REG_ACCESS:
				break;
		
			case HostCmd_RET_HW_SPEC_INFO:
			case HostCmd_RET_802_11_RESET:
				PRINTK1("HWAC - Reset command Failed\n");
				Adapter->bIsPendingReset = FALSE;
				/*
				 * FALL THROUGH 
				 */
			case HostCmd_RET_802_11_STATUS_INFO:
				Adapter->HardwareStatus = 
					WlanHardwareStatusNotReady;
				break;
		
			case HostCmd_RET_802_11_SCAN:
			case HostCmd_RET_802_11_ASSOCIATE:
				if (RespCmd == HostCmd_RET_802_11_SCAN) {
					Adapter->bIsScanInProgress = FALSE;
					wake_up_interruptible(&Adapter->scan_q);
#ifdef PROGRESSIVE_SCAN
					Adapter->ScanChannelsLeft = 0;
#endif
				}
				else if (RespCmd == 
						HostCmd_RET_802_11_ASSOCIATE)
					wake_up_interruptible(&Adapter->
							association_q);
				/* FALL THROUGH */
			case HostCmd_RET_802_11_REASSOCIATE:
			case HostCmd_RET_802_11_AD_HOC_JOIN:
			case HostCmd_RET_802_11_AD_HOC_START:
				Adapter->bIsAssociationInProgress = FALSE;
				break;
#ifdef CAL_DATA
			case HostCmd_RET_802_11_CAL_DATA:
				wake_up_interruptible(&Adapter->
						wait_cal_dataResponse);
				break;
#endif
#ifdef BCA
			case HostCmd_RET_802_11_BCA_CONFIG_TIMESHARE:
				break;
#endif

#ifdef V4_CMDS			
			case HostCmd_RET_802_11_INACTIVITY_TIMEOUT:
				break;
#endif
			case HostCmd_RET_802_11_SLEEP_PERIOD:
				{ 
				HostCmd_DS_802_11_SLEEP_PERIOD ps_sleeppd;
					ps_sleeppd.Period = 5;
					ps_sleeppd.Direction = 2;
					
				memcpy(Adapter->CurCmd->InformationBuffer,
					&ps_sleeppd,
					sizeof(HostCmd_DS_802_11_SLEEP_PERIOD));
				}
				break;
		}
                CleanupAndInsertCmd(priv, Adapter->CurCmd);
                spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
		Adapter->CurCmd = NULL;
		spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);

		return WLAN_STATUS_FAILURE;
	}

	/*
	 * Process the newly received response 
	 */

	switch (RespCmd) {
#ifdef PASSTHROUGH_MODE
		case HostCmd_RET_802_11_PASSTHROUGH:
			ret = wlan_ret_passthrough(priv, resp);
			break;
#endif
		case HostCmd_RET_MAC_REG_ACCESS:
		case HostCmd_RET_BBP_REG_ACCESS:
		case HostCmd_RET_RF_REG_ACCESS:
			ret = wlan_ret_reg_access(priv, RespCmd, resp);
			break;
	    
		case HostCmd_RET_HW_SPEC_INFO:
			ret = wlan_ret_get_hw_spec(priv, resp);
			break;
	    
#ifdef PS_REQUIRED
		case HostCmd_RET_802_11_PS_MODE:
			ret = wlan_ret_802_11_ps_mode(priv, resp);
			break;
#endif

#ifdef BG_SCAN
		case HostCmd_RET_802_11_BG_SCAN_QUERY:
#endif /* BG_SCAN */
		case HostCmd_RET_802_11_SCAN:
			ret = wlan_ret_802_11_scan(priv, resp);

#ifdef PROGRESSIVE_SCAN
			if (Adapter->ScanChannelsLeft > 0) {
				ret = PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_SCAN, 0,
						HostCmd_OPTION_USE_INT, 0,
						HostCmd_PENDING_ON_NONE,
						NULL);

				if (ret) {
					LEAVE();
					return ret;
				}
			}
			else
#endif
			{
				Adapter->bIsScanInProgress = FALSE;
#ifdef ENABLE_802_11D
				/* IEfound and Changed, update domainreg and set to FW*/
				if ( Adapter->State11D.CountryInfoIEFound == TRUE ) { 
					if ( Adapter->State11D.CountryInfoChanged == TRUE ) 
						wlan_802_11d_sm_transfer( priv, 
									CMD_SCAN_RESP_COMPLIANT_11D );
				}
				else {
					wlan_802_11d_sm_transfer( priv, 
								CMD_SCAN_RESP_NON_COMPLIANT_11D );
				}
#endif	
				wake_up_interruptible(&Adapter->scan_q); 
			}

			break;

		case HostCmd_RET_MAC_CONTROL:
			ret = wlan_ret_mac_control(priv, resp);
			break;
			
#ifdef	linux
		case HostCmd_RET_802_11_GET_LOG:
			ret = HandleGetLogResponse(priv, resp);		
			break;
#endif
		case HostCmd_RET_802_11_ASSOCIATE:
		case HostCmd_RET_802_11_REASSOCIATE:
			ret = wlan_ret_802_11_associate(priv, resp);
			
#ifdef PS_REQUIRED
			/* SD8381 was having problems 
			 * with PSSleep without the delay 
			 */ 
/*
			if ((Adapter->PSState == PS_STATE_FULL_POWER) && 
					ret == 0)
				wlan_sched_timeout(50);
*/
#endif
			break;
	    
		case HostCmd_RET_802_11_DISASSOCIATE:
		case HostCmd_RET_802_11_DEAUTHENTICATE:
			ret = wlan_ret_802_11_disassociate(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_SET_WEP:
			ret = wlan_ret_802_11_set_wep(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_AD_HOC_START:
		case HostCmd_RET_802_11_AD_HOC_JOIN:
			ret = wlan_ret_802_11_ad_hoc_start(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_RESET:
			ret = wlan_ret_802_11_reset(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_QUERY_TRAFFIC:
			ret = wlan_ret_802_11_query_traffic(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_STATUS_INFO:
			ret = wlan_ret_802_11_status_info(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_AUTHENTICATE:
			ret = wlan_ret_802_11_authenticate(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_STAT:
			ret = wlan_ret_802_11_stat(priv, resp);
			break;
	    
		case HostCmd_RET_802_3_STAT:
			ret = wlan_ret_802_3_stat(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_SNMP_MIB:
			ret = wlan_ret_802_11_snmp_mib(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_RF_TX_POWER:
			ret = wlan_ret_802_11_rf_tx_power(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_RADIO_CONTROL:
			ret = wlan_ret_802_11_radio_control(priv, resp);
			break;

#ifdef BCA
		case HostCmd_RET_BCA_CONFIG:
			ret = wlan_ret_bca_config(priv, resp);
			break;
#endif

#if defined(DEEP_SLEEP_CMD)
		case HostCmd_RET_802_11_DEEP_SLEEP:
			Adapter->IsDeepSleep = TRUE;
			break;
#endif // DEEP_SLEEP_CMD

#ifdef HOST_WAKEUP
		case HostCmd_RET_802_11_HOST_WAKE_UP_CFG:
			Adapter->bHostWakeupDevRequired = TRUE;	// valid in PS mode only
			printk("Host Wake Up Cfg cmd resp 0x%04X received\n",
								RespCmd);
			if (Adapter->PSMode != Wlan802_11PowerModeCAM) {
				printk("F/W OK to sleep: Sending Sleep Confirm\n");
#ifdef PS_REQUIRED
				if (SendConfirmSleep(priv, 
					(u8 *) Adapter->PSConfirmSleep,
					sizeof(PS_CMD_ConfirmSleep)) == 0) {
				} else {
					printk("Sending F/W OK to sleep: FAILED !!!\n");
				}
			}
#endif
			break;
		case HostCmd_RET_802_11_HOST_AWAKE_CONFIRM:
			printk("Host Awake Confirm cmd resp 0x%04X received\n",RespCmd);
			break;
#endif

#ifdef AUTO_FREQ_CTRL
		case HostCmd_RET_802_11_SET_AFC:
		case HostCmd_RET_802_11_GET_AFC:
			memcpy(Adapter->CurCmd->InformationBuffer, resp,
					(wlan_le16_to_cpu(resp->Size) + sizeof (HostCmd_DS_GEN));
		break;
#endif
		case HostCmd_RET_802_11_RF_ANTENNA:
			ret = wlan_ret_802_11_rf_antenna(priv, resp);
			break;
	    
		case HostCmd_RET_MAC_MULTICAST_ADR:
			ret = wlan_ret_mac_multicast_adr(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_DATA_RATE:
			ret = wlan_ret_802_11_data_rate(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_RF_CHANNEL:
			ret = wlan_ret_802_11_rf_channel(priv, resp);
			break;
	    		
		case HostCmd_RET_802_11_RSSI:
			ret = wlan_ret_802_11_rssi(priv, resp);
			break;

#ifdef PS_REQUIRED
		case HostCmd_RET_802_11_PRE_TBTT:
			ret = wlan_ret_set_pre_tbtt(priv, resp);
			break;
#endif
		
		case HostCmd_RET_802_11_MAC_ADDRESS:
			ret = wlan_ret_802_11_mac_address(priv, resp);
			break;

#ifdef MFG_CMD_SUPPORT
		case HostCmd_RET_MFG_COMMAND:
			ret = wlan_ret_mfg_cmd(priv, resp);
			break;
#endif
		case HostCmd_RET_802_11_AD_HOC_STOP:
			ret = wlan_ret_802_11_ad_hoc_stop(priv, resp);
			break;
	    
		case HostCmd_RET_802_11_BEACON_STOP:
			break;
			
#ifdef WPA
		case HostCmd_RET_802_11_ENABLE_RSN:
			break;
#ifndef WPA2
		case HostCmd_RET_802_11_RSN_AUTH_SUITES:
			break;
#endif //WPA2
		case HostCmd_RET_802_11_PWK_KEY:
			ret = wlan_ret_802_11_pwk_key(priv, resp); 
			break;
#ifdef WPA2
		case HostCmd_RET_802_11_KEY_MATERIAL:
			PRINTK("KEY_MATERIAL command response\n"); 
			break;
#endif //WPA2
		case HostCmd_RET_802_11_QUERY_RSN_OPTION:
			break;
#ifndef WPA2
		case HostCmd_RET_802_11_GRP_KEY:
			PRINTK("WPA: GTK set is finished\n");	
			Adapter->IsGTK_SET = TRUE;
			break;
#endif //WPA2
#endif //WPA
		case HostCmd_RET_802_11_TX_MODE:
		case HostCmd_RET_802_11_RX_MODE:
			break;
#ifdef CAL_DATA
		case HostCmd_RET_802_11_CAL_DATA:
			/* TODO Handle the Big Endian conversion */
			memcpy(&priv->adapter->caldata, resp, 
					sizeof(HostCmd_DS_802_11_CAL_DATA) + 
					sizeof(HostCmd_DS_GEN));
			wake_up_interruptible(&Adapter->
					wait_cal_dataResponse);
			break;
#endif
			
#ifdef MULTI_BANDS
		case HostCmd_RET_802_11_BAND_CONFIG:
			ret = wlan_ret_802_11_band_config(priv, resp); 
			break;
#endif
		case HostCmd_RET_802_11_EEPROM_ACCESS:
			ret = wlan_ret_802_11_eeprom_access(priv, resp);
			break;

#ifdef GSPI8385
		case HostCmd_RET_CMD_GSPI_BUS_CONFIG:
			ret = wlan_ret_cmd_gspi_bus_config(priv, resp);
			break;
#endif /* GSPI8385 */
				
		case HostCmd_RET_802_11_GENERATE_ATIM:
			ret = wlan_ret_802_11_generate_atim(priv, resp);
			break;
#ifdef ENABLE_802_11D
		case HostCmd_RET_802_11D_DOMAIN_INFO:
			ret = wlan_ret_802_11d_domain_info(priv, resp);
			break;
#endif		

#ifdef ENABLE_802_11H_TPC
		case HostCmd_RET_802_11H_TPC_REQUEST:
			ret = wlan_ret_802_11h_tpc_request(priv, resp);
			break;

		case HostCmd_RET_802_11H_TPC_INFO:
			ret = wlan_ret_802_11h_tpc_info(priv, resp);
			PRINTK2("11H TPC_INFO return\n");
			break;
#endif		

		case HostCmd_RET_802_11_SLEEP_PARAMS: /* 0x8066 */		
			ret = wlan_ret_802_11_sleep_params(priv, resp);
			break;
#ifdef BCA
		case HostCmd_RET_802_11_BCA_CONFIG_TIMESHARE:
			/* TODO Check Big Endian Conversion */
			memcpy(Adapter->CurCmd->InformationBuffer,
				&resp->params.bca_timeshare,
				sizeof(HostCmd_DS_802_11_BCA_TIMESHARE));
			break;
#endif
#ifdef V4_CMDS
		case HostCmd_RET_802_11_INACTIVITY_TIMEOUT:
			*((u16 *)Adapter->CurCmd->InformationBuffer) =
					wlan_le16_to_cpu(resp->params.inactivity_timeout.Timeout);
			break;
#endif
#ifdef BG_SCAN
		case HostCmd_RET_802_11_BG_SCAN_CONFIG:
			printk("BG_CONFIG CMD response\n");
			break;
#endif /* BG_SCAN */
		case HostCmd_RET_802_11_SLEEP_PERIOD:
			/* TODO Check Big Endian Conversion */
			memcpy(Adapter->CurCmd->InformationBuffer,
				&resp->params.ps_sleeppd,
				sizeof(HostCmd_DS_802_11_SLEEP_PERIOD));
			break;

		default:
			PRINTK("Unknown command response %#x\n",resp->Command);
			break;
	}

        if (Adapter->CurCmd) {
		// Clean up and Put current command back to CmdFreeQ
		CleanupAndInsertCmd(priv, Adapter->CurCmd);
		spin_lock_irqsave(&Adapter->QueueSpinLock, flags);
		Adapter->CurCmd = NULL;
		spin_unlock_irqrestore(&Adapter->QueueSpinLock, flags);
	}

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}
