/*
 *  adcm3800.h
 *
 *  Agilent ADCM 3800 Camera Module driver.
 *
 *  Copyright (C) 2004  Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
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
 *  Revision History:
 *                             Modification     Tracking
 *       Date          Description of Changes
 *  ----------------  -------------------------
 *   06/14/2004      Created, modified from adcm2700.h
 *   09/29/2004      Update
 *
*/

/*
 * ==================================================================================
 *                                  INCLUDE FILES
 * ==================================================================================
 */

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

