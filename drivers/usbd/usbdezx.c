/*
 * 
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
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

#ifdef CONFIG_ARCH_EZX

#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/tqueue.h>
#include <linux/string.h>
#include <linux/ezxusbd.h>
#include <linux/interrupt.h>
#include "../misc/ezx-emu.h"

#define USB_FUNC_ACM	1
#define USB_FUNC_PST	2
#define USB_FUNC_NET	3

static int usb_flag = USB_FUNC_ACM;
int usbcable_status = 0;		// status of USB cable, depends on USB enumeration
int emucable_status = 0;		// status of EMU cable, such as USB/charger/carkit, depends on PCAP 4V INT

extern void pcap_switch_on_usb(void);
extern void pcap_switch_off_usb(void);
extern int motusbd_nfs_flag;
extern int motusbd_load(int status);

#ifdef CONFIG_ARCH_EZX // added by Jordan 
extern void udc_clock_enable(void);
extern void udc_clock_disable(void);
#endif

static int __init usbd_func_switch(char *line)
{
	//printk(KERN_INFO"%s: line %s\n", __FUNCTION__, line);
	if (strncmp (line, "acm", 3) == 0) usb_flag=USB_FUNC_ACM;
	if (strncmp (line, "pst", 3) == 0) usb_flag=USB_FUNC_PST;
	if (strncmp (line, "net", 3) == 0) usb_flag=USB_FUNC_NET;
	return 1;
}
__setup("usbd=", usbd_func_switch);

void pulldown_usb(void)
{
	if((usb_flag == USB_FUNC_ACM) || (usb_flag == USB_FUNC_PST)) {
		pcap_switch_off_usb();
	}
}

void pullup_usb(void)
{
	if((usb_flag == USB_FUNC_ACM) || (usb_flag == USB_FUNC_PST)) {
                //mdelay(200);
		pcap_switch_on_usb();
	}
}

#include <asm/hardware.h>

#ifdef CONFIG_HOTPLUG

// cable event hotplug mechanism
// If cable is attached, system will enable usb gpio and call a script file to handle usb status.
// If cable is detached, system will enable uart gpio and call a script file to handle usb status.

#include <linux/kmod.h>
#include "../misc/ssp_pcap.h"
#include <linux/ezx-button.h>

#define AGENT		"usbcable"
#define POWERON_JIFF	100	// 1 second to initialize the system component
#define EMU_USBD_JIFF	50	// Fix me - time for waiting for the first GET DESCRIPTOR command
#define SYS_INIT_RDY_JIFF       1500            // ??? App will launch the application when inited ???

struct tq_struct cable_hotplug_tq;
static ACCESSORY_DEVICE_STATUS cable_attach;

extern unsigned int udc_interrupts;
extern void clear_device_state(void);
extern void restore_powermode(void);

static void cable_hotplug_bh(void *data);
int cable_hotplug_attach(ACCESSORY_DEVICE_STATUS attach);

#include "usbd.h"
extern struct usb_bus_instance *usbd_bus;
extern void usbd_bus_event_irq (struct usb_bus_instance *bus, usb_device_event_t event, int data);

//#define PM_USBDCLK
#undef PM_USBDCLK

// The function will be called in the interrupt handle of cable event in PCAP module.
int cable_hotplug_attach(ACCESSORY_DEVICE_STATUS attach)
{
	//printk(KERN_INFO"%s: jiffies %d attach %d\n", __FUNCTION__, (int)jiffies, (int)attach);
	//printk(KERN_INFO"%s: usbcable_status %d\n", __FUNCTION__, usbcable_status);

	if(attach == ACCESSORY_DEVICE_STATUS_ATTACHED)
	{
		/*
                 * usbc_gpio_init();
                 */
		emucable_status = 1;
                emu_switch_to(EMU_SWITCH_TO_USB);
#ifdef PM_USBDCLK
#error
		udc_clock_enable();
#endif
	}
	else if(attach == ACCESSORY_DEVICE_STATUS_DETACHED)
	{
		emucable_status = 0;
		if(usbcable_status)
		{
			// send CABLE detach event to usbd stack
			usbd_bus_event_irq (usbd_bus, DEVICE_RESET, 0);
			usbd_bus_event_irq (usbd_bus, DEVICE_HUB_RESET, 0);
			usbd_bus_event_irq (usbd_bus, DEVICE_DE_CONFIGURED, 0);

			// wait till QT is ready???
			if(jiffies > SYS_INIT_RDY_JIFF)
			{
				// cable detaching: call the hotplug script in the PCAP 4V ISR
				// cable attaching: call the hotplug script when enumeration
				*((ACCESSORY_DEVICE_STATUS *)(cable_hotplug_tq.data)) = attach;
				schedule_task(&cable_hotplug_tq);
			}
			
			usbcable_status = 0;
			usb_cable_event_handler(USB_CABLE_OUT);
			// clear USBD state machine
			clear_device_state();
		}
#ifdef PM_USBDCLK
		udc_clock_disable();
#endif
		emu_switch_to(EMU_SWITCH_TO_NO_DEV);
	}
	return 0;
}

static void cable_hotplug_bh(void *data)
{
	char *argv[3];
	char *envp[10];
	int i;
	ACCESSORY_DEVICE_STATUS attach = *((ACCESSORY_DEVICE_STATUS *)(data));

	if(!hotplug_path[0])	// || (in_interrupt()))
	{
		//printk(KERN_INFO"%s: error! hotplug_path[0] = 0x%x\n", __FUNCTION__, hotplug_path[0]);
		//printk(KERN_INFO"%s: error! in_interrupt() = %d\n", __FUNCTION__, in_interrrupt());
		return ;
	}

	i = 0;
	argv[i++] = hotplug_path;
	argv[i++] = AGENT;
	argv[i++] = 0;

	i = 0;
	envp[i++] = "HOME=/";
	envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";

	if (attach == ACCESSORY_DEVICE_STATUS_ATTACHED)
	{
		//printk(KERN_INFO"%s: attach\n", __FUNCTION__);
		envp[i++] = "ACTION=attach";
	}
	else if (attach == ACCESSORY_DEVICE_STATUS_DETACHED)
	{
		//printk(KERN_INFO"%s: detach\n", __FUNCTION__);
		envp[i++] = "ACTION=detach";
	}
	else
	{
		//printk(KERN_INFO"%s: unknow event %x\n", __FUNCTION__, attach);
		return ;
	}
	
	envp[i++] = 0;
	call_usermodehelper (argv[0], argv, envp);
	return ;
}

#endif		// CONFIG_HOTPLUG

static int __init motusbd_func_modinit(void)
{

#ifdef CONFIG_HOTPLUG
	cable_hotplug_tq.routine = cable_hotplug_bh;
	cable_hotplug_tq.data = &cable_attach;
#endif
	
	//printk (KERN_INFO "%s: usb_flag = %d\n", __FUNCTION__, usb_flag);
	switch (usb_flag) {
	case USB_FUNC_NET:
		motusbd_nfs_flag = 1;
		motusbd_load(USB_STATUS_NET);
		break;
	case USB_FUNC_PST:
		motusbd_load(USB_STATUS_PST);
		break;
	case USB_FUNC_ACM:
		// fall through
	default:
		motusbd_load(USB_STATUS_MODEM);
		break;
	}
	return 0;
}

static void __exit motusbd_func_modexit (void)
{
	// unload usbd func?
	return ;
}

module_init (motusbd_func_modinit);
module_exit (motusbd_func_modexit);

//EXPORT_SYMBOL(pullup_usb);
//EXPORT_SYMBOL(pulldown_usb);

#endif	// CONFIG_ARCH_EZX
