/******************* (c) Marvell Semiconductor, Inc., 2001 ********************
 *
 * 	File:		dsdef.h
 *	Purpose:
 *          This file contains definitions and data structures that are specific
 *          to Marvell 802.11 NIC. 
 *
 *****************************************************************************/

#ifndef _DSDEF_H_
#define _DSDEF_H_

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__ ((packed))
#endif

//=============================================================================
//          PUBLIC TYPE DEFINITIONS
//=============================================================================

//
//	Ethernet Frame Sizes
//
//	Currently, driver allocates 1536 bytes of host memory to keep
//	Tx/Rx packets.
//
#ifdef WPA
extern u32 DSFreqList[15];
#endif

#define MRVDRV_SNAP_HEADER_LEN			 8
#define MRVDRV_ETH_ADDR_LEN                      6
#define MRVDRV_ETH_HEADER_SIZE                   14
#define MRVDRV_MINIMUM_ETH_PACKET_SIZE           60
#define MRVDRV_UPPER_PROTOCOL_RESERVED_LENGTH    0x10
#define MRVDRV_MAX_DMA_SIZE                      0x00001000

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
#define MRVDRV_NUM_OF_SQPPA		0x00000008
#define MRVDRV_NUM_OF_SQDPA		0x00000008
#define MRVDRV_NUM_OF_RXPD_REF		0x00000008
#define MRVDRV_NUM_OF_RXPD		0x00000020
#define MRVDRV_NUM_OF_SQTxPD		0x00000008
#define MRVDRV_NUM_OF_TxPD		0x00000020

#define MRVDRV_MAX_MULTICAST_LIST_SIZE	32

#define MRVDRV_NUM_PS_CMD_BUFFER	8
#define MRVDRV_NUM_OF_CMD_BUFFER        16
#define MRVDRV_MAX_CHANNEL_SIZE		14
#define MRVDRV_SIZE_OF_CMD_BUFFER       (3 * 1024) 

#define MRVDRV_MAX_BSSID_LIST		64
#define MRVDRV_MAX_PACKETS_PER_PPA	1 

#define MRVDRV_NUM_OF_TX_CTRL_NODE	MRVDRV_NUM_OF_TxPD

#define MRVDRV_DEFAULT_TIMER_INTERVAL	10000   
#define MRVDRV_TIMER_10S		10000
#define MRVDRV_TIMER_5S			5000
#define MRVDRV_ASSOCIATION_TIME_OUT	255

#define MRVDRV_DEFAULT_TX_PKT_TIME_OUT  100
//
//          Misc constants
//
//          This section defines 802.11 specific contants
//
#define MRVDRV_DEFAULT_MACHINE_NAME_SIZE 	32
#define MRVDRV_MAX_SSID_LENGTH			32
#define MRVDRV_MAX_BSS_DESCRIPTS		16
#define MRVDRV_MAX_REGION_CODE			6
#ifdef PS_REQUIRED
#define MRVDRV_MIN_MULTIPLE_DTIM		1
#define MRVDRV_MAX_MULTIPLE_DTIM		5
#define MRVDRV_DEFAULT_MULTIPLE_DTIM		1
#endif

#define MRVDRV_DEFAULT_LISTEN_INTERVAL		10

#define MRVDRV_MAX_TX_POWER		      	15

#ifdef PROGRESSIVE_SCAN
#define	MRVDRV_CHANNELS_PER_SCAN		4
#endif
#define	MRVDRV_CHANNELS_PER_ACTIVE_SCAN		14

//
//          Define TxPD, PPA, DPA page mask
//
//          SQ memory on the chip starts at address 0xC00000000
//
#define MRVDRV_SQ_BASE_ADDR_MASK         	0xC0000000

//
// TxPD Status
//
//	Station firmware use TxPD status field to report final Tx transmit
//	result, Bit masks are used to present combined situations.
//

#define MRVDRV_TxPD_STATUS_IDLE			0x00000000
#define MRVDRV_TxPD_STATUS_USED			0x00000001   
#define MRVDRV_TxPD_STATUS_OK			0x00000001
#define MRVDRV_TxPD_STATUS_OK_RETRY		0x00000002
#define MRVDRV_TxPD_STATUS_OK_MORE_RETRY		0x00000004
#define MRVDRV_TxPD_STATUS_MULTICAST_TX		0x00000008
#define MRVDRV_TxPD_STATUS_BROADCAST_TX		0x00000010
#define MRVDRV_TxPD_STATUS_FAILED_LINK_ERROR	0x00000020
#define MRVDRV_TxPD_STATUS_FAILED_EXCEED_LIMIT	0x00000040
#define MRVDRV_TxPD_STATUS_FAILED_AGING		0x00000080

