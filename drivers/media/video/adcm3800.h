/*
 *  linux/drivers/media/video/adcm3800.h
 *
 *  ADCM 3800 Camera Module driver header.
 *
 *  Copyright (C) 2003  Motorola
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Dec 15,2003 - (Motorola) Created new file for new part based on ADCM2650.h file
 */  

/*==================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef _ADCM3800_H_
#define _ADCM3800_H_

#include "camera.h"

//////////////////////////////////////////////////////////////////////////////////////
//
//          Prototypes
//
//////////////////////////////////////////////////////////////////////////////////////

/* WINDOW SIZE */
typedef struct {
    u16 width;
    u16 height;
} window_size;

#endif /* _ADCM3800_H_ */

