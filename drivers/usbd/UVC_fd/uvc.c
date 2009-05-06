/*
                                                                               
 *  Module Name:  uvc.c

 *  General Description: Motorola UVC function driver

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
Author                   Date        Description of Changes
-----------------    ------------    -------------------------
Motorola              01/10/2006         Initial Version
Motorola              01/23/2006         Handle FIFO overrun
Motorola              03/07/2006         Handle the phone power down when web camera connect to netmeeting
Motorola              05/17/2006         Handle the displaying problem at the beginning of changing size
Motorola              08/17/2006         update comments for GPL
Motorola              08/29/2006         fix some minor faults
Motorola              09/28/2006         Simplify the parameter negotiation process to accommodate a Netmeeting bug. 
                                         Remove the Color Matching Descriptor. 
                                         Add some comments for the 0-bandwidth endpoint.
                                         The kernel thread will return to the INIT state when setting Alternate 0.
*/
/*================================================================================
                                 INCLUDE FILES
================================================================================*/
#include <linux/config.h>
#include <linux/module.h>
#include "../usbd-build.h"
#include "../usbd-export.h"

MODULE_AUTHOR ("Motorola");
MODULE_DESCRIPTION ("UVC Function Driver");

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/autoconf.h>
#include <asm/hardware.h>

#include <linux/ezxusbd.h>

#include "../usbd-chap9.h"
#include "../usbd-mem.h"
#include "../usbd.h"
#include "../usbd-func.h"
#include "uvc.h"

USBD_MODULE_INFO ("uvc 1.0");

/*================================================================================
                                  LOCAL MACROS
================================================================================*/
//#define UVC_DEBUG 	1	/* For debug information */
//#define PSEUDO_DATA	1	/* 1 indicate use pseudo data for testing */
//#define UVC_DEMO_RGB
#define CAMERA_TERMINAL_ZOOM

#ifdef  UVC_DEBUG
#define dbg_printk(fmt, args...) printk( fmt , ## args)
#else
#define dbg_printk(fmt, args...)
#endif

/*================================================================================
                                LOCAL CONSTANTS
================================================================================*/
/* format id */
#define FORMAT_CIF	0
#define FORMAT_QCIF	1
#define FORMAT_VGA	2
#define FORMAT_QVGA	3
#define FORMAT_SQVGA	4
#define FORMAT_DEFAULT  FORMAT_CIF

#define FORMAT_CIF_WIDTH	352
#define FORMAT_CIF_HEIGHT	288
#define FORMAT_QCIF_WIDTH	176
#define FORMAT_QCIF_HEIGHT	144
#define FORMAT_VGA_WIDTH	640
#define FORMAT_VGA_HEIGHT	480
#define FORMAT_QVGA_WIDTH	320
#define FORMAT_QVGA_HEIGHT	240
#define FORMAT_SQVGA_WIDTH	160
#define FORMAT_SQVGA_HEIGHT	120
#define FORMAT_DEFAULT_WIDTH	FORMAT_CIF_WIDTH
#define FORMAT_DEFAULT_HEIGHT	FORMAT_CIF_HEIGHT

#define CAMERA_ATTR_BRIGHTNESS 1
#define CAMERA_ATTR_ZOOM 2
#define CAMERA_BRIGHT_DEF 5
#define CAMERA_ZOOM_DEF 1

/* interface id */
#define VC_INTERFACE_ID	0
#define VS_INTERFACE_ID	1
/* terminal and unit id */
#define ID_NONE 0
#define ID_CT1	1
#define	ID_IT1	2
#define	ID_OT1	3
#define	ID_SU1	4
#define	ID_PU1	5

/* endpoint index */
#define UVC_ISO_IN_ENDPOINT			0x00

/* duration between continuous read-null operations */
#define UVC_VIDEO_FRAME_DELAY_MAX	(50*HZ/1000)	/* 50 ms */

/* bandwidth parameters */
#define ISO_HS_PAYLOAD_HEADER_LENGTH	12
#define ISO_HS_PAYLOAD_DATA_LENGTH		(ISO_HS_PAYLOAD_TRANSFER_LENGTH - ISO_HS_PAYLOAD_HEADER_LENGTH)
#define ISO_HS_PAYLOAD_TRANSFER_LENGTH	3068
#define ISO_FS_PAYLOAD_HEADER_LENGTH	12
#define ISO_FS_PAYLOAD_DATA_LENGTH		(ISO_FS_PAYLOAD_TRANSFER_LENGTH - ISO_FS_PAYLOAD_HEADER_LENGTH)
#define ISO_FS_PAYLOAD_TRANSFER_LENGTH	1020
/* bandwidth parameters */

/* used to skip several frames each time changing frame size */
#define UVC_FRAME_SKIP_NUM		5

#ifdef PSEUDO_DATA
unsigned char * uvc_debug_video_frame_buf = NULL; /* pseudo video frame */
#endif	/* #ifdef PSEUDO_DATA */


/*================================================================================
                                EXTERNAL FUNCTION DECLARATIONS
================================================================================*/
extern int cam_ipm_hook(void);
extern void cam_ipm_unhook(void);
/*================================================================================
                                LOCAL VARIABLES
================================================================================*/
static __u8 err_code = REQUEST_NO_ERROR;
static __u8 cur_input_terminal = ID_NONE;
static unsigned int payload_transfer_length = ISO_FS_PAYLOAD_TRANSFER_LENGTH;
static unsigned int payload_header_length = ISO_FS_PAYLOAD_HEADER_LENGTH;
static unsigned int payload_data_length = ISO_FS_PAYLOAD_DATA_LENGTH;
static DECLARE_MUTEX_LOCKED (uvc_thread_sem);

typedef enum {
	THREAD_IDLE,
	THREAD_INIT,
	THREAD_RUNNING,
	THREAD_STOP,
} uvc_thread_state_t;

typedef struct {
	uvc_thread_state_t thread_state;
	spinlock_t thread_state_lock;
} uvc_state_t;

static uvc_state_t uvc_state = {
	thread_state: THREAD_STOP,
	thread_state_lock: SPIN_LOCK_UNLOCKED,
};

typedef struct {
	__u8 index;
	__u16 width;
	__u16 height;
} uvc_video_frame_t;

typedef enum {
	SET_CUR_NONE,
	SET_CUR_BRIGHTNESS,
	SET_CUR_DIGITAL_MULTIPLIER,
	SET_CUR_FRAME,
} uvc_set_cur_op_code_t;

typedef struct {
	uvc_set_cur_op_code_t op_code;
	spinlock_t op_lock;
	__u16 cur_brightness;
	__u16 cur_digital_multiplier;
	uvc_video_frame_t cur_frame;
} uvc_set_cur_op_t;

static uvc_set_cur_op_t uvc_set_cur_op = {
	op_code: SET_CUR_NONE,
	op_lock: SPIN_LOCK_UNLOCKED,
	cur_brightness: CAMERA_BRIGHT_DEF,
	cur_digital_multiplier: CAMERA_ZOOM_DEF,
	cur_frame: {	index: FORMAT_DEFAULT, 
			width: FORMAT_DEFAULT_WIDTH, 
			height: FORMAT_DEFAULT_HEIGHT,},
};
/*================================================================================
                            LOCAL POINTER DECLARATIONS
================================================================================*/
static struct usb_function_instance * uvc_function_instance = NULL;
static struct uvc_req_operations *vs_control_ops[VS_CONTROL_MAX_UNDEFINED];
static struct uvc_req_operations *pu_control_ops[PU_CONTROL_MAX_UNDEFINED];
#ifdef CAMERA_TERMINAL_ZOOM
static struct uvc_req_operations *ct_control_ops[CT_CONTROL_MAX_UNDEFINED];
#endif
static struct probe_commit_data *cur_vs_frame = NULL;
static struct probe_commit_data *probe_state = NULL;

static struct urb * uvc_iso_in_urb = NULL;

/*================================================================================
                            LOCAL FUNCTION PROTOTYPES
================================================================================*/
static void uvc_set_thread_state(uvc_thread_state_t state);
static int uvc_thread_state_is(uvc_thread_state_t state);
static void uvc_event_irq(struct usb_function_instance *function, usb_device_event_t event, int data);
static int uvc_recv_setup_irq  (struct usb_device_request *request);
static int uvc_function_enable( struct usb_function_instance *function );
static void uvc_function_disable( struct usb_function_instance *function );

static int uvc_alloc_urb_buffer(struct usb_function_instance *function);
static void uvc_free_urb_buffer(void);
static int uvc_urb_sent (struct urb *urb, int rc);
static int uvc_kernel_thread(void *data);
static int uvc_kernel_thread_kickoff(void);
static void uvc_kernel_thread_killoff(void);
static int uvc_modinit(void);
static void uvc_modexit(void);

/*
 * functions that camera driver export to UVC driver
 */
#ifndef PSEUDO_DATA
#include "../../media/video/camera4uvc.h"
static camera4uvc_buffer_t uvc_video_frame_buffer;
#ifdef UVC_DEMO_RGB
#include "red.h"
#include "blue.h"
#define MAX_PAYLOAD_TRANSFER	610 /* 2*640*480/1008 */
#define COLOR_TOGGLE_NUM	3000000
int camera4uvc_get_current_frame_demo(camera4uvc_buffer_t * frame_buffer)
{
	static int payload[MAX_PAYLOAD_TRANSFER];
	static unsigned int call_num = 0;
	int i;
	u16 width, height;
	
	if(!frame_buffer)
		return -EINVAL;

	i = camera4uvc_get_frame_size(&width, &height);
	if(i)
		return -EINVAL;

	call_num++;
	if(call_num == 1) {
		for(i = 0; i < MAX_PAYLOAD_TRANSFER; i++)
			payload[i] = (int)my_picture_red;
	} 
	else if(call_num == COLOR_TOGGLE_NUM/(width*height)) {
		for(i = 0; i < MAX_PAYLOAD_TRANSFER; i++)
			payload[i] = (int)my_picture_blue;
	}
	else if(call_num > 2*COLOR_TOGGLE_NUM/(width*height)) {
		call_num = 0;
	}

	frame_buffer->y_array = payload;
	if((2*width*height) % payload_data_length) {
		frame_buffer->y_count = (2*width*height)/payload_data_length + 1;
		frame_buffer->y_last_size = (2*width*height)%payload_data_length + payload_header_length;
	}
	else {
		frame_buffer->y_count = (2*width*height)/payload_data_length;
		frame_buffer->y_last_size = 0;
	}

	return 0;
}
#endif /* #ifdef UVC_DEMO_RGB */
#else /* #ifndef PSEUDO_DATA */
static __u16 dumb_brightness = 1;
static __u16 dumb_zoom = 1;
static __u16 dumb_width = 352;
static __u16 dumb_height = 288;
int camera4uvc_open (int header_size, int payload_size){ return 0; }
void camera4uvc_close (void){}
int camera4uvc_startcapture (void){return 0;}
void camera4uvc_stopcapture (void){}
int camera4uvc_get_max (int attr_type, __u16 * value)
{
	if (attr_type  == CAMERA_ATTR_BRIGHTNESS)
		*value = 4; //max brightness
	else if (attr_type == CAMERA_ATTR_ZOOM)
		*value = 8; //max digi-zoom

	return 0;
}