//
// TxPD data rate
//
#define MRVDRV_TxPD_DATARATE_AUTO             0x00  // FW will determine the rate
#define MRVDRV_TxPD_DATARATE_1mbps            0x01  // 1Mbps
#define MRVDRV_TxPD_DATARATE_2mbps            0x02  // 2Mbps
#define MRVDRV_TxPD_DATARATE_5dot5mbps        0x04  // 5.5Mbps
#define MRVDRV_TxPD_DATARATE_11mbps           0x08  // 11Mbps
#define MRVDRV_TxPD_DATARATE_22mbps           0x10  // 22Mbps

//
// Tx control node status
//
#define MRVDRV_TX_CTRL_NODE_STATUS_IDLE      0x0000
#define MRVDRV_TX_CTRL_NODE_STATUS_USED      0x0001
#define MRVDRV_TX_CTRL_NODE_STATUS_PENDING   0x0002

//
// RxPD Control
//
#define MRVDRV_RXPD_CONTROL_DRIVER_OWNED	0x00
#define MRVDRV_RXPD_CONTROL_OS_OWNED		0x04
#define MRVDRV_RXPD_CONTROL_DMA_OWNED		0x80

//
// RxPD Status
//
#define MRVDRV_RXPD_STATUS_IDLE              0x0000
#define MRVDRV_RXPD_STATUS_OK                0x0001
#define MRVDRV_RXPD_STATUS_MULTICAST_RX      0x0002
#define MRVDRV_RXPD_STATUS_BROADCAST_RX      0x0004
#define MRVDRV_RXPD_STATUS_FRAGMENT_RX       0x0008

//
// RxPD Status - Received packet types
//
#define MRVDRV_RXPD_STATUS_802_3_RX		(0x00 << 8)
#define	MRVDRV_RXPD_STATUS_802_11_RX		(0x01 << 8)
#define	MRVDRV_RXPD_STATUS_DEBUG_RX		(0x02 << 8)
#define	MRVDRV_RXPD_STATUS_EVENTDATA_RX		(0x03 << 8)
#define	MRVDRV_RXPD_STATUS_TXDONE_RX		(0x04 << 8)
#define	MRVDRV_RXPD_STATUS_MAXTYPES_RX		6

#define MRVDRV_RX_PKT_TYPE_802_3			0
#define MRVDRV_RX_PKT_TYPE_EXTENDED			0x01
#define MRVDRV_RX_PKT_TYPE_EXTENDED_CONVERTED		0x02
#define MRVDRV_RX_PKT_TYPE_PASSTHROUGH			0x03  //refer to 2.1.1.3

//
// Command control node status
//
#define MRVDRV_CMD_CTRL_NODE_STATUS_IDLE         0x0000
#define MRVDRV_CMD_CTRL_NODE_STATUS_PENDING      0x0001
#define MRVDRV_CMD_CTRL_NODE_STATUS_PROCESSING   0x0002

//
// Link spped
//
#define MRVDRV_LINK_SPEED_0mbps          0
#define MRVDRV_LINK_SPEED_1mbps          10000   // in unit of 100bps
#define MRVDRV_LINK_SPEED_2mbps          20000
#define MRVDRV_LINK_SPEED_5dot5mbps      55000
#define MRVDRV_LINK_SPEED_10mbps         100000
#define MRVDRV_LINK_SPEED_11mbps         110000
#define MRVDRV_LINK_SPEED_22mbps         220000
#define MRVDRV_LINK_SPEED_100mbps        1000000

//
// RSSI-related defines
//
//	RSSI constants are used to implement 802.11 RSSI threshold 
//	indication. if the Rx packet signal got too weak for 5 consecutive
//	times, miniport driver (driver) will report this event to wrapper
//

#define MRVDRV_RSSI_TRIGGER_DEFAULT      	(-200)
#define MRVDRV_RSSI_INDICATION_THRESHOLD 	5
#define MRVDRV_RSSI_DEFAULT_NOISE_VALUE		0

