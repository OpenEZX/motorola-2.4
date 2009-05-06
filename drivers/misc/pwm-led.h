/* 
 * Copyright ©  2004,2006 Motorola
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
 * Motorola	2004-Mar	create file
 * Motorola	2006-Oct	Update comments
 */

#ifndef PWMLED_H
#define PWMLED_H


#ifdef __cplusplus
extern "C" {
#endif
	
#define PWMLED_MINOR 		168
#define PWMLED_0_ON    		0xf0
#define PWMLED_1_ON		0xf1
#define PWMLED_0_OFF		0xf2
#define PWMLED_1_OFF		0xf3
#define PWMLED_0_SETTING  	0xf4
#define PWMLED_1_SETTING 	0xf5	

#ifdef __cplusplus
}
#endif
#endif
  
