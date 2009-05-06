/*****************************************************************************
 *
 *  	File:	wlan_defs.h
 *
 *****************************************************************************/

#ifndef _WLAN_DEFS_H_
#define _WLAN_DEFS_H_

#include	"os_defs.h"

/* Bit definition */

#define DW_BIT_0	0x00000001
#define DW_BIT_1	0x00000002
#define DW_BIT_2	0x00000004
#define DW_BIT_3	0x00000008
#define DW_BIT_4	0x00000010
#define DW_BIT_5	0x00000020
#define DW_BIT_6	0x00000040
#define DW_BIT_7	0x00000080
#define DW_BIT_8	0x00000100
#define DW_BIT_9	0x00000200
#define DW_BIT_10	0x00000400
#define DW_BIT_11       0x00000800
#define DW_BIT_12       0x00001000
#define DW_BIT_13       0x00002000
#define DW_BIT_14       0x00004000
#define DW_BIT_15       0x00008000
#define DW_BIT_16       0x00010000
#define DW_BIT_17       0x00020000
#define DW_BIT_18       0x00040000
#define DW_BIT_19       0x00080000
#define DW_BIT_20       0x00100000
#define DW_BIT_21       0x00200000
#define DW_BIT_22       0x00400000
#define DW_BIT_23       0x00800000
#define DW_BIT_24       0x01000000
#define DW_BIT_25       0x02000000
#define DW_BIT_26       0x04000000
#define DW_BIT_27       0x08000000
#define DW_BIT_28       0x10000000
#define DW_BIT_29       0x20000000
#define DW_BIT_30	0x40000000
#define DW_BIT_31	0x80000000

#define W_BIT_0		0x0001
#define W_BIT_1		0x0002
#define W_BIT_2		0x0004
#define W_BIT_3		0x0008
#define W_BIT_4		0x0010
#define W_BIT_5		0x0020
#define W_BIT_6		0x0040
#define W_BIT_7		0x0080
#define W_BIT_8		0x0100
#define W_BIT_9		0x0200
#define W_BIT_10	0x0400
#define W_BIT_11	0x0800
#define W_BIT_12	0x1000
#define W_BIT_13	0x2000
#define W_BIT_14	0x4000
#define W_BIT_15	0x8000

#define B_BIT_0		0x01
#define B_BIT_1		0x02
#define B_BIT_2		0x04
#define B_BIT_3		0x08
#define B_BIT_4		0x10
#define B_BIT_5		0x20
#define B_BIT_6		0x40
#define B_BIT_7		0x80

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

#ifndef	TRUE
#define TRUE			1
#endif
#ifndef	FALSE
#define	FALSE			0
#endif

#ifdef	DEBUG_LEVEL0
static inline void HexDump(char *prompt, u8 * buf, int len)
{
	int             i = 0;

	/* 
	 * Here dont change these 'printk's to 'PRINTK'. Using 'PRINTK' 
	 * macro will make the hexdump to add '<7>' before each byte 
	 */
	printk(KERN_DEBUG "%s: ", prompt);
	for (i = 1; i <= len; i++) {
		printk("%02x ", (u8) * buf);
		buf++;
	}

	printk("\n");
}
#endif

#define EXTERN			extern

#define ASSERT(cond)

#ifndef MIN
#define MIN(a,b)		((a) < (b) ? (a) : (b))
#endif

//
//          Buffer Constants
//
//	The size of SQ memory PPA, DPA are 8 DWORDs, that keep the physical
//	addresses of TxPD buffers. Station has only 8 TxPD available, Whereas
//	driver has more local TxPDs. Each TxPD on the host memory is associated 
//	with a Tx control node. The driver maintains 8 RxPD descriptors for 
//	station firmware to store Rx packet information.
//
//	Current version of MAC has a 32x6 multicast address buffer.
//
//	802.11b can have up to  14 channels, the driver keeps the
//	BSSID(MAC address) of each APs or Ad hoc stations it has sensed.
//

