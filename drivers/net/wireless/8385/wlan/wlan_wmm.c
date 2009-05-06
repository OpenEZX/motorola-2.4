/*
 *	File: wlan_wmm.c
 */

#include	"include.h"

static u8  wmm_tos2ac[8] = {
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VO,
		AC_PRIO_VO
};
/*
static u8  wmm_tos2ac[16][8] = {
	{	// 0 0 0 0	all enabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 0 0 1	AC_BK should NOT be disabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 0 1 0	AC_BE disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 0 1 1	AC_BE & AC_BK disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 1 0 0	AC_VI disabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 1 0 1	AC_VI & AC_BK disabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 1 1 0	AC_VI & AC_BE disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 0 1 1 1	AC_VI & AC_BE & AC_BK disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_VO,
		AC_PRIO_VO
	},
	{	// 1 0 0 0	AC_VO disabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI
	},
	{	// 1 0 0 1	AC_VO & BK disabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI
	},
	{	// 1 0 1 0	AC_VO & AC_BE disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI
	},
	{	// 1 0 1 1	AC_VO & AC_BE & AC_BK disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI,
		AC_PRIO_VI
	},
	{	// 1 1 0 0	AC_VO & AC_VI disabled
		AC_PRIO_BE,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE
	},
	{	// 1 1 0 1	AC_VO & AC_VI & AC_BK disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE,
		AC_PRIO_BE
	},
	{	// 1 1 1 0	AC_VO & AC_VI & AC_BE disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK
	},
	{	// 1 1 1 1	all disabled
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK,
		AC_PRIO_BK
	}
};*/

static u8  wmm_ac_downgrade[MAX_AC_QUEUES] = {
		AC_PRIO_BK,
		AC_PRIO_BE,
		AC_PRIO_VI,
		AC_PRIO_VO
};

/* This mapping table will be useful if bit-flip is needed */
static u8 wmm_tos2priority[8] = {
/*	Priority   DSCP	  DSCP	 DSCP	WMM
			P2	   P1	  P0	AC	  */
	0x00,	/*	0	   0	  0	AC_BE */
	0x01,	/*	0	   0	  1	AC_BK */
	0x02,	/*	0	   1	  0	AC_BK */
	0x03,	/*	0	   1	  1	AC_BE */
	0x04,	/*	1	   0	  0	AC_VI */
	0x05,	/*	1	   0	  1	AC_VI */
	0x06,	/*	1	   1	  0	AC_VO */
	0x07	/*	1	   1	  1	AC_VO */
};

int wlan_wmm_enable_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter	*Adapter = priv->adapter;
	u32		flags;
	int		ret;

	ENTER();

	switch((int)(*wrq->u.data.pointer)) {
	case CMD_DISABLED: /* disable */
		if (Adapter->MediaConnectStatus == WlanMediaStateConnected)
			return -EPERM; 

		spin_lock_irqsave(&Adapter->CurrentTxLock, flags);
		Adapter->wmm.required = 0;

		if (!Adapter->wmm.enabled) {
			spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);
			return 0;
		}
		else
			Adapter->wmm.enabled = 0;

		if (Adapter->CurrentTxSkb) {
			kfree_skb(Adapter->CurrentTxSkb);
			OS_INT_DISABLE;
			Adapter->CurrentTxSkb = NULL;
			OS_INT_RESTORE;
			priv->stats.tx_dropped++;
		}

		/* Release all skb's in all the queues */
		wmm_cleanup_queues(priv);
		
		spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);

		Adapter->CurrentPacketFilter &= ~HostCmd_ACT_MAC_WMM_ENABLE;

		SetMacPacketFilter(priv);
		break;

	case CMD_ENABLED: /* enable */
		if (Adapter->MediaConnectStatus == WlanMediaStateConnected)
			return -EPERM; 

		spin_lock_irqsave(&Adapter->CurrentTxLock, flags);
		
		Adapter->wmm.required = 1;
		
		spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);

		break;
	case CMD_GET:
		wrq->u.data.length = sizeof(u8);

		if (copy_to_user(wrq->u.data.pointer, &Adapter->wmm.required,
					wrq->u.data.length)) {
			PRINTK1("Copy to user failed\n");
			ret = -EFAULT;
		}

		break;
	default:
		PRINTK("Invalid option\n");
		return -EINVAL;
	}

	return 0;
}

