/*
 * Mapping for Ebony user flash
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>
#include <asm/io.h>
#include <asm/ibm440.h>
#include <platforms/ebony.h>

static struct mtd_info *flash;

static __u8 ebony_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

static __u16 ebony_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

static __u32 ebony_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

static void ebony_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

static void ebony_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

static void ebony_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

static void ebony_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

static void ebony_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct map_info ebony_small_map = {
	name: "Ebony small flash",
	size: EBONY_SMALL_FLASH_SIZE,
	buswidth: 1,
	read8: ebony_read8,
	read16: ebony_read16,
	read32: ebony_read32,
	copy_from: ebony_copy_from,
	write8: ebony_write8,
	write16: ebony_write16,
	write32: ebony_write32,
	copy_to: ebony_copy_to,
};

static struct map_info ebony_large_map = {
	name: "Ebony large flash",
	size: EBONY_LARGE_FLASH_SIZE,
	buswidth: 1,
	read8: ebony_read8,
	read16: ebony_read16,
	read32: ebony_read32,
	copy_from: ebony_copy_from,
	write8: ebony_write8,
	write16: ebony_write16,
	write32: ebony_write32,
	copy_to: ebony_copy_to,
};

static struct mtd_partition ebony_small_partitions[] = {
	{
		name: "OpenBIOS",
		offset: 0x0,
		size: 0x80000,
	}
};

static struct mtd_partition ebony_large_partitions[] = {
	{
		name: "fs",
		offset: 0,
		size:   0x380000,
	},
	{
		name: "firmware",
		offset: 0x380000,
		size: 0x80000,
	}
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

int __init init_ebony(void)
{
	u8 fpga0_reg;
	unsigned long long small_flash_base, large_flash_base;

	fpga0_reg = readb(ioremap64(EBONY_FPGA_ADDR, 16));

	if (EBONY_BOOT_SMALL_FLASH(fpga0_reg) &&
			!EBONY_FLASH_SEL(fpga0_reg))
		small_flash_base = EBONY_SMALL_FLASH_HIGH2;
	else if (EBONY_BOOT_SMALL_FLASH(fpga0_reg) &&
			EBONY_FLASH_SEL(fpga0_reg))
		small_flash_base = EBONY_SMALL_FLASH_HIGH1;
	else if (!EBONY_BOOT_SMALL_FLASH(fpga0_reg) &&
			!EBONY_FLASH_SEL(fpga0_reg))
		small_flash_base = EBONY_SMALL_FLASH_LOW2;
	else
		small_flash_base = EBONY_SMALL_FLASH_LOW1;
			
	if (EBONY_BOOT_SMALL_FLASH(fpga0_reg) &&
			!EBONY_ONBRD_FLASH_EN(fpga0_reg))
		large_flash_base = EBONY_LARGE_FLASH_LOW;
	else
		large_flash_base = EBONY_LARGE_FLASH_HIGH;

	ebony_small_map.map_priv_1 =
		(unsigned long)ioremap64(small_flash_base,
					 ebony_small_map.size);

	if (!ebony_small_map.map_priv_1) {
		printk("Failed to ioremap flash\n");
		return -EIO;
	}

	flash = do_map_probe("map_rom", &ebony_small_map);
	if (flash) {
		flash->module = THIS_MODULE;
		add_mtd_partitions(flash, ebony_small_partitions,
					NB_OF(ebony_small_partitions));
	} else {
		printk("map probe failed for flash\n");
		return -ENXIO;
	}

	ebony_large_map.map_priv_1 =
		(unsigned long)ioremap64(large_flash_base,
					 ebony_large_map.size);

	if (!ebony_large_map.map_priv_1) {
		printk("Failed to ioremap flash\n");
		return -EIO;
	}

	flash = do_map_probe("cfi_probe", &ebony_large_map);
	if (flash) {
		flash->module = THIS_MODULE;
		add_mtd_partitions(flash, ebony_large_partitions,
					NB_OF(ebony_large_partitions));
	} else {
		printk("map probe failed for flash\n");
		return -ENXIO;
	}

	return 0;
}

static void __exit cleanup_ebony(void)
{
	if (flash) {
		del_mtd_partitions(flash);
		map_destroy(flash);
	}

	if (ebony_small_map.map_priv_1) {
		iounmap((void *)ebony_small_map.map_priv_1);
		ebony_small_map.map_priv_1 = 0;
	}

	if (ebony_large_map.map_priv_1) {
		iounmap((void *)ebony_large_map.map_priv_1);
		ebony_large_map.map_priv_1 = 0;
	}
}

module_init(init_ebony);
module_exit(cleanup_ebony);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ebony flash map");
