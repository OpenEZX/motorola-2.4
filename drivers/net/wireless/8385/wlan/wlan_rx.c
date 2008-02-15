/*
 * File : wlan_rx.c
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * (c) Copyright 2006, Motorola, Inc.
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
 *
 * 2006-Jul-07 Motorola - Add power management related changes.
 */

#include	"include.h"

#ifdef SLIDING_WIN_AVG
static u8 wlan_getAvgSNR(wlan_private * priv)
{
	u8 i;
	u16 temp = 0;
	wlan_adapter   *Adapter = priv->adapter;
	if(Adapter->numSNRNF == 0)
		return 0;
	for (i = 0; i <Adapter->numSNRNF; i++)
		temp += Adapter->rawSNR[i];
	return (u8)(temp/Adapter->numSNRNF);
	
}

static u8 wlan_getAvgNF(wlan_private * priv)
{
	u8 i;
	u16 temp = 0;
	wlan_adapter   *Adapter = priv->adapter;
	if(Adapter->numSNRNF == 0)
		return 0;
	for (i = 0; i <Adapter->numSNRNF; i++)
		temp += Adapter->rawNF[i];
	return (u8)(temp/Adapter->numSNRNF);
	
}

static void wlan_save_rawSNRNF(wlan_private * priv, RxPD * pRxPD)
{
	wlan_adapter   *Adapter = priv->adapter;
	if(Adapter->numSNRNF < Adapter->data_avg_factor)
		Adapter->numSNRNF++;
	Adapter->rawSNR[Adapter->nextSNRNF] = pRxPD->SNR;
	Adapter->rawNF[Adapter->nextSNRNF] = pRxPD->NF;
	Adapter->nextSNRNF++;
	if(Adapter->nextSNRNF >= Adapter->data_avg_factor)
		Adapter->nextSNRNF = 0;
	return;
}
#endif //SLIDING_WIN_AVG
/*
 * If we recieve the rx data process and push them to the above layers 
 */

static int wlan_compute_rssi(wlan_private * priv, RxPD * pRxPD)
{
	wlan_adapter   *Adapter = priv->adapter;

	ENTER();

	PRINTK1("RxPD: SNR = %d, NF = %d\n", pRxPD->SNR, pRxPD->NF);
	PRINTK1("Before computing SNR and NF\n");
	PRINTK1("Adapter: SNR- avg = %d, NF-avg = %d\n",
		Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE, 
		Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE);

	Adapter->SNR[TYPE_RXPD][TYPE_NOAVG] = pRxPD->SNR;
	Adapter->NF[TYPE_RXPD][TYPE_NOAVG] = pRxPD->NF;
#ifdef SLIDING_WIN_AVG
	wlan_save_rawSNRNF(priv , pRxPD);
#endif //SLIDING_WIN_AVG
	Adapter->RxPDSNRAge = os_time_get();
	Adapter->RxPDRate = pRxPD->RxRate;

	/*
	 * Average out the SNR from the received packet 
	 */
#ifdef SLIDING_WIN_AVG
	Adapter->SNR[TYPE_RXPD][TYPE_AVG] = wlan_getAvgSNR(priv) * AVG_SCALE;
	Adapter->NF[TYPE_RXPD][TYPE_AVG] = wlan_getAvgNF(priv ) * AVG_SCALE;
#else
	Adapter->SNR[TYPE_RXPD][TYPE_AVG] =
		CAL_AVG_SNR_NF(Adapter->SNR[TYPE_RXPD][TYPE_AVG], pRxPD->SNR, Adapter->data_avg_factor);

	/*
	 * Average out the NF value 
	 */
	Adapter->NF[TYPE_RXPD][TYPE_AVG] = 
		CAL_AVG_SNR_NF(Adapter->NF[TYPE_RXPD][TYPE_AVG], pRxPD->NF, Adapter->data_avg_factor);
#endif //SLIDING_WIN_AVG
	PRINTK1("After computing SNR and NF\n");
	PRINTK1("Adapter: SNR-avg = %d, NF-avg = %d\n",
		Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE, 
		Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE);

	/*
	 * store the RXPD values in SNRNF variable, 
	 * which is present in adapter structure 
	 */
	Adapter->SNRNF[SNR_RXPD][TYPE_NOAVG]	=	
		Adapter->SNR[TYPE_RXPD][TYPE_NOAVG];
	Adapter->SNRNF[SNR_RXPD][TYPE_AVG]	=	
		Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE; 

	Adapter->SNRNF[NF_RXPD][TYPE_NOAVG]	=	
		Adapter->NF[TYPE_RXPD][TYPE_NOAVG];
	Adapter->SNRNF[NF_RXPD][TYPE_AVG]	=	
		Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE;
   
	// NOTE: Adapter->SNRNF[SNR_BEACON][X] - represents BEACON for SNR
	//       Adapter->SNRNF[NF_BEACON][X] - represents BEACON for NF

	Adapter->RSSI[TYPE_RXPD][TYPE_NOAVG] = 
		CAL_RSSI(Adapter->SNR[TYPE_RXPD][TYPE_NOAVG],
			Adapter->NF[TYPE_RXPD][TYPE_NOAVG]);

	Adapter->RSSI[TYPE_RXPD][TYPE_AVG] = 
		CAL_RSSI(Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE,
			Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE);
	
	LEAVE();
	return 0;
}