int wlan_do_wmm_tspec_ioctl(wlan_private *priv, struct ifreq *req)
{
	int					ret = 0;
	u16				Action;
	HostCmd_DS_802_11_WMM_TSPEC	*tSpec = &priv->adapter->tspec;

	Action = req->ifr_data[1] | (req->ifr_data[2] << 8);

	switch(Action) {
	case HostCmd_ACT_GEN_GET:
		ret = PrepareAndSendCommand(priv, 
			HostCmd_CMD_802_11_WMM_GET_TSPEC, 0,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, NULL);
		HEXDUMP("Tspec Conf GET", (u8*)tSpec, sizeof(HostCmd_DS_802_11_WMM_TSPEC));
		if (copy_to_user(req->ifr_data + SKIP_TYPE_SIZE, tSpec,
				sizeof(HostCmd_DS_802_11_WMM_TSPEC))) {
			PRINTK1("Copy to user failed\n");
			return -EFAULT;
		}
		break;
	case HostCmd_ACT_GEN_SET:
		memset(tSpec, 0, sizeof(HostCmd_DS_COMMAND));
		if (copy_from_user(tSpec, req->ifr_data + SKIP_TYPE_SIZE,
				sizeof(HostCmd_DS_802_11_WMM_TSPEC))) {
			PRINTK1("Copy from user failed\n");
			return -EFAULT;
		}
		HEXDUMP("Tspec Conf SET", (u8*)tSpec, sizeof(HostCmd_DS_802_11_WMM_TSPEC));
		ret = PrepareAndSendCommand(priv, 
			HostCmd_CMD_802_11_WMM_ADD_TSPEC, 0,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, NULL);
		break;
	case HostCmd_ACT_GEN_REMOVE:
		ret = PrepareAndSendCommand(priv, 
			HostCmd_CMD_802_11_WMM_REMOVE_TSPEC, 0,
			HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, NULL);
		break;
	default:
		PRINTK("Invalid Command\n");
		return -EINVAL;
	}

	return ret;
}

int wlan_do_wmm_para_ie_ioctl(wlan_private *priv, struct ifreq *req)
{
	u16	Action;
	u8	*para_ie = priv->adapter->wmm.Para_IE;

	Action = req->ifr_data[1] | (req->ifr_data[2] << 8);

	switch (Action) {
	case HostCmd_ACT_GEN_GET:
		if (copy_to_user(req->ifr_data + SKIP_TYPE_SIZE, para_ie, 
					WMM_PARA_IE_LENGTH)) {
			PRINTK1("Copy to user failed\n");
			return -EFAULT;
		}

		HEXDUMP("Para IE Conf GET", (u8*)para_ie, WMM_PARA_IE_LENGTH);

		break;
	case HostCmd_ACT_GEN_SET:
		if (priv->adapter->MediaConnectStatus ==
					WlanMediaStateConnected)
			return -EPERM;
	
		HEXDUMP("Para IE Conf SET", (u8*)para_ie, WMM_PARA_IE_LENGTH);

		if (copy_from_user(para_ie, req->ifr_data + SKIP_TYPE_SIZE, 
					WMM_PARA_IE_LENGTH)) {
			PRINTK1("Copy from user failed\n");
			return -EFAULT;
		}
		break;
	default:
		PRINTK("Invalid Option\n");
		return -EINVAL;
	}

	return 0;
}

