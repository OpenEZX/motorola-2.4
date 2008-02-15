/*
 * linux/drivers/usbd/usbd-procfs.c - USB Device Core Layer
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Tom Rushworth <tbr@belcarra.com>, 
 *      Bruce Balden <balden@belcarra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <linux/config.h>
#include <linux/module.h>

#include "usbd-build.h"
#include "usbd-module.h"

MODULE_AUTHOR ("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION ("USB Device Core Support Procfs");
MODULE_LICENSE("GPL");


USBD_MODULE_INFO ("usbdprocfs 0.1");

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <linux/delay.h>

#include "usbd.h"
#include "usbd-debug.h"
#include "usbd-func.h"
#include "usbd-bus.h"
#include "usbd-inline.h"

#include "hotplug.h"

#define MAX_INTERFACES 2

extern int usb_devices;
extern int maxstrings;

extern struct usb_function_driver ep0_driver;

extern int registered_functions;
extern int registered_devices;

extern struct list_head function_drivers;

/* List support functions ******************************************************************** */


static inline struct usb_function_driver *list_entry_func (const struct list_head *le)
{
	return list_entry (le, struct usb_function_driver, drivers);
}

static inline struct usb_device_instance *list_entry_device (const struct list_head *le)
{
	return list_entry (le, struct usb_device_instance, devices);
}


#ifdef CONFIG_USBD_PROCFS
/* Proc Filesystem *************************************************************************** */
/* *
 * dohex
 *
 */
static void dohexdigit (char *cp, unsigned char val)
{
	if (val < 0xa) {
		*cp = val + '0';
	} else if ((val >= 0x0a) && (val <= 0x0f)) {
		*cp = val - 0x0a + 'a';
	}
}

/* *
 * dohex
 *
 */
static void dohexval (char *cp, unsigned char val)
{
	dohexdigit (cp++, val >> 4);
	dohexdigit (cp++, val & 0xf);
}

/* *
 * dump_descriptor
 */
static int dump_descriptor (char *buf, char *sp)
{
        int num = *sp;
	int len = 0;

        //printk(KERN_INFO"%s: %p %d %d %d\n", __FUNCTION__, buf, *buf, buf[0], num);

	while (sp && num--) {
		dohexval (buf, *sp++);
		buf += 2;
		*buf++ = ' ';
		len += 3;
	}
	len++;
	*buf = '\n';
	return len;
}


/* *
 * usbd_device_proc_read - implement proc file system read.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system read function.
 *
 * We let upper layers iterate for us, *pos will indicate which device to return
 * statistics for.
 */
