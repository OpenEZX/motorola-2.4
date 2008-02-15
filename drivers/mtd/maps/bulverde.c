/*
 * $Id:
 *
 * Map driver for the Bulverde developer platform.
 
 * Author: Echo Engineering (Stolen from Nicolas Pitre's lubbock.c):
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 *  Intel MTD patch
 *  modified 04/07/2004 Jared Hulbert <jared dot e dot hulbert at intel dot com>
 *   - added cached read changes
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#ifdef CONFIG_MTD_CACHED_READS
#include <asm/string.h>
#include <asm/pgtable.h>
#define MCLSIZE	32 /* Size of cacheline in Bytes */
#endif




#define BULVERDE_DEBUGGING 0
#if BULVERDE_DEBUGGING
static unsigned int bulverde_debug = BULVERDE_DEBUGGING;
#else
#define bulverde_debug 0
#endif

#ifdef CONFIG_ARCH_EZX

#ifdef CONFIG_ARCH_EZX_A780
#define WINDOW_ADDR 	0
//#define WINDOW_ADDR 	0x04000000
/* total flash memory is 32M, split between two chips */
#define WINDOW_SIZE 	32*1024*1024
#define WINDOW_CACHE_ADDR 0x0
#define WINDOW_CACHE_SIZE 0x1A00000
#endif

#ifdef CONFIG_ARCH_EZX_E680
#define WINDOW_ADDR 	0
//#define WINDOW_ADDR 	0x04000000
/* total flash memory is 32M, split between two chips */
#define WINDOW_SIZE 	32*1024*1024
#define WINDOW_CACHE_ADDR 0x0
#define WINDOW_CACHE_SIZE 0x1A00000
#endif

/* Added by Susan Gu */
extern struct mtd_info *ezx_mymtd;
//unsigned long BULVERDE_L18_BASE_ADDR = 0x00000000;
unsigned long bulverde_map_cacheable = 0x0;
/* End of modification */

#else
#define WINDOW_ADDR 	0
#define WINDOW_SIZE 	32*1024*1024
#endif

#ifdef CONFIG_MTD_CACHED_READS
static void bulverde_invalidate_cache( unsigned long startaddress, unsigned long length)
{
	unsigned long endaddress, i, j;
	endaddress = startaddress + length -1;
	startaddress &= ~( MCLSIZE - 1);
	endaddress &= ~(MCLSIZE - 1);
	for(i=startaddress; i <= endaddress; i = i +MCLSIZE )
	{
		/*This line invalidates one line of the cache*/
		asm("mcr	p15, 0, %0, c7, c6, 1"::"r"(i)); 

	}
	
	/*Waits until CP15 changes are done*/
	asm(	"mrc p15, 0, %0, c2 ,c0, 0\n"
		"mov %0, %0\n"
		"sub pc, pc, #4"
		:"=r"(j)); 
	
}
#endif


static __u8 bulverde_read8(struct map_info *map, unsigned long ofs)
{
	return *(__u8 *)(map->map_priv_1 + ofs);
}

static __u16 bulverde_read16(struct map_info *map, unsigned long ofs)
{
	return *(__u16 *)(map->map_priv_1 + ofs);
}

static __u32 bulverde_read32(struct map_info *map, unsigned long ofs)
{
	return *(__u32 *)(map->map_priv_1 + ofs);
}

static void bulverde_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
#ifdef CONFIG_MTD_CACHED_READS
	/* map->map_priv_2 is mapped as a cacheable bufferable address*/
	memcpy(to, (void *)(map->map_priv_2 + from), len);
#else
 	memcpy(to, (void *)(map->map_priv_1 + from), len);
#endif	
//	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void bulverde_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	*(__u8 *)(map->map_priv_1 + adr) = d;
}

static void bulverde_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static void bulverde_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	*(__u32 *)(map->map_priv_1 + adr) = d;
}

static void bulverde_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

#ifdef CONFIG_ARCH_EZX
/* Modified by Susan */
//static struct map_info bulverde_map = {
struct map_info bulverde_map = {
	name: "Bulverde flash",
	size: WINDOW_SIZE,
	read8:		bulverde_read8,
	read16:		bulverde_read16,
	read32:		bulverde_read32,
	copy_from:	bulverde_copy_from,
	write8:		bulverde_write8,
	write16:	bulverde_write16,
	write32:	bulverde_write32,
	copy_to:	bulverde_copy_to
};