int wlan_do_wmm_ack_policy_ioctl(wlan_private *priv, struct ifreq *req)
{
	int						ret = 0, i, index;
	HostCmd_DS_802_11_WMM_ACK_POLICY	
				*ackPolicy = &priv->adapter->ackpolicy;

	memset(ackPolicy, 0, sizeof(HostCmd_DS_COMMAND));

	if (copy_from_user(ackPolicy, req->ifr_data + SKIP_TYPE,
				sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY))) {
		PRINTK1("Copy from user failed\n");
		return -EFAULT;
	}
  
	HEXDUMP("Ack Policy Conf", (u8*)ackPolicy, 
				sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY));

	switch(ackPolicy->Action) {
	case HostCmd_ACT_GET:
		for(i=0; i < WMM_ACK_POLICY_PRIO; ++i) {
			ackPolicy->AC = i;

			if((ret = PrepareAndSendCommand(priv, 
				HostCmd_CMD_802_11_WMM_ACK_POLICY, 0,
				HostCmd_OPTION_USE_INT
				| HostCmd_OPTION_WAITFORRSP,
				0, HostCmd_PENDING_ON_NONE, NULL))) {

				LEAVE();
				printk(KERN_DEBUG "PrepareAndSend Failed\n");
				return ret;
			}

			index = SKIP_TYPE_ACTION + (i * 2);
			if (copy_to_user(req->ifr_data + index, 
					(u8*)(&ackPolicy->AC), 16)) {
				printk(KERN_DEBUG "Copy from user failed\n");
				return -EFAULT;
			}

			HEXDUMP("Ack Policy Conf", (u8*)ackPolicy + SKIP_ACTION, 
				sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY));
		}


		break;
	case HostCmd_ACT_SET:
		ackPolicy->AC = req->ifr_data[SKIP_TYPE_ACTION];
		ackPolicy->AckPolicy = req->ifr_data[SKIP_TYPE_ACTION + 1];

		if((ret = PrepareAndSendCommand(priv, 
			HostCmd_CMD_802_11_WMM_ACK_POLICY, 0,
			HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, NULL))) {

			LEAVE();
			return ret;
		}

		if (copy_to_user(req->ifr_data + SKIP_TYPE, ackPolicy,
				sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY))) {
			PRINTK1("Copy from user failed\n");
			return -EFAULT;
		}

		HEXDUMP("Ack Policy Conf", (u8*)ackPolicy, 
			sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY));

		break;
	default:
		printk("Invalid Action\n");
		return -EINVAL;
	}

	return 0;
}
int wlan_cmd_802_11_wmm_tspec(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 cmdno, void *InfoBuf)
{

	cmd->Command = wlan_cpu_to_le16(cmdno);	
	cmd->Size = wlan_cpu_to_le16(
			sizeof(HostCmd_DS_802_11_WMM_TSPEC) + S_DS_GEN);

	memcpy(&cmd->params.tspec, &priv->adapter->tspec, 
				sizeof(HostCmd_DS_802_11_WMM_TSPEC));

	return 0;
}

int wlan_cmd_802_11_wmm_ack_policy(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 action, void *InfoBuf)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_WMM_ACK_POLICY);
	cmd->Size = wlan_cpu_to_le16(
				sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY) + 
							S_DS_GEN);

	memcpy(&cmd->params.ackpolicy, &priv->adapter->ackpolicy, 
				sizeof(HostCmd_DS_802_11_WMM_ACK_POLICY));

	return 0;
}

int wlan_cmd_802_11_wmm_get_status(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 action, void *InfoBuf)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_WMM_GET_STATUS);	
	cmd->Size  = wlan_cpu_to_le16(
			sizeof(HostCmd_DS_802_11_WMM_GET_STATUS)+S_DS_GEN);

	return 0;
}

int wlan_cmd_802_11_wmm_prio_pkt_avail(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 action, void *InfoBuf)
{
	cmd->Command = wlan_cpu_to_le16(HostCmd_CMD_802_11_WMM_PRIO_PKT_AVAIL);
	cmd->Size = wlan_cpu_to_le16(
			sizeof(HostCmd_DS_802_11_WMM_PRIO_PKT_AVAIL)+S_DS_GEN);

	cmd->params.priopktavail.PacketAC = priv->adapter->
						priopktavail.PacketAC;

	return 0;
}

inline int sendWMMStatusChangeCmd(wlan_private *priv)
{
	return PrepareAndSendCommand(priv, HostCmd_CMD_802_11_WMM_GET_STATUS,
			0, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);
}

int wmm_lists_empty(wlan_private *priv)
{
	int i;

	for (i = 0; i < MAX_AC_QUEUES; i++)
		if (!list_empty((struct list_head*)
					&priv->adapter->wmm.TxSkbQ[i]))
			return 0;

	return 1;
}

