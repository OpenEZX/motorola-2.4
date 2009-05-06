/*
 *  camera4uvc.c
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
Author            Date            Description of Changes
-----------     ------------     -------------------------
Motorola         12/15/2005       Created
Motorola         01/24/2006       Resolve sometime camera FIFO overrun make for UVC driver can't release camera driver when
                                  USB cable is plug off
Motorola         04/28/2006       resolve function complexity and divid function camera4uvc_startcapture()
 
*/

/*
==================================================================================
                                 INCLUDE FILES
==================================================================================*/
#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <linux/wrapper.h>
#include <linux/videodev.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/wait.h>

#include <linux/types.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>

#include "camera.h"
#include "camera4uvc.h"


#ifdef CAMERA4UVC_DEBUG
                                                                                                                              
#define dbg_print(fmt, args...) printk(KERN_INFO "fun %s "fmt"\n", __FUNCTION__, ##args)
#define err_print(fmt, args...) printk(KERN_ERR "fun %s "fmt"\n", __FUNCTION__, ##args)
                                                                                                                              
#else
                                                                                                                              
#define dbg_print(fmt, args...)  ;
#define err_print(fmt, args...) printk(KERN_ERR fmt"\n", ##args)
                                                                                                                              
#endif

/* output format 
 * currently camera4uvc only support output format PACKED and PLANNER 
 * */
#define OUTPUT_FORMAT CAMERA_IMAGE_FORMAT_YCBCR422_PACKED
//#define OUTPUT_FORMAT CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR

#define CIBR0_PHY	(0x50000000 + 0x28)
#define CIBR1_PHY	(0x50000000 + 0x30)
#define CIBR2_PHY	(0x50000000 + 0x38)

#define DMA_ALIGNMENT 8

/*
 * It is required to have at least 3 frames in buffer
 * in current implementation.
 */
#define FRAMES_IN_BUFFER    	3

#define CAMERA_BRIGHT_MAX 9
#define CAMERA_BRIGHT_MIN 1
#define CAMERA_BRIGHT_DEF 5
#define CAMERA_BRIGHT_STEP 1
#define CAMERA_ZOOM_MAX 8
#define CAMERA_ZOOM_MIN 1
#define CAMERA_ZOOM_DEF 1
#define CAMERA_ZOOM_STEP 1
#define CAMERA_FPS_DEF 20

#define MAX_SENSOR_A_WIDTH      800
#define MAX_SENSOR_A_HEIGHT     600


extern camera_context_t  *g_camera_context;
extern spinlock_t camera_state_lock;
extern camera_state_t camera_state;
extern wait_queue_head_t  camera_wait_q;

extern int pxa_camera_open_device( void );
extern void pxa_camera_close_device( void );

/* camera4uvc Interface 
 * camera4uvc interface aimed to provide a interface for 
 * UVC (USB video class) function driver to support webcam feature, 
 * which get the video data from camera sensor, and deliver the
 * video stream to PC via USB.
 * To reduce cpu and memory consume, we shall organize video data
 * to USB frame, which include a header and payload, the header is
 * set of information which describe the USB frame, and payload is
 * video data, every frame of picture will split to many USB frame,
 * by this organization, UVC can directly transfer these USB frame 
 * and no need to copy and move, and to distinguish with picture 
 * frame, we use section to describe USB frame.
 * For camera driver, it generate a DMA descriptor chain to transfer 
 * picture frame, every descriptor item transfer on page of data.
 * While for camera4uvc, it also generate a DMA descriptor chain, but
 * every descriptor item transfer payload size of data to corresponding
 * section, one or more section embedded in one page.
 */
static int camera4uvc_header_size;
static int camera4uvc_header_align_size;
static int camera4uvc_payload_size;
static int camera4uvc_payload_align_size;
static int section_size;
#define SECTION_PER_PAGE	((int)(PAGE_SIZE/section_size))

static int* y_buffer_array;
static int* cb_buffer_array;
static int* cr_buffer_array;
static int y_buffer_array_size;
static int cb_buffer_array_size;
static int cr_buffer_array_size;

typedef enum {
	CAMERA4UVC_NA = 0,
	CAMERA4UVC_ACQUIRED,
	CAMERA4UVC_INITIALIZED,
	CAMERA4UVC_START,
	CAMERA4UVC_STOP,
} CAMERA4UVC_PROCESS_STATE;
static CAMERA4UVC_PROCESS_STATE camera4uvc_process_state = CAMERA4UVC_NA;

static int camera4uvc_set_brightness(int value);
static int camera4uvc_set_flicker_freq(int value );
static int camera4uvc_set_fps(int value);
#if 0
static int camera4uvc_set_mirror(int value);
static int camera4uvc_set_mode(int mode, int maxexpotime);
#endif
static int camera4uvc_set_sensor_register(int addr, unsigned int value);
static int camera4uvc_get_sensor_register( int addr, unsigned int* value);
static int camera4uvc_set_capture_format(unsigned int input, unsigned int output);

/* only marks camera driver in used */
int camera4uvc_acquire( void )
{
	int ret=0;
	
	spin_lock( &camera_state_lock );
	if ( camera_state == CAMERA_STATE_IDLE){
		camera_state = CAMERA_STATE_UVC;
		camera4uvc_process_state = CAMERA4UVC_ACQUIRED;
	}else if (camera_state == CAMERA_STATE_APP )
	       ret = -EBUSY;
	else
		ret = -EIO;
	spin_unlock( &camera_state_lock );

	return ret;
}
EXPORT_SYMBOL( camera4uvc_acquire );