int camera4uvc_get_min (int attr_type, __u16 * value)
{
	if (attr_type  == CAMERA_ATTR_BRIGHTNESS)
		*value = 1; //min brightness
	else if (attr_type == CAMERA_ATTR_ZOOM)
		*value = 1; //min digi-zoom

	return 0;
}
int camera4uvc_get_default (int attr_type, __u16 * value)
{
	if (attr_type  == CAMERA_ATTR_BRIGHTNESS)
		*value = 2; 
	else if (attr_type == CAMERA_ATTR_ZOOM)
		*value = 1; 

	return 0;
}
int camera4uvc_get_resolution (int attr_type, __u16 * value)
{
	if (attr_type  == CAMERA_ATTR_BRIGHTNESS)
		*value = 1; 
	else if (attr_type == CAMERA_ATTR_ZOOM)
		*value = 1; 

	return 0;
}
int camera4uvc_get_current (int attr_type, __u16 * value)
{
	if (attr_type  == CAMERA_ATTR_BRIGHTNESS)
		*value = dumb_brightness; 
	else if (attr_type == CAMERA_ATTR_ZOOM)
		*value = dumb_zoom; 
	
	return 0;
}
int camera4uvc_set_current (int attr_type, __u16 value)
{
	if (attr_type  == CAMERA_ATTR_BRIGHTNESS)
		dumb_brightness = value;
	else if (attr_type == CAMERA_ATTR_ZOOM)
		dumb_zoom = value;

	return 0;
}
int camera4uvc_get_frame_size (__u16 * width, __u16 * height)
{
	*width = dumb_width;
	*height = dumb_height;
	return 0;
}
int camera4uvc_set_frame_size (__u16 width, __u16 height)
{
	if (width == 352 && height == 288) {
		dumb_width = width;
		dumb_height = height;
	}
	else if (width == 176 && height == 144) {
		dumb_width = width;
		dumb_height = height;
        }
	else if (width == 640 && height == 480) {
		dumb_width = width;
		dumb_height = height;
        }
	else if (width == 320 && height == 240) {
		dumb_width = width;
		dumb_height = height;
        }
	else if (width == 160 && height == 120) {
		dumb_width = width;
		dumb_height = height;
        }
	else
		return -1;

	return 0;	
}
#endif /* #ifndef PSEUDO_DATA */

/**vs class**/
//vs input header
static struct usb_video_streaming_interface_input_header_descriptor uvc_vs_input_header = {
	bLength: sizeof(struct usb_video_streaming_interface_input_header_descriptor), //0x0e,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_INPUT_HEADER,
	bNumFormats: 0x01,
	wTotalLength: 0x00C5,
	bEndpointAddress: 0x81,
	bmInfo: 0x00,
	bTerminalLink: 0x03,
	bStillCaptureMethod: 0x00,
	bTriggerSupport: 0x00,
	bTriggerUsage: 0x00,
	bControlSize: 0x01,
	bmaControls: 0x00
};
//vs format

static struct usb_video_payload_format_descriptor uvc_vs_format = {
	bLength: sizeof(struct usb_video_payload_format_descriptor), //0x1b,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_FORMAT_UNCOMPRESSED,
	bFormatIndex: 0x01,
	bNumFrameDescriptors: 0x05, 
	guidFormat: {0x59, 0x55, 0x59, 0x32, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71},	//YUV2
	//guidFormat: {0x4E, 0x56, 0x31, 0x32, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}, 	//NV12
	bBitsPerPixel: 0x10,
	bDefaultFrameIndex: 0x01,
	bAspectRatioX: 0x04,
	bAspectRatioY: 0x03,
	bmInterlaceFlags: 0x00,
	bCopyProtect: 0x00,
};

//vs frame one
static struct usb_video_frame_descriptor uvc_vs_frame_cif = {
	bLength: sizeof(struct usb_video_frame_descriptor), //0x1e,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_FRAME_UNCOMPRESSED,
	bFrameIndex: 0x01,
	bmCapabilities: 0x02,
	wWidth: 0x0160,
	wHeight: 0x0120,
	dwMinBitRate: 0x01EF0000,
	dwMaxBitRate: 0x01EF0000,
	dwMaxVideoFrameBufferSize: 0x00031800,
	dwDefaultFrameInterval: 0x0007A120,
	bFrameIntervalType: 0x01,
	dwFrameInterval: 0x0007A120,
};
//vs frame 2
static struct usb_video_frame_descriptor uvc_vs_frame_qcif = {
	bLength: sizeof(struct usb_video_frame_descriptor), //0x1e,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_FRAME_UNCOMPRESSED,
	bFrameIndex: 0x02,
	bmCapabilities: 0x02,
	wWidth: 0x00b0, 
	wHeight: 0x0090, 
	dwMinBitRate: 0x007BC000,
	dwMaxBitRate: 0x007BC000,
	dwMaxVideoFrameBufferSize: 0x0000C600,
	dwDefaultFrameInterval: 0x0007A120,
	bFrameIntervalType: 0x01, 
	dwFrameInterval: 0x0007A120,
};
static  struct usb_video_frame_descriptor uvc_vs_frame_vga = {
	bLength: sizeof(struct usb_video_frame_descriptor), //0x1e,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_FRAME_UNCOMPRESSED,
	bFrameIndex: 0x03,
	bmCapabilities: 0x02,
	wWidth: 0x0280,
	wHeight: 0x01e0,
	dwMinBitRate: 0x04650000,
	dwMaxBitRate: 0x04650000,
	dwMaxVideoFrameBufferSize: 0x00096000,
	dwDefaultFrameInterval: 0x000A2C2A,
	bFrameIntervalType: 0x01, 
	dwFrameInterval: 0x000A2C2A,
};

static  struct usb_video_frame_descriptor uvc_vs_frame_qvga = {
	bLength: sizeof(struct usb_video_frame_descriptor), //0x1e,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_FRAME_UNCOMPRESSED,
	bFrameIndex: 0x04,
	bmCapabilities: 0x02, 
	wWidth: 0x0140,
	wHeight: 0x00f0,
	dwMinBitRate: 0x01770000,
	dwMaxBitRate: 0x01770000,
	dwMaxVideoFrameBufferSize: 0x00025800,
	dwDefaultFrameInterval: 0x0007A120,
	bFrameIntervalType: 0x01, 
	dwFrameInterval: 0x0007A120,
};

static struct usb_video_frame_descriptor uvc_vs_frame_sqvga = {
	bLength: sizeof(struct usb_video_frame_descriptor), //0x1e, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VS_FRAME_UNCOMPRESSED,
	bFrameIndex: 0x05, 
	bmCapabilities: 0x02, 
	wWidth: 0x00a0,
	wHeight: 0x0078,
	dwMinBitRate: 0x005DC000,
	dwMaxBitRate: 0x005DC000,
	dwMaxVideoFrameBufferSize: 0x00009600,
	dwDefaultFrameInterval: 0x0007A120,
	bFrameIntervalType: 0x01, 
	dwFrameInterval: 0x0007A120,
};
static struct usb_color_matching_descriptor uvc_vs_color_match_class={
	bLength: sizeof(struct usb_color_matching_descriptor), //0x06,
	bDescriptorType: CS_INTERFACE,
	bDescriptorSubtype: VS_COLORFORMAT,
	bColorPrimaries: 0x01,
	bTransferCharacteristics: 0x01,
	MatrixCoefficients: 0x01,
};
/**vs class end**/

//vs class descriptors
static struct usb_generic_class_descriptor *uvc_vs_class_descriptors[] = { 
	(struct usb_generic_class_descriptor*) &uvc_vs_input_header, 
	(struct usb_generic_class_descriptor*) &uvc_vs_format,
	(struct usb_generic_class_descriptor*) &uvc_vs_frame_cif,
	(struct usb_generic_class_descriptor*) &uvc_vs_frame_qcif,
	(struct usb_generic_class_descriptor*) &uvc_vs_frame_vga,
	(struct usb_generic_class_descriptor*) &uvc_vs_frame_qvga,
	(struct usb_generic_class_descriptor*) &uvc_vs_frame_sqvga,
	/* (struct usb_generic_class_descriptor*) &uvc_vs_color_match_class, */
};

/**vc class**/
//vc-header 
static struct usb_video_control_interface_header_descriptor uvc_vc_header = { 
	bLength: sizeof(struct usb_video_control_interface_header_descriptor), //0x0d, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VC_HEADER, 
	bcdUVC: 0x0100,
	wTotalLength: 0x0042,
	dwClockFrequency: 0x018CBA80,
	bInCollection: 0x01, 
	baInterfaceNr: 0x01,
};
//vc-input terminal
static struct usb_video_control_camera_terminal_descriptor uvc_vc_camera_terminal = { 
	bLength: sizeof(struct usb_video_control_camera_terminal_descriptor), //0x11, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VC_INPUT_TERMINAL, 
	bTerminalID: ID_CT1,
	wTerminalType: ITT_CAMERA,
	bAssocTerminal: 0x00, 
	iTerminal: 0x00, 
#ifndef CAMERA_TERMINAL_ZOOM
	wObjectiveFocalLengthMin: 0x0000,
	wObjectiveFocalLengthMax: 0x0000, 
	wOcularFocalLength: 0x0000, 
#else
	wObjectiveFocalLengthMin: 0x0064,	/* 100 */
	wObjectiveFocalLengthMax: 0x0320,	/* 800 */
	wOcularFocalLength: 0x0064,		/* 100; M = Zcur, see P17 of UVC11 spec */
#endif
	bControlSize: 0x02, 
#ifndef CAMERA_TERMINAL_ZOOM
	bmControls: 0x0000,	/* D9: Zoom (Absolute) */
#else
	bmControls: 0x0200,	/* D9: Zoom (Absolute) */
#endif
}; 

