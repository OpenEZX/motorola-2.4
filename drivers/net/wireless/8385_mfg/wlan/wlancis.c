/*
 *	File : wlancis.c
 * 
 *	Program to get the cis info from the wlan driver
 * 
 *	Usage: wlancis <ethX> [outfile] 
 */

#include	<stdio.h>
#include	<fcntl.h>
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
//#include	<utils.h>

#include	"wlan_wext.h"

#ifndef	le16_to_cpu
#define	le16_to_cpu(x)	(x)
#endif

typedef unsigned char	cisdata_t;

/* Bits in IRQInfo1 field */
#define IRQ_MASK		0x0f
#define IRQ_NMI_ID		0x01
#define IRQ_IOCK_ID		0x02
#define IRQ_BERR_ID		0x04
#define IRQ_VEND_ID		0x08
#define IRQ_INFO2_VALID		0x10
#define IRQ_LEVEL_ID		0x20
#define IRQ_PULSE_ID		0x40
#define IRQ_SHARE_ID		0x80

/* Return codes */
#define CS_SUCCESS		0x00
#define CS_UNSUPPORTED_FUNCTION	0x15
#define CS_BAD_TUPLE		0x40

#define CISTPL_DEVICE		0x01
#define CISTPL_CONFIG_CB	0x04
#define CISTPL_LINKTARGET	0x13
#define CISTPL_NO_LINK		0x14
#define CISTPL_VERS_1		0x15
#define CISTPL_DEVICE_A		0x17
#define CISTPL_CONFIG		0x1a
#define CISTPL_CFTABLE_ENTRY	0x1b
#define CISTPL_MANFID		0x20
#define CISTPL_FUNCID		0x21
#define CISTPL_FUNCE		0x22
#define CISTPL_END		0xff

#define CISTPL_MAX_DEVICES	4

typedef struct cistpl_device_t {
	unsigned char	ndev;
	struct {
		unsigned char	type;
		unsigned char	wp;
		unsigned int	speed;
		unsigned int	size;
	} dev[CISTPL_MAX_DEVICES];
} cistpl_device_t;

#define CISTPL_VERS_1_MAX_PROD_STRINGS	4

typedef struct cistpl_vers_1_t {
	unsigned char	major;
	unsigned char	minor;
	unsigned char	ns;
	unsigned char	ofs[CISTPL_VERS_1_MAX_PROD_STRINGS];
	char		str[254];
} cistpl_vers_1_t;

typedef struct cistpl_manfid_t {
	unsigned short	manf;
	unsigned short	card;
} cistpl_manfid_t;

#define CISTPL_FUNCID_NETWORK	0x06

#define CISTPL_SYSINIT_POST	0x01
#define CISTPL_SYSINIT_ROM	0x02

typedef struct cistpl_funcid_t {
	unsigned char	func;
	unsigned char	sysinit;
} cistpl_funcid_t;

typedef struct cistpl_funce_t {
	unsigned char	type;
	unsigned char	data[0];
} cistpl_funce_t;

/*======================================================================

    LAN Function Extension Tuples

======================================================================*/

#define CISTPL_FUNCE_LAN_TECH		0x01
#define CISTPL_FUNCE_LAN_SPEED		0x02
#define CISTPL_FUNCE_LAN_MEDIA		0x03
#define CISTPL_FUNCE_LAN_NODE_ID	0x04
#define CISTPL_FUNCE_LAN_CONNECTOR	0x05

/* LAN technologies */
#define CISTPL_LAN_TECH_ARCNET		0x01
#define CISTPL_LAN_TECH_ETHERNET	0x02
#define CISTPL_LAN_TECH_TOKENRING	0x03
#define CISTPL_LAN_TECH_LOCALTALK	0x04
#define CISTPL_LAN_TECH_FDDI		0x05
#define CISTPL_LAN_TECH_ATM		0x06
#define CISTPL_LAN_TECH_WIRELESS	0x07

typedef struct cistpl_lan_tech_t {
	unsigned char	tech;
} cistpl_lan_tech_t;

typedef struct cistpl_lan_speed_t {
	unsigned int	speed;
} cistpl_lan_speed_t;

/* LAN media definitions */
#define CISTPL_LAN_MEDIA_UTP		0x01
#define CISTPL_LAN_MEDIA_STP		0x02
#define CISTPL_LAN_MEDIA_THIN_COAX	0x03
#define CISTPL_LAN_MEDIA_THICK_COAX	0x04
#define CISTPL_LAN_MEDIA_FIBER		0x05
#define CISTPL_LAN_MEDIA_900MHZ		0x06
#define CISTPL_LAN_MEDIA_2GHZ		0x07
#define CISTPL_LAN_MEDIA_5GHZ		0x08
#define CISTPL_LAN_MEDIA_DIFF_IR	0x09
#define CISTPL_LAN_MEDIA_PTP_IR		0x0a

typedef struct cistpl_lan_media_t {
	unsigned char	media;
} cistpl_lan_media_t;

typedef struct cistpl_lan_node_id_t {
	unsigned char	nb;
	unsigned char	id[16];
} cistpl_lan_node_id_t;

typedef struct cistpl_lan_connector_t {
	unsigned char	code;
} cistpl_lan_connector_t;