int ProcessRxed_802_3_Packet(wlan_private * priv, struct sk_buff *skb)
{
    	wlan_adapter	*Adapter = priv->adapter;
	RxPD		*pRxPD = (RxPD *) skb->data;
    	u8  		*ptr = skb->data;
    	int             i;

    	ENTER();

 	HEXDUMP("Before chop RxPD", ptr, MIN(skb->len, 100));

	// chop off the wireless portion
	/* TODO: SDCF: wlan_dev->upld_len is the wrong value to check
	for length of received packet. Please use the correct variable */
	if (skb->len < (ETH_HLEN + 8 + sizeof(RxPD))) {
		PRINTK1("WARNING!! FRAME RECEIVED WITH BAD LENGTH\n");
		priv->stats.rx_length_errors++;
		LEAVE();
		return 0;
	}
#ifdef BIG_ENDIAN
	endian_convert_RxPD(pRxPD);
#endif

	// Check RxPD status and update 802.3 stat,
	// we should have more bits to test
	if (!(pRxPD->Status & MRVDRV_RXPD_STATUS_OK)) {
		PRINTK1("WARNING: frame received with bad status\n");
		priv->stats.rx_errors++;
		LEAVE();
		return 0;
	}

	ptr += sizeof(RxPD);

	HEXDUMP("After chop RxPD", ptr, skb->len-sizeof(RxPD));

	HEXDUMP("Dest   Mac", ptr, MRVDRV_ETH_ADDR_LEN);
	HEXDUMP("Source Mac", (ptr + MRVDRV_ETH_ADDR_LEN), MRVDRV_ETH_ADDR_LEN);

#ifndef FW_REMOVED_SNAP_HEADER
	// Shift everything after the DEST/SRC 8 bytes forward...
	// to take care of the SNAP header that is somewhere here...
	for (i = 11; i >= 0; i--)
		ptr[MRVDRV_SNAP_HEADER_LEN + i] = ptr[i];

	ptr += MRVDRV_SNAP_HEADER_LEN;
#endif

	/* Take the data rate from the RxPD structure 
	 * only if the rate is auto */
	if (Adapter->Is_DataRate_Auto)
		Adapter->DataRate = index_to_data_rate(pRxPD->RxRate);

#ifndef MULTIPLERX_SUPPORT
	wlan_compute_rssi(priv, pRxPD);
#endif

#ifndef FW_REMOVED_SNAP_HEADER
	/* Compute actual packet size */
	i = MRVDRV_SNAP_HEADER_LEN + sizeof(RxPD);
#else
	i = sizeof(RxPD);
#endif
	skb_pull(skb, i);
	
	PRINTK1("Size of actual 802.3 packet = %d\n", skb->len);

	if (!os_upload_rx_packet(priv, skb)) {
		priv->stats.rx_bytes += skb->len; 
		priv->stats.rx_packets++;
#ifdef WPRM_DRV
		WPRM_DRV_TRACING_PRINT();
		wprm_rx_packet_cnt ++;
		wprm_traffic_measurement(priv, Adapter, FALSE);
#endif

		LEAVE();
		return 0;
	} else {
		LEAVE();
		return WLAN_STATUS_FAILURE;
	}

}

#ifdef	MULTIPLERX_SUPPORT

