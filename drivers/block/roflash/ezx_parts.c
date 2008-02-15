/*
 * drivers/mtd/maps/ezx_parts.c
 *
 * Parse multiple flash partitions in Ezx project into mtd_table or into act_roflash_table
 *
 * Created by Susan Gu  Oct, 22  2002
 * 
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/ezx_roflash.h>

extern int roflash_init(void);

extern int cfi_intelext_read_roflash(void *priv_map, unsigned long from, size_t len, size_t *retlen, u_char *buf);
extern int cfi_intelext_read_roflash_c(void *priv_map, unsigned long from, size_t len, size_t *retlen, u_char *buf);
roflash_area **roflash_table_pptr = NULL;

static DECLARE_MUTEX(roflash_table_mutex);

/* I know what I am doing, so setup l_x_b flag */
#ifdef CONFIG_ARCH_EZX_A780
static roflash_area  fix_roflash_table[]= 
{
	{
		name:		"rootfs",
		size:		0x018E0000,  /* rootfs size is 24.875MB */
		offset:		0x00120000,
		roflash_read:   NULL,  /* force read-only in cacheable mapping*/
		l_x_b:		ROFLASH_LINEAR,
		phys_addr:	0x00000000,
	},{
		name:		"NO-USE",  /* language package is moved to MDOC */
		size:		0x00000000,
		offset:		0x01A00000,  //Susan -- correspond to the beginning of the second flash chip //
		roflash_read:  cfi_intelext_read_roflash,
		l_x_b:		ROFLASH_BLOCK,
		phys_addr:  0xffffffff, // not use for block cramfs mount
	},{
		name:		"setup",
		size:		0x00020000,
		offset:         0x01FA0000,
		roflash_read:  cfi_intelext_read_roflash,
		l_x_b:		ROFLASH_BLOCK,
		phys_addr:  0xffffffff, // not use for block cramfs mount
	}
};
#if (0)
static roflash_area  fix_roflash_table[]= 
{
	{
		name:		"rootfs",
		size:		0x00f00000,  /* rootfs size is 15MB */
		offset:		0x00100000,
		roflash_read:  NULL,  /* force read-only in cacheable mapping*/
		l_x_b:		ROFLASH_LINEAR_XIP,
		phys_addr:	0x00000000,
	},{
		name:		"nxipapp",  /* old language area is 0xAA0000 */
		size:		0x00AA0000,
		offset:		0x00500000,
		roflash_read:  cfi_intelext_read_roflash,
		l_x_b:		ROFLASH_BLOCK,
		phys_addr:  0xffffffff, // not use for block cramfs mount
	},{
		name:		"setup",     /* setup stuff is 1MB */
		size:		0x00020000,
		offset:         0x00FA0000,
		roflash_read:  cfi_intelext_read_roflash,
		l_x_b:		ROFLASH_BLOCK,
		phys_addr:  0xffffffff, //phys_addr is not useful for block cramfs mount
	}
};
#endif

#endif

#ifdef CONFIG_ARCH_EZX_E680
static roflash_area  fix_roflash_table[]= 
{
	{
		name:		"rootfs",
		size:		0x018E0000,  /* rootfs size is 15MB */
		offset:		0x00120000,
		roflash_read:   NULL,  /* force read-only in cacheable mapping*/
		l_x_b:		ROFLASH_LINEAR,
		phys_addr:	0x00000000,
	},{
		name:		"NO-USE",  /* language package is 2MB */
		size:		0x00000000,
		offset:		0x01A00000,  //Susan -- correspond to the beginning of the second flash chip //
		roflash_read:  cfi_intelext_read_roflash,
		l_x_b:		ROFLASH_BLOCK,
		phys_addr:  0xffffffff, // not use for block cramfs mount
	},{
		name:		"setup",     /* setup stuff is 1MB */
		size:		0x00020000,
		offset:         0x01FA0000,  //Susan -- correspond to the beginning of the second flash chip //
		roflash_read:  cfi_intelext_read_roflash,
		l_x_b:		ROFLASH_BLOCK,
		phys_addr:  0xffffffff, // not use for block cramfs mount
	}
};
#endif

roflash_area *act_roflash_table[MAX_ROFLASH] = {0};
unsigned short roflash_partitions = 0;

/* If we decide to use flash partition parse table, this function will parse the fpt - flash partition table,
*  the beginning address of fpt is defined in CONFIG_FPT_OFFSET.  This function will parser out all roflash areas
*  and animation area( or re-fresh code area).   
*  For roflash areas, registered them into act_roflash_table, for animation region, register it into mtd_table[6].
*  Note 
*  6 is fixed for animation region -- /dev/mtd6 c 90 12
*  /dev/roflash b 62 0  -- root file system
*  /dev/roflash1 b 62 1 -- language package
*  /dev/roflash2 b 62 2 -- setup stuff
*/  
int ezx_parse(struct map_info *chip_map)
{
	return 0;  // depends on discussion about DC tool
}

int ezx_parts_parser(void)
{
	int ret = 0;
	int nrparts = 0;
	int i = 0;

//#ifdef CONFIG_EZX_PARSER_TABLE
//	ret = ezx_parse(chip_map);
//#else
	down(&roflash_table_mutex);

	nrparts = sizeof(fix_roflash_table) / sizeof(roflash_area);

#ifdef ROFLASH_DEBUG_ERR
	printk("ezx_parts_parser:  nrparts is %d\n", nrparts);
#endif
	for ( i = 0; i < nrparts; i ++ )
	{
		if (act_roflash_table[i])
			return -1;
		act_roflash_table[i] = (roflash_area *)(&(fix_roflash_table[i]));
#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "ezx_parts_parser: act_roflash_table[%d] = %d\n",i,(unsigned long)(act_roflash_table[i]));
#endif
	}
	roflash_partitions = i;

#ifdef ROFLASH_DEBUG_ERR
	printk("ezx_parts_parser:  roflash_partitions is %d\n", roflash_partitions);
#endif
	
	up(&roflash_table_mutex);
	
	return ret;
}
			
int ezx_partition_init(void)
{
	int ret = 0;

	
#ifdef ROFLASH_DEBUG_ERR
	printk(KERN_ERR "ezx_partition_init: roflash_table_pptr = %d\n",(unsigned long)(act_roflash_table));
#endif
	roflash_table_pptr = (roflash_area **)(act_roflash_table);

	if ( ret = ezx_parts_parser() )
	{
		printk(KERN_NOTICE "invoke ezx_parts_parser, return %d\n", ret);
		return -EIO;
	}

#ifdef ROFLASH_DEBUG_ERR
	printk("ezx_partition_init:  ret of ezx_parts_parser is %d\n", ret);
#endif

	if ( ret = roflash_init() )
	{
		printk(KERN_NOTICE "invoke roflash_init, return %d\n", ret);
		return -EIO;
	}
	return 0 ;
}






