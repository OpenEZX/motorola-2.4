#ifndef __EZX_USBD_H
#define __EZX_USBD_H

/*
 *  linux/include/linux/ezx_usbd.h 
 *
 *  USBD definitions
 *
 *  Copyright (C) 2004 Motorola
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
 *  2004-Jan-16 - (Motorola) File created
 *  2004-Mar-2 - (Motorola) Added usb storage definitions
 *  2004-Mar-28 - (Motorola) Added usblan definitions
 *  2004-Apr-6 - (Motorola) Added dsplog definitions
*/

#define MOTUSBD_PROCFILE		"/proc/motusbd"

// macros for USB status
#define USB_STATUS_NONE		(-1)
#define USB_STATUS_MODEM	0
#define USB_STATUS_NET		1
#define USB_STATUS_USBLAN	USB_STATUS_NET
#define USB_STATUS_CFG11	2
#define USB_STATUS_PST		3
#define USB_STATUS_STORAGE	4
#define USB_STATUS_DSPLOG       5
#define USB_STATUS_NM           6

#define USB_STATUS_STRING_NONE		"MotNone"
#define USB_STATUS_STRING_PST		"MotPst"
#define USB_STATUS_STRING_CFG11		"MotCdc"
#define USB_STATUS_STRING_NET		"MotNet"
#define USB_STATUS_STRING_USBLAN	USB_STATUS_STRING_NET
#define USB_STATUS_STRING_MODEM		"MotACM"
#define USB_STATUS_STRING_STORAGE	"MotMas"
#define USB_STATUS_STRING_DSPLOG        "MotDSPLog"
#define USB_STATUS_STRING_NM            "MotNM"
#endif
