#ifndef __EZX_USBD_H
#define __EZX_USBD_H

#define MOTUSBD_PROCFILE		"/proc/motusbd"

// macros for USB status
#define USB_STATUS_NONE		(-1)
#define USB_STATUS_MODEM	0
#define USB_STATUS_NET		1
#define USB_STATUS_USBLAN	USB_STATUS_NET
#define USB_STATUS_CFG11	2
#define USB_STATUS_PST		3
#define USB_STATUS_STORAGE	4
#define USB_STATUS_DSPLOG       5
#define USB_STATUS_NM           6

#define USB_STATUS_STRING_NONE		"MotNone"
#define USB_STATUS_STRING_PST		"MotPst"
#define USB_STATUS_STRING_CFG11		"MotCdc"
#define USB_STATUS_STRING_NET		"MotNet"
#define USB_STATUS_STRING_USBLAN	USB_STATUS_STRING_NET
#define USB_STATUS_STRING_MODEM		"MotACM"
#define USB_STATUS_STRING_STORAGE	"MotMas"
#define USB_STATUS_STRING_DSPLOG        "MotDSPLog"
#define USB_STATUS_STRING_NM            "MotNM"
#endif