#define MRVDRV_SIZE_OF_PPA		0x00000008
#define MRVDRV_SIZE_OF_DPA		0x00000008
#define MRVDRV_NUM_OF_TxPD		0x00000020
#define MRVDRV_MAX_MULTICAST_LIST_SIZE	32
#define MRVDRV_NUM_OF_CMD_BUFFER        16
#define MRVDRV_MAX_CHANNEL_SIZE		14
#define MRVDRV_SIZE_OF_CMD_BUFFER       (3 * 1024) 
#define MRVDRV_MAX_BSSID_LIST		64
#define MRVDRV_DEFAULT_TIMER_INTERVAL	10000   
#define MRVDRV_TIMER_10S		10000
#define MRVDRV_TIMER_5S			5000
#define MRVDRV_ASSOCIATION_TIME_OUT	255
#define MRVDRV_DEFAULT_TX_PKT_TIME_OUT  100
#define MRVDRV_SNAP_HEADER_LEN			 8
#define MRVDRV_ETH_ADDR_LEN                      6
#define MRVDRV_ETH_HEADER_SIZE                   14

#define	WLAN_UPLD_SIZE		2312
#define DEV_NAME_LEN		32

#ifndef	ETH_ALEN
#define ETH_ALEN	6
#endif

//
//          Misc constants
//
//          This section defines 802.11 specific contants
//

#define MRVDRV_MAX_SSID_LENGTH			32
#define MRVDRV_MAX_BSS_DESCRIPTS		16
#define MRVDRV_MAX_REGION_CODE			6

#ifdef PS_REQUIRED
#define MRVDRV_IGNORE_MULTIPLE_DTIM		0xfffe
#define MRVDRV_MIN_MULTIPLE_DTIM		1
#define MRVDRV_MAX_MULTIPLE_DTIM		5
#define MRVDRV_DEFAULT_MULTIPLE_DTIM		1
#endif /* PS_REQUIRED */

#define MRVDRV_DEFAULT_LISTEN_INTERVAL		10
#define MRVDRV_DEFAULT_LOCAL_LISTEN_INTERVAL		0
#define MRVDRV_MAX_TX_POWER		      	15

#ifdef PROGRESSIVE_SCAN
#define	MRVDRV_CHANNELS_PER_SCAN		4
#define	MRVDRV_MAX_CHANNELS_PER_SCAN		14
#endif /* PROGRESSIVE_SCAN */

#define	MRVDRV_CHANNELS_PER_ACTIVE_SCAN		14

//
// TxPD Status
//
//	Station firmware use TxPD status field to report final Tx transmit
//	result, Bit masks are used to present combined situations.
//

#define MRVDRV_TxPD_STATUS_USED			0x00000001   
#define MRVDRV_TxPD_POWER_MGMT_NULL_PACKET 0x01
#define MRVDRV_TxPD_POWER_MGMT_LAST_PACKET 0x08

//
// Tx control node status
//
#define MRVDRV_TX_CTRL_NODE_STATUS_IDLE      0x0000

//
// RxPD Status
//
#define MRVDRV_RXPD_STATUS_OK                0x0001
#define MRVDRV_RXPD_STATUS_MULTICAST_RX      0x0002

//
// RxPD Status - Received packet types
//
#define	MRVDRV_RXPD_STATUS_MAXTYPES_RX		6
#define MRVDRV_RX_PKT_TYPE_PASSTHROUGH			0x03  //refer to 2.1.1.3

//
// Link spped
//
#define MRVDRV_LINK_SPEED_1mbps          10000   // in unit of 100bps
#define MRVDRV_LINK_SPEED_11mbps         110000

//
// RSSI-related defines
//
//	RSSI constants are used to implement 802.11 RSSI threshold 
//	indication. if the Rx packet signal got too weak for 5 consecutive
//	times, miniport driver (driver) will report this event to wrapper
//

#define MRVDRV_RSSI_TRIGGER_DEFAULT      	(-200)
#define MRVDRV_RSSI_DEFAULT_NOISE_VALUE		0
#define MRVDRV_NF_DEFAULT_SCAN_VALUE		(-96)

