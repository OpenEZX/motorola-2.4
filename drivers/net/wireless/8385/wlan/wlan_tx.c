/*
 * File : wlan_tx.c 
 */

#include	"include.h"

/*
 * Without threads 
 */
void wlan_process_tx(wlan_private * priv)
{
	wlan_adapter   *Adapter = priv->adapter;
	u32		flags;

	OS_INTERRUPT_SAVE_AREA;	/* Needed for Threadx; Dummy for Linux */

	ENTER();
    
	spin_lock_irqsave(&Adapter->CurrentTxLock, flags);

	if (!Adapter->CurrentTxSkb) {
		spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);
		LEAVE();
		return;
	}

	spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);

#ifdef PS_REQUIRED
	if ((Adapter->PSState == PS_STATE_SLEEP)
#ifdef PS_PRESLEEP
		|| (Adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
	) {
		PRINTK1("In PS State %d"
			" - Not sending the packet\n", Adapter->PSState);
		goto done;
	}
#endif

	if(priv->wlan_dev.dnld_sent) {
		PRINTK("wlan_process_tx(): dnld_sent = %d, not sending\n",
						priv->wlan_dev.dnld_sent);
		return;
	}

	if (MrvDrvSend(priv, Adapter->CurrentTxSkb)) {
		if (Adapter->CurrentTxSkb) {
			kfree_skb(Adapter->CurrentTxSkb);
			Adapter->CurrentTxSkb =  NULL;
		}
		priv->stats.tx_dropped++;
		priv->stats.tx_errors++;
	}
	
	OS_INT_DISABLE;
	priv->adapter->HisRegCpy &= ~HIS_TxDnLdRdy;
	OS_INT_RESTORE;
	
#ifdef PS_REQUIRED
done:
#endif
	LEAVE();
}

int wlan_tx_packet(wlan_private * priv, struct sk_buff *skb)
{
	u32		flags;
	wlan_adapter 	*Adapter = priv->adapter;
	int		ret = WLAN_STATUS_SUCCESS;

	ENTER();

	spin_lock_irqsave(&Adapter->CurrentTxLock, flags);

#ifdef WMM
	if(Adapter->wmm.enabled) {
		wmm_map_and_add_skb(priv, skb);
#ifdef WPRM_DRV
                WPRM_DRV_TRACING_PRINT();
                wprm_tx_packet_cnt ++;
                wprm_traffic_measurement(priv, Adapter, TRUE);
#endif
		wake_up_interruptible(&priv->MainThread.waitQ);
	} else
#endif /* WMM */
	if (!Adapter->CurrentTxSkb) {
		Adapter->CurrentTxSkb = skb;
#ifdef WPRM_DRV
                WPRM_DRV_TRACING_PRINT();
                wprm_tx_packet_cnt ++;
                wprm_traffic_measurement(priv, Adapter, TRUE);
#endif
		wake_up_interruptible(&priv->MainThread.waitQ);
	} else {
		ret = WLAN_STATUS_FAILURE;
	}
	
	spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);

	LEAVE();

	return ret;
}

int wlan_tx_end_packet(wlan_private * priv)
{
	int             rval = WLAN_STATUS_FAILURE;
	PWLAN_OBJECT    iface = &priv->iface;

	ENTER();

	if (!iface->tx_started) {
		PRINTK1("tx not started!\n");
		goto done;
	}

	rval = sbi_host_to_card(priv, MVMS_DAT, iface->tx_buf, 
							iface->tx_pktlen);
	if (!rval) {
		PRINTK1("Data transfer to device ok!\n");
	} else {
		PRINTK("Data transfer to device failed!\n");
	}

done:	
	iface->tx_started = FALSE;
	iface->tx_pktlen = 0;
	iface->tx_pktcnt = 0;

	LEAVE();
	return rval;
}

/*
 * Is called by MrvDrvSend - who can be called either by a thread or
 * directly after a packet is received from the network layer 
 */
