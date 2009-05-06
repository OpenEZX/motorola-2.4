/*
 * File : wlan_main.c The main device driver file. 
 */

#ifdef	linux

#include	<linux/module.h>
#include	<linux/kernel.h>
#include	<linux/init.h>
#include	<linux/delay.h>
#include	<linux/sched.h>
#include	<linux/ioport.h>
#include	<linux/net.h>
#include	<linux/netdevice.h>
#include	<linux/wireless.h>
#include	<linux/rtnetlink.h>
#include	<net/iw_handler.h>
#include	<linux/etherdevice.h>

#include	<asm/uaccess.h>

#include	<net/arp.h>

#endif

#include	"os_headers.h"
#include	"wlan.h"
#include	"wlan_fw.h"
#include	"wlan_wext.h"

/* New Code to synchronize between IEEE Power save and PM*/
#ifdef CONFIG_MARVELL_PM
#include <linux/pm.h>
#endif

#ifdef PS_REQUIRED
#ifdef BULVERDE
extern int ps_sleep_confirmed;
#endif
#endif 

#ifdef TXRX_DEBUG
int tx_n = 0;
unsigned long tt[7][16];
struct TRD trd[TRD_N];
int trd_n = 0;
int trd_p = 0;
#endif

#ifdef CONFIG_MARVELL_PM
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

/* The following init data is from cisco: CISCO Aironet Access Point SOftware Configuration Guide, Appendix B*/
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
			{240, 4920, WLAN_TX_PWR_JP_DEFAULT}, 
			{244, 4940, WLAN_TX_PWR_JP_DEFAULT}, 
			{248, 4960, WLAN_TX_PWR_JP_DEFAULT}, 
			{252, 4980, WLAN_TX_PWR_JP_DEFAULT} 
};
#endif /* MULTI BANDS */

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

u8	AdhocRates_G[4] =
		{ 0x82, 0x84, 0x8b, 0x96};

#ifdef OMAP1510_TIMER_DEBUG
extern unsigned long times[20][10];
extern int tm_ind[10];
#endif /* OMAP1510_TIMER_DEBUG */

static wlan_private	*wlan_add_card(void *card);
static int      wlan_remove_card(void *card);

//static void     free_private(wlan_private * priv);

int wlan_process_rx_command(wlan_private * priv);
void wlan_process_tx(wlan_private * priv);

char           *driver_name = "marvell";

wlan_private   *wlanpriv = NULL;

const char     *errRequestMem = "request_mem_region failed!";
const char     *errAllocation = "memory allocation failed!";
const char     *errNoEtherDev = "cannot initialize ethernet device!";


int wlan_set_regiontable(wlan_private *priv, u8 region, u8 band)
{
	wlan_adapter	*Adapter = priv->adapter;
	int		size = sizeof(CHANNEL_FREQ_POWER);
	int		i = 0;

	ENTER();
	
	memset(Adapter->region_channel, 0, sizeof(Adapter->region_channel));

#ifdef MULTI_BANDS
	if (band & (BAND_B | BAND_G)) 
#endif
	{
		switch (region) {
				case 0x10:	/* USA FCC	*/
				case 0x20:	/* Canada IC	*/
					Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_US_BG) / size;
					Adapter->region_channel[i].CFP = 
						channel_freq_power_US_BG;
					break;

				case 0x30:	/* Europe ETSI	*/
					Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_EU_BG) / size;
					Adapter->region_channel[i].CFP = 
						channel_freq_power_EU_BG;
					break;

				case 0x31:	/* Spain	*/
					Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_SPN_BG) /size;
					Adapter->region_channel[i].CFP = 
						channel_freq_power_SPN_BG;
					break;

				case 0x32:	/* France	*/
					Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_FR_BG) / size;
					Adapter->region_channel[i].CFP = 
						channel_freq_power_FR_BG;
					break;

				case 0x40:	/* Japan	*/
					Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_JPN_BG) /size;
					Adapter->region_channel[i].CFP = 
						channel_freq_power_JPN_BG;
					break;

				default: 
					printk("wrong region code %#x\n",region);
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
		switch (region) {
			case 0x10:	/* USA FCC	*/
			case 0x20:	/* Canada IC	*/
			case 0x31:	/* Spain	*/
			case 0x32:	/* France	*/
				Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_A) / size;
				Adapter->region_channel[i].CFP = 
					channel_freq_power_A;
				break;

			case 0x30:	/* Europe ETSI	*/
				Adapter->region_channel[i].NrCFP =
					sizeof(channel_freq_power_EU_A) / size;
				Adapter->region_channel[i].CFP = 
					channel_freq_power_EU_A;
				break;

			case 0x40:	/* Japan	*/
				Adapter->region_channel[i].NrCFP =
				     sizeof(channel_freq_power_JPN_A) /	size;
				Adapter->region_channel[i].CFP = 
					channel_freq_power_JPN_A;
				break;

			default: 
				printk("wrong region code %#x\n",region);
				return -1;
		}
		Adapter->region_channel[i].Valid	= TRUE;
		Adapter->region_channel[i].Region	= region;
		Adapter->region_channel[i].Band		= BAND_A;
		i++;
	}
