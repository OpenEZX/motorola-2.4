/*
 * Kernel panic log interface for Linux on A760(XScale PXA262).
 *
 * Copyright (C) 2003 Motorola Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "log.h"

static ssize_t log_read(const char *buf, size_t count, loff_t *ppos);
static ssize_t log_write(const char *buf, size_t count);

static struct log_area flash_log = {
	name:		"A760",
	start:		0x01fc0000,
	size:		0x00020000,
	write:		log_write,
	read:		log_read,
};

/*
 * Here the read implementation should not impact mtd device.
 * Most part of the read function is coming from mtdchar.c
 */
#define MAX_KMALLOC_SIZE 0x20000
static ssize_t log_read(const char *buf, size_t count, loff_t *ppos)
{
	struct mtd_info *mtd;
	size_t retlen=0;
	size_t total_retlen=0;
	int ret=0;
	int len;
	char *kbuf;

	mtd = get_mtd_device(NULL, 3);
	if (!mtd)
		return -ENODEV;

	while (count) {
		if (count > MAX_KMALLOC_SIZE) 
			len = MAX_KMALLOC_SIZE;
		else
			len = count;

		kbuf=kmalloc(len,GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;
		
		ret = MTD_READ(mtd, *ppos, len, &retlen, kbuf);
		if (!ret) {
			*ppos += retlen;
			if (copy_to_user(buf, kbuf, retlen)) {
			        kfree(kbuf);
				return -EFAULT;
			}
			else
				total_retlen += retlen;

			count -= retlen;
			buf += retlen;
		}
		else {
			kfree(kbuf);
			return ret;
		}
		
		kfree(kbuf);
	}
	
	return total_retlen;
}

static ssize_t log_write(const char *buf, size_t count)
{
	int i;
	ssize_t ret;
	void *area;
	unsigned short *buffer;
	volatile unsigned short *ptr;

	area = __ioremap(flash_log.start, flash_log.size, 0);
	if (!area)
		return -ENOMEM;

	ptr = (unsigned short *)area;
	ret = count;

	/* wait state ready */
	*ptr = 0x70;
	while (!((*ptr) & 0x80));

	/* unlock the block */
	*ptr = 0x60; *ptr = 0xd0;
	while (!((*ptr) & 0x80));

	/* erase the block */
	*ptr = 0x20; *ptr = 0xd0;
	while (!((*ptr) & 0x80));

	/* program the block */
	count = count / sizeof(unsigned short); /* it doesn't matter to lose a little data */
	buffer = (unsigned short *)buf;
	for (i = 0; i < count; i++, ptr++) {
		*ptr = 0x40;
		*ptr = *buffer++;
		while (!((*ptr) & 0x80));
	}

	__iounmap(area);
	return ret;
}


static int __init ezxlog_init(void)
{
	return log_register(&flash_log);
}

static void __exit ezxlog_exit(void)
{
	log_unregister(&flash_log);
}

module_init(ezxlog_init);
module_exit(ezxlog_exit);

MODULE_AUTHOR("zxf <w19962@motorola.com>");
MODULE_DESCRIPTION("A760 Kernel panic log interface");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