void wmm_cleanup_queues(wlan_private *priv)
{
	int			i;
	struct sk_buff	*delNode, *Q;

	for (i = 0; i < MAX_AC_QUEUES; i++) {
		Q = &priv->adapter->wmm.TxSkbQ[i];
		
		while (!list_empty((struct list_head *) Q)) {
			delNode = Q->next;
			list_del((struct list_head *)delNode);
			kfree_skb(delNode);
		}
	}
}

#define IPTOS_OFFSET 5
void wmm_map_and_add_skb(wlan_private *priv, struct sk_buff *skb)
{
	wlan_adapter	*Adapter = priv->adapter;
	u8		tos, ac0, ac;
	struct ethhdr	*eth = (struct ethhdr *)skb->data;

	switch (eth->h_proto)
	{
	case __constant_htons(ETH_P_IP):
		PRINTK("packet type ETH_P_IP: %04x, tos=%#x prio=%#x\n",eth->h_proto,skb->nh.iph->tos,skb->priority);
		tos = IPTOS_PREC(skb->nh.iph->tos) >> IPTOS_OFFSET;
		break;
	case __constant_htons(ETH_P_ARP):
		PRINTK("ARP packet %04x\n",eth->h_proto);
	default:
		tos = 0;
		break;
	}

	ac0 = wmm_tos2ac[tos];
	ac = wmm_ac_downgrade[ac0];
	
	skb->priority = wmm_tos2priority[tos];
	PRINTK("wmm_map: tos=%#x, ac0=%#x ac=%#x, prio=%#x\n",tos,ac0,ac,skb->priority);

	/* Access control of the current packet not the Lowest */
	if(ac > AC_PRIO_BE)
		Adapter->wmm.fw_notify = 1;

	list_add_tail((struct list_head *) skb,
			(struct list_head *) &Adapter->wmm.TxSkbQ[ac]);
}

static void wmm_pop_highest_prio_skb(wlan_private *priv)
{
	int i;
	wlan_adapter *Adapter = priv->adapter;
	u8 ac;

	for (i = 0; i < MAX_AC_QUEUES; i++) {
		ac = Adapter->CurBssParams.wmm_queue_prio[i];
		if(!list_empty((struct list_head*) 
					&Adapter->wmm.TxSkbQ[ac])) {
			Adapter->CurrentTxSkb = Adapter->wmm.TxSkbQ[ac].next;
			list_del((struct list_head *) Adapter->
						wmm.TxSkbQ[ac].next);
			break;
		}
	}
}

