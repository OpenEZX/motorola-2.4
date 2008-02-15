/*
 * $Id:
 *
 * Map driver for the Lubbock developer platform.
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>


#define WINDOW_ADDR 	0X00000000

/* Susan:  16MB is for P1,P2 and P3B -- Ezx */
/*      #define WINDOW_SIZE 	16*1024*1024 */
/* Susan:  32MB is for P3A -- Ezx */
#define WINDOW_SIZE 	32*1024*1024
#define BUSWIDTH 	2

/* Added by Susan Gu */
unsigned long DALHART_K3_BASE_ADDR = 0x00000000;
/* End of modification */

static __u8 lubbock_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 lubbock_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 lubbock_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void lubbock_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void lubbock_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void lubbock_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void lubbock_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void lubbock_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

//static struct map_info lubbock_map = {
struct map_info lubbock_map = {
	name: "Lubbock flash",
	size: WINDOW_SIZE,
	read8:		lubbock_read8,
	read16:		lubbock_read16,
	read32:		lubbock_read32,
	copy_from:	lubbock_copy_from,
	write8:		lubbock_write8,
	write16:	lubbock_write16,
	write32:	lubbock_write32,
	copy_to:	lubbock_copy_to
};

/* Modified by Susan */
#if (0)
static struct mtd_partition lubbock_partitions[] = {
	{
		name:		"Bootloader",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"Kernel",
		size:		0x00100000,
		offset:		0x00040000,
	},{
		name:		"Filesystem",
		size:		MTDPART_SIZ_FULL,
		offset:		0x00140000
	}
};
#endif
static struct mtd_partition lubbock_partitions[] = {
	{
		name:		"Bootloader",
		size:		0x00020000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"Kernel",
		size:		0x000E0000,
		offset:		0x00020000,
	},{
		name:		"VFM_Filesystem",
#ifdef A768
		size:		0x00500000,  //Susan -- 5MB in A768//
#else
		size:		0x00900000,  //Susan -- 9MB in A760//
#endif
//Susan -- for VR request;		offset:     0x01400000,
		offset:     0x01000000,
	},{
		name:		"Logo",
		size:		0x00020000,
		offset:		0x01FC0000, //Logo size is 1 EB
	}
};
/* End of modification */

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);

extern int ezx_partition_init(struct map_info *chip_map);

static int __init init_lubbock(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	int i;
	char *part_type = "static";

	int ret = 0;

    /* MV3.0 --	lubbock_map.buswidth = (BOOT_DEF & 1) ? 2 : 4; */
	lubbock_map.buswidth = BUSWIDTH;  //Susan

	printk( "Probing Lubbock flash at physical address 0x%08x (%d-bit buswidth)\n",
		WINDOW_ADDR, lubbock_map.buswidth * 8 );
	lubbock_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, 0);

	/* Added by Susan */
	DALHART_K3_BASE_ADDR = lubbock_map.map_priv_1;
	/* End of modification */
	
	#ifdef CONFIG_K3_DEBUG
	printk("The virtual address of K3 flash device is at %d\n", lubbock_map.map_priv_1);
	#endif
	
	if (!lubbock_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &lubbock_map);
	if (!mymtd) {
		iounmap((void *)lubbock_map.map_priv_1);
		return -ENXIO;
	}
	mymtd->module = THIS_MODULE;

    /* MV3.0 --
	 Unlock the flash device.
	for (i = 0; i < mymtd->numeraseregions; i++) {
		int j;
		for( j = 0; j < mymtd->eraseregions[i].numblocks; j++) {
			mymtd->unlock(mymtd, mymtd->eraseregions[i].offset +
				      j * mymtd->eraseregions[i].erasesize, 4);
		}
	}
    -- MV3.0 */

#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	} else {
		parts = lubbock_partitions;
		nb_parts = NB_OF(lubbock_partitions);
	}
	if (nb_parts) {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	} else {
		add_mtd_device(mymtd);
	}
	
/* Added by Susan Gu */
#ifdef CONFIG_ROFLASH
	if ( ret = ezx_partition_init(&lubbock_map) );
		return -EIO; 
#endif
/* End of midification */

	return 0;
}

static void __exit cleanup_lubbock(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (lubbock_map.map_priv_1)
		iounmap((void *)lubbock_map.map_priv_1);
	return 0;
}

module_init(init_lubbock);
module_exit(cleanup_lubbock);



