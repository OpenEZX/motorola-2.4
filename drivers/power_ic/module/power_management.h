/* 
 * Copyright 2005-2006 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
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
 * Motorola 2005-Jun-09 - Design of interfaces to control power-related functions.
 * Motorola 2006-Oct-25 - Update comments
 */

#ifndef __POWER_MANAGEMENT_H__
#define __POWER_MANAGEMENT_H__

/*!
 * @file power_management.h
 *
 * @ingroup poweric_power_management
 *
 * @brief This is the header of internal definitions the power IC power management interface.
 *
 */

#include <linux/power_ic_kernel.h>

int pmm_ioctl(unsigned int cmd, unsigned int arg);


#endif /* __POWER_MANAGEMENT_H__ */
