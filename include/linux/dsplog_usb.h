/*
 * Copyright © 2005,2006 Motorola
 *
 * This code is licensed under LGPL see the GNU Lesser General Public License for full LGPL notice.
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the 
 * GNU Lesser General Public License as published by the Free Software Foundation; 
 * either version 2.1 of the License, or (at your option) any later version.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU Lesser General Public License for more details.
 *   
 * You should have received a copy of the GNU Lesser General Public License along with this library; 
 * if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA"
 *
 *  History:
 *  Motorola      Apr 11, 2005     add dsplog ioctl for ezx platform
 *  Motorola	  Oct 25, 2006	   Update comments
 *
 */
#ifndef __EZX_DSPLOG_USB_H
#define __EZX_DSPLOG_USB_H

#define DL_DRV_NAME		"usbdl"
#define DSPLOG_MINOR            0x15

/* ioctl command */
#define DSPLOG_ENABLED          0
#define DSPLOG_DISABLED         1

#endif		// __EZX_DSPLOG_USB_H