/* RTS/FRAG related defines */
#define MRVDRV_RTS_MIN_VALUE		0
#define MRVDRV_RTS_MAX_VALUE		2347
#define MRVDRV_FRAG_MIN_VALUE		256
#define MRVDRV_FRAG_MAX_VALUE		2346

/* Fixed IE size is 8 bytes time stamp + 2 bytes beacon interval +
 * 2 bytes cap */
#define MRVL_FIXED_IE_SIZE      12

#define EXTRA_LEN	36 /* This is for firmware specific length */
#define MRVDRV_MAXIMUM_ETH_PACKET_SIZE	1514  

#define MRVDRV_ETH_TX_PACKET_BUFFER_SIZE \
	(MRVDRV_MAXIMUM_ETH_PACKET_SIZE + sizeof(TxPD) + EXTRA_LEN)

#define MRVDRV_ETH_RX_PACKET_BUFFER_SIZE \
	(MRVDRV_MAXIMUM_ETH_PACKET_SIZE + sizeof(RxPD) \
	 + MRVDRV_SNAP_HEADER_LEN + EXTRA_LEN)

/* Have the skb len just below 2K, with the alignments done by alloc_skb we would
 * eventually we will get 2k */
#define	MRVDRV_MAX_SKB_LEN	2000
#define	CMD_F_STDCMD		(1 << 0)

/* WEP list macros & data structures */
#define MRVL_KEY_BUFFER_SIZE_IN_BYTE  16

#ifdef FWVERSION3
#define MRVDRV_SCAN_LIST_VAR_IE_SPACE  	256 // to resolve CISCO AP extension
#define FW_IS_WPA_ENABLED(_adapter) \
		(_adapter->fwCapInfo & FW_CAPINFO_WPA)
	#define FW_CAPINFO_WPA  	(1 << 0)
#ifdef CAL_DATA
	#define FW_CAPINFO_EEPROM	(1 << 3)
#endif /* CAL_DATA */
#define WLAN_802_11_AI_REQFI_CAPABILITIES 	1
#define WLAN_802_11_AI_REQFI_LISTENINTERVAL 	2
#define WLAN_802_11_AI_REQFI_CURRENTAPADDRESS 	4

#define WLAN_802_11_AI_RESFI_CAPABILITIES 	1
#define WLAN_802_11_AI_RESFI_STATUSCODE 	2
#define WLAN_802_11_AI_RESFI_ASSOCIATIONID 	4
#endif /* FWVERSION3 */

#define MRVDRV_ASSOCIATE_INFO_BUFFER_SIZE 500
/* Support 4 keys per key set */
#define MRVL_NUM_WPA_KEY_PER_SET        4
#define MRVL_MAX_WPA_KEY_LENGTH 	32
#define MRVL_MAX_KEY_WPA_KEY_LENGTH     32

#define WPA_AES_KEY_LEN 		16
#define WPA_TKIP_KEY_LEN 		32

#define RF_ANTENNA_1		0x1
#define RF_ANTENNA_2		0x2
#define RF_ANTENNA_AUTO		0xFFFF

/* A few details needed for WEP (Wireless Equivalent Privacy) */
#define MAX_KEY_SIZE		13	// 104 bits
#define MIN_KEY_SIZE		5	// 40 bits RC4 - WEP
#define MAX_SIZE_ARRA		64

#define MAX_WLAN_TX_SIZE  3000  
#define MAX_WLAN_RX_SIZE  3000  
#define MAX_WLAN_RSP_SIZE  256

#ifdef MULTI_BANDS
#define BS_802_11B	0
#define BS_802_11G	1
#define BS_802_11A	2
/*
** Setup the number of rates pased in the driver/firmware API.
*/
#define A_SUPPORTED_RATES		14 /* TODO: Check max support rates */

#define FW_MULTI_BANDS_SUPPORT	(DW_BIT_8 | DW_BIT_9 | DW_BIT_10)
#define	BAND_B			(0x01)
#define	BAND_G			(0x02) 
#define BAND_A			(0x04)
#define ALL_802_11_BANDS	(BAND_B | BAND_G | BAND_A)

