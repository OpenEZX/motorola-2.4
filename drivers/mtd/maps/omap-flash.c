/*
 * Flash memory access on OMAP based devices
 *
 * (C) 2002 MontaVista Software, Inc.
 *
 * $Id: omap-flash.c,v 1.1.4.5.2.1 2003/12/04 23:35:46 gdavis Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/hardware.h>
#include <asm/io.h>


#ifndef CONFIG_ARCH_OMAP
#error This is for OMAP architecture only
#endif


static __u8 omap_read8(struct map_info *map, unsigned long ofs)
{
	return readb(map->map_priv_1 + ofs);
}

static __u16 omap_read16(struct map_info *map, unsigned long ofs)
{
	return readw(map->map_priv_1 + ofs);
}

static __u32 omap_read32(struct map_info *map, unsigned long ofs)
{
	return readl(map->map_priv_1 + ofs);
}

static void omap_copy_from(struct map_info *map, void *to,
	unsigned long from, ssize_t len)
{
	memcpy(to, (void *)(map->map_priv_1 + from), len);
}

static void omap_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	writeb(d, map->map_priv_1 + adr);
}

static void omap_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	writew(d, map->map_priv_1 + adr);
}

static void omap_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	writel(d, map->map_priv_1 + adr);
}

static void omap_copy_to(struct map_info *map, unsigned long to,
	const void *from, ssize_t len)
{
	memcpy((void *)(map->map_priv_1 + to), from, len);
}

static spinlock_t vpp_spin = SPIN_LOCK_UNLOCKED;
static int vpp_counter;

static void
omap_set_vpp(struct map_info *map, int vpp)
{
	volatile u32 * v = (volatile u32 *)(OMAP_EMIFS_CONFIG_REG);

	spin_lock_irq(&vpp_spin);

	if (vpp) {
		vpp_counter += 1;
		*v |= OMAP_EMIFS_CONFIG_WP;
	} else {
		if (vpp_counter > 0)
			vpp_counter -= 1;
 
		if (vpp_counter == 0)
			*v &= ~OMAP_EMIFS_CONFIG_WP;
	}
 
	spin_unlock_irq(&vpp_spin);
}


static struct map_info omap_map_flash_0 = {
	name:		"OMAP flash",
	read8:		omap_read8,
	read16:		omap_read16,
	read32:		omap_read32,
	copy_from:	omap_copy_from,
	write8:		omap_write8,
	write16:	omap_write16,
	write32:	omap_write32,
	copy_to:	omap_copy_to,
	set_vpp:	omap_set_vpp,
	buswidth:	2,
	map_priv_1:	OMAP_FLASH_0_BASE,
};

static struct map_info omap_map_flash_1 = {
	name:		"OMAP flash",
	read8:		omap_read8,
	read16:		omap_read16,
	read32:		omap_read32,
	copy_from:	omap_copy_from,
	write8:		omap_write8,
	write16:	omap_write16,
	write32:	omap_write32,
	copy_to:	omap_copy_to,
	set_vpp:	omap_set_vpp,
	buswidth:	2,
	map_priv_1:	OMAP_FLASH_1_BASE,
};


/*
 * Here are partition information for all known OMAP-based devices.
 * See include/linux/mtd/partitions.h for definition of the mtd_partition
 * structure.
 *
 * The *_max_flash_size is the maximum possible mapped flash size which
 * is not necessarily the actual flash size.  It must be no more than
 * the value specified in the "struct map_desc *_io_desc" mapping
 * definition for the corresponding machine.
 *
 * Please keep these in alphabetical order, and formatted as per existing
 * entries.  Thanks.
 */

