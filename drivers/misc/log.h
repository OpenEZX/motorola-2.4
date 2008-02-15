/*
 *	Kernel panic log interface for Linux on A760(XScale PXA262).
 *
 *	Copyright (c) 2000 Motorola
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	0.01	2003-07-01	zxf <w19962@motorola.com>
 *	- initial release
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