#define	IS_SUPPORT_MULTI_BANDS(_adapter) \
				(_adapter->fwCapInfo & FW_MULTI_BANDS_SUPPORT)
#define GET_FW_DEFAULT_BANDS(_adapter)	\
				((_adapter->fwCapInfo >> 8) & ALL_802_11_BANDS)

/*
** Setup the number of rates pased in the driver/firmware API.
*/
#define HOSTCMD_SUPPORTED_RATES A_SUPPORTED_RATES
#else
#ifdef G_RATE
#define HOSTCMD_SUPPORTED_RATES G_SUPPORTED_RATES
#else
#define HOSTCMD_SUPPORTED_RATES B_SUPPORTED_RATES
#endif
#endif /* MULTI_BANDS */

#ifdef WPA2
#define KEY_INFO_ENABLED	0x01
#endif /* WPA2 */

/* For Wireless Extensions */
#define		RF_GET			0
#define		RF_SET			1
#define		OID_MRVL_MFG_COMMAND	1

/* Wlan RFI Tx/Tx_Control Modes */
#define		TX_MODE_NORMAL		0	/* normal */
#define		TX_MODE_CONT		1	/* continuous modulated data */
#define		TX_MODE_CW		2	/* continuous carrier */

#define		TX_MODE_EXIT		0	/* exit test mode */
#define		TX_MODE_ENTER		1	/* enter test mode */
#define		MRVDRV_DEFAULT_TX_PKT_TIME_OUT           100

#define MAX_SNR			4
#define SNR_BEACON		0
#define SNR_RXPD		1
#define NF_BEACON		2
#define NF_RXPD			3

/*
 *            MACRO DEFINITIONS
 */
	
#define CAL_NF(NF)			((s32)(-(s32)(NF)))
#define CAL_RSSI(SNR, NF) 		((s32)((s32)(SNR) + CAL_NF(NF)))
#define SCAN_RSSI(RSSI)			(0x100 - ((u8)(RSSI)))

#define DEFAULT_BCN_AVG_FACTOR		8
#define DEFAULT_DATA_AVG_FACTOR		8
#define AVG_SCALE			100
#define CAL_AVG_SNR_NF(AVG, SNRNF, N)         \
                        (((AVG) == 0) ? ((u16)(SNRNF) * AVG_SCALE) : \
                        ((((int)(AVG) * (N -1)) + ((u16)(SNRNF) * \
                        AVG_SCALE))  / N))

#define WLAN_STATUS_SUCCESS			(0)
#define WLAN_STATUS_FAILURE			(-1)
#define WLAN_STATUS_NOT_ACCEPTED                (-2)

#define B_SUPPORTED_RATES		8
#define G_SUPPORTED_RATES		14

#define	WLAN_SUPPORTED_RATES		14
#define WLAN_MAX_SSID_LENGTH		32

#ifdef	LED_GPIO_CTRL
#define	MAX_LEDS			16
#endif

/*
 *          S_SWAP : To swap 2 unsigned char
 */
#define S_SWAP(a,b) 	do { \
				unsigned char  t = SArr[a]; \
				SArr[a] = SArr[b]; SArr[b] = t; \
			} while(0)

/* SWAP: swap u8 */
#define SWAP_U8(a,b)	{u8 t; t=a; a=b; b=t;}
/* SWAP: swap u8 */
#define SWAP_U16(a,b)	{u16 t; t=a; a=b; b=t;}

#ifdef BIG_ENDIAN
#define wlan_le16_to_cpu(x) le16_to_cpu(x)
#define wlan_le32_to_cpu(x) le32_to_cpu(x)
#define wlan_cpu_to_le16(x) cpu_to_le16(x)
#define wlan_cpu_to_le32(x) cpu_to_le32(x)


