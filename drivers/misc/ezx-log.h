/*
 * Kernel panic log interface for EZX Platform.
 *
 * Copyright (C) 2005, 2006 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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
 * Hisotry:
 *      2005-10-10 initialize for ezxbase platform 
 * 	2005-12-21 support Macau product
 *      2006-03-10 redesign the panic log interface
 *	2006-06-06 change memory layout for macau (Add 1M space to resource area, and reduce 1M from language space)
 *	2006-06-15 support Penglai product
 *	2006-06-29 support APR phase II
 *	2006-08-09 Modify memory map to support multi-product
 *	2006-08-18 add a new log type for test purpose
 */
#ifndef _EZX_PLOG_H_
#define _EZX_PLOG_H_
#include <linux/config.h>

#ifdef CONFIG_ARCH_EZXBASE
#define LOG_NAME	"EZX"
#define LOG_START	MMAP_PANICLOG_START
#define LOG_SIZE	MMAP_PANICLOG_SIZE
#endif

#ifdef CONFIG_ARCH_EZXBASE
#define PANIC_LOG_DEV
#else
#define PANIC_LOG_DEV 3
#endif

typedef enum {
	APR_AP_KERNELPANIC_TYPE_DEFAULT_EV = 0x00,      // default panic 
	APR_AP_KERNELPANIC_TYPE_HANG_EV,          	// 3 keypad trigger panic
	APR_AP_KERNELPANIC_TYPE_TEST_EV          	// test purpose, e.g. 808A5 log record
	/* add new panic type here */        
} APR_AP_KERNELPANIC_TYPE_E;

struct panic_log_header{
	char magic[8];
	char version[4];
	char panic_time[18];
	char panic_type[4];
	uint32_t panic_jiffies;
	char reserved[218];
} __attribute__((packed));
#define LOG_HEAD_LEN 	(sizeof(struct panic_log_header))

void log_set_panic_type(unsigned char type);
/* the file will not only be included by ezx-log.c, so change
   the two static functions declaration to ezx-log.c to avoid 
   compilation warning and klock check erro*/ 
/* static ssize_t log_read(const char *buf, size_t count, loff_t *ppos); */
/* static ssize_t log_write(const char *buf, size_t count); */ 
extern void printout_string(void);

#endif //_EZX_PLOG_H_
