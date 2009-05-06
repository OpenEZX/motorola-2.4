/* 
 * include/linux/mtd/mtdhiddenpart.h
 *  
 * Copyright (c) 2006 Motorola Inc.
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
 * Operate mtd hidden area.
 *
 * 2006/03/02	create file
 */

#ifndef __MTD_HIDDENPART_LINUX_H__
#define __MTD_HIDDENPART_LINUX_H__
#include <linux/version.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>

/*
 * Export the mtd hidden part struct 
 */
struct mtd_part_t {
	struct mtd_info mtd;
	struct mtd_info *master;
};

struct mtd_hidden_info {
	struct mtd_info **master;
	uint32_t start;
	uint32_t size;

	int (*erase) (struct mtd_hidden_info *mtd_hinfo);
	int (*read) (struct mtd_hidden_info *mtd_hinfo, loff_t from, size_t len, size_t *retlen, u_char *buf);
	int (*write) (struct mtd_hidden_info *mtd_hinfo, loff_t to, size_t len, size_t *retlen, const u_char *buf);
};

#define F_NORMAL 	0
#define F_PANIC		1

extern int setup_mtd_hidden_info(struct mtd_hidden_info *mtd_hinfo, uint32_t start, uint32_t size, int flag);
#endif /* __MTD_HIDDENPART_LINUX_H__*/
