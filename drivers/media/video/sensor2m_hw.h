/*
 *  sensor2m_hw.h
 *
 *  Camera Module driver.
 *
 *  Copyright (C) 2005,2006 Motorola Inc.
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
      Date        Description of Changes
   ------------  -------------------------
    4/30/2005     created, for EZX platform
    4/12/2006     Fix sensor name

*/

#ifndef _PXA_SENSOR2M_HW_H__
#define _PXA_SENSOR2M_HW_H__

#include "camera.h"

/***********************************************************************
 * 
 * Constants & Structures
 *
 ***********************************************************************/
 
#define MAX_SENSOR_B_WIDTH      1600
#define MAX_SENSOR_B_HEIGHT     1200
#define MAX_SENSOR_A_WIDTH      (MAX_SENSOR_B_WIDTH/2) 
#define MAX_SENSOR_A_HEIGHT     (MAX_SENSOR_B_HEIGHT/2)

#define DEFT_SENSOR_B_WIDTH     1600
#define DEFT_SENSOR_B_HEIGHT    1200
#define DEFT_SENSOR_A_WIDTH     320
#define DEFT_SENSOR_A_HEIGHT    240

/* SENSOR_WINDOWSIZE */
typedef struct {
    u16 width;
    u16 height;
} sensor_window_size;
                                                                            
/***********************************************************************                   
 *                                                                                         
 * Function Prototype                 
 *                                    
 ***********************************************************************/

u16  sensor2m_reg_read(u16 reg_addr);
void sensor2m_reg_write(u16 reg_addr, u16 reg_value);


// Configuration Procedures
int sensor2m_viewfinder_on( void );
int sensor2m_snapshot_trigger( void );

int sensor2m_set_time(u16 time);
int sensor2m_set_exposure_mode(int mode, u32 metime);

int sensor2m_sensor_size(sensor_window_size * win);
int sensor2m_output_size(sensor_window_size * win);
int sensor2m_capture_sensor_size(sensor_window_size * win);
int sensor2m_capture_size(sensor_window_size * win);
int sensor2m_capture_format(u16 format);
int sensor2m_set_camera_size(sensor_window_size * win);
int sensor2m_get_camera_size(sensor_window_size * win);

int sensor2m_set_style(V4l_PIC_STYLE style);
int sensor2m_set_light(V4l_PIC_WB light);
int sensor2m_set_bright(int bright);
int sensor2m_set_flicker(int bright);
int sensor2m_set_mirror(int rows, int columns);
int sensor2m_set_sfact(int scale);

int sensor2m_get_cstatus(int *length, int *status);

int sensor2m_default_settings( void );
int sensor2m_enter_9(void);
int sensor2m_exit_9(void);
int sensor2m_reset_init(void);

#endif

