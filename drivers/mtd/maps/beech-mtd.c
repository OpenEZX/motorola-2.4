/*
 * drivers/mtd/maps/beech-mtd.c MTD mappings and partition tables for 
 *                              IBM 405LP Beech boards.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (C) 2002, International Business Machines Corporation
 * All Rights Reserved.
 *
 * Bishop Brock
 * IBM Research, Austin Center for Low-Power Computing
 * bcbrock@us.ibm.com
 * March 2002
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/io.h>
#include <asm/ibm4xx.h>

#define NAME     "Beech Linux Flash"
#define PADDR    BEECH_BIGFLASH_PADDR
#define SIZE     BEECH_BIGFLASH_SIZE
#define BUSWIDTH 1

/* Flash memories on these boards are memory resources, accessed big-endian. */

static __u8
beech_mtd_read8(struct map_info *map, unsigned long offset)
{
	return __raw_readb(map->map_priv_1 + offset);
}

static __u16
beech_mtd_read16(struct map_info *map, unsigned long offset)
{
	return __raw_readw(map->map_priv_1 + offset);
}

static __u32
beech_mtd_read32(struct map_info *map, unsigned long offset)
{
	return __raw_readl(map->map_priv_1 + offset);
}

static void
beech_mtd_copy_from(struct map_info *map,
		    void *to, unsigned long from, ssize_t len)
{
	memcpy(to, (void *) (map->map_priv_1 + from), len);
}

static void
beech_mtd_write8(struct map_info *map, __u8 data, unsigned long address)
{
	__raw_writeb(data, map->map_priv_1 + address);
	mb();
}

static void
beech_mtd_write16(struct map_info *map, __u16 data, unsigned long address)
{
	__raw_writew(data, map->map_priv_1 + address);
	mb();
}

static void
beech_mtd_write32(struct map_info *map, __u32 data, unsigned long address)
{
	__raw_writel(data, map->map_priv_1 + address);
	mb();
}

static void
beech_mtd_copy_to(struct map_info *map,
		  unsigned long to, const void *from, ssize_t len)
{
	memcpy((void *) (map->map_priv_1 + to), from, len);
}

static struct map_info beech_mtd_map = {
	name:NAME,
	size:SIZE,
	buswidth:BUSWIDTH,
	read8:beech_mtd_read8,
	read16:beech_mtd_read16,
	read32:beech_mtd_read32,
	copy_from:beech_mtd_copy_from,
	write8:beech_mtd_write8,
	write16:beech_mtd_write16,
	write32:beech_mtd_write32,
	copy_to:beech_mtd_copy_to
};

static struct mtd_info *beech_mtd;

static struct mtd_partition beech_partitions[2] = {
	{
	      name:"Linux Kernel",
	      size:BEECH_KERNEL_SIZE,
      offset:BEECH_KERNEL_OFFSET}, {
	      name:		"Free Area",
	      size:		BEECH_FREE_AREA_SIZE,
      offset:			BEECH_FREE_AREA_OFFSET}
};

static int __init
init_beech_mtd(void)
{
	printk("%s: 0x%08x at 0x%08x\n", NAME, SIZE, PADDR);

	beech_mtd_map.map_priv_1 = (unsigned long) ioremap(PADDR, SIZE);

	if (!beech_mtd_map.map_priv_1) {
		printk("%s: failed to ioremap 0x%x\n", NAME, PADDR);
		return -EIO;
	}

	printk("%s: probing %d-bit flash bus\n", NAME, BUSWIDTH * 8);
	beech_mtd = do_map_probe("cfi_probe", &beech_mtd_map);

	if (!beech_mtd)
		return -ENXIO;

	beech_mtd->module = THIS_MODULE;

	return add_mtd_partitions(beech_mtd, beech_partitions, 2);
}

static void __exit
cleanup_beech_mtd(void)
{
	if (beech_mtd) {
		del_mtd_partitions(beech_mtd);
		map_destroy(beech_mtd);
		iounmap((void *) beech_mtd_map.map_priv_1);
	}
}

module_init(init_beech_mtd);
module_exit(cleanup_beech_mtd);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bishop Brock, bcbrock@us.ibm.com");
MODULE_DESCRIPTION("MTD map and partitions for IBM 405LP Beech boards");