#endif
		
	LEAVE();
	return 0;
}

#ifdef ENABLE_802_11D
int wlan_set_domainreg(wlan_private *priv, u8 region, u8 band)
{
	wlan_adapter	*Adapter = priv->adapter;
	int		size = sizeof(CHANNEL_FREQ_POWER);
	int		i = 0;

	ENTER();
	
	memset(&Adapter->DomainReg, 0, sizeof(Adapter->DomainReg));

#ifdef MULTI_BANDS
	if (band & (BAND_B | BAND_G)) 
#endif
	{
		switch (region) {
				case 0x10:	/* USA FCC	*/
				case 0x20:	/* Canada IC	*/
					Adapter->DomainReg.NoOfSubband += 1;
					if ( region == 0x10 ) {
						memcpy(Adapter->DomainReg.CountryCode,"US ", 
						sizeof( Adapter->DomainReg.CountryCode) );
					}
					else {
						memcpy(Adapter->DomainReg.CountryCode,"CA ",
						sizeof( Adapter->DomainReg.CountryCode) );
					}

					Adapter->DomainReg.Subband[i].FirstChan = 
						channel_freq_power_US_BG[0].Channel;
					Adapter->DomainReg.Subband[i].NoOfChan =
					        sizeof(channel_freq_power_US_BG) / size;
					Adapter->DomainReg.Subband[i].MaxTxPwr = 
						channel_freq_power_US_BG[0].MaxTxPower;
					break;

				case 0x30:	/* Europe ETSI	*/
					Adapter->DomainReg.NoOfSubband = 1;
					memcpy(Adapter->DomainReg.CountryCode,"EU ",
						sizeof( Adapter->DomainReg.CountryCode) );
					Adapter->DomainReg.Subband[i].FirstChan = 
						channel_freq_power_EU_BG[0].Channel;
					Adapter->DomainReg.Subband[i].NoOfChan =
					        sizeof(channel_freq_power_EU_BG) / size;
					Adapter->DomainReg.Subband[i].MaxTxPwr = 
						channel_freq_power_EU_BG[0].MaxTxPower;
					break;

				case 0x31:	/* Spain	*/
					Adapter->DomainReg.NoOfSubband = 1;
					memcpy(Adapter->DomainReg.CountryCode,"ES ",
						sizeof( Adapter->DomainReg.CountryCode) );
					Adapter->DomainReg.Subband[i].FirstChan = 
						channel_freq_power_SPN_BG[0].Channel;
					Adapter->DomainReg.Subband[i].NoOfChan =
					        sizeof(channel_freq_power_SPN_BG) / size;
					Adapter->DomainReg.Subband[i].MaxTxPwr = 
						channel_freq_power_SPN_BG[0].MaxTxPower;
					break;

				case 0x32:	/* France	*/
					Adapter->DomainReg.NoOfSubband = 1;
					memcpy(Adapter->DomainReg.CountryCode,"FR ",
						sizeof( Adapter->DomainReg.CountryCode) );
					Adapter->DomainReg.Subband[i].FirstChan = 
						channel_freq_power_FR_BG[0].Channel;
					Adapter->DomainReg.Subband[i].NoOfChan =
					        sizeof(channel_freq_power_FR_BG) / size;
					Adapter->DomainReg.Subband[i].MaxTxPwr = 
						channel_freq_power_FR_BG[0].MaxTxPower;
					break;

				case 0x40:	/* Japan	*/
					Adapter->DomainReg.NoOfSubband = 1;
					memcpy(Adapter->DomainReg.CountryCode,"JP ",
						sizeof( Adapter->DomainReg.CountryCode) );
					Adapter->DomainReg.Subband[i].FirstChan = 
						channel_freq_power_JPN_BG[0].Channel;
					Adapter->DomainReg.Subband[i].NoOfChan =
					        sizeof(channel_freq_power_JPN_BG) / size;
					Adapter->DomainReg.Subband[i].MaxTxPwr = 
						channel_freq_power_JPN_BG[0].MaxTxPower;
					break;

				default: 
					printk("wrong region code %#x\n",region);
					return -1;
		}

		i++;
	}

#ifdef MULTI_BANDS
	if (band & BAND_A) {
		switch (region) {
			case 0x10:	/* USA FCC	*/
			case 0x20:	/* Canada IC	*/
			case 0x31:	/* Spain	*/
			case 0x32:	/* France	*/
				if ( region == 0x10 ) {
					memcpy( Adapter->DomainReg.CountryCode,"US ",
						sizeof( Adapter->DomainReg.CountryCode) );
				}
				
				if ( region == 0x20 ) {
					memcpy( Adapter->DomainReg.CountryCode,"CA ",
						sizeof( Adapter->DomainReg.CountryCode) );
				}

				if ( region == 0x31 ) {
					memcpy( Adapter->DomainReg.CountryCode,"ES ",
						sizeof( Adapter->DomainReg.CountryCode) );
				}

				if ( region == 0x32 ) {
					memcpy( Adapter->DomainReg.CountryCode,"FR ",
						sizeof( Adapter->DomainReg.CountryCode) );
				}
				Adapter->DomainReg.NoOfSubband += 1 ;
				Adapter->DomainReg.Subband[i].FirstChan = 
					channel_freq_power_A[0].Channel;
				Adapter->DomainReg.Subband[i].NoOfChan =
				        sizeof(channel_freq_power_A) / size;
				Adapter->DomainReg.Subband[i].MaxTxPwr = 
					channel_freq_power_A[0].MaxTxPower;
				break;

			case 0x30:	/* Europe ETSI	*/
				Adapter->DomainReg.NoOfSubband += 1 ;
				memcpy(Adapter->DomainReg.CountryCode,"EU ",
					sizeof( Adapter->DomainReg.CountryCode) );
				Adapter->DomainReg.Subband[i].FirstChan = 
					channel_freq_power_EU_A[0].Channel;
				Adapter->DomainReg.Subband[i].NoOfChan =
				        sizeof(channel_freq_power_EU_A) / size;
				Adapter->DomainReg.Subband[i].MaxTxPwr = 
					channel_freq_power_EU_A[0].MaxTxPower;
				break;

			case 0x40:	/* Japan	*/
				Adapter->DomainReg.NoOfSubband += 1 ;
				memcpy(Adapter->DomainReg.CountryCode,"JP ",
					sizeof( Adapter->DomainReg.CountryCode) );
				Adapter->DomainReg.Subband[i].FirstChan = 
					channel_freq_power_JPN_A[0].Channel;
				Adapter->DomainReg.Subband[i].NoOfChan =
				        sizeof(channel_freq_power_JPN_A) / size;
				Adapter->DomainReg.Subband[i].MaxTxPwr = 
					channel_freq_power_JPN_A[0].MaxTxPower;
				break;

			default: 
				printk("wrong region code %#x\n",region);
				return -1;
		}
	}
#endif
		
	LEAVE();
	return 0;
}

