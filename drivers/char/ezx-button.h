/*
 * linux/drivers/char/ezx-button.h
 * 
 * Copyright (C) 2002 Motorola Inc.
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
 */

#include <linux/ioctl.h>
/* CE BUS device definations */

#define CEBUS_OPTION1  (1<<6)     /*GPIO6*/
#define CEBUS_OPTION2  (1<<7)     /*GPIO7*/
#define CEBUS_ASEL1    (1<<8)     /*GPIO8*/
#define CEBUS_ASEL0    (1<<5)     /*GPIO37*/
#define CEBUS_ASEL2    (1<<6)     /*GPIO38*/

#define CEBUS_DEVICE_MAX                     14

#define CEBUS_BUTTON                         0x0100
#define CEBUS_NONE_DEVICE                    0
#define CEBUS_PC_POWERED_USB                 1
#define CEBUS_DUMB_TTY                       2
#define CEBUS_DUMB_DESKTOP_SPEAKERPHONE      3
#define CEBUS_DUMB_PTT_HEADSET               4
#define CEBUS_DUMB_FM_RADIO_HEADSET          5
#define CEBUS_DUMB_IRDA_ADAPTER              6
#define CEBUS_DUMB_CLIP_ON_SPEAKERPHONE      7
#define CEBUS_DUMB_SMART_AUDIO_DEVICE        8
#define CEBUS_DUMB_EIHF                      9
#define CEBUS_PHONE_POWERED_USB              10
#define CEBUS_RS232_8_WIRES                  11
#define CEBUS_RS232_4_WIRES                  12
#define CEBUS_PUSHTOTALK_PTT                 13

/* Various defines: */
#define BUTTON_DELAY	1 	/* How many jiffies to wait */
#define BUTTON_INTERVAL 10 	/* How many jiffies to interval */
#define VERSION "0.1"		/* Driver version number */
#define BUTTON_MINOR 158	/* Major 10, Minor 158, /dev/button */

#define KEYDOWN		0x8000
#define KEYUP		0x0000

#define MATRIX_NUMBER	6
/* scan to decide */
#define UP		0x1
#define DOWN		0x2
#define JOY_M		0x3
#define HOME		0x4
#define JOY_L		0x5
#define JOY_R		0x6
/* single button */
#define VOICE_REC	0x10
#define ANSWER		0x20
#define FLIP		0x30
#define HEADSET_INT	0x40

#define NR_BUTTONS	256

#define BUTTON_IOCTL_BASE		0xde	
#define BUTTON_GET_FLIPSTATUS		_IOR(BUTTON_IOCTL_BASE, 1,int)
#define BUTTON_GET_HEADSETSTATUS	_IOR(BUTTON_IOCTL_BASE, 2,int)
#define NR_REPEAT	5
#define BUTTON_GET_CEBUSSTATUS	    _IOR(BUTTON_IOCTL_BASE, 3,int)
#define FLIP_ON		1
#define FLIP_OFF	0
#define HEADSET_IN	0
#define HEADSET_OUT	1

#ifndef u16
#define u16    unsigned short
#endif

/*add by linwq for cebus*/
extern u16 get_current_cebus_status(void);
extern void cebus_int_hndler(int irq, void *dev_id, struct pt_regs *regs);

extern void answer_button_handler(int irq, void *dev_id, struct pt_regs *regs);
extern void headset_in_handler(int irq, void *dev_id, struct pt_regs *regs);
extern void headset_out_handler(int irq, void *dev_id, struct pt_regs *regs);