#define endian_convert_pLocalTxPD(x);	{	\
		pLocalTxPD->TxStatus = wlan_cpu_to_le32(MRVDRV_TxPD_STATUS_USED);	\
		pLocalTxPD->TxPacketLength = wlan_cpu_to_le16((u16)skb->len);	\
		pLocalTxPD->TxPacketLocation = wlan_cpu_to_le32(sizeof(TxPD));	\
		pLocalTxPD->TxControl = wlan_cpu_to_le32(pLocalTxPD->TxControl);	\
	}

#define endian_convert_RxPD(x); { \
		pRxPD->Status = wlan_le16_to_cpu(pRxPD->Status); \
		pRxPD->PktLen = wlan_le16_to_cpu(pRxPD->PktLen); \
		pRxPD->PktPtr = wlan_le32_to_cpu(pRxPD->PktPtr); \
		pRxPD->NextRxPDPtr = wlan_le32_to_cpu(pRxPD->NextRxPDPtr); \
}

#define endian_convert_GET_LOG(x); { \
		Adapter->LogMsg.mcastrxframe = wlan_le32_to_cpu(Adapter->LogMsg.mcastrxframe); \
		Adapter->LogMsg.failed = wlan_le32_to_cpu(Adapter->LogMsg.failed); \
		Adapter->LogMsg.retry = wlan_le32_to_cpu(Adapter->LogMsg.retry); \
		Adapter->LogMsg.multiretry = wlan_le32_to_cpu(Adapter->LogMsg.multiretry); \
		Adapter->LogMsg.framedup = wlan_le32_to_cpu(Adapter->LogMsg.framedup); \
		Adapter->LogMsg.rtssuccess = wlan_le32_to_cpu(Adapter->LogMsg.rtssuccess); \
		Adapter->LogMsg.rtsfailure = wlan_le32_to_cpu(Adapter->LogMsg.rtsfailure); \
		Adapter->LogMsg.ackfailure = wlan_le32_to_cpu(Adapter->LogMsg.ackfailure); \
		Adapter->LogMsg.rxfrag = wlan_le32_to_cpu(Adapter->LogMsg.rxfrag); \
		Adapter->LogMsg.mcastrxframe = wlan_le32_to_cpu(Adapter->LogMsg.mcastrxframe); \
		Adapter->LogMsg.fcserror = wlan_le32_to_cpu(Adapter->LogMsg.fcserror); \
		Adapter->LogMsg.wepundecryptable = wlan_le32_to_cpu(Adapter->LogMsg.wepundecryptable); \
}

#else /* BIG_ENDIAN */
#define wlan_le16_to_cpu(x) x
#define wlan_le32_to_cpu(x) x
#define wlan_cpu_to_le16(x) x
#define wlan_cpu_to_le32(x) x
#endif /* BIG_ENDIAN */

typedef struct _wlan_private wlan_private;
typedef struct _wlan_adapter wlan_adapter;
typedef struct _HostCmd_DS_COMMAND HostCmd_DS_COMMAND;

extern u32 DSFreqList[15];
extern const char driver_version[];
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

#ifdef FW_WAKEUP_TIME
extern unsigned long wt_pwrup_sending, wt_pwrup_sent, wt_int, wt_wakeup_event, wt_awake_confirmrsp;
#endif

#ifdef PS_REQUIRED
#ifdef BULVERDE_SDIO
extern int ps_sleep_confirmed;
#endif
#endif 

#ifdef GSPI83xx
extern int g_bus_mode_reg;
extern int g_dummy_clk_ioport;
extern int g_dummy_clk_reg;
#endif /* GSPI83xx */

#ifdef OMAP1510_TIMER_DEBUG
extern unsigned long times[20][10];
extern int tm_ind[10];
#endif /* OMAP1510_TIMER_DEBUG */

#ifdef	MEM_CHECK
extern u32	kmalloc_count, kfree_count;
extern u32	rxskb_alloc, rxskb_free;
extern u32	txskb_alloc, txskb_free;
#define	kmalloc	KMALLOC
#define	kfree	KFREE
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
#endif	/* MEM_CHECK */


#ifdef MANF_CMD_SUPPORT
typedef struct PkHeader //For the mfg command
{
	u16	cmd;
	u16	len;
	u16	seq;
	u16	result;
	u32	MfgCmd;
} PkHeader;