#endif

#ifdef CONFIG_MARVELL_PM
extern int SetDeepSleep(wlan_private *priv, BOOLEAN bDeepSleep);

/* Linux Power Management */
static int wlan_pm_callback(struct pm_dev *pmdev, pm_request_t pmreq,
			    void *pmdata)
{
	wlan_private 		*priv = wlanpriv;
	wlan_adapter 		*Adapter = priv->adapter;
	struct net_device 	*dev = priv->wlan_dev.netdev;

	switch (pmreq) {
		case PM_SUSPEND:
			/*
			 * Check for Association, if  associated return -1
			 * indicating NOT to put the host processor to sleep
			 */
		
			if ((Adapter->MediaConnectStatus == 
				WlanMediaStateConnected) && 
					(Adapter->DeepSleep_State != 
						DEEP_SLEEP_USER_ENABLED))
				return -1;

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
			if (Adapter->DeepSleep_State == DEEP_SLEEP_DISABLED) {
#if defined(DEEP_SLEEP_CMD)
				SetDeepSleep(priv, TRUE);
#else
				sbi_enter_deep_sleep(priv);
#endif
				Adapter->DeepSleep_State =
							DEEP_SLEEP_OS_ENABLED;
			}
			
			if (Adapter->PM_State == PM_DISABLED) {
				sbi_suspend(priv);
				Adapter->PM_State = PM_ENABLED;
			}

			break;

		case PM_RESUME:
			/*
			 * Bring the inteface up first 
			 * This case should not happen still ...
			 */
			if (Adapter->PM_State == PM_ENABLED) {
				sbi_resume(priv);
				Adapter->PM_State = PM_DISABLED;
			}

			if (Adapter->DeepSleep_State == DEEP_SLEEP_OS_ENABLED) {
#if defined(DEEP_SLEEP_CMD)
				SetDeepSleep(priv, FALSE);
#else
				sbi_exit_deep_sleep(priv);
#endif
				Adapter->DeepSleep_State = DEEP_SLEEP_DISABLED;
			}
		      
			if (netif_running(dev))
				netif_device_attach(dev);

			break;
	}

	return 0;
}
#endif /* CONFIG_MARVELL_PM */

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
#ifdef OMAP1510_TIMER_DEBUG
	    int            i;