static void wmm_send_prio_pkt_avail(wlan_private *priv)
{
#if 0	/* WMM_PRIO_PKT_AVAIL command not supported for now */
	int i;

	for (i = 0; i < MAX_AC_QUEUES; i++) {
		ac = Adapter->CurBssParams.wmm_queue_prio[i];
		if(!list_empty((struct list_head*)
					&priv->adapter->wmm.TxSkbQ[ac]))
			break;

	if(i >= MAX_AC_QUEUES)		/* No High prio packets available */
		return;

	priv->adapter->priopktavail.PacketAC = ac;

	PrepareAndSendCommand(priv,
		HostCmd_CMD_802_11_WMM_PRIO_PKT_AVAIL,
		0, HostCmd_OPTION_USE_INT,
		0, HostCmd_PENDING_ON_NONE, 
		NULL);
#endif
}

#ifdef DEBUG_LEVEL1
static void wmm_debugPrintAC(const char* acStr, const WMM_AC_PARAS* pACParam)
{
	PRINTK1("WMM AC_%s: ACI=%d, ACM=%d, AIFSN=%d, "
			"ECWmin=%d, ECWmax=%d, Txop_Limit=%d\n",
			acStr, pACParam->ACI_AIFSN.ACI, pACParam->ACI_AIFSN.ACM,
			pACParam->ACI_AIFSN.AIFSN, pACParam->ECW.ECW_Min,
			pACParam->ECW.ECW_Max, wlan_le16_to_cpu(pACParam->Txop_Limit));

}
#define PRINTK_AC(acStr, pACParam) wmm_debugPrintAC(acStr, pACParam)
#else
#define PRINTK_AC(acStr, pACParam) 
#endif


/** 
 *	@brief Initialize WMM priority queue
 *	@param priv		  pointer to wlan_private
 *	@return			  N/A
 */
static void wmm_setup_queue_priorities(wlan_private *priv)
{
	wlan_adapter *Adapter = priv->adapter;
#ifdef WMM_AIFS
	WMM_PARAMETER_IE* pIe;
	u16 cwmax, cwmin, avg_back_off, tmp[4];
	int i, j, n;

	n = 0;
	pIe = (WMM_PARAMETER_IE*)&Adapter->BSSIDList[Adapter
												 ->ulCurrentBSSIDIndex].Wmm_IE;

	HEXDUMP("WMM: setup_queue_priorities: param IE", 
			(u8 *)pIe, sizeof(WMM_PARAMETER_IE));

	PRINTK1("WMM Parameter IE: version=%d, "
			"QoSInfo Parameter Set Count=%d, Reserved=%#x\n",
			pIe->Version, pIe->QoSInfo.ParaSetCount, pIe->Reserved);

	/*
	 * AC_BE
	 */
	cwmax = (1 << pIe->AC_Paras_BE.ECW.ECW_Max) - 1;
	cwmin = (1 << pIe->AC_Paras_BE.ECW.ECW_Min) - 1;
	avg_back_off = (cwmin >> 1) + pIe->AC_Paras_BE.ACI_AIFSN.AIFSN;
	Adapter->CurBssParams.wmm_queue_prio[n] = AC_PRIO_BE;
	tmp[n++] = avg_back_off;
	
	PRINTK_AC("BE", &pIe->AC_Paras_BE);
	PRINTK1("WMM AC_BE: CWmax=%d CWmin=%d Avg Back-off=%d\n",
			cwmax, cwmin, avg_back_off);
	
	/*
	 * AC_BK
	 */
	cwmax = (1 << pIe->AC_Paras_BK.ECW.ECW_Max) - 1;
	cwmin = (1 << pIe->AC_Paras_BK.ECW.ECW_Min) - 1;
	avg_back_off = (cwmin >> 1) + pIe->AC_Paras_BK.ACI_AIFSN.AIFSN;
	Adapter->CurBssParams.wmm_queue_prio[n] = AC_PRIO_BK;
	tmp[n++] = avg_back_off;
	
	PRINTK_AC("BK", &pIe->AC_Paras_BK);
	PRINTK1("WMM AC_BK: CWmax=%d CWmin=%d Avg Back-off=%d\n",
			cwmax, cwmin, avg_back_off);
	
	/*
	 * AC_VI
	 */
	cwmax = (1 << pIe->AC_Paras_VI.ECW.ECW_Max) - 1;
	cwmin = (1 << pIe->AC_Paras_VI.ECW.ECW_Min) - 1;
	avg_back_off = (cwmin >> 1) + pIe->AC_Paras_VI.ACI_AIFSN.AIFSN;
	Adapter->CurBssParams.wmm_queue_prio[n] = AC_PRIO_VI;
	tmp[n++] = avg_back_off;
	
	PRINTK_AC("VI", &pIe->AC_Paras_VI);
	PRINTK1("WMM AC_VI: CWmax=%d CWmin=%d Avg Back-off=%d\n",
			cwmax, cwmin, avg_back_off);
	
	/*
	 * AC_VO
	 */
	cwmax = (1 << pIe->AC_Paras_VO.ECW.ECW_Max) - 1;
	cwmin = (1 << pIe->AC_Paras_VO.ECW.ECW_Min) - 1;
	avg_back_off = (cwmin >> 1) + pIe->AC_Paras_VO.ACI_AIFSN.AIFSN;
	Adapter->CurBssParams.wmm_queue_prio[n] = AC_PRIO_VO;
	tmp[n++] = avg_back_off;
	
	PRINTK_AC("VO", &pIe->AC_Paras_VO);
	PRINTK1("WMM AC_VO: CWmax=%d CWmin=%d Avg Back-off=%d\n",
			cwmax, cwmin, avg_back_off);
	
	
	HEXDUMP("WMM avg_back_off  ", (u8 *)tmp, sizeof(tmp));
	HEXDUMP("WMM wmm_queue_prio", Adapter->CurBssParams.wmm_queue_prio,
			sizeof(Adapter->CurBssParams.wmm_queue_prio));

	/* bubble sort */
	for (i=0; i < n; i++) {
		for (j=1; j < n - i; j++) {
			if (tmp[j-1] > tmp[j]) {
				SWAP_U16(tmp[j-1],tmp[j]);
				SWAP_U8(Adapter->CurBssParams.wmm_queue_prio[j-1],
						Adapter->CurBssParams.wmm_queue_prio[j]);
			}
			else if (tmp[j-1] == tmp[j]) {
				if (Adapter->CurBssParams.wmm_queue_prio[j-1] <
					Adapter->CurBssParams.wmm_queue_prio[j]) {
					SWAP_U8(Adapter->CurBssParams.wmm_queue_prio[j-1],
							Adapter->CurBssParams.wmm_queue_prio[j]);
				}
			}
		}
	}
	
	HEXDUMP("WMM avg_back_off ", (u8 *)tmp, sizeof(tmp));
	HEXDUMP("WMM wmm_queue_prio", Adapter->CurBssParams.wmm_queue_prio,
			sizeof(Adapter->CurBssParams.wmm_queue_prio));

#else /* WMM_AIFS */
	/* default queue priorities: VO->VI->BE->BK */
	Adapter->CurBssParams.wmm_queue_prio[0] = AC_PRIO_VO;
	Adapter->CurBssParams.wmm_queue_prio[1] = AC_PRIO_VI;
	Adapter->CurBssParams.wmm_queue_prio[2] = AC_PRIO_BE;
	Adapter->CurBssParams.wmm_queue_prio[3] = AC_PRIO_BK;
#endif
}

static void wmm_setup_ac_downgrade(wlan_private *priv, u8 acstatus )
{
	wlan_adapter *Adapter = priv->adapter;
	u8 ac0, ac;
	int i, j;

	PRINTK("acstatus = %#x\n", acstatus);

	for (i = 0; i < MAX_AC_QUEUES; i++) {	/* default settings without ACM */
		wmm_ac_downgrade[i] = (u8)i;
	}
	HEXDUMP("wmm_ac_downgrade default", wmm_ac_downgrade, 
			sizeof(wmm_ac_downgrade));

	for (i = 0; i < MAX_AC_QUEUES - 1; i++) {	/* skip the last one */
		ac0 = Adapter->CurBssParams.wmm_queue_prio[i];	/* original AC */
		ac = Adapter->CurBssParams.wmm_queue_prio[i+1];	/* downgraded AC */
		if (acstatus & (1 << ac0)) {
			for (j = 0; j < MAX_AC_QUEUES; j++) {
				if (ac0 == wmm_ac_downgrade[j]) {
					PRINTK("wmm_setup_ac_downgrade: i=%d j=%d "
						   "ac0=%d ac=%d\n", i, j, ac0, ac);
					wmm_ac_downgrade[j] = ac;
				}
			}
			PRINTK("wmm_setup_ac_downgrade: AC %#x has been "
				   "downgraded to %#x\n", ac0, ac);
		}
	}

	HEXDUMP("wmm_ac_downgrade", wmm_ac_downgrade, sizeof(wmm_ac_downgrade));

	ac0 = Adapter->CurBssParams.wmm_queue_prio[MAX_AC_QUEUES - 1];
	if (acstatus & (1 << ac0)) {
		PRINTK("wmm_setup_ac_downgrade: ignored ACM for the AC %#x "
			   "as it has the lowest priority\n", ac0);
	}
}

/*
 *	Process the GET_WMM_STATUS command response from firmware
 *
 *	The GET_WMM_STATUS command returns multiple TLVs for:
 *		- Each AC Queue status
 *		- Current WMM Paramter IE
 *
 *	This function parses the TLVs and then calls further functions
 *	 to process any changes in the queue prioritization or state.
 *
 *	Parameters:
 *		priv		Pointer to the wlan_private driver data struct
 *		cmdResp		TLV buffer pointer containing TLVs for each queue and
 *					a TLV containing the WMM Parameter IE.
 * 
 *	Return: void
 */
void wlan_cmdresp_get_wmm_status(wlan_private *priv,
								 u8* pCmdResp, u32 respSize)
{
	wlan_adapter *Adapter = priv->adapter;
	u8* pCurrent = pCmdResp;
	u32 respLen = respSize;
	int valid = TRUE;

	/* x x x x O I E K --- 1=disabled 0=enabled */
	u8 acstatus = 0;
	
	MrvlIEtypesGeneric_t* pTlvHdr;
	MrvlIEtypes_WmmQueueStatus_t* pTlvWmmQStatus;
	WMM_PARAMETER_IE* pWmmParamIe;

	PRINTK("WMM: WMM_GET_STATUS cmdresp received: %d\n", respLen);
	HEXDUMP("CMD_RESP: WMM_GET_STATUS", pCmdResp, respLen);

	while ((respLen >= sizeof(pTlvHdr->Header)) && valid) {
		pTlvHdr = (MrvlIEtypesGeneric_t*)pCurrent;

		switch (pTlvHdr->Header.Type)
		{
		case TLV_TYPE_WMMQSTATUS:
			pTlvWmmQStatus = (MrvlIEtypes_WmmQueueStatus_t*)pTlvHdr;
			PRINTK("CMD_RESP: WMM_GET_STATUS: QSTATUS TLV: %d, %d, %d\n",
				   pTlvWmmQStatus->QueueIndex, 
				   pTlvWmmQStatus->FlowRequired,
				   pTlvWmmQStatus->Disabled);
			
			acstatus |= (pTlvWmmQStatus->Disabled 
						 << pTlvWmmQStatus->QueueIndex);
			break;
			
		case WMM_IE:
			/*
			 * Point the regular IEEE IE 2 bytes into the Marvell TLV
			 *	 and setup the IEEE IE type and length byte fields
			 */
			pWmmParamIe = (WMM_PARAMETER_IE*)(pCurrent + 2);
			pWmmParamIe->Length = pTlvHdr->Header.Len;
			pWmmParamIe->ElementId = WMM_IE;

			PRINTK("CMD_RESP: WMM_GET_STATUS: WMM Parameter Set: %d\n",
				   pWmmParamIe->QoSInfo.ParaSetCount);

			memcpy(Adapter->BSSIDList[Adapter->ulCurrentBSSIDIndex].Wmm_IE,
				   pWmmParamIe,
				   pWmmParamIe->Length + 2);
			break;

		default:
			valid = FALSE;
			break;
		}
		
		pCurrent += (pTlvHdr->Header.Len + sizeof(pTlvHdr->Header));
		respLen	 -= (pTlvHdr->Header.Len + sizeof(pTlvHdr->Header));
	}

	wmm_setup_queue_priorities(priv);
	wmm_setup_ac_downgrade(priv, acstatus);

	os_carrier_on(priv);
	netif_wake_queue(priv->wlan_dev.netdev);
}



void wmm_process_tx(wlan_private * priv)
{
	wlan_adapter   *Adapter = priv->adapter;
	u32		flags;

	OS_INTERRUPT_SAVE_AREA;	/* Needed for Threadx; Dummy for Linux */

	ENTER();
	
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

	spin_lock_irqsave(&Adapter->CurrentTxLock, flags);

	if(priv->wlan_dev.dnld_sent) {

		if(priv->adapter->wmm.fw_notify) {
			wmm_send_prio_pkt_avail(priv);
			priv->adapter->wmm.fw_notify = 0;
		}

		spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);
		
		return;
	}

	wmm_pop_highest_prio_skb(priv);
	spin_unlock_irqrestore(&Adapter->CurrentTxLock, flags);

	if (MrvDrvSend(priv, Adapter->CurrentTxSkb)) {
		if (Adapter->CurrentTxSkb) { 
			kfree_skb(Adapter->CurrentTxSkb);
			OS_INT_DISABLE;
			Adapter->CurrentTxSkb = NULL;
			OS_INT_RESTORE;
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