#if	defined(CONFIG_OMAP_INNOVATOR)
/* Both Flash 0 and 1 are the same size */
static unsigned long innovator_max_flash_size = OMAP_FLASH_0_SIZE;
static struct mtd_partition innovator_flash_0_partitions[] = {
	{
		name:		"BootLoader",
		size:		0x00020000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"Params",
		size:		0x00040000,
		offset:		MTDPART_OFS_APPEND,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"Kernel",
		size:		0x00200000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		name:		"Flash0 FileSys",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};

static struct mtd_partition innovator_flash_1_partitions[] = {
	{
		name:		"Flash1 FileSys",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif

#if	defined(CONFIG_OMAP_PERSEUS)
static unsigned long perseus_max_flash_size = OMAP_FLASH_CS0_SIZE;
static struct mtd_partition perseus_partitions[] = {
	{
		name:		"Bootloader",
		size:		0x00100000,
		offset:		0,
		mask_flags:	MTD_WRITEABLE,  /* force read-only */
	}, {
		name:		"zImage",
		size:		0x000a0000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		name:		"Root FS",
		size:		0x00660000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		name:		"User1 FS",
		size:		0x00400000,
		offset:		MTDPART_OFS_APPEND,
	}, {
		name:		"User2 FS",
		size:		MTDPART_SIZ_FULL,
		offset:		MTDPART_OFS_APPEND,
	}
};
#endif



extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
extern int parse_bootldr_partitions(struct mtd_info *master, struct mtd_partition **pparts);

static struct mtd_partition *parsed_parts;

static struct mtd_info *flash0_mtd;
static struct mtd_info *flash1_mtd;

#ifdef CONFIG_MTD_OMAP_0
static int __init init_flash_0 (void)
{

	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	const char *part_type;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";

#if	defined(CONFIG_OMAP_INNOVATOR)
	if (machine_is_innovator()) {
		parts = innovator_flash_0_partitions;
		nb_parts = ARRAY_SIZE(innovator_flash_0_partitions);
		omap_map_flash_0.size = innovator_max_flash_size;
	}
#endif
#if	defined(CONFIG_OMAP_PERSEUS)
	if (machine_is_perseus()) {
		parts = perseus_partitions;
		nb_parts = ARRAY_SIZE(perseus_partitions);
		omap_map_flash_0.size = perseus_max_flash_size;
	}
#endif

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "OMAP flash 0: probing %d-bit flash bus\n",
		omap_map_flash_0.buswidth*8);
	flash0_mtd = do_map_probe("cfi_probe", &omap_map_flash_0);
	if (!flash0_mtd)
		return -ENXIO;
	flash0_mtd->module = THIS_MODULE;

	/*
	 * Dynamic partition selection stuff (might override the static ones)
	 */
#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(flash0_mtd, &parsed_parts);

		if (ret > 0) {
			part_type = "RedBoot";
			parsed_nr_parts = ret;
		}
	}
#endif
#ifdef CONFIG_MTD_BOOTLDR_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_bootldr_partitions(flash0_mtd, &parsed_parts);
		if (ret > 0) {
			part_type = "Compaq bootldr";
			parsed_nr_parts = ret;
		}
	}
#endif

	if (parsed_nr_parts > 0) {
		parts = parsed_parts;
		nb_parts = parsed_nr_parts;
	}

	if (nb_parts == 0) {
		printk(KERN_NOTICE "OMAP flash: no partition info available,"
			"registering whole flash at once\n");
		add_mtd_device(flash0_mtd);
	} else {
		printk(KERN_NOTICE "Using %s partition definition\n",
			part_type);
		add_mtd_partitions(flash0_mtd, parts, nb_parts);
	}
	return 0;
}
#endif

#if	defined(CONFIG_OMAP_INNOVATOR)
static int __init init_flash_1 (void)
{

	struct mtd_partition *parts;
	int nb_parts = 0;
	const char *part_type;

	/*
	 * Static partition definition selection
	 */
	part_type = "static";

	if (machine_is_innovator()) {
		parts = innovator_flash_1_partitions;
		nb_parts = ARRAY_SIZE(innovator_flash_1_partitions);
		omap_map_flash_1.size = innovator_max_flash_size;
	}

	/*
	 * Now let's probe for the actual flash.  Do it here since
	 * specific machine settings might have been set above.
	 */
	printk(KERN_NOTICE "OMAP flash 1: probing %d-bit flash bus\n",
		omap_map_flash_1.buswidth*8);
	flash1_mtd = do_map_probe("cfi_probe", &omap_map_flash_1);
	if (!flash1_mtd)
		return -ENXIO;
	flash1_mtd->module = THIS_MODULE;

	if (nb_parts == 0) {
		printk(KERN_NOTICE "OMAP flash 1: no partition info available,"
			"registering whole flash at once\n");
		add_mtd_device(flash1_mtd);
	} else {
		printk(KERN_NOTICE "Using %s partition definition\n",
			part_type);
		add_mtd_partitions(flash1_mtd, parts, nb_parts);
	}
	return 0;
}
#endif

int __init omap_mtd_init(void)
{
	int status;

#ifdef CONFIG_MTD_OMAP_0
	if (status = init_flash_0 ()) {
		printk(KERN_ERR "OMAP Flash 0: unable to init map\n");
	}
#endif
#if defined(CONFIG_OMAP_INNOVATOR) && defined(CONFIG_MTD_OMAP_1)
	if (status = init_flash_1 ()) {
		printk(KERN_ERR "OMAP Flash 1: unable to init map\n");
	}
#endif
	return status;
}

static void __exit omap_mtd_cleanup(void)
{
	if (flash0_mtd) {
		del_mtd_partitions(flash0_mtd);
		map_destroy(flash0_mtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
	if (flash1_mtd) {
		del_mtd_partitions(flash1_mtd);
		map_destroy(flash1_mtd);
		if (parsed_parts)
			kfree(parsed_parts);
	}
}

module_init(omap_mtd_init);
module_exit(omap_mtd_cleanup);

MODULE_AUTHOR("George G. Davis");
MODULE_DESCRIPTION("OMAP CFI map driver");
MODULE_LICENSE("GPL");