#endif /* OMAP1510_TIMER_DEBUG */

    	ENTER();
    
	if (priv->adapter->SurpriseRemoved) {
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

	if (priv->adapter->MediaConnectStatus != WlanMediaStateConnected) {
		ret = 1;
		goto done;
	}	

#ifdef PS_REQUIRED
	if (priv->adapter->PSState == PS_STATE_SLEEP) {
		PRINTK1("hard_start_xmit() in sleep\n");
		ret = 1;
		goto done;
	}
#endif

#ifdef DEEP_SLEEP
	if (priv->adapter->IsDeepSleep == TRUE) {
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
		ret = 1;
		goto done;
	} else {
		UpdateTransStart(dev);
		netif_stop_queue(dev);
	}

	ret = 0;
done:
	LEAVE();
	return ret;
}

#ifdef	linux	
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
	wake_up_interruptible(&priv->intThread.waitQ);
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

	priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
	dev->trans_start = jiffies;
	if (priv->adapter->CurrentTxSkb) {
		wake_up_interruptible(&priv->intThread.waitQ);
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
	wlan_adapter   *Adapter = priv->adapter;
	
	/*
	 * detailed rx_errors: 
	 */
	priv->stats.rx_crc_errors = Adapter->wlan802_3Stat.RcvCRCError;
	priv->stats.rx_fifo_errors = Adapter->wlan802_3Stat.RcvNoBuffer;

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
		HostCmd_ACT_SET, HostCmd_OPTION_USE_INT, 0,
		HostCmd_PENDING_ON_NONE, NULL);

	if (ret) {
		LEAVE();
		return ret;
	}

	wlan_sched_timeout(200);

	/* This is not necessary for production code */
	PRINTK1("Get MAC Address\n");
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_MAC_ADDRESS,
		HostCmd_ACT_GET, HostCmd_OPTION_USE_INT, 0,
		HostCmd_PENDING_ON_NONE, NULL);

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
		/*
		 * go to sleep slowly to take care of races 
		 */
		add_wait_queue(&thread->waitQ, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		TX_DISABLE;	/* required for threadx */
		
		if (!Adapter->EventCounter) {
			schedule();
			TX_DISABLE;	/* required for threadx */
		}

		set_current_state(TASK_RUNNING);
		remove_wait_queue(&thread->waitQ, &wait);

		Adapter->EventCounter = 0;

		if (thread->state == WLAN_THREAD_STOPPED)
			break;

#ifdef PS_REQUIRED
#ifdef BULVERDE
		if (Adapter->PSState == PS_STATE_SLEEP)
			continue;
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
				
					wlan_sched_timeout(2000);

#ifdef PS_REQUIRED
					if (Adapter->PSState != 
							PS_STATE_FULL_POWER)
						PSWakeup(priv);
#endif

				} else if (Adapter->InfrastructureMode == 
						Wlan802_11IBSS) {
					PRINTK1("MISC - Do a recover join" 
							" command \n");
					PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_AD_HOC_JOIN,
						0, HostCmd_OPTION_USE_INT,
						0, HostCmd_PENDING_ON_CMD,
						&Adapter->PreviousSSID);

					wlan_sched_timeout(2000);
				}
			} else { 
				/* No handling for infrastructure mode */
				if (Adapter->InfrastructureMode == 
							Wlan802_11IBSS) {
					PrepareAndSendCommand(priv,
						HostCmd_CMD_802_11_AD_HOC_START,
						0,HostCmd_OPTION_USE_INT,
						0, HostCmd_PENDING_ON_CMD,
						&Adapter->PreviousSSID);

					wlan_sched_timeout(2000);
				}
			}
		}

		if (Adapter->MediaConnectStatus == WlanMediaStateDisconnected) {
			PRINTK1("Restarting Re-association Timer @ %lu\n",
								os_time_get());
			Adapter->TimerIsSet = TRUE;
			ModTimer(&Adapter->MrvDrvTimer, 10000);
		}
	}

	wlan_deactivate_thread(thread);

	LEAVE();
	return 0;
}

