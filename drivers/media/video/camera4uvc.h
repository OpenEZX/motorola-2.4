/*
 *  camera4uvc.h
 *
 *  Camera Module driver.
 *
 *  Copyright (C) 2005,2006 Motorola Inc.
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

Revision History:
                 Modification  
Author              Date         Description of Changes
------------     ------------   -------------------------
Motorola         12/23/2005     Created, for EZX platform
Motorola         08/17/2006     Update comments for GPL
*/

/*================================================================================
                                 INCLUDE FILES
==================================================================================*/
#ifndef CAMERA4UVC_H
#define CAMERA4UVC_H

#include "camera.h"

#define CAMERA_ATTR_BRIGHTNESS 1
#define CAMERA_ATTR_ZOOM 2

#define VGA_W 640
#define VGA_H 480
#define QVGA_W 320
#define QVGA_H 240
#define CIF_W 352
#define CIF_H 288
#define QCIF_W 176
#define QCIF_H 144
#define sQVGA_W 160
#define sQVGA_H 120

#define CAMERA4UVC_MAX_WIDTH VGA_W
#define CAMERA4UVC_MAX_HEIGHT VGA_H
#define CAMERA4UVC_MAX_FRAME_SIZE (CAMERA4UVC_MAX_WIDTH * CAMERA4UVC_MAX_HEIGHT * 2)

#define CAMERA4UVC_DEF_WIDTH CIF_W
#define CAMERA4UVC_DEF_HEIGHT CIF_H

//#define CAMERA4UVC_DEBUG

/* camera4uvc interface */

/* This struct keeps information about a frame of YUV2 video 
 * data, first video data split to three part: y, cb cr, and every
 * part of data is organized into USB sections.
 * y_array, cb_array, cr_array is array of physic address which point
 * to y, cb, cr USB sections header respectively, and the array 
 * size is indicate by *_count.
 * Every USB section's length is equal to (header_size + payload_size)
 * except the last USB section, which's lenth is spec by *_last_size  */
typedef struct 
{
	int*y_array;
	int y_count;
	int y_last_size;

	int*cb_array;
	int cb_count;
	int cb_last_size;
	
	int*cr_array;
	int cr_count;
	int cr_last_size;

	int presentation_time;
       	int source_clock;
} camera4uvc_buffer_t;


/* Marked camera driver is acquired by UVC driver, and
 * set header_size and payload_size, do initialization 
 *
 * camera4uvc_open is a wrap of camera4uvc_acquire() and
 * camera4uvc_init(header_size, payload_size).   */
int camera4uvc_open(int header_size, int payload_size);
void camera4uvc_close( void );

int camera4uvc_startcapture( void );
int camera4uvc_stopcapture( void );

int camera4uvc_get_max( int attr_type, u16* value);
int camera4uvc_get_min( int attr_type, u16* value);
int camera4uvc_get_default( int attr_type, u16* value);
int camera4uvc_get_resolution( int attr_type, u16* value);
int camera4uvc_get_current( int attr_type, u16* value);
int camera4uvc_set_current( int attr_type, u16 value);

/* set and get the video resolution, which may be 
 * VGA(640*480), CIF(352*288), QVGA(320*240),
 * QCIF(176*144), sQVGA(160*120)   */
int camera4uvc_get_frame_size(u16* width, u16* height);
int camera4uvc_set_frame_size(u16 width, u16 height);

int camera4uvc_get_clock_frequency(int* hz);

int camera4uvc_get_current_frame( camera4uvc_buffer_t* buffer ); 
void camera4uvc_release_current_frame( void );


int camera4uvc_acquire( void );
void camera4uvc_release( void );
int camera4uvc_init( int header_size, int payload_size);


/*  internal function */

/* camera4uvc function
 * calculate the size relative buffer, and return the buffer size */
int camera4uvc_calc_size(int, int*, int*, int*);
int camera4uvc_ring_buf_init(p_camera_context_t camera_context);

#ifdef CAMERA4UVC_DEBUG

void camera4uvc_dump_info( void );
void camera4uvc_dump_descriptor(void);

#else

#define camera4uvc_dump_info() while(0){} 
#define camera4uvc_dump_descriptor() while(0){} 

#endif

#endif