/*======================================================================

    Configuration Table Entries

======================================================================*/

typedef struct cistpl_config_t {
	unsigned char	last_idx;
	unsigned int	base;
	unsigned int	rmask[4];
	unsigned char	subtuples;
} cistpl_config_t;

/* These are bits in the 'present' field, and indices in 'param' */
#define CISTPL_POWER_VNOM	0
#define CISTPL_POWER_VMIN	1
#define CISTPL_POWER_VMAX	2
#define CISTPL_POWER_ISTATIC	3
#define CISTPL_POWER_IAVG	4
#define CISTPL_POWER_IPEAK	5
#define CISTPL_POWER_IDOWN	6

#define CISTPL_POWER_HIGHZ_OK	0x01
#define CISTPL_POWER_HIGHZ_REQ	0x02

typedef struct cistpl_power_t {
	unsigned char	present;
	unsigned char	flags;
	unsigned int	param[7];
} cistpl_power_t;

typedef struct cistpl_timing_t {
	unsigned int	wait, waitscale;
	unsigned int	ready, rdyscale;
	unsigned int	reserved, rsvscale;
} cistpl_timing_t;

#define CISTPL_IO_LINES_MASK	0x1f
#define CISTPL_IO_8BIT		0x20
#define CISTPL_IO_16BIT		0x40
#define CISTPL_IO_RANGE		0x80

#define CISTPL_IO_MAX_WIN	16

typedef struct cistpl_io_t {
	unsigned char	flags;
	unsigned char	nwin;
	struct {
		unsigned int	base;
		unsigned int	len;
	} win[CISTPL_IO_MAX_WIN];
} cistpl_io_t;

typedef struct cistpl_irq_t {
	unsigned int	IRQInfo1;
	unsigned int	IRQInfo2;
} cistpl_irq_t;

#define CISTPL_MEM_MAX_WIN	8

typedef struct cistpl_mem_t {
	unsigned char	flags;
	unsigned char	nwin;
	struct {
		unsigned int	len;
		unsigned int	card_addr;
		unsigned int	host_addr;
	} win[CISTPL_MEM_MAX_WIN];
} cistpl_mem_t;

#define CISTPL_CFTABLE_DEFAULT		0x0001
#define CISTPL_CFTABLE_BVDS		0x0002
#define CISTPL_CFTABLE_WP		0x0004
#define CISTPL_CFTABLE_RDYBSY		0x0008
#define CISTPL_CFTABLE_MWAIT		0x0010
#define CISTPL_CFTABLE_AUDIO		0x0800
#define CISTPL_CFTABLE_READONLY		0x1000
#define CISTPL_CFTABLE_PWRDOWN		0x2000

typedef struct cistpl_cftable_entry_t {
	unsigned char	index;
	unsigned short	flags;
	unsigned char	interface;
	cistpl_power_t	vcc, vpp1, vpp2;
	cistpl_timing_t	timing;
	cistpl_io_t	io;
	cistpl_irq_t	irq;
	cistpl_mem_t	mem;
	unsigned char	subtuples;
} cistpl_cftable_entry_t;

typedef union cisparse_t {
	cistpl_device_t		device;
	cistpl_vers_1_t		version_1;
	cistpl_manfid_t		manfid;
	cistpl_funcid_t		funcid;
	cistpl_funce_t		funce;
	cistpl_config_t		config;
	cistpl_cftable_entry_t	cftable_entry;
} cisparse_t;

typedef struct tuple_t {
	unsigned int	CISOffset;	/* internal use */
	cisdata_t	TupleCode;
	cisdata_t	TupleLink;
	cisdata_t	TupleOffset;
	cisdata_t	TupleDataMax;
	cisdata_t	TupleDataLen;
	cisdata_t	*TupleData;
} tuple_t;

#define CISTPL_MAX_CIS_SIZE	0x200

typedef struct tuple_parse_t {
	tuple_t		tuple;
	cisdata_t	data[255];
	cisparse_t	parse;
} tuple_parse_t;

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

static char    *usage[] = {
	"Usage: wlancis <ethX> [outputfile]",
};

void display_usage(void)
{
	int             i;

	for (i = 0; i < (sizeof(usage) / sizeof(usage[0])); i++)
		fprintf(stderr, "%s\n", usage[i]);
}

static char indent[10] = "  ";

#ifdef DEBUG
static void print_tuple(tuple_parse_t *tup)
{
	int	i;
	
	printf("%soffset 0x%2.2x, tuple 0x%2.2x, link 0x%2.2x\n",
			indent, tup->tuple.CISOffset, tup->tuple.TupleCode,
			tup->tuple.TupleLink);

	for (i = 0; i < tup->tuple.TupleDataLen; i++) {
		if ((i % 16) == 0) printf("%s  ", indent);
		printf("%2.2x ", (unsigned char)tup->data[i]);
		if ((i % 16) == 15) putchar('\n');
	}
	
	if ((i % 16) != 0) putchar('\n');
}
#endif	/* DEBUG */

