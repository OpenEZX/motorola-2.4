/******************* (c) Marvell Semiconductor, Inc., 2001 ********************
 *
 *  $HEADER%
 *
 * 	File: macro_defs.h
 * 	
 *      Purpose: 
 *      	This file contains the macro and symbol definitions
 *
 *****************************************************************************/

#ifndef _MACRO_DEFS_H_
#define _MACRO_DEFS_H_

/*
 *	PUBLIC DEFINITIONS
 */

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

#define RF_ANTENNA_1		0x1
#define RF_ANTENNA_2		0x2
#define RF_ANTENNA_AUTO		0xFFFF

/* A few details needed for WEP (Wireless Equivalent Privacy) */
#define MAX_KEY_SIZE		13	// 104 bits
#define MIN_KEY_SIZE		5	// 40 bits RC4 - WEP

/*
 *            MACRO DEFINITIONS
 */

#ifdef FWVERSION3
	#define FW_CAPINFO_WPA  	(1 << 0)
#ifdef CAL_DATA
	#define FW_CAPINFO_EEPROM	(1 << 3)
#endif
#endif /* FWVERSION3 */

#ifdef PS_REQUIRED
	#define FW_CAPINFO_PS   	(1 << 1)
#endif

#ifdef FWVERSION3
	#define FW_IS_WPA_ENABLED(_adapter) \
		(_adapter->fwCapInfo & FW_CAPINFO_WPA)
#ifdef CAL_DATA
	#define FW_IS_EEPROM_EXIST(_adapter) \
		(_adapter->fwCapInfo & FW_CAPINFO_EEPROM)
#endif
#endif

#ifdef MULTI_BANDS
#define FW_MULTI_BANDS_SUPPORT	(DW_BIT_8 | DW_BIT_9 | DW_BIT_10)
#define	BAND_B			(0x01)
#define	BAND_G			(0x02) 
#define BAND_A			(0x04)
#define ALL_802_11_BANDS	(BAND_B | BAND_G | BAND_A)

#define	IS_SUPPORT_MULTI_BANDS(_adapter) \
				(_adapter->fwCapInfo & FW_MULTI_BANDS_SUPPORT)
#define GET_FW_DEFAULT_BANDS(_adapter)	\
				((_adapter->fwCapInfo >> 8) & ALL_802_11_BANDS)
	
#endif
	
#define CAL_NF(NF)			((s32)(-(s32)(NF)))
#define CAL_RSSI(SNR, NF) 		((s32)((s32)(SNR) + CAL_NF(NF)))
#define SCAN_RSSI(RSSI)			(0x100 - ((u8)(RSSI)))

#define AVG_SCALE			100
#define CAL_AVG_SNR_NF(AVG, SNRNF)	(((AVG) == 0) ? ((u16)(SNRNF) * AVG_SCALE) : \
					((((int)(AVG) * 7) + ((u16)(SNRNF) * \
						AVG_SCALE)) >> 3))

/*
 *          S_SWAP : To swap 2 unsigned char
 */
#define S_SWAP(a,b) 	do { \
				unsigned char  t = SArr[a]; \
				SArr[a] = SArr[b]; SArr[b] = t; \
			} while(0)

#ifdef BIG_ENDIAN

#define wlan_le16_to_cpu(x) le16_to_cpu(x)
#define wlan_le32_to_cpu(x) le32_to_cpu(x)
#define wlan_cpu_to_le16(x) cpu_to_le16(x)
#define wlan_cpu_to_le32(x) cpu_to_le32(x)


#define endian_convert_pTxNode(x);	{	\
		pTxNode->LocalTxPD->TxStatus = wlan_cpu_to_le32(MRVDRV_TxPD_STATUS_USED);	\
		pTxNode->LocalTxPD->TxPacketLength = wlan_cpu_to_le16((u16)skb->len);	\
		pTxNode->LocalTxPD->TxPacketLocation = wlan_cpu_to_le32(sizeof(TxPD));	\
		pTxNode->LocalTxPD->TxControl = wlan_cpu_to_le32(pTxNode->LocalTxPD->TxControl);	\
	}

#define endian_convert_RxPD(x); { \
		pRxPD->Status = wlan_le16_to_cpu(pRxPD->Status); \
		pRxPD->PktLen = wlan_le16_to_cpu(pRxPD->PktLen); \
		pRxPD->PktPtr = wlan_le32_to_cpu(pRxPD->PktPtr); \
		pRxPD->NextRxPDPtr = wlan_le32_to_cpu(pRxPD->NextRxPDPtr); \
}

#else
#define wlan_le16_to_cpu(x) x
#define wlan_le32_to_cpu(x) x
#define wlan_cpu_to_le16(x) x
#define wlan_cpu_to_le32(x) x
#endif


#endif /* _MACRODEF_H_ */

