/*
 * File : wlan_tx.c 
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
	if (Adapter->PSState == PS_STATE_SLEEP) {
		PRINTK1("In Power Mode Sleep"
				" - Not Sending the Packet\n");
		goto done;
	}
#endif

	if(priv->wlan_dev.dnld_sent) {
		PRINTK("wlan_process_tx(): dnld_sent = %d, not sending\n",
						priv->wlan_dev.dnld_sent);
		return;
	}

	if (MrvDrvSend(priv, Adapter->CurrentTxSkb)) {
		if(Adapter->CurrentTxSkb) {
			kfree_skb(Adapter->CurrentTxSkb);
			Adapter->SentPacket =  NULL;
			Adapter->CurrentTxSkb =  NULL;
		}
		priv->stats.tx_dropped++;
		priv->stats.tx_errors++;
	}
	
	cli();
	priv->adapter->HisRegCpy &= ~HIS_TxDnLdRdy;
	sti();
	
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

	if (!Adapter->CurrentTxSkb) {
		Adapter->CurrentTxSkb = skb;
		wake_up_interruptible(&priv->intThread.waitQ);
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
		//dev->trans_start = jiffies;
		PRINTK("Data transfer to device failed!\n");
	}

done:	
	iface->tx_started = FALSE;
	iface->tx_pktlen = 0;
	iface->tx_pktcnt = 0;

	LEAVE();
	return rval;
}

void wlan_Tx_Block(wlan_private * priv, u8 * pkt, u16 pktlen)
{
	// TODO hwiface can be a part of priv
	PWLAN_OBJECT    iface = &priv->iface;

	ENTER();

	if (!iface->tx_started) {
		PRINTK1("Tx not started - wlan_tx_block!\n");
		goto done;
	}

	if (iface->tx_pktcnt + pktlen > MAX_WLAN_TX_SIZE) {
		PRINTK1("Tx buffer would overrun!\n");
		goto done;
	}

	memcpy((void *) &iface->tx_buf[iface->tx_pktcnt],(void *) pkt, 
			(size_t) pktlen);
	iface->tx_pktcnt += pktlen;

done:
	LEAVE();
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
	PTxCtrlNode     pTxNode = &Adapter->TxNode;
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
		Adapter->SentPacket =  NULL;
		Adapter->CurrentTxSkb =  NULL;
		priv->stats.tx_dropped++;
		return -1;
	}

	pTxNode->NPTxPacket = skb;	// TODO: required?

	memset(pTxNode->LocalTxPD, 0, sizeof(TxPD));

	pTxNode->LocalTxPD->TxStatus = MRVDRV_TxPD_STATUS_USED;

	pTxNode->LocalTxPD->TxPacketLength = skb->len;

	// Size of TxPd structure;
	pTxNode->LocalTxPD->TxPacketLocation = sizeof(TxPD);

	// Other TxPD member can be filled in here
	// Auto Mode , 3 retries 
	pTxNode->LocalTxPD->TxControl =
		((Adapter->TxRetryCount & 0x0f) << 12)
		| data_rate_to_index(Adapter->DataRate);
#ifdef	BIG_ENDIAN 
	endian_convert_pTxNode(pTxNode);
#endif	

	memcpy((PVOID) pTxNode->LocalTxPD->TxDestAddrHigh,
			skb->data, MRVDRV_ETH_ADDR_LEN);

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
	
	HEXDUMP("TxPD", (u8 *) pTxNode->LocalTxPD, sizeof(TxPD));

	memcpy(ptr, pTxNode->LocalTxPD, sizeof(TxPD));
	
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

	wlan_free_tx_packet(priv);
	
	PRINTK1("DONE TX PACKET:\n");

	Adapter->SentFlags = 0;
	Adapter->wlan802_3Stat.XmitOK++;
	priv->stats.tx_packets++;
	Adapter->TxBytes += TotalBufferLength;
	priv->stats.tx_bytes += TotalBufferLength;

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

/*
 * Allocate the Single Tx : Used in the SendSinglePacket 
 */

int AllocateSingleTx(wlan_private * priv)
{

	wlan_adapter   *Adapter = priv->adapter;
	ENTER();

	Adapter->TxNode.BufVirtualAddr = NULL;
	Adapter->TxNode.BufPhyAddr = 0xfffffff;
	Adapter->TxNode.LocalTxPD = &Adapter->Wcb;
	Adapter->TxNode.Status = MRVDRV_TX_CTRL_NODE_STATUS_IDLE;

	return WLAN_STATUS_SUCCESS;
}

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

int CleanUpSingleTxBuffer(wlan_private * priv)
{
	wlan_adapter   *adapter = priv->adapter;
	
	if (adapter->SentPacket) {
		adapter->wlan802_3Stat.XmitError++;
		priv->stats.tx_errors++;
		adapter->SentPacket = NULL;
	}

	return WLAN_STATUS_SUCCESS;
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
