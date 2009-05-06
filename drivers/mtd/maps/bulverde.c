/*
 * $Id:
 *
 * Map driver for the Bulverde developer platform.
 *
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * Copyright    (c) 2005,2006 Motorola Inc.
 *
 * Author      Date              Comment
 * ======     ======          ==========================
 * Motorola  2005/01/15       Initial map for EzX  
 * Motorola  2005/10/11       Change memory layout
 * Motorola  2005/12/21	      add MACAU product 
 * Motorola  2006/06/06       change memory layout for macau (Add 1M space to resource area, and reduce 1M from language space)
 * Motorola  2006/06/15       add Penglai Product
 * Motorola  2006/08/09       modify memory map to support multi-product

 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/system.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#define BULVERDE_DEBUGGING 0
#if BULVERDE_DEBUGGING
static unsigned int bulverde_debug = BULVERDE_DEBUGGING;
#else
#define bulverde_debug 0
#endif

#ifdef CONFIG_ARCH_EZXBASE

#define WINDOW_ADDR 	0
//#define WINDOW_ADDR 	0x04000000
/* total flash memory is 32M, split between two chips */
#define WINDOW_SIZE     64*1024*1024  //since P2, dataflash size is 64MB
#define WINDOW_CACHE_ADDR 0x02000000   //The starting of the second chip //0x0
#define WINDOW_CACHE_SIZE 0x02000000   //partial language(no use)+ setup partition + rootfs partition //From physical beginning 0x0 to the end of rootfs is 28MB //

extern struct mtd_info *ezx_mymtd;
unsigned long bulverde_map_cacheable = 0x0;
/* End of modification */

#else  //CONFIG_ARCH_EZXBASE

#define WINDOW_ADDR 	0
//#define WINDOW_ADDR 	0x04000000
/* total flash memory is 32M, split between two chips */
#define WINDOW_SIZE 	32*1024*1024

#endif  //CONFIG_ARCH_EZXBASE

static void bulverde_inval_cache(struct map_info *map, unsigned long from, ssize_t len)
{
	cpu_dcache_invalidate_range(map->map_priv_2 + from, map->map_priv_2 + from + len);
}

#ifdef CONFIG_ARCH_EZXBASE
struct map_info bulverde_map = {
	.name		= "Bulverde flash",
	.size		= WINDOW_SIZE,
//	.inval_cache	= bulverde_inval_cache,
};

static struct mtd_partition bulverde_partitions[] = {
	{
		name:		MMAP_CADDO_SEC_NAME,
		size:		MMAP_CADDO_SEC_SIZE,
		offset:		MMAP_CADDO_SEC_START,
//		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_ITUNES_NAME,
		size:		MMAP_ITUNES_SIZE,
		offset:		MMAP_ITUNES_START,
//		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_CADDO_PRI_NAME,
		size:		MMAP_CADDO_PRI_SIZE,
		offset:		MMAP_CADDO_PRI_START,
//		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_FATO_REV_NAME,
		size:		MMAP_FATO_REV_SIZE,
		offset:		MMAP_FATO_REV_START,
//		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_MBM_NAME,
		size:		MMAP_MBM_SIZE,
		offset:		MMAP_MBM_START,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_CDT_KCT_NAME,
		size:		MMAP_CDT_KCT_SIZE,
		offset:		MMAP_CDT_KCT_START,
	},{
		name:		MMAP_BLOB_NAME,
		size:		MMAP_BLOB_SIZE,
		offset:		MMAP_BLOB_START,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_KERNEL_NAME,
		size:		MMAP_KERNEL_SIZE,
		offset:		MMAP_KERNEL_START,
		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_USERDB_NAME,
		size:		MMAP_USERDB_SIZE,
		offset:		MMAP_USERDB_START,
	},{
		name:		MMAP_USERGEN_NAME,
		size:		MMAP_USERGEN_SIZE,
		offset:		MMAP_USERGEN_START,
	},{
		name:		MMAP_TCMD_NAME,
		size:		MMAP_TCMD_SIZE,
		offset:		MMAP_TCMD_START,
	},{
		name:		MMAP_LOGO_NAME,
		size:		MMAP_LOGO_SIZE,
		offset:		MMAP_LOGO_START,
//		mask_flags:	MTD_WRITEABLE  /* force read-only */
	},{
		name:		MMAP_FOTA_NAME,
		size:		MMAP_FOTA_SIZE,
		offset:		MMAP_FOTA_START,
	},{
		name:		MMAP_SYS_REV_NAME,
		size:		MMAP_SYS_REV_SIZE,
		offset:		MMAP_SYS_REV_START,
	}
};
#else  //CONFIG_ARCH_EZXBASE

static struct map_info bulverde_map = {
	.name		= "Bulverde flash",
	.size		= WINDOW_SIZE,
	.inval_cache	= bulverde_inval_cache,
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
#endif  //CONFIG_ARCH_EZXBASE

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *mymtd;
static struct mtd_partition *parsed_parts;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts, unsigned long fis_origin);

#ifdef CONFIG_ARCH_EZXBASE
extern int cfi_intelext_unlockdown(struct mtd_info *mymtd);
extern int ezx_partition_init(void);