static ssize_t usbd_device_proc_read_functions (struct file *file, char *buf, size_t count, loff_t * pos)
{
	unsigned long page;
	int len = 0;
	int index;

	struct list_head *lhd;

	// get a page, max 4095 bytes of data...
	if (!(page = get_free_page (GFP_KERNEL))) {
		return -ENOMEM;
	}

	len = 0;
	index = (*pos)++;

	if (index == 0) {
		len += sprintf ((char *) page + len, "usb-device list\n");
        }

        //printk(KERN_INFO"%s: index: %d len: %d\n", __FUNCTION__, index, len);

        //read_lock(&usb_device_rwlock);
        list_for_each (lhd, &function_drivers) {

                int configuration = index;
                struct usb_function_driver *function_driver = list_entry_func (lhd);
                struct usb_configuration_instance *configuration_instance_array = function_driver->configuration_instance_array;

                //printk(KERN_INFO"%s: name: %s\n", __FUNCTION__, function_driver->name);

                if (!configuration_instance_array) {
                        //printk(KERN_INFO"%s: configuration_instance_array NULL\n", __FUNCTION__);
                        continue;
                }

                if (index == 0) {
                        len += sprintf ((char *) page + len, "\nDevice descriptor                  ");
                        len += dump_descriptor ((char *) page + len, (char *) function_driver->device_descriptor);
                }

                //printk(KERN_INFO"%s: %d configurations: %d\n", __FUNCTION__, configuration, function_driver->configurations);

                if (configuration < function_driver->configurations) {

                        int interface;
                        struct usb_configuration_descriptor *configuration_descriptor = 
                                configuration_instance_array[configuration].configuration_descriptor;

                        len += sprintf ((char *) page + len, "\nConfiguration descriptor [%d      ] ", configuration);

                        len += dump_descriptor ((char *) page + len, (char *) configuration_descriptor);

                        //printk(KERN_INFO"%s: interfaces: %d\n", __FUNCTION__, 
                        //                configuration_instance_array[configuration].interfaces);

                        for (interface = 0; interface < configuration_instance_array[configuration].interfaces; interface++) {

                                int alternate;
                                struct usb_interface_instance *interface_instance = 
                                        configuration_instance_array[configuration].interface_instance_array + interface;

                                //printk(KERN_INFO"BBB len: %d\n%s", len, page);

                                for (alternate = 0; alternate < interface_instance->alternates; alternate++) {

                                        int endpoint;
                                        int class;
                                        struct usb_alternate_instance *alternate_instance = 
                                                interface_instance->alternates_instance_array + alternate;

                                        struct usb_interface_descriptor *interface_descriptor = 
                                                alternate_instance->interface_descriptor;

                                        //printk(KERN_INFO"%s:alternate: %d:%d:%d classes: %d\n", __FUNCTION__,
                                        //                configuration, interface, alternate, alternate_instance->classes);

                                        len += sprintf ((char *) page + len, "\nInterface descriptor     [%d:%d:%d  ] ", 
                                                        configuration, interface + 1, alternate);

                                        len += dump_descriptor ((char *) page + len, (char *) interface_descriptor);

                                        for (class = 0; class < alternate_instance->classes; class++) {

                                                __u8 *class_descriptor = *(alternate_instance->class_list + class);

                                                len += sprintf ((char *) page + len, "Class descriptor         [%d:%d:%d  ] ",
                                                                configuration, interface + 1, class);

                                                len += dump_descriptor ((char *) page + len, (char *) class_descriptor);
                                        }


                                        //printk(KERN_INFO"%s: alternate: %d:%d:%d endpoints: %d\n", __FUNCTION__,
                                        //                configuration, interface, alternate, alternate_instance->endpoints);

                                        for (endpoint = 0; endpoint < alternate_instance->endpoints; endpoint++) 
                                        {

                                                __u8 *endpoint_descriptor = *(alternate_instance->endpoint_list + endpoint);

                                                //printk(KERN_INFO"%s: endpoint: %d:%d:%d:%d %p\n", __FUNCTION__,
                                                //                configuration, interface, alternate, endpoint, 
                                                //                endpoint_descriptor);

                                                len += sprintf ((char *) page + len, "Endpoint descriptor      [%d:%d:%d:%d] ",
                                                                configuration, interface + 1, alternate, endpoint);

                                                len += dump_descriptor ((char *) page + len, (char *)endpoint_descriptor);
                                        }
                                }
                        }
                }
                else if (configuration == function_driver->configurations) {


                        int i;
                        int k;
                        struct usb_string_descriptor *string_descriptor;

                        len += sprintf ((char *) page + len, "\n\n");

                        if ((string_descriptor = usbd_get_string (0)) != NULL) {
                                len += sprintf ((char *) page + len, "String                   [%2d]      ", 0);

                                for (k = 0; k < (string_descriptor->bLength / 2) - 1; k++) {
                                        len += sprintf ((char *) page + len, "%02x %02x ", 
                                                        string_descriptor->wData[k] >> 8, string_descriptor->wData[k] & 0xff);
                                        len++;
                                }
                                len += sprintf ((char *) page + len, "\n");
                        }

                        for (i = 1; i < maxstrings; i++) {

                                if ((string_descriptor = usbd_get_string (i)) != NULL) {

                                        len += sprintf((char *)page+len, "String                   [%2d:%2d]   ", 
                                                        i, string_descriptor->bLength);

                                        // bLength = sizeof(struct usb_string_descriptor) + 2*strlen(str)-2;

                                        for (k = 0; k < (string_descriptor->bLength / 2) - 1; k++) {
                                                *(char *) (page + len) = (char) string_descriptor->wData[k];
                                                len++;
                                        }
                                        len += sprintf ((char *) page + len, "\n");
                                }
                        }
                }
        }

        //printk(KERN_INFO"%s: len: %d count: %d\n", __FUNCTION__, len, count);

        if (len > count) {
                //printk(KERN_INFO"%s: len > count\n", __FUNCTION__);
                //printk(KERN_INFO"%s", page);
                len = -EINVAL;
        } 
        else if ((len > 0) && copy_to_user (buf, (char *) page, len)) {
                //printk(KERN_INFO"%s: EFAULT\n", __FUNCTION__);
                len = -EFAULT;
        }
        else {
                //printk(KERN_INFO"%s: OK\n", __FUNCTION__);
        }
        free_page (page);
        return len;
}

