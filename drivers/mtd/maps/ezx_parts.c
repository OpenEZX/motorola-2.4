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

#ifdef CONFIG_ROFLASH
extern int roflash_init(void);
#endif

extern int cfi_intelext_read_roflash (unsigned long from, size_t len, size_t *retlen, u_char *buf);
extern roflash_area **roflash_table_pptr;

static DECLARE_MUTEX(roflash_table_mutex);
static roflash_area  fix_roflash_table[]= 
{
	{
		name:		"rootfs",
		size:		0x00f00000,  /* rootfs size is 15MB */
		offset:		0x00100000,
		roflash_read:  cfi_intelext_read_roflash,  /* force read-only */
	},{
		name:		"language",  /* language package is 2MB */
//Susan -- VR request;		size:		0x003A0000,
//Susan -- VR request;		offset:		0x01c00000,
#ifdef A768
		size:		0x00AA0000,  //Susan, 0xAA0000 for language in A768 //
		offset:		0x01500000,
#else
		size:		0x006A0000,  //Susan, 0x6A0000 for language in A760 //
		offset:		0x01900000,
#endif
		roflash_read:  cfi_intelext_read_roflash,
	},{
		name:		"setup",     /* setup stuff is 1MB */
		size:		0x00020000,
		offset:         0x01FA0000,
		roflash_read:  cfi_intelext_read_roflash,
	}
};

roflash_area *act_roflash_table[MAX_CRAMFS] = {0};
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

int ezx_parts_parser(struct map_info *chip_map)
{
	int ret = 0;
	int nrparts = 0;
	int i = 0;

#ifdef CONFIG_EZX_PARSER_TABLE
	ret = ezx_parse(chip_map);
#else
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
#endif

	return ret;
}
			
int ezx_partition_init(struct map_info *chip_map)
{
	int ret = 0;

	if (!chip_map)
		return -1;
	
#ifdef ROFLASH_DEBUG_ERR
	printk(KERN_ERR "ezx_partition_init: roflash_table_pptr = %d\n",(unsigned long)(act_roflash_table));
#endif
	roflash_table_pptr = (roflash_area **)(act_roflash_table);

	if ( ret = ezx_parts_parser(chip_map) )
		return -EIO;

#ifdef ROFLASH_DEBUG_ERR
	printk("ezx_partition_init:  ret of ezx_parts_parser is %d\n", ret);
#endif

	if ( ret = roflash_init() )
		return -EIO;

	return 0 ;
}