#define SIOCCFMFG SIOCDEVPRIVATE //For the manfacturing tool
#endif /* MANF_CMD_SUPPORT */

/* Definitions for SNR and NF */
typedef enum _SNRNF_TYPE {
	TYPE_BEACON = 0,
	TYPE_RXPD,
	MAX_TYPE_B
} SNRNF_TYPE;

typedef enum _SNRNF_DATA {
	TYPE_NOAVG = 0,
	TYPE_AVG,
	MAX_TYPE_AVG
} SNRNF_DATA;

typedef enum _WLAN_802_11_AUTH_ALG
{
	AUTH_ALG_OPEN_SYSTEM = 1,
	AUTH_ALG_SHARED_KEY = 2,
	AUTH_ALG_NETWORK_EAP = 8,
} WLAN_802_11_AUTH_ALG;

typedef enum _WLAN_802_1X_AUTH_ALG
{
	WLAN_1X_AUTH_ALG_NONE = 1,
	WLAN_1X_AUTH_ALG_LEAP = 2,
	WLAN_1X_AUTH_ALG_TLS = 4,
	WLAN_1X_AUTH_ALG_TTLS = 8,
	WLAN_1X_AUTH_ALG_MD5 = 16,
} WLAN_802_1X_AUTH_ALG;

typedef enum _WLAN_802_11_ENCRYPTION_MODE
{
	CIPHER_NONE,
	CIPHER_WEP40,
	CIPHER_TKIP,
	CIPHER_CCMP,
	CIPHER_WEP104,
} WLAN_802_11_ENCRYPTION_MODE;

#ifdef PS_REQUIRED	
typedef enum _WLAN_802_11_POWER_MODE
{
	Wlan802_11PowerModeCAM,
	Wlan802_11PowerModeMAX_PSP,
	Wlan802_11PowerModeFast_PSP,
	Wlan802_11PowerModeMax		// not a real mode, defined as 
					//an upper bound
} WLAN_802_11_POWER_MODE, *PWLAN_802_11_POWER_MODE;

typedef enum {
	PS_STATE_FULL_POWER,
	PS_STATE_AWAKE,
#ifdef PS_PRESLEEP
	PS_STATE_PRE_SLEEP,
#endif
	PS_STATE_SLEEP
} PS_STATE;
#endif /* PS_REQUIRED */

/* Command or Data download status */
typedef enum {
	DNLD_RES_RECEIVED,
	DNLD_DATA_SENT,
	DNLD_CMD_SENT
} DNLD_STATE;

/*
 *  Defines the state of the LAN media
 */
typedef enum _WLAN_MEDIA_STATE {
	WlanMediaStateConnected,
	WlanMediaStateDisconnected
} WLAN_MEDIA_STATE, *PWLAN_MEDIA_STATE;

typedef enum _WLAN_802_11_PRIVACY_FILTER {
	Wlan802_11PrivFilterAcceptAll,
	Wlan802_11PrivFilter8021xWEP
} WLAN_802_11_PRIVACY_FILTER, *PWLAN_802_11_PRIVACY_FILTER;

typedef enum {
	MVMS_DAT = 0,
	MVMS_CMD = 1,
	MVMS_TXDONE = 2,
	MVMS_EVENT
} mv_ms_type;

typedef u8 WLAN_802_11_RATES[WLAN_SUPPORTED_RATES] __ATTRIB_PACK__;    // Set of 8 data rates
typedef u8 WLAN_802_11_MAC_ADDRESS[ETH_ALEN] __ATTRIB_PACK__;

/*
 * Hardware status codes (OID_GEN_HARDWARE_STATUS).
 */
typedef enum _WLAN_HARDWARE_STATUS {
	WlanHardwareStatusReady,
	WlanHardwareStatusInitializing,
	WlanHardwareStatusReset,
	WlanHardwareStatusClosing,
	WlanHardwareStatusNotReady
} WLAN_HARDWARE_STATUS, *PWLAN_HARDWARE_STATUS;