static void print_funcid(cistpl_funcid_t *fn)
{
	printf("%sfuncid ", indent);

	switch (fn->func) {
	case CISTPL_FUNCID_NETWORK:
		printf("network_adapter"); break;
	default:
		printf("unknown");
		break;
	}

	if (fn->sysinit & CISTPL_SYSINIT_POST)
		printf(" [post]");
	if (fn->sysinit & CISTPL_SYSINIT_ROM)
		printf(" [rom]");
	
	putchar('\n');
}

static void print_size(unsigned int size)
{
	if (size < 1024)
		printf("%ub", size);
	else if (size < 1024*1024)
		printf("%ukb", size/1024);
	else
		printf("%umb", size/(1024*1024));
}

static void print_unit(unsigned int v, char *unit, char tag)
{
	int	n;

	for (n = 0; (v % 1000) == 0; n++) v /= 1000;
	
	printf("%u", v);

	if (n < strlen(unit)) putchar(unit[n]);

	putchar(tag);
}

static void print_time(unsigned int tm, unsigned long scale)
{
	print_unit(tm * scale, "num", 's');
}

static void print_volt(unsigned int vi)
{
	print_unit(vi * 10, "um", 'V');
}
    
static void print_current(unsigned int ii)
{
	print_unit(ii / 10, "um", 'A');
}

static void print_speed(unsigned int b)
{
	if (b < 1000)
		printf("%u bits/sec", b);
	else if (b < 1000000)
		printf("%u kb/sec", b/1000);
	else
		printf("%u mb/sec", b/1000000);
}

static const char *dtype[] = {
    "NULL", "ROM", "OTPROM", "EPROM", "EEPROM", "FLASH", "SRAM",
    "DRAM", "rsvd", "rsvd", "rsvd", "rsvd", "rsvd", "fn_specific",
    "extended", "rsvd"
};

static void print_device(cistpl_device_t *dev)
{
	int	i;

	for (i = 0; i < dev->ndev; i++) {
		printf("%s  %s ", indent, dtype[dev->dev[i].type]);
		printf("%uns, ", dev->dev[i].speed);
		print_size(dev->dev[i].size);
		putchar('\n');
	}

	if (dev->ndev == 0)
		printf("%s  no_info\n", indent);
}

static void print_power(char *tag, cistpl_power_t *power)
{
	int	i, n;

	for (i = n = 0; i < 8; i++)
		if (power->present & (1<<i)) n++;
	
	i = 0;
	printf("%s  %s", indent, tag);

	if (power->present & (1<<CISTPL_POWER_VNOM)) {
		printf(" Vnom "); i++;
		print_volt(power->param[CISTPL_POWER_VNOM]);
	}
	
	if (power->present & (1<<CISTPL_POWER_VMIN)) {
		printf(" Vmin "); i++;
		print_volt(power->param[CISTPL_POWER_VMIN]);
	}

	if (power->present & (1<<CISTPL_POWER_VMAX)) {
		printf(" Vmax "); i++;
		print_volt(power->param[CISTPL_POWER_VMAX]);
	}

	if (power->present & (1<<CISTPL_POWER_ISTATIC)) {
		printf(" Istatic "); i++;
		print_current(power->param[CISTPL_POWER_ISTATIC]);
	}

	if (power->present & (1<<CISTPL_POWER_IAVG)) {
		if (++i == 5) printf("\n%s   ", indent);
		printf(" Iavg ");
		print_current(power->param[CISTPL_POWER_IAVG]);
	}

	if (power->present & (1<<CISTPL_POWER_IPEAK)) {
		if (++i == 5) printf("\n%s ", indent);
		printf(" Ipeak ");
		print_current(power->param[CISTPL_POWER_IPEAK]);
	}

	if (power->present & (1<<CISTPL_POWER_IDOWN)) {
		if (++i == 5) printf("\n%s ", indent);
		printf(" Idown ");
		print_current(power->param[CISTPL_POWER_IDOWN]);
	}

	if (power->flags & CISTPL_POWER_HIGHZ_OK) {
		if (++i == 5) printf("\n%s ", indent);
		printf(" [highz OK]");
	}

	if (power->flags & CISTPL_POWER_HIGHZ_REQ) {
		printf(" [highz]");
	}

	putchar('\n');
}

