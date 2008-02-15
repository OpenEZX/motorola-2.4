/*
 * File : wlanconfig.c
 * 
 * Program to configure addition paramters into the wlan driver
 * 
 * Usage:
 * 
 * wlanconfig <ethX> <cmd> [...] 
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
#include	<stdio.h>
#include	<unistd.h>
#include	<time.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<string.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<linux/if.h>
#include	<sys/ioctl.h>
#include	<linux/wireless.h>
#include	<linux/if_ether.h>

#include 	<linux/byteorder/swab.h>

#ifdef EXTSCAN
#include	"wlan_config.h"
#endif

#ifdef 	BYTE_SWAP
#define 	cpu_to_le16(x)	__swab16(x)
#else
#define		cpu_to_le16(x)	(x)
#endif


/* TODO These defenitions need to moved to one file, as these should 
 *  be visible to wlan_config.c
 */ 
#ifndef __ATTRIB_ALIGN__
#define __ATTRIB_ALIGN__ __attribute__((aligned(4)))
#endif

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__   __attribute__((packed))
#endif

/*
 *  ctype from older glib installations defines BIG_ENDIAN.  Check it 
 *   and undef it if necessary to correctly process the wlan header
 *   files
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)
#undef BIG_ENDIAN
#endif

typedef unsigned char	u8;
typedef unsigned short  u16;
typedef unsigned int	u32;

#include	"wlan_defs.h"
#include	"wlan_types.h"
#include	"host.h"
#ifdef WMM
#include	"wlan_wmm.h"
#endif
#include	"hostcmd.h"
#include	"wlan_wext.h"

#ifdef	DEBUG
#define	PRINTF		printf
#else
#define	PRINTF(...)
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif /* MIN */

enum COMMANDS {
	CMD_HOSTCMD,
	CMD_RDMAC,
	CMD_WRMAC,
	CMD_RDBBP,
	CMD_WRBBP,
	CMD_RDRF,
	CMD_WRRF,
	CMD_RDBCA,
	CMD_WRBCA,
	CMD_RDEEPROM,
	/* TODO: Move these as low-level driver ioctl calls */
	CMD_CMD52R,
	CMD_CMD52W,
	CMD_CMD53R,
	CMD_CMD53W,
#ifdef SUBSCRIBE_EVENT_CTRL
	CMD_SUB_EVENT,
#endif
#ifdef BG_SCAN
	CMD_BG_SCAN_CONFIG,
#endif
#ifdef WMM
	CMD_WMM_TSPEC,
	CMD_WMM_ACK_POLICY,
	CMD_WMM_AC_WPAIE,
#endif /* WMM */
#ifdef CAL_DATA	
	CMD_CAL_DATA,
	CMD_CAL_DATA_EXT,
#endif
	CMD_CFREGR,
	CMD_CFREGW,
	CMD_GETRATE,
	CMD_SLEEPPARAMS,
	CMD_BCA,
	CMD_REQUESTTPC,
	CMD_BCA_TS,
	CMD_SCAN_BSSID,
	CMD_SETADHOCCH,
	CMD_GETADHOCCH,
	CMD_REASSOCIATE,
#ifdef EXTSCAN	
	CMD_EXTSCAN,
	CMD_SCAN_LIST,
#endif	

#ifdef CIPHER_TEST
  CMD_CIPHER_TEST,
#endif
};

static char    *commands[] = {
	"hostcmd",
	"rdmac",
	"wrmac",
	"rdbbp",
	"wrbbp",
	"rdrf",
	"wrrf",
	"rdbca",
	"wrbca",
	"rdeeprom",
	"sdcmd52r",
	"sdcmd52w",
	"sdcmd53r",
	"sdcmd53w",
#ifdef SUBSCRIBE_EVENT_CTRL
	"subevent",
#endif
#ifdef BG_SCAN
	"bgscanconfig",
#endif
#ifdef WMM
	"wmmtspec",
	"wmm_ack_policy",
	"wmmparaie",
#endif /* WMM */
#ifdef CAL_DATA	
	"caldata",
	"caldataext",
#endif
	"rdcfreg",
	"wrcfreg",
	"getrate",
	"sleepparams",
	"bca",
	"requesttpc",
	"bca-ts",
	"scanbssid",
	"setadhocch",
	"getadhocch",
    	"reassociate",
#ifdef EXTSCAN	
	"extscan",
	"getscanlist",
#endif
#ifdef CIPHER_TEST
  	"cipher_test",
#endif
};

#define	MAX_COMMANDS	(sizeof(commands)/sizeof(commands[0]))

enum SUB_CMDS {
	SUBCMD_MODE,
	SUBCMD_ANTENNA,
	SUBCMD_BTFREQ,
	SUBCMD_TXPRIORITYLOW32,
	SUBCMD_TXPRIORITYHIGH32,
	SUBCMD_RXPRIORITYLOW32,
	SUBCMD_RXPRIORITYHIGH32,
};

static char    *sub_cmds[] = {
	"mode",
	"antenna",
	"btfreq",
	"txprioritylow32",
	"txpriorityhigh32",
	"rxprioritylow32",
	"rxpriorityhigh32",
};

#define	MAX_SUBCOMMANDS	(sizeof(sub_cmds)/sizeof(sub_cmds[0]))

static int process_host_cmd(int argc, char *argv[]);

#ifdef SUBSCRIBE_EVENT_CTRL
int sub_event_parse_action(EventSubscribe *EventData, int line, char *value);
int sub_event_parse_event(EventSubscribe *EventData, int line, char *value);
int sub_event_parse_RSSI(EventSubscribe *EventData, int line, char *value);
int sub_event_parse_SNR(EventSubscribe *EventData, int line, char *value);
int sub_event_parse_failcnt(EventSubscribe *EventData, int line, char *value);
int sub_event_parse_beacon_missed(EventSubscribe *EventData, int line, char *value);


static struct sub_event_fields {
  char *name;
  int (*parser)(EventSubscribe *EventData, int line, char *value);
} sub_event_fields[] = {
  { "Action", sub_event_parse_action },
  { "Event", sub_event_parse_event },
  { "RSSI", sub_event_parse_RSSI },
  { "SNR", sub_event_parse_SNR },
  { "FailureCount", sub_event_parse_failcnt },
  { "BcnMissed", sub_event_parse_beacon_missed },
};

#define NUM_SUB_EVENT_FIELDS (sizeof(sub_event_fields) / sizeof(sub_event_fields[0]))
#endif

#ifdef BG_SCAN
u16 TLVChanSize;
u16 TLVSsidSize;
u16 TLVProbeSize;
u16 TLVSnrSize;
u16 TLVBcProbeSize;
u16 TLVNumSsidProbeSize;
u16 ActualPos = sizeof(HostCmd_DS_802_11_BG_SCAN_CONFIG);
int bgscan_parse_action(u8 *CmdBuf, int line, char *value);
int bgscan_parse_enable(u8 *CmdBuf, int line, char *value);
int bgscan_parse_bsstype(u8 *CmdBuf, int line, char *value);
int bgscan_parse_channelsperscan(u8 *CmdBuf, int line, char *value);
int bgscan_parse_discardwhenfull(u8 *CmdBuf, int line, char *value);
int bgscan_parse_scaninterval(u8 *CmdBuf, int line, char *value);
int bgscan_parse_storecondition(u8 *CmdBuf, int line, char *value);
int bgscan_parse_reportconditions(u8 *CmdBuf, int line, char *value);
int bgscan_parse_maxscanresults(u8 *CmdBuf, int line, char *value);
int bgscan_parse_ssid(u8 *CmdBuf, int line, char *value);
int bgscan_parse_probes(u8 *CmdBuf, int line, char *value);
int bgscan_parse_channellist(u8 *CmdBuf, int line, char *value);
int bgscan_parse_snrthreshold(u8 *CmdBuf, int line, char *value);
int bgscan_parse_bcastprobe(u8 *CmdBuf, int line, char *value);
int bgscan_parse_numssidprobe(u8 *CmdBuf, int line, char *value);

static struct bgscan_fields {
  char *name;
  int (*parser)(u8 *CmdBuf, int line, char *value);
} bgscan_fields[] = {
  { "Action", bgscan_parse_action },
  { "Enable", bgscan_parse_enable },
  { "BssType", bgscan_parse_bsstype },
  { "ChannelsPerScan", bgscan_parse_channelsperscan },
  { "DiscardWhenFull", bgscan_parse_discardwhenfull },
  { "ScanInterval", bgscan_parse_scaninterval },
  { "StoreCondition", bgscan_parse_storecondition },
  { "ReportConditions", bgscan_parse_reportconditions },
  { "MaxScanResults", bgscan_parse_maxscanresults },
  { "SSID1", bgscan_parse_ssid },
  { "SSID2", bgscan_parse_ssid },
  { "SSID3", bgscan_parse_ssid },
  { "SSID4", bgscan_parse_ssid },
  { "SSID5", bgscan_parse_ssid },
  { "SSID6", bgscan_parse_ssid },
  { "Probes", bgscan_parse_probes },
  { "ChannelList", bgscan_parse_channellist },
  { "SnrThreshold", bgscan_parse_snrthreshold },
  { "BcastProbe", bgscan_parse_bcastprobe },
  { "NumSSIDProbe", bgscan_parse_numssidprobe },
};

#define NUM_BGSCAN_FIELDS (sizeof(bgscan_fields) / sizeof(bgscan_fields[0]))

#endif

void            display_usage(void);
int             findcommand(int maxcmds, char *cmds[], char *cmd);
int             process_read_register(int cmd, char *stroffset);
int             process_write_register(int cmd, char *stroffset, char *strvalue);
int             ascii2hex(unsigned char *d, char *s, uint dlen);
unsigned int    a2hex(char *s);
void            hexdump(char *prompt, void *p, int len, char delim);
int             process_sdcmd52r(int argc, char *argv[]);
int             process_sdcmd52w(int argc, char *argv[]);
int             process_sdcmd53r(void);
#ifdef CAL_DATA
int		process_cal_data(int argc, char *argv[]);
int		process_cal_data_ext(int argc, char *argv[]);
#endif

#ifdef SUBSCRIBE_EVENT_CTRL
int		process_event_subscribe(int argc, char *argv[]);
#endif

#ifdef BG_SCAN
int		process_bg_scan_config(int argc, char *argv[]);
#endif
int 		fparse_for_hex(FILE *fp, u8 *dst);
int 		fparse_for_cmd_and_hex(FILE *fp, u8 *dst, u8 *cmd);
int		process_adhocencryption_key(int cmd, char *setget, char *key); 

int 		process_write_cfreg(char *stroffset, char *strvalue);
int 		process_read_cfreg(char *stroffset);
int		process_read_eeprom(char *stroffset, char *strnob);

static int      sockfd;
static char     DevName[IFNAMSIZ + 1];

#define IW_MAX_PRIV_DEF		128
#define IW_PRIV_IOCTL_COUNT	(SIOCIWLASTPRIV-SIOCIWFIRSTPRIV+1)

static int	Priv_count;
static struct iw_priv_args	Priv_args[IW_MAX_PRIV_DEF];

#ifdef WMM
int process_wmm_ack_policy(int argc, char *argv[]);
int process_wmm_para_conf(int argc, char *argv[], int cmd);
#endif /* WMM */

static int get_private_info(const char *ifname)
{
	/* This function sends the SIOCGIWPRIV command which is
	 * handled by the kernel. and gets the total number of
	 * private ioctl's available in the host driver.
	 */
	struct iwreq iwr;
	int s, ret = 0;
	struct iw_priv_args *pPriv = Priv_args ;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		return -1;
	}

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) pPriv;
	iwr.u.data.length = IW_MAX_PRIV_DEF;
	iwr.u.data.flags = 0;
	      
	if (ioctl(s, SIOCGIWPRIV, &iwr) < 0) {
		perror("ioctl[SIOCGIWPRIV]");
		ret = -1;
	} else {
		/* Return the number of private ioctls */
		ret = iwr.u.data.length;
	}

	close(s);

	return ret;
}

/*
 * Add the private commands handled by the host driver 
 * it can be either subcommand or the main command
 */
static char	*priv_ioctl_names[] = {
		"getrate",
		"sleepparams",
		"bca",
		"bca-ts",
		"scanbssid",
		"requesttpc",
		"setadhocch",
		"getadhocch",
		"reassociate",
#ifdef EXTSCAN		
		"extscan",
		"getscanlist",
#endif
#ifdef CIPHER_TEST
    		"cipher_test",
#endif
		};