static struct usb_video_control_input_terminal_descriptor uvc_vc_input_terminal = { 
	bLength: sizeof(struct usb_video_control_input_terminal_descriptor), //0x08, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VC_INPUT_TERMINAL, 
	bTerminalID: ID_IT1,
	wTerminalType: 0x0401,
	bAssocTerminal: 0x00, 
	iTerminal: 0x00, 

}; 

//vc-output terminal
static struct usb_video_control_output_terminal_descriptor uvc_vc_output_terminal = {
	bLength: sizeof(struct usb_video_control_output_terminal_descriptor), //0x09, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VC_OUTPUT_TERMINAL, 
	bTerminalID: ID_OT1,
	wTerminalType: TT_STREAMING,
	bAssocTerminal: 0x00, 
	bSourceID: ID_PU1, 
	iTerminal: 0x00,
};            
//selector unit
static struct usb_video_control_selector_unit_descriptor uvc_vc_selector_unit = {
	bLength: sizeof(struct usb_video_control_selector_unit_descriptor), //0x08, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VC_SELECTOR_UNIT, 
	bUnitID: ID_SU1,
	bNrInPins: 0x02, 
	baSourceID1: ID_CT1, 
	baSourceID2: ID_IT1, 
	iSelector: 0x00,
};        

//vc-processing unit
static struct usb_video_control_processing_unit_descriptor uvc_vc_processing_unit = {
	bLength: sizeof(struct usb_video_control_processing_unit_descriptor), //0x0b, 
	bDescriptorType: CS_INTERFACE, 
	bDescriptorSubtype: VC_PROCESSING_UNIT, 
	bUnitID: ID_PU1, 
	bSourceID: ID_SU1, 
	wMaxMultiplier: 0x0320,	/* 8*100 */
	bControlSize: 0x02, 
	bmControls: 0x4001,	/* D14: Digital Multiplier; D0: Brightness */
	iProcessing: 0x00, 
};
/**vc class end**/

//vc class descriptors
static struct usb_generic_class_descriptor *uvc_vc_class_descriptors[] = {
	(struct usb_generic_class_descriptor*) &uvc_vc_header, 
	(struct usb_generic_class_descriptor*) &uvc_vc_camera_terminal,
	(struct usb_generic_class_descriptor*) &uvc_vc_input_terminal,
	(struct usb_generic_class_descriptor*) &uvc_vc_output_terminal, 
	(struct usb_generic_class_descriptor*) &uvc_vc_selector_unit,
	(struct usb_generic_class_descriptor*) &uvc_vc_processing_unit,
};

static struct usb_interface_association_descriptor uvc_IAD_descriptor = {
	bLength: sizeof(struct usb_interface_association_descriptor), //0x08,
	bDescriptorType: USB_DT_INTERFACE_ASSOCIATION,
	bFirstInterface: 0x00, 
	bInterfaceCount: 0x02, 
	bFunctionClass: 0x0E,		/* CC_VIDEO */
	bFunctionSubClass: 0x03,	/* SC_VIDEO_INTERFACE_COLLECTION */
	bFunctionProtocol: 0x00,	/* PC_PROTOCOL_UNDEFINED */
	iFunction: 0x00, 
};

static struct usb_interface_descriptor uvc_vc_alternate_descriptor0 = {
        bLength: sizeof(struct usb_interface_descriptor), //0x09, 
        bDescriptorType: USB_DT_INTERFACE, 
        bInterfaceNumber: VC_INTERFACE_ID, 
        bAlternateSetting: 0x00,
        bNumEndpoints: 0x00,
        bInterfaceClass: CC_VIDEO, 
        bInterfaceSubClass: SC_VIDEOCONTROL, 
        bInterfaceProtocol: PC_PROTOCOL_UNDEFINED, 
        iInterface: 0x00, //this must be identical with the peer of IAD**************
};


static struct usb_interface_descriptor uvc_vs_alternate_descriptor0 = {
        bLength: sizeof(struct usb_interface_descriptor), //0x09, 
        bDescriptorType: USB_DT_INTERFACE, 
        bInterfaceNumber: VS_INTERFACE_ID, 
        bAlternateSetting: 0x00,
        bNumEndpoints: 0x01,
        bInterfaceClass: CC_VIDEO, 
        bInterfaceSubClass: SC_VIDEOSTREAMING, 
        bInterfaceProtocol: PC_PROTOCOL_UNDEFINED, 
        iInterface: 0x00,
};

static struct usb_interface_descriptor uvc_vs_alternate_descriptor1 = {
        bLength: sizeof(struct usb_interface_descriptor), //0x09, 
        bDescriptorType: USB_DT_INTERFACE, 
        bInterfaceNumber: VS_INTERFACE_ID,
        bAlternateSetting: 0x01,
        bNumEndpoints: 0x01,
        bInterfaceClass: CC_VIDEO, 
        bInterfaceSubClass: SC_VIDEOSTREAMING, 
        bInterfaceProtocol: PC_PROTOCOL_UNDEFINED, 
        iInterface: 0x00,
};

/**vs endpoint**/
static struct usb_endpoint_descriptor uvc_vs_endpoint = { 
	bLength: sizeof(struct usb_endpoint_descriptor), //0x07,
	bDescriptorType: USB_DT_ENDPOINT,
	bEndpointAddress: 0x00, 
	bmAttributes: ISOCHRONOUS | USB_ENDPOINT_ISO_ASYNCH | USB_ENDPOINT_ISO_DATA,
	wMaxPacketSize: 0x0000,
	bInterval: 0x01
 };

static struct usb_endpoint_descriptor *uvc_vs_endpoints[] = {&uvc_vs_endpoint};
u8 uvc_alt_vs_indexes[]={UVC_ISO_IN_ENDPOINT};


static struct usb_alternate_description uvc_vs_alternate_descriptions[] = {
	{ 	iInterface: CONFIG_USBD_UVC_VS_CLASS_INTF,
		interface_descriptor: (struct usb_interface_descriptor *)&uvc_vs_alternate_descriptor0,
		classes: sizeof (uvc_vs_class_descriptors) / sizeof (struct usb_generic_class_descriptor *),
		class_list: uvc_vs_class_descriptors,

/*
* Please pay enough attention to this extra 0-bandwidth Endpoint 
* in Alternate 0 of Interface 1. We add this Endpoint just to help Motech 
* about an enumeration problem of their UVC host driver. 
* Essentially this is an UNNECESSARY Endpoint according to UVC specification.
* 
* It might be a potential bug since this Endpoint has same address
* with the ISO Endpoint in Alternate 1 of Interface 1.
*/
		endpoints: 1,
		endpoint_list: uvc_vs_endpoints,
		endpoint_indexes: uvc_alt_vs_indexes,
	},
	{ 	iInterface: CONFIG_USBD_UVC_VS_STANDARD_INTF,
		interface_descriptor: (struct usb_interface_descriptor *)&uvc_vs_alternate_descriptor1,
		endpoints: 1,
		endpoint_list: uvc_vs_endpoints,
		endpoint_indexes: uvc_alt_vs_indexes,
	},
};


static struct usb_alternate_description uvc_vc_alternate_descriptions[] = {
	{ 	//iInterface: CONFIG_USBD_UVC_VC_INTF,
		interface_descriptor: (struct usb_interface_descriptor *)&uvc_vc_alternate_descriptor0,
		classes:sizeof (uvc_vc_class_descriptors) / sizeof (struct usb_generic_class_descriptor *),
		class_list: uvc_vc_class_descriptors,
		endpoints: 0,
	},
};

/* Interface descriptions and descriptors
 */
static struct usb_interface_description uvc_interfaces[] = {
	{ 
		alternates: 1,
		alternate_list: uvc_vc_alternate_descriptions, 
#ifdef CONFIG_USBD_IAD
		IAD_descriptor: &uvc_IAD_descriptor,
#endif
	},
	{
		alternates: 2,
		alternate_list: uvc_vs_alternate_descriptions,
#ifdef CONFIG_USBD_IAD
		IAD_descriptor: &uvc_IAD_descriptor,
#endif
	}
};


/* Configuration descriptions and descriptors
 */

static struct usb_configuration_descriptor uvc_configuration_descriptor = {
	bLength: sizeof(struct usb_configuration_descriptor), //0x09, 
	bDescriptorType: USB_DT_CONFIG, 
	wTotalLength: 0x0000,  
	bNumInterfaces: sizeof (uvc_interfaces) / sizeof (struct usb_interface_description),
	bConfigurationValue: 0x01, 
	iConfiguration: 0x00, 
	bmAttributes: BMATTRIBUTE, 
	bMaxPower: BMAXPOWER,
};

static struct usb_configuration_description uvc_description[] = {
	{ 
		iConfiguration: "Motorola UVC Configuration",
		configuration_descriptor: (struct usb_configuration_descriptor *)&uvc_configuration_descriptor,
		bNumInterfaces: sizeof (uvc_interfaces) / sizeof (struct usb_interface_description),
		interface_list:uvc_interfaces, 
	},
};

/* Device Description
 */

static struct usb_device_descriptor uvc_device_descriptor = {
	bLength: sizeof(struct usb_device_descriptor),
	bDescriptorType: USB_DT_DEVICE,
	bcdUSB: __constant_cpu_to_le16(USB_BCD_VERSION),
	bDeviceClass: 0xEF,	/* Miscellaneous Device Class */
	bDeviceSubClass: 0x02,	/* Common Class */
	bDeviceProtocol: 0x01,	/* Interface Association Descriptor */
	bMaxPacketSize0: 0x00,
	idVendor: __constant_cpu_to_le16(CONFIG_USBD_UVC_VENDORID),
	idProduct: __constant_cpu_to_le16(CONFIG_USBD_UVC_PRODUCTID),
	bcdDevice: __constant_cpu_to_le16(CONFIG_USBD_UVC_BCDDEVICE),
};