static void print_cftable_entry(cistpl_cftable_entry_t *entry)
{
	int	i;

	printf("%scftable_entry 0x%2.2x%s\n", indent, entry->index,
		(entry->flags & CISTPL_CFTABLE_DEFAULT) ? " [default]" : "");

	if (entry->flags & ~CISTPL_CFTABLE_DEFAULT) {
		printf("%s ", indent);
		if (entry->flags & CISTPL_CFTABLE_BVDS)
			printf(" [bvd]");
		if (entry->flags & CISTPL_CFTABLE_WP)
			printf(" [wp]");
		if (entry->flags & CISTPL_CFTABLE_RDYBSY)
			printf(" [rdybsy]");
		if (entry->flags & CISTPL_CFTABLE_MWAIT)
			printf(" [mwait]");
		if (entry->flags & CISTPL_CFTABLE_AUDIO)
			printf(" [audio]");
		if (entry->flags & CISTPL_CFTABLE_READONLY)
			printf(" [readonly]");
		if (entry->flags & CISTPL_CFTABLE_PWRDOWN)
			printf(" [pwrdown]");
		
		putchar('\n');
	}

	if (entry->vcc.present)
		print_power("Vcc", &entry->vcc);
	if (entry->vpp1.present)
		print_power("Vpp1", &entry->vpp1);
	if (entry->vpp2.present)
		print_power("Vpp2", &entry->vpp2);
	if ((entry->timing.wait != 0) || (entry->timing.ready != 0) ||
				(entry->timing.reserved != 0)) {
		printf("%s  timing", indent);
		if (entry->timing.wait != 0) {
			printf(" wait ");
			print_time(entry->timing.wait, entry->timing.waitscale);
		}
		if (entry->timing.ready != 0) {
			printf(" ready ");
			print_time(entry->timing.ready, entry->timing.rdyscale);
		}
		if (entry->timing.reserved != 0) {
			printf(" reserved ");
			print_time(entry->timing.reserved, 
						entry->timing.rsvscale);
		}
		
		putchar('\n');
	}
    
	if (entry->io.nwin) {
		cistpl_io_t *io = &entry->io;
		printf("%s  io", indent);
		for (i = 0; i < io->nwin; i++) {
			if (i) putchar(',');
			printf(" 0x%4.4x-0x%4.4x", io->win[i].base,
					   io->win[i].base+io->win[i].len-1);
		}

		printf(" [lines=%d]", io->flags & CISTPL_IO_LINES_MASK);
		if (io->flags & CISTPL_IO_8BIT) printf(" [8bit]");
		if (io->flags & CISTPL_IO_16BIT) printf(" [16bit]");
		if (io->flags & CISTPL_IO_RANGE) printf(" [range]");
		putchar('\n');
	}

	if (entry->irq.IRQInfo1) {
		printf("%s  irq ", indent);
		
		if (entry->irq.IRQInfo1 & IRQ_INFO2_VALID)
			printf("mask 0x%04x", entry->irq.IRQInfo2);
		else
			printf("%u", entry->irq.IRQInfo1 & IRQ_MASK);
		
		if (entry->irq.IRQInfo1 & IRQ_LEVEL_ID) printf(" [level]");
		if (entry->irq.IRQInfo1 & IRQ_PULSE_ID) printf(" [pulse]");
		if (entry->irq.IRQInfo1 & IRQ_SHARE_ID) printf(" [shared]");
		putchar('\n');
	}

	if (entry->mem.nwin) {
		cistpl_mem_t *mem = &entry->mem;
		printf("%s  memory", indent);
		for (i = 0; i < mem->nwin; i++) {
			if (i) putchar(',');
			printf(" 0x%4.4x-0x%4.4x @ 0x%4.4x", 
				mem->win[i].card_addr,
				mem->win[i].card_addr + mem->win[i].len-1,
				mem->win[i].host_addr);
		}
		putchar('\n');
	}

	if (entry->subtuples)
		printf("%s  %d bytes in subtuples\n", indent, entry->subtuples);
    
}

static const char *tech[] = {
	"undefined", "ARCnet", "ethernet", "token_ring",
	"localtalk", "FDDI/CDDI", "ATM", "wireless"
};

static const char *media[] = {
	"undefined", "unshielded_twisted_pair", "shielded_twisted_pair",
	"thin_coax", "thick_coax", "fiber", "900_MHz", "2.4_GHz",
	"5.4_GHz", "diffuse_infrared", "point_to_point_infrared"
};

static void print_network(cistpl_funce_t *funce)
{
	cistpl_lan_tech_t	*t;
	cistpl_lan_speed_t	*s;
	cistpl_lan_media_t	*m;
	cistpl_lan_node_id_t	*n;
	cistpl_lan_connector_t	*c;
	int			i;
    
	switch (funce->type) {
	case CISTPL_FUNCE_LAN_TECH:
		t = (cistpl_lan_tech_t *)(funce->data);
		printf("%slan_technology %s\n", indent, tech[t->tech]);
		break;
	case CISTPL_FUNCE_LAN_SPEED:
		s = (cistpl_lan_speed_t *)(funce->data);
		printf("%slan_speed ", indent);
		print_speed(s->speed);
		putchar('\n');
		break;
	case CISTPL_FUNCE_LAN_MEDIA:
		m = (cistpl_lan_media_t *)(funce->data);
		printf("%slan_media %s\n", indent, media[m->media]);
		break;
	case CISTPL_FUNCE_LAN_NODE_ID:
		n = (cistpl_lan_node_id_t *)(funce->data);
		printf("%slan_node_id", indent);
		for (i = 0; i < n->nb; i++)
			printf(" %02x", n->id[i]);
		putchar('\n');
		break;
	case CISTPL_FUNCE_LAN_CONNECTOR:
		c = (cistpl_lan_connector_t *)(funce->data);
		printf("%slan_connector ", indent);
		if (c->code == 0)
			printf("Open connector standard\n");
		else
			printf("Closed connector standard\n");
		break;
	}
}

