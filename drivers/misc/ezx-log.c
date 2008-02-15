/*
 * Kernel panic log interface for Linux on A760(XScale PXA262).
 *
 * Copyright (C) 2003, 2005, 2006 Motorola
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 */

/* 
 * 0.01	2003-07-01  initial release
 * 0.02	2005-10-10  modify for ezxbase platform
 * 0.03	2006-03-10  change the panic log design and use the hidden part of mtd 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/mtd/mtdhiddenpart.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "log.h"
#include "ezx-log.h"

static struct log_area flash_log = {
	name:		LOG_NAME,	//"A760",
	start:		LOG_START,	// LOGO_ADDR is 0x01fc0000
	size:		LOG_SIZE,
	write:		log_write,
	read:		log_read,
};

struct mtd_hidden_info *log_mtd;
/*
 * Here the read implementation should not impact mtd device.
 * Most part of the read function is coming from mtdchar.c
 */
static ssize_t log_read(const char *buf, size_t count, loff_t *ppos)
{
	int ret, len;
	ssize_t retlen, total_retlen;
	char *kbuf;
	
	if((uint32_t)(*ppos + count) > flash_log.size)
		return -EFAULT;

	retlen = total_retlen = 0;
	while (count) {	
		if (count > flash_log.size)
			len = flash_log.size;
		else
			len = count;
		kbuf = (char *)kmalloc(len, GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;
		
		ret = log_mtd->read(log_mtd, *ppos, len, &retlen, kbuf);
		if (!ret) {
			*ppos += retlen;
			if (copy_to_user(buf, kbuf, retlen)) {
				kfree(kbuf);
				return -EFAULT;
			}
			else {
				total_retlen += retlen;
			}
			count -=retlen;
			buf +=retlen;
		}
		else {
			kfree(kbuf);
			return ret;
		}
		kfree(kbuf);
	}
	return total_retlen;
}

#define is_leap(year) \
	        ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)
static const unsigned char days_in_mo[] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void sprint_panic_daytime(unsigned long t, char *s)
{
	int days, month, year, hour, min, sec, rem;

	days = t / 86400;
	rem = t % 86400;
	hour = rem / 3600;
	rem %= 3600;
	min = rem / 60;
	sec = rem % 60;
	
	year = 1970 + days / 365;
	days -= ((year - 1970) * 365
			+ LEAPS_THRU_END_OF (year - 1)
			- LEAPS_THRU_END_OF (1970 - 1));
	if (days < 0) {
		year -= 1;
		days += 365 + is_leap(year);
	}
	
	month = 0;
	if (days >= 31) {
		days -= 31;
		month++;
		if (days >= (28 + is_leap(year))) {
			days -= (28 + is_leap(year));
			month++;
			while (days >= days_in_mo[month]) {
				days -= days_in_mo[month];
				month++;
			}
		}
	}
	month++;
	days++;

	sprintf(s, "%4d", year);
	s += 4;
	if (month < 10)
		sprintf(s, "0%d", month);
	else
		sprintf(s, "%2d", month);
	s += 2;
	if (days < 10)
		sprintf(s, "0%d", days);
	else
		sprintf(s, "%2d", days);
	s += 2;
	if (hour < 10)
		sprintf(s, " 0%d", hour);
	else
		sprintf(s, " %2d", hour);
	s += 3;
	if (min < 10)
		sprintf(s, ":0%d", min);
	else
		sprintf(s, ":%2d", min);
	s += 3;
	if (sec < 10)
		sprintf(s, ":0%d", sec);
	else
		sprintf(s, ":%2d", sec);

	return;
}

static struct panic_log_header* init_log_header(void)
{
	struct panic_log_header *log_header;

	log_header = (struct panic_log_header*)kmalloc(sizeof(struct panic_log_header), GFP_KERNEL);
	if (!log_header)
		return ERR_PTR(-ENOMEM);

	memset(log_header, 0xff, sizeof(struct panic_log_header));

	sprintf(log_header->magic, "&&KERNEL");
	sprintf(log_header->version, "0.10");
	sprint_panic_daytime(xtime.tv_sec, log_header->panic_time);
	log_header->panic_jiffies = (uint32_t)jiffies;
	
	return log_header; 
}

static ssize_t log_write(const char *buf, size_t count)
{
	ssize_t ret, retlen;
	struct panic_log_header *log_header;
	unsigned char *buffer;

	//printk(KERN_EMERG "****** panic time: %d ******\n", OSCR);
#ifdef CONFIG_PANIC_BLUE_SCREEN
	printout_string();
#endif
	ret = log_mtd->erase(log_mtd);	
	log_header = init_log_header();

	buffer = (unsigned char *)log_header;
	ret = log_mtd->write(log_mtd, 0, LOG_HEAD_LEN, &retlen, buffer);
	
	/* program the block */
	if(count > flash_log.size - LOG_HEAD_LEN)
		count = flash_log.size - LOG_HEAD_LEN;
	ret = log_mtd->write(log_mtd, LOG_HEAD_LEN, count, &retlen, buf);
	
	return ret;
}


static int __init ezxlog_init(void)
{
	int ret;
	printk("Moto EzX panic log initialize!\n");
	log_mtd = (struct mtd_hidden_info *)kmalloc(sizeof(struct mtd_hidden_info), GFP_KERNEL);
	if (!log_mtd)
		return -ENOMEM;

	ret = setup_mtd_hidden_info(log_mtd, flash_log.start, flash_log.size, F_PANIC);
	if (ret)
		return ret;	
	
	return log_register(&flash_log);
}

static void __exit ezxlog_exit(void)
{
	log_unregister(&flash_log);
}

module_init(ezxlog_init);
module_exit(ezxlog_exit);

MODULE_AUTHOR("zxf <w19962@motorola.com>");
MODULE_DESCRIPTION("EzX Kernel panic log interface");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