#ifdef CONFIG_USBD_HIGH_SPEED
static struct usb_device_qualifier_descriptor uvc_device_qualifier_descriptor = {
	bLength: sizeof(struct usb_device_qualifier_descriptor),
	bDescriptorType: USB_DT_DEVICE_QUALIFIER,
	bcdUSB: __constant_cpu_to_le16(USB_BCD_VERSION),
	bDeviceClass: 0xEF,
	bDeviceSubClass: 0x02,
	bDeviceProtocol: 0x01,         
	bMaxPacketSize0: 0x00,
	bNumConfigurations: 0x00,
	bReserved: 0x00,
};
#endif

#define UVC_ENDPOINTS      0x01

static struct usb_endpoint_request uvc_endpoint_requests[UVC_ENDPOINTS+1] = {
        { 1, 1, 1, USB_DIR_IN | USB_ENDPOINT_ISOCHRONOUS, 0xFFFF, 0xFFFF, },
        { 0, },
};

static struct usb_device_description uvc_device_description = {
	device_descriptor: &uvc_device_descriptor,
#ifdef CONFIG_USBD_HIGH_SPEED
	device_qualifier_descriptor: &uvc_device_qualifier_descriptor,
#endif /* CONFIG_USBD_HIGH_SPEED */
	iManufacturer: "Motorola Inc.",
	iProduct: CONFIG_USBD_UVC_PRODUCT_NAME,
#if !defined(CONFIG_USBD_NO_SERIAL_NUMBER) && defined(CONFIG_USBD_SERIAL_NUMBER_STR)
	iSerialNumber:CONFIG_USBD_SERIAL_NUMBER_STR,
#endif
	endpointsRequested: UVC_ENDPOINTS,
	requestedEndpoints: uvc_endpoint_requests,
};

/*USB Device Function driver define */
static struct usb_function_operations uvc_function_ops = {
	event_irq:uvc_event_irq,
	recv_setup_irq:uvc_recv_setup_irq,
	function_enable:uvc_function_enable,
	function_disable:uvc_function_disable,
};

static struct usb_function_driver uvc_function_driver = {
	name: "uvc",
	fops: &uvc_function_ops,
	device_description: &uvc_device_description,
	bNumConfigurations: sizeof (uvc_description) / sizeof (struct usb_configuration_description),
	configuration_description: uvc_description,
	idVendor: __constant_cpu_to_le16(CONFIG_USBD_UVC_VENDORID),
	idProduct: __constant_cpu_to_le16(CONFIG_USBD_UVC_PRODUCTID),
	bcdDevice: __constant_cpu_to_le16(CONFIG_USBD_UVC_BCDDEVICE),
};

/*================================================================================
                               GLOBAL FUNCTIONS
================================================================================*/
static int pu_brightness_set_cur(struct urb *urb, int rc)
{
	if(RECV_OK != urb->status) {
		return -EINVAL;
	}
	__u16 value = *(__u16 *)(urb->buffer);

	spin_lock(&uvc_set_cur_op.op_lock);
	uvc_set_cur_op.op_code = SET_CUR_BRIGHTNESS;
	uvc_set_cur_op.cur_brightness = value;
	spin_unlock(&uvc_set_cur_op.op_lock);

	usbd_dealloc_urb(urb);
	return 0;
}
static int pu_brightness_get_cur(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_current(CAMERA_ATTR_BRIGHTNESS, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}

	return ret;
}
static int pu_brightness_get_min(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_min(CAMERA_ATTR_BRIGHTNESS, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}

	return ret;
}
static int pu_brightness_get_max(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_max(CAMERA_ATTR_BRIGHTNESS, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}

	return ret;
}
static int pu_brightness_get_res(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_resolution(CAMERA_ATTR_BRIGHTNESS, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}

	return ret;
}
static int generic_get_info(struct usb_device_request *request, struct urb *urb)
{
	//D0 for supporting Get value requests and D1 for supporting Set value requests
	urb->buffer[0] = 0x03; 
	urb->actual_length = 1;

	return 0;
}

static int pu_brightness_get_info(struct usb_device_request *request, struct urb *urb)
{
	return generic_get_info(request, urb);
}

static int pu_brightness_get_def(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_default(CAMERA_ATTR_BRIGHTNESS, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}

	return ret;
}

static struct uvc_req_operations pu_bightness_ops = {
	set_cur:	pu_brightness_set_cur,
	get_cur:	pu_brightness_get_cur,
	get_min:	pu_brightness_get_min,
	get_max:	pu_brightness_get_max,
	get_res:	pu_brightness_get_res,
	get_info:	pu_brightness_get_info,
	get_def:	pu_brightness_get_def,
};

static int pu_digital_multiplier_set_cur(struct urb *urb, int rc)
{
	if(RECV_OK != urb->status) {
		return -EINVAL;
	}

	__u16 value = *(__u16 *)(urb->buffer);

	spin_lock(&uvc_set_cur_op.op_lock);
	uvc_set_cur_op.op_code = SET_CUR_DIGITAL_MULTIPLIER;
	uvc_set_cur_op.cur_digital_multiplier = value;
	spin_unlock(&uvc_set_cur_op.op_lock);

	usbd_dealloc_urb(urb);
	return 0;
}

static int pu_digital_multiplier_get_cur(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_current(CAMERA_ATTR_ZOOM, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}
	return ret;
}

static int pu_digital_multiplier_get_min(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_min(CAMERA_ATTR_ZOOM, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}
	return ret;
}

static int pu_digital_multiplier_get_max(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_max(CAMERA_ATTR_ZOOM, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}
	return ret;
}

static int pu_digital_multiplier_get_res(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_resolution(CAMERA_ATTR_ZOOM, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}
	return ret;
}

static int pu_digital_multiplier_get_info(struct usb_device_request *request, struct urb *urb)
{
	return generic_get_info(request, urb);
}

static int pu_digital_multiplier_get_def(struct usb_device_request *request, struct urb *urb)
{
	__u16 value;
	int ret = -1;

	ret = camera4uvc_get_default(CAMERA_ATTR_ZOOM, &value);
	if (!ret) {
		urb->actual_length = 2;
		*((__u16 *)urb->buffer) = value;
	}
	return ret;
}

static struct uvc_req_operations pu_digital_multiplier_ops = {
	set_cur:	pu_digital_multiplier_set_cur,
	get_cur:	pu_digital_multiplier_get_cur,
	get_min:	pu_digital_multiplier_get_min,
	get_max:	pu_digital_multiplier_get_max,
	get_res:	pu_digital_multiplier_get_res,
	get_info:	pu_digital_multiplier_get_info,
	get_def:	pu_digital_multiplier_get_def,
};

#ifdef CAMERA_TERMINAL_ZOOM
static struct uvc_req_operations ct_zoom_absolute_ops = {
	set_cur:	pu_digital_multiplier_set_cur,
	get_cur:	pu_digital_multiplier_get_cur,
	get_min:	pu_digital_multiplier_get_min,
	get_max:	pu_digital_multiplier_get_max,
	get_res:	pu_digital_multiplier_get_res,
	get_info:	pu_digital_multiplier_get_info,
	get_def:	pu_digital_multiplier_get_def,
};
#endif

static int su_control_set_cur(struct urb *urb, int rc)
{
	if(RECV_OK != urb->status) {
		return -EINVAL;
	}

	__u8 bSelector = *(__u8 *)(urb->buffer);

	if ((bSelector != ID_CT1) && (bSelector != ID_IT1)) {
		return -EINVAL;
	}

	cur_input_terminal = bSelector;

	usbd_dealloc_urb(urb);
	return 0;
}

static int su_control_get_cur(struct usb_device_request *request, struct urb *urb)
{
	urb->buffer[0] = cur_input_terminal;
	urb->actual_length = 1;

	return 0;
}

static int su_control_get_min(struct usb_device_request *request, struct urb *urb)
{
	urb->buffer[0] = ID_CT1;
	urb->actual_length = 1;

	return 0;
}

static int su_control_get_max(struct usb_device_request *request, struct urb *urb)
{
	urb->buffer[0] = ID_IT1;
	urb->actual_length = 1;

	return 0;
}

static int su_control_get_res(struct usb_device_request *request, struct urb *urb)
{
	urb->buffer[0] = 1;
	urb->actual_length = 1;

	return 0;
}

static int su_control_get_info(struct usb_device_request *request, struct urb *urb)
{
	return generic_get_info(request, urb);
}

static struct uvc_req_operations su_control_ops = {
	set_cur:	su_control_set_cur,
	get_cur:	su_control_get_cur,
	get_min:	su_control_get_min,
	get_max:	su_control_get_max,
	get_res:	su_control_get_res,
	get_info:	su_control_get_info,
};

static int req_error_code_get_cur(struct usb_device_request *request, struct urb *urb)
{
	//0x00 for No error
	urb->buffer[0] = err_code;
	urb->actual_length = 1;
	err_code = REQUEST_NO_ERROR;

	return 0;
}

static int req_error_code_get_info(struct usb_device_request *request, struct urb *urb)
{
	int rc = -EINVAL;
	rc = generic_get_info(request, urb);
	if (!rc)
		urb->buffer[0] = 0x01;	//only support GET	
	return rc;
}

static struct uvc_req_operations req_error_code_ops = {
	get_cur:	req_error_code_get_cur,
	get_info:	req_error_code_get_info,
};