static void print_vers_1(cistpl_vers_1_t *v1)
{
	int	i, n;
	char	s[32];
	
	sprintf(s, "%svers_1 %d.%d", indent, v1->major, v1->minor);
	printf("%s", s);
	n = strlen(s);
	for (i = 0; i < v1->ns; i++) {
		if (n + strlen(v1->str + v1->ofs[i]) + 4 > 72) {
			n = strlen(indent) + 2;
			printf(",\n%s  ", indent);
		} else {
			printf(", ");
			n += 2;
		}
		
		printf("\"%s\"", v1->str + v1->ofs[i]);
		n += strlen(v1->str + v1->ofs[i]) + 2;
	}
	
	putchar('\n');
}

static void print_config(int code, cistpl_config_t *cfg)
{
	printf("%sconfig%s base 0x%4.4x", indent,
			(code == CISTPL_CONFIG_CB) ? "_cb" : "", cfg->base);

	if (code == CISTPL_CONFIG)
		printf(" mask 0x%4.4x", cfg->rmask[0]);
	
	printf(" last_index 0x%2.2x\n", cfg->last_idx);

	if (cfg->subtuples)
		printf("%s  %d bytes in subtuples\n", indent, cfg->subtuples);
}

static void print_parse(tuple_parse_t *tup)
{
	static int	func = 0;

	switch (tup->tuple.TupleCode) {
	case CISTPL_DEVICE:
	case CISTPL_DEVICE_A:
		if (tup->tuple.TupleCode == CISTPL_DEVICE)
			printf("%sdev_info\n", indent);
		else
			printf("%sattr_dev_info\n", indent);
		print_device(&tup->parse.device);
		break;
	case CISTPL_VERS_1:
		print_vers_1(&tup->parse.version_1);
		break;
	case CISTPL_MANFID:
		printf("%smanfid 0x%4.4x, 0x%4.4x\n", indent,
			tup->parse.manfid.manf, tup->parse.manfid.card);
		break;
	case CISTPL_FUNCID:
		print_funcid(&tup->parse.funcid);
		func = tup->parse.funcid.func;
		break;
	case CISTPL_FUNCE:
		switch (func) {
		case CISTPL_FUNCID_NETWORK:
			print_network(&tup->parse.funce);
			break;
		}
		break;
	case CISTPL_CONFIG:
		print_config(tup->tuple.TupleCode, &tup->parse.config);
		break;
	case CISTPL_CFTABLE_ENTRY:
		print_cftable_entry(&tup->parse.cftable_entry);
		break;
	}
}

static const unsigned char mantissa[] = {
    10, 12, 13, 15, 20, 25, 30, 35,
    40, 45, 50, 55, 60, 70, 80, 90
};

static const unsigned int exponent[] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};

/* Convert an extended speed byte to a time in nanoseconds */
#define SPEED_CVT(v) \
    (mantissa[(((v)>>3)&15)-1] * exponent[(v)&7] / 10)
/* Convert a power byte to a current in 0.1 microamps */
#define POWER_CVT(v) \
    (mantissa[((v)>>3)&15] * exponent[(v)&7] / 10)
#define POWER_SCALE(v)		(exponent[(v)&7])

static int parse_device(tuple_t *tuple, cistpl_device_t *device)
{
	int		i;
	unsigned char	scale, *p, *q;

	p = (unsigned char *)tuple->TupleData;
	q = p + tuple->TupleDataLen;

	device->ndev = 0;

	for (i = 0; i < CISTPL_MAX_DEVICES; i++) {
		if (*p == 0xff) break;
		device->dev[i].type = (*p >> 4);
		device->dev[i].wp = (*p & 0x08) ? 1 : 0;
		switch (*p & 0x07) {
		case 0: device->dev[i].speed = 0;   break;
		case 1: device->dev[i].speed = 250; break;
		case 2: device->dev[i].speed = 200; break;
		case 3: device->dev[i].speed = 150; break;
		case 4: device->dev[i].speed = 100; break;
		case 7:
		    if (++p == q) return CS_BAD_TUPLE;
		    device->dev[i].speed = SPEED_CVT(*p);
		    while (*p & 0x80)
			    if (++p == q) return CS_BAD_TUPLE;
		    break;
		default:
		    return CS_BAD_TUPLE;
		 }
		
		if (++p == q) return CS_BAD_TUPLE;
		if (*p == 0xff) break;

		scale = *p & 7;
		if (scale == 7) return CS_BAD_TUPLE;

		device->dev[i].size = ((*p >> 3) + 1) * (512 << (scale*2));
		device->ndev++;
		if (++p == q) break;
	}

	return CS_SUCCESS;
}

static int parse_strings(unsigned char *p, unsigned char *q, int max,
			 char *s, unsigned char *ofs, unsigned char *found)
{
	int	i, j, ns;

	if (p == q) return CS_BAD_TUPLE;

	ns = 0; j = 0;

	for (i = 0; i < max; i++) {
		if (*p == 0xff) break;
		
		ofs[i] = j;
		ns++;

		for (;;) {
			s[j++] = (*p == 0xff) ? '\0' : *p;
			if ((*p == '\0') || (*p == 0xff)) break;
			if (++p == q) return CS_BAD_TUPLE;
		}
		
		if ((*p == 0xff) || (++p == q)) break;
	}

	if (found) {
		*found = ns;
		return CS_SUCCESS;
	} else {
		return (ns == max) ? CS_SUCCESS : CS_BAD_TUPLE;
	}
}

