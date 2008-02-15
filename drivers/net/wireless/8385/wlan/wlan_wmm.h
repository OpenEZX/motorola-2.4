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
#ifndef __WLAN_WMM_H
#define __WLAN_WMM_H

enum AC_QUEUES {
	AC_PRIO_BK,
	AC_PRIO_BE,
	AC_PRIO_VI,
	AC_PRIO_VO,
	MAX_AC_QUEUES
};

#define WMM_IE_LENGTH				0x0009
#define WMM_PARA_IE_LENGTH			0x0018
#define WMM_QOS_INFO_OFFSET			(0x08)
#define WMM_QOS_INFO_UAPSD_BIT			(0x80)
#define WMM_OUISUBTYPE_IE			0x00
#define WMM_OUISUBTYPE_PARA			0x01
#define WMM_TXOP_LIMIT_UNITS_SHIFT		5

#define HostCmd_CMD_802_11_WMM_GET_TSPEC	0x006E
#define HostCmd_CMD_802_11_WMM_ADD_TSPEC	0x006F
#define HostCmd_CMD_802_11_WMM_REMOVE_TSPEC	0x0070
#define HostCmd_CMD_802_11_WMM_ACK_POLICY	0x005C
#define HostCmd_CMD_802_11_WMM_PRIO_PKT_AVAIL	0x005D
#define HostCmd_CMD_802_11_WMM_GET_STATUS	0x0071
#define HostCmd_RET_802_11_WMM_GET_TSPEC	0x806E
#define HostCmd_RET_802_11_WMM_ADD_TSPEC	0x806F
#define HostCmd_RET_802_11_WMM_REMOVE_TSPEC	0x8070
#define HostCmd_RET_802_11_WMM_ACK_POLICY	0x805C
#define HostCmd_RET_803_11_WMM_PRIO_PKT_AVAIL	0x805D
#define HostCmd_RET_802_11_WMM_GET_STATUS	0x8071
#define HostCmd_ACT_MAC_WMM_ENABLE		0x0800
#define MACREG_INT_CODE_WMM_STATUS_CHANGE	0x0017

typedef struct _wlan_ioctl_wmm_para_ie  wlan_ioctl_wmm_para_ie;
typedef wlan_ioctl_wmm_para_ie  *pwlan_ioctl_wmm_para_ie;
typedef struct _wlan_ioctl_wmm_tspec  wlan_ioctl_wmm_tspec;
typedef wlan_ioctl_wmm_tspec  *pwlan_ioctl_wmm_tspec;
typedef struct _wlan_ioctl_wmm_ack_policy  wlan_ioctl_wmm_ack_policy;
typedef wlan_ioctl_wmm_ack_policy  *pwlan_ioctl_wmm_ack_policy;

typedef struct _WMM_QoS_INFO {
#ifdef BIG_ENDIAN
	u8	Reserved:4;
	u8	ParaSetCount:4;
#else
	u8	ParaSetCount:4;
	u8	Reserved:4;
#endif
} __ATTRIB_PACK__ WMM_QoS_INFO, *pWMM_QoS_INFO;

typedef struct _WMM_ACI_AIFSN {
#ifdef BIG_ENDIAN
	u8	Reserved:1;
	u8	ACI:2;
	u8	ACM:1;
	u8	AIFSN:4;
#else
	u8	AIFSN:4;
	u8	ACM:1;
	u8	ACI:2;
	u8	Reserved:1;
#endif
} __ATTRIB_PACK__ WMM_ACI_AIFSN, *pWMM_ACI_AIFSN;

typedef struct _WMM_ECW {
#ifdef BIG_ENDIAN
	u8	ECW_Max:4;
	u8	ECW_Min:4;
#else
	u8	ECW_Min:4;
	u8	ECW_Max:4;
#endif
} __ATTRIB_PACK__ WMM_ECW, *pWMM_ECW;

typedef struct _WMM_AC_PARAS {
	WMM_ACI_AIFSN	ACI_AIFSN;
	WMM_ECW		ECW;
	u16		Txop_Limit;
} __ATTRIB_PACK__ WMM_AC_PARAS, *pWMM_AC_PARAS;

typedef struct _WMM_PARA_IE {
	u8	ElementId; 		/* 221 */
	u8	Length;			/* 24 */
	u8	Oui[3];			/* 00:50:f2 (hex) */
	u8	OuiType;		/* 2 */
	u8	OuiSubtype;		/* 1 */
	u8	Version;		/* 1 */
	WMM_QoS_INFO QoSInfo;
	u8	Reserved;
	WMM_AC_PARAS AC_Paras_BE;	/* AC Parameters Record AC_BE */
	WMM_AC_PARAS AC_Paras_BK;	/* AC Parameters Record AC_BK */
	WMM_AC_PARAS AC_Paras_VI;	/* AC Parameters Record AC_VI */
	WMM_AC_PARAS AC_Paras_VO;	/* AC Parameters Record AC_VO */
} __ATTRIB_PACK__ WMM_PARAMETER_IE;

