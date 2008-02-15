/*
 *  File: sbi.h
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

/* This file provides the common definations for the Serial Bus Interface*/
#ifndef	_SBI_H_
#define	_SBI_H_

#define B_BIT_0		0x01
#define B_BIT_1		0x02
#define B_BIT_2		0x04
#define B_BIT_3		0x08
#define B_BIT_4		0x10
#define B_BIT_5		0x20
#define B_BIT_6		0x40
#define B_BIT_7		0x80

#define HIS_RxUpLdRdy			B_BIT_0
#define HIS_TxDnLdRdy			B_BIT_1
#define HIS_CmdDnLdRdy			B_BIT_2
#define HIS_CardEvent			B_BIT_3
#define HIS_CmdUpLdRdy			B_BIT_4
#define HIS_WrFifoOvrflow		B_BIT_5
#define HIS_RdFifoUndrflow		B_BIT_6
#define HIS_WlanReady			B_BIT_7

//#if defined (CF8385) || defined (CF8381) || defined (CF8305)
#if defined (CF83xx)
#define	HIM_ENABLE			0x001f
#define	HIM_DISABLE			0x001f
#else
#define	HIM_DISABLE			0xff
#ifdef MS83xx
#define HIM_ENABLE			0x7F
#else
#define HIM_ENABLE			0x03
#endif  /* MS83xx */
#endif

#define FIRMWARE_READY			0xfedc
#ifndef DEV_NAME_LEN
#define DEV_NAME_LEN			32
#endif
#define MAXKEYLEN			13
/* The number of times to try when polling for status bits */
#define MAX_POLL_TRIES			100

/* The number of times to try when waiting for downloaded firmware to */
/* become active. (polling the scratch register). */

#define MAX_FIRMWARE_POLL_TRIES		100

#define FIRMWARE_TRANSFER_NBLOCK	2   /* blocks */
#define SBI_EVENT_CAUSE_SHIFT		3
typedef enum {
	MVSD_DAT = 0,
	MVSD_CMD = 1,
	MVSD_TXDONE = 2,
	MVSD_EVENT
} mv_sd_type;

typedef wlan_private *	(*wlan_notifier_fn_add)(void *dev_id);
typedef int		(*wlan_notifier_fn_remove)(void *dev_id);
typedef void (*isr_notifier_fn_t)(int irq, void *dev_id, struct pt_regs *reg);
typedef void (*handler_fn_t)(int irq, void *dev_id, struct pt_regs *);
int sbi_probe_card (void *card); /* Probe and Check if the card is present*/
int sbi_register_dev (wlan_private *priv); 
int sbi_unregister_dev (wlan_private *);
int  sbi_disable_host_int(wlan_private *priv, u8 mask);
int sbi_get_int_status(wlan_private *priv, u8 *);
int *sbi_register(wlan_notifier_fn_add,wlan_notifier_fn_remove,void *);
void sbi_unregister(void);
int sbi_prog_firmware(wlan_private *);
int sbi_verify_fw_download(wlan_private *);
#ifdef HELPER_IMAGE
int sbi_prog_helper(wlan_private *);
int sbi_download_wlan_fw(wlan_private *);
#endif
int sbi_card_to_host(wlan_private *priv, u32 type, u32 *nb, u8 *payload,
							u16 npayload);
int sbi_read_event_cause(wlan_private *);
int sbi_reenable_host_interrupt(wlan_private *, u8);
int sbi_is_tx_download_ready(wlan_private *);
int sbi_host_to_card(wlan_private *priv, u8 type, u8 *payload, u16 nb);
int sbi_enable_host_int(wlan_private *, u8 mask);

int sbi_enter_bus_powersave(wlan_private *);
int sbi_exit_bus_powersave(wlan_private *);

#ifdef DEEP_SLEEP
inline int sbi_enter_deep_sleep(wlan_private *);
inline int sbi_exit_deep_sleep(wlan_private *);
inline int sbi_reset_deepsleep_wakeup(wlan_private *);
#endif

#ifdef CONFIG_PM
inline int sbi_suspend(wlan_private *);
inline int sbi_resume(wlan_private *);
#endif

