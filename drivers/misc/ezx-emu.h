#if EMU_PIHF_FEATURE
/*
 * linux/drivers/misc/ezx-emu.h
 *
 * EMU driver on ezx.
 * Support for the Motorola Ezx A780 Development Platform.
 * 
 * Copyright (C) 2004 Motorola
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
 * Apr 30,2004 - (Motorola) Created
 * 
 */
#include <linux/ioctl.h>

/* Various defines: */
#define VERSION   "0.1"        /* Driver version number */
#define EMU_MINOR 250          /* Major 10, Minor 250, /dev/emu */

/* single button */

#define EMU_IOCTL_BASE             0xdf    

/* Switch the Mux1/2 and set the GPIOs */
#define EMU_SW_TO_UART             _IO(EMU_IOCTL_BASE, 1)
#define EMU_SW_TO_USB              _IO(EMU_IOCTL_BASE, 2)
#define EMU_SW_TO_AUDIO_MONO       _IO(EMU_IOCTL_BASE, 3)
#define EMU_SW_TO_AUDIO_STEREO     _IO(EMU_IOCTL_BASE, 4)
#define EMU_SMART_SPD_INIT               _IO(EMU_IOCTL_BASE, 5)

/* EMU PIHF */
#define EMU_PIHF_INIT              _IOW(EMU_IOCTL_BASE, 21,int)
/*#define EMU_PIHF_GET_STATUS        _IOR(EMU_IOCTL_BASE, 12,int)*/
#define EMU_PIHF_SNP_INT_CTL       _IOR(EMU_IOCTL_BASE, 23,int)
#define EMU_PIHF_ID_LOW            _IO(EMU_IOCTL_BASE, 24)
#define EMU_PIHF_ID_HIGH           _IO(EMU_IOCTL_BASE, 25)

/* EMU EIHF */
#define EMU_EIHF_INIT       _IOW(EMU_IOCTL_BASE, 31,int)
#define EMU_EIHF_MUTE       _IO(EMU_IOCTL_BASE, 32)
#define EMU_EIHF_UNMUTE     _IO(EMU_IOCTL_BASE, 33)

typedef enum{
    EMU_SWITCH_TO_UART = 0,
    EMU_SWITCH_TO_USB,
    EMU_SWITCH_TO_AUDIO_MONO,
    EMU_SWITCH_TO_AUDIO_STEREO,
    EMU_SWITCH_TO_NO_DEV
} T_EMU_SWITCH_TO;

void emu_switch_to(T_EMU_SWITCH_TO to);

#else /* EMU_PIHF_FEATURE */

/*
 * linux/drivers/misc/ezx-emu.c
 *
 * EMU driver on ezx.
 * Support for the Motorola Ezx A780 Development Platform.
 * 
 * Copyright (C) 2004 Motorola
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
 * Apr 30,2004 - (Motorola) Created
 * 
 */

#include <linux/ioctl.h>

/* Various defines: */
#define VERSION   "0.1"        /* Driver version number */
#define EMU_MINOR 250          /* Major 10, Minor 250, /dev/emu */

/* single button */

#define EMU_IOCTL_BASE             0xdf    

/* Switch the Mux1/2 and set the GPIOs */
#define EMU_SW_TO_UART             _IO(EMU_IOCTL_BASE, 1)
#define EMU_SW_TO_USB              _IO(EMU_IOCTL_BASE, 2)
#define EMU_SW_TO_AUDIO_MONO       _IO(EMU_IOCTL_BASE, 3)
#define EMU_SW_TO_AUDIO_STEREO     _IO(EMU_IOCTL_BASE, 4)

/* EMU PIHF */
#define EMU_PIHF_INIT              _IOW(EMU_IOCTL_BASE, 21,int)
/*#define EMU_PIHF_GET_STATUS        _IOR(EMU_IOCTL_BASE, 12,int)*/
#define EMU_PIHF_SNP_INT_CTL       _IOR(EMU_IOCTL_BASE, 23,int)
#define EMU_PIHF_ID_LOW            _IO(EMU_IOCTL_BASE, 24)
#define EMU_PIHF_ID_HIGH           _IO(EMU_IOCTL_BASE, 25)
#define EMU_PIHF_INIT_DPLUS_CTL    _IO(EMU_IOCTL_BASE, 26)

/* EMU EIHF */
#define EMU_EIHF_INIT       _IOW(EMU_IOCTL_BASE, 31,int)
#define EMU_EIHF_MUTE       _IO(EMU_IOCTL_BASE, 32)
#define EMU_EIHF_UNMUTE     _IO(EMU_IOCTL_BASE, 33)

typedef enum{
    EMU_SWITCH_TO_UART = 0,
    EMU_SWITCH_TO_USB,
    EMU_SWITCH_TO_AUDIO_MONO,
    EMU_SWITCH_TO_AUDIO_STEREO,
    EMU_SWITCH_TO_NO_DEV
} T_EMU_SWITCH_TO;

void emu_switch_to(T_EMU_SWITCH_TO to);
#endif /* EMU_PIHF_FEATURE */