/* release camera driver */
void camera4uvc_release( void )
{
	dbg_print("camera4uvc_release");
	spin_lock( &camera_state_lock );
	if ( camera_state == CAMERA_STATE_UVC) {
		camera_state = CAMERA_STATE_IDLE;
		camera4uvc_process_state = CAMERA4UVC_NA;
	}
	spin_unlock( &camera_state_lock );

}
EXPORT_SYMBOL( camera4uvc_release );

/* camera driver initialize, almost do the same thing as
 * pxa_camera_open */
int camera4uvc_init( int header_size, int payload_size)
{
	int ret;

	if ( camera4uvc_process_state != CAMERA4UVC_ACQUIRED ){
		err_print("camera4uvc is not acquired");
		return -1;
	}

	/* check if header is align to 32 bit, or else DMA transfer will fail */
	if ( (header_size < 0) || (payload_size <= 0) ||
		( (payload_size%8) != 0 ) ||
		(PAGE_SIZE < header_size + payload_size) ) {
		err_print( "invalid header_size (%d) or payload_size (%d, must align to 8)\n",
				header_size, payload_size);
		return -EIO;
	}

	camera4uvc_header_size = header_size;
	camera4uvc_header_align_size = 
		((int)((header_size+DMA_ALIGNMENT-1)/DMA_ALIGNMENT))*DMA_ALIGNMENT;
	camera4uvc_payload_size = payload_size;
	camera4uvc_payload_align_size = 
		((int)((payload_size+DMA_ALIGNMENT-1)/DMA_ALIGNMENT))*DMA_ALIGNMENT;
	//section_size = (camera4uvc_header_align_size 
	//		+ camera4uvc_payload_align_size);
	// To use less memory
	section_size = camera4uvc_header_align_size + camera4uvc_payload_size;
	section_size = ((int)((section_size +DMA_ALIGNMENT-1)/DMA_ALIGNMENT))*DMA_ALIGNMENT;

	y_buffer_array_size = 0;
	cb_buffer_array_size = 0;
	cr_buffer_array_size = 0;
	y_buffer_array = NULL;
	cr_buffer_array = NULL;
	cb_buffer_array = NULL;

	ret = pxa_camera_open_device();
	if ( ret ) {
		err_print("open device failed\n");
		return ret;
	}

	/* set sensor to default value */
	camera4uvc_set_flicker_freq( 50 );
	camera4uvc_set_fps( 20 );
	camera4uvc_set_brightness(0);
	
	camera4uvc_process_state = CAMERA4UVC_INITIALIZED;
	return 0;
}
EXPORT_SYMBOL( camera4uvc_init );

int camera4uvc_open(int header_size, int payload_size)
{
	int ret = camera4uvc_acquire();

	if ( ret ) {
		dbg_print("camera4uvc open device failed, ret %d", ret);
		return ret;
	}

	ret = camera4uvc_init( header_size, payload_size );
	if ( ret ) {
		dbg_print("camera4uvc initial failed, ret %d", ret );
		camera4uvc_release();
	}
	
	return ret;
}
EXPORT_SYMBOL( camera4uvc_open );

void camera4uvc_close( void )
{
	if ( camera_state == CAMERA_STATE_UVC) {
		if ( camera4uvc_process_state == CAMERA4UVC_START) {
			camera4uvc_stopcapture();
		}
		if ( y_buffer_array != NULL) {
			kfree( y_buffer_array);
			y_buffer_array = NULL;
		}
		if ( cb_buffer_array != NULL) {
			kfree( cb_buffer_array);
			cb_buffer_array = NULL;
		}
		if ( cr_buffer_array != NULL) {
			kfree( cr_buffer_array);
			cr_buffer_array = NULL;
		}
		
		pxa_camera_close_device( );
		camera4uvc_release();
	}
}
EXPORT_SYMBOL( camera4uvc_close );

static int alloc_section_array()
{
	int i;

	camera4uvc_calc_size( g_camera_context->fifo0_transfer_size, &i, NULL, NULL);

	/* no need to update the section array */
	if ( i == y_buffer_array_size )
		return 0;
	
	if ( y_buffer_array != NULL) {
		kfree( y_buffer_array);
		y_buffer_array = NULL;
	}
	if ( cb_buffer_array != NULL) {
		kfree( cb_buffer_array);
		cb_buffer_array = NULL;
	}
	if ( cr_buffer_array != NULL) {
		kfree( cr_buffer_array);
		cr_buffer_array = NULL;
	}
	
	y_buffer_array_size = i;
	camera4uvc_calc_size(g_camera_context->fifo1_transfer_size, 
			&cb_buffer_array_size, NULL, NULL);
	camera4uvc_calc_size(g_camera_context->fifo2_transfer_size, 
			&cr_buffer_array_size, NULL, NULL);

	if ( y_buffer_array_size > 0) {
		y_buffer_array = (int*)kmalloc(y_buffer_array_size*sizeof(int), GFP_KERNEL);
		if ( !y_buffer_array ){
			err_print("Failed to alloc memory for buffer(y,cb,cr)\n");
			y_buffer_array_size = 0;
			return -1;
		}
	}

	if ( cb_buffer_array_size > 0 ) {
		cb_buffer_array = (int*)kmalloc(cb_buffer_array_size*sizeof(int), GFP_KERNEL);
		if ( !cb_buffer_array ){
			err_print("Failed to alloc memory for buffer(y,cb,cr)\n");
			kfree( y_buffer_array );
			y_buffer_array = NULL;
			y_buffer_array_size = 0;
			return -1;
		}
	}

	if (cr_buffer_array_size > 0 ) {
		cr_buffer_array = (int*)kmalloc(cr_buffer_array_size*sizeof(int), GFP_KERNEL);
		if ( !cr_buffer_array ){
			err_print("Failed to alloc memory for buffer(y,cb,cr)\n");
			kfree( y_buffer_array );
			kfree( cb_buffer_array );
			y_buffer_array = NULL;
			cr_buffer_array = NULL;
			y_buffer_array_size = 0;
			return -1;
		}
	}

	return 0;
}

