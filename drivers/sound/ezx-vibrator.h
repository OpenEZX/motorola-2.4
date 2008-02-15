/*
 * linux/drivers/sound/ezx-vibrator.h
 *
 * Vibrator interface for EZX. for application can't direct call interface in 
 * kernel space from user space.so vibrator still realize a char device.
 *
 * Copyright (C) 2002-2005 Motorola
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 *  Jun 20,2002 - (Motorola) created
 *  Jan 02,2004 - (Motorola) (1) Port from UDC e680 kernel of jem vob.
 *                           (2) reorganize file header
 *  Apr.13,2004 - (Motorola) reorganise file header
 *
 */

#ifndef EZX_VIBRATOR_H
#define EZX_VIBRATOR_H

#include <linux/ioctl.h>


#define VIBRATOR_MAJOR	108
#define VIBRATOR_IOCTL_BASE	0xbb
#define VIBRATOR_ENABLE		_IOW (VIBRATOR_IOCTL_BASE,1,int)
#define VIBRATOR_DISABLE	_IO (VIBRATOR_IOCTL_BASE,2)


#endif


