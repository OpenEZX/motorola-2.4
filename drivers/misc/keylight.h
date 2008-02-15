/*
 * linux/drivers/misc/keylight.h
 *
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
 * Mar 2,2004 - (Motorola) Created
 * 
 */
 
#ifndef KEYLIGHT_H
#define KEYLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif
	
#define KEYLIGHT_MINOR 		168
#define KEYLIGHT_MAIN_ON    	0xf0
#define KEYLIGHT_AUX_ON		0xf1
#define KEYLIGHT_MAIN_OFF	0xf2
#define KEYLIGHT_AUX_OFF	0xf3
#define KEYLIGHT_MAIN_SETTING  	0xf4
#define KEYLIGHT_AUX_SETTING 	0xf5	

#ifdef __cplusplus
}
#endif
#endif
  
