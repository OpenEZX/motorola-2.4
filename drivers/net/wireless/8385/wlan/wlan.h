/*
	File	: wlan.h
*/

#ifndef	_WLAN_H_
#define	_WLAN_H_

#ifndef MIN
#define MIN(a,b)		((a) < (b) ? (a) : (b))
#endif

#define	WLAN_UPLD_SIZE		2312

#define DEV_NAME_LEN		32

#define MAX_SNR			4
#define SNR_BEACON		0
#define SNR_RXPD		1
#define NF_BEACON		2
#define NF_RXPD			3

#ifdef	DEBUG_LEVEL1
#ifndef DEBUG_LEVEL0
#define	DEBUG_LEVEL0
#endif
#endif

#ifdef	DEBUG_LEVEL2
#ifndef DEBUG_LEVEL0
#define	DEBUG_LEVEL0
#endif
#ifndef DEBUG_LEVEL1
#define	DEBUG_LEVEL1
#endif
#endif

#ifdef	DEBUG_LEVEL0

#define	HEXDUMP1(x, y, z)	HexDump(x, y, z)
#define HEXDUMP(x, y, z)	HexDump(x, y, z)

#ifdef CONFIG_X86
#define	PRINTK(x...)		printk(KERN_DEBUG x)
#else
#define	PRINTK(x...)		printk(x)
#endif 

#else

#define	PRINTK(x...)		
#define HEXDUMP(x,y,z)
#define HEXDUMP1(x,y,z)

#endif  /* DEBUG_LEVEL0 */

#ifdef	DEBUG_LEVEL1

#ifdef CONFIG_X86
#define	PRINTK1(x...)		printk(KERN_DEBUG x)
#else
#define	PRINTK1(x...)		printk(x)
#endif 

#else  

#define	PRINTK1(...)

#endif  /* DEBUG_LEVEL1 */

/* Enable this flag if u want to see a lot of PRINTK's..*/
#ifdef	DEBUG_LEVEL2

#ifdef CONFIG_X86
#define	PRINTK2(x...)		printk(KERN_DEBUG x)
#else
#define	PRINTK2(x...)		printk(x)
#endif 

#define	ENTER()			PRINTK2("Enter: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
#define	LEAVE()			PRINTK2("Exit: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
#define	ENTER1()		PRINTK2("Enter: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
#define	LEAVE1()		PRINTK2("Exit: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
#define	DEBUGINFO(x)		printk(KERN_INFO x);
#else 
#define	PRINTK2(...)
#define	ENTER()
#define	LEAVE()
#define	ENTER1()
#define	LEAVE1()
#define	DEBUGINFO(x)
#endif  /* DEBUG_LEVEL2 */

typedef struct _wlan_private wlan_private;
typedef struct _HostCmd_DS_COMMAND HostCmd_DS_COMMAND;

#include	"wlan_defs.h"

#ifdef WMM
#include	"wlan_wmm.h"
#endif /* WMM */

#include	"dsdef.h"
#include	"macro_defs.h"
#include	"hostcmd.h"
#include	"os_timers.h"
#include	"mrvdrv_dev.h"
#include	"wlan_thread.h"

extern const char driver_version[];

#define MV_MEM_EQUAL(src1,src2,len)  	(memcmp(src1,src2,len)==0 ? 1 : 0)

typedef enum {
	MVMS_DAT = 0,
	MVMS_CMD = 1,
	MVMS_TXDONE = 2,
	MVMS_EVENT
} mv_ms_type;

/**
 * Data structure for the Marvell WLAN device.
 * The comments in parenthesis are in relation to the user of this
 * structure.
 */
typedef struct _wlan_dev {
	char 			name[DEV_NAME_LEN];  /* device name (RO) */
	void			*card;  	/* card pointer (RO) */
	u32 			ioport;  	/* IO port (RO) */
	u32  			upld_rcv;  	/* Upload received (RW) */
	u32  			upld_typ;  	/* Upload type (RO) */
	u32 			upld_len;  	/* Upload length (RO) */
	void 			*dev_id;  	/* Ptr private device (WO) */
	struct net_device 	*netdev;  	/* Linux netdev (WO) */
	void			(*upldevent)(void *); /* Upload event
							 callback (WO) */
	u8  			upld_buf[WLAN_UPLD_SIZE];  /* Upload buffer (RO) */
	u8			dnld_sent;	/* Download sent: bit0 1/0=data_sent/data_tx_done, bit1 1/0=cmd_sent/cmd_tx_done, all other bits reserved 0 */
} __attribute__((packed)) wlan_dev_t, *pwlan_dev_t;

/* Data structure defined for the tx packet transmission */

#define MAX_WLAN_TX_SIZE  3000  
#define MAX_WLAN_RX_SIZE  3000  
#define MAX_WLAN_RSP_SIZE  256