#define MRVDRV_NF_DEFAULT_SCAN_VALUE		(-96)

// RTS/FRAG related defines
#define MRVDRV_RTS_MIN_VALUE		0
#define MRVDRV_RTS_MAX_VALUE		2347
#define MRVDRV_FRAG_MIN_VALUE		256
#define MRVDRV_FRAG_MAX_VALUE		2346

//
//Power Saving Mode related
//

#define PS_CAM_STATE		1
#define PS_HOST_CMD		2

//
// Define TxPD, PPA, DPA and RxPD
//

//          TxPD descriptor
//
//          Status     : Current Tx packet transmit status
//          PktPtr     : Physical address of the Tx Packet on host PC memory
//          PktLen		: Tx packet length
//          u8 DestMACAdrHi[2]	: First 2 byte of destination MAC address
//          DestMACAdrLow[4]	: Last 4 byte of destination MAC address
//          DataRate		: Driver uses this field to specify data
//				: rate for the Tx packet
//          NextTxPDPtr            : Address to next TxPD (Used by the driver)
//

#ifdef	FIRMWARE1

typedef struct _TxPD
{
	u32	Status;
	u32	PktPtr;
	u16	PktLen;
	u8	DestMACAdrHi[2];
	u8	DestMACAdrLow[4];
	u8	DataRate;
	u8	NotUsed[3];
	struct _TxPD *NextTxPDPtr;
	u32	Reserved[2];
} __attribute__ ((packed)) TxPD, *PTxPD;	// size = 32 bytes

#else
/* FIRMWARE2 */

typedef struct _TxPD
{
	u32	TxStatus;
	u32	TxControl;
	u32	TxPacketLocation;
	u16	TxPacketLength;
	u8	TxDestAddrHigh[2];
	u8	TxDestAddrLow[4];
#ifdef WMM
	u8	Priority;
	u8	Reserved[3];
#endif /* WMM */
//	struct _TxPD *NextTxPDPtr;
} __attribute__ ((packed)) TxPD, *PTxPD;	// size = 32 bytes

#endif

//	RxPD descirptor
//                      
//	Control     : Ownership indication
//	RSSI        : FW uses this field to report last Rx packet RSSI
//	Status      : Rx packet type
//	PktLen      : FW uses this field to report last Rx packet length 
//	SQBody      : Signal quality for packet body
//	Rate        : In FW. 
//	PktPtr      : Driver uses this field to report host memory buffer size
//	NextRxPDPtr : Physical address of the next RxPD
//

typedef struct _RxPD {
	u16	Status __ATTRIB_PACK__;
	u8	SNR __ATTRIB_PACK__;
	u8	RxControl __ATTRIB_PACK__;
	u16	PktLen __ATTRIB_PACK__;
	u8	NF __ATTRIB_PACK__;	/* Noise Floor */
	u8	RxRate __ATTRIB_PACK__;
	u32	PktPtr __ATTRIB_PACK__;
	u32	NextRxPDPtr __ATTRIB_PACK__;
} __attribute__ ((packed)) RxPD, *PRxPD;

//          PPA array keeps pending Tx packet TxPD reference
typedef struct _PPA
{
	u32 TxPDArr[MRVDRV_SIZE_OF_PPA] __ATTRIB_PACK__;
} __attribute__ ((packed)) PPA, *PPPA;

//          DPA array keeps finished Tx packet TxPD reference
typedef struct _DPA
{
	u32 TxPDArr[MRVDRV_SIZE_OF_DPA] __ATTRIB_PACK__;
} __attribute__ ((packed)) DPA, *PDPA;


#define EXTRA_LEN	36 /* This is for firmware specific length */

#define MRVDRV_MAXIMUM_ETH_PACKET_SIZE	1514  

#define MRVDRV_ETH_TX_PACKET_BUFFER_SIZE \
			(MRVDRV_MAXIMUM_ETH_PACKET_SIZE + sizeof(TxPD) + EXTRA_LEN)
#define MRVDRV_ETH_RX_PACKET_BUFFER_SIZE \
			(MRVDRV_MAXIMUM_ETH_PACKET_SIZE + sizeof(RxPD) + MRVDRV_SNAP_HEADER_LEN + \
										EXTRA_LEN)