int camera4uvc_startcapture()
{
	int ret;
	int i;

	if ( camera4uvc_process_state <  CAMERA4UVC_INITIALIZED ){
		err_print("camera driver is not initialized");
		return -1;
	}
	
	if ( camera4uvc_process_state ==  CAMERA4UVC_START ){
		err_print("camera driver is already start capture");
		return 0;
	}
	
#if (OUTPUT_FORMAT == CAMERA_IMAGE_FORMAT_YCBCR422_PACKED)
	camera4uvc_set_capture_format(CAMERA_IMAGE_FORMAT_YCBCR422_PACKED,
				CAMERA_IMAGE_FORMAT_YCBCR422_PACKED);
#endif

	ret = camera_start_video_capture(g_camera_context, 0, 1);
	if ( ret != 0 ){
		err_print("Fail to start capture, errno %d", ret);
		camera4uvc_dump_info();
		ret = -1;
	} else {
#if (OUTPUT_FORMAT == CAMERA_IMAGE_FORMAT_YCBCR422_PACKED)
		camera4uvc_set_sensor_register(0x197, /*reg|*/ 2);
#endif
		
		if ( alloc_section_array() != 0) {
			//stop capture
			camera_set_int_mask(g_camera_context, 0x3ff);
			camera_stop_video_capture(g_camera_context);
			return -1;
		}
		camera4uvc_process_state =  CAMERA4UVC_START;
	}
	camera4uvc_dump_info();
	
	// skip first 2 frame of data
	while( g_camera_context->block_header <= 1){
		schedule();
	}
	camera4uvc_release_current_frame();
	camera4uvc_release_current_frame();

	return ret;
}
EXPORT_SYMBOL( camera4uvc_startcapture );

int camera4uvc_stopcapture()
{
	dbg_print("Capture stop!");

	if ( camera4uvc_process_state != CAMERA4UVC_START  ){
		err_print("camera driver is not start capture\n");
		return -1;
	}
	
	camera_set_int_mask(g_camera_context, 0x3ff);
	camera_stop_video_capture(g_camera_context);
	camera4uvc_process_state = CAMERA4UVC_STOP;

	return 0;
}
EXPORT_SYMBOL( camera4uvc_stopcapture );