static int parse_vers_1(tuple_t *tuple, cistpl_vers_1_t *vers_1)
{
	unsigned char	*p, *q;

	p = (unsigned char *)tuple->TupleData;
	q = p + tuple->TupleDataLen;

	vers_1->major = *p; p++;
	vers_1->minor = *p; p++;

	if (p >= q) return CS_BAD_TUPLE;

	return parse_strings(p, q, CISTPL_VERS_1_MAX_PROD_STRINGS,
				       vers_1->str, vers_1->ofs, &vers_1->ns);
}

static int parse_manfid(tuple_t *tuple, cistpl_manfid_t *m)
{
	unsigned short	*p;

	if (tuple->TupleDataLen < 4)
		return CS_BAD_TUPLE;

	p = (unsigned short *)tuple->TupleData;
	m->manf = le16_to_cpu(p[0]);
	m->card = le16_to_cpu(p[1]);
	return CS_SUCCESS;
}

static int parse_funcid(tuple_t *tuple, cistpl_funcid_t *f)
{
	unsigned char	*p;

	if (tuple->TupleDataLen < 2)
		return CS_BAD_TUPLE;

	p = (unsigned char *)tuple->TupleData;
	
	f->func = p[0];
	f->sysinit = p[1];

	return CS_SUCCESS;
}

static int parse_funce(tuple_t *tuple, cistpl_funce_t *f)
{
	unsigned char	*p;
	int		i;

	if (tuple->TupleDataLen < 1)
		return CS_BAD_TUPLE;

	p = (unsigned char *)tuple->TupleData;

	f->type = p[0];

	for (i = 1; i < tuple->TupleDataLen; i++)
		f->data[i-1] = p[i];

	return CS_SUCCESS;
}

static int parse_config(tuple_t *tuple, cistpl_config_t *config)
{
	int		rasz, rmsz, i;
	unsigned char	*p;

	p = (unsigned char *)tuple->TupleData;
	rasz = *p & 0x03;
	rmsz = (*p & 0x3c) >> 2;

	if (tuple->TupleDataLen < rasz+rmsz+4)
		return CS_BAD_TUPLE;

	config->last_idx = *(++p);
	p++;
	config->base = 0;

	for (i = 0; i <= rasz; i++)
		config->base += p[i] << (8*i);
	
	p += rasz+1;
	
	for (i = 0; i < 4; i++)
		config->rmask[i] = 0;

	for (i = 0; i <= rmsz; i++)
		config->rmask[i>>2] += p[i] << (8*(i%4));

	config->subtuples = tuple->TupleDataLen - (rasz+rmsz+4);
	return CS_SUCCESS;
}

static unsigned char *parse_power(unsigned char *p, unsigned char *q, 
							cistpl_power_t *pwr)
{
	int		i;
	unsigned int	scale;

	if (p == q) return NULL;

	pwr->present = *p;
	pwr->flags = 0;
	p++;

	for (i = 0; i < 7; i++)
		if (pwr->present & (1<<i)) {
			if (p == q) return NULL;
			pwr->param[i] = POWER_CVT(*p);
			
			scale = POWER_SCALE(*p);
			while (*p & 0x80) {
				if (++p == q) return NULL;
				if ((*p & 0x7f) < 100)
					pwr->param[i] += 
						(*p & 0x7f) * scale / 100;
				else if (*p == 0x7d)
					pwr->flags |= CISTPL_POWER_HIGHZ_OK;
				else if (*p == 0x7e)
					pwr->param[i] = 0;
				else if (*p == 0x7f)
					pwr->flags |= CISTPL_POWER_HIGHZ_REQ;
				else
					return NULL;
			}
			p++;
		}

	return p;
}

static unsigned char *parse_timing(unsigned char *p, unsigned char *q,
						    cistpl_timing_t *timing)
{
	unsigned char	scale;

	if (p == q) return NULL;
	scale = *p;
	if ((scale & 3) != 3) {
		if (++p == q) return NULL;
		timing->wait = SPEED_CVT(*p);
		timing->waitscale = exponent[scale & 3];
	} else
		timing->wait = 0;
	
	scale >>= 2;
	if ((scale & 7) != 7) {
		if (++p == q) return NULL;
		timing->ready = SPEED_CVT(*p);
		timing->rdyscale = exponent[scale & 7];
	} else
		timing->ready = 0;

	scale >>= 3;
	if (scale != 7) {
		if (++p == q) return NULL;
		timing->reserved = SPEED_CVT(*p);
		timing->rsvscale = exponent[scale];
	} else
		timing->reserved = 0;
	
	p++;
	return p;
}

