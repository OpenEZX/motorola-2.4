#ifndef UVC_H
#define UVC_H

/*
 *  uvc.h
 *
 *  Motorola UVC function driver
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
 Author                 Date        Description of Changes
 -----------------  ------------   --------------------------------
 Motorola   	     12/23/2005     Initial Version
 Motorola	     01/10/2006     Add UVC-relative Macros
*/
#ifdef __cplusplus
extern "C" {
#endif


/*===========================================================================
				CONTANTS
============================================================================*/
/*
 * class-specif requests for UVC device
 */
#define RC_UNDEFINED     0x00
#define SET_CUR          0x01
#define GET_CUR          0x81
#define GET_MIN          0x82
#define GET_MAX          0x83
#define GET_RES          0x84
#define GET_LEN          0x85
#define GET_INFO         0x86
#define GET_DEF          0x87



/*
 * Video Interface Class Code
 */
#define CC_VIDEO	0x0E

/*
 * Video Interface Subclass Codes
 */
#define	SC_UNDEFINED 			0x00
#define SC_VIDEOCONTROL			0x01
#define SC_VIDEOSTREAMING		0x02
#define SC_VIDEO_INTERFACE_COLLECTION	0x03

/*
 * Video Interface Protocol Codes
 */
#define PC_PROTOCOL_UNDEFINED		0x00


/*
 * Video Class-Speciic VC Interface Descriptor Subtypes
 */
#define VC__DESCRIPTOR_UNDEFINED        0x00
#define VC_HEADER                       0x01
#define VC_INPUT_TERMINAL               0x02
#define VC_OUTPUT_TERMINAL              0x03
#define VC_SELECTOR_UNIT                0x04
#define VC_PROCESSING_UNIT              0x05
#define VC_EXTENSION_UNIT		0x06

/*
 * Video Class-Specific VS Interface Descriptor Subtypes
 */
#define VS_UNDEFINED			0x00
#define VS_INPUT_HEADER			0x01
#define VS_OUTPUT_HEADER		0x02
#define VS_STILL_IMAGE_FRAME		0x03
#define VS_FORMAT_UNCOMPRESSED		0x04
#define VS_FRAME_UNCOMPRESSED		0x05
#define VS_FORMAT_MJPEG			0x06
#define VS_FRAME_MJPEG			0x07
#define VS_FORMAT_MPEG2TS		0x0A
#define VS_FORMAT_DV			0x0C
#define VS_COLORFORMAT			0x0D
#define VS_FORMAT_FRAME_BASED		0x10
#define VS_FRAME_FRAME_BASED		0x11
#define VS_FORMAT_STREAM_BASED		0x12
/*
 * Video Class_Specific Endpoint Descriptor Subtypes
 */
#define EP_UNDEFINED			0x00
#define EP_GENERAL			0x01
#define EP_ENDPOINT			0x02
#define EP_INTERRUPT			0x03

/*
 * VideoControl Interface Control Selectors
 */
#define VC_CONTROL_UNDEFINED 		0x00
#define VC_VIDEO_POWER_MODE_CONTROL	0x01
#define VC_REQUEST_ERROR_CODE_CONTROL	0x02

/*
 * Terminal Control Selectors
 */
#define TE_CONTROL_UNDEFINED		0x00

/*
 * Selector Unit Control Selectors
 */
#define SU_CONTROL_UNDEFINED  		0x00
#define SU_INPUT_SELECT_CONTROL		0x01

/*
 * Camera Terminal Control Selectors
 */
#define CT_CONTROL_UNDEFINED			0x00
#define CT_SCANNING_MODE_CONTROL		0x01
#define CT_AE_MODE_CONTROL			0x02
#define CT_AE_PRIORITY_CONTROL			0x03
#define CT_EXPOSURE_TIME_ABSOLUTE_CONTROL	0x04
#define CT_EXPOSURE_TIME_RELATIVE_CONTROL	0x05
#define CT_FOCUS_ABSOLUTE_CONTROL		0x06
#define CT_FOCUS_RELATIVE_CONTROL		0x07
#define CT_FOCUS_AUTO_CONTROL			0x08
#define CT_IRIS_ABSOLUTE_CONTROL		0x09
#define CT_IRIS_RELATIVE_CONTROL		0x0A
#define CT_ZOOM_ABSOLUTE_CONTROL 		0x0B
#define CT_ZOOM_RELATIVE_CONTROL		0x0C
#define CT_PANTILT_ABSOLUTE_CONTROL		0x0D
#define CT_PANTILT_RELATIVE_CONTROL		0x0E
#define CT_ROLL_ABSOLUTE_CONTROL		0x0F
#define CT_ROLL_RELATIVE_CONTROL		0x10
#define CT_PRIVACY_CONTROL			0x11

#define CT_CONTROL_MAX_UNDEFINED		0x12 //not defined in specification, only for convenience

/*
 * Processing Unit Control Selectors
 */
#define PU_CONTROL_UNDEFINED			0x00
#define PU_BACKLIGHT_COMPENSATION_CONTROL	0x01
#define PU_BRIGHTNESS_CONTROL			0x02
#define PU_CONTRAST_CONTROL			0x03
#define PU_GAIN_CONTROL				0x04
#define PU_POWER_LINE_FREQUENCY_CONTROL		0x05
#define PU_HUE_CONTROL				0x06
#define PU_SATURATION_CONTROL			0x07
#define PU_SHARPNESS_CONTROL			0x08
#define PU_GAMMA_CONTROL			0x09
#define PU_WHITE_BALANCE_TEMPERATURE_CONTROL	0x0A
#define PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL	0x0B
#define PU_WHITE_BALANCE_COMPONENT_CONTROL	0x0C
#define PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL	0x0D
#define PU_DIGITAL_MULTIPLIER_CONTROL		0x0E
#define PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL	0x0F
#define PU_HUE_AUTO_CONTROL			0x10
#define PU_ANALOG_VIDEO_STANDARD_CONTROL	0x11
#define PU_ANALOG_LOCK_STATUS_CONTROL		0x12

#define PU_CONTROL_MAX_UNDEFINED		0x13 //not defined in specification, only for convenience

/*
 * Extension Unit Control Selectors
 */
#define XU_CONTROL_UNDEFINED  		0x00


/*
 * VideoStreaming Interface Control Selector
 */
#define VS_CONTROL_UNDEFINED			0x00
#define VS_PROBE_CONTROL			0x01
#define VS_COMMIT_CONTROL			0x02
#define VS_STILL_PROBE_CONTROL			0x03
#define VS_STILL_COMMIT_CONTROL			0x04
#define VS_STILL_IMAGE_TRIGGER_CONTROL		0x05
#define VS_STREAM_ERROR_CODE_CONTROL		0x06
#define VS_GENERATE_KEY_FRAME_CONTROL		0x07
#define VS_UPDATE_FRAME_SEGMENT_CONTROL		0x08
#define VS_SYNCH_DELAY_CONTROL			0x09
#define VS_CONTROL_MAX_UNDEFINED		0x0A	//not defined in specification, only for convenience

/*
 * USB Terminal Types
 */
#define TT_VENDOR_SPECIFIC 			0x0100
#define TT_STREAMING				0x0101

/*
 * Input Terminal Types
 */
#define ITT_VENDOR_SPECIFIC			0x0200
#define ITT_CAMERA				0x0201  //Camera sensor,To be used only in camera terminal descriptors.
#define ITT_MEDIA_TRANSPORT_INPUT		0x0202

/*
 * Output Terminal Types
 */
#define OTT_VENDOR_SPECIFIC			0x0300
#define OTT_DISPLAY				0x0301
#define OTT_MEDIA_TRANSPORT_OUTPUT		0x0302

/*
 * External Terminal Typers
 */
#define EXTERNAL_VENDOR_SPECIFIC		0x0400
#define COMPOSITE_CONNECTOR			0x0401
#define SVIDEO_CONNECTOR			0x0402
#define COMPONENT_CONNECTOR			0x0403

/*
 * Request error code
 */
#define REQUEST_NO_ERROR			0x00
#define REQUEST_NOT_READY			0x01
#define REQUEST_WRONG_STATE			0x02
#define REQUEST_POWER_NOT_ENOUGH		0x03
#define REQUEST_OUT_RANGE			0x04
#define REQUEST_INVALID_UNIT			0x05
#define REQUEST_INVALID_CONTROL			0x06
#define REQUEST_INVALID_REQUEST			0x07
#define REQUEST_UNKNOWN_ERROR			0xFF


/*============================================================================
                               STRUCTURES AND OTHER TYPEDEFS
============================================================================*/
/**
 * @struct uvc_req_operations
 * @function pointer of unit and terminal operations
 */
struct uvc_req_operations {
        int (*set_cur) (struct urb *urb, int rc);
        int (*get_cur) (struct usb_device_request *request, struct urb *urb);
        int (*get_min) (struct usb_device_request *request, struct urb *urb);
        int (*get_max) (struct usb_device_request *request, struct urb *urb);
        int (*get_res) (struct usb_device_request *request, struct urb *urb);
        int (*get_len) (struct usb_device_request *request, struct urb *urb);
        int (*get_info) (struct usb_device_request *request, struct urb *urb);
        int (*get_def) (struct usb_device_request *request, struct urb *urb);
};

/**
 * @struct probe_commit_data
 * @data struct used in probe and commit request.
 */
struct probe_commit_data {
        __u16 bmHint;
        __u8  bFormatIndex;
        __u8  bFrameIndex;
        __u32 dwFrameInterval;
        __u16 wKeyFrameRate;
        __u16 wPFrameRate;
        __u16 wCompQuality;
        __u16 wCompWindowSize;
        __u16 wDelay;
        __u32 dwMaxVideoFrameSize;
        __u32 dwMaxPayloadTransferSize;
//      __u32 dwClockFrequency;
//      __u8  bmFramingInfo;
//      __u8  bPreferedVersion;
//      __u8  bMinVersion;
//      __u8  bMaxVersion;
}__attribute__ ((packed));



/*
 * Descriptors for UVC
 */
struct usb_video_control_interface_header_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u16 bcdUVC;
	__u16 wTotalLength;
	__u32 dwClockFrequency;
	__u8 bInCollection;
	__u8 baInterfaceNr;	//only one video streaming interface
}__attribute__((packed));