/* 
 * These two static array contains the main command number and the
 * subcommand number respectively
 */
static int PRIV_IOCTL[sizeof(priv_ioctl_names)/sizeof(priv_ioctl_names[0])];
static int PRIV_SUBIOCTL[sizeof(priv_ioctl_names)/sizeof(priv_ioctl_names[0])];

#define IOCTL_WLANGETRATE		    PRIV_IOCTL[0]
#define SUBIOCTL_WLANGETRATE		PRIV_SUBIOCTL[0]
#define IOCTL_WLANSLEEPPARAMS		PRIV_IOCTL[1]
#define SUBIOCTL_WLANSLEEPPARAMS	PRIV_SUBIOCTL[1]
#define IOCTL_WLANBCA			    PRIV_IOCTL[2]
#define SUBIOCTL_WLANBCA 		    PRIV_SUBIOCTL[2]
#define IOCTL_WLANBCA_TS		    PRIV_IOCTL[3]
#define SUBIOCTL_WLANBCA_TS 		PRIV_SUBIOCTL[3]
#define IOCTL_WLANSCAN_BSSID		PRIV_IOCTL[4]
#define SUBIOCTL_WLANSCAN_BSSID 	PRIV_SUBIOCTL[4]
#define IOCTL_WLANREQUESTTPC		PRIV_IOCTL[5]
#define SUBIOCTL_WLANREQUESTTPC 	PRIV_SUBIOCTL[5]
#define IOCTL_WLANSETADHOCCH		PRIV_IOCTL[6]
#define SUBIOCTL_WLANSETADHOCCH 	PRIV_SUBIOCTL[6]
#define IOCTL_WLANGETADHOCCH		PRIV_IOCTL[7]
#define SUBIOCTL_WLANGETADHOCCH 	PRIV_SUBIOCTL[7]
#define IOCTL_WLANREASSOCIATE		PRIV_IOCTL[8]
#define SUBIOCTL_WLANREASSOCIATE 	PRIV_SUBIOCTL[8]
#ifdef EXTSCAN
#define IOCTL_WLANEXTSCAN		    PRIV_IOCTL[9]
#define SUBIOCTL_WLANEXTSCAN	 	PRIV_SUBIOCTL[9]
#define IOCTL_WLANSCAN			    PRIV_IOCTL[10]
#define SUBIOCTL_WLANSCAN	 	    PRIV_SUBIOCTL[10]
#endif
#ifdef CIPHER_TEST
#define IOCTL_CIPHER_TEST			    PRIV_IOCTL[11]
#define SUBIOCTL_CIPHER_TEST 	    PRIV_SUBIOCTL[11]
#endif

int marvell_get_subioctl_no(int i, int *sub_cmd)
{
	int j;

	if (Priv_args[i].cmd >= SIOCDEVPRIVATE) {
		*sub_cmd = 0;
		return Priv_args[i].cmd;
	}

	j = -1;

	/* Find the matching *real* ioctl */

	while ((++j < Priv_count) && ((Priv_args[j].name[0] != '\0') ||
			(Priv_args[j].set_args != Priv_args[i].set_args) ||
			(Priv_args[j].get_args != Priv_args[i].get_args)));

	/* If not found... */
	if (j == Priv_count) {
		printf("%s: Invalid private ioctl definition for: 0x%x\n",
					DevName, Priv_args[i].cmd);
		return -1;
	}

	/* TODO handle the case of IW_PRIV_SIZE_FIXED */
	/* Save sub-ioctl number */
	*sub_cmd = Priv_args[i].cmd;

	/* Return the real IOCTL number */
	return Priv_args[j].cmd;
}

static int marvell_get_ioctl_no(const char *ifname, const char *priv_cmd, 
								int *sub_cmd)
{
	int	i;

	/* Are there any private ioctls? */
	if (Priv_count <= 0) {
		/* Could skip this message ? */
		printf("%-8.8s  no private ioctls.\n", ifname);
	} else {
		//printf("%-8.8s  Available private ioctl :\n", ifname);
		for (i = 0; i < Priv_count; i++) {
			if (Priv_args[i].name[0] &&
					!strcmp(Priv_args[i].name, priv_cmd)) {

				return marvell_get_subioctl_no(i, sub_cmd);
			}
		}
	}

	return -1;
}

void marvell_init_ioctl_numbers(const char *ifname)
{
	int	i;
	
	/* Read the private ioctls */
	Priv_count = get_private_info(ifname);

	for (i = 0; i < sizeof(priv_ioctl_names)/sizeof(priv_ioctl_names[0]);
								i++) {
		PRIV_IOCTL[i] = marvell_get_ioctl_no(ifname,
					priv_ioctl_names[i], &PRIV_SUBIOCTL[i]);
    }
/*
	if (IOCTL_WLANEXTSCAN < 0 && IOCTL_WLANSPECIFICSCAN >= SIOCDEVPRIVATE) {
		IOCTL_WLANEXTSCAN = IOCTL_WLANSPECIFICSCAN;
		SUBIOCTL_WLANEXTSCAN = SUBIOCTL_WLANSPECIFICSCAN;
	}
*/
}

#define WLAN_MAX_RATES	14
#define	GIGA		1e9
#define	MEGA		1e6
#define	KILO		1e3

int print_bitrate(double rate, int current, int fixed) 
{
	char	scale = 'k', buf[128];
	int	divisor = KILO;
	
	if (!current)
		rate *= 500000;

	if (rate >= GIGA) {
		scale = 'G';
		divisor = GIGA;
	} else if (rate >= MEGA) {
		scale = 'M';
		divisor = MEGA;
	}

	snprintf(buf, sizeof(buf), "%g %cb/s", rate/divisor, scale);

	if (current) {
		printf("\t  Current Bit Rate%c%s\n\n",
					(fixed) ? '=' : ':', buf);
	} else {
		printf("\t  %s\n", buf);
	}

	return 0;
}