int ProcessRxed_802_11_Packet(wlan_private * priv, RxPD * pRxPD)
{
	mv_mspio_dev_rec_t *wlan_dev_t = &priv->wlan_dev;
	int             len = pRxPD->PktLen;
	u8             *data = (u8 *) (wlan_dev->upld_buf + sizeof(RxPD));

#ifdef PASSTHROUGH_MODE
	PTDataPacketList *node;
	int             flags;
	wlan_adapter   *Adapter = priv->adapter;
#endif

	ENTER();

	PRINTK1("802_11 Packet Length = %x\n", len);
	HEXDUMP("802_11DATA", data, MIN(len, 100));
#ifdef PASSTHROUGH_MODE
	spin_lock_irqsave(&Adapter->PTDataLock, flags);
	if (!list_empty(&Adapter->PTFreeDataQ)) {
		node = (PTDataPacketList *) Adapter->PTFreeDataQ.next;
		list_del((struct list_head *) node);

		if (len > (sizeof(PTDataPacket) - 4)) {
			PRINTK1("Insufficient memory for data pkt!\n");
			len = sizeof(PTDataPacket) - 4;
		}

		memset(node->pt_pkt, 0, sizeof(PTDataPacket));
		node->pt_pkt->PTPktType = MRVDRV_RX_PKT_TYPE_PASSTHROUGH;
		node->pt_pkt->RSSI =
			/* 
			 * not sure where the result is getting stored, 
			 * since CAL_RSSI is a macro
			 */		
			CAL_RSSI(Adapter->SNR[TYPE_RXPD][TYPE_AVG] / AVG_SCALE, 
				Adapter->NF[TYPE_RXPD][TYPE_AVG] / AVG_SCALE);
		node->pt_pkt->SizeOfPkt = len;
		memcpy(node->pt_pkt->packet, data, len);

		list_add_tail((struct list_head *) node, &Adapter->PTDataQ);
		PRINTK1("Passthrough packet added to PTDataQ\n");
	}

	spin_unlock_irqrestore(&Adapter->PTDataLock, flags);
#ifndef PASSTHROUGH_NONBLOCK
	wake_up_interruptible(&Adapter->wait_PTData);
#endif
#endif

	LEAVE();
	return 0;
}

int ProcessRxed_Debug_Packet(wlan_private * priv, RxPD * pRxPD)
{
	int             len = pRxPD->PktLen;
	mv_mspio_dev_rec_t *wlan_dev_t = &priv->wlan_dev;
	char           *data = (u8 *) (wlan_dev->upld_buf + sizeof(RxPD));

	ENTER();

	*(data + len) = '\0';
	PRINTK1("FWDBG: %s\n", data);

	LEAVE();
	return 0;
}

int ProcessRxed_EventData_Packet(wlan_private * priv, RxPD * pRxPD)
{
	mv_mspio_dev_rec_t *wlan_dev_t = &priv->wlan_dev;
	char           *data = (u8 *) (wlan_dev->upld_buf + sizeof(RxPD));
	int             len = pRxPD->PktLen;

	ENTER();

	*(data + len) = '\0';
	PRINTK1("EVENTDATA: %s\n", data);

	LEAVE();
	return 0;
}

int ProcessRxed_TxDone_Packet(wlan_private * priv, RxPD * pRxPD)
{
	return 0;
}

static int      (*ProcessRxPacket[MRVDRV_RXPD_STATUS_MAXTYPES_RX])
	(wlan_private * priv, RxPD * pRxPD) = {
		ProcessRxed_802_3_Packet,
		ProcessRxed_802_11_Packet,
		ProcessRxed_Debug_Packet,
		ProcessRxed_EventData_Packet, 
		ProcessRxed_TxDone_Packet,
};

int ProcessRxedPacket(wlan_private * priv, RxPD * pRxPD)
{
	int 	    pkttype;
	int             ret;

	ENTER();
	/*
	 * Check the packet type & then process it accordingly 
	 */
	pkttype = ((pRxPD->Status >> 8) & 0x0F);
	PRINTK1("Rx Packet Type = %d\n", pkttype);
    
	if (pkttype >= (MRVDRV_RXPD_STATUS_MAXTYPES_RX - 1)) {
		PRINTK1("Invalid Receive Payload Type. Discarding!!!\n");
		LEAVE();
		return -1;
	}

	wlan_compute_rssi(priv, pRxPD);

	ret = ProcessRxPacket[pkttype] (priv, pRxPD);
	LEAVE();
	return ret;
}
#endif /* MULTIPLERX_SUPPORT */