typedef enum _WLAN_802_11_NETWORK_TYPE {
	Wlan802_11FH,
	Wlan802_11DS,
	Wlan802_11NetworkTypeMax	// not a real type, 
					//defined as an upper bound
} WLAN_802_11_NETWORK_TYPE, *PWLAN_802_11_NETWORK_TYPE
  __ATTRIB_PACK__;

typedef enum _WLAN_802_11_NETWORK_INFRASTRUCTURE {
	Wlan802_11IBSS,
	Wlan802_11Infrastructure,
	Wlan802_11AutoUnknown,
	Wlan802_11InfrastructureMax	// Not a real value, 
					// defined as upper bound
} WLAN_802_11_NETWORK_INFRASTRUCTURE, *PWLAN_802_11_NETWORK_INFRASTRUCTURE;

typedef enum _WLAN_802_11_AUTHENTICATION_MODE {
	Wlan802_11AuthModeOpen = 0x00,
	Wlan802_11AuthModeShared = 0x01,
	Wlan802_11AuthModeNetworkEAP = 0x80,
} WLAN_802_11_AUTHENTICATION_MODE, *PWLAN_802_11_AUTHENTICATION_MODE;

typedef enum _WLAN_802_11_WEP_STATUS
{
	Wlan802_11WEPEnabled,
	Wlan802_11WEPDisabled,
	Wlan802_11WEPKeyAbsent,
	Wlan802_11WEPNotSupported
#ifdef WPA
	,Wlan802_11Encryption2Enabled,
	Wlan802_11Encryption2KeyAbsent,
	Wlan802_11Encryption3Enabled,
	Wlan802_11Encryption3KeyAbsent
#endif
} WLAN_802_11_WEP_STATUS, *PWLAN_802_11_WEP_STATUS
#ifdef WPA
,WLAN_802_11_ENCRYPTION_STATUS, *PWLAN_802_11_ENCRYPTION_STATUS;
#else
;
#endif

typedef enum {
	DesiredBssType_i = 0,
	OpRateSet_i,
	BcnPeriod_i,
	DtimPeriod_i,
	AssocRspTimeOut_i,
	RtsThresh_i,
	ShortRetryLim_i,
	LongRetryLim_i,
	FragThresh_i,
	Dot11D_i,
	Dot11HTPC_i,
	ManufId_i,
	ProdId_i,
	ManufOui_i,
	ManufName_i,
	ManufProdName_i,
	ManufProdVer_i
} SNMP_MIB_INDEX_e;

#ifdef WPA2
typedef enum {
	KEY_TYPE_ID_WEP = 0,
	KEY_TYPE_ID_TKIP,
	KEY_TYPE_ID_AES
} KEY_TYPE_ID;

typedef enum {
	KEY_INFO_WEP_DEFAULT_KEY = 0x01
} KEY_INFO_WEP;

typedef enum {
	KEY_INFO_TKIP_MCAST = 0x01,
	KEY_INFO_TKIP_UNICAST = 0x02,
	KEY_INFO_TKIP_ENABLED = 0x04
} KEY_INFO_TKIP;

typedef enum {
	KEY_INFO_AES_MCAST = 0x01,
	KEY_INFO_AES_UNICAST = 0x02,
	KEY_INFO_AES_ENABLED = 0x04
} KEY_INFO_AES;

#endif /* WPA2 */

typedef enum {
	SNMP_MIB_VALUE_INFRA = 1,
	SNMP_MIB_VALUE_ADHOC
} SNMP_MIB_VALUE_e;

/* Linux Dynamic Power Management,
 *  If the user has enabled the deep sleep, user has the highest privilege.
 *  The Kernel PM takes the next priority
 */
enum {
	DEEP_SLEEP_DISABLED,
	DEEP_SLEEP_ENABLED,
	DEEP_SLEEP_OS_ENABLED,
	DEEP_SLEEP_USER_ENABLED
};
enum {
	PM_DISABLED,
	PM_ENABLED
};
#endif /* _WLAN_DEFS_H_ */