int process_get_rate(void)
{
	unsigned char	bitrate[WLAN_MAX_RATES];
	struct iwreq	iwr;
	int		i = 0;
	
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) bitrate;
	iwr.u.data.length = sizeof(bitrate);
	
	if (IOCTL_WLANGETRATE <= 0) {
		return -EOPNOTSUPP;
	}
	
	if (SUBIOCTL_WLANGETRATE > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANGETRATE;
	}

	if (ioctl(sockfd, IOCTL_WLANGETRATE, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	printf("%-8.16s  %d available bit-rates :\n",
					DevName, iwr.u.data.length);
	
	for (i = 0 ;  i < iwr.u.data.length; i++) {
		print_bitrate(bitrate[i], 0, 0);
	}

	if (ioctl(sockfd, SIOCGIWRATE, &iwr)) {
		perror("wlanconfig");
		return -1;
	}

	print_bitrate(iwr.u.bitrate.value, 1, iwr.u.bitrate.fixed);
	
	return 0;
}

int ishexstring(char *s)
{
	int tmp, ret = -1;
	
	while(*s) {
		tmp = toupper(*s);
		if (tmp >= 'A' && tmp <= 'F') {
			ret = 0;
			break;
		}
		s++;
	}
	
	return ret;
}

int atoval(char *buf)
{
	if (!strncasecmp(buf, "0x", 2))
		return a2hex(buf+2);
	else if (!ishexstring(buf))
		return a2hex(buf);
	else 	
		return atoi(buf);
}

void display_sleep_params(wlan_ioctl_sleep_params_config *sp)
{
	printf("Sleep Params for %s:\n", sp->Action ? "set" : "get");
   	printf("----------------------------------------\n");
	printf("Error		: %u\n", sp->Error);
	printf("Offset		: %u\n", sp->Offset);
	printf("StableTime	: %u\n", sp->StableTime);
	printf("CalControl	: %u\n", sp->CalControl);
	printf("ExtSleepClk	: %u\n", sp->ExtSleepClk);
	printf("Reserved	: %u\n", sp->Reserved);
}

int process_sleep_params(int argc, char *argv[])
{
	struct iwreq			iwr;
	int				ret;
	wlan_ioctl_sleep_params_config 	sp;

	if (argc < 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 sleepparams get/set <p1>"
				" <p2> <p3> <p4> <p5> <p6>\n");
		exit(1);
	}

	if (IOCTL_WLANSLEEPPARAMS <= 0) {
		return -EOPNOTSUPP;
	}

	memset(&sp, 0, sizeof(wlan_ioctl_sleep_params_config));
	if (!strcmp(argv[3], "get")) {
		sp.Action = 0;
	} else if (!strncmp(argv[3], "set", 3)) {
		if (argc != 10) {
			printf("Error: invalid no of arguments\n");
			printf("Syntax: ./wlanconfig eth1 sleepparams get/set" 
					"<p1> <p2> <p3> <p4> <p5> <p6>\n");
			exit(1);
		}

		sp.Action = 1;
		if ((ret = atoval(argv[4])) < 0)
			return -EINVAL;
		sp.Error = (unsigned short) ret;
		if ((ret = atoval(argv[5])) < 0)
			return -EINVAL;
		sp.Offset = (unsigned short) ret;
		if ((ret = atoval(argv[6])) < 0)
			return -EINVAL;
		sp.StableTime = (unsigned short) ret;
		if ((ret = atoval(argv[7])) < 0)
			return -EINVAL;
		sp.CalControl = (unsigned char) ret;
		if ((ret = atoval(argv[8])) < 0)
			return -EINVAL;
		sp.ExtSleepClk = (unsigned char) ret;
		if ((ret = atoval(argv[9])) < 0)
			return -EINVAL;
		sp.Reserved = (unsigned short) ret;
	} else 	{
		return -EINVAL;
	}

	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANSLEEPPARAMS > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANSLEEPPARAMS;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) &sp;
	iwr.u.data.length = sizeof(wlan_ioctl_sleep_params_config);

	if (ioctl(sockfd, IOCTL_WLANSLEEPPARAMS, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	display_sleep_params(&sp);

	return 0;
}

void display_bca_params(wlan_ioctl_bca_params_config *bca)
{
	printf("BCA Params for %s:\n", bca->Action ? "set" : "get");
   	printf("----------------------------------------\n");
	printf("Mode		: %u\n", bca->Mode);	
	printf("Antenna		: %u\n", bca->Antenna);	
	printf("BtFreq		: %u\n", bca->BtFreq);	
	printf("TxPriorityLow32	: %u\n", bca->Txpl32);	
	printf("TxPriorityHigh32: %u\n", bca->Txph32);	
	printf("RxPriorityLow32	: %u\n", bca->Rxpl32);	
	printf("RxPriorityHigh32: %u\n", bca->Rxph32);	
}

int process_bca(int argc, char *argv[]) 
{
	struct iwreq			iwr;
	int				ret;
	wlan_ioctl_bca_params_config 	bca;

	if (argc < 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 bca get/set <p1>"
				" <p2> <p3> <p4> <p5> <p6> <p7>\n");
		exit(1);
	}

	if (IOCTL_WLANBCA <= 0) {
		return -EOPNOTSUPP;
	}

	memset(&bca, 0, sizeof(wlan_ioctl_bca_params_config));
	if (!strcmp(argv[3], "get")) {
		bca.Action = 0;
	} else if (!strncmp(argv[3], "set", 3)) {
		if (argc != 11) {
			printf("Error: invalid no of arguments\n");
			printf("Syntax: ./wlanconfig eth1 bca get/set" 
					"<p1> <p2> <p3> <p4> <p5> <p6> <p7>\n");
			exit(1);
		}

		bca.Action = 1;
		if ((ret = atoval(argv[4])) < 0)
			return -EINVAL;
		bca.Mode = (unsigned short) ret;
		if ((ret = atoval(argv[5])) < 0)
			return -EINVAL;
		bca.Antenna = (unsigned short) ret;
		if ((ret = atoval(argv[6])) < 0)
			return -EINVAL;
		bca.BtFreq = (unsigned short) ret;
		if ((ret = atoval(argv[7])) < 0)
			return -EINVAL;
		bca.Txpl32 = (unsigned char) ret;
		if ((ret = atoval(argv[8])) < 0)
			return -EINVAL;
		bca.Txph32 = (unsigned char) ret;
		if ((ret = atoval(argv[9])) < 0)
			return -EINVAL;
		bca.Rxpl32 = (unsigned short) ret;
		if ((ret = atoval(argv[10])) < 0)
			return -EINVAL;
		bca.Rxph32 = (unsigned short) ret;
	} else 	{
		return -EINVAL;
	}

	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANBCA > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANBCA;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) &bca;
	iwr.u.data.length = sizeof(wlan_ioctl_bca_params_config);

	if (ioctl(sockfd, IOCTL_WLANBCA, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	display_bca_params(&bca);

	return 0;
}

void display_bca_ts_params(wlan_ioctl_bca_timeshare_config *bca_ts)
{
	printf("BCA Time Share Params for %s:\n", bca_ts->Action?"set" : "get");
   	printf("----------------------------------------\n");
	printf("TrafficType		: %u\n", bca_ts->TrafficType);	
	printf("TimeShareInterval	: %u\n", bca_ts->TimeShareInterval);	
	printf("BTTime			: %u\n", bca_ts->BTTime);	
}

int process_bca_ts(int argc, char *argv[]) 
{
	int				ret, i;
	struct iwreq			iwr;
	wlan_ioctl_bca_timeshare_config	bca_ts;

	if (argc < 5) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 bca_ts get/set <p1>"
								" <p2> <p3>\n");
		exit(1);
	}

	if (IOCTL_WLANBCA_TS <= 0) {
		return -EOPNOTSUPP;
	}

	memset(&bca_ts, 0, sizeof(wlan_ioctl_bca_timeshare_config));
	
	if ((ret = atoval(argv[4])) < 0)
		return -EINVAL;
	if (ret > 1)
		return -EINVAL;
	bca_ts.TrafficType = (unsigned short) ret; // 0 or 1
	
	if (!strcmp(argv[3], "get")) {
		bca_ts.Action = 0;
	} else if (!strncmp(argv[3], "set", 3)) {
		if (argc != 7) {
			printf("Error: invalid no of arguments\n");
			printf("Syntax: ./wlanconfig eth1 bca_ts get/set" 
							" <p1> <p2> <p3>\n");
			exit(1);
		}

		bca_ts.Action = 1;
		
		if ((ret = atoval(argv[5])) < 0)
			return -EINVAL;
		/* If value is not multiple of 10 then take the floor value */
		i = ret % 10;
		ret -= i;
		/* Valid Range for TimeShareInterval: < 20 ... 60_000 > ms */
		if (ret < 20  || ret > 60000) {
			printf("Invalid TimeShareInterval Range:"
						" < 20 ... 60000 > ms\n");
			return -EINVAL;
		}
		bca_ts.TimeShareInterval = (unsigned int) ret;
		
		if ((ret = atoval(argv[6])) < 0)
			return -EINVAL;
		/* If value is not multiple of 10 then take the floor value */
		i = ret % 10;
		ret -= i;
		
		if (ret > bca_ts.TimeShareInterval) {
			printf("Invalid BTTime"
				"  Range: < 0 .. TimeShareInterval > ms\n");
			return -EINVAL;
		}
		bca_ts.BTTime = (unsigned int) ret;
	} else 	{
		return -EINVAL;
	}

	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANBCA_TS > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANBCA_TS;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) &bca_ts;
	iwr.u.data.length = sizeof(wlan_ioctl_bca_timeshare_config);

	if (ioctl(sockfd, IOCTL_WLANBCA_TS, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	display_bca_ts_params(&bca_ts);

	return 0;
}

/*void display_req_tpc_params(wlan_ioctl_tpc_request_params *tpc)
{
	printf("TPC Params :\n");
   	printf("----------------------------------------\n");
	printf("Mac Addr		: %x:%x:%x:%x:%x:%x\n", 
		tpc->mac[0],tpc->mac[1],tpc->mac[2],tpc->mac[3],tpc->mac[4],
		tpc->mac[5]);
	printf("time-out		: %u\n", tpc->timeout);	
	printf("index			: %u\n", tpc->index);	
	printf("TxPower			: %d\n", tpc->TxPower);	
	printf("LinkMargin		: %d\n", tpc->LinkMargin);	
	printf("RSSI			: %d\n", tpc->RSSI);	
}


int process_requesttpc(int argc, char *argv[]) 
{
	int				ret;
	unsigned char			mac[6];
	struct iwreq			iwr;
	wlan_ioctl_tpc_request_params 	tpc;

	if (argc != 6) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 requesttpc <p1> <p2> <p3>\n");
		exit(1);
	}

	if (IOCTL_WLANREQUESTTPC <= 0) {
		return -EOPNOTSUPP;
	}

	memset(&tpc, 0, sizeof(wlan_ioctl_tpc_request_params));
	memset(mac, 0, sizeof(mac));
	
	sscanf(argv[3], "%4x:%4x:%4x:%4x:%4x:%4x",
			(unsigned int *)&mac[0], 
			(unsigned int *)&mac[1], 
			(unsigned int *)&mac[2], 
			(unsigned int *)&mac[3], 
			(unsigned int *)&mac[4], 
			(unsigned int *)&mac[5]); 

	memcpy(&tpc.mac, mac, sizeof(mac));

	if ((ret = atoval(argv[4])) < 0)
		return -EINVAL;
	tpc.timeout = (unsigned short) ret;
	if ((ret = atoval(argv[5])) < 0)
		return -EINVAL;
	tpc.index = (unsigned char) ret;

	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANREQUESTTPC > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANREQUESTTPC;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) &tpc;
	iwr.u.data.length = sizeof(wlan_ioctl_tpc_request_params);

	if (ioctl(sockfd, IOCTL_WLANREQUESTTPC, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	display_req_tpc_params(&tpc);

	return 0;
}
Temporary COmmented out. Needs to fix the compileing issue*/

static int process_reassociation(int argc, char *argv[]) 
{
	wlan_ioctl_reassociation_info reassocInfo;
	struct iwreq			iwr;
    unsigned int mac[6];
    unsigned int idx;
    int numToks;

    /*
    ** Reassociation request is expected to be in the following format:
    **
    **  <xx:xx:xx:xx:xx:xx>  <yy:yy:yy:yy:yy:yy>  <ssid string>
    **
    **  where xx..xx is the current AP BSSID to be included in the reassoc req
    **        yy..yy is the desired AP to send the reassoc req to
    **        <ssid string> is the desired AP SSID to send the reassoc req to.
    **
    **  The current and desired AP BSSIDs are required.  
    **  The SSID string can be omitted if the desired BSSID is provided.
    **
    **  If we fail to find the desired BSSID, we attempt the SSID.
    **  If the desired BSSID is set to all 0's, the ssid string is used.
    **  
    */

    /* Verify the number of arguments is either 5 or 6 */
    if (argc != 5 && argc != 6)
    {
		fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

	memset(&iwr, 0, sizeof(iwr));
    memset(&reassocInfo, 0x00, sizeof(reassocInfo));

    /*
    **  Scan in and set the current AP BSSID
    */
	numToks = sscanf(argv[3], "%2x:%2x:%2x:%2x:%2x:%2x",
                     mac + 0, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);

    if (numToks != 6)
    {
		fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

    for (idx = 0; idx < 6; idx++)
    {
        reassocInfo.CurrentBSSID[idx] = (unsigned char)mac[idx];
    }


    /*
    **  Scan in and set the desired AP BSSID
    */
	numToks = sscanf(argv[4], "%2x:%2x:%2x:%2x:%2x:%2x",
                     mac + 0, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5);

    if (numToks != 6)
    {
		fprintf(stderr, "Invalid number of parameters!\n");
        return -EINVAL;
    }

    for (idx = 0; idx < 6; idx++)
    {
        reassocInfo.DesiredBSSID[idx] = (unsigned char)mac[idx];
    }

    /*
    ** If the ssid string is provided, save it; otherwise it is an empty string
    */
    if (argc == 6)
    {
        strcpy(reassocInfo.DesiredSSID, argv[5]);
    }

    /* 
    ** Set up and execute the ioctl call
    */
    iwr.u.data.flags = SUBIOCTL_WLANREASSOCIATE;
	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t)&reassocInfo;
	iwr.u.data.length = sizeof(reassocInfo);

	if (ioctl(sockfd, IOCTL_WLANREASSOCIATE, &iwr) < 0) {
		perror("wlanconfig: reassociate response");
		return -1;
	}

    /* No error return */
    return 0;
}


int process_scan_bssid(int argc, char *argv[]) 
{
	unsigned char			mac[6];
	struct iwreq			iwr;
	int ret;
	unsigned char 			extra;

	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 scan-bssid <MAC Address>\n");
		exit(1);
	}

	if (IOCTL_WLANSCAN_BSSID <= 0) {
		return -EOPNOTSUPP;
	}

	memset(mac, 0, sizeof(mac));

	/* strip all ':' */
	ret = sscanf(argv[3], "%2x:%2x:%2x:%2x:%2x:%2x%c",
			(unsigned int *)&mac[0], 
			(unsigned int *)&mac[1], 
			(unsigned int *)&mac[2], 
			(unsigned int *)&mac[3], 
			(unsigned int *)&mac[4], 
			(unsigned int *)&mac[5], &extra); 
	
	printf("Scan BSSID %02X:%02X:%02X:%02X:%02X:%02X\n", 
		 mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

	if( ret != 6 ) {
		printf("Wrong MAC address format!\n");
		return -EOPNOTSUPP;
	}
	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANSCAN_BSSID > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANSCAN_BSSID;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) &mac;
	iwr.u.data.length = sizeof(mac);

	if (ioctl(sockfd, IOCTL_WLANSCAN_BSSID, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	return 0;
}


int process_setadhocch(int argc, char *argv[]) 
{
	unsigned int	vals[2];
	struct iwreq	iwr;

	if (argc != 5) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: wlanconfig eth1 setadhocch <band> <channel>\n");
		exit(1);
	}

	if (IOCTL_WLANSETADHOCCH <= 0) {
		return -EOPNOTSUPP;
	}

	vals[0] = *argv[3];
	vals[1] = atoval(argv[4]);

	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANSETADHOCCH > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANSETADHOCCH;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t)vals;
	iwr.u.data.length = sizeof(vals);

	if (ioctl(sockfd, IOCTL_WLANSETADHOCCH, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	return 0;
}

int process_getadhocch(int argc, char *argv[]) 
{
	unsigned int	vals[2];
	struct iwreq	iwr;

	if (argc != 3) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: wlanconfig eth1 getadhocch\n");
		exit(1);
	}

	if (IOCTL_WLANGETADHOCCH <= 0) {
		return -EOPNOTSUPP;
	}

	memset(&iwr, 0, sizeof(iwr));
	
	if (SUBIOCTL_WLANGETADHOCCH > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANGETADHOCCH;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t)vals;
	iwr.u.data.length = sizeof(vals);

	if (ioctl(sockfd, IOCTL_WLANGETADHOCCH, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	printf("getadhocch: %c %d\n", vals[0], vals[1]);

	return 0;
}

#ifdef EXTSCAN
int process_extscan(int argc, char *argv[]) 
{
	struct iwreq			iwr;
	WCON_SSID 			Ssid;

	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 extscan <SSID>\n");
		exit(1);
	}

	if (IOCTL_WLANEXTSCAN <= 0) {
		return -EOPNOTSUPP;
	}

	printf("Ssid: %s\n", argv[3]);

	memset(&Ssid, 0, sizeof(Ssid));
	memset(&iwr, 0, sizeof(iwr));

	Ssid.ssid_len = strlen(argv[3]);
	memcpy(Ssid.ssid, argv[3], Ssid.ssid_len);

	if (SUBIOCTL_WLANEXTSCAN > 0) {
		iwr.u.data.flags = SUBIOCTL_WLANEXTSCAN;
	}

	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) &Ssid;
	iwr.u.data.length = sizeof(Ssid);

	if (ioctl(sockfd, IOCTL_WLANEXTSCAN, &iwr) < 0) {
		perror("wlanconfig");
		return -1;
	}

	return 0;
}

void parse_custom_info(WCON_HANDLE *pHandle, struct iw_point *data, int idx)
{
	int	i = 0;
	char 	*custom_cmd[] = { "wpa_ie", "rsn_ie", NULL };
	
	if (!data->pointer || !data->length) {
		printf("iw_point: Invalid Pointer/Length\n");
		return;
	}
	
	if (!strncmp(data->pointer, "wmm_ie", strlen("wmm_ie"))) {
			pHandle->ScanList[idx].Wmm = WCON_WMM_ENABLED;
	}

	while (custom_cmd[i]) {
		if (!strncmp(data->pointer, custom_cmd[i], 
					strlen(custom_cmd[i]))) {
			pHandle->ScanList[idx].WpaAP = WCON_WPA_ENABLED;
			break;
		}
		i++;
	}

	printf("Wpa:\t %s\n", pHandle->ScanList[idx].WpaAP ?
					"enabled" : "disabled");
	printf("Wmm:\t %s\n", pHandle->ScanList[idx].Wmm ?
					"enabled" : "disabled");
}

void parse_scan_info(WCON_HANDLE *pHandle, unsigned char buffer[], int length)
{
	int			len = 0;
	int			ap_index = -1;
	char			*mode[3] = {"auto", "ad-hoc", "infra"};
	struct iw_event		iwe;

	memset(pHandle->ScanList, 0, sizeof(pHandle->ScanList));
	pHandle->ApNum = 0;

	while (len + IW_EV_LCP_LEN < length) {
		memcpy((char *)&iwe, buffer + len, sizeof(struct iw_event));
		switch (iwe.cmd) {
		case SIOCGIWAP:
			ap_index++;
			memcpy(pHandle->ScanList[ap_index].Bssid, 
					iwe.u.ap_addr.sa_data, ETH_ALEN);
			printf("\nBSSID:\t %02X:%02X:%02X:%02X:%02X:%02X\n",
				HWA_ARG(pHandle->ScanList[ap_index].Bssid));
			break;
			
		case SIOCGIWESSID:
			iwe.u.essid.pointer = buffer + len + IW_EV_POINT_LEN;	
			if ((iwe.u.essid.pointer) && (iwe.u.essid.length)) {
				memcpy(pHandle->ScanList[ap_index].Ssid.ssid,
						(char *)iwe.u.essid.pointer,
						iwe.u.essid.length);
				pHandle->ScanList[ap_index].Ssid.ssid_len = 
							iwe.u.essid.length;
			}
			printf("SSID:\t %s\n",
				pHandle->ScanList[ap_index].Ssid.ssid);
			break;
		
		case SIOCGIWENCODE:
			if (!(iwe.u.data.flags & IW_ENCODE_DISABLED)) {
				pHandle->ScanList[ap_index].Privacy =
							WCON_ENC_ENABLED;
			}
			printf("Privacy: %s\n", 
					pHandle->ScanList[ap_index].Privacy ?
					"enabled": "disabled");
			break;

		case SIOCGIWMODE:
			pHandle->ScanList[ap_index].NetMode = iwe.u.mode;
			printf("NetMode: %s\n", 
				mode[pHandle->ScanList[ap_index].NetMode]);
			break;
	
#if WIRELESS_EXT > 14
		case IWEVCUSTOM:
			iwe.u.data.pointer = buffer + len + IW_EV_POINT_LEN;
			parse_custom_info(pHandle, &iwe.u.data, ap_index);
			break;
#endif

		case IWEVQUAL:
			pHandle->ScanList[ap_index].Rssi = iwe.u.qual.level;
			printf("Quality: %d\n", 
					pHandle->ScanList[ap_index].Rssi);
			break;
		}
		
		len += iwe.len;
	}
	
	pHandle->ApNum = ap_index + 1;
	printf("\nNo of AP's = %d\n", pHandle->ApNum);

	return;
}

int process_scan_results(int argc, char *argv[]) 
{
	unsigned char		buffer[IW_SCAN_MAX_DATA];
	struct iwreq		iwr;
	WCON_HANDLE		mhandle, *pHandle = &mhandle;

	memset(pHandle, 0, sizeof(WCON_HANDLE));
	memset(&iwr, 0, sizeof(struct iwreq));
	
	iwr.u.data.pointer = buffer;
	iwr.u.data.length = sizeof(buffer);
	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);

	if ((ioctl(sockfd, SIOCGIWSCAN, &iwr)) < 0) {
		printf("Get Scan Results Failed\n");
		return -1;
	}

	parse_scan_info(pHandle, buffer, iwr.u.data.length);

	return 0;
}
#endif

#ifdef CIPHER_TEST
int process_cipher_test(int argc, char *argv[])
{
  int n, i;
  struct iwreq iwr;
  HostCmd_DS_802_11_KEY_ENCRYPT cmd;

  memset(&cmd, 0, sizeof(cmd));

  ascii2hex(cmd.KeyEncKey,  argv[4], 32);
  ascii2hex(cmd.KeyIV,      argv[5], 32);

  cmd.EncType = 
    (strcmp(argv[3], "aes") == 0) ? CIPHER_TEST_AES: CIPHER_TEST_RC4;

  cmd.Action  = HostCmd_ACT_SET;

  for (i = 0; i < 512; i++)
  {
    n = getchar();

    if (n == EOF)
      break;

    cmd.KeyData[i] = n;    
  }
  
  cmd.KeyDataLen = i;

  strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
  iwr.u.data.pointer  = (caddr_t) &cmd;
  iwr.u.data.length   = sizeof(cmd);

  if (IOCTL_CIPHER_TEST <= 0)
    return -EOPNOTSUPP;

  if (SUBIOCTL_CIPHER_TEST > 0)
    iwr.u.data.flags = SUBIOCTL_CIPHER_TEST;

  if (ioctl(sockfd, IOCTL_CIPHER_TEST, &iwr) < 0)
    return -1;

  for(i = 0; i < cmd.KeyDataLen; i++)
    putchar(cmd.KeyData[i]);

  return 0;
}

#endif

int main(int argc, char *argv[])
{
	int             cmd;
	int		PrivateCmds;

	if (argc < 3) {
		fprintf(stderr, "Invalid number of parameters!\n");
		display_usage();
		exit(1);
	}

	strncpy(DevName, argv[1], IFNAMSIZ);

	/*
	 * create a socket 
	 */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "wlanconfig: Cannot open socket.\n");
		exit(1);
	}

	PrivateCmds = get_private_info(DevName);

	marvell_init_ioctl_numbers(DevName);

	switch ((cmd = findcommand(MAX_COMMANDS, commands, argv[2]))) {
	case CMD_HOSTCMD:
		process_host_cmd(argc, argv);
		break;
	case CMD_RDMAC:
	case CMD_RDBBP:
	case CMD_RDRF:
		if (argc < 4) {
			fprintf(stderr, 
				"Register offset required!\n");
			display_usage();
			exit(1);
		}

		if (process_read_register(cmd, argv[3])) {
			fprintf(stderr,
				"Read command failed!\n");
			exit(1);
		}
		break;
	case CMD_WRMAC:
	case CMD_WRBBP:
	case CMD_WRRF:
		if (argc < 5) {
			fprintf(stderr,
				"Register offset required &" 
					"value!\n");
			display_usage();
			exit(1);
		}
		if (process_write_register(cmd, argv[3],
					argv[4])) {
			fprintf(stderr,
				"Write command failed!\n");
			exit(1);
		}
		break;
	case CMD_RDBCA:
		//get_bca_config();
		break;
	case CMD_WRBCA:
		{
			int subcmd;

			switch ((subcmd =
				findcommand(MAX_SUBCOMMANDS,
						sub_cmds,
						argv[3]))) {
				case SUBCMD_MODE:
					break;
				default:
					fprintf(stderr,
					"Invalid sub-command" 
					"specified!\n");
					display_usage();
					exit(1);
			}
			break;
		}
	case CMD_CMD52R:
		process_sdcmd52r(argc,argv);
		break;
	case CMD_CMD52W:
		process_sdcmd52w(argc,argv);
		break;
	case CMD_CMD53R:
		process_sdcmd53r();
		break;
#ifdef SUBSCRIBE_EVENT_CTRL
	case CMD_SUB_EVENT:
		process_event_subscribe(argc, argv);
		break;
#endif

#ifdef BG_SCAN
	case CMD_BG_SCAN_CONFIG:
		process_bg_scan_config(argc, argv);
		break;
#endif /* BG_SCAN */
#ifdef WMM
	case CMD_WMM_ACK_POLICY:
		process_wmm_ack_policy(argc, argv);
		break;
	case CMD_WMM_TSPEC:
	case CMD_WMM_AC_WPAIE:
		process_wmm_para_conf(argc, argv, cmd);
		break;
#endif /* WMM */
#ifdef CAL_DATA
	case CMD_CAL_DATA:
		process_cal_data(argc, argv);
		break;
	case CMD_CAL_DATA_EXT:
		process_cal_data_ext(argc, argv); 
		break;
#endif
	case CMD_CFREGR:
		printf("process read cfreg\n");
		if (argc < 4) {
			fprintf(stderr,	"Register offset required!\n");
			display_usage();
			exit(1);
		}
		if (process_read_cfreg(argv[3])) {
			fprintf(stderr, "Read CF register failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_CFREGW:
		printf("process write cfreg\n");
		if (argc < 5) {
			fprintf(stderr,	"Register offset required!\n");
			display_usage();
			exit(1);
		}
		if (process_write_cfreg(argv[3], argv[4])) {
			fprintf(stderr, "Read CF register failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_RDEEPROM:
		printf("proces read eeprom\n");

		if(argc < 5) {
			fprintf(stderr, "Register offset, number of bytes required\n");
			display_usage();
			exit(1);
		}
		
		if(process_read_eeprom(argv[3], argv[4])) {
			fprintf(stderr, "EEPROM Read failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_GETRATE:
		if (process_get_rate()) {
			fprintf(stderr, "Get Rate Failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_SLEEPPARAMS:
		if (process_sleep_params(argc, argv)) {
			fprintf(stderr, "Sleep Params Failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_BCA:
		if (process_bca(argc, argv)) {
			fprintf(stderr, "BCA Params Failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_REQUESTTPC:
/*		if (process_requesttpc(argc, argv)) {
			fprintf(stderr, "Requesttpc Params Failed\n");
			display_usage();
			exit(1);
		}
*/
		break;
	case CMD_BCA_TS:
		if (process_bca_ts(argc, argv)) {
			fprintf(stderr, "SetBcaTs Failed\n");
			display_usage();
			exit(1);
		}
		break;
    case CMD_REASSOCIATE:
        if (process_reassociation(argc, argv))
        {
			exit(1);
        }
        break;
	case CMD_SCAN_BSSID:
		if (process_scan_bssid(argc, argv)) {
			fprintf(stderr, "ScanBssid Failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_SETADHOCCH:
		if (process_setadhocch(argc, argv)) {
			fprintf(stderr, "SetAdhocCh Failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_GETADHOCCH:
		if (process_getadhocch(argc, argv)) {
			fprintf(stderr, "GetAdhocCh Failed\n");
			display_usage();
			exit(1);
		}
		break;
#ifdef EXTSCAN		
	case CMD_EXTSCAN:
		if (process_extscan(argc, argv)) {
			fprintf(stderr, "ExtScan Failed\n");
			display_usage();
			exit(1);
		}
		break;
	case CMD_SCAN_LIST:
		if (process_scan_results(argc, argv)) {
			fprintf(stderr, "getscanlist Failed\n");
			display_usage();
			exit(1);
		}
		break;

#endif		

#ifdef CIPHER_TEST
	case CMD_CIPHER_TEST:
		if (process_cipher_test(argc, argv)) {
			fprintf(stderr, "cipher_test Failed\n");
			display_usage();
			exit(1);
		}
		break;
#endif

	default:
		fprintf(stderr, "Invalid command specified!\n");
		display_usage();
		exit(1);
	}

	return 0;
}

int process_read_eeprom(char *stroffset, char *strnob)
{
	char 			buffer[MAX_EEPROM_DATA];
	struct ifreq    	userdata;
	wlan_ioctl_regrdwr 	*reg = (wlan_ioctl_regrdwr *)buffer;

	memset(buffer, 0, sizeof(buffer));
	reg->WhichReg = REG_EEPROM;
	reg->Action = 0; // Read the eeprom

	if (!strncasecmp(stroffset, "0x", 2))
		reg->Offset = a2hex((stroffset + 2));
	else
		reg->Offset = atoi(stroffset);

	if (!strncasecmp(strnob, "0x", 2))
		reg->NOB = a2hex((strnob + 2));
	else
		reg->NOB = atoi(strnob);

	if (reg->NOB > MAX_EEPROM_DATA) {
		fprintf(stderr, "Number of bytes exceeds MAX EEPROM Read size\n");
		return -1;
	}

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buffer;

	if (ioctl(sockfd, WLANREGRDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr, 
			"wlanconfig: EEPROM read not possible "
			"by interface %s\n", DevName);
		return -1;
	}

	hexdump("RD EEPROM", &reg->Value, reg->NOB, ' '); 

	return 0;
}

static char    *usage[] = {
	"Usage: wlanconfig <ethX> <cmd> [...]",
	"where",
	"	ethX	: wireless network interface",
	"	cmd	: hostcmd, rdmac, wrmac, rdbbp, wrbbp, rdrf, wrrf,\n"
	"		: rdbca, wrbca, sdcmd52r, sdcmd52w, sdcmd53r,\n" 
	"		: caldata, rdcfreg, wrcfreg, rdeeprom, bgscanconfig,\n"
	"		: sleepparams, bca, bca-ts, scanbssid,\n"
	"		: requesttpc, setadhocch, getadhocch,\n"
	"		: wmmparaie, wmm_ack_policy, wmmtspec\n"
	"		: reassociate\n",
#ifdef SUBSCRIBE_EVENT_CTRL
	"		: subevent\n"
#endif
	"	[...]	: additional parameters for read registers are",
	"		:	<offset>",
	"		: additional parameters for write registers are",
	"		:	<offset> <value>",
	"		: additional parameters for BlueTooth Co-existence",
	"		:  Arbitration (BCA) wrbca are:",
	"		:	mode			off | mbca | csrbci",
	"		:	antenna			single | dual",
	"		:	btfreq			off | ib | oob",
	"		:	txprioritylow32		low | high",
	"		:	txpriorityhigh32	low | high",
	"		:	rxprioritylow32		low | high",
	"		:	rxpriorityhigh32	low | high",
	"		: addition parameters for cal data",
	"		: 	< filename >",
#ifdef SUBSCRIBE_EVENT_CTRL
	"		: additonal parameter for subevent",
	"		: 	< filename >",
#endif
	"		: additional parameters for reassociate are:",
	"		:	XX:XX:XX:XX:XX:XX YY:YY:YY:YY:YY:YY < string max 32>",
	"		:	< Current BSSID > < Desired BSSID > < Desired SSID >",
};

void display_usage(void)
{
	int i;

	for (i = 0; i < (sizeof(usage) / sizeof(usage[0])); i++)
		fprintf(stderr, "%s\n", usage[i]);
}

int findcommand(int maxcmds, char *cmds[], char *cmd)
{
	int i;

	for (i = 0; i < maxcmds; i++) {
		if (!strcasecmp(cmds[i], cmd)) {
			return i;
		}
	}

	return -1;
}

int process_sdcmd52r(int argc, char *argv[])
{
	struct ifreq    userdata;
	unsigned char 	buf[6];
	unsigned int 	tmp;

	buf[0] = 0;			//CMD52 read
	if (argc == 5) {
		buf[1] = atoval(argv[3]);	//func
		tmp = 	 atoval(argv[4]);	//reg
		buf[2] = tmp & 0xff;
		buf[3] = (tmp >> 8) & 0xff;
		buf[4] = (tmp >> 16) & 0xff;
		buf[5] = (tmp >> 24) & 0xff;
	} else {
		fprintf(stderr, "Invalid number of parameters!\n");
		return -1;
	}
	

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;

	if (ioctl(sockfd, WLANCMD52RDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr,
			"wlanconfig: CMD52 R/W not supported by "
				"interface %s\n", DevName);
		return -1;
   	}
   	printf("sdcmd52r returns 0x%02X\n", buf[0]);

   	return 0;
}

int process_sdcmd52w(int argc, char *argv[])
{
   	struct ifreq    userdata;
   	unsigned char 	buf[7];
   	unsigned int 	tmp;

	buf[0] = 1;			//CMD52 write
	if (argc == 6) {
		buf[1] = atoval(argv[3]);		//func
		tmp =    atoval(argv[4]);		//reg
		buf[2] = tmp & 0xff;
		buf[3] = (tmp >> 8) & 0xff;
		buf[4] = (tmp >> 16) & 0xff;
		buf[5] = (tmp >> 24) & 0xff;
		buf[6] = atoval(argv[5]);		//dat
	} else {
		fprintf(stderr, "Invalid number of parameters!\n");
		return -1;
	}

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;

	if (ioctl(sockfd, WLANCMD52RDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr,
			"wlanconfig: CMD52 R/W not supported by "
				"interface %s\n", DevName);
			return -1;
   	}
   	printf("sdcmd52w returns 0x%02X\n",buf[0]);

   	return 0;
}

int process_sdcmd53r(void)
{
	struct ifreq    userdata;
	char 		buf[CMD53BUFLEN];
	int 		i;

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;

	for(i=0; i < sizeof(buf); i++)
		buf[i] = i & 0xff;

	if (ioctl(sockfd, WLANCMD53RDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr,
			"wlanconfig: CMD53 R/W not supported by "
			"interface %s\n", DevName);
		return -1;
	}

	for(i=0; i < sizeof(buf); i++) {
		if (buf[i] != (i ^ 0xff))
			printf("i=%02X  %02X\n",i,buf[i]);
	}

	return 0;
}

void display_calibration(wlan_ioctl_cal_data *cal_data, int type)
{
	printf("Calibration %s calling the cal_data command:\n", 
						type ? "after" : "before");
   	printf("------------------------------------------------\n");
	printf("Action      : %x\n", cal_data->Action);
	printf("PAOption    : %x\n", cal_data->PAOption);
	printf("ExtPA       : %x\n", cal_data->ExtPA);
	printf("Ant         : %x\n", cal_data->Ant);
	hexdump("IntPA      ", &cal_data->IntPA, 28, ' ');
	hexdump("PAConfig   ", &cal_data->PAConfig, 4, ' ');
	printf("Domain      : %02x %02x\n", cal_data->Domain & 0x00ff, 
					(cal_data->Domain & 0xff00) >> 8);
	printf("ECO         : %x\n", cal_data->ECO);
	printf("LCT_cal     : %x\n", cal_data->LCT_cal);
	hexdump("MAC Addr   ", &cal_data->MacAddr, 6, ' ');
	printf("\n");
}


#ifdef WMM
int process_wmm_ack_policy(int argc, char *argv[])
{
	unsigned char	buf[(WMM_ACK_POLICY_PRIO * 2) + 3];
	int		count, i;
	struct ifreq	userdata;

	if ((argc != 3) && (argc != 5)) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 wmm_ack_policy\n");
		printf("Syntax: ./wlanconfig eth1 wmm_ack_policy <AC> <POLICY>\n");
		exit(1);
	}
	
	memset(buf, 0, (WMM_ACK_POLICY_PRIO * 2) + 3);

	buf[0] = WMM_ACK_POLICY;

	if (argc == 5) {
		buf[1] = HostCmd_ACT_SET;
		buf[2] = 0;

		buf[3] = atoi(argv[3]);
		if (buf[3] > WMM_ACK_POLICY_PRIO - 1) {
			printf("Invalid Priority. Should be between 0 and %d\n",
					WMM_ACK_POLICY_PRIO - 1);
			exit(1);
		}

		buf[4] = atoi(argv[4]);
		if(buf[4] > 1) {
			printf("Invalid Ack Policy. Should be 1 or 0\n");
			exit(1);
		}

		count = 5;
	} else {
		count = 2;
		buf[1] = HostCmd_ACT_GET;
	}
	
	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;

//	hexdump(argv[2], buf, count, ' ');

	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) {
		fprintf(stderr, "wlanconfig: %s not supported by %s\n", 
							argv[2], DevName);
			return -1;
   	}

	if (buf[1] == HostCmd_ACT_GET) {
		printf("AC Value    Priority\n");
		for (i=0; i < WMM_ACK_POLICY_PRIO; ++i) {
			count = SKIP_TYPE_ACTION + (i*2);
			printf("%4x       %5x\n", buf[count], buf[count+1]);
		}
	}		

	return 0;
}

int process_wmm_para_conf(int argc, char *argv[], int cmd)
{
	int		count;
	FILE		*fp;
	char		buf[256];
	char		filename[48] = "";
	struct ifreq	userdata;

	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 %s <filename>\n",argv[2]);
		exit(1);
	}
	
	strncpy(filename, argv[3], MIN(sizeof(filename)-1, strlen(argv[3])));
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", argv[3]);
		exit(1);
	}

	count = fparse_for_cmd_and_hex(fp, buf + SKIP_TYPE, argv[2]);
	if(count < 0) {
		printf("Invalid command parsing failed !!!\n");
		return -EINVAL;
	}

	/* This will set the type of command sent */
	buf[0] = (cmd - CMD_WMM_TSPEC) + WMM_TSPEC;

	hexdump(argv[2], buf, count + SKIP_TYPE, ' ');
	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;

	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) {
		fprintf(stderr, "wlanconfig: %s not supported by %s\n", 
							argv[2], DevName);
			return -1;
   	}

	hexdump(argv[2], buf, count + SKIP_TYPE, ' ');

	return 0;
}
#endif /* WMM */

static int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, u8 *buf, size_t len)
{
	int i, a;
	const char *ipos = hex;
	char *opos = buf;

	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}
	return 0;
}

unsigned int a2hex_or_atoi(char *value)
{
	if (value[0] == '0' && (value[1] == 'X' || value[1] == 'x'))
		return a2hex(value + 2);
	else
		return atoi(value);
}

static char * wlan_config_get_line(char *s, int size, FILE *stream, int *line)
{
	char *pos, *end, *sstart;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (*pos == '#' || (*pos == '\r' && *(pos+1) == '\n') || 
						*pos == '\n' || *pos == '\0')
			continue;

		/* Remove # comments unless they are within a double quoted
		* string. Remove trailing white space. */
		sstart = strchr(pos, '"');
		if (sstart)
			sstart = strchr(sstart + 1, '"');
		if (!sstart)
			sstart = pos;
		end = strchr(sstart, '#');
		if (end)
			*end-- = '\0';
		else
			end = pos + strlen(pos) - 1;
		while (end > pos && (*end == '\r' || *end == '\n' || 
						*end == ' ' || *end == '\t')) {
			*end-- = '\0';
		}
		if (*pos == '\0')
			continue;
		return pos;
	}

	return NULL;
}

/* 
 *  @brief get hostcmd data
 *  
 *  @param fp			A pointer to file stream
 *  @param ln			A pointer to line number
 *  @param buf			A pointer to hostcmd data
 *  @param size			A pointer to the return size of hostcmd buffer
 *  @return      		WLAN_STATUS_SUCCESS
 */
static int wlan_get_hostcmd_data(FILE *fp, int *ln, u8 *buf, u16 *size)
{
	int	errors = 0, i;
	char	line[256], *pos, *pos1, *pos2, *pos3;
	u16	len;


	while ((pos = wlan_config_get_line(line, sizeof(line), fp, ln))) {
		(*ln)++;
		if (strcmp(pos, "}") == 0) {
			break;
		}

		pos1 = strchr(pos, ':');
		if (pos1 == NULL) {
			printf("Line %d: Invalid hostcmd line '%s'\n", *ln, pos);
			errors++;
			continue;
		}
		*pos1++ = '\0';

		pos2 = strchr(pos1, '=');
		if (pos2 == NULL) {
			printf("Line %d: Invalid hostcmd line '%s'\n", *ln, pos);
			errors++;
			continue;
		}
		*pos2++ = '\0';

		len = a2hex_or_atoi(pos1);
		if (len < 1 || len > MRVDRV_SIZE_OF_CMD_BUFFER) {
			printf("Line %d: Invalid hostcmd line '%s'\n", *ln, pos);
			errors++;
			continue;
		}

		*size += len;

		if (*pos2 == '"') {
			pos2++;
			if ((pos3=strchr(pos2, '"')) == NULL) {
				printf("Line %d: invalid quotation '%s'\n", *ln, pos);
				errors++;
				continue;
			}
			*pos3 = '\0';
			memset(buf, 0, len);
			memmove(buf, pos2, MIN(strlen(pos2),len));
			buf += len;
		}
		else if (*pos2 == '\'') {
			pos2++;
			if ((pos3=strchr(pos2, '\'')) == NULL) {
				printf("Line %d: invalid quotation '%s'\n", *ln, pos);
				errors++;
				continue;
			}
			*pos3 = ',';
			for (i=0; i<len; i++) {
				if ((pos3=strchr(pos2, ',')) != NULL) {
					*pos3 = '\0';
					*buf++ = (u8)a2hex_or_atoi(pos2);
					pos2 = pos3 + 1;
				}
				else
					*buf++ = 0;
			}
		}
		else if (*pos2 == '{') {
			u16 *tlvlen = (u16 *)buf;
			wlan_get_hostcmd_data(fp, ln, buf+len, tlvlen);
			*size += *tlvlen;
			buf += len + *tlvlen;
		}
		else {
			u32 value = a2hex_or_atoi(pos2);
			while (len--) {
				*buf++ = (u8)(value & 0xff);
				value >>= 8;
			}
		}
	}
	return WLAN_STATUS_SUCCESS;
}

/** 
 *  @brief Process host_cmd 
 *  @param hostcmd      A pointer to HostCmd_DS_GEN data structure
 *  @return      	WLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int process_host_cmd(int argc, char *argv[])
{
	u8		line[256], cmdname[256], *buf, *pos;
	FILE		*fp;
	HostCmd_DS_GEN	*hostcmd;
	struct iwreq	iwr;
	int 		ln = 0;
	int		cmdname_found = 0, cmdcode_found = 0;
	int		ret = WLAN_STATUS_SUCCESS;

	if (argc < 5) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 hostcmd <hostcmd.conf> <cmdname>\n");
		exit(1);
	}
	
	if ((fp = fopen(argv[3], "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", argv[4]);
		exit(1);
	}

	buf = (u8 *)malloc(MRVDRV_SIZE_OF_CMD_BUFFER);
	memset(buf, 0, MRVDRV_SIZE_OF_CMD_BUFFER);
	hostcmd = (PHostCmd_DS_GEN)buf;

	hostcmd->Command = 0xffff;

	sprintf(cmdname, "%s={", argv[4]);
	cmdname_found = 0;
	while ((pos = wlan_config_get_line(line, sizeof(line), fp, &ln))) {
		if (strcmp(pos, cmdname) == 0) {
			cmdname_found = 1;
			sprintf(cmdname, "CmdCode=");
			cmdcode_found = 0;
			while ((pos = wlan_config_get_line(line, sizeof(line), fp, &ln))) {
				if (strncmp(pos, cmdname, strlen(cmdname)) == 0) {
					cmdcode_found = 1;
					hostcmd->Command = a2hex_or_atoi(pos+strlen(cmdname));
					hostcmd->Size = S_DS_GEN;
					wlan_get_hostcmd_data(fp, &ln, buf+hostcmd->Size, &hostcmd->Size);
					break;
				}
			}
			if (!cmdcode_found) {
				fprintf(stderr, "wlanconfig: CmdCode not found in file '%s'\n", argv[3]);
			}
			break;
		}
	}

	fclose(fp);

	if (!cmdname_found)
		fprintf(stderr, "wlanconfig: cmdname '%s' not found in file '%s'\n", argv[4],argv[3]);

	if (!cmdname_found || !cmdcode_found) {
		ret = -1;
		goto _exit_;
	}

	buf = (u8 *)hostcmd;

	hostcmd->SeqNum = 0;
	hostcmd->Result = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, DevName, IFNAMSIZ);
	iwr.u.data.pointer = (u8 *)hostcmd;
	iwr.u.data.length = hostcmd->Size;
	
	iwr.u.data.flags = 0;
/*
	{
	int i;
	printf("HOSTCMD buffer:\n");
	for(i = 0; i < hostcmd->Size; i++) {
		printf("%02x ", ((u8 *)iwr.u.data.pointer)[i]);
		if ((i % 16) == 15) {
			printf("\n");
		}
	}
	printf("\n");
	}
*/
	if (ioctl(sockfd, WLANHOSTCMD, &iwr)) {
		fprintf(stderr, "wlanconfig: WLANHOSTCMD is not supported by %s\n", DevName);
		ret = -1;
		goto _exit_;
   	}
/*
	{
	int i;
	printf("HOSTCMD response buffer:\n");
	for(i = 0; i < hostcmd->Size; i++) {
		printf("%02x ", ((u8 *)userdata.ifr_data)[i]);
		if ((i % 16) == 15) {
			printf("\n");
		}
	}
	printf("\n");
	}
*/
	if (!hostcmd->Result) {
		switch (hostcmd->Command) {
#ifdef POWER_ADAPT_CFG_EXT
		case HostCmd_RET_802_11_POWER_ADAPT_CFG_EXT:
		{
			PHostCmd_DS_802_11_POWER_ADAPT_CFG_EXT pace = (PHostCmd_DS_802_11_POWER_ADAPT_CFG_EXT)(buf + S_DS_GEN);
			int i, j;
			printf("EnablePA=%#04x\n",pace->EnablePA);
			for (i = 0; i < pace->PowerAdaptGroup.Header.Len / sizeof(PA_Group_t); i++) {
				printf("PA Group #%d: level=%ddbm, Rate Bitmap=%#04x (",i,
					pace->PowerAdaptGroup.PA_Group[i].PowerAdaptLevel,
					pace->PowerAdaptGroup.PA_Group[i].RateBitmap);
				for (j = 8 * sizeof(pace->PowerAdaptGroup.PA_Group[i].RateBitmap) - 1; j >= 0; j--) {
					if ((j == 4) || (j >= 13))	// reserved
						continue;
					if (pace->PowerAdaptGroup.PA_Group[i].RateBitmap & (1 << j))
						printf("%s ", rate_bitmap[j]);
				}
				printf("Mbps)\n");
			}
			break;
		}
#endif
#ifdef AUTO_TX
		case HostCmd_RET_802_11_AUTO_TX:
		{
			PHostCmd_DS_802_11_AUTO_TX at = 
				(PHostCmd_DS_802_11_AUTO_TX)(buf + S_DS_GEN);
			if (at->Action == HostCmd_ACT_GET) {
				if (S_DS_GEN + sizeof(at->Action) == hostcmd->Size) {
					printf("auto_tx not configured\n");
				}
				else {
				    MrvlIEtypesHeader_t *header = &at->AutoTx.Header;
				    if ((S_DS_GEN + sizeof(at->Action) + sizeof(MrvlIEtypesHeader_t) +
					header->Len == hostcmd->Size) && (header->Type == TLV_TYPE_AUTO_TX)) {
					AutoTx_MacFrame_t *atmf = &at->AutoTx.AutoTx_MacFrame;

					printf("Interval: %d second(s)\n", atmf->Interval);
					printf("Priority: %#x\n", atmf->Priority);

					printf("Frame Length: %d\n", atmf->FrameLen);
					printf("Dest Mac Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
						atmf->DestMacAddr[0],
						atmf->DestMacAddr[1],
						atmf->DestMacAddr[2],
						atmf->DestMacAddr[3],
						atmf->DestMacAddr[4],
						atmf->DestMacAddr[5]);
					printf("Src Mac Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
						atmf->SrcMacAddr[0],
						atmf->SrcMacAddr[1],
						atmf->SrcMacAddr[2],
						atmf->SrcMacAddr[3],
						atmf->SrcMacAddr[4],
						atmf->SrcMacAddr[5]);

					hexdump("Frame Payload", atmf->Payload,
						atmf->FrameLen - ETH_ALEN * 2, ' ');
				    }
				    else {
					printf("incorrect auto_tx command response\n");
				    }
				}
			}
			break;
		}
#endif
		default:
			printf("HOSTCMD_RESP: ReturnCode=%#04x, Result=%#04x\n",hostcmd->Command,hostcmd->Result);
			break;
		}
	}
	else {
		printf("HOSTCMD failed: ReturnCode=%#04x, Result=%#04x\n",hostcmd->Command,hostcmd->Result);
	}

_exit_:
	if (buf)
		free(buf);

	return WLAN_STATUS_SUCCESS;
}

#ifdef BG_SCAN
static int wlan_config_parse_string(const char *value, char *str, size_t *len, size_t *maxlen)
{
	char * p;
	p = strchr(value, ',');
	if(p){
		*maxlen =  (u16)a2hex_or_atoi(p+1);
		*p = '\0';
	}
	else{
		*maxlen = 0;
	}
	
	if (*value == '"') {
		char *pos;
		value++;
		pos = strchr(value, '"');
		
		if (pos == NULL || pos[1] != '\0') {
			value--;
			return -1;
		}
		*pos = '\0';
		*len = strlen(value);
		strcpy(str, value);
		return 0;
	} else {
		int hlen = strlen(value);

		if (hlen % 1)
			return -1;
		*len = hlen / 2;
		if (str == NULL)
			return -1;
		if (hexstr2bin(value, str, *len)) {
			return -1;
   		}
	    	return 0;
 	 }
}

int bgscan_parse_action(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->Action = (u16)a2hex_or_atoi(value);
	return 0;
}

int bgscan_parse_enable(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;

	bgscan_config->Enable = (u8)a2hex_or_atoi(value);
	
	return 0;
}

int bgscan_parse_bsstype(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;

	bgscan_config->BssType = (u8)a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_channelsperscan(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->ChannelsPerScan = (u8)a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_discardwhenfull(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->DiscardWhenFull = (u8)a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_scaninterval(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->ScanInterval = (u32)a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_storecondition(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->StoreCondition = a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_reportconditions(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->ReportConditions = a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_maxscanresults(u8 *CmdBuf, int line, char *value)
{
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config = 
				(HostCmd_DS_802_11_BG_SCAN_CONFIG *)CmdBuf;
	
	bgscan_config->MaxScanResults = (u16)a2hex_or_atoi(value);

	return 0;
}

int bgscan_parse_ssid(u8 *CmdBuf, int line, char *value)
{
	static int 			ssidCnt;
	MrvlIEtypes_SsIdParamSet_t 	*SsIdParamSet = NULL;
	MrvlIEtypes_WildCardSsIdParamSet_t *WildcardSsIdParamSet = NULL;
	char				*buf;
	size_t				len = 0;
	size_t				maxlen = 0;
	
	SsIdParamSet = (MrvlIEtypes_SsIdParamSet_t *)(CmdBuf + ActualPos);
	WildcardSsIdParamSet = (MrvlIEtypes_WildCardSsIdParamSet_t *)(CmdBuf + ActualPos);

	buf = (char *)malloc(strlen(value));	
	memset(buf, 0, strlen(value));
	if(wlan_config_parse_string(value, buf, &len, &maxlen)) {
		printf("Invalid SSID\n");
       		free(buf);
 		return -1;
	}		
	ssidCnt++;
	if(!strlen(buf)) {
		printf("The %dth SSID is NULL.\n", ssidCnt);
	}
	if(maxlen > len)
	{
		WildcardSsIdParamSet->Header.Type = cpu_to_le16(TLV_TYPE_WILDCARDSSID);
		WildcardSsIdParamSet->Header.Len = strlen(buf) + sizeof(WildcardSsIdParamSet->MaxSsidLength);
		WildcardSsIdParamSet->MaxSsidLength = maxlen;
		TLVSsidSize += WildcardSsIdParamSet->Header.Len + sizeof(MrvlIEtypesHeader_t);
		ActualPos += WildcardSsIdParamSet->Header.Len + sizeof(MrvlIEtypesHeader_t);
		WildcardSsIdParamSet->Header.Len  = cpu_to_le16(WildcardSsIdParamSet->Header.Len);
		memcpy(WildcardSsIdParamSet->SsId, buf, strlen(buf));
	}
	else
	{
 		SsIdParamSet->Header.Type = cpu_to_le16(TLV_TYPE_SSID); /*0x0000; */
		SsIdParamSet->Header.Len  = strlen(buf);
		TLVSsidSize += SsIdParamSet->Header.Len + sizeof(MrvlIEtypesHeader_t);
		ActualPos += SsIdParamSet->Header.Len + sizeof(MrvlIEtypesHeader_t);
		SsIdParamSet->Header.Len  = cpu_to_le16(SsIdParamSet->Header.Len);
		memcpy(SsIdParamSet->SsId, buf, strlen(buf));
	}
	free(buf);
	return 0;
}		

int bgscan_parse_probes(u8 *CmdBuf, int line, char *value)
{
	MrvlIEtypes_NumProbes_t	*Probes = NULL;

#define PROBES_PAYLOAD_SIZE	2
	
	Probes = (MrvlIEtypes_NumProbes_t *)(CmdBuf + ActualPos);

	Probes->Header.Type = TLV_TYPE_NUMPROBES;
	Probes->Header.Len = PROBES_PAYLOAD_SIZE;
	
	Probes->NumProbes = (u16)a2hex_or_atoi(value);

	if (Probes->NumProbes) {
		TLVProbeSize += sizeof(MrvlIEtypesHeader_t) +
						Probes->Header.Len;
	} else {
		TLVProbeSize = 0;
	}

	ActualPos += TLVProbeSize;
	return 0;
}

int bgscan_parse_channellist(u8 *CmdBuf, int line, char *value)
{
  MrvlIEtypes_ChanListParamSet_t  *chan;
  char *buf, *grp0, *grp1;
  int len, idx;

  chan = (MrvlIEtypes_ChanListParamSet_t *)(CmdBuf + ActualPos);

  len = strlen(value) + 1;
  buf = malloc(len);

  if (buf == NULL)
    return -1;

  memset(buf, 0, len);
  strcpy(buf, value);

  chan->Header.Type = cpu_to_le16(TLV_TYPE_CHANLIST);
  grp1 = buf;
  idx = 0;

  while ((grp1 != NULL) && (*grp1 != 0))
  {
    grp0 = strsep(&grp1, "\";");

    if ((grp0 != NULL) && (*grp0 != 0))
    {
      chan->ChanScanParam[idx].RadioType    = atoi(strtok(grp0, ","));
      chan->ChanScanParam[idx].ChanNumber   = atoi(strtok(NULL, ","));
      chan->ChanScanParam[idx].ChanScanMode.PassiveScan 
          = atoi(strtok(NULL, ","));
      chan->ChanScanParam[idx].ChanScanMode.DisableChanFilt = 1;
      chan->ChanScanParam[idx].MinScanTime  = atoi(strtok(NULL, ","));
      chan->ChanScanParam[idx].MaxScanTime  = atoi(strtok(NULL, ","));
      idx ++;
    }
  }

  chan->Header.Len = (idx * sizeof(ChanScanParamSet_t));
  TLVChanSize += (chan->Header.Len + sizeof(MrvlIEtypesHeader_t));
  chan->Header.Len = cpu_to_le16(chan->Header.Len);
  ActualPos += TLVChanSize;

  free (buf);
  return 0;
}

int bgscan_parse_snrthreshold(u8 *CmdBuf, int line, char *value)
{
	MrvlIEtypes_SnrThreshold_t	*SnrThreshold = NULL;
	unsigned int 	tmp;
	
	SnrThreshold = (MrvlIEtypes_SnrThreshold_t *)(CmdBuf + ActualPos);

	SnrThreshold->Header.Type = TLV_TYPE_SNR;
	SnrThreshold->Header.Len = PROBES_PAYLOAD_SIZE;
	
	tmp = (u16)a2hex_or_atoi(value);
	SnrThreshold->SNRValue = tmp & 0xff;
	SnrThreshold->SNRFreq = (tmp >> 8) & 0xff;

    	TLVSnrSize += sizeof(MrvlIEtypesHeader_t) + SnrThreshold->Header.Len; 
	ActualPos += TLVSnrSize;
	return 0;
}

int bgscan_parse_bcastprobe(u8 *CmdBuf, int line, char *value)
{
	MrvlIEtypes_BcastProbe_t *BcastProbe = NULL;


	BcastProbe = (MrvlIEtypes_BcastProbe_t *)(CmdBuf + ActualPos);

	BcastProbe->Header.Type = TLV_TYPE_BCASTPROBE;
	BcastProbe->Header.Len = PROBES_PAYLOAD_SIZE;
	
	BcastProbe->BcastProbe = (u16)a2hex_or_atoi(value);

    	TLVBcProbeSize = sizeof(MrvlIEtypesHeader_t) + BcastProbe->Header.Len;
	ActualPos += TLVBcProbeSize;
	return 0;
}

int bgscan_parse_numssidprobe(u8 *CmdBuf, int line, char *value)
{
	MrvlIEtypes_NumSSIDProbe_t *NumSSIDProbe = NULL;


	NumSSIDProbe = (MrvlIEtypes_NumSSIDProbe_t *)(CmdBuf + ActualPos);

	NumSSIDProbe->Header.Type = TLV_TYPE_NUMSSID_PROBE;
	NumSSIDProbe->Header.Len = PROBES_PAYLOAD_SIZE;
	
	NumSSIDProbe->NumSSIDProbe = (u16)a2hex_or_atoi(value);

    	TLVNumSsidProbeSize = sizeof(MrvlIEtypesHeader_t) + NumSSIDProbe->Header.Len;
	ActualPos += TLVNumSsidProbeSize;
	return 0;
}

int wlan_get_bgscan_data(FILE *fp, int *line, 
				HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config)
{
	int	errors = 0, i, end = 0;
	char	buf[256], *pos, *pos2;


	while ((pos = wlan_config_get_line(buf, sizeof(buf), fp, line))) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		}

		pos2 = strchr(pos, '=');
		if (pos2 == NULL) {
			printf("Line %d: Invalid bgscan line '%s'.",
								*line, pos);
			errors++;
			continue;
		}

		*pos2++ = '\0';
		if (*pos2 == '"') {
			if (strchr(pos2 + 1, '"') == NULL) {
				printf("Line %d: invalid quotation '%s'.", 
								*line, pos2);
				errors++;
				continue;
			}
		}

		for (i = 0; i < NUM_BGSCAN_FIELDS; i++) {
			if (strcmp(pos, bgscan_fields[i].name) == 0) {
				if (bgscan_fields[i].parser((u8 *)bgscan_config,
								*line, pos2)) {
					printf("Line %d: failed to parse %s"
						"'%s'.", *line, pos, pos2);
					errors++;
					}
				break;
			}
		}
		if (i == NUM_BGSCAN_FIELDS) {
			printf("Line %d: unknown bgscan field '%s'.\n", 
								*line, pos);
			errors++;
		}
	}
	return 0;
}

int process_bg_scan_config(int argc, char *argv[])
{
	u8				scanCfg[512], *pos, *buf = NULL;
	char				filename[48] = "";
	FILE				*fp;
	HostCmd_DS_802_11_BG_SCAN_CONFIG *bgscan_config;
	struct ifreq			userdata;
	int 				line = 0;
	int				CmdNum = BG_SCAN_CONFIG;
	int				Action;
	u16				Size;

	buf = (u8 *)malloc(512);
	
	memset(buf, 0, 512);

	bgscan_config = (HostCmd_DS_802_11_BG_SCAN_CONFIG *)
				(buf + sizeof(int) + sizeof(u16));

	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 bgscanconfig <filename>\n");
		exit(1);
	}
	
	strncpy(filename, argv[3], MIN(sizeof(filename)-1, strlen(argv[3])));
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", filename);
		exit(1);
	}

	while ((pos = wlan_config_get_line(scanCfg, sizeof(scanCfg), fp, 
								&line))) {
		if (strcmp(pos, "bgscan={") == 0) {
			wlan_get_bgscan_data(fp, &line, bgscan_config);
		}
	}

	fclose(fp);

	Action = bgscan_config->Action;

	Size = sizeof(HostCmd_DS_802_11_BG_SCAN_CONFIG) +
		TLVSsidSize + TLVProbeSize + TLVChanSize + TLVSnrSize + TLVBcProbeSize + TLVNumSsidProbeSize;

	memcpy(buf, &CmdNum, sizeof(int));
	memcpy(buf + sizeof(int), &Size, sizeof(u16));

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;

#if 0
	{
		int i;
		for(i = 0; i < Size + 6; i++)
			printf(" 0x%X",buf[i]);
		printf("\n");
	}
#endif

	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) {
		fprintf(stderr, "wlanconfig: BG_SCAN is not supported by %s\n", 
								DevName);
			return -1;
   	}

	if (Action == HostCmd_ACT_GEN_GET) {
		int i;
		printf("BGSCAN Configuration setup:\n");
		for(i = 0; i < Size; i++) {
			if (!(i % 10)) {
				printf("\n");
			}
			printf(" 0x%x ", buf[i + 3]);
		}
		printf("\n");
	}

	free(buf);

	return 0;

}
#endif

#ifdef SUBSCRIBE_EVENT_CTRL
int sub_event_parse_action(EventSubscribe *EventData, int line, char *value)
{
	
	EventData->Action = (u16)a2hex_or_atoi(value);

	return 0;
}

int sub_event_parse_event(EventSubscribe *EventData, int line, char *value)
{
	
	EventData->Events = (u16)a2hex_or_atoi(value);

	return 0;	
}

int sub_event_parse_RSSI(EventSubscribe *EventData, int line, char *value)
{
	char		*p = value, *token;
	char		delim = ',';

	token = strtok(p, &delim);
	EventData->RSSIValue = (u8)a2hex_or_atoi(token);
	p = NULL;
	token = strtok(p, &delim);
	EventData->RSSIFreq = (u8)a2hex_or_atoi(token);

	return 0;
}

int sub_event_parse_SNR(EventSubscribe *EventData, int line, char *value)
{
	char		*p = value, *token;
	char		delim = ',';

	token = strtok(p, &delim);
	EventData->SNRValue = (u8)a2hex_or_atoi(token);
	p = NULL;
	token = strtok(p, &delim);
	EventData->SNRFreq = (u8)a2hex_or_atoi(token);

	return 0;
}

int sub_event_parse_failcnt(EventSubscribe *EventData, int line, char *value)
{
	char		*p = value, *token;
	char		delim = ',';

	token = strtok(p, &delim);
	EventData->FailValue = (u8)a2hex_or_atoi(token);
	p = NULL;
	token = strtok(p, &delim);
	EventData->FailFreq = (u8)a2hex_or_atoi(token);

	return 0;
}

int sub_event_parse_beacon_missed(EventSubscribe *EventData, int line, char *value)
{
	EventData->BcnMissed = (u8)a2hex_or_atoi(value);

	return 0;
}

int wlan_get_subscribe_event_data(FILE *fp, int *line, EventSubscribe *EventData)
{
	int	errors = 0, i, end = 0;
	char	buf[256], *pos, *pos2;


	while ((pos = wlan_config_get_line(buf, sizeof(buf), fp, line))) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		}

		pos2 = strchr(pos, '=');
		if (pos2 == NULL) {
			printf("Line %d: Invalid sub_event line '%s'.",
								*line, pos);
			errors++;
			continue;
		}

		*pos2++ = '\0';
		if (*pos2 == '"') {
			if (strchr(pos2 + 1, '"') == NULL) {
				printf("Line %d: invalid quotation '%s'.", 
								*line, pos2);
				errors++;
				continue;
			}
		}

		for (i = 0; i < NUM_SUB_EVENT_FIELDS; i++) {
			if (strcmp(pos, sub_event_fields[i].name) == 0) {
				if (sub_event_fields[i].parser(EventData,
								*line, pos2)) {
					printf("Line %d: failed to parse %s"
						"'%s'.", *line, pos, pos2);
					errors++;
					}
				break;
			}
		}
		if (i == NUM_SUB_EVENT_FIELDS) {
			printf("Line %d: unknown sub_event field '%s'.", 
								*line, pos);
			errors++;
		}
	}
	return 0;
}

int process_event_subscribe(int argc, char *argv[])
{
	u8			*buf;
	char			event_data[512];
	char			filename[48] = "";
	FILE			*fp;
	EventSubscribe		EventData;
	struct ifreq		userdata;
	u8			*pos;
	int			line = 0;
	int			CmdNum = SUBSCRIBE_EVENT; 

	memset(&EventData, 0, sizeof(EventSubscribe));
	
	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 subevent <filename>\n");
		exit(1);
	}
	
	strncpy(filename, argv[3], MIN(sizeof(filename)-1, strlen(argv[3])));

	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", argv[1]);
		exit(1);
	}

	while ((pos = wlan_config_get_line(event_data, sizeof(event_data), fp, 
							(int *)&line))) {
		if (strcmp(pos, "Events={") == 0) {
			wlan_get_subscribe_event_data(fp, &line, &EventData);
		}
	}

	fclose(fp);

	buf = (u8 *)malloc(512);

	memcpy(buf, &CmdNum, sizeof(int));
	memcpy(buf + sizeof(int), (u8 *)&EventData.Action, 
							sizeof(EventSubscribe));

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = buf;
	
	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) {
		fprintf(stderr, "wlanconfig: SUBSCRIBE_EVENT not"
						" supported by %s\n", DevName);
			return -1;
   	}
	
#if 0	
	{
		int	i = 0;
		for(i = 3; i < sizeof(EventSubscribe); i++)
			printf("0x%X ", buf[i]);
		printf("\n");
	}
#endif
#define GET_EVENTS	0
#define BIT_0		(1 << 0) 
#define BIT_1		(1 << 1)
#define BIT_2		(1 << 2)
#define BIT_3		(1 << 3)
	if (EventData.Action == GET_EVENTS) {
		u16		Events;

		Events = *(u16 *)(buf + sizeof(int));

		printf("Events Subscribed to Firmware are\n");

		if (Events & BIT_0) {
			printf("RSSI_LOW\n");
		}

		if (Events & BIT_1) {
			printf("SNR_LOW\n");
		}

		if (Events & BIT_2) {
			printf("MAX_FAIL\n");
		}

		if (Events & BIT_3) {
			printf("LINK LOSS\n");
		}
			
	}
		
	free(buf);

	return 0;
}
#endif

#ifdef CAL_DATA
/* To Change the calibration */
int process_cal_data(int argc, char *argv[])
{
	int			i, count;
	u8			*buf;
	char			filename[48] = "";
	FILE			*fp;
	wlan_ioctl_cal_data	cal_data;
	struct ifreq		userdata;

	memset(&cal_data, 0, sizeof(cal_data));

	buf = (u8 *) &cal_data.Type;
	
	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 caldata <filename>\n");
		exit(1);
	}
	
	strncpy(filename, argv[3], MIN(sizeof(filename)-1, strlen(argv[3])));
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", argv[1]);
		exit(1);
	}

	count = fparse_for_hex(fp, buf + 1);
	fclose(fp);

#ifdef  DEBUG	
	printf("\n");
	printf("HEXDUMP of the configuration file:");
	for (i = 0; i < count; i++) {
		if (!(i % 16))
			printf("\n");
		printf("%02x ", buf[i]);
	}
	printf("\n\n");
#endif

	display_calibration(&cal_data, 0);

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = (char *) &cal_data;

	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) {
		fprintf(stderr, "wlanconfig: CAL DATA not supported by %s\n", 
								DevName);
			return -1;
   	}
	
	display_calibration(&cal_data, 1);

	return 0;
}

