#ifndef __BRT_H__
#define __BRT_H__
/*
 *
 * Copyright 2006 - Motorola
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Motorola 2006-Feb-06 - File Creation
 *
 */

/*!
 * @file brt.h
 * @brief This is the header of internal definitions of battery rom data interface.
 *
 * @ingroup poweric_brt
 */

int brt_ioctl(unsigned int cmd, unsigned long arg);

#endif /* __BRT_H__ */
