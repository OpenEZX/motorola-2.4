/*
 * File : wlan_main.c The main device driver file.
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
 * 2006-Aug-22 Motorola - Add bg_scan disable in remove_card.
 */

#include	"include.h"

#ifdef TXRX_DEBUG
int tx_n = 0;
unsigned long tt[7][16];
struct TRD trd[TRD_N];
int trd_n = 0;
int trd_p = 0;
#endif

#ifdef CONFIG_PM
struct pm_dev *wlan_pm_dev = NULL;
#endif 

#ifdef	WLAN_MEM_CHECK
u32           kmalloc_count,
                kfree_count;
u32           rxskb_alloc,
                rxskb_free;
u32           txskb_alloc,
                txskb_free;
#endif

u32	DSFreqList[15] = {
	0, 2412000, 2417000, 2422000, 2427000, 2432000, 2437000, 2442000,
	2447000, 2452000, 2457000, 2462000, 2467000, 2472000, 2484000
};

/* region code table */
u16	RegionCodeToIndex[MRVDRV_MAX_REGION_CODE] =
		{ 0x10, 0x20, 0x30, 0x31, 0x32, 0x40 };

#define WLAN_TX_PWR_DEFAULT		20 		/*100mW*/

#define WLAN_TX_PWR_US_DEFAULT		20 		/*100mW*/
#define WLAN_TX_PWR_JP_DEFAULT		16 		/*50mW*/
#define WLAN_TX_PWR_FR_DEFAULT		20 		/*100mW*/
#define WLAN_TX_PWR_EMEA_DEFAULT	20 		/*100mW*/

/* 
 * Format { Channel, Frequency (MHz), MaxTxPower } 
 */
/* Band: 'B/G', Region: USA FCC/Canada IC*/
CHANNEL_FREQ_POWER	channel_freq_power_US_BG[] = {
			{1, 2412, WLAN_TX_PWR_US_DEFAULT},   
			{2, 2417, WLAN_TX_PWR_US_DEFAULT},
			{3, 2422, WLAN_TX_PWR_US_DEFAULT}, 
			{4, 2427, WLAN_TX_PWR_US_DEFAULT}, 
			{5, 2432, WLAN_TX_PWR_US_DEFAULT}, 
			{6, 2437, WLAN_TX_PWR_US_DEFAULT}, 
			{7, 2442, WLAN_TX_PWR_US_DEFAULT}, 
			{8, 2447, WLAN_TX_PWR_US_DEFAULT}, 
			{9, 2452, WLAN_TX_PWR_US_DEFAULT},
			{10, 2457, WLAN_TX_PWR_US_DEFAULT},
			{11, 2462, WLAN_TX_PWR_US_DEFAULT}
};