int process_cal_data_ext(int argc, char *argv[])
{
	int				i, count;
	u8				*buf, *CmdBuf;
	FILE				*fp;
	char				filename[48] = "";
	HostCmd_DS_802_11_CAL_DATA_EXT	cal_data; 
	struct ifreq			userdata;
	int				CmdNum = CAL_DATA_EXT_CONFIG;

	printf("Enter process_cal_data_ext()\n");
	memset(&cal_data, 0, sizeof(cal_data));

	buf = (u8 *) &cal_data.Action;
	
	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 caldataext <filename>\n");
		exit(1);
	}
	
	strncpy(filename, argv[3], MIN(sizeof(filename)-1, strlen(argv[3])));
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", argv[1]);
		exit(1);
	}
	
	count = fparse_for_hex(fp, buf); //copyfile data to buf+1
	fclose(fp);

	CmdBuf = malloc(sizeof(HostCmd_DS_802_11_CAL_DATA_EXT) + sizeof(int));
	memcpy(CmdBuf, &CmdNum, sizeof(int));
	memcpy(CmdBuf + sizeof(int), &cal_data, 
					sizeof(HostCmd_DS_802_11_CAL_DATA_EXT));
	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = CmdBuf;

#ifdef  DEBUG	
	printf("\n");
	printf("HEXDUMP of the configuration file:");
	for (i = 0; i < count; i++) {
		if (!(i % 16))
			printf("\n");
		printf("%02x ", buf[i]);
	}
	printf("\n\n");