static int __init init_bulverde(void)
{
	struct mtd_partition *parts;
	int nb_parts = 0;
	int parsed_nr_parts = 0;
	char *part_type = "static";
	int ret;

	bulverde_map.bankwidth = (BOOT_DEF & 1) ? 2 : 4;

	printk( "Probing Bulverde flash at physical address 0x%08x (%d-bit buswidth)\n",
		WINDOW_ADDR, bulverde_map.bankwidth * 8 );
	bulverde_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, 0);
#ifdef CONFIG_ARCH_EZXBASE
	bulverde_map.map_priv_2 = NULL;
	if (!bulverde_map.map_priv_1) {

#else
	bulverde_map.map_priv_2 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, L_PTE_CACHEABLE);
	if (!bulverde_map.map_priv_1 || !bulverde_map.map_priv_2) {
#endif
		printk("init_bulverde: Failed to ioremap\n");
		if (bulverde_map.map_priv_1)
			iounmap((void *)bulverde_map.map_priv_1);
		if (bulverde_map.map_priv_2)
			iounmap((void *)bulverde_map.map_priv_2);
		return -EIO;
	}

	if (bulverde_map.map_priv_2 == NULL)
		bulverde_map.inval_cache = NULL;
	else
		bulverde_map.inval_cache = bulverde_inval_cache;

	bulverde_map.virt = bulverde_map.map_priv_1;
	bulverde_map.cached = bulverde_map.map_priv_2;
	mymtd = do_map_probe("cfi_probe", &bulverde_map);
	if (!mymtd) {
		iounmap((void *)bulverde_map.map_priv_1);
		iounmap((void *)bulverde_map.map_priv_2);
		return -ENXIO;
	}
	mymtd->owner = THIS_MODULE;
	ezx_mymtd = mymtd;    //Be used by unlockdown functions //
	
	/* unlock/lockdown relative blocks in L18 chip */
	#ifdef CONFIG_INTELFLASH_DEBUG
	printk(KERN_NOTICE "init_bulverde(%d): unlock/lockdown EBs in Intel-Flash-chip\n", current->pid);
	#endif

	ret = cfi_intelext_unlockdown(mymtd);

	if (ret)
	{
#ifdef CONFIG_INTELFLASH_DEBUG
		printk(KERN_NOTICE "cfi_intelext_unlockdown fail\n");
#endif
		iounmap((void *)bulverde_map.map_priv_1);
		iounmap((void *)bulverde_map.map_priv_2);
		return -ENXIO;
	}

	if (WINDOW_CACHE_SIZE)  //There is cacheable memory mapping
	{	
		/*ioremap the first flash chip as cacheable( && bufferable? ) */
		bulverde_map_cacheable = (unsigned long)__ioremap(WINDOW_CACHE_ADDR, WINDOW_CACHE_SIZE, L_PTE_CACHEABLE);
		if (!bulverde_map_cacheable) 
		{
			printk("Failed to do cacheable-ioremap which is specific for Ezx product\n");
			iounmap((void *)bulverde_map.map_priv_1);
			iounmap((void *)bulverde_map.map_priv_2);
			return -EIO;
		}
		
	}
	else  // There is not cacheable memory mapping //
		bulverde_map_cacheable = 0xffffffff;

#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts, 0);

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
	
	ret = ezx_partition_init();
	if (ret)
	{
		iounmap((void *)bulverde_map.map_priv_1);
		iounmap((void *)bulverde_map.map_priv_2);
		if ( bulverde_map_cacheable && (bulverde_map_cacheable != 0xffffffff) )
			iounmap((void *)bulverde_map_cacheable);			
		return -EIO;
	}
	
	return 0;
}

#else  //MVLCEE3.1 orginal code //

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

	bulverde_map.bankwidth = (BOOT_DEF & 1) ? 2 : 4;

	printk( "Probing Bulverde flash at physical address 0x%08x (%d-bit buswidth)\n",
		WINDOW_ADDR, bulverde_map.bankwidth * 8 );
	bulverde_map.map_priv_1 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, 0);
	bulverde_map.map_priv_2 = (unsigned long)__ioremap(WINDOW_ADDR, WINDOW_SIZE, L_PTE_CACHEABLE);
	if (!bulverde_map.map_priv_1 || !bulverde_map.map_priv_2) {
		printk("Failed to ioremap\n");
		if (bulverde_map.map_priv_1)
			iounmap((void *)bulverde_map.map_priv_1);
		if (bulverde_map.map_priv_2)
			iounmap((void *)bulverde_map.map_priv_2);
		return -EIO;
	}
	bulverde_map.virt = bulverde_map.map_priv_1;
	bulverde_map.cached = bulverde_map.map_priv_2;
	mymtd = do_map_probe("cfi_probe", &bulverde_map);
	if (!mymtd) {
		iounmap((void *)bulverde_map.map_priv_1);
		iounmap((void *)bulverde_map.map_priv_2);
		return -ENXIO;
	}
	mymtd->owner = THIS_MODULE;

	/* Unlock the flash device. */
	bulverde_mtd_unlock_all();

#ifdef CONFIG_MTD_REDBOOT_PARTS
	if (parsed_nr_parts == 0) {
		int ret = parse_redboot_partitions(mymtd, &parsed_parts, 0);

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
#endif  //CONFIG_ARCH_EZXBASE
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
	if (bulverde_map.map_priv_2)
		iounmap((void *)bulverde_map.map_priv_2);
	return;
}

module_init(init_bulverde);
module_exit(cleanup_bulverde);