int sbi_read_ioreg(wlan_private *priv, u8 func, u32 reg, u8 *dat);
int mv_sdio_card_to_host(wlan_private *priv, u32 *type, int *nb, u8 *payload, 
								int npayload);
int sbi_write_ioreg(wlan_private *priv, u8 func, u32 reg, u8 dat);

//#if defined (CF8385) || defined (CF8381) || defined (CF8305)
#if defined (CF83xx)
int sbi_read_cfreg(wlan_private *priv, int offset);
int sbi_write_cfreg(wlan_private *priv, int offset, u16 value);
#endif

int sbi_get_cis_info(wlan_private *priv);


static inline unsigned long get_utimeofday(void)
{
	struct timeval t;
	unsigned long ut;

	do_gettimeofday(&t);

	ut = (unsigned long)t.tv_sec * 1000000 + ((unsigned long)t.tv_usec);

	return ut;
}
#ifdef TXRX_DEBUG 
#define TXRX_DEBUG_GET_TIME(i) 	tt[i][tx_n] = get_utimeofday()
#define TXRX_DEBUG_GET_ALL(iloc, iireg, ics) \
	if (trd_n != -1) { \
		trd_n++; \
		if (trd_n>=TRD_N) \
			trd_n = 0; \
		trd[trd_n].time  = get_utimeofday(); \
		trd[trd_n].loc 	 = iloc; \
		trd[trd_n].intc  = priv->adapter->IntCounter; \
		trd[trd_n].txskb = (priv->adapter->CurrentTxSkb) ? 1 : 0; \
		trd[trd_n].dnld  = priv->wlan_dev.dnld_sent; \
		trd[trd_n].ireg  = iireg; \
		trd[trd_n].cs    = ics; \
	} 
#else
#define TXRX_DEBUG_GET_TIME(i)
#define	TXRX_DEBUG_GET_ALL(iloc, iireg, ics)
#endif

#ifdef TXRX_DEBUG
#define TRD_N 64
extern int tx_n;
extern int trd_n;
extern int trd_p;
extern unsigned long tt[7][16];
struct TRD {
	unsigned long time;
	u8	loc; 
	u8	intc;
	u8	txskb;
	u8	dnld;
	u8	ireg;
	u8	cs;
};
extern struct TRD trd[TRD_N];

static inline void print_txrx_debug(void)
{
	int i, j;
	int trd_n_save = 0;

	trd_n_save = trd_n;
	trd_n = -1;
	PRINTK("tx_n=%d\n", tx_n);
	for (i = 0; i < 16; i++) {
		PRINTK("i=%02d: ", i);
		for (j = 0; j < 1; j++)
			PRINTK("tt%d=%ld.%03ld.%03ld ",	j, tt[j][i] / 1000000,
						(tt[j][i] % 1000000) / 1000, 
						tt[j][i] % 1000);
		for (j = 1; j < 7; j++) {
			//PRINTK("tt%d=%ld.%03ld.%03ld ", j, tt[j][i]/1000000,
			//				(tt[j][i]%1000000)/1000,
			//				tt[j][i]%1000);
			long tt1 = tt[j][i] - tt[j-1][i];
			if (tt1 < 0) 
				tt1 = 0;
			PRINTK("tt%d=%ld.%ld.%03ld ", j, tt1 / 1000000,
							(tt1 % 1000000) / 1000,	
							tt1 % 1000);
		}
		PRINTK("\n");
	}
	PRINTK("trd_n=%d\n", trd_n_save);
	for (i = 0; i < TRD_N; i++) {
		PRINTK("i=%02d: ", i);
		PRINTK("%ld.%03ld.%03ld ", trd[i].time / 1000000,
						(trd[i].time % 1000000) / 1000,
						trd[i].time % 1000);
		PRINTK("loc=%02X  intc=%d  txskb=%d  dnld=%02X  ireg=%02X"
					"s=%02X\n", trd[i].loc, trd[i].intc,
						trd[i].txskb, trd[i].dnld,
						trd[i].ireg, trd[i].cs);
		if (i == trd_n_save) 
			PRINTK("\n");
	}
}
#endif

#endif /* _SBI_H */

