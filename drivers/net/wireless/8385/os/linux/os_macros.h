/*
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

#ifndef	_OS_MACROS_H
#define _OS_MACROS_H

#define os_time_get()	jiffies

#define OS_INT_DISABLE		cli()
#define OS_INT_RESTORE		sti()

#define	TX_DISABLE			/* require for threadx */
#define TX_RESTORE			/* require for threadx */
#define	ConfigureThreadPriority()	/* required for threadx */

/* Define these to nothing, these are needed only for threadx */
#define OS_INTERRUPT_SAVE_AREA
#define OS_FREE_LOCK(x)
#define TX_EVENT_FLAGS_SET(x, y, z)

#define UpdateTransStart(dev) { \
	dev->trans_start = jiffies; \
}

#define OS_SET_THREAD_STATE(x)		set_current_state(x)

static inline void os_sched_timeout(u32 millisec)
{
	set_current_state(TASK_INTERRUPTIBLE);

	schedule_timeout((millisec * HZ) / 1000);
}

static inline void os_schedule(u32 millisec)
{
	schedule_timeout((millisec * HZ) / 1000);
}

static inline int CopyMulticastAddrs(wlan_adapter *Adapter, 
						struct net_device *dev)
{
	int			i = 0;
	struct dev_mc_list	*mcptr = dev->mc_list;
				
	for (i = 0; i < dev->mc_count; i++) {
		memcpy(&Adapter->MulticastList[i], mcptr->dmi_addr, ETH_ALEN);
		mcptr = mcptr->next;
	}


	return i;

}

static inline int os_upload_rx_packet(wlan_private *priv, 
				struct sk_buff *skb)
{
#ifdef	DEBUG
    	unsigned short prot_sn, ping_sn;
#endif	/* DEBUG */

#ifdef OMAP1510_TIMER_DEBUG
	int		i, j;
	extern u32	times[20][10];
	extern int	tm_ind[10];
#endif /* OMAP1510_TIMER_DEBUG */

#define IPFIELD_ALIGN_OFFSET	2
	PRINTK1("skb->data=%p\n", skb->data);
//	HEXDUMP("After copy to skb", skb->data, MIN(skb->len, 100));

#ifdef	DEBUG
	prot_sn = ((unsigned short *)(skb->data))[17];
	ping_sn = ((unsigned short *)(skb->data))[20];
	if(!prot_sn) 
		PRINTK("R[%04X]",ping_sn);
#endif	/* DEBUG */

	skb->dev = priv->wlan_dev.netdev;
	skb->protocol = eth_type_trans(skb, priv->wlan_dev.netdev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

#ifdef OMAP1510_TIMER_DEBUG
	if(tm_ind[5] + 1 < 10) {
		times[5][(tm_ind[5])++] = inw(0xFFFEC500 + 0x08);
	}

	for(j=0;j<6;++j) {
		for(i=0;i<tm_ind[j];++i)
			printk("times[%d][%d] = %lu, ", j, i, times[j][i]);
	}

	printk("\n");
#endif /* OMAP1510_TIMER_DEBUG */
	
	netif_rx(skb);

	return 0;
}

static inline void os_free_tx_packet(wlan_private *priv)
{
	u32	flags;

	spin_lock_irqsave(&priv->adapter->CurrentTxLock, flags);
	
	if (priv->adapter->CurrentTxSkb) {
		kfree_skb(priv->adapter->CurrentTxSkb);
		if (netif_queue_stopped(priv->wlan_dev.netdev))
			netif_wake_queue(priv->wlan_dev.netdev);
	}

	priv->adapter->CurrentTxSkb = NULL;
	
	spin_unlock_irqrestore(&priv->adapter->CurrentTxLock, flags);
}

static inline void os_carrier_on(wlan_private *priv) 
{
	if (!netif_carrier_ok(priv->wlan_dev.netdev) &&
		(priv->adapter->MediaConnectStatus == WlanMediaStateConnected))
		netif_carrier_on(priv->wlan_dev.netdev);
}

static inline void os_carrier_off(wlan_private *priv) 
{
	if (netif_carrier_ok(priv->wlan_dev.netdev))
		netif_carrier_off(priv->wlan_dev.netdev);
}

#endif /* _OS_MACROS_H */
