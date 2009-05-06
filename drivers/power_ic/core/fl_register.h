/*
 * Copyright 2004 Freescale Semiconductor, Inc.
 * Copyright 2004, 2006 Motorola, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 *  Author     Date                Comment
 * ======== ===========   =====================================================
 * Motorola 2006-Oct-25 - Update comments.
 * Motorola 2004-Dec-06 - DDesign the Low Level interface to Funlight IC.
 */


#ifndef __FL_REGISTER_H__
#define __FL_REGISTER_H__  

/*!
 * @file fl_register.h
 * @brief This file contains prototypes of Funlight driver available in kernel space.
 *
 * @ingroup poweric_core
 */


void fl_initialize(void (*reg_init_fcn)(void));
int fl_reg_read (int reg, unsigned int *reg_value);
int fl_reg_write (int reg, unsigned int reg_value);

#endif  /* __FL_REGISTER_H__ */