//
//	Tx control node data structure 
//
//	Tx control node is used by the driver to keep ready to send Tx packet
//	information.
//
//	Status         :
//	NPTxPacket     : Packet reference
//	LocalTxPD       : Local TxPD reference
//	SQTxPD          : On-chip SQ memory TxPD reference
//	BufVirtualAddr : Tx staging buffer logical address
//	BufPhyAddr     : Tx staging buffer physical address
//	NextNode       : link to next Tx control node
//

typedef struct _TxCtrlNode
{
	u32		Status __ATTRIB_PACK__;
	struct sk_buff	*NPTxPacket __ATTRIB_PACK__;
	PTxPD		LocalTxPD __ATTRIB_PACK__;
	PTxPD		SQTxPD __ATTRIB_PACK__;
	u8		*BufVirtualAddr __ATTRIB_PACK__;
	long long	BufPhyAddr __ATTRIB_PACK__;
	struct _TxCtrlNode	*NextNode __ATTRIB_PACK__;
} __attribute__ ((packed)) TxCtrlNode, *PTxCtrlNode;

//
//	Command control node data structure, command data structures are
//	defined in hostcmd.h
//
//	Next              : Link to nect command control node
//	Status            : Current command execution status
//	PendingOID        : Set or query OID passed
//	ExpectedRetCode   : Result code for the current command
//	PendingInfo       : Init, Reset, Set OID, Get OID, Host command, etc.
//	INTOption         : USE_INT or NO_INT
//	BytesWritten      : For async OID processing
//	BytesRead         : For async OID processing
//	BytesNeeded       : For async OID processing
//	InformationBuffer : For async OID processing
//	BufVirtualAddr    : Command buffer logical address
//	BufPhyAddr        : Command buffer physical address
//

typedef struct _CmdCtrlNode
{
	struct list_head list __ATTRIB_PACK__;
	u32	Status __ATTRIB_PACK__;
	u32	PendingOID __ATTRIB_PACK__;
	u16	ExpectedRetCode __ATTRIB_PACK__;
	u16	PendingInfo __ATTRIB_PACK__;	// Init, Reset, Set OID,
						// Get OID, Host command, etc.
	u16 	INTOption __ATTRIB_PACK__;

	void 	*InformationBuffer __ATTRIB_PACK__; // For async OID processing
	u8 	*BufVirtualAddr __ATTRIB_PACK__;
	long long	BufPhyAddr __ATTRIB_PACK__;

	int	retcode;	/* Return status of the command */
    
	u16			CmdWaitQWoken;
	wait_queue_head_t	cmdwait_q __ATTRIB_ALIGN__;	// This has to be the last entry
} __attribute__ ((packed)) CmdCtrlNode, *PCmdCtrlNode;

#define	CMD_F_STDCMD		(1 << 0)
#define	CMD_F_MANFCMD		(1 << 1)

//
//	TxPD Reference Node
//
//	TxPDVirtualAddr : Local TxPD address
//	TxCtrlNodeRef  : Associated Tx control node address
//	TxPDSQOffset    : SQ TxPD offset
//	TxPDInUse       : If the TxPD is in use
//
typedef struct _TxPDRefNode
{
	TxPD *TxPDVirtualAddr __ATTRIB_PACK__;
	TxCtrlNode *TxCtrlNodeRef __ATTRIB_PACK__;
	u32 TxPDSQOffset __ATTRIB_PACK__;
	BOOLEAN TxPDInUse __ATTRIB_PACK__;
} __attribute__ ((packed)) TxPDRefNode, *PTxPDRefNode;

//
//	Ethernet Frame Structure
//

//	Ethernet MAC address
typedef struct _ETH_ADDRESS_STRUC
{
	u8 EthNodeAddr[MRVDRV_ETH_ADDR_LEN] __ATTRIB_PACK__;
} __attribute__ ((packed)) ETH_ADDR_STRUC, *PETH_ADDR_STRUC;

//	Ethernet frame header
typedef struct _ETH_HEADER_STRUC
{
	u8	DestAddr[MRVDRV_ETH_ADDR_LEN] __ATTRIB_PACK__;
	u8	SrcAddr[MRVDRV_ETH_ADDR_LEN] __ATTRIB_PACK__;
	u16	TTypeLength __ATTRIB_PACK__;
} __attribute__ ((packed)) ETH_HEADER_STRUC, *PETH_HEADER_STRUC;

