/*
 *  sensor13m.h
 *
 *  Camera Module Driver
 *
 *  Copyright (C) 2004, 2006 Motorola Inc.
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

Revision History:
   Modification   
      Date         Description of Changes
   ------------   -------------------------
    6/30/2004      Change for auto-detect
    4/12/2006      Fix sensor name

*/

/*================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef _SENSOR13M_H_
#define _SENSOR13M_H_

#include "camera.h"

//////////////////////////////////////////////////////////////////////////////////////
//
//          Prototypes
//
//////////////////////////////////////////////////////////////////////////////////////

int camera_func_sensor13m_init(p_camera_context_t);
int camera_func_sensor13m_deinit(p_camera_context_t);
int camera_func_sensor13m_set_capture_format(p_camera_context_t);
int camera_func_sensor13m_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_sensor13m_stop_capture(p_camera_context_t);
int camera_func_sensor13m_pm_management(p_camera_context_t cam_ctx, int suspend);
int camera_func_sensor13m_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param);
int camera_func_sensor13m_standby(void);

extern int sensor13m_param7(void);
extern int sensor13m_param8(void);
extern int sensor13m_param6(int *lin, int *lua);

#endif 