static struct probe_commit_data  vs_frames[5] = {
	//CIF
	{
		bmHint: 0x0000,
		bFormatIndex: 0x01,
		bFrameIndex: 0x01,
		dwFrameInterval: 0x0007A120,
		wKeyFrameRate: 0x0000,
		wPFrameRate: 0x0000,
		wCompQuality: 0x0000,
		wCompWindowSize: 0x0000,
		wDelay: 0x0000,
		dwMaxVideoFrameSize: 0x00096000,	//0x00031800,	//352*288*2
		dwMaxPayloadTransferSize: 0x00000000,     //0x00000C00,	//3072
//		dwClockFrequency: 0x018CBA80,	//26MHZ
//		bmFramingInfo: 0x00,
//		bPreferedVersion: 0x00,
//		bMinVersion: 0x00,
//		bMaxVersion: 0x00,
	},
	//QCIF
	{
		bmHint: 0x0000,
		bFormatIndex: 0x01,
		bFrameIndex: 0x02,
		dwFrameInterval: 0x0007A120,	//FrameInterval
		wKeyFrameRate: 0x0000,
		wPFrameRate: 0x0000,
		wCompQuality: 0x0000,
		wCompWindowSize: 0x0000,
		wDelay: 0x0000,		//wDelay 0?
		dwMaxVideoFrameSize: 0x00096000,	//0x0000C600,	//176*144*2
		dwMaxPayloadTransferSize: 0x00000000,	//0x00000C00,	//3072
//		dwClockFrequency: 0x018CBA80,	//26MHZ
//		bmFramingInfo: 0x00,
//		bPreferedVersion: 0x00,
//		bMinVersion: 0x00,
//		bMaxVersion: 0x00,
	},
	//VGA
	{
		bmHint: 0x0000,
		bFormatIndex: 0x01,
		bFrameIndex: 0x03,
		dwFrameInterval: 0x000A2C2A,	//FrameInterval
		wKeyFrameRate: 0x0000,
		wPFrameRate: 0x0000,
		wCompQuality: 0x0000,
		wCompWindowSize: 0x0000,
		wDelay: 0x0000,		//wDelay 0?
		dwMaxVideoFrameSize: 0x00096000,	//640*480*2
		dwMaxPayloadTransferSize: 0x00000000, 	//0x00000C00,	//3072
//		dwClockFrequency: 0x018CBA80,	//26MHZ
//		bmFramingInfo: 0x00,
//		bPreferedVersion: 0x00,
//		bMinVersion: 0x00,
//		bMaxVersion: 0x00,
	},
	//QVGA
	{
		bmHint: 0x0000,
		bFormatIndex: 0x01,
		bFrameIndex: 0x04,
		dwFrameInterval: 0x0007A120,	//FrameInterval
		wKeyFrameRate: 0x0000,
		wPFrameRate: 0x0000,
		wCompQuality: 0x0000,
		wCompWindowSize: 0x0000,
		wDelay: 0x0000,		//wDelay 0?
		dwMaxVideoFrameSize: 0x00096000,	//0x00025800,	//320*240*2
		dwMaxPayloadTransferSize: 0x00000000,	//0x00000C00,	//3072
//		dwClockFrequency: 0x018CBA80,	//26MHZ
//		bmFramingInfo: 0x00,
//		bPreferedVersion: 0x00,
//		bMinVersion: 0x00,
//		bMaxVersion: 0x00,
	},
	//sQVGA
	{
		bmHint: 0x0000,
		bFormatIndex: 0x01,
		bFrameIndex: 0x05,
		dwFrameInterval: 0x0007A120,	//FrameInterval
		wKeyFrameRate: 0x0000,
		wPFrameRate: 0x0000,
		wCompQuality: 0x0000,
		wCompWindowSize: 0x0000,
		wDelay: 0x0000,		//wDelay 0?
		dwMaxVideoFrameSize: 0x00096000,	//0x00009600,	//160*120*2
		dwMaxPayloadTransferSize: 0x00000000,	//0x00000C00,	//3072
//		dwClockFrequency: 0x018CBA80,	//26MHZ
//		bmFramingInfo: 0x00,
//		bPreferedVersion: 0x00,
//		bMinVersion: 0x00,
//		bMaxVersion: 0x00,
	},
};

int vs_probe_set_cur(struct urb *urb, int rc)
{
	int i;
	int which = -1;
	struct probe_commit_data *temp_vs_frame;

	if(RECV_OK != urb->status) {
		return -EINVAL;
	}

	temp_vs_frame = (struct probe_commit_data *)(urb->buffer);

	for (i = sizeof(vs_frames)/sizeof(struct probe_commit_data); i > 0; i--) {
		if ((temp_vs_frame->bFormatIndex != vs_frames[i - 1].bFormatIndex) || \
			(temp_vs_frame->bFrameIndex != vs_frames[i - 1].bFrameIndex))
			continue;
		else {
			which = i - 1;
			break;
		}
	}

	dbg_printk("%s:which=%d\n", __FUNCTION__, which);
	if (which != -1) {
		probe_state = &vs_frames[which];
		usbd_dealloc_urb(urb);
		return 0;
	}

	return -EINVAL;
}

int vs_probe_get_cur(struct usb_device_request *request, struct urb *urb)
{
	memcpy (urb->buffer, probe_state, sizeof(struct probe_commit_data));
	urb->actual_length = sizeof(struct probe_commit_data);

	return 0;
}

int vs_probe_get_min(struct usb_device_request *request, struct urb *urb)
{
	memcpy (urb->buffer, &vs_frames[4], sizeof(struct probe_commit_data));
	urb->actual_length = sizeof(struct probe_commit_data);
	
	return 0;
}

int vs_probe_get_max(struct usb_device_request *request, struct urb *urb)
{
	memcpy (urb->buffer, &vs_frames[2], sizeof(struct probe_commit_data));
	urb->actual_length = sizeof(struct probe_commit_data);
	
	return 0;
}

int vs_probe_get_res(struct usb_device_request *request, struct urb *urb)
{
	return -EINVAL;
}

int vs_probe_get_len(struct usb_device_request *request, struct urb *urb)
{
	*((__u16 *)(urb->buffer)) = sizeof(struct probe_commit_data);
	urb->actual_length  = 2;

	return 0;
}

int vs_probe_get_info(struct usb_device_request *request, struct urb *urb)
{
	return generic_get_info(request, urb);
}

int vs_probe_get_def(struct usb_device_request *request, struct urb *urb)
{
	memcpy (urb->buffer, &vs_frames[0], sizeof(struct probe_commit_data));
	urb->actual_length = sizeof(struct probe_commit_data);
	
	return 0;
}

struct uvc_req_operations vs_probe_ops = {
	set_cur:	vs_probe_set_cur,
	get_cur:	vs_probe_get_cur,
	get_min:	vs_probe_get_min,
	get_max:	vs_probe_get_max,
	get_res:	vs_probe_get_res,
	get_len:	vs_probe_get_len,
	get_info:	vs_probe_get_info,
	get_def:	vs_probe_get_def,
};

int vs_commit_set_cur(struct urb *urb, int rc)
{
	int i;
	__u16 width, height;
	int ret = -1;
	int which = -1;
	struct probe_commit_data *temp_vs_frame;
	
	if(RECV_OK != urb->status) {
		return -EINVAL;
	}

	temp_vs_frame = (struct probe_commit_data *)(urb->buffer);

	for (i = sizeof(vs_frames)/sizeof(struct probe_commit_data); i > 0; i--) {
		if (memcmp(temp_vs_frame,  &vs_frames[i - 1], sizeof(struct probe_commit_data)) != 0)
			continue;
		else {
			which = i - 1;
			break;
		}
	}
	dbg_printk("%s:which=%d\n", __FUNCTION__, which);
	if (which == -1) {
		return -EINVAL;
	}
	
	switch (which) {
	case FORMAT_QCIF:
		width  = 176;
		height = 144;
	break;
	case FORMAT_VGA:
		width  = 640;
		height = 480;
	break;
	case FORMAT_QVGA:
		width  = 320;
		height = 240;
	break;
	case FORMAT_SQVGA:
		width  = 160;
		height = 120;
	break;
	case FORMAT_CIF:
	default:
		which  = FORMAT_CIF;
		width  = 352;
		height = 288;
	break;
	}

	spin_lock(&uvc_set_cur_op.op_lock);
	uvc_set_cur_op.op_code = SET_CUR_FRAME;
	uvc_set_cur_op.cur_frame.index = which;
	uvc_set_cur_op.cur_frame.width = width;
	uvc_set_cur_op.cur_frame.height = height;
	spin_unlock(&uvc_set_cur_op.op_lock);

	dbg_printk("%s:ret=%d width=%d height=%d\n", __FUNCTION__, ret, width, height);
	cur_vs_frame =  &vs_frames[which];

	usbd_dealloc_urb(urb);
	return 0;
}

int vs_commit_get_cur(struct usb_device_request *request, struct urb *urb)
{
	__u16 width, height;
	int ret = -1;
	
	ret = camera4uvc_get_frame_size (&width, &height);
	if (ret)
		return -EINVAL;

	if (width == 352 && height == 288)
		cur_vs_frame = &vs_frames[FORMAT_CIF];
	else if (width == 176 && height == 144)
		cur_vs_frame = &vs_frames[FORMAT_QCIF];
	else if (width == 640 && height == 480)
		cur_vs_frame = &vs_frames[FORMAT_VGA];
	else if (width == 320 && height == 240)
		cur_vs_frame = &vs_frames[FORMAT_QVGA];
	else if (width == 160 && height == 120)
		cur_vs_frame = &vs_frames[FORMAT_SQVGA];
	else
		 cur_vs_frame = &vs_frames[FORMAT_CIF];

	memcpy (urb->buffer, cur_vs_frame, sizeof(struct probe_commit_data));
	urb->actual_length = sizeof(struct probe_commit_data);

	return 0;
}

int vs_commit_get_len(struct usb_device_request *request, struct urb *urb)
{
	*((int *)(urb->buffer)) = sizeof(struct probe_commit_data);
	urb->actual_length  = 2;

	return 0;
}

int vs_commit_get_info(struct usb_device_request *request, struct urb *urb)
{
	return generic_get_info(request, urb);
}

struct uvc_req_operations vs_commit_ops = {
	set_cur:	vs_commit_set_cur,
	get_cur:	vs_commit_get_cur,
	get_len:	vs_commit_get_len,
	get_info:	vs_commit_get_info,
};


static void uvc_set_thread_state(uvc_thread_state_t state)
{
	unsigned long flags;

	spin_lock_irqsave(&uvc_state.thread_state_lock, flags);
	uvc_state.thread_state = state;
	spin_unlock_irqrestore(&uvc_state.thread_state_lock, flags);
}

static int uvc_thread_state_is(uvc_thread_state_t state)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&uvc_state.thread_state_lock, flags);
	if (uvc_state.thread_state == state)
		ret = 1;
	else 
		ret = 0;
	spin_unlock_irqrestore(&uvc_state.thread_state_lock, flags);
	return ret;
}

static int uvc_alloc_urb_buffer(struct usb_function_instance *function)
{
	uvc_iso_in_urb = usbd_alloc_urb (function, UVC_ISO_IN_ENDPOINT, 0, uvc_urb_sent);
	if(uvc_iso_in_urb)
	{
		uvc_iso_in_urb->buffer = NULL;
		uvc_iso_in_urb->dma_physaddr = 0;
		uvc_iso_in_urb->buffer_length = 0;
		uvc_iso_in_urb->request_length = 0;
		uvc_iso_in_urb->actual_length = 0;
		uvc_iso_in_urb->dma_mode = 1;	/* ATTENTION */
		
#ifdef PSEUDO_DATA
		uvc_debug_video_frame_buf = (unsigned char *)get_free_page(GFP_KERNEL);
		
		if(!(uvc_debug_video_frame_buf))
			return -ENOMEM;

#else /* #ifdef PSEUDO_DATA */
		memset(&uvc_video_frame_buffer, 0, sizeof(camera4uvc_buffer_t));
#endif	/* #ifdef PSEUDO_DATA */
		return 0;
	}

	return -ENOMEM;
}

