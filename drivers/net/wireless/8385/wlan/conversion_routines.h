
#ifndef _CONVERSION_ROUTINES_H
#define _CONVERSION_ROUTINES_H

/* here add required header files for structures */
/*following 2 funcs for cmdresp.c file */
/*
void endian_convert_pBSSIDList(PWLAN_802_11_BSSID pBSSIDList) {

	pBSSIDList->Length = le32_to_cpu(pBSSIDList->Length);
    pBSSIDList->Ssid.SsidLength = le32_to_cpu(pBSSIDList->Ssid.SsidLength);
	pBSSIDList->Privacy = le32_to_cpu(pBSSIDList->Privacy);
	pBSSIDList->Rssi = (WLAN_802_11_RSSI)le32_to_cpu(pBSSIDList->Rssi);
	pBSSIDList->Channel = le32_to_cpu(pBSSIDList->Channel);
	pBSSIDList->NetworkTypeInUse = le32_to_cpu(pBSSIDList->NetworkTypeInUse);
	pBSSIDList->Configuration.Length = le32_to_cpu(pBSSIDList->Configuration.Length);
	pBSSIDList->Configuration.BeaconPeriod = le32_to_cpu(pBSSIDList->Configuration.BeaconPeriod);
	pBSSIDList->Configuration.ATIMWindow = le32_to_cpu(pBSSIDList->Configuration.ATIMWindow);
	pBSSIDList->Configuration.DSConfig = le32_to_cpu(pBSSIDList->Configuration.DSConfig);
	pBSSIDList->Configuration.FHConfig.Length = le32_to_cpu(pBSSIDList->Configuration.FHConfig.Length);
	pBSSIDList->Configuration.FHConfig.HopPattern = le32_to_cpu(pBSSIDList->Configuration.FHConfig.HopPattern);
	pBSSIDList->Configuration.FHConfig.HopSet = le32_to_cpu(pBSSIDList->Configuration.FHConfig.HopSet);
	pBSSIDList->Configuration.FHConfig.DwellTime = le32_to_cpu(pBSSIDList->Configuration.FHConfig.DwellTime);
	pBSSIDList->InfrastructureMode = (WLAN_802_11_NETWORK_INFRASTRUCTURE)le32_to_cpu(pBSSIDList->InfrastructureMode);

#ifdef FWVERSION3   
	pBSSIDList->IELength = le32_to_cpu(pBSSIDList->IELength);
#endif
}


void endian_convert_pFixedIE(WLAN_802_11_FIXED_IEs *pFixedIE) {
	
	pFixedIE->BeaconInterval = le16_to_cpu(pFixedIE->BeaconInterval);
	pFixedIE->Capabilities = le16_to_cpu(pFixedIE->Capabilities);
}
*/
/*following func is for wlan_tx.c file */
/*void endian_convert_pTxNode(PTxCtrlNode  pTxNode) {

	pTxNode->LocalWCB->TxStatus = le32_to_cpu(MRVDRV_WCB_STATUS_USED);
	pTxNode->LocalWCB->TxPacketLength = le16_to_cpu((u16)skb->len);
	pTxNode->LocalWCB->TxPacketLocation = le32_to_cpu(sizeof(WCB));
	pTxNode->LocalWCB->TxControl = le32_to_cpu(pTxNode->LocalWCB->TxControl);
}*/
#define endian_convert_pBSSIDList(x);	{	\
	pBSSIDList->Length = le32_to_cpu(pBSSIDList->Length);	\
	pBSSIDList->Ssid.SsidLength = le32_to_cpu(pBSSIDList->Ssid.SsidLength);	\
	pBSSIDList->Privacy = le32_to_cpu(pBSSIDList->Privacy);		\
	pBSSIDList->Rssi = (WLAN_802_11_RSSI)le32_to_cpu(pBSSIDList->Rssi);	\
	pBSSIDList->Channel = le32_to_cpu(pBSSIDList->Channel);		\
	pBSSIDList->NetworkTypeInUse = le32_to_cpu(pBSSIDList->NetworkTypeInUse);	\
	pBSSIDList->Configuration.Length = le32_to_cpu(pBSSIDList->Configuration.Length);	\
	pBSSIDList->Configuration.BeaconPeriod = le32_to_cpu(pBSSIDList->Configuration.BeaconPeriod);	\
	pBSSIDList->Configuration.ATIMWindow = le32_to_cpu(pBSSIDList->Configuration.ATIMWindow);	\
	pBSSIDList->Configuration.DSConfig = le32_to_cpu(pBSSIDList->Configuration.DSConfig);	\
	pBSSIDList->Configuration.FHConfig.Length = le32_to_cpu(pBSSIDList->Configuration.FHConfig.Length);	\
	pBSSIDList->Configuration.FHConfig.HopPattern = le32_to_cpu(pBSSIDList->Configuration.FHConfig.HopPattern);	\
	pBSSIDList->Configuration.FHConfig.HopSet = le32_to_cpu(pBSSIDList->Configuration.FHConfig.HopSet);	\
	pBSSIDList->Configuration.FHConfig.DwellTime = le32_to_cpu(pBSSIDList->Configuration.FHConfig.DwellTime);	\
	pBSSIDList->InfrastructureMode = (WLAN_802_11_NETWORK_INFRASTRUCTURE)le32_to_cpu(pBSSIDList->InfrastructureMode);\
	}
	
/*
#ifdef FWVERSION3	\
	pBSSIDList->IELength = le32_to_cpu(pBSSIDList->IELength);	\
#endif	\
	}
*/

#define endian_convert_pFixedIE(x);	{	\
		pFixedIE->BeaconInterval = le16_to_cpu(pFixedIE->BeaconInterval);	\
		pFixedIE->Capabilities = le16_to_cpu(pFixedIE->Capabilities);		\
	}


#define endian_convert_pTxNode(x);	{	\
			pTxNode->LocalWCB->TxStatus = le32_to_cpu(MRVDRV_WCB_STATUS_USED);	\
			pTxNode->LocalWCB->TxPacketLength = le16_to_cpu((u16)skb->len);	\
			pTxNode->LocalWCB->TxPacketLocation = le32_to_cpu(sizeof(WCB));	\
			pTxNode->LocalWCB->TxControl = le32_to_cpu(pTxNode->LocalWCB->TxControl);	\
			}

#endif	// _CONVERSION_ROUTINES_H