static struct file_operations usbd_device_proc_operations_functions = {
read:usbd_device_proc_read_functions,
};

#endif

/**add by Levis for switch**/
#define MAX_FUNC_NAME_LEN  64
extern void udc_connect(void);
extern void udc_disconnect(void);
extern int bi_modinit(void);
extern void bi_modexit(void);
extern void acm_modexit(void);
extern int pst_modinit(void);
extern int switch_cfg11(struct usb_device_instance *device);

static ssize_t usbd_proc_switch_write (struct file *file, const char *buf, size_t count, loff_t * pos)
{

	struct usb_device_instance *device = NULL;
	char command[MAX_FUNC_NAME_LEN+1];
	size_t n = count;
        size_t l;
	char c;

	MOD_INC_USE_COUNT;
	command[0] = 0;
	//printk(KERN_DEBUG "%s: count=%u\n",__FUNCTION__,count);
	if (n > 0) {
		l = MIN(n,MAX_FUNC_NAME_LEN);
		if (copy_from_user (command, buf, l)) {
			count = -EFAULT;
		} else {
			if (l > 0 && command[l-1] == '\n') {
				l -= 1;
			}
			command[l] = 0;
			n -= l;
			// flush remainder, if any
			while (n > 0) {
				// Not too efficient, but it shouldn't matter
				if (copy_from_user (&c, buf + (count - n), 1)) {
					count = -EFAULT;
					break;
				}
				n -= 1;
			}
		}
	}
	//printk(KERN_ERR "%s: searching for [%s]\n",__FUNCTION__,command);
	if (count > 0 && !strcmp (command, "plug")) {
		udc_connect ();
	} 
        else if (count > 0 && !strcmp (command, "unplug")) {
		udc_disconnect ();
                usbd_device_event (device, DEVICE_RESET, 0);
	}
	else if (count > 0 && !strcmp (command, "MotPst")) {
		bi_modexit();
		acm_modexit();
		udelay(10);
		pst_modinit();
		bi_modinit();
	}else if (count > 0 && !strcmp (command, "MotCdc")) {
		switch_cfg11(device);
	}
	MOD_DEC_USE_COUNT;

	return (count);
}

static struct file_operations usbd_proc_switch_functions = {
	write:usbd_proc_switch_write,
};
/**end**/

/* Module init ******************************************************************************* */


static int __init usbd_device_init (void)
{
	struct proc_dir_entry *pw;
#ifdef CONFIG_USBD_PROCFS
        {
                struct proc_dir_entry *p;

                // create proc filesystem entries
                if ((p = create_proc_entry ("usb-functions", 0, 0)) == NULL)
                        return -ENOMEM;
                p->proc_fops = &usbd_device_proc_operations_functions;
        }
#endif
/**add by Levis for switch**/
	// create proc filesystem entries for usb switch.
	if ((pw = create_proc_entry ("usbd-switch", 0666, 0)) == NULL) {
		return -ENOMEM;
	}
	pw->proc_fops = &usbd_proc_switch_functions;
/**end**/

        return 0;
}

static void __exit usbd_device_exit (void)
{
#ifdef CONFIG_USBD_PROCFS
        // remove proc filesystem entry
        remove_proc_entry ("usb-functions", NULL);
#endif
/**add by Levis for switch**/
	remove_proc_entry ("usbd-switch", NULL);
/**end**/
}

module_init (usbd_device_init);
module_exit (usbd_device_exit);