/* Band: 'B/G', Region: Europe ETSI*/
CHANNEL_FREQ_POWER	channel_freq_power_EU_BG[] = {
			{1, 2412, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{2, 2417, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{3, 2422, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{4, 2427, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{5, 2432, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{6, 2437, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{7, 2442, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{8, 2447, WLAN_TX_PWR_EMEA_DEFAULT},
			{9, 2452, WLAN_TX_PWR_EMEA_DEFAULT},
			{10, 2457, WLAN_TX_PWR_EMEA_DEFAULT},
			{11, 2462, WLAN_TX_PWR_EMEA_DEFAULT},
			{12, 2467, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{13, 2472, WLAN_TX_PWR_EMEA_DEFAULT} 
};

/* Band: 'B/G', Region: Spain */
CHANNEL_FREQ_POWER	channel_freq_power_SPN_BG[] = {
			{10, 2457, WLAN_TX_PWR_DEFAULT}, 
			{11, 2462, WLAN_TX_PWR_DEFAULT} 
};

/* Band: 'B/G', Region: France */
CHANNEL_FREQ_POWER	channel_freq_power_FR_BG[] = {
			{10, 2457, WLAN_TX_PWR_FR_DEFAULT}, 
			{11, 2462, WLAN_TX_PWR_FR_DEFAULT},
			{12, 2467, WLAN_TX_PWR_FR_DEFAULT}, 
			{13, 2472, WLAN_TX_PWR_FR_DEFAULT} 
};

/* Band: 'B/G', Region: Japan */
CHANNEL_FREQ_POWER	channel_freq_power_JPN_BG[] = {
			{1, 2412, WLAN_TX_PWR_JP_DEFAULT}, 
			{2, 2417, WLAN_TX_PWR_JP_DEFAULT}, 
			{3, 2422, WLAN_TX_PWR_JP_DEFAULT}, 
			{4, 2427, WLAN_TX_PWR_JP_DEFAULT}, 
			{5, 2432, WLAN_TX_PWR_JP_DEFAULT}, 
			{6, 2437, WLAN_TX_PWR_JP_DEFAULT}, 
			{7, 2442, WLAN_TX_PWR_JP_DEFAULT}, 
			{8, 2447, WLAN_TX_PWR_JP_DEFAULT},
			{9, 2452, WLAN_TX_PWR_JP_DEFAULT}, 
			{10, 2457, WLAN_TX_PWR_JP_DEFAULT},
			{11, 2462, WLAN_TX_PWR_JP_DEFAULT}, 
			{12, 2467, WLAN_TX_PWR_JP_DEFAULT},
			{13, 2472, WLAN_TX_PWR_JP_DEFAULT}, 
			{14, 2484, WLAN_TX_PWR_JP_DEFAULT} 
};

#ifdef MULTI_BANDS
/* Band: 'A', Region: USA FCC, Canada IC, Spain, France */
CHANNEL_FREQ_POWER	channel_freq_power_A[] = {
			{36, 5180, WLAN_TX_PWR_US_DEFAULT}, 
			{40, 5200, WLAN_TX_PWR_US_DEFAULT}, 
			{44, 5220, WLAN_TX_PWR_US_DEFAULT}, 
			{48, 5240, WLAN_TX_PWR_US_DEFAULT}, 
			{52, 5260, WLAN_TX_PWR_US_DEFAULT}, 
			{56, 5280, WLAN_TX_PWR_US_DEFAULT}, 
			{60, 5300, WLAN_TX_PWR_US_DEFAULT}, 
			{64, 5320, WLAN_TX_PWR_US_DEFAULT}, 
			{149, 5745, WLAN_TX_PWR_US_DEFAULT}, 
			{153, 5765, WLAN_TX_PWR_US_DEFAULT}, 
			{157, 5785, WLAN_TX_PWR_US_DEFAULT},
			{161, 5805, WLAN_TX_PWR_US_DEFAULT}, 
			{165, 5825, WLAN_TX_PWR_US_DEFAULT}
};
	
/* Band: 'A', Region: Europe ETSI */
CHANNEL_FREQ_POWER	channel_freq_power_EU_A[] = {
			{36, 5180, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{40, 5200, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{44, 5220, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{48, 5240, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{52, 5260, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{56, 5280, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{60, 5300, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{64, 5320, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{100, 5500, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{104, 5520, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{108, 5540, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{112, 5560, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{116, 5580, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{120, 5600, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{124, 5620, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{128, 5640, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{132, 5660, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{136, 5680, WLAN_TX_PWR_EMEA_DEFAULT}, 
			{140, 5700, WLAN_TX_PWR_EMEA_DEFAULT}
};
	
/* Band: 'A', Region: Japan */
CHANNEL_FREQ_POWER	channel_freq_power_JPN_A[] = {
			{8, 5040, WLAN_TX_PWR_JP_DEFAULT}, 
			{12, 5060, WLAN_TX_PWR_JP_DEFAULT}, 
			{16, 5080, WLAN_TX_PWR_JP_DEFAULT},
			{34, 5170, WLAN_TX_PWR_JP_DEFAULT}, 
			{38, 5190, WLAN_TX_PWR_JP_DEFAULT}, 
			{42, 5210, WLAN_TX_PWR_JP_DEFAULT}, 
			{46, 5230, WLAN_TX_PWR_JP_DEFAULT}, 
/*			
			{240, 4920, WLAN_TX_PWR_JP_DEFAULT}, 
			{244, 4940, WLAN_TX_PWR_JP_DEFAULT}, 
			{248, 4960, WLAN_TX_PWR_JP_DEFAULT}, 
			{252, 4980, WLAN_TX_PWR_JP_DEFAULT} 
Removed these 4 11J channels*/ 
};
#endif /* MULTI BANDS */

typedef struct _region_cfp_table { 
	u8 region;
	CHANNEL_FREQ_POWER *cfp_BG;
	int cfp_no_BG;
#ifdef MULTI_BANDS
	CHANNEL_FREQ_POWER *cfp_A;
	int cfp_no_A;
#endif /* MULTI BANDS */
} region_cfp_table_t;

/*region_cfp_table defines all the supported region-band- chan list*/
region_cfp_table_t region_cfp_table[] = {
	{ 0x10, /*US FCC*/ 
		channel_freq_power_US_BG, 
		sizeof(channel_freq_power_US_BG)/sizeof(CHANNEL_FREQ_POWER),
#ifdef MULTI_BANDS
      		channel_freq_power_A,  
		sizeof(channel_freq_power_A)/sizeof(CHANNEL_FREQ_POWER),
#endif /* MULTI BANDS */
	},
	{ 0x20,	/*CANADA IC*/
		channel_freq_power_US_BG, 
		sizeof(channel_freq_power_US_BG)/sizeof(CHANNEL_FREQ_POWER),
#ifdef MULTI_BANDS
      		channel_freq_power_A,  
		sizeof(channel_freq_power_A)/sizeof(CHANNEL_FREQ_POWER),
#endif /* MULTI BANDS */
	},
	{ 0x30,	/*EU*/
		channel_freq_power_EU_BG, 
		sizeof(channel_freq_power_EU_BG)/sizeof(CHANNEL_FREQ_POWER),
#ifdef MULTI_BANDS
      		channel_freq_power_EU_A,  
		sizeof(channel_freq_power_EU_A)/sizeof(CHANNEL_FREQ_POWER),
#endif /* MULTI BANDS */
	},
	{ 0x31,	/*SPAIN*/
		channel_freq_power_SPN_BG, 
		sizeof(channel_freq_power_SPN_BG)/sizeof(CHANNEL_FREQ_POWER),
#ifdef MULTI_BANDS
      		channel_freq_power_A,  
		sizeof(channel_freq_power_A)/sizeof(CHANNEL_FREQ_POWER),
#endif /* MULTI BANDS */
	},
	{ 0x32,	/*FRANCE*/
		channel_freq_power_FR_BG, 
		sizeof(channel_freq_power_FR_BG)/sizeof(CHANNEL_FREQ_POWER),
#ifdef MULTI_BANDS
      		channel_freq_power_A,  
		sizeof(channel_freq_power_A)/sizeof(CHANNEL_FREQ_POWER),
#endif /* MULTI BANDS */
	},
	{ 0x40,	/*JAPAN*/
		channel_freq_power_JPN_BG, 
		sizeof(channel_freq_power_JPN_BG)/sizeof(CHANNEL_FREQ_POWER),
#ifdef MULTI_BANDS
      		channel_freq_power_JPN_A,  
		sizeof(channel_freq_power_JPN_A)/sizeof(CHANNEL_FREQ_POWER),
#endif /* MULTI BANDS */
	},
/*Add new region here */	
};

u8	WlanDataRates[WLAN_SUPPORTED_RATES] = 
		{ 0x02, 0x04, 0x0B, 0x16, 0x00, 0x0C, 0x12, 
		  0x18, 0x24, 0x30, 0x48, 0x60, 0x6C, 0x00 };

#ifdef MULTI_BANDS
u8	SupportedRates[A_SUPPORTED_RATES];

/* First two rates are basic rates */
u8	SupportedRates_B[B_SUPPORTED_RATES] =
		{ 0x82, 0x84, 0x0b, 0x16, 0, 0, 0, 0 };

/* First four rates are basic rates */
u8	SupportedRates_G[G_SUPPORTED_RATES] =
		{ 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c, 0}; 

/* TODO:  Check which are basic rates for band 'a' */
/* First two rates are basic rates */
u8	SupportedRates_A[A_SUPPORTED_RATES] =
		{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0};

u8	AdhocRates_A[A_SUPPORTED_RATES] =
		{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0};
#else
#ifdef	G_RATE
u8	SupportedRates[G_SUPPORTED_RATES] =
		{ 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c, 0}; 
#else
u8	SupportedRates[B_SUPPORTED_RATES] =
		{ 0x82, 0x84, 0x0b, 0x16, 0, 0, 0, 0 };
#endif
#endif

#ifdef ADHOC_GRATE
u8	AdhocRates_G[G_SUPPORTED_RATES] =
		{ 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c, 0};
#else ///ADHOC_GRATE
u8	AdhocRates_G[4] =
		{ 0x82, 0x84, 0x8b, 0x96};
#endif ///ADHOC_GRATE

u8	AdhocRates_B[4] =
		{ 0x82, 0x84, 0x8b, 0x96};

static wlan_private	*wlan_add_card(void *card);
static int      wlan_remove_card(void *card);

//static void     free_private(wlan_private * priv);


char           *driver_name = "marvell";

wlan_private   *wlanpriv = NULL;

const char     *errRequestMem = "request_mem_region failed!";
const char     *errAllocation = "memory allocation failed!";
const char     *errNoEtherDev = "cannot initialize ethernet device!";

CHANNEL_FREQ_POWER *wlan_get_region_cfp_table(u8 region, u8 band, int *cfp_no)
{
	int  i;

	ENTER();
			
	for ( i=0; i< sizeof(region_cfp_table)/sizeof(region_cfp_table_t); 
		i++ ) {
		PRINTK2("region_cfp_table[i].region=%d\n", region_cfp_table[i].region );
		if (region_cfp_table[i].region == region ) {
#ifdef MULTI_BANDS
			if (band & (BAND_B | BAND_G)) 
#endif
			{
				*cfp_no = region_cfp_table[i].cfp_no_BG;
				LEAVE();
				return region_cfp_table[i].cfp_BG;
			}
#ifdef MULTI_BANDS
			else {
				if (band & BAND_A) {
					*cfp_no = region_cfp_table[i].cfp_no_A;
					LEAVE();
					return region_cfp_table[i].cfp_A;
				}
				else {
					PRINTK2("Error Band[%x]\n", band );
					LEAVE();
					return NULL;
				}
			}
#endif
		}
	}
	
	LEAVE();	
	return NULL;
}


int wlan_set_regiontable(wlan_private *priv, u8 region, u8 band)
{
	wlan_adapter	*Adapter = priv->adapter;
	int		i = 0;

	CHANNEL_FREQ_POWER *cfp;
	int 		cfp_no;

	ENTER();
	
	memset(Adapter->region_channel, 0, sizeof(Adapter->region_channel));

#ifdef MULTI_BANDS
	if (band & (BAND_B | BAND_G)) 
#endif
	{
#ifdef MULTI_BANDS
		cfp = wlan_get_region_cfp_table( region, BAND_G|BAND_B, &cfp_no);
#else
		cfp = wlan_get_region_cfp_table( region, band, &cfp_no);
#endif
		if ( cfp != NULL ) {
			Adapter->region_channel[i].NrCFP = cfp_no;
			Adapter->region_channel[i].CFP = cfp; 
		}
		else {
			PRINTK2("wrong region code %#x in Band B-G\n",region);
			return -1;
		}
		Adapter->region_channel[i].Valid	= TRUE;
		Adapter->region_channel[i].Region	= region;
#ifdef MULTI_BANDS
		Adapter->region_channel[i].Band	= 
			(band & BAND_G) ? BAND_G : BAND_B;
#else
		Adapter->region_channel[i].Band		= band;
#endif
		i++;
	}
#ifdef MULTI_BANDS
	if (band & BAND_A) {
		cfp = wlan_get_region_cfp_table( region, BAND_A, &cfp_no);
		if ( cfp != NULL ) {
			Adapter->region_channel[i].NrCFP = cfp_no;
			Adapter->region_channel[i].CFP = cfp; 
		}
		else {
			PRINTK2("wrong region code %#x in Band A\n",region);
			return -1;
		}
		Adapter->region_channel[i].Valid	= TRUE;
		Adapter->region_channel[i].Region	= region;
		Adapter->region_channel[i].Band		= BAND_A;
	}
#endif
	LEAVE();
	return 0;
}

#ifdef CONFIG_PM
extern int SetDeepSleep(wlan_private *priv, BOOLEAN bDeepSleep);

/* Linux Power Management */
static int wlan_pm_callback(struct pm_dev *pmdev, pm_request_t pmreq,
			    void *pmdata)
{
	wlan_private 		*priv = wlanpriv;
	wlan_adapter 		*Adapter = priv->adapter;
	struct net_device 	*dev = priv->wlan_dev.netdev;

        PRINTK("WPRM_PM_CALLBACK: pmreq = %d.\n", pmreq);

	switch (pmreq) {
		case PM_SUSPEND:
                        PRINTK("WPRM_PM_CALLBACK: enter PM_SUSPEND.\n");

#ifdef WPRM_DRV
                        /* check WLAN_HOST_WAKEB */
                        if(wprm_wlan_host_wakeb_is_triggered()) {
                                printk("exit on GPIO-1 triggered.\n");
                                return -1;
                        }
#endif
 
                        /* in associated mode */
                        if(Adapter->MediaConnectStatus == WlanMediaStateConnected) {
#ifdef PS_REQUIRED
                                if(
                                	(Adapter->PSState != PS_STATE_SLEEP)
#ifdef HOST_WAKEUP
					|| !Adapter->bHostWakeupDevRequired
					|| (Adapter->WakeupTries != 0)
#endif
				) {
					PRINTK("wlan_pm_callback: can't enter sleep mode\n");
                                        return -1;
                                }
                                else
#endif
				{

                                        /*
                                         * Detach the network interface
                                         * if the network is running
                                         */
                                        if (netif_running(dev)) {
                                                netif_device_detach(dev);
                                                PRINTK("netif_device_detach().\n");
                                        }

#ifdef BULVERDE_SDIO
                                        /*
                                         * Stop SDIO bus clock
                                         */
                                        stop_bus_clock_2(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);

#endif

                                        sbi_suspend(priv);
                                }
                                break;
                        }
 
                        /* in non associated mode */
			if (Adapter->IsDeepSleep == FALSE) {
				printk("WLAN: No allowed to enter sleep while in FW in full power.\n");
				return -1;
			}

			/*
			 * Detach the network interface 
			 * if the network is running
			 */
			if (netif_running(dev))
				netif_device_detach(dev);

			/* 
			 * Storing and restoring of the regs be taken care 
			 * at the driver rest will be done at wlan driver
			 * this makes driver independent of the card
			 */
			sbi_suspend(priv);

			break;

		case PM_RESUME:
                        /* in associated mode */
                        if(Adapter->MediaConnectStatus == WlanMediaStateConnected) {
#ifdef HOST_WAKEUP
                                if(Adapter->bHostWakeupDevRequired == FALSE) {
                                        /* could never happen */
                                        printk("wlan_pm_callback: serious error.\n");
                                }
                                else
#endif
				{
                                        /*
                                         * Bring the inteface up first
                                         * This case should not happen still ...
                                         */
                                        sbi_resume(priv);

#ifdef BULVERDE_SDIO
                                        /*
                                         * Start SDIO bus clock
                                         */
                                        start_bus_clock(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);

#endif

                                        /*
                                         * Attach the network interface
                                         * if the network is running
                                         */
                                        if (netif_running(dev)) {
                                                netif_device_attach(dev);
                                                PRINTK("after netif_device_attach().\n");
                                        }
                                        PRINTK("After netif attach, in associated mode.\n");
                                }
                        	break;
                        }
 
                        /* in non associated mode */

#ifdef WPRM_DRV
                        /* Background scan support */
                        WPRM_DRV_TRACING_PRINT();
                        /* check if WLAN_HOST_WAKEB triggered, turn on SDIO_CLK */
                        if(wprm_wlan_host_wakeb_is_triggered()) { /* WLAN_HSOT_WAKEB is triggered */
                                if(wlan_sdio_clock(priv, TRUE)) {
                                        printk("wlan_pm_callback: in PM_RESUME, wlan sdio clock turn on fail\n");
                                }
                                WPRM_DRV_TRACING_PRINT();
                        }
#endif
			/*
			 * Bring the inteface up first 
			 * This case should not happen still ...
			 */
			sbi_resume(priv);

			if (netif_running(dev))
				netif_device_attach(dev);

                        PRINTK("after netif attach, in NON associated mode.\n");
			break;
	}

	return 0;
}
#endif /* CONFIG_PM */

/*
 * The open function for the network device. 
 */
static int wlan_open(struct net_device *dev)
{
	wlan_private   *priv = (wlan_private *) dev->priv;
	wlan_adapter   *adapter = priv->adapter;

	ENTER();
	
	memcpy(dev->dev_addr, adapter->PermanentAddr, ETH_ALEN);
	
	MOD_INC_USE_COUNT;

	priv->open = TRUE;
	//netif_carrier_on(dev);
	netif_start_queue(dev);	// To be compatible with ifconfig up/down

	LEAVE();
	return 0;
}

/*
 * Network stop function 
 */
static int wlan_stop(struct net_device *dev)
{
	wlan_private   *priv = dev->priv;
	
	ENTER();

	/* Flush all the packets upto the OS before stopping */
	wlan_send_rxskbQ(priv);
	netif_stop_queue(dev);

	/*
	 * TODO: Tell the card to stop receiving. TODO: Tell the card to stop
	 * transmitting. TODO: If possible - power of the card 
	 */
	
	MOD_DEC_USE_COUNT;

	priv->open = FALSE;

	LEAVE();
	return 0;
}

/*
 * To push the packets on to the net work 
 */
int wlan_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int             ret;
	wlan_private   *priv = dev->priv;
	wlan_adapter   *adapter = priv->adapter;
#ifdef OMAP1510_TIMER_DEBUG
	    int            i;
#endif /* OMAP1510_TIMER_DEBUG */

    	ENTER();
    
	if (adapter->SurpriseRemoved) {
		PRINTK1("Card Removed!!\n");
		ret = 1;
		goto done;
	}

    	if (!skb) {
		PRINTK1("No SKB Available\n");
		ret = 1;
		goto done;
	}

	if (priv->open != TRUE) {
		PRINTK1("priv->open is set to false!!\n");
		ret = 1;		/* TODO: return right value */
		goto done;
	}

	if (netif_queue_stopped(dev)) {
		PRINTK1("called when queue stopped.\n");
		ret = 1;		/* TODO: return right value */
		goto done;
	}
/*
	if(wlan_is_tx_download_ready(priv)) {
		PRINTK("tx_download not ready, return to kernel\n");
		ret = 1;
		goto done;
	}
*/

	if (adapter->MediaConnectStatus != WlanMediaStateConnected) {
		ret = 1;
		goto done;
	}	

#ifdef WMM
	if (!adapter->wmm.enabled) {
#endif /* WMM */
		
#ifdef PS_REQUIRED
		if ((adapter->PSState == PS_STATE_SLEEP
#ifdef HOST_WAKEUP
			&& (!adapter->bHostWakeupDevRequired)
#endif
			)
#ifdef PS_PRESLEEP
			|| (adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
		) {
			PRINTK1("_xmit() in PS state: %d\n", adapter->PSState);
			ret = 1;
			goto done;
		}
#endif

#ifdef WMM
	}
#endif
	
#ifdef DEEP_SLEEP
	if (adapter->IsDeepSleep == TRUE) {
		ret = 1;
		goto done;
	}
#endif // DEEP_SLEEP

#ifdef TXRX_DEBUG
	tx_n++;
	if(tx_n>=16) tx_n=0;
	tt[0][tx_n] = get_utimeofday();
#endif

#ifdef OMAP1510_TIMER_DEBUG
	for(i=0; i<10; ++i)
		tm_ind[i] = 0;

	outl(0x22, OMAP1510_TIMER1_BASE);
	outl(0xFFFFFFFF, OMAP1510_TIMER1_BASE + 0x04);
	outl(0x23, OMAP1510_TIMER1_BASE);

	times[0][(tm_ind[0])++] = inw(0xFFFEC500 + 0x08);
#endif /* OMAP1510_TIMER_DEBUG */

	if (wlan_tx_packet(priv, skb)) {
		/* Transmit failed */
		ret = 1;
		goto done;
	} else {
		/* Transmit succeeded */
#ifdef WMM
		if (!adapter->wmm.enabled) {
#endif /* WMM */
			UpdateTransStart(dev);
			netif_stop_queue(dev);
#ifdef WMM
		}
#endif
	}

	ret = 0;
done:
	LEAVE();
	return ret;
}

#ifdef linux	
/*
 * Time out watch dog , which wakes up on a time out! 
 */
void wlan_tx_timeout(struct net_device *dev)
{
	wlan_private   *priv = (wlan_private *) dev->priv;
#ifdef TXRX_DEBUG
//	u8 ireg,cs;
	int i,j;
	int trd_n_save = 0;
#endif

	ENTER1();

#ifdef TXRX_DEBUG
	trd_n_save = trd_n;
	trd_n = -1;
	trd_p = -1;
	wake_up_interruptible(&priv->MainThread.waitQ);
	/* Remove: TxCount never updated, Remove or update this variable */
	PRINTK("main.c tx_timeout() called. TxCount=%d\n",
						priv->adapter->TxCount);
	PRINTK("tx_n=%d\n",tx_n);
	for(i=0;i<16;i++) {
		PRINTK("i=%02d: ",i);
		for(j=0;j<1;j++)
			PRINTK("tt%d=%ld.%03ld.%03ld ", j, tt[j][i]/1000000,
					(tt[j][i]%1000000)/1000,tt[j][i]%1000);
		for(j=1;j<7;j++) {
			//PRINTK("tt%d=%ld.%03ld.%03ld ", j, tt[j][i]/1000000,
			//		(tt[j][i]%1000000)/1000,tt[j][i]%1000);
			long tt1 = tt[j][i] - tt[j-1][i];
			if(tt1<0) tt1=0;
			PRINTK("tt%d=%ld.%ld.%03ld ", j, tt1/1000000,
						(tt1%1000000)/1000,tt1%1000);
		}
		PRINTK("\n");
	}
	PRINTK("trd_n=%d\n",trd_n_save);
	for(i=0;i<TRD_N;i++) {
		PRINTK("i=%02d: ",i);
		PRINTK("%ld.%03ld.%03ld ", trd[i].time/1000000, 
				(trd[i].time%1000000)/1000,trd[i].time%1000);
		PRINTK("loc=%02X intc=%d txskb=%d "
			"dnld=%02X ireg=%02X cs=%02X\n", 
			trd[i].loc, trd[i].intc, trd[i].txskb,
			trd[i].dnld,trd[i].ireg,trd[i].cs);
		if (i == trd_n_save) PRINTK("\n");
	}
#endif

	/* comment out to avoid crashing firmware */
//	priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;

	dev->trans_start = jiffies;
	if (priv->adapter->CurrentTxSkb) {
		wake_up_interruptible(&priv->MainThread.waitQ);
	} else {
		if (netif_queue_stopped(dev))
			netif_wake_queue(dev);
	}

	LEAVE1();
}
#endif	// linux

/*
 * Network statistics!! 
 */
static struct net_device_stats *wlan_get_stats(struct net_device *dev)         
{
	wlan_private   *priv = (wlan_private *) dev->priv;

	/* TODO: Use getlog command to get more stats */
#if 0
	wlan_adapter   *Adapter = priv->adapter;
	
	/*
	 * detailed rx_errors: 
	 */
	priv->stats.rx_crc_errors = Adapter->wlan802_3Stat.RcvCRCError;
	priv->stats.rx_fifo_errors = Adapter->wlan802_3Stat.RcvNoBuffer;
#endif

	return &priv->stats;
}

/* set the MAC address through ifconfig */
int wlan_set_mac_address(struct net_device *dev, void *addr)
{
	int		ret = 0;
	wlan_private	*priv = (wlan_private *) dev->priv;
	wlan_adapter	*Adapter = priv->adapter;
	struct sockaddr	*pHwAddr = (struct sockaddr *)addr;
	
	ENTER();
	
	memset(Adapter->CurrentAddr, 0, MRVDRV_ETH_ADDR_LEN); 
	
	/* dev->dev_addr is 8 bytes */
	HEXDUMP("dev->dev_addr:", dev->dev_addr, ETH_ALEN);

	HEXDUMP("addr:", pHwAddr->sa_data, ETH_ALEN);
	memcpy(Adapter->CurrentAddr, pHwAddr->sa_data, ETH_ALEN);

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_MAC_ADDRESS,
		HostCmd_ACT_SET,
		HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
		0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	/* This is not necessary for production code */
	PRINTK1("Get MAC Address\n");
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_MAC_ADDRESS,
		HostCmd_ACT_GET,
		HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
		0, HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	HEXDUMP("Adapter->MacAddr:", Adapter->CurrentAddr, ETH_ALEN);
	memcpy(dev->dev_addr, Adapter->CurrentAddr, ETH_ALEN);

	LEAVE();
	return ret;
}

int wlan_get_mac_address(struct net_device *dev, unsigned char *Addr)
{
	
	wlan_private   *priv = (wlan_private *) dev->priv;
	wlan_adapter   *Adapter = priv->adapter;
	
	ENTER();
	
	memcpy(Addr, Adapter->CurrentAddr, ETH_ALEN);
	
	LEAVE();
	return 0;	
}

int SendSetMulticast(wlan_private * priv, void *p)
{
	int 	ret = 0;

	ret = PrepareAndSendCommand(priv, HostCmd_CMD_MAC_MULTICAST_ADR,
			HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_GET_OID, NULL);

	return ret;
}

void wlan_set_multicast_list(struct net_device *dev)
{
	wlan_private   *priv = dev->priv;
	wlan_adapter   *Adapter = priv->adapter;
	int             OldPacketFilter;

	ENTER();

	OldPacketFilter = Adapter->CurrentPacketFilter;

	if (dev->flags & IFF_PROMISC) {
		PRINTK1("Enable Promiscuous mode\n");
		Adapter->CurrentPacketFilter |= 
			HostCmd_ACT_MAC_PROMISCUOUS_ENABLE;
		Adapter->CurrentPacketFilter &=
			~(HostCmd_ACT_MAC_ALL_MULTICAST_ENABLE |
					HostCmd_ACT_MAC_MULTICAST_ENABLE);
	} else {
		/* Multicast */
		Adapter->CurrentPacketFilter &=
			~HostCmd_ACT_MAC_PROMISCUOUS_ENABLE;

		if (dev->flags & IFF_ALLMULTI || dev->mc_count > 
					MRVDRV_MAX_MULTICAST_LIST_SIZE) {
			PRINTK1("Enabling All Multicast!\n");
			Adapter->CurrentPacketFilter |=
				HostCmd_ACT_MAC_ALL_MULTICAST_ENABLE;
			Adapter->CurrentPacketFilter &=
				~HostCmd_ACT_MAC_MULTICAST_ENABLE;
		} else {
			Adapter->CurrentPacketFilter &=
				~HostCmd_ACT_MAC_ALL_MULTICAST_ENABLE;

			if (!dev->mc_count) {
				PRINTK1("No multicast addresses - "
						"disabling multicast!\n");
				Adapter->CurrentPacketFilter &=
					~HostCmd_ACT_MAC_MULTICAST_ENABLE;
			} else {
#ifdef DEBUG_LEVEL1
				int	i;
#endif
				
				Adapter->CurrentPacketFilter |=
					HostCmd_ACT_MAC_MULTICAST_ENABLE;

				Adapter->NumOfMulticastMACAddr = 
					CopyMulticastAddrs(Adapter, dev);

#ifdef DEBUG_LEVEL1
				PRINTK1("Multicast addresses: %d\n", 
							dev->mc_count);
				
				for (i = 0; i < dev->mc_count; i++) {
					PRINTK1("Multicast address %d:"
						"%x %x %x %x %x %x\n", i,
						Adapter->MulticastList[i][0],
						Adapter->MulticastList[i][1],
						Adapter->MulticastList[i][2],
						Adapter->MulticastList[i][3],
						Adapter->MulticastList[i][4],
						Adapter->MulticastList[i][5]);
				}
#endif
				SendSetMulticast(priv, NULL);
			}
		}
	}

	if (Adapter->CurrentPacketFilter != OldPacketFilter) {
		SetMacPacketFilter(priv);
	}

	LEAVE();
}

int wlan_reassociation_thread(void *data)
{
	wlan_thread	*thread = data;
	wlan_private	*priv = thread->priv;
	wlan_adapter	*Adapter = priv->adapter;
	wait_queue_t	wait;
	int		i;//, Status;

	OS_INTERRUPT_SAVE_AREA;	/* Needed for Threadx */

    	ENTER();
    
	wlan_activate_thread(thread);
	init_waitqueue_entry(&wait, current);

	for (;;) {

                if(signal_pending(current))
		        break;
		/*
		 * go to sleep slowly to take care of races 
		 */
		add_wait_queue(&thread->waitQ, &wait);
		OS_SET_THREAD_STATE(TASK_INTERRUPTIBLE);

		TX_DISABLE;	/* required for threadx */
		
		if (!Adapter->EventCounter) {
			schedule();
			TX_DISABLE;	/* required for threadx */
		}

		OS_SET_THREAD_STATE(TASK_RUNNING);
		remove_wait_queue(&thread->waitQ, &wait);

		Adapter->EventCounter = 0;

		if (thread->state == WLAN_THREAD_STOPPED)
			break;

#ifdef PS_REQUIRED
#ifdef BULVERDE_SDIO
		if ((Adapter->PSState == PS_STATE_SLEEP)
#ifdef PS_PRESLEEP
			|| (Adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
		) {
			continue;
		}
#endif
#endif

		PRINTK1("Re-association Thread running...\n");
		PRINTK1("Performing Active Scan @ %lu\n", os_time_get());
		
		SendSpecificScan(priv, &Adapter->PreviousSSID);

		PRINTK1("#### PERIODIC TIMER ON, Previous SSID = %s"
				" #####\n", Adapter->PreviousSSID.Ssid);
		PRINTK1("Required ESSID :%s\n", Adapter->PreviousSSID.Ssid);
		PRINTK1("No of BSSID: %d\n", Adapter->ulNumOfBSSIDs);
		PRINTK1("InfrastructureMode: 0x%x\n", 
						Adapter->InfrastructureMode);
		PRINTK1("MediaConnectStatus: 0x%x\n", 
						Adapter->MediaConnectStatus);

		if (Adapter->MediaConnectStatus == WlanMediaStateDisconnected) {
			u8      *pReqBSSID = NULL;

			/*
			 * Comparing our PreviousSSID 
			 * and finding a match SSID from list 
			 */
			PRINTK1("Adapter->ulNumOfBSSIDs= %d\n", 
					Adapter->ulNumOfBSSIDs);

			if (Adapter->RequestViaBSSID)
				pReqBSSID = Adapter->RequestedBSSID;
  
			i = FindSSIDInList(Adapter, &Adapter->PreviousSSID,
					pReqBSSID, Adapter->InfrastructureMode);

			if (i >= 0) {
				if (Adapter->InfrastructureMode ==
						Wlan802_11Infrastructure) {
					// send an association command
					PRINTK1("MISC - Do recover association,"
						"previous SSID = %s\n", 
						Adapter->PreviousSSID.Ssid);

					wlan_associate(priv, 
							&Adapter->PreviousSSID);
				
#ifdef PS_REQUIRED
					if (Adapter->PSState != 
							PS_STATE_FULL_POWER)
						PSWakeup(priv, 0);
#endif

				} else if (Adapter->InfrastructureMode == 
						Wlan802_11IBSS) {
					PRINTK1("MISC - Do a recover join" 
							" command \n");
					PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_AD_HOC_JOIN,
						0, HostCmd_OPTION_USE_INT	
						| HostCmd_OPTION_WAITFORRSP,
						0, HostCmd_PENDING_ON_CMD,
						&Adapter->PreviousSSID);
				}
			} else { 
				/* No handling for infrastructure mode */
				if (Adapter->InfrastructureMode == 
							Wlan802_11IBSS) {
					PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_AD_HOC_START,
						0, HostCmd_OPTION_USE_INT	
						| HostCmd_OPTION_WAITFORRSP,
						0, HostCmd_PENDING_ON_CMD,
						&Adapter->PreviousSSID);
				}
			}
		}

		if (Adapter->MediaConnectStatus == WlanMediaStateDisconnected) {
			PRINTK1("Restarting Re-association Timer @ %lu\n",
								os_time_get());
			Adapter->TimerIsSet = TRUE;
			ModTimer(&Adapter->MrvDrvTimer, MRVDRV_TIMER_10S);
		}
	}

	wlan_deactivate_thread(thread);

	LEAVE();
	return 0;
}

struct sk_buff* wlan_pop_rx_skb(struct sk_buff *RxSkbQ)
{
	struct sk_buff* skb_data = NULL;

	if(!list_empty((struct list_head *) RxSkbQ)) {
		skb_data = RxSkbQ->next;	
		list_del((struct list_head *) RxSkbQ->next);
	}

	return skb_data;
}

int wlan_service_main_thread(void *data)
{
	wlan_thread	*thread = data;
	wlan_private	*priv = thread->priv;
	wlan_adapter	*Adapter = priv->adapter;
	wait_queue_t    wait;
	u8              ireg = 0;
        BOOLEAN		IsReqPending = FALSE;
	OS_INTERRUPT_SAVE_AREA;	/* Required for ThreadX */

	ENTER();

	wlan_activate_thread(thread);
	
	init_waitqueue_entry(&wait, current);

#ifdef DRIVER_SCHED_RR
        struct sched_param  param;
        param.sched_priority = MAX_USER_RT_PRIO - 1;

        if( setscheduler(0, SCHED_RR, &param) != 0 )
          printk("setscheduler :: FAILED \n");
        else
          printk("setscheduler :: SUCCESS PRIO = %d \n",param.sched_priority);
#endif 

	for (;;) {

                    if(signal_pending(current))
		           break;

		/* Go to sleep slowly to take care of races */
		TXRX_DEBUG_GET_ALL(0x10, 0xff, 0xff);
		PRINTK1("Interrupt thread 111: IntCounter=%d "
			"CurrentTxSkb=%p dnld_sent=%d\n", Adapter->IntCounter, 
			Adapter->CurrentTxSkb, priv->wlan_dev.dnld_sent);

		add_wait_queue(&thread->waitQ, &wait);
		OS_SET_THREAD_STATE(TASK_INTERRUPTIBLE);

		TXRX_DEBUG_GET_ALL(0x11, 0xff, 0xff);

		TX_DISABLE;	/* required for threadx */
		
		if (
#ifdef HOST_WAKEUP
#define MAX_WAKE_UP_TRY			5				
			(Adapter->WakeupTries >= MAX_WAKE_UP_TRY) ||
#endif 							
#ifdef PS_REQUIRED
			(Adapter->PSState == PS_STATE_SLEEP
#ifdef HOST_WAKEUP
				&& !Adapter->bHostWakeupDevRequired
#endif /* HOST_WAKEUP */
			) ||
#endif /* PS_REQUIRED */
			(
			!Adapter->IntCounter 
#ifdef WMM
			&& (priv->wlan_dev.dnld_sent || !Adapter->wmm.enabled || 
#ifdef WMM_UAPSD
				Adapter->wmm.no_more_packet || 
#endif				
				wmm_lists_empty(priv))
#endif /* WMM */
			&& (priv->wlan_dev.dnld_sent || !Adapter->CurrentTxSkb)
			&& (priv->wlan_dev.dnld_sent || Adapter->CurCmd ||
				list_empty(&Adapter->CmdPendingQ))
			)
		) {
			PRINTK1("scheduling out... Conn=%d IntC=%d PS_Mode=%d PS_State=%d\n",
				Adapter->MediaConnectStatus, Adapter->IntCounter,
				Adapter->PSMode, Adapter->PSState);			
#ifdef BUS_POWERSAVE
                        /* If this was the last request pending */		
			if (IsReqPending) {
				/* If we are not in deep sleep mode */
				if (!Adapter->IsDeepSleep) {
					/* If we are not in IEEE powersave mode */
					if (!ps_sleep_confirmed) {
						/* Enter bus powersave mode */
						if(sbi_enter_bus_powersave(priv)) {
							printk("ERROR: entering bus powersave mode\n");
						}
					}
				}
				IsReqPending = FALSE;
			}
#endif

#ifdef _MAINSTONE
			MST_LEDDAT1 = get_utimeofday();
#endif
			TX_RESTORE;	/* required for threadx */
			schedule();
		} else {
			TX_RESTORE;	/* required for threadx */
		}

		PRINTK1("Interrupt thread 222: IntCounter=%d CurrentTxSkb=%p "
				"dnld_sent=%d\n", Adapter->IntCounter, 
				Adapter->CurrentTxSkb,priv->wlan_dev.dnld_sent);

		TXRX_DEBUG_GET_ALL(0x12, 0xff, 0xff);
		OS_SET_THREAD_STATE(TASK_RUNNING);
		remove_wait_queue(&thread->waitQ, &wait);

		TXRX_DEBUG_GET_ALL(0x13, 0xff, 0xff);
		PRINTK1("Interrupt thread 333: IntCounter=%d CurrentTxSkb=%p "
				"dnld_sent=%d\n", Adapter->IntCounter, 
				Adapter->CurrentTxSkb,priv->wlan_dev.dnld_sent);

		PRINTK1("Main thread wakes up for service!\n");

#ifdef BUS_POWERSAVE 
		/* If this is the first request pending */
		if (!IsReqPending) {
			/* If we are not in deep sleep mode */
			if (!Adapter->IsDeepSleep) {
				/* Exit bus powersave mode */
				if(sbi_exit_bus_powersave(priv)) {
            				printk("ERROR: exiting bus powersave mode\n");
        			}
			}
        		IsReqPending = TRUE;
    		}
#endif

		if ((thread->state == WLAN_THREAD_STOPPED) || 
					Adapter->SurpriseRemoved)
			break;

#if 0
#ifdef PS_REQUIRED
#ifdef BULVERDE_SDIO
		/* Here 'IntCounter' is decremented. 
		 * This is done to increase the CPU idle time from 30% to 95%
		 * when the card is put to power save mode on BULVERDE. */
		if (Adapter->PSState == PS_STATE_SLEEP) {
			OS_INT_DISABLE;
			if (Adapter->IntCounter > 0)
				--Adapter->IntCounter;
			OS_INT_RESTORE;
			continue;
		}
#endif
#endif
#endif

		if (Adapter->IntCounter) {
			OS_INT_DISABLE;
			Adapter->IntCounter = 0;
			OS_INT_RESTORE;
			/*
			 * Read the interrupt status register 
			 */
			if (sbi_get_int_status(priv, &ireg)) {
				PRINTK1("ERROR: reading HOST_INT_STATUS_REG\n");
				continue;
			}
			Adapter->HisRegCpy |= ireg;
		}
#ifdef HOST_WAKEUP
#ifdef PS_REQUIRED
		else if (Adapter->PSState == PS_STATE_SLEEP
				&& Adapter->bHostWakeupDevRequired
#ifdef FW_WAKEUP_TIME
				&& (wt_pwrup_sending == 0L)
#endif
		) {
#ifdef DEEP_SLEEP
#ifdef FW_WAKEUP_TIME
			wt_pwrup_sending = get_utimeofday();
#endif
			Adapter->WakeupTries++;
			/* we borrow deep_sleep wakeup code for time being */
			if(sbi_exit_deep_sleep(priv))
				printk("Host Wakeup Dev: wakeup failed\n");
#endif //DEEP_SLEEP
			continue;
		}
#endif //PS_REQUIRED
#endif //HOST_WAKEUP

		PRINTK1("Interrupt thread 444: IntCounter=%d CurrentTxSkb=%p "
				"dnld_sent=%d\n", Adapter->IntCounter, 
				Adapter->CurrentTxSkb,priv->wlan_dev.dnld_sent);
		
		TXRX_DEBUG_GET_ALL(0x14, ireg, 0xff);

		/* Command response? */
		if (Adapter->HisRegCpy & HIS_CmdUpLdRdy) {
			PRINTK1("Cmd response ready! 0x%x\n",ireg);

			OS_INT_DISABLE;
			Adapter->HisRegCpy &= ~HIS_CmdUpLdRdy;
			OS_INT_RESTORE;

			wlan_process_rx_command(priv);
		}

		/* Any received data? */
		if (Adapter->HisRegCpy & HIS_RxUpLdRdy) {
			PRINTK1("Rx Packet ready! 0x%x\n",ireg);
			TXRX_DEBUG_GET_ALL(0x16, ireg, 0xff);
			
			OS_INT_DISABLE;
			Adapter->HisRegCpy &= ~HIS_RxUpLdRdy;
			OS_INT_RESTORE;

#ifndef THROUGHPUT_TEST
			wlan_send_rxskbQ(priv);
#else
			Adapter->NumTransfersRx += 1;
#endif /* THROUGHPUT_TEST */
		}

		/* Any Card Event */
		if (Adapter->HisRegCpy & HIS_CardEvent) {
			PRINTK1("Card Event Activity 0x%x\n",ireg);

			OS_INT_DISABLE;
			Adapter->HisRegCpy &= ~HIS_CardEvent;
			OS_INT_RESTORE;

			if (sbi_read_event_cause(priv)) {
				continue;
			}
			wlan_process_event(priv);
			PRINTK1("Clearing CardEvent INT\n");
		}

#ifdef PS_REQUIRED
#ifdef PS_PRESLEEP
		/* Check if we need to confirm Sleep Request received previously */
		if (Adapter->PSState == PS_STATE_PRE_SLEEP) {
			if (!priv->wlan_dev.dnld_sent && !Adapter->CurCmd) {
                            if(Adapter->MediaConnectStatus == WlanMediaStateConnected) {
				PRINTK1("main_thread PRE_SLEEP: IntCounter=%d CurrentTxSkb=%p "
				"dnld_sent=%d CurCmd=%p, confirm now\n", Adapter->IntCounter, 
				Adapter->CurrentTxSkb,priv->wlan_dev.dnld_sent,Adapter->CurCmd);
				PSConfirmSleep(priv, (u16) Adapter->PSMode);
			    }
			    else {
				/* workaround for firmware sending deauth/linkloss event
					immediately after sleep request, remove this after
					firmware fixes it */
				Adapter->PSState = PS_STATE_AWAKE;
				printk("ignore PS_SleepConfirm in non-connected state\n");
			    }
			}
		}
#endif

		/* The PS state is changed during processing of Sleep Request event */
		if ((priv->adapter->PSState == PS_STATE_SLEEP)
#ifdef PS_PRESLEEP
			|| (priv->adapter->PSState == PS_STATE_PRE_SLEEP)
#endif
		) {
			continue;
		}
#endif

		/* Execute the next command */
		if (!priv->wlan_dev.dnld_sent)
			ExecuteNextCommand(priv);

#ifdef THROUGHPUT_TEST
		if (!priv->wlan_dev.dnld_sent && 
				(Adapter->ThruputTest & 0x01)) {
			sbi_host_to_card(priv, MVMS_DAT, G_buf, 1530);
			Adapter->NumTransfersTx += 1;
		}
#endif /* THROUGHPUT_TEST */
#ifdef WMM
		if (Adapter->wmm.enabled) {
			if (!wmm_lists_empty(priv)) {
#ifdef WMM_UAPSD
			if ((Adapter->PSState == PS_STATE_FULL_POWER) || (Adapter->wmm.no_more_packet == 0))
#endif
				wmm_process_tx(priv);
			}
		} else {
#endif /* WMM */
			if (!priv->wlan_dev.dnld_sent && Adapter->CurrentTxSkb) {
				PRINTK1("Tx Download Ready! 0x%x\n",ireg);
				TXRX_DEBUG_GET_TIME(6);
				wlan_process_tx(priv);
				TXRX_DEBUG_GET_ALL(0x15, ireg, 0xff);
			}
#ifdef WMM
		}
#endif

		if (Adapter->HisRegCpy & HIS_CmdDnLdRdy) {
			PRINTK1("Command Download Ready 0x%x\n",ireg);
		}

		TXRX_DEBUG_GET_ALL(0x17, ireg, 0xff);
		TXRX_DEBUG_GET_ALL(0x18, ireg, 0xff);
	}

	wlan_deactivate_thread(thread);

	LEAVE();
	return 0;
}

/*
 * This is the entry point to the card , *1.Probe for the card *2.Get the 
 * priv strucutre allocated *3.Get the adapter structure initialized
 * 4.Initialize the Device 
 */
static wlan_private *wlan_add_card(void *card)
{
	struct net_device *dev = NULL;	/* TODO: warning on this */

	/*
	 * being used uninitialized 
	 */
	wlan_private   *priv = NULL;

	ENTER();

	/*
	 * TODO: Probe the card to see if this is the card we can handle 
	 */
	if (sbi_probe_card(card) < 0) {
		/*
		 * No device found Return ---exit the module 
		 */
		return NULL;
	}

	/*
	 * Allocate an Ethernet device and register it 
	 */
	if (!(dev = init_etherdev(dev, sizeof(wlan_private)))) {
		PRINTK(KERN_ERR "%s: %s", driver_name, errNoEtherDev);
		//goto err_freeadapter;
		return NULL;
	}

	priv = dev->priv;
	
	if (!(priv->adapter = kmalloc(sizeof(wlan_adapter), GFP_KERNEL))) {
		PRINTK(KERN_ERR "%s: Adapter %s", driver_name, errAllocation);
		goto err_freeadapter;
	}

	memset(priv->adapter, 0, sizeof(wlan_adapter));

	/*
	 * Added a semaphore for the TxPacket transfers 
	 */
	sema_init(&priv->iface.sem, 1);

	priv->wlan_dev.netdev = dev;
	priv->wlan_dev.card = card;
	wlanpriv = priv;

	SET_MODULE_OWNER(dev);

	/*
	 * Setup the OS Interface to our functions 
	 */
	dev->open = wlan_open;
	dev->hard_start_xmit = wlan_hard_start_xmit;
	dev->stop = wlan_stop;
	dev->do_ioctl = wlan_do_ioctl;
	dev->set_mac_address = wlan_set_mac_address;

#ifdef linux 
#ifdef _MAINSTONE
#define	WLAN_WATCHDOG_TIMEOUT	(HZ*2)
#else
#define	WLAN_WATCHDOG_TIMEOUT	5   /* 50 ms */
#endif /* _MAINSTONE*/
	

	dev->tx_timeout = wlan_tx_timeout;
	dev->get_stats = wlan_get_stats;
	dev->watchdog_timeo = WLAN_WATCHDOG_TIMEOUT;

#ifdef	WIRELESS_EXT
	dev->get_wireless_stats = wlan_get_wireless_stats;
	dev->wireless_handlers = (struct iw_handler_def *) &wlan_handler_def;
#endif
#endif  /* linux */	
	
	dev->features |= NETIF_F_DYNALLOC;
	dev->flags |= IFF_BROADCAST | IFF_MULTICAST;
	dev->set_multicast_list = wlan_set_multicast_list;

#ifdef MANF_CMD_SUPPORT
	/* Required for the mfg command */
	init_waitqueue_head(&priv->adapter->mfg_cmd_q);
#endif
    
	/* TODO: Kill this with the new kernel */
	init_waitqueue_head(&priv->adapter->scan_q);
	init_waitqueue_head(&priv->adapter->ds_awake_q);

#ifdef CAL_DATA	
	init_waitqueue_head(&priv->adapter->wait_cal_dataResponse);
#endif

#ifdef CONFIG_PM
  	if(!(wlan_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, wlan_pm_callback)))
		printk("Failed to register PM callback\n");
#endif

 	INIT_LIST_HEAD(&priv->adapter->CmdFreeQ);
        INIT_LIST_HEAD(&priv->adapter->CmdPendingQ);

	PRINTK("Starting kthread...\n");
	priv->MainThread.priv = priv;
	wlan_create_thread(wlan_service_main_thread, &priv->MainThread,
							"wlan_main_service");

	ConfigureThreadPriority();	//required for threadx

	priv->ReassocThread.priv = priv;
	wlan_create_thread(wlan_reassociation_thread, &priv->ReassocThread,
							"wlan_reassoc_service");
#ifdef WPRM_DRV
        WPRM_DRV_TRACING_PRINT();
        /* init Traffic meter thread */
        tmThread.priv = priv;
        wlan_create_thread(wprm_traffic_meter_service_thread, &tmThread, "wlan_traffic_meter");
#endif

	/*
	 * Register the device. Fillup the private data structure with
	 * relevant information from the card and request for the required
	 * IRQ. 
	 */

	if (sbi_register_dev(priv) < 0) {
		PRINTK1(KERN_ERR "Failed to register wlan device!\n");
		goto err_unregisternet;
	}

#ifdef linux
	printk(KERN_INFO "%s: Marvell Wlan 802.11 Adapter "
			"revision 0x%02X at IRQ %i\n", dev->name,
			priv->adapter->chip_rev, dev->irq);

	wlan_proc_entry(priv, dev);
#ifdef PROC_READ_FIRMWARE
	wlan_firmware_entry(priv, dev);
#endif		
#ifdef PROC_DEBUG
	wlan_debug_entry(priv, dev);
#endif
#endif

	/* Get the CIS Table */
	sbi_get_cis_info(priv);
	
	if (wlan_init_fw(priv)) {
		PRINTK1("Firmware Init Failed\n");
		sbi_unregister();
		wlan_proc_remove(priv);
		goto err_unregisterdev;
	}

	LEAVE();
	return priv;

err_unregisterdev:
	sbi_unregister_dev(priv);
err_unregisternet:
err_freeadapter:
	unregister_netdev(dev);
	/* Stop the thread servicing the interrupts */
	wake_up_interruptible(&priv->MainThread.waitQ);
	wlan_terminate_thread(&priv->MainThread);

	wake_up_interruptible(&priv->ReassocThread.waitQ);
	wlan_terminate_thread(&priv->ReassocThread);

#ifdef WPRM_DRV
        WPRM_DRV_TRACING_PRINT();
         /* remove traffic meter thread */
        wake_up_interruptible(&tmThread.waitQ);
        wlan_terminate_thread(&tmThread);
#endif

	kfree(priv->adapter);
	wlanpriv = NULL;

	LEAVE();
	return NULL;
}

/*
 * Remove the card , *Uninitialize all the stuff we allocated above 
 */
static int wlan_remove_card(void *card)
{
	wlan_private		*priv = wlanpriv;
	wlan_adapter		*Adapter = priv->adapter;
	struct net_device	*dev;
#ifdef WPA
	union	iwreq_data	wrqu;
#endif

#ifdef DEEP_SLEEP
#ifdef BULVERDE_SDIO
	int			ret;
#endif /* BULVERDE_SDIO */
#endif

	ENTER();

	dev = priv->wlan_dev.netdev;

	wake_up_interruptible(&Adapter->scan_q);
	wake_up_interruptible(&Adapter->ds_awake_q);

	if (Adapter->CurCmd) {
		PRINTK1("Wake up current command\n");
		wake_up_interruptible(&Adapter->CurCmd->cmdwait_q);
	}

	Adapter->CurCmd = NULL;
	
#ifdef DEEP_SLEEP
#ifdef BULVERDE_SDIO
	if (ps_sleep_confirmed){
		ps_sleep_confirmed = 0;
		ret = sbi_exit_deep_sleep(priv);
#ifdef PS_REQUIRED
		if (!ret) {
			Adapter->PSState = PS_STATE_FULL_POWER;
			netif_carrier_on(priv->wlan_dev.netdev);
		}
		else
			PRINTK("Deep Sleep Exit failed\n");
#endif
	}
#endif /* BULVERDE_SDIO */
#endif /* DEEP SLEEP */
	
#ifdef PS_REQUIRED
	if (Adapter->PSMode == Wlan802_11PowerModeMAX_PSP) {
		Adapter->PSMode = Wlan802_11PowerModeCAM;
		PSWakeup(priv, HostCmd_OPTION_WAITFORRSP);
	}
#endif
	
#ifdef DEEP_SLEEP
	if (Adapter->IsDeepSleep == TRUE){
		Adapter->IsDeepSleep = FALSE;
		sbi_exit_deep_sleep(priv);
	}
#endif //DEEP_SLEEP

#ifdef WPRM_DRV
    #ifdef BG_SCAN
	if(priv->adapter->bgScanConfig->Enable == TRUE) {
		wlan_bg_scan_enable(priv, FALSE);
	}
    #endif /* BG_SCAN */
#endif /*WPRM_DRV*/

	/* Need to send the event to the supplicant regarding the 
	 * removal of card 2004/11/08 MPS
	 */
#ifdef WPA
	memset(wrqu.ap_addr.sa_data, 0xaa, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
#ifdef linux
	wireless_send_event(priv->wlan_dev.netdev, SIOCGIWAP, &wrqu, NULL);
#endif
#endif

	/* Disable interrupts on the card as we cannot handle them after RESET */
	sbi_disable_host_int(priv, HIM_DISABLE);

	PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RESET,
			HostCmd_ACT_HALT, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);

	os_sched_timeout(200);

#ifdef CONFIG_PM
        pm_unregister(wlan_pm_dev);
#endif
	
	netif_carrier_off(dev);
	/* Flush all the packets upto the OS before stopping */
	wlan_send_rxskbQ(priv);
	/* Stop the thread servicing the interrupts */

	wake_up_interruptible(&priv->MainThread.waitQ);
	wlan_terminate_thread(&priv->MainThread);

	wake_up_interruptible(&priv->ReassocThread.waitQ);
	wlan_terminate_thread(&priv->ReassocThread);

#ifdef WPRM_DRV
        WPRM_DRV_TRACING_PRINT();
        /* remove traffic meter thread */
        wake_up_interruptible(&tmThread.waitQ);
        wlan_terminate_thread(&tmThread);
        /* deregister traffic meter timer */
	/* For the line above maybe initialize a new timer
         timer deregister should after this line*/
	wprm_traffic_meter_exit(priv);
        WPRM_DRV_TRACING_PRINT();
#endif

	Adapter->SurpriseRemoved = TRUE;

#ifdef linux
#ifdef PROC_DEBUG
  	wlan_debug_remove(priv);
#endif
#ifdef PROC_READ_FIRMWARE
	wlan_firmware_remove(priv);
#endif 	
	wlan_proc_remove(priv);
#endif
	
	PRINTK1("Netif Stop Queue\n");

	PRINTK1("unregester dev\n");
    	sbi_unregister_dev(priv);

	PRINTK1("Free Adapter\n");
    	wlan_free_adapter(priv);
    
	netif_stop_queue(dev);

	priv->wlan_dev.netdev = NULL;
	wlanpriv = NULL;

	/* Last reference is our one */
	PRINTK1("refcnt = %d\n", atomic_read(&dev->refcnt));
	
	/* There might be an incomplete stack
	 * allow it to return with error value
	 */
	os_schedule(10);
	
	PRINTK1("netdevice unregister\n");
	
	PRINTK1("netdev_finish_unregister: %s%s.\n", dev->name,
	       (dev->features & NETIF_F_DYNALLOC)?"":", old style");
	unregister_netdev(dev);

	PRINTK1("Unregister finish\n");

	LEAVE();
	return 0;
}

void wlan_interrupt(struct net_device *dev)
{
	wlan_private   *priv = dev->priv;

	ENTER1();

	PRINTK1("wlan_interrupt: IntCounter=%d\n",priv->adapter->IntCounter);
	priv->adapter->IntCounter++;

#ifdef HOST_WAKEUP
	priv->adapter->WakeupTries = 0;
#endif

#ifdef PS_REQUIRED
	if(priv->adapter->PSState == PS_STATE_SLEEP) {
#ifdef BULVERDE_SDIO
		ps_sleep_confirmed = 0;
#endif
		priv->adapter->PSState = PS_STATE_AWAKE;
		//	PRINTK("wlan_interrupt(WAKE UP)\n");
	}
#endif

#ifdef DEEP_SLEEP
	if (priv->adapter->IsDeepSleep == TRUE) {
		priv->adapter->IsDeepSleep = FALSE;
		printk("Interrupt received in DEEP SLEEP mode!\n");
		if (netif_queue_stopped(priv->wlan_dev.netdev))
			netif_wake_queue(priv->wlan_dev.netdev);
	}
#endif /* DEEP_SLEEP */

	wake_up_interruptible(&priv->MainThread.waitQ);

	LEAVE1();
}

int wlan_init_module(void)
{
	int            *wlan_ret;
	int             ret;

	ENTER();

	// TODO: Make the sbi_register return an int value
	wlan_ret = sbi_register(wlan_add_card, wlan_remove_card, NULL);

	if (wlan_ret == NULL) {
		PRINTK(KERN_NOTICE "Unable to register serial WLAN driver!\n");
		ret = -ENXIO;
		goto done;
	}

	ret = 0;

done:
	LEAVE();
	return ret;
}

void wlan_cleanup_module(void)
{
	ENTER();

	sbi_unregister();

	LEAVE();
}


module_init(wlan_init_module);
module_exit(wlan_cleanup_module);

MODULE_DESCRIPTION("Marvell SD8385 WLAN Driver");
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_LICENSE("GPL");