struct usb_video_control_input_terminal_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bTerminalID;
	__u16 wTerminalType;
	__u8 bAssocTerminal;
	__u8 iTerminal;
}__attribute__((packed));

struct usb_video_control_camera_terminal_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bTerminalID;
	__u16 wTerminalType;
	__u8 bAssocTerminal;
	__u8 iTerminal;
	__u16 wObjectiveFocalLengthMin;
	__u16 wObjectiveFocalLengthMax;
	__u16 wOcularFocalLength;
	__u8 bControlSize;
	__u16 bmControls;
}__attribute__((packed));

struct usb_video_control_output_terminal_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bTerminalID;
	__u16 wTerminalType;
	__u8 bAssocTerminal;
	__u8 bSourceID;
	__u8 iTerminal;
}__attribute__((packed));

struct usb_video_control_selector_unit_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bUnitID;
	__u8 bNrInPins;
	__u8 baSourceID1;//struct define here is for 2 input pins
	__u8 baSourceID2;
	__u8 iSelector;
}__attribute__((packed));

struct usb_video_control_processing_unit_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bUnitID;
	__u8 bSourceID;
	__u16 wMaxMultiplier;
	__u8 bControlSize;
	__u16 bmControls;
	__u8 iProcessing;
//	__u8 bmVideoStandards;
}__attribute__((packed));