static unsigned char *parse_io(unsigned char *p, unsigned char *q, 
							cistpl_io_t *io)
{
	int	i, j, bsz, lsz;

	if (p == q) return NULL;
	io->flags = *p;
	
	if (!(*p & 0x80)) {
		io->nwin = 1;
		io->win[0].base = 0;
		io->win[0].len = (1 << (io->flags & CISTPL_IO_LINES_MASK));
		return p+1;
	}

	if (++p == q) return NULL;
	io->nwin = (*p & 0x0f) + 1;
	bsz = (*p & 0x30) >> 4;
	if (bsz == 3) bsz++;
	lsz = (*p & 0xc0) >> 6;
	if (lsz == 3) lsz++;
	p++;
    
	for (i = 0; i < io->nwin; i++) {
		io->win[i].base = 0;
		io->win[i].len = 1;
		for (j = 0; j < bsz; j++, p++) {
			if (p == q) return NULL;
			io->win[i].base += *p << (j*8);
		}
		for (j = 0; j < lsz; j++, p++) {
			if (p == q) return NULL;
			io->win[i].len += *p << (j*8);
		}
	}
	
	return p;
}

static unsigned char *parse_mem(unsigned char *p, unsigned char *q, 
							cistpl_mem_t *mem)
{
	int		i, j, asz, lsz, has_ha;
	unsigned int	len, ca, ha;

	if (p == q) return NULL;
	mem->nwin = (*p & 0x07) + 1;
	lsz = (*p & 0x18) >> 3;
	asz = (*p & 0x60) >> 5;
	has_ha = (*p & 0x80);
	if (++p == q) return NULL;
	
	for (i = 0; i < mem->nwin; i++) {
		len = ca = ha = 0;
		for (j = 0; j < lsz; j++, p++) {
			if (p == q) return NULL;
			len += *p << (j*8);
		}
		for (j = 0; j < asz; j++, p++) {
			if (p == q) return NULL;
			ca += *p << (j*8);
		}
		if (has_ha)
			for (j = 0; j < asz; j++, p++) {
				if (p == q) return NULL;
				ha += *p << (j*8);
			}
			mem->win[i].len = len << 8;
			mem->win[i].card_addr = ca << 8;
			mem->win[i].host_addr = ha << 8;
	}

	return p;
}

static unsigned char *parse_irq(unsigned char *p, unsigned char *q, 
							cistpl_irq_t *irq)
{
	if (p == q) return NULL;
	irq->IRQInfo1 = *p; p++;
	if (irq->IRQInfo1 & IRQ_INFO2_VALID) {
		if (p+2 > q) return NULL;
		irq->IRQInfo2 = (p[1]<<8) + p[0];
		p += 2;
	}
	return p;
}

static int parse_cftable_entry(tuple_t *tuple, cistpl_cftable_entry_t *entry)
{
	unsigned char	*p, *q, features;

	p = tuple->TupleData;
	q = p + tuple->TupleDataLen;
	entry->index = *p & 0x3f;
	entry->flags = 0;
	if (*p & 0x40)
		entry->flags |= CISTPL_CFTABLE_DEFAULT;
	if (*p & 0x80) {
		if (++p == q) return CS_BAD_TUPLE;
		if (*p & 0x10)
			entry->flags |= CISTPL_CFTABLE_BVDS;
		if (*p & 0x20)
			entry->flags |= CISTPL_CFTABLE_WP;
		if (*p & 0x40)
			entry->flags |= CISTPL_CFTABLE_RDYBSY;
		if (*p & 0x80)
			entry->flags |= CISTPL_CFTABLE_MWAIT;
		entry->interface = *p & 0x0f;
	} else
		entry->interface = 0;
	
	/* Process optional features */
	if (++p == q) return CS_BAD_TUPLE;
	features = *p; p++;

	/* Power options */
	if ((features & 3) > 0) {
		p = parse_power(p, q, &entry->vcc);
		if (p == NULL) return CS_BAD_TUPLE;
	} else
		entry->vcc.present = 0;
	
	if ((features & 3) > 1) {
		p = parse_power(p, q, &entry->vpp1);
		if (p == NULL) return CS_BAD_TUPLE;
	} else
		entry->vpp1.present = 0;

	if ((features & 3) > 2) {
		p = parse_power(p, q, &entry->vpp2);
		if (p == NULL) return CS_BAD_TUPLE;
	} else
		entry->vpp2.present = 0;
	
	 /* Timing options */
	if (features & 0x04) {
		p = parse_timing(p, q, &entry->timing);
		if (p == NULL) return CS_BAD_TUPLE;
	} else {
		entry->timing.wait = 0;
		entry->timing.ready = 0;
		entry->timing.reserved = 0;
	}
    
	/* I/O window options */
	if (features & 0x08) {
		p = parse_io(p, q, &entry->io);
		if (p == NULL) return CS_BAD_TUPLE;
	} else
		entry->io.nwin = 0;
	
	/* Interrupt options */
	if (features & 0x10) {
		p = parse_irq(p, q, &entry->irq);
		if (p == NULL) return CS_BAD_TUPLE;
	} else
		entry->irq.IRQInfo1 = 0;
	
	switch (features & 0x60) {
	case 0x00:
		entry->mem.nwin = 0;
		break;
	case 0x20:
		entry->mem.nwin = 1;
		entry->mem.win[0].len = le16_to_cpu(*(unsigned short *)p) << 8;
		entry->mem.win[0].card_addr = 0;
		entry->mem.win[0].host_addr = 0;
		p += 2;
		if (p > q) return CS_BAD_TUPLE;
		break;
	case 0x40:
		entry->mem.nwin = 1;
		entry->mem.win[0].len = le16_to_cpu(*(unsigned short *)p) << 8;
		entry->mem.win[0].card_addr =
				    le16_to_cpu(*(unsigned short *)(p+2)) << 8;
		entry->mem.win[0].host_addr = 0;
		p += 4;
		if (p > q) return CS_BAD_TUPLE;
		break;
	case 0x60:
		p = parse_mem(p, q, &entry->mem);
		if (p == NULL) return CS_BAD_TUPLE;
		break;
	}

	/* Misc features */
	if (features & 0x80) {
		if (p == q) return CS_BAD_TUPLE;
		entry->flags |= (*p << 8);
		while (*p & 0x80)
			if (++p == q) return CS_BAD_TUPLE;
		p++;
	}

	entry->subtuples = q - p;

	return CS_SUCCESS;
}