#endif
	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) { 
		fprintf(stderr, "wlanconfig: CAL DATA not supported by %s\n", 
								DevName);
			return -1;
   	}

	free(CmdBuf); //free CmdBuf
	printf("Exit process_cal_data_ext()\n"); 
	return 0;
}
#endif

char *readCurCmd(char *ptr, char *curCmd)
{
	int i = 0;
#define MAX_CMD_SIZE 64

	while(*ptr != ']' && i < (MAX_CMD_SIZE - 1))
		curCmd[i++] = *(++ptr);	

	if(*ptr != ']')
		return NULL;

	curCmd[i - 1] = '\0';

	return ++ptr;
}

int fparse_for_cmd_and_hex(FILE *fp, u8 *dst, u8 *cmd)
{
	char	*ptr;
	u8	*dptr;
	char	buf[256], curCmd[64];
	int	isCurCmd = 0;

	char *convert2hex(char *ptr, u8 *chr);

	dptr = dst;
	while (fgets(buf, sizeof(buf), fp)) {
		ptr = buf;

		while (*ptr) {
			// skip leading spaces
			while (*ptr && isspace(*ptr))
				ptr++;

			// skip blank lines and lines beginning with '#'
			if (*ptr == '\0' || *ptr == '#')
				break;

			if(*ptr == '[' && *(ptr + 1) != '/') {
				if(!(ptr = readCurCmd(ptr, curCmd)))
					return -1;

				if(strcasecmp(curCmd, cmd)) /* Not equal */
					isCurCmd = 0;
				else
					isCurCmd = 1;
			}

			/* Ignore the rest if it is not correct cmd */
			if(!isCurCmd)
				break;

			if(*ptr == '[' && *(ptr + 1) == '/' )
				return (dptr - dst);

			if (isxdigit(*ptr)) {
				ptr = convert2hex(ptr, dptr++);
			} else {
				/* Invalid character on data line */
				ptr++;
			}
		}
	}

	return -1;
}