typedef struct _REGINFOTAB {
	u8	*ObjNSName __ATTRIB_PACK__;   // Object name (UNICOODE)
	char	*ObjName __ATTRIB_PACK__;           // Object name (ASCII)
	u32	Type __ATTRIB_PACK__;	// StringData (1), IntegerData (0)
	u32	Offset __ATTRIB_PACK__;	// Offset to adapter object field
	u32	MaxSize __ATTRIB_PACK__;            // Maximum Size (in bytes) 
} __attribute__ ((packed)) REGINFOTAB;

//	WEP list macros & data structures
#define MRVL_KEY_BUFFER_SIZE_IN_BYTE  16

typedef struct _MRVL_WEP_KEY
{	// Based on WLAN_802_11_WEP, we extend the WEP buffer length to 128 bits
	u32 Length __ATTRIB_PACK__;
	u32 KeyIndex __ATTRIB_PACK__;
	u32 KeyLength __ATTRIB_PACK__;
	u8 KeyMaterial[MRVL_KEY_BUFFER_SIZE_IN_BYTE] __ATTRIB_PACK__;
} __attribute__ ((packed)) MRVL_WEP_KEY, *PMRVL_WEP_KEY;

typedef struct _MRVL_WEP_INFO
{
	u32 LastKeyIndex __ATTRIB_PACK__;
	WLAN_802_11_SSID SSID __ATTRIB_PACK__;
	MRVL_WEP_KEY KEY[4] __ATTRIB_PACK__;
} __attribute__ ((packed)) MRVL_WEP_INFO, *PMRVL_WEP_INFO;

#define MRVDRV_LINK_SPEED_1mbps		10000 // in unit of 100b


#ifdef FWVERSION3
#define MRVDRV_SCAN_LIST_VAR_IE_SPACE  	256 // to resolve CISCO AP extension
#endif

#define MRVDRV_ASSOCIATE_INFO_BUFFER_SIZE 500
        //support 5 key sets
#define MRVL_NUM_WPA_KEYSET_SUPPORTED   5

	//support 4 keys per key set
#define MRVL_NUM_WPA_KEY_PER_SET        4
#define MRVL_MAX_WPA_KEY_LENGTH 	32
#define MRVL_MAX_KEY_WPA_KEY_LENGTH     32

#define WPA_AES_KEY_LEN 		16
#define WPA_TKIP_KEY_LEN 		32

typedef ULONGLONG       WLAN_802_11_KEY_RSC;

typedef struct _WLAN_802_11_KEY
{
	u32	Length;
	u32   KeyIndex;
	u32   KeyLength;
	WLAN_802_11_MAC_ADDRESS BSSID;
	WLAN_802_11_KEY_RSC KeyRSC;
	u8   KeyMaterial[MRVL_MAX_KEY_WPA_KEY_LENGTH];
}__attribute__((packed)) WLAN_802_11_KEY, *PWLAN_802_11_KEY;

#ifdef WPA 
typedef struct _MRVL_WPA_KEY {
	u32   KeyIndex;
	u32   KeyLength;
	u32	KeyRSC;
	u8   KeyMaterial[MRVL_MAX_KEY_WPA_KEY_LENGTH];
} MRVL_WPA_KEY, *PMRVL_WPA_KEY;


typedef struct _MRVL_WPA_KEY_SET {
	WLAN_802_11_MAC_ADDRESS BSSID;
	MRVL_WPA_KEY            Key[MRVL_NUM_WPA_KEY_PER_SET];
}MRVL_WPA_KEY_SET, *PMRVL_WPA_KEY_SET;

typedef struct _MRVL_WLAN_WPA_KEY {
	u8   EncryptionKey[16];
	u8   MICKey1[8];
	u8   MICKey2[8];
}MRVL_WLAN_WPA_KEY, *PMRVL_WLAN_WPA_KEY;

typedef struct _IE_WPA {
	u8	Elementid;
	u8	Len;
	u8	oui[4];
	u16	version;
	//....
}IE_WPA,*PIE_WPA;

//Fixed IE size is 8 bytes time stamp + 2 bytes beacon interval + 2 bytes cap
#define MRVL_FIXED_IE_SIZE      12

#endif
#endif  /* _DSDEF_H_ */