int SendSinglePacket(wlan_private * priv, struct sk_buff *skb)
{
	wlan_adapter   *Adapter = priv->adapter;
	int             ixStatus = WLAN_STATUS_SUCCESS;
	u32             TotalBufferLength = 0;
	TxPD		*pLocalTxPD = &Adapter->LocalTxPD;
	u8		*ptr = priv->iface.tx_buf;
	unsigned short 	prot_sn,ping_sn;

	ENTER();

	TXRX_DEBUG_GET_TIME(1);

	if (!skb->len) {
		PRINTK("TX - WLAN buffer is not valid, return FAILURE \n");
		return -1;
	}

	/* need to compare to entire buffer size */
	if (skb->len > MRVDRV_ETH_TX_PACKET_BUFFER_SIZE) {
		/* TODO: Handle this condition gracefully. */
		PRINTK("Bad skb length %d > %d\n", skb->len, 
					MRVDRV_ETH_TX_PACKET_BUFFER_SIZE);
		kfree_skb((struct sk_buff *) skb);
		Adapter->CurrentTxSkb =  NULL;
		priv->stats.tx_dropped++;
		return -1;
	}

	memset(pLocalTxPD, 0, sizeof(TxPD));

	pLocalTxPD->TxStatus = MRVDRV_TxPD_STATUS_USED;

	pLocalTxPD->TxPacketLength = skb->len;

#ifdef WMM
	if(Adapter->wmm.enabled) {
		/* 
		 * original skb->priority has been overwritten 
		 * by wmm_map_and_add_skb()
		 */
		pLocalTxPD->Priority = (u8)skb->priority;
#ifdef WMM_UAPSD
    if (Adapter->PSState != PS_STATE_FULL_POWER)
    {
			if ((Adapter->CurBssParams.wmm_uapsd_enabled == TRUE) && (Adapter->wmm.qosinfo != 0))
      {
        if (wmm_lists_empty(priv) && (Adapter->sleep_period.period != 0))
        {
          Adapter->wmm.no_more_packet = 1;
          pLocalTxPD->PowerMgmt = MRVDRV_TxPD_POWER_MGMT_LAST_PACKET;
        } 
      }
    }
#endif
	}
#endif /* WMM */

	// Size of TxPd structure;
	pLocalTxPD->TxPacketLocation = sizeof(TxPD);

	// Other TxPD member can be filled in here
	// Auto Mode , 3 retries 
	pLocalTxPD->TxControl = ((Adapter->TxRetryCount & 0x0f) << 12) | 
					data_rate_to_index(Adapter->DataRate);
	// retry_and_rate is no longer used, overrid with TxControl ioctl value
	pLocalTxPD->TxControl = Adapter->PktTxCtrl;

#ifdef	BIG_ENDIAN 
	endian_convert_pLocalTxPD(pLocalTxPD);
#endif	

	memcpy(pLocalTxPD->TxDestAddrHigh, skb->data, MRVDRV_ETH_ADDR_LEN);

	// Start the packet.
	TotalBufferLength = skb->len + sizeof(TxPD);

	ixStatus = wlan_Start_Tx_Packet(priv, (u16) TotalBufferLength);
	TXRX_DEBUG_GET_TIME(2);
	PRINTK1("TX %d bytes: @ %lu\n", TotalBufferLength, 
			os_time_get());
	if (!(ixStatus)) {
		PRINTK("Frame not accepted: interface still "
				"busy after waiting\n");
		return WLAN_STATUS_FAILURE;
	}
	TXRX_DEBUG_GET_TIME(3);
	
	HEXDUMP("TxPD", (u8 *) pLocalTxPD, sizeof(TxPD));

	memcpy(ptr, pLocalTxPD, sizeof(TxPD));
	
	ptr += sizeof(TxPD);
		
	HEXDUMP("Tx Data", (u8 *) skb->data, skb->len);
	memcpy(ptr, skb->data, skb->len);

	TXRX_DEBUG_GET_TIME(4);
	prot_sn = ((unsigned short *)(skb->data))[17];
	ping_sn = ((unsigned short *)(skb->data))[20];
	
	ixStatus = wlan_tx_end_packet(priv);
	
	if (ixStatus) {
		PRINTK("tx_end_packet failed: 0x%X\n",ixStatus);
		 return ixStatus;
	}

	TXRX_DEBUG_GET_TIME(5);

	os_free_tx_packet(priv);
	
	PRINTK1("DONE TX PACKET:\n");

	Adapter->SentFlags = 0;
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += TotalBufferLength;

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

#ifdef WMM_UAPSD
int SendNullPacket(wlan_private * priv, u8 pwr_mgmt)
{
  TxPD txpd;
  u32 retry_and_rate;
  int r;

  ENTER();

	if (priv->adapter->SurpriseRemoved == TRUE)
    return -1;

	if (priv->adapter->bIsPendingReset == TRUE)
    return -1;

	if (priv->adapter->MediaConnectStatus == WlanMediaStateDisconnected)
    return -1;

  memset(&txpd, 0, sizeof(TxPD));

  retry_and_rate  = (priv->adapter->TxRetryCount & 0x0f) << 12;
  retry_and_rate |= data_rate_to_index(priv->adapter->DataRate);

  txpd.TxControl   = retry_and_rate;
  txpd.PowerMgmt   = pwr_mgmt;
  txpd.TxStatus    = MRVDRV_TxPD_STATUS_USED;
  txpd.TxPacketLocation = sizeof(TxPD);

#ifdef BIG_ENDIAN 
  endian_convert_pLocalTxPD(&txpd);
#endif  

  r = wlan_Start_Tx_Packet(priv, sizeof(TxPD));

  if (r == 0) 
  {
    PRINTK("SendNullPacket failed!\n");
    return WLAN_STATUS_FAILURE;
  }

  memcpy(priv->iface.tx_buf, &txpd, sizeof(TxPD));

  r = wlan_tx_end_packet(priv);
  
  if (r != 0)
  {
    PRINTK("SendNullPacket failed!\n");
    return r;
  }

  LEAVE();
  return WLAN_STATUS_SUCCESS;
}
#endif

int wlan_Start_Tx_Packet(wlan_private * priv, int pktlen)
{
	int             ret;
	PWLAN_OBJECT    iface = &priv->iface;	// TODO Get rid of hwiface

	ENTER();

	PRINTK1("Packet Size=%d\n", pktlen);

	if (pktlen > MAX_WLAN_TX_SIZE) {
		PRINTK1("%d  MAX_WLAN_TX_SIZE\n", pktlen);
		priv->stats.tx_dropped++;
		ret = -1;
		goto done;
	}
#ifndef DUMMY_PACKET
/*
	if (sbi_is_tx_download_ready(priv)) {
		PRINTK("DnLd Not Ready - tx_start_packet\n");
		ret = -1;
		goto done;
	}
*/
#endif
	if (iface->tx_started) {
		PRINTK1("Previous packet under process yet!\n");
		ret = -1;
		goto done;
	}
	
	iface->tx_started = TRUE;
	iface->tx_pktlen = pktlen;
	iface->tx_pktcnt = 0;
	ret = 1;

done:
 	LEAVE();
 	return ret;
}

/*
 * To prepare the packet and send can be called (mostly always) * called
 * from the tx_event_thread 
 */
int MrvDrvSend(wlan_private * priv, struct sk_buff *skb)
{
	u32             ret;
	wlan_adapter   *Adapter = priv->adapter;

	ENTER();

	// Check device removal status
	if (Adapter->SurpriseRemoved == TRUE) {
		PRINTK("ERROR: Device Removed!!!\n");
		ret = WLAN_STATUS_FAILURE;
		goto done;
	}

	if (Adapter->bIsPendingReset == TRUE) {
		PRINTK("ERROR: Reset is Pending!!!\n");
		ret = WLAN_STATUS_FAILURE;
		goto done;
	}

	if (Adapter->MediaConnectStatus == WlanMediaStateDisconnected) {
		PRINTK("ERROR: WlanMediaStateDisconnected!!!\n");
		ret = WLAN_STATUS_FAILURE;
		goto done;
	}

	ret = SendSinglePacket(priv, skb);
	
done:

	LEAVE();
	return ret;
}
