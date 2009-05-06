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
 * Motorola 2006-Oct-09 - Miscellaneous cleanup.
 * Motorola 2004-Dec-06 - Add register access function prototypes for
 *                        EMU One Chip IC.
 * Motorola 2006-Oct-25 - Update comments.
 */

#ifndef __EOC_REGISTER_H__
#define __EOC_REGISTER_H__

/*!
 * @file eoc_register.h
 *
 * @brief This file contains prototypes of EMU One Chip register access functions. 
 *
 * @ingroup poweric_core
 */ 

void eoc_initialize (void (*reg_init_fcn)(void));
int eoc_reg_read (int reg, unsigned int *reg_value);
int eoc_reg_write (int reg, unsigned int reg_value);

#endif /* __EOC_REGISTER_H__ */
