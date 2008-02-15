/* 
 * drivers/mtd/mtdhiddenpart.c
 *  
 * Copyright (c) 2006 Motorola Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Operate mtd hidden area.
 *
 * Created 2006/03/02
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mtd/mtdhiddenpart.h>

#include <asm/io.h>
#include <asm/uaccess.h>

//#define MTD_HIDDEN_DEBUG

#ifdef MTD_HIDDEN_DEBUG
#define MTDH_WARNING(fmt, ...) \
	printk(KERN_WARNING "MTD_HIDDEN %s: "  fmt, __FUNCTION__, ##__VA_ARGS__)
#define MTDH_INFO(fmt, ...) \
	printk(KERN_INFO "MTD_HIDDEN %s: " fmt, __FUNCTION__, ##__VA_ARGS__)
#else
#define MTDH_WARNING(fmt, ...)
#define MTDH_INFO(fmt, ...)
#endif

#define ERASE_BLOCK_SIZE 	0x20000
#define ERASE_BLOCK_MASK 	~(ERASE_BLOCK_SIZE - 1)

static int erase_normal (struct mtd_hidden_info *mtd_hinfo)
{
	struct erase_info *instr;
	int ret;

	MTDH_INFO("Normal erase hidden part\n");
	instr = (struct erase_info *)kmalloc(sizeof(struct erase_info), GFP_KERNEL);
	if(!instr)
		return -ENOMEM;

	memset(instr, 0, sizeof(struct erase_info));

	instr->mtd = *mtd_hinfo->master;
	instr->addr = (mtd_hinfo->start & ERASE_BLOCK_MASK);
	instr->len = ERASE_BLOCK_SIZE;
	instr->callback = NULL;
	instr->priv = NULL;
	instr->fail_addr = 0xffffffff;

	ret = (*mtd_hinfo->master)->erase(*mtd_hinfo->master, instr);
	if(ret) {
		MTDH_WARNING("Erase failed %d !\n", ret);
		return ret;
	}
	MTDH_INFO("Erase succeed!\n");
	kfree(instr);
	return 0;
}


static int erase_panic (struct mtd_hidden_info *mtd_hinfo)
{
	void *area_map;
	volatile unsigned short *ptr;
	
	MTDH_INFO("MTD_HIDDEN: Panic erase hidden part\n");
	area_map = __ioremap((mtd_hinfo->start & ERASE_BLOCK_MASK), ERASE_BLOCK_SIZE, 0);
	if(!area_map)
		return -ENOMEM;

	ptr = (unsigned short *)area_map;
	/* wait state ready */
	*ptr = 0x70;
	while (!((*ptr) & 0x80));

	/* unlock the block */
	*ptr = 0x60; *ptr = 0xd0;
	while (!((*ptr) & 0x80));

	/* erase the block */
	*ptr = 0x20; *ptr = 0xd0;
	while (!((*ptr) & 0x80));
	
	MTDH_INFO("Panic erase succeed!\n");
	__iounmap(area_map);
	return 0;
}

static int read_normal (struct mtd_hidden_info *mtd_hinfo, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	//MTDH_INFO(" Normal read hidden part\n");
	return (*mtd_hinfo->master)->read(*mtd_hinfo->master, mtd_hinfo->start + from, len, retlen, buf);
}
	
static int read_panic (struct mtd_hidden_info *mtd_hinfo, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	//MTDH_INFO("Panic read hidden part\n");
	return (*mtd_hinfo->master)->read(*mtd_hinfo->master, mtd_hinfo->start + from, len, retlen, buf);
}

static int write_normal (struct mtd_hidden_info *mtd_hinfo, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	MTDH_INFO("Normal write hidden part\n");
	return (*mtd_hinfo->master)->write(*mtd_hinfo->master, mtd_hinfo->start + to, len, retlen, buf);
}

static int write_panic (struct mtd_hidden_info *mtd_hinfo, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	int i;
	void *area_map;
	unsigned short *buffer;
	volatile unsigned short *ptr;
	
	MTDH_INFO("Panic write hidden part\n");
	if ((uint32_t)(to + len) > mtd_hinfo->size)
		return -EFAULT;
	
	area_map = __ioremap(mtd_hinfo->start, mtd_hinfo->size, 0);
	if(!area_map)
		return -ENOMEM;

	ptr = (unsigned short *)(area_map + to);
	retlen = 0;
	len = len / sizeof(unsigned short); 
	buffer = (unsigned short *)buf;
	
	/* wait state ready */
	*ptr = 0x70;
	while (!((*ptr) & 0x80));
	
	for (i = 0; i < len; i++, ptr++) {
		*ptr = 0x40;
		*ptr = *buffer++;
		retlen+=2;
		while (!((*ptr) & 0x80));
	}

	MTDH_INFO("Panic write finished\n");
        __iounmap(area_map);
        return retlen;
}

#ifdef CONFIG_ARCH_EZXBASE
extern struct mtd_info *ezx_mymtd;
#else
struct mtd_info *mtd_info_t;
#endif

int setup_mtd_hidden_info(struct mtd_hidden_info *mtd_hinfo, uint32_t start, uint32_t size, int flag)
{
	if (!mtd_hinfo)
		return -EFAULT;
	
	MTDH_INFO("Setup mtd_hidden_info\n");

#ifdef CONFIG_ARCH_EZXBASE
	mtd_hinfo->master = &ezx_mymtd;
#else
	mtd_info_t = get_mtd_device(NULL, 0);
	if (!mtd)
		return -ENODEV;
	
	struct mtd_part_t *part_t = (struct mtd_part_t *)mtd_info_t;
	mtd_hinfo->master = &mtd->master;
#endif

	mtd_hinfo->start = start;
	mtd_hinfo->size = size;
	if (flag == F_NORMAL) {
		mtd_hinfo->erase = erase_normal;
		mtd_hinfo->read = read_normal;
		mtd_hinfo->write = write_normal;
	}
	else if (flag == F_PANIC) {
		mtd_hinfo->erase = erase_panic;
		mtd_hinfo->read = read_panic;
		mtd_hinfo->write = write_panic;
	}
	else {
		MTDH_WARNING("flag error\n");
		return -EFAULT;
	}
	return 0;	
}
	
