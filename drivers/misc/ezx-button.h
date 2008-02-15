/*
 * linux/drivers/misc/ezx-button.h
 *
 * Support for the Motorola Ezx A780 Development Platform.
 * 
 * Copyright (C) 2002-2003 Motorola
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * Nov 6,2002 - (Motorola) Created
 * Jan 15,2003 - (Motorola) Add flip status ioctl definition
 * 
 */

#include <linux/ioctl.h>

/* Various defines: */
#define BUTTON_DELAY	1 	/* How many jiffies to wait */
#define BUTTON_INTERVAL 10 	/* How many jiffies to interval */
#define VERSION "0.1"		/* Driver version number */
#define BUTTON_MINOR 158	/* Major 10, Minor 158, /dev/button */

/* single button */
#define FLIP		0x80
#define ANSWER		0x81
#define HEADSET_INT	0x82
#define EMU_CHARGER_500mA	0x83
#define EMU_USB     0x84
#define EMU_CHARGER_0	0x85

#define NR_BUTTONS	256

#define BUTTON_IOCTL_BASE		0xde	
#define BUTTON_GET_FLIPSTATUS		_IOR(BUTTON_IOCTL_BASE, 1,int)
#define BUTTON_GET_HEADSETSTATUS	_IOR(BUTTON_IOCTL_BASE, 2,int)
#define BUTTON_GET_EMU_CABLE		_IOR(BUTTON_IOCTL_BASE, 3,int)
#define BUTTON_GET_EMU_CHARGER		_IOR(BUTTON_IOCTL_BASE, 4,int)
#define BUTTON_USB_CABLE_IN		_IOR(BUTTON_IOCTL_BASE, 5,int)
#define BUTTON_USB_CABLE_OUT		_IOR(BUTTON_IOCTL_BASE, 6,int)
#define BUTTON_CHARGER_CAP		_IOR(BUTTON_IOCTL_BASE, 7,int)
#define BUTTON_USB_DEV_TYPE		_IOR(BUTTON_IOCTL_BASE, 8,int)
#define NR_REPEAT	5
#define FLIP_ON		1
#define FLIP_OFF	0
#define HEADSET_IN	0
#define HEADSET_OUT	1
#define EMU_CABLE_BEFORE 1
#define EMU_CHARGE_500mA_BEFORE 1
#define EMU_CHARGE_0mA_BEFORE   2

#define KEYDOWN		0x8000
#define KEYUP		0x0000
//#define LOCKSCREEN 	0x84
#define LOCKSCREEN  FLIP
//# define BUTTON_GET_LOCKSCREENSTATUS 	_IOR(BUTTON_IOCTL_BASE, 3,int)
# define BUTTON_GET_LOCKSCREENSTATUS BUTTON_GET_FLIPSTATUS
//#define LOCKSCREEN_ON   1
#define  LOCKSCREEN_ON  FLIP_OFF
//#define LOCKSCREEN_OFF  0
#define  LOCKSCREEN_OFF FLIP_ON

#ifndef u16
#define u16    unsigned short
#endif
/* usb cable event */
#define USB_CABLE_IN		0xa0
#define USB_CABLE_OUT		0xa1
#define USB_CABLE_500mA		0xa2
#define USB_CABLE_0mA 		0xa3
#define NETMONITOR_CABLE	0xa4
#define MODEM_CABLE        	0xa5
#define APLOG_CABLE        	0xa6
#define CFG11_CABLE        	0xa7
#define DSPLOG_CABLE        	0xa8
#define USBNET_CABLE        	0xa9
#define PST_CABLE            	0xaa
#define MASSSTORAGE_CABLE        0xab
#define UNKNOWN_CABLE            0xff
/*add function*/
extern void answer_button_handler(int irq, void *dev_id, struct pt_regs *regs);
extern void headset_in_handler(int irq, void *dev_id, struct pt_regs *regs);
extern void headset_out_handler(int irq, void *dev_id, struct pt_regs *regs);
/*
extern void emu_charger_500mA_handler(void);
extern void emu_usb_in_handler(void);
extern void emu_usb_out_handler(void);
extern void emu_charger_0(void);
*/
extern void usb_cable_event_handler(unsigned short cable_event);