int camera4uvc_get_max( int attr_type, u16* value)
{
	if ( value == NULL)
		return -1;
	
	switch ( attr_type )
	{
		case CAMERA_ATTR_BRIGHTNESS:
			*value = CAMERA_BRIGHT_MAX;
			break;
		case CAMERA_ATTR_ZOOM:
			*value = CAMERA_ZOOM_MAX;
			break;
		default:
			break;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_max );

int camera4uvc_get_min( int attr_type, u16* value)
{
	if ( value == NULL)
		return -1;

	switch ( attr_type )
	{
		case CAMERA_ATTR_BRIGHTNESS:
			*value = CAMERA_BRIGHT_MIN;
			break;
		case CAMERA_ATTR_ZOOM:
			*value = CAMERA_ZOOM_MIN;
			break;
		default:
			break;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_min );

int camera4uvc_get_default( int attr_type, u16* value)
{
	if ( value == NULL)
		return -1;

	switch ( attr_type )
	{
		case CAMERA_ATTR_BRIGHTNESS:
			*value = CAMERA_BRIGHT_DEF;
			break;
		case CAMERA_ATTR_ZOOM:
			*value = CAMERA_ZOOM_DEF;
			break;
		default:
			break;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_default );

int camera4uvc_get_resolution( int attr_type, u16* value)
{
	if ( value == NULL)
		return -1;

	switch ( attr_type )
	{
		case CAMERA_ATTR_BRIGHTNESS:
			*value = CAMERA_BRIGHT_STEP;
			break;
		case CAMERA_ATTR_ZOOM:
			*value = CAMERA_ZOOM_STEP;
			break;
		default:
			break;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_resolution );

static int calcu_zoom_step( void )
{
	int zoom_step = (CAMERA_ZOOM_MAX - CAMERA_ZOOM_MIN)/CAMERA_ZOOM_STEP;
	int i;

	if ( (g_camera_context->capture_width <= 0) ||
			(g_camera_context->capture_height <= 0) )
		return 1;
	i = (MAX_SENSOR_A_HEIGHT - g_camera_context->capture_height) / zoom_step;
	if ( i < 1 )
		i = 1;
	return i;
}

int camera4uvc_get_current( int attr_type, u16* value)
{
	int i;
	int sheight;
	
	if ( camera4uvc_process_state <  CAMERA4UVC_INITIALIZED ){
		err_print("cann't get attributes without open");
		return -1;
	}
	
	if ( value == NULL)
		return -1;

	switch ( attr_type )
	{
		case CAMERA_ATTR_BRIGHTNESS:
			i = g_camera_context->capture_bright + 5;
			if ( i < CAMERA_BRIGHT_MIN )
				i = CAMERA_BRIGHT_MIN;
			if ( i > CAMERA_BRIGHT_MAX)
				i = CAMERA_BRIGHT_MAX;
			*value = i;
			break;
		case CAMERA_ATTR_ZOOM:
			sheight = MAX_SENSOR_A_HEIGHT*256/g_camera_context->capture_digital_zoom;
			i = CAMERA_ZOOM_MIN + (MAX_SENSOR_A_HEIGHT - sheight)/calcu_zoom_step();
			if (i < CAMERA_ZOOM_MIN )
				i = CAMERA_ZOOM_MIN;
			if ( i > CAMERA_ZOOM_MAX )
				i = CAMERA_ZOOM_MAX;
			*value = i;
			break;
		default:
			break;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_current );

int camera4uvc_set_current( int attr_type, u16 value)
{
	int i;
	
	if ( camera4uvc_process_state <  CAMERA4UVC_INITIALIZED ){
		err_print("cann't set attributes without open");
		return -1;
	}
	
	switch ( attr_type )
	{
		case CAMERA_ATTR_BRIGHTNESS:
			i = (int)value;
			if ( i < CAMERA_BRIGHT_MIN )
				i = CAMERA_BRIGHT_MIN;
			if ( i > CAMERA_BRIGHT_MAX)
				i = CAMERA_BRIGHT_MAX;
			camera4uvc_set_brightness( i-5 );
			break;
		case CAMERA_ATTR_ZOOM:
			i = (int)value;
			if (i < CAMERA_ZOOM_MIN )
				i = CAMERA_ZOOM_MIN;
			if ( i > CAMERA_ZOOM_MAX )
				i = CAMERA_ZOOM_MAX;
			g_camera_context->capture_digital_zoom = MAX_SENSOR_A_HEIGHT * 256 /
					(MAX_SENSOR_A_HEIGHT - (i-CAMERA_ZOOM_MIN)*calcu_zoom_step());
			break;
		default:
			break;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_set_current );

int camera4uvc_get_frame_size(u16* width, u16* height)
{
	if ( camera4uvc_process_state <  CAMERA4UVC_INITIALIZED ){
		err_print("camera driver is not opened\n");
		return -1;
	}
	
	if ( (NULL != width) && (NULL != height)) {
		*width = g_camera_context->capture_width;
		*height= g_camera_context->capture_height;
	}
	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_frame_size );

#define isFRAMESIZEOF(w, h, format) ((w == format##_W)&&(h == format##_H))
#define isVGA(w, h) isFRAMESIZEOF(w, h, VGA)
#define isQVGA(w, h) isFRAMESIZEOF(w, h, QVGA)
#define issQVGA(w, h) isFRAMESIZEOF(w, h, sQVGA)
#define isCIF(w, h) isFRAMESIZEOF(w, h, CIF)
#define isQCIF(w, h) isFRAMESIZEOF(w, h, QCIF)

int camera4uvc_set_frame_size(u16 width, u16 height)
{
	if ( camera4uvc_process_state <  CAMERA4UVC_INITIALIZED ){
		err_print("camera driver is not opened\n");
		return -1;
	}
	
	if ( isVGA(width, height) || isQVGA(width, height) ||
	  	issQVGA(width, height) || isCIF(width, height)|| 
		isQCIF(width, height) )
	{
		g_camera_context->capture_width = width;
		g_camera_context->capture_height= height;
	}

	if ( isVGA(width, height)  ) {
		camera4uvc_set_fps(15);
	}else{
		camera4uvc_set_fps(20);
	}
	
	return 0;
}
EXPORT_SYMBOL( camera4uvc_set_frame_size );

#define MCLK_DEFT 26
int camera4uvc_get_clock_frequency(int* hz)
{
	return CAMERA_FPS_DEF;
	//return MCLK_DEFT*1000000;
}
EXPORT_SYMBOL( camera4uvc_get_clock_frequency );

/* Generate buffer information for UVC */
int camera4uvc_get_current_frame( camera4uvc_buffer_t* buffer)
{
	p_camera_context_t cam_ctx = g_camera_context;
	int pages, i, j,section_per_page;
	unsigned int paddr=0;
	
	if ( camera4uvc_process_state !=  CAMERA4UVC_START ){
		err_print("camera driver is not start\n");
		return -1;
	}
	
	
	if ( !buffer )
		return -1;

	if(cam_ctx->block_header == cam_ctx->block_tail) {
		cam_ctx->task_waiting = 1;
		interruptible_sleep_on (&camera_wait_q);
		if ( cam_ctx->block_header == cam_ctx->block_tail ) {
			dbg_print( "no data available\n" );
			return -1;
		}
	}

	section_per_page = SECTION_PER_PAGE;

	/* Y  */
	pages = cam_ctx->pages_per_block * cam_ctx->block_tail;
	for (i=0; i< y_buffer_array_size; i++ ){
		if ( i%section_per_page == 0){
			paddr = page_address( cam_ctx->page_array[pages]);
			paddr += camera4uvc_header_align_size-camera4uvc_header_size;
			pages++;
		}
		y_buffer_array[i] = paddr;
		paddr += section_size;
	}
	buffer->y_array = y_buffer_array;
	buffer->y_count = y_buffer_array_size;
	buffer->y_last_size = cam_ctx->fifo0_transfer_size % camera4uvc_payload_size +
				camera4uvc_header_size;
	
	/* Cb  */
	pages = cam_ctx->pages_per_block * cam_ctx->block_tail +
			cam_ctx->pages_per_fifo0;
	for (i=0; i< cb_buffer_array_size; i++ ){
		if ( i%section_per_page == 0){
			paddr = page_address( cam_ctx->page_array[pages]);
			paddr += camera4uvc_header_align_size-camera4uvc_header_size;
			pages++;
		}
		cb_buffer_array[i] = paddr;
		paddr += section_size;
	}
	buffer->cb_array = cb_buffer_array;
	buffer->cb_count = cb_buffer_array_size;
	buffer->cb_last_size = cam_ctx->fifo1_transfer_size % camera4uvc_payload_size +
				camera4uvc_header_size;
	
	/* Cr  */
	pages = cam_ctx->pages_per_block * cam_ctx->block_tail +
			cam_ctx->pages_per_fifo0 +
			cam_ctx->pages_per_fifo1;
	for (i=0; i< cr_buffer_array_size; i++ ){
		if ( i%section_per_page == 0){
			paddr = page_address( cam_ctx->page_array[pages]);
			paddr += camera4uvc_header_align_size-camera4uvc_header_size;
			pages++;
		}
		cr_buffer_array[i] = paddr;
		paddr += section_size;
	}
	buffer->cr_array = cr_buffer_array;
	buffer->cr_count = cr_buffer_array_size;
	buffer->cr_last_size = cam_ctx->fifo2_transfer_size % camera4uvc_payload_size +
				camera4uvc_header_size;

	// simplly use FPS to measure time
	buffer->presentation_time = cam_ctx->capture_time[cam_ctx->block_tail];
	buffer->source_clock = CAMERA_FPS_DEF;

	return 0;
}
EXPORT_SYMBOL( camera4uvc_get_current_frame );

void camera4uvc_release_current_frame( void )
{
	if ( camera4uvc_process_state !=  CAMERA4UVC_START ){
		err_print("camera driver is not start\n");
		return;
	}
	
	if(g_camera_context->block_header == g_camera_context->block_tail) {
		dbg_print( "current frame is not available\n" );
		return;
	}

	g_camera_context->block_tail = 
		(g_camera_context->block_tail + 1) % g_camera_context->block_number;
}

#ifdef CAMERA4UVC_DEBUG
void camera4uvc_dump_info()
{
	p_camera_context_t ct;
	printk(KERN_INFO "camera driver state %d\n", camera_state);
	if ( !g_camera_context ){
		printk(KERN_INFO " camera driver initial failted\n");
		return;
	}
	ct = g_camera_context;
	printk(KERN_INFO "header %d payload %d\n", camera4uvc_header_size,
			camera4uvc_payload_size);
	printk(KERN_INFO "align header %d align payload %d\n", camera4uvc_header_align_size,
			camera4uvc_payload_align_size);
	printk(KERN_INFO "size(%d, %d) brightness %d zoom %d\n", 
			ct->capture_width, ct->capture_height,
			ct->capture_bright, ct->capture_digital_zoom);
	printk(KERN_INFO "buf_size: %d dma_descriptors_size:%d \n",
			ct->buf_size, ct->dma_descriptors_size);
	printk(KERN_INFO "block_number:%d block_size:%d header:%d tail:%d pages:%d\n",
			ct->block_number, ct->block_size,
			ct->block_header, ct->block_tail,
			ct->pages_per_block);
	printk(KERN_INFO "fifo	descrips	transfers	pages\n");
	printk(KERN_INFO "fifo0	%d	%d	%d\n",ct->fifo0_num_descriptors,
			ct->fifo0_transfer_size, ct->pages_per_fifo0);
	printk(KERN_INFO "fifo1	%d	%d	%d\n",ct->fifo1_num_descriptors,
			ct->fifo1_transfer_size, ct->pages_per_fifo1);
	printk(KERN_INFO "fifo2	%d	%d	%d\n",ct->fifo2_num_descriptors,
			ct->fifo2_transfer_size, ct->pages_per_fifo2);
}

void camera4uvc_dump_descriptor()
{
	p_camera_context_t ct = g_camera_context;
	pxa_dma_desc *desc;
	int i;
	
	if ( camera_state !=  CAMERA_STATE_UVC ){
		err_print("camera driver is not opened\n");
		return;
	}
	
	printk(KERN_INFO "ddadr\t\tdsadr\t\tdtadr\t\tdcmd\n");
	desc = ct->dma_descriptors_virtual; 
	for(i=0; i< ct->dma_descriptors_size; i++) {
		printk(KERN_INFO "%08x        %08x        %08x        %08x\n",
				desc->ddadr, desc->dsadr,
				desc->dtadr, desc->dcmd);
		desc++;
	}
}
#endif

/* calculate the size relative buffer, and return the buffer size */
int camera4uvc_calc_size(int data_size, 
		int* sections, int* desc_s, int* page_s)
{
	int desc_count, page_count, sec_count;
	int i = SECTION_PER_PAGE;
	
	sec_count = (data_size + camera4uvc_payload_size -1)
		/camera4uvc_payload_size;
	desc_count = sec_count;

	page_count = (sec_count + i - 1)/i;
	

	if ( sections )
		*sections = sec_count;

	if ( desc_s)
		*desc_s = desc_count;

	if ( page_s )
		*page_s = page_count;
	
	return page_count * PAGE_SIZE;;
}

static int camera4uvc_sensor_command(camera_context_t*cam_ctx, unsigned int cmd, void *param)
{

	return cam_ctx->camera_functions->camera4uvc_command(cam_ctx, cmd, param);
}

static int camera4uvc_get_sensor_register( int addr, unsigned int* value)
{
	int ret;
	unsigned int param;

	param = addr;
	ret = camera4uvc_sensor_command(g_camera_context, WCAM_VIDIOCGCAMREG, &param);
	if ( !ret && value)
		*value = param;
	return ret;
}
static int camera4uvc_set_sensor_register(int addr, unsigned int value)
{
	int param[2];

	param[0] = addr;
	param[1] = value;
	return camera4uvc_sensor_command(g_camera_context, WCAM_VIDIOCSCAMREG, param);
}

static int camera4uvc_set_brightness(int value)
{
	g_camera_context->capture_bright = value;
	return camera4uvc_sensor_command(g_camera_context, WCAM_VIDIOCSBRIGHT, NULL);
}

static int camera4uvc_set_flicker_freq(int value )
{
	g_camera_context->flicker_freq = value;
	return camera4uvc_sensor_command(g_camera_context, WCAM_VIDIOCSFLICKER, NULL);
}

static int camera4uvc_set_fps(int value)
{
	g_camera_context->fps = value;
	return camera4uvc_sensor_command(g_camera_context,WCAM_VIDIOCSFPS, NULL);
}

static int camera4uvc_set_capture_format(unsigned int input, unsigned int output)
{
	unsigned int reg = -1;

	if(input >  CAMERA_IMAGE_FORMAT_MAX ||
			output > CAMERA_IMAGE_FORMAT_MAX ){
		err_print("invalid format input %d output %d", input, output);
		return -1;
	}

	g_camera_context->capture_input_format = input;
	g_camera_context->capture_output_format = output;

	if ( output == CAMERA_IMAGE_FORMAT_YCBCR422_PACKED ) {
		/* set sensor MT9D111 to swap chrominance byte 
		 * with luminance byte in YUV output
		 */
		//camera4uvc_get_sensor_register(0x197, &reg);
		camera4uvc_set_sensor_register(0x197, /*reg|*/ 2);
	}

	return 0;
}

#if 0
static int camera4uvc_set_mirror(int value)
{
	int param[2];
	param[0] = value;
	return camera4uvc_sensor_command(g_camera_context,WCAM_VIDIOCSMIRROR, param);
}

static int camera4uvc_set_mode(int mode, int maxexpotime)
{
	int param[2];
	param[0] = mode;
	param[1] = maxexpotime;
	return camera4uvc_sensor_command(g_camera_context,WCAM_VIDIOCSNIGHTMODE, param);
}
#endif

static int camera4uvc_generate_fifo2_dma_chain(p_camera_context_t camera_context, 
		pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
	pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
	int des_transfer_size, remain_size, target_page_num;
	unsigned int i,j;
	int sections_per_page;
	int page_paddr=0;

	dbg_print("fifo2");

	cur_des_virtual = (pxa_dma_desc *)camera_context->fifo2_descriptors_virtual;
	cur_des_physical = (pxa_dma_desc *)camera_context->fifo2_descriptors_physical;

	sections_per_page = SECTION_PER_PAGE;

	for(i=0; i<camera_context->block_number; i++)     {
		// in each iteration, generate one dma chain for one frame
		remain_size = camera_context->fifo2_transfer_size;
		target_page_num = i * camera_context->pages_per_block +
					camera_context->pages_per_fifo0 +
					camera_context->pages_per_fifo1;

		for(j=0; j<camera_context->fifo2_num_descriptors; j++)  {
			// set descriptor
			if (remain_size > camera4uvc_payload_size)
				des_transfer_size = camera4uvc_payload_size;
			else
				des_transfer_size = remain_size;

			if ( (j % sections_per_page) == 0) {
				page_paddr = 
					page_to_bus( camera_context->page_array[target_page_num] );
				target_page_num ++;
			}

			cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
			cur_des_virtual->dsadr = CIBR2_PHY;       // FIFO2 physical address
			cur_des_virtual->dtadr = page_paddr + camera4uvc_header_align_size;
			cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
			ddbg_print( "(%03d, %03d): page %03d addr %08x len %d\n",
					i,j, target_page_num - 1, cur_des_virtual->dtadr, des_transfer_size);

			// advance pointers
			remain_size -= des_transfer_size;
			cur_des_virtual++;
			cur_des_physical++;
			page_paddr += section_size;
		}
		// stop the dma transfer on one frame captured
		last_des_virtual = cur_des_virtual - 1;
	}
	
	last_des_virtual->ddadr = ((unsigned)camera_context->fifo2_descriptors_physical);
	*cur_vir = cur_des_virtual;
	*cur_phy = cur_des_physical;

	return 0;
}

static int camera4uvc_generate_fifo1_dma_chain(p_camera_context_t camera_context, 
		pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
	pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
	int des_transfer_size, remain_size, target_page_num;
	unsigned int i,j;
	int sections_per_page;
	unsigned long page_paddr=0;

	dbg_print("fifo1");

	cur_des_virtual = (pxa_dma_desc *)camera_context->fifo1_descriptors_virtual;
	cur_des_physical = (pxa_dma_desc *)camera_context->fifo1_descriptors_physical;

	sections_per_page = SECTION_PER_PAGE;

	for(i=0; i<camera_context->block_number; i++)     {
		// in each iteration, generate one dma chain for one frame
		remain_size = camera_context->fifo1_transfer_size;
		target_page_num = i * camera_context->pages_per_block +
					camera_context->pages_per_fifo0;

		for(j=0; j<camera_context->fifo1_num_descriptors; j++)  {
			// set descriptor
			if (remain_size > camera4uvc_payload_size)
				des_transfer_size = camera4uvc_payload_size;
			else
				des_transfer_size = remain_size;

			if ( (j % sections_per_page) == 0) {
				page_paddr = 
					page_to_bus( camera_context->page_array[target_page_num] );
				target_page_num ++;
			}

			cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
			cur_des_virtual->dsadr = CIBR1_PHY;       // FIFO1 physical address
			cur_des_virtual->dtadr = page_paddr + camera4uvc_header_align_size;
			cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
			ddbg_print("(%03d, %03d): page %03d addr %08x len %d\n",
					i,j, target_page_num - 1, cur_des_virtual->dtadr, des_transfer_size);

			// advance pointers
			remain_size -= des_transfer_size;
			cur_des_virtual++;
			cur_des_physical++;
			page_paddr += section_size;
		}
		// stop the dma transfer on one frame captured
		last_des_virtual = cur_des_virtual - 1;
	}
	
	last_des_virtual->ddadr = ((unsigned)camera_context->fifo1_descriptors_physical);
	*cur_vir = cur_des_virtual;
	*cur_phy = cur_des_physical;

	return 0;
}

static int camera4uvc_generate_fifo0_dma_chain(p_camera_context_t camera_context, 
		pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
	pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
	int des_transfer_size, remain_size, target_page_num;
	unsigned int i,j;
	int sections_per_page;
	unsigned long page_paddr=0;

	dbg_print("fifo0");

	cur_des_virtual = (pxa_dma_desc *)camera_context->fifo0_descriptors_virtual;
	cur_des_physical = (pxa_dma_desc *)camera_context->fifo0_descriptors_physical;

	sections_per_page = SECTION_PER_PAGE;

	for(i=0; i<camera_context->block_number; i++)     {
		// in each iteration, generate one dma chain for one frame
		remain_size = camera_context->fifo0_transfer_size;
		target_page_num = i *  camera_context->pages_per_block;

		for(j=0; j<camera_context->fifo0_num_descriptors; j++)  {
			// set descriptor
			if (remain_size > camera4uvc_payload_size)
				des_transfer_size = camera4uvc_payload_size;
			else
				des_transfer_size = remain_size;

			if ( (j % sections_per_page) == 0) {
				page_paddr = 
					page_to_bus( camera_context->page_array[target_page_num] );
				target_page_num ++;
			}

			cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
			cur_des_virtual->dsadr = CIBR0_PHY;       // FIFO0 physical address
			cur_des_virtual->dtadr = page_paddr + camera4uvc_header_align_size;
			cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
			ddbg_print("(%03d, %03d): page %03d addr %08x len %d\n",
					i,j, target_page_num - 1, cur_des_virtual->dtadr, des_transfer_size);

			// advance pointers
			remain_size -= des_transfer_size;
			cur_des_virtual++;
			cur_des_physical++;
			page_paddr += section_size;
		}
		// stop the dma transfer on one frame captured
		last_des_virtual = cur_des_virtual - 1;
	}
	
	last_des_virtual->ddadr = ((unsigned)camera_context->fifo0_descriptors_physical);
	*cur_vir = cur_des_virtual;
	*cur_phy = cur_des_physical;

	return 0;
}

static int camera4uvc_update_dma_chain(p_camera_context_t camera_context)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical; 
    int desc_count;
    
    //lxlxlxlxlx
    dbg_print("start...");

    // clear descriptor pointers
    camera_context->fifo0_descriptors_virtual = camera_context->fifo0_descriptors_physical = 0;
    camera_context->fifo1_descriptors_virtual = camera_context->fifo1_descriptors_physical = 0;
    camera_context->fifo2_descriptors_virtual = camera_context->fifo2_descriptors_physical = 0;

    // calculate how many descriptors are needed per frame
    camera4uvc_calc_size(camera_context->fifo0_transfer_size, NULL, &desc_count, NULL);
    camera_context->fifo0_num_descriptors = desc_count;
    camera4uvc_calc_size(camera_context->fifo1_transfer_size, NULL, &desc_count, NULL);
    camera_context->fifo1_num_descriptors = desc_count;
    camera4uvc_calc_size(camera_context->fifo2_transfer_size, NULL, &desc_count, NULL);
    camera_context->fifo2_num_descriptors = desc_count;

    // check if enough memory to generate descriptors
    if((camera_context->fifo0_num_descriptors + camera_context->fifo1_num_descriptors + 
        camera_context->fifo2_num_descriptors) * camera_context->block_number 
         > camera_context->dma_descriptors_size)
    {
	    camera4uvc_dump_info();
      	    return -ENOMEM;
    }

    // generate fifo0 dma chains
    camera_context->fifo0_descriptors_virtual = (unsigned)camera_context->dma_descriptors_virtual;
    camera_context->fifo0_descriptors_physical = (unsigned)camera_context->dma_descriptors_physical;
    // generate fifo0 dma chains
    camera4uvc_generate_fifo0_dma_chain(camera_context, &cur_des_virtual, &cur_des_physical);
     
    // generate fifo1 dma chains
    if(!camera_context->fifo1_transfer_size)
    {
       return 0;
    }
    // record fifo1 descriptors' start address
    camera_context->fifo1_descriptors_virtual = (unsigned)cur_des_virtual;
    camera_context->fifo1_descriptors_physical = (unsigned)cur_des_physical;
    // generate fifo1 dma chains
    camera4uvc_generate_fifo1_dma_chain(camera_context, &cur_des_virtual, &cur_des_physical);
    
    if(!camera_context->fifo2_transfer_size) 
    {
      return 0;
    }
    // record fifo1 descriptors' start address
    camera_context->fifo2_descriptors_virtual = (unsigned)cur_des_virtual;
    camera_context->fifo2_descriptors_physical = (unsigned)cur_des_physical;
    // generate fifo2 dma chains
    camera4uvc_generate_fifo2_dma_chain(camera_context,  &cur_des_virtual, &cur_des_physical);
        
    return 0;   
}

int camera4uvc_ring_buf_init(p_camera_context_t camera_context)
{
	unsigned         frame_size;
	unsigned int width, height;
	unsigned int output_format;
	int desc;
	int descriptors_per_block;
	
	width = camera_context->capture_width;
	height = camera_context->capture_height;
	output_format = camera_context->capture_output_format;

	switch(output_format)
	{
		case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
			frame_size = width * height * 2;
			camera_context->fifo0_transfer_size = frame_size;
			camera_context->fifo1_transfer_size = 0;
			camera_context->fifo2_transfer_size = 0;
			camera_context->plane_number = 1;
			break;
		case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
			frame_size = width * height * 2;
			camera_context->fifo0_transfer_size = frame_size / 2;
			camera_context->fifo1_transfer_size = frame_size / 4;
			camera_context->fifo2_transfer_size = frame_size / 4;
			camera_context->plane_number = 3;
			break;
#if 0
		/* not support by camera4uvc interface now */
		case CAMERA_IMAGE_FORMAT_RGB565:
			frame_size = width * height * 2;
			camera_context->fifo0_transfer_size = frame_size;
			camera_context->fifo1_transfer_size = 0;
			camera_context->fifo2_transfer_size = 0;
			camera_context->plane_number = 1;
			break;
		case CAMERA_IMAGE_FORMAT_RGB666_PLANAR:
			frame_size = width * height * 4;
			camera_context->fifo0_transfer_size = frame_size;
			camera_context->fifo1_transfer_size = 0;
			camera_context->fifo2_transfer_size = 0;
			camera_context->plane_number = 1;
			break;
		case CAMERA_IMAGE_FORMAT_RGB666_PACKED:
			frame_size = width * height * 3;
			camera_context->fifo0_transfer_size = frame_size;
			camera_context->fifo1_transfer_size = 0;
			camera_context->fifo2_transfer_size = 0;
			camera_context->plane_number = 1;
			break;
		case CAMERA_IMAGE_FORMAT_RAW8:
			frame_size = width * height * 1;
			camera_context->fifo0_transfer_size = frame_size;
			camera_context->fifo1_transfer_size = 0;
			camera_context->fifo2_transfer_size = 0;
			camera_context->plane_number = 1;
			break;
#endif
		default:
			return -EINVAL;
	}

	camera_context->block_size = 
		camera4uvc_calc_size( frame_size, NULL, NULL, NULL );

	camera4uvc_calc_size(camera_context->fifo0_transfer_size, 
			NULL, NULL, 
			&camera_context->pages_per_fifo0);
	camera4uvc_calc_size(camera_context->fifo1_transfer_size, 
			NULL, NULL, 
			&camera_context->pages_per_fifo1);
	camera4uvc_calc_size(camera_context->fifo2_transfer_size, 
			NULL, NULL, 
			&camera_context->pages_per_fifo2);
	
	camera_context->pages_per_block =
			camera_context->pages_per_fifo0 +
			camera_context->pages_per_fifo1 +
			camera_context->pages_per_fifo2;

	camera_context->page_aligned_block_size =
			camera_context->pages_per_block * PAGE_SIZE;

	camera_context->block_number_max =
			camera_context->pages_allocated /
			camera_context->pages_per_block;

	/* check descriptor size */
	desc = camera_context->dma_descriptors_size;
	descriptors_per_block = 
		camera_context->pages_per_block * SECTION_PER_PAGE;
	if ( camera_context->block_number_max > (int)(desc/descriptors_per_block))
		camera_context->block_number_max=desc/descriptors_per_block;

	//restrict max block number
	if(camera_context->block_number_max > VIDEO_MAX_FRAME)
	{
		camera_context->block_number_max = VIDEO_MAX_FRAME;
	}

	if ( camera_context->block_number_max > 5)
	{
		camera_context->block_number = 5;
	}
	else
	{
		camera_context->block_number = camera_context->block_number_max;
	}

	if( camera_context->block_number < FRAMES_IN_BUFFER )
	{
		err_print("Out of Memory");
		camera4uvc_dump_info();
		return -ENOMEM;
	}

	camera_context->block_header = camera_context->block_tail = 0;
	// generate dma descriptor chain
	return camera4uvc_update_dma_chain(camera_context);

}