int fparse_for_hex(FILE *fp, u8 *dst)
{
	char	*ptr;
	u8	*dptr;
	char	buf[256];

	char *convert2hex(char *ptr, u8 *chr);

	dptr = dst;
	while (fgets(buf, sizeof(buf), fp)) {
		ptr = buf;

		while (*ptr) {
			// skip leading spaces
			while (*ptr && isspace(*ptr))
				ptr++;

			// skip blank lines and lines beginning with '#'
			if (*ptr == '\0' || *ptr == '#')
				break;

			if (isxdigit(*ptr)) {
				ptr = convert2hex(ptr, dptr++);
			} else {
				/* Invalid character on data line */
				ptr++;
			}
		}
	}

	return (dptr - dst);
}

char *convert2hex(char *ptr, u8 *chr)
{
	u8	val;

	int hexval(int chr);

	for (val = 0; *ptr && isxdigit(*ptr); ptr++) {
		val = (val * 16) + hexval(*ptr);
	}

	*chr = val;

	return ptr;
}

int hexval(int chr)
{
	if (chr >= '0' && chr <= '9')
		return chr - '0';
	if (chr >= 'A' && chr <= 'F')
		return chr - 'A' + 10;
	if (chr >= 'a' && chr <= 'f')
		return chr - 'a' + 10;

	return 0;
}