typedef struct _HostCmd_DS_802_11_WMM_TSPEC {
	u8	UserPriority; 		/* 0 - TSPEC for AC_BE
					   3 - TSPEC for AC_BE
					   1 - TSPEC for AC_BK
					   2 - TSPEC for AC_BK
					   4 - TSPEC for AC_VI
					   5 - TSPEC for AC_VI
					   6 - TSPEC for AC_VO
					   7 - TSPEC for AC_VO */
	u8	Direction;		/* 3 - Bi-directional
					   2 - Reserved
					   1 - Downlink
					   0 - Uplink
					       Only bi-directional streams will be supported for WMM */
	u8	PSMethod;		/* 0 - WMM_PS_LEGACY
					   1 - WMM_PS_TRIGGERED */
	u8	FixedSizeMSDU;		/* 0 - Size of MSDU not fixed
					   1 - Size of MSDU is fixed */ 
	u16	MSDUSize;		/* Nominal MSDU size in bytes */
	u32	MeanDataRate;		/* Average data rate in bits/sec */
	u32	MinPhyRate;		/* Minimum PHY rate in bits/sec */
	u16	ExtraBandwidth;		/* Extra bandwidth factor to
					   account for retries */
} __ATTRIB_PACK__ HostCmd_DS_802_11_WMM_TSPEC,
	*pHostCmd_DS_802_11_WMM_TSPEC;

typedef struct _HostCmd_DS_802_11_WMM_ACK_POLICY {
	u16	Action; 		/* 0 - ACT_GET
					   1 - ACT_SET */
	u8	AC;	 		/* 0 - AC_BE
					   1 - AC_BK
					   2 - AC_VI
					   3 - AC_VO */
	u8	AckPolicy;		/* 0 - WMM_ACK_POLICY_IMM_ACK
					   1 - WMM_ACK_POLICY_NO_ACK */
} __ATTRIB_PACK__ HostCmd_DS_802_11_WMM_ACK_POLICY,
	*pHostCmd_DS_802_11_WMM_ACK_POLICY;

typedef struct _HostCmd_DS_802_11_WMM_GET_STATUS {
    /*
     * TLV based command, byte arrays used for max sizing purpose.
     *   There are no arguments sent in the command, the TLVs are  
     *   returned by the firmware.
     */
    u8 queueStatusTlv[sizeof(MrvlIEtypes_WmmQueueStatus_t) * MAX_AC_QUEUES];
    u8 wmmParamTlv[sizeof(WMM_PARAMETER_IE) + 2];
} __ATTRIB_PACK__ HostCmd_DS_802_11_WMM_GET_STATUS;

typedef struct _HostCmd_DS_802_11_WMM_PRIO_PKT_AVAIL {
	u16	PacketAC;
} __ATTRIB_PACK__ HostCmd_DS_802_11_WMM_PRIO_PKT_AVAIL,
	*pHostCmd_DS_802_11_WMM_PRIO_PKT_AVAIL;

#ifdef __KERNEL__

typedef struct _WMM_DESC
{
	u8		required;
	u8		enabled;
	u8		fw_notify;
	struct sk_buff	TxSkbQ[MAX_AC_QUEUES];
	u8		Para_IE[WMM_PARA_IE_LENGTH];
#ifdef WMM_UAPSD
	u8		qosinfo;
	u8		no_more_packet;
	BOOLEAN		gen_null_pkg;
#endif
} __ATTRIB_PACK__ WMM_DESC,
	*pWMM_DESC;
#endif

int wlan_wmm_enable_ioctl(wlan_private *priv, struct iwreq *wrq);
int wlan_do_wmm_para_ie_ioctl(wlan_private *priv, struct ifreq *req);
int wlan_do_wmm_tspec_ioctl(wlan_private *priv, struct ifreq *req);
int wlan_do_wmm_ack_policy_ioctl(wlan_private *priv, struct ifreq *req);
int wlan_cmd_802_11_wmm_tspec(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 cmdno, void *InfoBuf);
int wlan_cmd_802_11_wmm_ack_policy(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 action, void *InfoBuf);
int wlan_cmd_802_11_wmm_get_status(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 action, void *InfoBuf);
int wlan_cmd_802_11_wmm_prio_pkt_avail(wlan_private *priv,
			HostCmd_DS_COMMAND *cmd, u16 action, void *InfoBuf);
int sendWMMStatusChangeCmd(wlan_private *priv);
void wmm_cleanup_queues(wlan_private *priv);
void wmm_process_tx(wlan_private * priv);
#ifdef __KERNEL__
void wmm_map_and_add_skb(wlan_private *priv, struct sk_buff *);
#endif
int wmm_lists_empty(wlan_private *priv);

extern void wlan_cmdresp_get_wmm_status(wlan_private *priv, 
                                        u8* pCmdResp, u32 respSize);

#endif /* __WLAN_WMM_H */
