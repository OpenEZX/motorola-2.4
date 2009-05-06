/*
 *  sensor13m_hw.h
 *
 *  Camera Module driver.
 *
 *  Copyright (C) 2004-2006 Motorola Inc.
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
     Date          Description of Changes
  ------------    -------------------------
   6/30/2004       Change for auto-detect
   4/12/2006       Fix sensor name

*/

/*================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef _PXA_SENSOR13M_HW_H__
#define _PXA_SENSOR13M_HW_H__

#include "camera.h"

/***********************************************************************
 * 
 * Constants & Structures
 *
 ***********************************************************************/
 
#define MAX_SENSOR_B_WIDTH      1280
#define MAX_SENSOR_B_HEIGHT     1024
#define MAX_SENSOR_A_WIDTH      MAX_SENSOR_B_WIDTH
#define MAX_SENSOR_A_HEIGHT     MAX_SENSOR_B_HEIGHT

#define DEFT_SENSOR_B_WIDTH     1280
#define DEFT_SENSOR_B_HEIGHT    1024
#define DEFT_SENSOR_A_WIDTH     320
#define DEFT_SENSOR_A_HEIGHT    240

typedef struct {
    u16 width;
    u16 height;
} sensor_window_size;

                                                                           
/***********************************************************************                   
 *                                                                                         
 * Function Prototype                 
 *                                    
 ***********************************************************************/

u16  sensor13m_reg_read(u16 reg_addr);
void sensor13m_reg_write(u16 reg_addr, u16 reg_value);


// Configuration Procedures
int sensor13m_get_device_id(u16 *id, u16 *rev);

int sensor13m_viewfinder_on( void );
int sensor13m_viewfinder_off( void );
int sensor13m_snapshot_trigger( void );
int sensor13m_snapshot_complete( void );

int sensor13m_set_fps(u16 fps, u16 minfps);
int sensor13m_param12(u16 minfps);

int sensor13m_sensor_size(sensor_window_size * win);
int sensor13m_capture_sensor_size(sensor_window_size * win);
int sensor13m_output_size(sensor_window_size * win);
int sensor13m_capture_size(sensor_window_size * win);
int sensor13m_output_format(u16 format);

int sensor13m_set_contrast(int contrast);
int sensor13m_set_style(V4l_PIC_STYLE style);
int sensor13m_set_light(V4l_PIC_WB light);
int sensor13m_param14(int bright);
int sensor13m_param15(int bright);
int sensor13m_param16(int rows, int columns);
int sensor13m_param17(int enable);  

int sensor13m_default_settings( void );

#endif 