#ifdef CONFIG_ARCH_EZX_A780
/* Susan changed it */
static struct mtd_partition bulverde_partitions[] = {
	{
		name:		"Bootloader",
		size:		0x00020000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"Kernel",
		size:		0x000E0000,
		offset:		0x00020000,
//	},{
//		name:		"rootfs",
//		size:		0x001000000,
//		offset:     0x00100000,
	},{
		name:		"VFM_Filesystem",
		size:		0x00580000,
		offset:     0x01A00000,
	},{
		name:		"Logo",
		size:		0x00020000,
		offset:		0x01FC0000, //Logo size is 1 EB
	}
};
#endif

#ifdef CONFIG_ARCH_EZX_E680
/* Susan changed it */
static struct mtd_partition bulverde_partitions[] = {
	{
		name:		"Bootloader",
		size:		0x00020000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"Kernel",
		size:		0x000E0000,
		offset:		0x00020000,
//	},{
//		name:		"rootfs",
//		size:		0x001000000,
//		offset:     0x00100000,
	},{
		name:		"VFM_Filesystem",
		size:		0x00580000,
		offset:     0x01A00000,
	},{
		name:		"Logo",
		size:		0x00020000,
		offset:		0x01FC0000, //Logo size is 1 EB
	}
};
#endif

#else  //original code --mvlcee

static struct map_info bulverde_map = {
	name: "Bulverde flash",
	size: WINDOW_SIZE,
	read8:		bulverde_read8,
	read16:		bulverde_read16,
	read32:		bulverde_read32,
	copy_from:	bulverde_copy_from,
	write8:		bulverde_write8,
	write16:	bulverde_write16,
	write32:	bulverde_write32,
	copy_to:	bulverde_copy_to
};

static struct mtd_partition bulverde_partitions[] = {
	{
		name:		"Bootloader",
		size:		0x00040000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		"Kernel",
		size:		0x00200000,
		offset:		0x00040000,
	},{
		name:		"Filesystem",
		size:		MTDPART_SIZ_FULL,
		offset:		0x00240000
	}
};
#endif


#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);

#ifdef CONFIG_ARCH_EZX
static int __init init_bulverde(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type = "static";
	int i;
	unsigned long region_offset = 0;  //Susan
	int ret;

	bulverde_map.buswidth = (BOOT_DEF & 1) ? 2 : 4;

	printk( "Probing Bulverde flash at physical address 0x%08x (%d-bit buswidth)\n",
		WINDOW_ADDR, bulverde_map.buswidth * 8 );
	bulverde_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, 0);
	
	if (!bulverde_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}

	mymtd = do_map_probe("cfi_probe", &bulverde_map);
	if (!mymtd) {
		iounmap((void *)bulverde_map.map_priv_1);
		return -ENXIO;
	}
	mymtd->module = THIS_MODULE;
	ezx_mymtd = mymtd;    // Be used by unlockdown functions //
	
	/* Susan -- unlock/lockdown relative blocks in L18 chip */
	#ifdef CONFIG_L18_DEBUG
	printk(KERN_NOTICE "init_bulverde(%d): unlock/lockdown EBs in L18 chip\n", current->pid);
	#endif

	ret = cfi_intelext_unlockdown_L18(mymtd);
//	ret=0;

	if (ret)
	{
#ifdef CONFIG_L18_DEBUG
		printk(KERN_NOTICE "cfi_intelext_unlockdown_L18 fail\n");
#endif
		iounmap((void *)bulverde_map.map_priv_1);
		return -ENXIO;
	}

	if (WINDOW_CACHE_SIZE)  //There is cacheable memory mapping
	{	
		/* Susan -- ioremap the first flash chip as cacheable( && bufferable? ) */
		bulverde_map_cacheable = (unsigned long)__ioremap(WINDOW_CACHE_ADDR, WINDOW_CACHE_SIZE, L_PTE_CACHEABLE);
		if (!bulverde_map_cacheable) 
		{
			printk("Failed to do cacheable-ioremap\n");
			iounmap((void *)bulverde_map.map_priv_1);
			return -EIO;
		}
		
	}
	else  // There is not cacheable memory mapping //
		bulverde_map_cacheable = 0xffffffff;

#ifdef CONFIG_MTD_CACHED_READS
	bulverde_map.invalidate_cache = bulverde_invalidate_cache;	
