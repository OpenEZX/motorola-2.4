/*
 * linux/drivers/misc/log.c
 * 
 * Copyright (C) 2003 Motorola
 * 
 * Kernel panic log interface for Linux.
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
 * 
 * July, 7,2003 - (Motorola) Created
 * 
 */

#ifndef _PANIC_LOG_H_
#define _PANIC_LOG_H_

#include <linux/fs.h>

struct log_area {
	char name[8];
	unsigned long start;
	unsigned long size;
	ssize_t (*write) (const char *buf, size_t count);
	ssize_t (*read) (const char *buf, size_t count, loff_t *ppos);
};

struct log_cmdline {
	char name[8];
};

extern int log_register(struct log_area *log_f);
extern int log_unregister(struct log_area *log_f);

#endif /* _PANIC_LOG_H_ */