int process_read_register(int cmd, char *stroffset)
{
	struct ifreq    userdata;
	wlan_ioctl_regrdwr reg;
	char           *whichreg;

	switch (cmd) {
		case CMD_RDMAC:
			/*
			 * HostCmd_CMD_MAC_REG_ACCESS 
			 */
			reg.WhichReg = REG_MAC;
			whichreg = "MAC";
			break;
		case CMD_RDBBP:
			/*
			 * HostCmd_CMD_BBP_REG_ACCESS 
			 */
			reg.WhichReg = REG_BBP;
			whichreg = "BBP";
			break;
		case CMD_RDRF:
			/*
			 * HostCmd_CMD_RF_REG_ACCESS 
			 */
			reg.WhichReg = REG_RF;
			whichreg = "RF";
			break;
		default:
			fprintf(stderr, 
				"Invalid Register set specified.\n");
			return -1;
	}

	reg.Action = 0;		/* READ */

	if (!strncasecmp(stroffset, "0x", 2))
		reg.Offset = a2hex((stroffset + 2));
	else
		reg.Offset = atoi(stroffset);

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = (char *) &reg;

	if (ioctl(sockfd, WLANREGRDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr,
			"wlanconfig: Register Reading not supported by"
			"interface %s\n", DevName);
		return -1;
	}

	printf("%s[0x%04lx] = 0x%08lx\n", 
			whichreg, reg.Offset, reg.Value);

	return 0;
}