static void uvc_free_urb_buffer(void)
{
	if(uvc_iso_in_urb) {
		uvc_iso_in_urb->buffer = NULL;  /* MUST go before uvc_free_urb_buffer()! */
		uvc_iso_in_urb->dma_physaddr = 0;
		usbd_dealloc_urb(uvc_iso_in_urb);
		uvc_iso_in_urb = NULL;
	}

#ifdef PSEUDO_DATA
	free_page((unsigned long)uvc_debug_video_frame_buf);
#endif	
}

static int uvc_urb_sent (struct urb *urb, int rc)
{
	if(uvc_thread_state_is(THREAD_RUNNING)) {
		//dbg_printk("%s: up\n", __FUNCTION__);
		up(&uvc_thread_sem);
	}
        return 0;
}

#ifdef PSEUDO_DATA
#include "blue.h"
#include "red.h"
static int uvc_kernel_thread(void *data)
{       
	u32 i = 0;
	__u8 value = 0;
	unsigned long remain = 0;
	int cur_size = 0;
	char *ptr = my_picture_blue;
	int flag = 0;
	unsigned long start_time = 0, end_time = 0;
	 
	daemonize();
	reparent_to_init();
	spin_lock_irq(&current->sigmask_lock);
	sigemptyset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	dbg_printk("%s:waiting...\n", __FUNCTION__);
	down_interruptible(&uvc_thread_sem);
	dbg_printk("%s:running...\n", __FUNCTION__);
	uvc_set_thread_state(THREAD_IDLE);
	uvc_iso_in_urb = NULL;

	if(uvc_alloc_urb_buffer(uvc_function_instance))
	{
		uvc_set_thread_state(THREAD_STOP);
		dbg_printk("\n%s: failed to allocate urb buffer", __FUNCTION__);
		goto end;
	}
        
	uvc_set_thread_state(THREAD_RUNNING);
	while(uvc_thread_state_is(THREAD_RUNNING)) {
		value++;
		if (value >= 0xFE)
			value=0;
		*(uvc_debug_video_frame_buf + 0) = payload_header_length;
		if (value%2) 
			*(uvc_debug_video_frame_buf + 1) = 0x00;
		else 
			*(uvc_debug_video_frame_buf + 1) = 0x01;

		remain = dumb_height*dumb_width*2;
		if((value%20)==0) {
			if (flag == 0) {
				flag = 1;
				start_time = jiffies;
			}
			else if (flag == 1) {
				flag = 0;
				end_time = jiffies;
				dbg_printk("start=%u end=%u HZ=%u interval=%u(%u)-%u\n", 
					start_time, end_time, HZ, end_time - start_time, (end_time-start_time)/HZ, 
					HZ*20*remain/(end_time-start_time));
			}
		}
		if (flag == 1) {
			ptr = my_picture_red;
		}
		else if (flag == 0) {
			ptr = my_picture_blue;
		}

        	while(remain) {
        		if(uvc_debug_video_frame_buf) {
				cur_size = (remain>payload_data_length)?payload_data_length:remain;
				if (remain <= payload_data_length)
					*(uvc_debug_video_frame_buf + 1) |= 0x02;
				
				memcpy(uvc_debug_video_frame_buf + 2, ptr, cur_size);
				ptr += cur_size;
				remain -= cur_size;
				uvc_iso_in_urb->buffer = uvc_debug_video_frame_buf;
				uvc_iso_in_urb->dma_physaddr = (unsigned long)uvc_iso_in_urb->buffer;
				
				uvc_iso_in_urb->buffer_length = payload_transfer_length;
				uvc_iso_in_urb->request_length = payload_transfer_length;
				uvc_iso_in_urb->actual_length = cur_size + payload_header_length;
				if(usbd_send_urb(uvc_iso_in_urb)) {
					uvc_set_thread_state(THREAD_STOP);
	        			dbg_printk("\n\n************\nusbd_send_urb failed!\n*************\n\n");	
					goto end;
				}					
				down_interruptible(&uvc_thread_sem);
        		}
		}	
	}
        
end:
	dbg_printk("\n%s: exiting", __FUNCTION__);
	if(uvc_iso_in_urb)
	{
		uvc_iso_in_urb->buffer = NULL;	/* MUST go before uvc_free_urb_buffer()! */
		uvc_iso_in_urb->dma_physaddr = 0;
		uvc_free_urb_buffer();
	}	
	complete_and_exit (NULL, 0);

	return 0;
}
#else /* #ifdef PSEUDO_DATA*/

//#define UVC_PERFORMANCE_TEST

#ifdef UVC_PERFORMANCE_TEST
#define UVC_PERFORMANCE_TEST_NUM	100
static struct timeval video_frame_start_tv;
static struct timeval video_frame_end_tv;
static long video_frame_tv[UVC_PERFORMANCE_TEST_NUM];
static int video_frame_num = 0;
#endif /* #ifdef UVC_PERFORMANCE_TEST */

static int uvc_kernel_thread(void *data)
{       
	u32 current_payload_transfer_len = 0;
	u32 i = 0, ret = 0;
	u8 video_frame_id = 0x01;
	u32 skip_flag = 0;

	daemonize();
	reparent_to_init();
	spin_lock_irq(&current->sigmask_lock);
	sigemptyset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	uvc_set_thread_state(THREAD_IDLE);
	uvc_iso_in_urb = NULL;

	if(uvc_alloc_urb_buffer(uvc_function_instance)) {
		uvc_set_thread_state(THREAD_STOP);
		dbg_printk("\n%s: failed to allocate urb buffer", __FUNCTION__);
		goto alloc_error;
	}

	while(uvc_thread_state_is(THREAD_IDLE) || uvc_thread_state_is(THREAD_INIT)) {
		init_MUTEX_LOCKED(&uvc_thread_sem);
	
		if(uvc_thread_state_is(THREAD_IDLE)) {
			/* wait for the DEVICE_CONFIGURED event */
			dbg_printk ("%s: waiting DEVICE_CONFIGURED\n", __FUNCTION__);
			if(down_interruptible(&uvc_thread_sem)) {
				uvc_set_thread_state(THREAD_STOP);
				break;
			}
			if(!uvc_thread_state_is(THREAD_INIT)) {
				uvc_set_thread_state(THREAD_STOP);
				break;
			}
			else {
				cam_ipm_hook();		
			}

			ret = camera4uvc_open (payload_header_length, payload_data_length);
			if (!ret) {
				ret = camera4uvc_startcapture();
				if(!ret) {
					queue_motusbd_event(USB_UVC_READY);
					dbg_printk ("\n%s: camera ready!\n payload_header_length = %d, payload_data_length = %d, bmAttributes = %0x, bMaxPower = %0x", __FUNCTION__, payload_header_length, payload_data_length, BMATTRIBUTE, BMAXPOWER);
				}
				else {
					dbg_printk ("\n%s: failed to start camera", __FUNCTION__);
					uvc_set_thread_state(THREAD_IDLE);
					goto start_error;
				} /* if(!ret) */
			}
			else {
				if(ret == -EBUSY) {
					queue_motusbd_event(USB_UVC_CAMERA_BUSY);
				}
				dbg_printk ("\n%s: failed to open camera", __FUNCTION__);
				uvc_set_thread_state(THREAD_IDLE);
				goto open_error;
			} /* if(!ret) */
		} /* if(uvc_thread_state_is(THREAD_IDLE)) */

		/* wait for the DEVICE_SET_INTERFACE event */
		dbg_printk ("%s: waiting SET_INTERFACE\n", __FUNCTION__);
		if(down_interruptible(&uvc_thread_sem)) {
			uvc_set_thread_state(THREAD_STOP);
		}

		while(uvc_thread_state_is(THREAD_RUNNING)) {
			spin_lock(&uvc_set_cur_op.op_lock);
			if(uvc_set_cur_op.op_code != SET_CUR_NONE) {
				camera4uvc_stopcapture();
				
				switch(uvc_set_cur_op.op_code) {
				case SET_CUR_BRIGHTNESS:
					ret = camera4uvc_set_current(CAMERA_ATTR_BRIGHTNESS, uvc_set_cur_op.cur_brightness);
					if(ret) {
						dbg_printk("\n%s: failed to set brightness", __FUNCTION__);
					}
					break;
				case SET_CUR_DIGITAL_MULTIPLIER:
					ret = camera4uvc_set_current(CAMERA_ATTR_ZOOM, uvc_set_cur_op.cur_digital_multiplier);
					if(ret) {
						dbg_printk("\n%s: failed to set digital multiplier", __FUNCTION__);
					}
					break;
				case SET_CUR_FRAME:
					ret = camera4uvc_set_frame_size (uvc_set_cur_op.cur_frame.width, uvc_set_cur_op.cur_frame.height);
					if(ret) {
						dbg_printk("\n%s: failed to set frame", __FUNCTION__);
					}
					skip_flag = UVC_FRAME_SKIP_NUM;
					break;
				default:
					dbg_printk("\n%s: unsupported operation", __FUNCTION__);
					break;
				}
				uvc_set_cur_op.op_code = SET_CUR_NONE;

				ret = camera4uvc_startcapture();
				if(ret) {
					dbg_printk("\n%s: failed to start camera", __FUNCTION__);
					spin_unlock(&uvc_set_cur_op.op_lock);
					uvc_set_thread_state(THREAD_IDLE);
					goto start_error;
				}
			}
			spin_unlock(&uvc_set_cur_op.op_lock);
			
#ifdef UVC_PERFORMANCE_TEST
			do_gettimeofday(&video_frame_start_tv);
#endif

#ifdef UVC_DEMO_RGB
			if(camera4uvc_get_current_frame_demo(&uvc_video_frame_buffer)) 
			{
				dbg_printk ("\n%s: camera error", __FUNCTION__);
				//uvc_set_thread_state(THREAD_STOP);
				uvc_set_thread_state(THREAD_IDLE);
				break;
			}
#else
			for(; skip_flag > 0; skip_flag--) {
				if(camera4uvc_get_current_frame(&uvc_video_frame_buffer)) 
				{
					dbg_printk ("\n%s: camera error", __FUNCTION__);
					//uvc_set_thread_state(THREAD_STOP);
					//uvc_set_thread_state(THREAD_IDLE);
					break;
				}
				camera4uvc_release_current_frame();
			}

			if(camera4uvc_get_current_frame(&uvc_video_frame_buffer)) 
			{
				dbg_printk ("\n%s: camera error", __FUNCTION__);
				//uvc_set_thread_state(THREAD_STOP);
				uvc_set_thread_state(THREAD_IDLE);
				break;
			}
#endif /* #ifdef UVC_DEMO_RGB */
			
			video_frame_id ^= 0x01;

			for(i = 0; i < uvc_video_frame_buffer.y_count; i++)
			{
				if(uvc_video_frame_buffer.y_array && uvc_video_frame_buffer.y_array[i]) 
				{
					*((unsigned char *)uvc_video_frame_buffer.y_array[i] + 0) = (unsigned char)payload_header_length;
					*((unsigned char *)uvc_video_frame_buffer.y_array[i] + 1) = video_frame_id;

					if(i == (uvc_video_frame_buffer.y_count - 1))
					{
						/* last payload transfer */
						current_payload_transfer_len = uvc_video_frame_buffer.y_last_size;
						*((unsigned char *)uvc_video_frame_buffer.y_array[i] + 1) |= 0x02;
					}
					else
					{
						current_payload_transfer_len = payload_transfer_length;
					}
					
					/*==============================================*/
					memset((unsigned char *)uvc_video_frame_buffer.y_array[i] + 2, 0, payload_header_length - 2);
					/*==============================================*/
					
					uvc_iso_in_urb->buffer = (unsigned char *)uvc_video_frame_buffer.y_array[i];
					uvc_iso_in_urb->dma_physaddr = (unsigned long)uvc_iso_in_urb->buffer;
					uvc_iso_in_urb->buffer_length = current_payload_transfer_len;
					uvc_iso_in_urb->request_length = current_payload_transfer_len;
					uvc_iso_in_urb->actual_length = current_payload_transfer_len;
					if(usbd_send_urb(uvc_iso_in_urb)) {
						dbg_printk("\n******\nusbd_send_urb failed!\n*******\n");	
						//uvc_set_thread_state(THREAD_STOP);
						uvc_set_thread_state(THREAD_IDLE);
						break;
					}					
					if(down_interruptible(&uvc_thread_sem)) {
						uvc_set_thread_state(THREAD_STOP);
						break;
					}
					if(!uvc_thread_state_is(THREAD_RUNNING)) {
						usbd_flush_endpoint_index(uvc_function_instance, UVC_ISO_IN_ENDPOINT);
						break;
					}
				} /* if(uvc_video_frame_buffer.y_array && uvc_video_frame_buffer.y_array[i]) */
				else
				{
					dbg_printk("\n%s: null payload", __FUNCTION__);	
				} /* if(uvc_video_frame_buffer.y_array && uvc_video_frame_buffer.y_array[i]) */
			} /* for(i = 0; i < uvc_video_frame_buffer.y_count; i++) */	
#ifndef UVC_DEMO_RGB
			camera4uvc_release_current_frame();
#endif

#ifdef UVC_PERFORMANCE_TEST
			do_gettimeofday(&video_frame_end_tv);
			video_frame_tv[video_frame_num++] = video_frame_end_tv.tv_usec - video_frame_start_tv.tv_usec;
			if(video_frame_num == UVC_PERFORMANCE_TEST_NUM)
			{
				dbg_printk("\nperformance:\n");
				for(video_frame_num = 0; video_frame_num < UVC_PERFORMANCE_TEST_NUM; video_frame_num++)
				{
					dbg_printk("   %ld\n", video_frame_tv[video_frame_num]);
				}
					
				video_frame_num = 0;
			}
#endif

		} /* while(RUNNING) */

		/* Set Alternate 0 event */
		if(uvc_thread_state_is(THREAD_INIT)) continue;
		
		if(!uvc_thread_state_is(THREAD_IDLE)) {
			uvc_set_thread_state(THREAD_STOP);
		}
		camera4uvc_stopcapture();
start_error:
		camera4uvc_close();
open_error:
		cam_ipm_unhook();		
	} /* while(uvc_thread_state_is(THREAD_IDLE) || uvc_thread_state_is(THREAD_INIT)) */
	uvc_free_urb_buffer();
alloc_error:
	dbg_printk("\n%s: exiting", __FUNCTION__);
	complete_and_exit (NULL, 0);
	return 0;
}
#endif	/* #ifdef PSEUDO_DATA*/