#endif	
	/*Susan -- comment it out, lock down method is not supported by mtd_info  */
	/* Unlock the flash device.
	if (bulverde_debug)
	  printk("init_bulverde(): unlocking flash device\n");
	if(mymtd->unlock){
		for (i = 0; i < mymtd->numeraseregions; i++) {
			int j;

			if (bulverde_debug) {
			  printk("init_bulverde(): unlocking region %d:\n"
					 "  numblocks is %lu\n"
					 "  offset is %lu\n"
					 "  erasesize is %lu\n",
					 i,
					 (unsigned long)mymtd->eraseregions[i].numblocks,
					 (unsigned long)mymtd->eraseregions[i].offset,
					 (unsigned long)mymtd->eraseregions[i].erasesize);
			}
			for (j = 0; j < mymtd->eraseregions[i].numblocks; j++){
				mymtd->unlock(mymtd, mymtd->eraseregions[i].offset +
							  j * mymtd->eraseregions[i].erasesize,
							  mymtd->eraseregions[i].erasesize);
			}
			if (bulverde_debug)
			  printk("init_bulverde(): number of blocks was %d\n", j);
		}
		if (bulverde_debug)
		  printk("init_bulverde(): number of eraseregions was %d\n", i);
	}
Susan -- End of unlock */

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
		parts = bulverde_partitions;
		nb_parts = NB_OF(bulverde_partitions);
	}

	if (nb_parts) {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	} else {
		add_mtd_device(mymtd);
	}

	if ( ret = ezx_partition_init() )
	{
		iounmap((void *)bulverde_map.map_priv_1);
		if ( bulverde_map_cacheable && (bulverde_map_cacheable != 0xffffffff) )
			iounmap((void *)bulverde_map_cacheable);
			
		return -EIO;
	}
	
	return 0;
}

#else  //original code //

void bulverde_mtd_unlock_all(void)
{
	int i;

	/* Unlock the flash device. */
	if (bulverde_debug)
	  printk("bulverde_mtd_unlock_all(): unlocking flash device\n");
	if(mymtd->unlock){
		for (i = 0; i < mymtd->numeraseregions; i++) {
			int j;

			if (bulverde_debug) {
			  printk("bulverde_mtd_unlock_all(): unlocking region %d:\n"
					 "  numblocks is %lu\n"
					 "  offset is %lu\n"
					 "  erasesize is %lu\n",
					 i,
					 (unsigned long)mymtd->eraseregions[i].numblocks,
					 (unsigned long)mymtd->eraseregions[i].offset,
					 (unsigned long)mymtd->eraseregions[i].erasesize);
			}
			for (j = 0; j < mymtd->eraseregions[i].numblocks; j++){
				mymtd->unlock(mymtd, mymtd->eraseregions[i].offset +
							  j * mymtd->eraseregions[i].erasesize,
							  mymtd->eraseregions[i].erasesize);
			}
			if (bulverde_debug)
			  printk("bulverde_mtd_unlock_all(): number of blocks was %d\n", j);
		}
		if (bulverde_debug)
		  printk("bulverde_mtd_unlock_all(): number of eraseregions was %d\n", i);
	}

	return;
}

static int __init init_bulverde(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type = "static";

	bulverde_map.buswidth = (BOOT_DEF & 1) ? 2 : 4;

	printk( "Probing Bulverde flash at physical address 0x%08x (%d-bit buswidth)\n",
		WINDOW_ADDR, bulverde_map.buswidth * 8 );
	bulverde_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, 0);
	if (!bulverde_map.map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	mymtd = do_map_probe("cfi_probe", &bulverde_map);
	if (!mymtd) {
		iounmap((void *)bulverde_map.map_priv_1);
		return -ENXIO;
	}
	mymtd->module = THIS_MODULE;

	/* Unlock the flash device. */
	bulverde_mtd_unlock_all();

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
		parts = bulverde_partitions;
		nb_parts = NB_OF(bulverde_partitions);
	}
	if (nb_parts) {
		printk(KERN_NOTICE "Using %s partition definition\n", part_type);
		add_mtd_partitions(mymtd, parts, nb_parts);
	} else {
		add_mtd_device(mymtd);
	}
	return 0;
}

#endif
static void __exit cleanup_bulverde(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (bulverde_map.map_priv_1)
		iounmap((void *)bulverde_map.map_priv_1);
	if ( (bulverde_map_cacheable) && (bulverde_map_cacheable != 0xffffffff) )
		iounmap((void *)bulverde_map_cacheable);
	return;
}

module_init(init_bulverde);
module_exit(cleanup_bulverde);

