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

#define UpdateTransStart(dev) { \
	dev->trans_start = jiffies; \
}

static inline void wlan_sched_timeout(u32 millisec)
{
	set_current_state(TASK_INTERRUPTIBLE);

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

static inline int wlan_upload_rx_packet(wlan_private *priv, u8 *ptr, int size)
{
    	struct sk_buff *skb = NULL;
    	u8             *skb_ptr;

#ifdef	DEBUG
    	unsigned short prot_sn, ping_sn;
#endif	/* DEBUG */

#ifdef OMAP1510_TIMER_DEBUG
	int		i, j;
	extern u32	times[20][10];
	extern int	tm_ind[10];
#endif /* OMAP1510_TIMER_DEBUG */

#define IPFIELD_ALIGN_OFFSET	2
	/* 2 bytes are reserved bytes of skb */
	if (!(skb = dev_alloc_skb(size + IPFIELD_ALIGN_OFFSET))) {
		PRINTK(KERN_ERR "No skb free!");
		priv->stats.rx_dropped++;
		return -1;
	}

	//    HEXDUMP("Before copy to skb", ptr, MIN(size, 100));
	
	skb_reserve(skb, IPFIELD_ALIGN_OFFSET);  /*16 Byte Align the IP fields */ 

	skb_ptr = skb_put(skb, size);

	memcpy(skb_ptr, ptr, size);

	//  HEXDUMP("After copy to skb", skb_ptr, MIN(size, 100));
	PRINTK1("skb->data=%p skb_ptr=%p\n", skb->data, skb_ptr);

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
		//outw(0x22, 0xFFFEC500);
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

static inline void wlan_free_tx_packet(wlan_private *priv)
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

#endif /* _OS_MACROS_H */