static int parse_tuple(tuple_t *tuple, cisparse_t *parse)
{
	int	ret = CS_SUCCESS;

	if (tuple->TupleDataLen > tuple->TupleDataMax)
		return CS_BAD_TUPLE;

#ifdef DEBUG
	printf("Tuple Code = 0x%02X\n", tuple->TupleCode);
#endif	/* DEBUG */

	switch (tuple->TupleCode) {
    	case CISTPL_DEVICE:
	case CISTPL_DEVICE_A:
		ret = parse_device(tuple, &parse->device);
		break;
	case CISTPL_VERS_1:
		ret = parse_vers_1(tuple, &parse->version_1);
		break;
	case CISTPL_MANFID:
		ret = parse_manfid(tuple, &parse->manfid);
		break;
	case CISTPL_FUNCID:
		ret = parse_funcid(tuple, &parse->funcid);
		break;
	case CISTPL_FUNCE:
		ret = parse_funce(tuple, &parse->funce);
		break;
    	case CISTPL_CONFIG:
		ret = parse_config(tuple, &parse->config);
		break;
	case CISTPL_CFTABLE_ENTRY:
		ret = parse_cftable_entry(tuple, &parse->cftable_entry);
		break;
	case CISTPL_NO_LINK:
	case CISTPL_LINKTARGET:
		ret = CS_SUCCESS;
		break;
	default:
		ret = CS_UNSUPPORTED_FUNCTION;
		break;
	}

	return ret;
}

static int get_tuple(unsigned char *buf, int nb, tuple_t *tuple, int first)
{
	unsigned int	ofs;

	if (!nb)
		return -1;

	if (first) {
		tuple->TupleLink = tuple->CISOffset = 0;
		tuple->TupleDataMax = 255;
	}

	ofs = tuple->CISOffset + tuple->TupleLink;

	if (ofs >= nb) return -1;

	tuple->TupleCode = buf[ofs++];
	tuple->TupleDataLen = tuple->TupleLink = buf[ofs++];
	tuple->CISOffset = ofs;

	if (tuple->TupleData) {
		memset(tuple->TupleData, 0, 255);
		memcpy(tuple->TupleData, buf + ofs, tuple->TupleLink);
	}

	return CS_SUCCESS;
}

int main(int argc, char *argv[])
{
	int		first, fd = 0, flag = 0, cislen = 0, sockfd = 0;
	unsigned char	cisbuf[512];
	struct iwreq	iwr;
	tuple_parse_t	tp;

	memset(&cisbuf, 0, sizeof(cisbuf));
	memset(&iwr, 0, sizeof(iwr));
	memset(&tp, 0, sizeof(tuple_parse_t));
	
	if (argc < 2) {
		fprintf(stderr, "Invalid number of parameters!\n");
		display_usage();
		exit(1);
	}

	strncpy(iwr.ifr_name, argv[1], IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) cisbuf;
	/*
	 * create a socket 
	 */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "wlancis: Cannot open socket.\n");
		exit(1);
	}

	/* call ioctl */
	if (ioctl(sockfd, WLANCISDUMP, &iwr)) {
		perror("wlancis");
		fprintf(stderr, "wlancis: Cmd unsupported by %s\n", argv[1]);
		exit(1);
	}
	
	cislen = iwr.u.data.length;
	
#ifdef DEBUG
	hexdump("wlan cis", cisbuf, cislen, ' ');
#endif	/* DEBUG */

	if (argc > 2) {
		flag = 1;
		fd = open(argv[2], O_CREAT | O_RDWR | O_TRUNC, 00666); 
		if (fd < 0) {
			perror("wlancis - open error");
			exit(1);
		}
		printf("Open success\n");
	}
	
	if (flag) {
		if (write(fd, cisbuf, cislen) != cislen)
			perror("wlancis - write error");
	}
	
	tp.tuple.TupleDataMax = sizeof(tp.data);
	tp.tuple.TupleOffset = 0;
	tp.tuple.TupleData = tp.data;

	for (first = 1; ; first = 0) {
		if (get_tuple(cisbuf, cislen, &tp.tuple, first))
			break;

#ifdef DEBUG
		print_tuple(&tp);
#endif	/* DEBUG */
		
		if (parse_tuple(&tp.tuple, &tp.parse) == CS_SUCCESS)
			print_parse(&tp);
	    
		if (tp.tuple.TupleCode == CISTPL_END)
			break;
	}
				
	return 0;
}
