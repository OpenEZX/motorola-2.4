/*
 *  sensor2m.h
 *
 *  Camera Module driver.
 *
 *  Copyright (C) 2005,2006 Motorola Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
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
   4/30/2005      created, for EZX platform
   4/12/2006      Fix sensor name

*/

/*================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef _SENSOR2M_H_
#define _SENSOR2M_H_

#include "camera.h"

//////////////////////////////////////////////////////////////////////////////////////
//
//          Prototypes
//
//////////////////////////////////////////////////////////////////////////////////////

int camera_func_sensor2m_init(p_camera_context_t);
int camera_func_sensor2m_deinit(p_camera_context_t);
int camera_func_sensor2m_set_capture_format(p_camera_context_t);
int camera_func_sensor2m_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_sensor2m_stop_capture(p_camera_context_t);
int camera_func_sensor2m_pm_management(p_camera_context_t cam_ctx, int suspend);
int camera_func_sensor2m_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param);

#endif