typedef struct
{
  struct semaphore sem           __attribute__ ((packed)); /* semaphore */

  u8  tx_started                 __attribute__ ((packed)); /* tx started? */
  u32 tx_pktlen                  __attribute__ ((packed)); /* tx packet len  */
  u32 tx_pktcnt                  __attribute__ ((packed)); /* tx packet count */
  u8  tx_buf[MAX_WLAN_TX_SIZE]  __attribute__ ((packed));  /* tx buffer */

  u8  rx_received                __attribute__ ((packed)); /* rx received? */
  u8  rx_started                 __attribute__ ((packed)); /* rx started? */
  u32 rx_pktlen                  __attribute__ ((packed)); /* rx packet len */
  u32 rx_pktcnt                  __attribute__ ((packed)); /* rx packet count */
  u8  rx_buf[MAX_WLAN_RX_SIZE]  __attribute__ ((packed));  /* rx buffer */

  u8  rsp_received               __attribute__ ((packed)); /* rsp received? */
  u32 rsp_len                    __attribute__ ((packed)); /* rsp data len */
  u8  rsp_buf[MAX_WLAN_RSP_SIZE] __attribute__ ((packed)); /* rsp data buf */

} __attribute__ ((packed)) WLAN_OBJECT, *PWLAN_OBJECT;


/**
 * Private structure for the MV device.
 * The comments in parenthesis are in relation to the user of this
 * structure.
 */

struct _wlan_private {
	int			open;

	wlan_adapter		*adapter;
	WLAN_OBJECT     	iface; 
	wlan_dev_t		wlan_dev;

	struct net_device_stats	stats;
#ifdef linux
	struct iw_statistics	wstats;
	struct proc_dir_entry	*proc_entry;
	struct proc_dir_entry	*proc_dev;
#endif

	/* thread to service interrupts */
	wlan_thread 		intThread;

	/* thread to service mac events */
	wlan_thread 		eventThread;	
	
};

#ifdef linux
#include	<linux/etherdevice.h>
#endif
#include	"os_macros.h"

extern u32	DSFreqList[];
extern u16	RegionCodeToIndex[MRVDRV_MAX_REGION_CODE];

extern u8	WlanDataRates[WLAN_SUPPORTED_RATES];

#ifdef MULTI_BANDS
extern u8	SupportedRates[A_SUPPORTED_RATES];
extern u8	SupportedRates_B[B_SUPPORTED_RATES];
extern u8	SupportedRates_G[G_SUPPORTED_RATES];
extern u8	SupportedRates_A[A_SUPPORTED_RATES];
extern u8	AdhocRates_A[A_SUPPORTED_RATES];
#else
#ifdef G_RATE
extern u8 	SupportedRates[G_SUPPORTED_RATES];
#else
extern u8 	SupportedRates[B_SUPPORTED_RATES];
#endif
#endif

#ifdef ADHOC_GRATE
extern u8 	AdhocRates_G[G_SUPPORTED_RATES];
#else ///ADHOC_GRATE
extern u8 	AdhocRates_G[4];
#endif ///ADHOC_GRATE

extern u8 	AdhocRates_B[4];

extern wlan_private	*wlanpriv;

#ifdef	MEM_CHECK
extern u32	kmalloc_count, kfree_count;
extern u32	rxskb_alloc, rxskb_free;
extern u32	txskb_alloc, txskb_free;

static inline void *KMALLOC(size_t size, int flags)
{
	void	*b;

	if ((b = kmalloc(size, flags)))
		kmalloc_count++;

	return b;
}

static inline void KFREE(const void *obj)
{
	kfree(obj);
	kfree_count++;
}

static void inline DisplayMemCounters(void)
{
	PRINTK1(KERN_DEBUG "kmalloc's: %ld kfree's: %ld\n",
			kmalloc_count, kfree_count);
	PRINTK1(KERN_DEBUG "RxSkb's Allocated: %ld Freed: %ld\n",
			rxskb_alloc, rxskb_free);
	PRINTK1(KERN_DEBUG "TxSkb's Allocated: %ld Freed: %ld\n",
			txskb_alloc, txskb_free);
}

#define	kmalloc	KMALLOC
#define	kfree	KFREE
#endif	/* MEM_CHECK */

typedef struct PkHeader //For the mfg command
{
	u16	cmd;
	u16	len;
	u16	seq;
	u16	result;
	u32	MfgCmd;
} PkHeader;

#define SIOCCFMFG SIOCDEVPRIVATE //For the manfacturing tool


/* For Wireless Extensions */
#define		WLAN_BSSID_LEN 		6
#define		RF_GET			0
#define		RF_SET			1
#define		OPNOTSUPPORTED		-2
#define		OID_MRVL_MFG_COMMAND	1

#define 	CIPHER_TYPE_NONE	(0<<22)
#define 	CIPHER_TYPE_WEP_64	(1<<22)
#define 	CIPHER_TYPE_WEP_128	(2<<22)
#define 	CIPHER_TYPE_AES		(3<<22)