static int uvc_kernel_thread_kickoff(void)
{
	if(uvc_thread_state_is(THREAD_STOP)) {
		kernel_thread(&uvc_kernel_thread, NULL, CLONE_FS | CLONE_FILES);
        	return 0;
	}
	else {
		dbg_printk("\n%s: uvc thread state error", __FUNCTION__);
		uvc_set_thread_state(THREAD_STOP);
		return -EINVAL;
	}
}
	
static void uvc_kernel_thread_killoff(void)
{
	if(!uvc_thread_state_is(THREAD_STOP)) {
		uvc_set_thread_state(THREAD_STOP);
		dbg_printk("%s: up\n", __FUNCTION__);
		up(&uvc_thread_sem);
	}
}	

/*
 * uvc_event - process a device event
 * @device: usb device 
 * @event: the event that happened
 *
 * Called by the usb device core layer to respond to various USB events.
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 */

static void uvc_event_irq (struct usb_function_instance *function, usb_device_event_t event, int data)
{
	int highspeed = -1;	
	int alternate = -1;
	
	switch (event) {
	case DEVICE_CREATE:	
		break;

	case DEVICE_CONFIGURED:
		highspeed = usbd_high_speed(function);
		if (highspeed == 1) {
			payload_transfer_length = ISO_HS_PAYLOAD_TRANSFER_LENGTH;
			payload_header_length = ISO_HS_PAYLOAD_HEADER_LENGTH;
			payload_data_length = ISO_HS_PAYLOAD_DATA_LENGTH;
		}
		else {
			payload_transfer_length = ISO_FS_PAYLOAD_TRANSFER_LENGTH;
			payload_header_length = ISO_FS_PAYLOAD_HEADER_LENGTH;
			payload_data_length = ISO_FS_PAYLOAD_DATA_LENGTH;
		}
	
		dbg_printk("HighSpeedFlag = %d\n", highspeed);	
		dbg_printk("DEVICE_CONFIGURED.");
		if(uvc_thread_state_is(THREAD_IDLE)) {
			uvc_set_thread_state(THREAD_INIT);
			dbg_printk("%s: up\n", __FUNCTION__);
			up(&uvc_thread_sem);
		}
		break;
		
	case DEVICE_SET_INTERFACE:
		alternate = usbd_interface_AltSetting(function, VS_INTERFACE_ID);
		dbg_printk("DEVICE_SET_INTERFACE alternate=%d.\n", alternate);
		if (alternate == 1) {
			if(uvc_thread_state_is(THREAD_INIT)) {
				uvc_set_thread_state(THREAD_RUNNING);
				dbg_printk("%s: up\n", __FUNCTION__);
				up(&uvc_thread_sem);
			}
		}
		else if(alternate == 0) {
			if(uvc_thread_state_is(THREAD_RUNNING)) {
				uvc_set_thread_state(THREAD_INIT);
				up(&uvc_thread_sem);
			}
		}
		break;	

	case DEVICE_DE_CONFIGURED:
		dbg_printk("DEVICE_DE_CONFIGURED.");
		break;	

	case DEVICE_BUS_INACTIVE:
		dbg_printk("DEVICE_BUS_INACTIVE.");
		if(!uvc_thread_state_is(THREAD_IDLE) && !uvc_thread_state_is(THREAD_STOP)) {
			uvc_set_thread_state(THREAD_IDLE);
			dbg_printk("%s: up\n", __FUNCTION__);
			up(&uvc_thread_sem);
		}
		break;	
		
        default:
		break;
	}

	return;
}

/**
 * uvc_recv_setup_irq - called with a control URB 
 * @urb - pointer to struct urb
 *
 * Check if this is a setup packet, process the device request, put results
 * back into the urb and return zero or non-zero to indicate success (DATA)
 * or failure (STALL).
 *
 * This routine IS called at interrupt time. Please use the usual precautions.
 */
