/*
 *              File: wlan_passthrough.c
 */

#include	"include.h"

#define	MRVDRV_NUM_OF_PASSTHROUGH_PKT		16

int AllocatePassthroughList(wlan_private * priv)
{
	int			i, size;
	wlan_adapter		*Adapter = priv->adapter;
	PTDataPacketList	*PTDataList;
	PTDataPacket		*PTDataPkt;
	u32			flags;

	ENTER();

	size = sizeof(PTDataPacketList) * MRVDRV_NUM_OF_PASSTHROUGH_PKT;

	if (!(Adapter->PTDataList = PTDataList = kmalloc(size, GFP_KERNEL))) {
		PRINTK1("Passthrough mode mem allocation failed\n");
		return -1;
	}

	PRINTK1("PTDataList Allocated\n");

	spin_lock_irqsave(&Adapter->PTDataLock, flags);
	for (i = 0; i < MRVDRV_NUM_OF_PASSTHROUGH_PKT; i++) {
		/*
		 * add to the free Q 
		 */
		list_add_tail((struct list_head *) &PTDataList[i],
				&Adapter->PTFreeDataQ);
	}

	PRINTK1("PTFreeDataQ Initialized\n");

	/*
	 * Allocate memory for passthrough pkt nodes 
	 */
	size = sizeof(PTDataPacket) * MRVDRV_NUM_OF_PASSTHROUGH_PKT;
	if (!(Adapter->PTDataPkt = PTDataPkt = kmalloc(size, GFP_KERNEL))) {
		PRINTK1("Passthrough mode mem allocation failed\n");
		kfree(PTDataList);
		spin_unlock_irqrestore(&Adapter->PTDataLock, flags);
		return -1;
	}

	PRINTK1("PTDataPkt Allocated\n");

	for (i = 0; i < MRVDRV_NUM_OF_PASSTHROUGH_PKT; i++) {
		PTDataList[i].pt_pkt = PTDataPkt;
		PRINTK1("PTDataPkt[%d] = %p, Size=%d\n", i, PTDataPkt,
				sizeof(PTDataPacket));
		PTDataPkt++;
	}

	PRINTK1("pt_pkt nodes initialized\n");
	
	spin_unlock_irqrestore(&Adapter->PTDataLock, flags);

	LEAVE();
	return 0;
}

void FreePassthroughList(wlan_adapter * Adapter)
{
	if (Adapter->PTDataPkt) {
		kfree(Adapter->PTDataPkt);
		Adapter->PTDataPkt = NULL;
	}

	if (Adapter->PTDataList) {
		kfree(Adapter->PTDataList);
		Adapter->PTDataList = NULL;
	}
}

int GetPassthroughPacket(wlan_adapter * Adapter, u8 ** ptr, int *payload)
{
	PTDataPacketList *PTPktList;
	int             ret;

	ENTER();

	/*
	 * extract a node from data Q copy its contents to user space put the 
	 * node back to empty Q 
	 */

	if (!list_empty(&Adapter->PTDataQ)) {
		PTPktList = (PTDataPacketList *) Adapter->PTDataQ.next;
		list_del((struct list_head *) PTPktList);
		PRINTK1("current pt_pkt=%p\n", PTPktList->pt_pkt);
		*ptr = (u8 *) PTPktList->pt_pkt;
		*payload = PTPktList->pt_pkt->SizeOfPkt;

		HEXDUMP("Contents of pasthrough data given to app",
				(u8 *) PTPktList->pt_pkt, 40);

		/*
		 * put the node to free Q 
		 */
		list_add_tail((struct list_head *) PTPktList,
				&Adapter->PTFreeDataQ);
		ret = 0;
	} else {			/* no packet, sleep now */
		ret = -1;
	}

	LEAVE();
	return ret;
}