int wlan_service_main_thread(void *data)
{
	wlan_thread	*thread = data;
	wlan_private	*priv = thread->priv;
	wlan_adapter	*Adapter = priv->adapter;
	wlan_dev	*wlan_dev = &priv->wlan_dev;
	wait_queue_t    wait;
	u8              ireg = 0;
	OS_INTERRUPT_SAVE_AREA;	/* Required for ThreadX */

	ENTER();

	wlan_activate_thread(thread);
	
	init_waitqueue_entry(&wait, current);

	for (;;) {
		/* Go to sleep slowly to take care of races */
		TXRX_DEBUG_GET_ALL(0x10, 0xff, 0xff);
		PRINTK1("Interrupt thread 111: IntCounter=%d "
			"CurrentTxSkb=%p dnld_sent=%d\n", Adapter->IntCounter, 
			Adapter->CurrentTxSkb, priv->wlan_dev.dnld_sent);

		add_wait_queue(&thread->waitQ, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		TXRX_DEBUG_GET_ALL(0x11, 0xff, 0xff);

		TX_DISABLE;	/* required for threadx */
		
		if (!Adapter->IntCounter 
			&& (priv->wlan_dev.dnld_sent || !Adapter->CurrentTxSkb)
			&& (priv->wlan_dev.dnld_sent || Adapter->CurCmd ||
				list_empty(&Adapter->CmdPendingQ))) {
			PRINTK1("No Interrupts to service,"
					"Scheduling out...\n");
			TX_RESTORE;	/* required for threadx */
			schedule();
		} else {
			TX_RESTORE;	/* required for threadx */
		}

		PRINTK1("Interrupt thread 222: IntCounter=%d CurrentTxSkb=%p "
				"dnld_sent=%d\n", Adapter->IntCounter, 
				Adapter->CurrentTxSkb,priv->wlan_dev.dnld_sent);

		TXRX_DEBUG_GET_ALL(0x12, 0xff, 0xff);
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&thread->waitQ, &wait);

		TXRX_DEBUG_GET_ALL(0x13, 0xff, 0xff);
		PRINTK1("Interrupt thread 333: IntCounter=%d CurrentTxSkb=%p "
				"dnld_sent=%d\n", Adapter->IntCounter, 
				Adapter->CurrentTxSkb,priv->wlan_dev.dnld_sent);

		PRINTK1("Main thread got card interrupt to service!\n");

		if ((thread->state == WLAN_THREAD_STOPPED) || 
					Adapter->SurpriseRemoved)
			break;

#ifdef PS_REQUIRED
#ifdef BULVERDE
		/* Here 'IntCounter' is decremented. 
		 * This is done to increase the CPU idle time from 30% to 95%
		 * when the card is put to power save mode on BULVERDE. */
		if (Adapter->PSState == PS_STATE_SLEEP) {
			cli();
			if (Adapter->IntCounter > 0)
				--Adapter->IntCounter;
			sti();
			continue;
		}
#endif
#endif

		if (Adapter->IntCounter) {
			cli();
			Adapter->IntCounter = 0;
			sti();
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
		else if (Adapter->PSState != PS_STATE_FULL_POWER && 
				Adapter->bHostWakeupDevRequired) {
#ifdef DEEP_SLEEP
			/* we borrow deep_sleep wakeup code for time being */
			if(sbi_exit_deep_sleep(priv))
				printk("Host Wakeup Dev: wakeup failed\n");
			else
				printk("Host Wakeup Dev: DONE, "
					"waiting for Host_awake_event\n");
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

			cli();
			Adapter->HisRegCpy &= ~HIS_CmdUpLdRdy;
			sti();

			/* SDCF: Move this into the process_rx function. */
			if (sbi_card_to_host(priv, MVMS_CMD, 
					&wlan_dev->upld_len,
					wlan_dev->upld_buf, 
					WLAN_UPLD_SIZE) < 0) {
				PRINTK1("ERROR: CmdResponse Transfer failed\n");
				continue;
			}
			/* TODO: Should use a spinlock to be SNMP safe */

			wlan_process_rx_command(priv);
		}

		/* Any received data? */
		if (Adapter->HisRegCpy & HIS_RxUpLdRdy) {
			PRINTK1("Rx Packet ready! 0x%x\n",ireg);
			TXRX_DEBUG_GET_ALL(0x16, ireg, 0xff);
			
			cli();
			Adapter->HisRegCpy &= ~HIS_RxUpLdRdy;
			sti();

			/* SDCF: Move this into ProcessRxedPacket */
			if (sbi_card_to_host(priv, MVMS_DAT, 
					&wlan_dev->upld_len, 
					wlan_dev->upld_buf, 
						WLAN_UPLD_SIZE) < 0) {
				PRINTK1("ERROR: Data Transfer from"
							"device failed\n");
				continue;
			}
			
			/* No need to clear */
			PRINTK1("Data Transfer from device ok\n");

#ifndef THROUGHPUT_TEST
 			ProcessRxedPacket(priv, (RxPD *) wlan_dev->upld_buf);
#else
			Adapter->NumTransfersRx += 1;
#endif /* THROUGHPUT_TEST */
		}

		/* Any Card Event */
		if (Adapter->HisRegCpy & HIS_CardEvent) {
			PRINTK1("Card Event Activity 0x%x\n",ireg);

			cli();
			Adapter->HisRegCpy &= ~HIS_CardEvent;
			sti();

			if (sbi_read_event_cause(priv)) {
				continue;
			}
			wlan_process_event(priv);
			PRINTK1("Clearing CardEvent INT\n");
		}

		/* Execute the next command */
		ExecuteNextCommand(priv);

#ifdef THROUGHPUT_TEST
		if (!priv->wlan_dev.dnld_sent && 
				(Adapter->ThruputTest & 0x01)) {
			sbi_host_to_card(priv, MVMS_DAT, G_buf, 1530);
			Adapter->NumTransfersTx += 1;
		}
#endif /* THROUGHPUT_TEST */

		if (!priv->wlan_dev.dnld_sent && Adapter->CurrentTxSkb) {
			PRINTK1("Tx Download Ready! 0x%x\n",ireg);
			TXRX_DEBUG_GET_TIME(6);
			wlan_process_tx(priv);
			TXRX_DEBUG_GET_ALL(0x15, ireg, 0xff);
		}

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
		goto err_freeadapter;
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
	dev->tx_timeout = wlan_tx_timeout;
	dev->get_stats = wlan_get_stats;
	dev->watchdog_timeo = (2 * HZ);	// 2s worst time
	// in PS mode

#ifdef	WIRELESS_EXT
	dev->get_wireless_stats = wlan_get_wireless_stats;
	dev->wireless_handlers = (struct iw_handler_def *) &wlan_handler_def;
#endif
#endif  /* linux */	

	dev->flags |= IFF_BROADCAST | IFF_MULTICAST;
	dev->set_multicast_list = wlan_set_multicast_list;

#ifdef MFG_CMD_SUPPORT
	/* Required for the mfg command */
	init_waitqueue_head(&priv->adapter->mfg_cmd_q);
#endif
    
	/* TODO: Kill this with the new kernel */
	init_waitqueue_head(&priv->adapter->scan_q);

	init_waitqueue_head(&priv->adapter->association_q);

#ifdef CAL_DATA	
	init_waitqueue_head(&priv->adapter->wait_cal_dataResponse);
#endif

#ifdef CONFIG_MARVELL_PM
  	if(!(wlan_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, wlan_pm_callback)))
		printk("Failed to register PM callback\n");
#endif

 	INIT_LIST_HEAD(&priv->adapter->CmdFreeQ);
        INIT_LIST_HEAD(&priv->adapter->CmdPendingQ);

	PRINTK("Starting kthread...\n");
	priv->intThread.priv = priv;
	wlan_create_thread(wlan_service_main_thread, &priv->intThread,
		"wlan_service");

	ConfigureThreadPriority();	//required for threadx

	priv->eventThread.priv = priv;
	wlan_create_thread(wlan_reassociation_thread, &priv->eventThread,
		"wlan_event");

	/*
	 * Register the device. Fillup the private data structure with
	 * relevant information from the card and request for the required
	 * IRQ. 
	 */

	if (sbi_register_dev(priv) < 0) {
		PRINTK1(KERN_ERR "Failed to register wlan device!\n");
		goto err_unregisternet;
	}

#ifdef	linux
	printk(KERN_INFO "%s: Marvell Wlan 802.11 Adapter "
			"revision 0x%02X at IRQ %i\n", dev->name,
			priv->adapter->chip_rev, dev->irq);

	wlan_proc_entry(priv, dev);
#endif

	/* Get the CIS Table */
	sbi_get_cis_info(priv);
	
	if (wlan_init_fw(priv)) {
		PRINTK1("Firmware Init Failed\n");
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
	wake_up_interruptible(&priv->intThread.waitQ);
	wlan_terminate_thread(&priv->intThread);

	wake_up_interruptible(&priv->eventThread.waitQ);
	wlan_terminate_thread(&priv->eventThread);

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
#ifdef BULVERDE
	int			ret;
#endif /* BULVERDE */
#endif

	ENTER();

	dev = priv->wlan_dev.netdev;

#ifdef DEEP_SLEEP
#ifdef BULVERDE
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
#endif /* BULVERDE */
#endif /* DEEP SLEEP */
	
#ifdef PS_REQUIRED
	if (Adapter->PSMode == Wlan802_11PowerModeMAX_PSP) {
		Adapter->PSMode = Wlan802_11PowerModeCAM;
		PSWakeup(priv);
		wlan_sched_timeout(200);
	}
#endif

	/* Need to send the event to the supplicant regarding the 
	 * removal of card 2004/11/08 MPS
	 */
#ifdef WPA
	memset(wrqu.ap_addr.sa_data, 0xaa, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
#ifdef linux
	wireless_send_event(priv->wlan_dev.netdev, RTM_DELLINK, &wrqu, NULL);	
#endif
#endif

	PrepareAndSendCommand(priv, HostCmd_CMD_802_11_RESET,
			HostCmd_ACT_HALT, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);

	wlan_sched_timeout(200);

#ifdef CONFIG_MARVELL_PM
        pm_unregister(wlan_pm_dev);
#endif
	
	/* Disable interrupts on the card */
	/* TODO: wlan_disable_host_int(priv, 0xff); */

	/* Stop the thread servicing the interrupts */

	wake_up_interruptible(&priv->intThread.waitQ);
	wlan_terminate_thread(&priv->intThread);

	wake_up_interruptible(&priv->eventThread.waitQ);
	wlan_terminate_thread(&priv->eventThread);

	Adapter->SurpriseRemoved = TRUE;

#ifdef	linux
	wlan_proc_remove("wlan");
#endif

    	sbi_unregister_dev(priv);

    	wlan_free_adapter(priv);
    
	netif_stop_queue(dev);

        unregister_netdev(dev);

	LEAVE();
	return 0;
}

void wlan_interrupt(struct net_device *dev)
{
	wlan_private   *priv = dev->priv;

	ENTER1();

	PRINTK1("wlan_interrupt: IntCounter=%d\n",priv->adapter->IntCounter);
	priv->adapter->IntCounter++;

#ifdef PS_REQUIRED
	if(priv->adapter->PSState == PS_STATE_SLEEP) {
#ifdef BULVERDE
		ps_sleep_confirmed = 0;
#endif
		priv->adapter->PSState = PS_STATE_AWAKE;
		//	PRINTK("wlan_interrupt(WAKE UP)\n");
	}
#endif

	wake_up_interruptible(&priv->intThread.waitQ);

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

MODULE_DESCRIPTION("Marvell WLAN SB83XX Driver");
MODULE_AUTHOR("Marvell Semiconductor Inc");