static int uvc_recv_setup_irq (struct usb_device_request *request)
{
	struct usb_function_instance *function = uvc_function_instance;
        struct urb *urb;
	__u16 length_checked = 0;
	struct uvc_req_operations *req_op = NULL;
        int rc = 0;
	__u8 wValue = (__u8)(request->wValue >> 8);	

	dbg_printk("%s: bmRequestType:%02x bRequest:%02x wValue:%04x wIndex:%04x wLength:%04x\n", 
		__FUNCTION__,
		request->bmRequestType, 
		request->bRequest, 
		le16_to_cpu(request->wValue), 
		le16_to_cpu(request->wIndex), 
		le16_to_cpu(request->wLength));
	//return -EINVAL if request type don't equal class-specific request.
	if ((request->bmRequestType & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_CLASS) {
		err_code = REQUEST_INVALID_REQUEST;
		return -EINVAL;
	}

	if ( (request->bRequest != SET_CUR)
		&&  (request->bRequest != GET_CUR)
		&&  (request->bRequest != GET_MIN)
		&&  (request->bRequest != GET_MAX)
		&&  (request->bRequest != GET_RES)
		&&  (request->bRequest != GET_LEN)
		&&  (request->bRequest != GET_INFO)
		&&  (request->bRequest != GET_DEF)  ) 
	{
		err_code = REQUEST_INVALID_REQUEST;
		return -EINVAL;
	}
	
	// handle all [GET] requests that return data (direction bit set on bm RequestType)
	if ((request->bmRequestType & USB_REQ_DIRECTION_MASK)) {
		switch (request->bmRequestType & USB_REQ_RECIPIENT_MASK) {
		case USB_REQ_RECIPIENT_INTERFACE:
			switch (request->wIndex) {
			case (VC_INTERFACE_ID):
				if (wValue == VC_REQUEST_ERROR_CODE_CONTROL) {
					req_op = &req_error_code_ops;
					length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
				
			case ((ID_SU1 << 8) | VC_INTERFACE_ID):
				if (wValue == SU_INPUT_SELECT_CONTROL) {
					req_op = &su_control_ops;
					length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
					
			case ((ID_PU1 << 8) | VC_INTERFACE_ID):
				if (wValue > PU_CONTROL_UNDEFINED && wValue < PU_CONTROL_MAX_UNDEFINED) {
					req_op = pu_control_ops[wValue];
					length_checked = 2;
					if (request->bRequest == GET_INFO)
						length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
		
#ifdef CAMERA_TERMINAL_ZOOM		
			case ((ID_CT1 << 8) | VC_INTERFACE_ID):
				if (wValue > CT_CONTROL_UNDEFINED && wValue < CT_CONTROL_MAX_UNDEFINED) {
					req_op = ct_control_ops[wValue];
					length_checked = 2;
					if (request->bRequest == GET_INFO)
						length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
#endif
				
			case (VS_INTERFACE_ID):
				if (wValue > VS_CONTROL_UNDEFINED && wValue < VS_CONTROL_MAX_UNDEFINED) {
					req_op = vs_control_ops[wValue];
					length_checked = sizeof(struct probe_commit_data);
                                        if (request->bRequest == GET_INFO)
                                                length_checked = 1;
					else if (request->bRequest == GET_LEN)
                                                length_checked = 2;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
				
			default: 
				err_code = REQUEST_INVALID_UNIT;
				return -EINVAL;
			} /* switch (request->wIndex) */
			break;
		case USB_REQ_RECIPIENT_ENDPOINT:
		default:
			err_code = REQUEST_INVALID_UNIT;
			return -EINVAL;	
		} /* switch (request->bmRequestType & USB_REQ_RECIPIENT_MASK) */

		if(!req_op) {
			err_code = REQUEST_INVALID_CONTROL;
			return -EINVAL;	
		}

		if (!le16_to_cpu(request->wLength) || le16_to_cpu(request->wLength) != length_checked) {
			err_code = REQUEST_INVALID_CONTROL;
			return -EINVAL;	
		}
		//dbg_printk("checked wLength=%x\n", length_checked);

		// allocate urb, no callback, urb will be automatically de-allocated
		RETURN_EINVAL_IF(!(urb = usbd_alloc_urb_ep0 (function, le16_to_cpu(request->wLength), NULL)));
	
		switch (request->bRequest) {
		case GET_CUR:
			if (!req_op->get_cur) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_cur(request, urb);
			break;
			
		case GET_MIN:
			if (!req_op->get_min) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_min(request, urb);
			break;
			
		case GET_MAX:
			if (!req_op->get_max) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_max(request, urb);
			break;
			
		case GET_RES:
			if (!req_op->get_res) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_res(request, urb);
			break;
			
		case GET_LEN:
			if (!req_op->get_len) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_len(request, urb);
			break;
			
		case GET_INFO:
			if (!req_op->get_info) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_info(request, urb);
			break;
			
		case GET_DEF:
			if (!req_op->get_def) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			rc = req_op->get_def(request, urb);
			break;
			
		default:
			return -EINVAL;
		} /* switch (request->bRequest) */

		//dbg_printk("exec request rc=%d urb->actual_length=%x\n", rc, urb->actual_length);
		if (!rc) {
			RETURN_ZERO_IF(!usbd_send_urb(urb));
		}
		dbg_printk("send data failed\n");
		err_code = REQUEST_UNKNOWN_ERROR;
		usbd_dealloc_urb(urb);
		return -EINVAL;
	} /* if ((request->bmRequestType & USB_REQ_DIRECTION_MASK)) */
	// handle the [SET] requests that do not return data
	else {
		switch (request->bmRequestType & USB_REQ_RECIPIENT_MASK) {
		case USB_REQ_RECIPIENT_INTERFACE:
			switch (request->wIndex) {
			case ((ID_SU1 << 8) | VC_INTERFACE_ID):
				if (wValue == SU_INPUT_SELECT_CONTROL) {
					req_op = &su_control_ops;
					length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;

			case ((ID_PU1 << 8) | VC_INTERFACE_ID):
				if (wValue > PU_CONTROL_UNDEFINED && wValue < PU_CONTROL_MAX_UNDEFINED) {
					req_op = pu_control_ops[wValue];
					length_checked = 2;
					if (request->bRequest == GET_INFO)
						length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;

#ifdef CAMERA_TERMINAL_ZOOM
			case ((ID_CT1 << 8) | VC_INTERFACE_ID):
				if (wValue > CT_CONTROL_UNDEFINED && wValue < CT_CONTROL_MAX_UNDEFINED) {
					req_op = ct_control_ops[wValue];
					length_checked = 2;
					if (request->bRequest == GET_INFO)
						length_checked = 1;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
#endif
			
			case (VS_INTERFACE_ID):
				if (wValue > VS_CONTROL_UNDEFINED && wValue < VS_CONTROL_MAX_UNDEFINED) {
					req_op = vs_control_ops[wValue];
					length_checked = sizeof(struct probe_commit_data);
                                        if (request->bRequest == GET_INFO)
                                                length_checked = 1;
					else if (request->bRequest == GET_LEN)
                                                length_checked = 2;
				}
				else {
					err_code = REQUEST_INVALID_CONTROL;	
					return -EINVAL;
				}
				break;
			default:
				err_code = REQUEST_INVALID_UNIT;
				return -EINVAL;
			} /* switch (request->wIndex) */
			break;
			
		case USB_REQ_RECIPIENT_ENDPOINT:
		default:
			err_code = REQUEST_INVALID_UNIT;
			return -EINVAL;	
		} /* switch (request->bmRequestType & USB_REQ_RECIPIENT_MASK) */

		if(!req_op) {
			err_code = REQUEST_INVALID_CONTROL;
			return -EINVAL;	
		}

		if (!le16_to_cpu(request->wLength) || le16_to_cpu(request->wLength) != length_checked) {
			err_code = REQUEST_INVALID_CONTROL;
			return -EINVAL;	
		}
	
		switch (request->bRequest) {
		case SET_CUR:
			if (!req_op->set_cur) {
				err_code = REQUEST_INVALID_CONTROL;
				return -EINVAL;
			}
			if (!(urb = usbd_alloc_urb_ep0 (function, le16_to_cpu(request->wLength), req_op->set_cur))) {
				err_code = REQUEST_UNKNOWN_ERROR;
				return -EINVAL;
			}
			RETURN_ZERO_IF(!usbd_start_recv(urb));

			usbd_dealloc_urb(urb);
			err_code = REQUEST_UNKNOWN_ERROR;
			return	-EINVAL;
			break;
			
		default:
			return	-EINVAL;
		} /* switch (request->bRequest) */
	} /* if ((request->bmRequestType & USB_REQ_DIRECTION_MASK)) */
	return 0;		
}

/* 
 * uvc_function_enable - enable the UVC function
 *
 * Return non-zero on failure.
 */
static int uvc_function_enable( struct usb_function_instance* function)
{
	int i;

	dbg_printk ("%s:**********function enable =%p %p *************\n",__FUNCTION__,uvc_function_instance,function);
	cur_input_terminal = ID_CT1;

	for (i = PU_CONTROL_UNDEFINED; i < PU_CONTROL_MAX_UNDEFINED; i++)
		pu_control_ops[i] = NULL;
	for (i = VS_CONTROL_UNDEFINED; i < VS_CONTROL_MAX_UNDEFINED; i++)
		vs_control_ops[i] = NULL;
#ifdef CAMERA_TERMINAL_ZOOM
	for (i = CT_CONTROL_UNDEFINED; i < CT_CONTROL_MAX_UNDEFINED; i++)
		ct_control_ops[i] = NULL;
#endif

	pu_control_ops[PU_BRIGHTNESS_CONTROL] = &pu_bightness_ops;
	pu_control_ops[PU_DIGITAL_MULTIPLIER_CONTROL] = &pu_digital_multiplier_ops;
	vs_control_ops[VS_PROBE_CONTROL] = &vs_probe_ops;
	vs_control_ops[VS_COMMIT_CONTROL] = &vs_commit_ops;
#ifdef CAMERA_TERMINAL_ZOOM
	ct_control_ops[CT_ZOOM_ABSOLUTE_CONTROL] = &ct_zoom_absolute_ops;
#endif

	cur_vs_frame = &vs_frames[FORMAT_CIF];
	probe_state = &vs_frames[FORMAT_CIF];

	uvc_function_instance = function;
	uvc_set_thread_state(THREAD_STOP);
	return uvc_kernel_thread_kickoff();
}

/* 
 * uvc_function_disable - disable the UVC function
 */
static void uvc_function_disable( struct usb_function_instance *function )
{
	int i;

	dbg_printk("%s:***********function disable*************\n",__FUNCTION__);
	uvc_kernel_thread_killoff();

	for (i = PU_CONTROL_UNDEFINED; i < PU_CONTROL_MAX_UNDEFINED; i++)
		pu_control_ops[i] = NULL;
	for (i = VS_CONTROL_UNDEFINED; i < VS_CONTROL_MAX_UNDEFINED; i++)
		vs_control_ops[i] = NULL;
#ifdef CAMERA_TERMINAL_ZOOM
	for (i = CT_CONTROL_UNDEFINED; i < CT_CONTROL_MAX_UNDEFINED; i++)
		ct_control_ops[i] = NULL;
#endif

	cur_input_terminal = ID_NONE;
	cur_vs_frame = NULL;
	probe_state = NULL;
	
	uvc_function_instance = NULL;
}

/*
 * uvc_modinit - module init
 *
 */
int uvc_modinit(void)
{
	if (usbd_register_function (&uvc_function_driver)) {
		dbg_printk("\n%s: failed to register uvc function driver\n", __FUNCTION__);
		return -EINVAL;
	}
	dbg_printk("%s:\n UVC Function Driver initiated\n",__FUNCTION__);
	spin_lock_init(&uvc_state.thread_state_lock);
	spin_lock_init(&uvc_set_cur_op.op_lock);

	return 0;
}

/*
 *  uvc_modexit  module cleanup
 *
 */
void uvc_modexit(void)
{
	usbd_deregister_function (&uvc_function_driver);
	dbg_printk("\n%s: UVC Function Driver exited\n",__FUNCTION__);
}	

module_init(uvc_modinit);
module_exit(uvc_modexit);