struct usb_video_streaming_interface_input_header_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bNumFormats;
	__u16 wTotalLength;
	__u8 bEndpointAddress;
	__u8 bmInfo;
	__u8 bTerminalLink;
	__u8 bStillCaptureMethod;
	__u8 bTriggerSupport;
	__u8 bTriggerUsage;
	__u8 bControlSize;
	__u8 bmaControls;
}__attribute__((packed));

struct usb_video_streaming_interface_output_header_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bNumFormats;
	__u16 wTotalLength;
	__u8 bEndpointAddress;
	__u8 bTerminalLink;
	__u8 bControlSize;
	__u8 bmaControls;
}__attribute__((packed));

struct usb_video_payload_format_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bFormatIndex;
	__u8 bNumFrameDescriptors;
	__u8 guidFormat[16];
	__u8 bBitsPerPixel;
	__u8 bDefaultFrameIndex;
	__u8 bAspectRatioX;
	__u8 bAspectRatioY;
	__u8 bmInterlaceFlags;
	__u8 bCopyProtect;
}__attribute__((packed));

struct usb_video_frame_descriptor{
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bFrameIndex;
	__u8 bmCapabilities;
	__u16 wWidth;
	__u16 wHeight;
	__u32 dwMinBitRate;
	__u32 dwMaxBitRate;
	__u32 dwMaxVideoFrameBufferSize;
	__u32 dwDefaultFrameInterval;
	__u8 bFrameIntervalType;
	__u32 dwFrameInterval;
}__attribute__((packed));

struct usb_color_matching_descriptor{
        __u8 bLength;
        __u8 bDescriptorType;
        __u8 bDescriptorSubtype;
	__u8 bColorPrimaries;
	__u8 bTransferCharacteristics;
	__u8 MatrixCoefficients;
}__attribute__((packed));

#ifdef __cplusplus
}
#endif
#endif  /*UVC_H*/