/* Wlan RFI Tx/Tx_Control Modes */
#define		TX_MODE_NORMAL		0	/* normal */
#define		TX_MODE_CONT		1	/* continuous modulated data */
#define		TX_MODE_CW		2	/* continuous carrier */

#define		TX_MODE_EXIT		0	/* exit test mode */
#define		TX_MODE_ENTER		1	/* enter test mode */

static inline void *PopFirstListNode(struct list_head *list)
{
	void	*ptr;

	if (!list_empty(list)) {
		ptr = list->next;
		list_del(ptr);
	} else {
		ptr = NULL;
	}

	return ptr;
}

#define MRVDRV_DEFAULT_TX_PKT_TIME_OUT           100

void MacEventDisconnected(wlan_private *priv);
int wlan_tx_packet(wlan_private *priv, struct sk_buff *skb);

#ifndef	MULTIPLERX_SUPPORT
int ProcessRxed_802_3_Packet(wlan_private *priv, RxPD *pRxPD);
#define	ProcessRxedPacket	ProcessRxed_802_3_Packet
#else
int ProcessRxedPacket(wlan_private *priv, RxPD *pRxPD);
#endif

void wlan_free_adapter(wlan_private *priv);

int wlan_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
int SetMacPacketFilter(wlan_private *priv);

int AllocateSingleTx(wlan_private *priv);
int CleanUpSingleTxBuffer(wlan_private *priv);
int wlan_Start_Tx_Packet(wlan_private *priv,int pktlen);

int MrvDrvSend (wlan_private *priv, struct sk_buff *skb);
int SendSinglePacket(wlan_private *priv, struct sk_buff *skb);
void Wep_encrypt(wlan_private *priv,  u8* Buf, u32 Len);
u16 GetExpectedRetCode(u16 cmdno);
int FreeCmdBuffer(wlan_private *priv);
void CleanUpCmdCtrlNode(CmdCtrlNode *pTempNode);
CmdCtrlNode *GetFreeCmdCtrlNode(wlan_private *priv);
void SetCmdCtrlNode(wlan_private *priv,
			CmdCtrlNode *pTempNode, WLAN_OID PendingOID,
			u16 PendingInfo, u16 INTOption, void *InfoBuf);

void QueueCmd(wlan_adapter *Adapter, CmdCtrlNode *CmdNode, BOOLEAN addtail);
int PrepareAndSendCommand(wlan_private *priv, u16 cmdno, u16 cmd_option,
				u16 intoption, WLAN_OID PendingOID,
				u16 PendingInfo,
				void *InformationBuffer);

int DownloadCommandToStation(wlan_private *priv, CmdCtrlNode *CmdNode);
int JoinAdhocNetwork(wlan_private *priv, WLAN_802_11_SSID *AdhocSSID, int i);
int StartAdhocNetwork(wlan_private *priv, WLAN_802_11_SSID *AdhocSSID);
int SendSpecificScan(wlan_private *priv, WLAN_802_11_SSID *RequestedSSID);

#ifdef PS_REQUIRED
int AllocatePSConfirmBuffer(wlan_private *priv);
void PSSleep(wlan_private *priv, u16 PSMode);
void PSConfirmSleep(wlan_private *priv, u16 PSMode);
void PSWakeup(wlan_private *priv);
int SendConfirmSleep(wlan_private * priv, u8 * CmdPtr, u16 size);
#endif

int ResetSingleTxDoneAck(wlan_private *priv);
int AllocateCmdBuffer(wlan_private *priv);
int ExecuteNextCommand(wlan_private *priv);
int wlan_process_event(wlan_private *priv);
void wlan_interrupt(struct net_device *);
#ifdef linux
static inline void wlan_sched_timeout(u32 millisec);
#endif
int wlan_scan_networks(wlan_private *priv, u16 pending_info);
int wlan_associate(wlan_private *priv, WLAN_802_11_SSID *ssid);

inline int get_common_rates(wlan_adapter *Adapter, u8 *dest, int size1, 
				u8 *card_rates, int size2);
u32 index_to_data_rate(u8 index);
u8 data_rate_to_index(u32 rate);

void HexDump(char *prompt, u8 *data, int len);
void get_version(wlan_adapter *adapter, char *version, int maxlen);

#ifdef	ADHOCAES
int SetupAdhocEncryptionKey(wlan_private *priv, PHostCmd_DS_802_11_PWK_KEY pEnc,
		int CmdOption, char *InformationBuffer);
#endif

void wlan_read_write_rfreg(wlan_private *priv);

/* The proc fs interface */
int wlan_proc_read(char *page, char **start, off_t offset,
		 int count, int *eof, void *data);
void wlan_proc_entry(wlan_private *priv, struct net_device *dev);
void wlan_proc_remove(char *name);
int wlan_set_mac_address(struct net_device *dev, void *addr);
	
#include "sbi.h"

#endif /* _wlan_h_ */
