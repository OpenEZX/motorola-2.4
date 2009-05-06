/*
 * Copyright©  2002, 2004, 2006 Motorola
 * 
 * This code is licensed under LGPL 
 * see the GNU Lesser General Public License for full LGPL notice.
 *
 * This library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published 
 * by the Free Software Foundation; either version 2.1 of the License, 
 * or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this library; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 * History:
 * Author	Date		Description of Changes
 * ---------	-----------	--------------------------
 * Motorola	Jun 20,2002	created
 * Motorola	Jan 02,2004	(1) Port from UDC e680 kernel of jem vob.
 * 				(2) reorganize file header
 * Motorola	Apr.13,2004	reorganise file header
 * Motorola	Oct.25,2006	Update comments
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


