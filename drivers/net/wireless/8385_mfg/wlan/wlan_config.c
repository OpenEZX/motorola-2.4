/*
 * File : wlanconfig.c
 * 
 * Program to configure addition paramters into the wlan driver
 * 
 * Usage:
 * 
 * wlanconfig <ethX> <cmd> [...] 
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

#include	"wlan_wext.h"

#ifdef	DEBUG
#define	PRINTF		printf
#else
#define	PRINTF(...)
#endif

#ifndef u8
typedef unsigned char u8;
#endif

enum COMMANDS {
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
#ifdef BG_SCAN
	CMD_BG_SCAN_CONFIG,
#endif
	CMD_CAL_DATA,
	CMD_CFREGR,
	CMD_CFREGW,
};

static char    *commands[] = {
	"rdmac",
	"wrmac",
	"rdbbp",
	"wrbbp",
	"rdrf",
	"wrrf",
	"rdbca",
	"wrbca",
	"rdeeprom",
	"cmd52r",
	"cmd52w",
	"cmd53r",
	"cmd53w",
#ifdef BG_SCAN
	"bgscanconfig",
#endif
	"caldata",
	"rdcfreg",
	"wrcfreg",
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

void            display_usage(void);
int             findcommand(int maxcmds, char *cmds[], char *cmd);
int             process_read_register(int cmd, char *stroffset);
int             process_write_register(int cmd, char *stroffset, char *strvalue);
int             ascii2hex(unsigned char *d, char *s, uint dlen);
unsigned int    a2hex(char *s);
void            hexdump(char *prompt, void *p, int len, char delim);
int             process_cmd52r(int argc, char *argv[]);
int             process_cmd52w(int argc, char *argv[]);
int             process_cmd53r(void);
int		process_cal_data(int argc, char *argv[]);
#ifdef BG_SCAN
int		process_bg_scan_config(int argc, char *argv[]);
#endif
int 		fparse_for_hex(FILE *fp, u8 *dst);
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
		"setadhockey",
		"getadhockey",
		};

/* 
 * These two static array contains the main command number and the
 * subcommand number respectively
 */
static int PRIV_IOCTL[sizeof(priv_ioctl_names)/sizeof(priv_ioctl_names[0])];
static int PRIV_SUBIOCTL[sizeof(priv_ioctl_names)/sizeof(priv_ioctl_names[0])];

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
			process_cmd52r(argc,argv);
			break;
		case CMD_CMD52W:
			process_cmd52w(argc,argv);
			break;
		case CMD_CMD53R:
			process_cmd53r();
			break;
#ifdef BG_SCAN
		case CMD_BG_SCAN_CONFIG:
			process_bg_scan_config(argc, argv);
			break;
#endif
		case CMD_CAL_DATA:
			process_cal_data(argc, argv);
			break;
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
	reg -> WhichReg = REG_EEPROM;
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

	//printf("CFREG[0x%04X] = 0x%04X\n",

static char    *usage[] = {
	"Usage: wlanconfig <ethX> <cmd> [...]",
	"where",
	"	ethX	: wireless network interface",
	"	cmd	: rdmac, wrmac, rdbbp, wrbbp, rdrf, wrrf,\n"
	"		: rdbca, wrbca, cmd52r, cmd52w, cmd53r, caldata,\n" 
	"		: rdcfreg, wrcfreg, rdeeprom bgscanconfig",
	"	[...]	: additional parameters for read registers are",
	"		:	<offset>",
	"		: additional parameters for write registers are",
	"		:	<offset> <value>",
	"		: additional parameters for BlueTooth Co-existence Arbitration (BCA) wrbca are",
	"		:	mode			off | mbca | csrbci",
	"		:	antenna			single | dual",
	"		:	btfreq			off | ib | oob",
	"		:	txprioritylow32		low | high",
	"		:	txpriorityhigh32	low | high",
	"		:	rxprioritylow32		low | high",
	"		:	rxpriorityhigh32	low | high",
	"		: addition parameters for cal data",
	"		: 	< filename >",
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

int process_cmd52r(int argc, char *argv[])
{
	struct ifreq    userdata;
	unsigned char 	buf[6];
	unsigned int 	tmp;

	buf[0] = 0;			//CMD52 read
	if (argc == 5) {
		buf[1] = atoi(argv[3]);		//func
		tmp = strtol(argv[4],NULL,16);	//reg
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
   	printf("cmd52r returns 0x%02X\n",buf[0]);

   	return 0;
}

int process_cmd52w(int argc, char *argv[])
{
   	struct ifreq    userdata;
   	unsigned char 	buf[7];
   	unsigned int 	tmp;

	buf[0] = 1;			//CMD52 write
	if (argc == 6) {
		buf[1] = atoi(argv[3]);		//func
		tmp = strtol(argv[4],NULL,16);	//reg
		buf[2] = tmp & 0xff;
		buf[3] = (tmp >> 8) & 0xff;
		buf[4] = (tmp >> 16) & 0xff;
		buf[5] = (tmp >> 24) & 0xff;
		buf[6] = strtol(argv[5],NULL,16);	//dat
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
   	printf("cmd52w returns 0x%02X\n",buf[0]);

   	return 0;
}

int process_cmd53r(void)
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

#ifdef BG_SCAN
/* Config Background command */
int process_bg_scan_config(int argc, char *argv[])
{
	int				count;
	u8				*buf, scanCfg[256];
	FILE				*fp;
	wlan_ioctl_bg_scan_config	*bgscan_config;
	struct ifreq			userdata;

	bgscan_config = (wlan_ioctl_bg_scan_config*) scanCfg;
	memset(bgscan_config, 0, sizeof(scanCfg));

	buf = (u8 *) &bgscan_config->Type;
	
	if (argc != 4) {
		printf("Error: invalid no of arguments\n");
		printf("Syntax: ./wlanconfig eth1 caldata <filename>\n");
		exit(1);
	}
	
	if ((fp = fopen(argv[3], "r")) == NULL) {
		fprintf(stderr, "Cannot open file %s\n", argv[1]);
		exit(1);
	}

	count = fparse_for_hex(fp, buf+3);

	buf[0] = BG_SCAN_CONFIG;
	buf[1] = count & 0xff;
	buf[2] = (count & 0xff00) >> 8;
	fclose(fp);

	strncpy(userdata.ifr_name, DevName, IFNAMSIZ);
	userdata.ifr_data = (char *) bgscan_config;

	if (ioctl(sockfd, WLAN_SETCONF_GETCONF, &userdata)) {
		fprintf(stderr, "wlanconfig: CAL DATA not supported by %s\n", 
								DevName);
			return -1;
   	}

	return 0;
}
#endif

/* To Change the calibration */
int process_cal_data(int argc, char *argv[])
{
	int			i, count;
	u8			*buf;
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
	
	if ((fp = fopen(argv[3], "r")) == NULL) {
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
/*
int ascii2hex(unsigned char *d, char *s, uint dlen)
{
	int             	len, count;
	char			*buf, *tok;
	static const char 	*delims = "-:";

	len = strlen(s);
	if (!(buf = malloc(len + 2))) {
		fprintf(stderr, "Out of memory!\n");
		exit(1);
	}

	memset(buf, 0, len + 2);
	strncpy(buf, s, len);
	
	for (tok = strtok(buf, delims), count = 0;
			count < dlen && tok; 
			tok = strtok(NULL, delims), count++) {
		*d++ = a2hex(tok);
		printf(" dest %d", *d);
	}

	*d = '\0';

	free(buf);

	return count;
}
*/

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
