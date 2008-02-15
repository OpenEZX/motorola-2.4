#ifndef __LINUX_KEYPAD_H
#define __LINUX_KEYPAD_H

/*
 * include/linux/keypad.h
 * 
 * Copyright (C) 2003 Motorola
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
 * 2003-Dec-24 - (Motorola) File created
 */

#include <linux/ioctl.h>

#define KEYDOWN		0x8000
#define KEYUP		0x0000

#define EVENTSIZE       sizeof(short)
#define KEY_IS_DOWN(x)    ((x) & KEYDOWN)
#define KEYCODE(x)        ((x) & 0x7fff)

struct autorepeatinfo {
   int r_repeat;                /* 1 to do autorepeat, 0 to not do it */
   int r_time_to_first_repeat;  /* time to first repeat in milliseconds */
   int r_time_between_repeats;  /* time between repeats in milliseconds */
};

#define KEYPAD_IOCTL_BASE    'k'

/* 
 * return the bitmap of keys currently down.
 * The bitmap is an array of NUM_WORDS_IN_BITMAP unsigned long words.
 * We count the bits starting with 1 in the rightmost position in
 * the rightmost (highest-indexed) word.  If NUM_WORDS_IN_BITMAP is two, 
 * the bitmap looks like this:
 *              0                          1
 *  ----------------------------------------------------
 * | 64 63 ........... 34 33 | 32 31 ............. 2 1 |
 *  ----------------------------------------------------
 * Bit n corresponds to key code n.
 */

#define KEYPAD_IOC_GETBITMAP      _IOR(KEYPAD_IOCTL_BASE,1,unsigned long *)
#define NUM_WORDS_IN_BITMAP 2

/**
 * put the specified event on the keypad's input queue and update the bitmap.
 * The exact event as specified will be returned by read() when it
 * reaches the front of the queue.  A key press event consists of
 * the key code or'd with KEYDOWN.  A key release event consists of
 * just the key code.
 */
#define KEYPAD_IOC_INSERT_EVENT   _IOW(KEYPAD_IOCTL_BASE,2,unsigned short)

/**
 * have the driver interpret the specified scan register values as
 * if it had gotten an interrupt.  The passed-in argument is a pointer
 * to three consecutive 32-bit words in this order: KPAS, KPASMKP0, KPASMKP1.
 */
#define KEYPAD_IOC_INSERTKEYSCAN  _IOW(KEYPAD_IOCTL_BASE,3,unsigned long *)

/* get the hardware debounce interval (in milliseconds) */
#define KEYPAD_IOC_GET_DEBOUNCE_INTERVAL \
                                  _IOR(KEYPAD_IOCTL_BASE,4,unsigned short *)

/* set the hardware debounce interval (in milliseconds) */
#define KEYPAD_IOC_SET_DEBOUNCE_INTERVAL  \
                                  _IOW(KEYPAD_IOCTL_BASE,4,unsigned short)

/* get "Ignore Multiple Key Press" bit; 1 means they are ignored */
#define KEYPAD_IOC_GET_IMKP_SETTING _IOR(KEYPAD_IOCTL_BASE,5,unsigned char *)

/* set "Ignore Multiple Key Press" bit; 1 means they will be ignored */
#define KEYPAD_IOC_SET_IMKP_SETTING _IOW(KEYPAD_IOCTL_BASE,5,unsigned char)

/* 
 * specify what to do about autorepeat on held keys
 * the 3rd argument is a pointer to a struct autorepeat
 */
#define KEYPAD_IOC_SET_AUTOREPEAT _IOW(KEYPAD_IOCTL_BASE,6,unsigned long *)

#define KEYPADI_TURN_ON_LED		1
#define KEYPADI_TURN_OFF_LED		0
/*
 * the constant KEY_k is both the code for key k in an event returned by
 * read() and the bit position for key k in the bitmap returned by
 * ioctl(KEYPAD_IOC_GETBITMAP).
 */
#define KEYPAD_NONE        0

#define KEYPAD_0           1
#define KEYPAD_1           2
#define KEYPAD_2           3
#define KEYPAD_3           4
#define KEYPAD_4           5
#define KEYPAD_5           6
#define KEYPAD_6           7
#define KEYPAD_7           8
#define KEYPAD_8           9
#define KEYPAD_9           10

#define KEYPAD_UP          11
#define KEYPAD_DOWN        12
#define KEYPAD_LEFT        13
#define KEYPAD_RIGHT       14
#define KEYPAD_POUND       15
#define KEYPAD_STAR        16
#define KEYPAD_MENU        17
#define KEYPAD_SLEFT       18
#define KEYPAD_SRIGHT      19
#define KEYPAD_VOLUP       20
#define KEYPAD_VOLDOWN     21
#define KEYPAD_CAMERA      22
#define KEYPAD_CLEAR       23
#define KEYPAD_CARRIER     24
#define KEYPAD_ACTIVATE    25
#define KEYPAD_SEND        26
#define KEYPAD_SMART       27
#define KEYPAD_VAVR        28

#define KEYPAD_CENTER	    29
#define KEYPAD_HOME	    30
#define KEYPAD_A            31
#define KEYPAD_B            32
#define KEYPAD_GAME_R       33
#define KEYPAD_GAME_L       34
#define KEYPAD_CAMERA_VOICE 35
//#define KEYPAD_SCREEN_LOCK  36
#define KEYPAD_POWER        37

#define KEYPAD_OK           38
#define KEYPAD_CANCEL       39
#define KEYPAD_PTT          40
#define KEYPAD_JOG_UP       41
#define KEYPAD_JOG_MIDDLE   42
#define KEYPAD_JOG_DOWN     43

#define KEYPAD_MAXCODE      43

#endif /* __LINUX_KEYPAD_H */
