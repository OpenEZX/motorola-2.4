/*
 *  linux/arch/arm/mach-ezx/usb_sw.c
 *
 *  Usb setup support for Motorola Ezx Development Platform.
 *  
 *  Author:	Zhuang Xiaofan
 *  Created:	Nov 25, 2003
 *  Copyright:	Motorola Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/init.h>


#define USB_FUNC_ACM	1
#define USB_FUNC_PST	2
#define USB_FUNC_NET	3

static int usb_flag = USB_FUNC_ACM;

extern void pcap_switch_on_usb(void);
extern void pcap_switch_off_usb(void);

static int __init usbd_func_switch(char *line)
{
	if (strncmp (line, "acm", 3) == 0) usb_flag=USB_FUNC_ACM;
	if (strncmp (line, "pst", 3) == 0) usb_flag=USB_FUNC_PST;
	if (strncmp (line, "net", 3) == 0) usb_flag=USB_FUNC_NET;
	return 1;
}
__setup("usbd=", usbd_func_switch);

void pulldown_usb()
{
	if((usb_flag == USB_FUNC_ACM) || (usb_flag == USB_FUNC_PST)) {
		pcap_switch_off_usb();
	}
}

void pullup_usb()
{
	if((usb_flag == USB_FUNC_ACM) || (usb_flag == USB_FUNC_PST)) {
		pcap_switch_on_usb();
	}
}

void __init usbd_func_init()
{
	switch (usb_flag) {
	case USB_FUNC_NET:
		network_modinit();
		break;
	case USB_FUNC_PST:
		pst_modinit();
		break;
	case USB_FUNC_ACM:
	default:
		acm_modinit();
		break;
	}
	bi_modinit();
}