int process_write_register(int cmd, char *stroffset, char *strvalue)
{
	struct ifreq    	userdata;
	wlan_ioctl_regrdwr 	reg;
	char           		*whichreg;

	switch (cmd) {
		case CMD_WRMAC:
			/*
			 * HostCmd_CMD_MAC_REG_ACCESS 
			 */
			reg.WhichReg = REG_MAC;
			whichreg = "MAC";
			break;
		case CMD_WRBBP:
			/*
			 * HostCmd_CMD_BBP_REG_ACCESS 
			 */
			reg.WhichReg = REG_BBP;
			whichreg = "BBP";
			break;
		case CMD_WRRF:
			/*
			 * HostCmd_CMD_RF_REG_ACCESS 
			 */
			reg.WhichReg = REG_RF;
			whichreg = "RF";
			break;
		default:
			fprintf(stderr, 
				"Invalid register set specified.\n");
			return -1;
	}

	reg.Action = 1;		/* WRITE */

	if (!strncasecmp(stroffset, "0x", 2))
		reg.Offset = a2hex((stroffset + 2));
	else
		reg.Offset = atoi(stroffset);

	if (!strncasecmp(strvalue, "0x", 2))
		reg.Value = a2hex((strvalue + 2));
	else
		reg.Value = atoi(strvalue);

	printf("Writing %s Register 0x%04lx with 0x%08lx\n", whichreg,
			reg.Offset, reg.Value);

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = (char *) &reg;

	if (ioctl(sockfd, WLANREGRDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr, 
			"wlanconfig: Register Writing not supported "
			"by interface %s\n", DevName);
		return -1;
	}

	printf("%s[0x%04lx] = 0x%08lx\n",
			whichreg, reg.Offset, reg.Value);

	return 0;
}

int process_read_cfreg(char *stroffset)
{
	struct ifreq    	userdata;
	wlan_ioctl_cfregrdwr 	reg;
	
	reg.Action = 0; //Read register

	if (!strncasecmp(stroffset, "0x", 2))
		reg.Offset = a2hex((stroffset + 2));
	else
		reg.Offset = atoi(stroffset);

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = (char *) &reg;

	if (ioctl(sockfd, WLANREGCFRDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr, 
			"wlanconfig: Register reading not supported "
			"by interface %s\n", DevName);
		return -1;
	}

	printf("CFREG[0x%04X] = 0x%04X\n",
				reg.Offset, reg.Value);

	return 0;

}
	
int process_write_cfreg(char *stroffset, char *strvalue)
{
	struct ifreq    	userdata;
	wlan_ioctl_cfregrdwr 	reg;

	reg.Action = 1; //Write register

	if (!strncasecmp(stroffset, "0x", 2))
		reg.Offset = a2hex((stroffset + 2));
	else
		reg.Offset = atoi(stroffset);

	if (!strncasecmp(strvalue, "0x", 2))
		reg.Value = a2hex((strvalue + 2));
	else
		reg.Value = atoi(strvalue);

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = (char *) &reg;

	if (ioctl(sockfd, WLANREGCFRDWR, &userdata)) {
		perror("wlanconfig");
		fprintf(stderr, 
			"wlanconfig: Register writing not supported "
			"by interface %s\n", DevName);
		return -1;
	}

	//printf("CFREG[0x%04X] = 0x%04X\n",
	//			reg.Offset, reg.Value);

	return 0;
}

int ascii2hex(unsigned char *d, char *s, uint dlen)
{
	int i;
	unsigned char n;

	memset(d, 0x00, dlen);

	for (i = 0; i < dlen * 2; i++) {
		if ((s[i] >= 48) && (s[i] <= 57))
			n = s[i] - 48;
		else if ((s[i] >= 65) && (s[i] <= 70))
			n = s[i] - 65;
		else if ((s[i] >= 97) && (s[i] <= 102))
			n = s[i] - 97;
		else
			break;
		if ((i % 2) == 0)
			n = n * 16;
		d[i / 2] += n;
	}

	return i;
}

unsigned int a2hex(char *s)
{
	unsigned int    val = 0;
	unsigned char   hexc2bin(char chr);

	while (*s && isxdigit(*s)) {
		val = (val << 4) + hexc2bin(*s++);
	}

	return val;
}

unsigned char hexc2bin(char chr)
{
	if (chr >= '0' && chr <= '9')
		chr -= '0';
	else if (chr >= 'A' && chr <= 'F')
		chr -= ('A' - 10);
	else if (chr >= 'a' && chr <= 'f')
		chr -= ('a' - 10);

	return chr;
}

void hexdump(char *prompt, void *p, int len, char delim)
{
	int             i;
	unsigned char  *s = p;
	
	printf("%s : ", prompt);
	for (i = 0; i < len; i++) {
		if (i != len - 1)
			printf("%02x%c", *s++, delim);
		else
			printf("%02x\n", *s);
	}
}
