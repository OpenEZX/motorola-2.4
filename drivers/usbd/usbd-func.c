/*
 * linux/drivers/usbd/usbd-func.c - USB Function support
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

#include "usbd.h"
#include "usbd-debug.h"
#include "usbd-func.h"
#include "usbd-bus.h"
#include "usbd-inline.h"


#define LANGID_ENGLISH          "\011"
#define LANGID_US_ENGLISH       "\004"

//#define LANGIDs  LANGID_US_ENGLISH LANGID_ENGLISH LANGID_US_ENGLISH LANGID_ENGLISH LANGID_US_ENGLISH LANGID_ENGLISH 
#define LANGIDs  LANGID_US_ENGLISH LANGID_ENGLISH

//#define LANGIDs  LANGID_ENGLISH LANGID_US_ENGLISH LANGID_US_ENGLISH

extern int maxstrings;
struct usb_string_descriptor **usb_strings;
extern int usb_devices;
extern struct usb_function_driver ep0_driver;

extern struct list_head function_drivers;
extern struct list_head devices;

static __inline__ struct usb_function_driver *list_entry_func (const struct list_head *le)
{
	return list_entry (le, struct usb_function_driver, drivers);
}

static __inline__ struct usb_device_instance *list_entry_device (const struct list_head *le)
{
	return list_entry (le, struct usb_device_instance, devices);
}


int dbgflg_usbdfd_init;

extern int registered_functions;
extern int registered_devices;


#define dbg_init(lvl,fmt,args...) dbgPRINT(dbgflg_usbdfd_init,lvl,fmt,##args)


/**
 * usbd_get_string - find and return a string descriptor
 * @index: string index to return
 *
 * Find an indexed string and return a pointer to a it.
 */
struct usb_string_descriptor *usbd_get_string (__u8 index)
{
	if (index >= maxstrings) {
		return NULL;
	}
	return usb_strings[index];
}

/* *
 * usbd_alloc_string_zero - allocate a string descriptor and return index number
 * @str: pointer to string
 *
 * Find an empty slot in index string array, create a corresponding descriptor
 * and return the slot number.
 */
__init __u8 usbd_alloc_string_zero (char *str)
{
	struct usb_string_descriptor *string;
	__u8 bLength;
	__u16 *wData;

	//dbg_init (1, "%02x %02x %02x %02x", str[0], str[1], str[2], str[3]);

	if (usb_strings[0] == NULL) {

		bLength = sizeof (struct usb_string_descriptor) + strlen (str);
		//dbg_init (1, "blength: %d strlen: %d", bLength, strlen (str));

		if (!(string = ckmalloc (bLength, GFP_KERNEL))) {
			return 0;
		}

		string->bLength = bLength;
		string->bDescriptorType = USB_DT_STRING;

		for (wData = string->wData; *str;) {
			*wData = (__u16) ((str[0] << 8 | str[1]));
			//dbg_init (1, "%p %04x %04x", wData, *wData, (((__u16) str[0] << 8 | str[1])));
			str += 2;
			wData++;
		}

		// store in string index array
		usb_strings[0] = string;
	}
	return 0;
}

/* *
 * usbd_alloc_string - allocate a string descriptor and return index number
 * @str: pointer to string
 *
 * Find an empty slot in index string array, create a corresponding descriptor
 * and return the slot number.
 */
__u8 usbd_alloc_string (char *str)
{
	int i;
	struct usb_string_descriptor *string;
	__u8 bLength;
	__u16 *wData;

	if (!str || !strlen (str)) {
		return 0;
	}

	// find an empty string descriptor slot
	for (i = 1; i < maxstrings; i++) {

		if (usb_strings[i] == NULL) {


			bLength = sizeof (struct usb_string_descriptor) + 2 * strlen (str);

			if (!(string = ckmalloc (bLength, GFP_KERNEL))) {
				return 0;
			}

			string->bLength = bLength;
			string->bDescriptorType = USB_DT_STRING;

			for (wData = string->wData; *str;) {
				*wData++ = (__u16) (*str++);
			}

			// store in string index array
			usb_strings[i] = string;
			return i;
		}
		else {
			char *cp = str;
			int j;
                        for (j = 0; (j < ((usb_strings[i]->bLength - 1) / 2)) && ((usb_strings[i]->wData[j] & 0xff) == cp[j]);
                                        j++);
			if ((j >= ((usb_strings[i]->bLength - 1) / 2)) /*&& ((usb_strings[i]->wData[j]&0xff) == cp[j]) */ ) {
				//dbg_init (1, "%s -> %d (duplicate)", str, i);
				return i;
			}
		}
	}
	return 0;
}


#if 1
/* *
 * usbd_dealloc_string - deallocate a string descriptor
 * @index: index into strings array to deallocte
 *
 * Find and remove an allocated string.
 */
void usbd_dealloc_string (__u8 index)
{
	struct usb_string_descriptor *string;

        // duplicate strings are only stored once we have to check to avoid
        // freeing more than once
        // XXX should we either avoid freeing them separately altogether or add a refcount?

	if ((index < maxstrings) && (string = usb_strings[index])) {
		usb_strings[index] = NULL;
		//dbg_init (1, "%p", string);
		lkfree (string);
	}
}
#endif

/* usb-device USB FUNCTION generic functions ************************************************* */


/* *
 * alloc_function_alternates - allocate alternate instance array
 * @alternates: number of alternates
 * @alternate_description_array: pointer to an array of alternate descriptions
 *
 * Return a pointer to an array of alternate instances. Each instance contains a pointer
 * to a filled in alternate descriptor and a pointer to the endpoint descriptor array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_alternate_instance *alloc_function_alternates (int bInterfaceNumber,
                struct usb_interface_description *interface_description,
                int alternates, struct usb_alternate_description *alternate_description_array, 
                __u8 renumber)
{
	int i;
	struct usb_alternate_instance *alternate_instance_array;

	//dbg_init (1, "bInterfaceNumber: %d, alternates: %d", bInterfaceNumber, alternates);

	// allocate array of alternate instances
	if (!(alternate_instance_array = ckmalloc (sizeof (struct usb_alternate_instance) * alternates, GFP_KERNEL))) {
		return NULL;
	}
	// iterate across the alternate descriptions
	for (i = 0; i < alternates; i++) {

		struct usb_alternate_description *alternate_description = alternate_description_array + i;
		struct usb_alternate_instance *alternate_instance = alternate_instance_array + i;
		struct usb_interface_descriptor *interface_descriptor;

		//dbg_init (1, "alternate: %d:%d", i, alternates);

                interface_descriptor = alternate_description->interface_descriptor;
                if (renumber) {
                        interface_descriptor->bInterfaceNumber = bInterfaceNumber;	
                }
		interface_descriptor->bNumEndpoints = alternate_description->endpoints;

		interface_descriptor->iInterface = usbd_alloc_string (alternate_description->iInterface );

                alternate_instance->class_list = alternate_description->class_list;
		alternate_instance->classes = alternate_description->classes;
		alternate_instance->endpoint_list = alternate_description->endpoint_list; 
		alternate_instance->endpoint_transfersize_array = alternate_description->endpoint_transferSizes_list; 

		// save number of alternates, classes and endpoints for this alternate
		alternate_instance->endpoints = alternate_description->endpoints;
		alternate_instance->interface_descriptor = interface_descriptor;
	}
	return alternate_instance_array;
}

/* *
 * alloc_function_interfaces - allocate interface instance array
 * @interfaces: number of interfaces
 * @interface_description_array: pointer to an array of interface descriptions
 *
 * Return a pointer to an array of interface instances. Each instance contains a pointer
 * to a filled in interface descriptor and a pointer to the endpoint descriptor array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_interface_instance *alloc_function_interfaces (int interfaces, 
                struct usb_interface_description *interface_description_array, __u8 renumber)
{
	int interface;
	struct usb_interface_instance *interface_instance_array;

	// allocate array of interface instances
	if (!(interface_instance_array = ckmalloc (sizeof (struct usb_interface_instance) * interfaces, GFP_KERNEL))) {
		return NULL;
	}
	// iterate across the interface descriptions
	for (interface = 0; interface < interfaces; interface++) {

		struct usb_interface_description *interface_description = interface_description_array + interface;
		struct usb_interface_instance *interface_instance = interface_instance_array + interface;

		//dbg_init (1, "interface: %d:%d   alternates: %d", interface, interfaces, interface_description->alternates);

		interface_instance->alternates_instance_array = 
                        alloc_function_alternates (interface, interface_description, 
                                        interface_description->alternates, interface_description->alternate_list, renumber);

		interface_instance->alternates = interface_description->alternates;

	}
	return interface_instance_array;
}


/* *
 * alloc_function_configurations - allocate configuration instance array
 * @configurations: number of configurations
 * @configuration_description_array: pointer to an array of configuration descriptions
 *
 * Return a pointer to an array of configuration instances. Each instance contains a pointer
 * to a filled in configuration descriptor and a pointer to the interface instances array.
 *
 * Returning NULL will cause the caller to cleanup all previously allocated memory.
 */
__init static struct usb_configuration_instance *alloc_function_configurations (int configurations, 
                struct usb_configuration_description *configuration_description_array)
{
	int i;
	struct usb_configuration_instance *configuration_instance_array;


	// allocate array
	if (!(configuration_instance_array = ckmalloc (sizeof (struct usb_configuration_instance) * configurations, GFP_KERNEL))) 
        {
		return NULL;
	}
	// fill in array
	for (i = 0; i < configurations; i++) {
		int j;
		int length;

		struct usb_configuration_description *configuration_description = configuration_description_array + i;
		struct usb_configuration_descriptor *configuration_descriptor;

		//dbg_init (1, "configurations: %d:%d", i, configurations);

                configuration_descriptor = configuration_description->configuration_descriptor;

		// setup fields in configuration descriptor
		// XXX c.f. 9.4.7 zero is default, so config MUST BE 1 to n, not 0 to n-1
		// XXX N.B. the configuration itself is fetched 0 to n-1.

		configuration_descriptor->bConfigurationValue = i + 1;
		configuration_descriptor->wTotalLength = 0;
		configuration_descriptor->bNumInterfaces = configuration_description->interfaces;
		configuration_descriptor->iConfiguration = usbd_alloc_string (configuration_description->iConfiguration);

		// save the configuration descriptor in the configuration instance array
		configuration_instance_array[i].configuration_descriptor = configuration_descriptor;

		if (!(configuration_instance_array[i].interface_instance_array = 
                                        alloc_function_interfaces (configuration_description->interfaces, 
                                                configuration_description->interface_list, 
                                                !configuration_description->interface_no_renumber))) 
                {
			// XXX
			return NULL;
		}


		length = sizeof(struct usb_configuration_descriptor);

		for (j = 0; j < configuration_descriptor->bNumInterfaces; j++) {

			int alternate;
			struct usb_interface_instance *interface_instance = 
                                configuration_instance_array[i].interface_instance_array + j;

                        //printk(KERN_INFO"%s: len: %02x:%02d configuration\n", __FUNCTION__, length, length);

			for (alternate = 0; alternate < interface_instance->alternates; alternate++) {
				int class;
                                int endpoint;
				struct usb_alternate_instance *alternate_instance = 
                                        interface_instance->alternates_instance_array + alternate;

				length += sizeof (struct usb_interface_descriptor);

                                //printk(KERN_INFO"%s: len: %02x:%02d interface\n", __FUNCTION__, length, length);

				for (class = 0; class < alternate_instance->classes; class++) {

                                        __u8 * class_descriptor = *(alternate_instance->class_list + class);

					length += class_descriptor[0];

                                        //printk(KERN_INFO"%s: len: %02x:%02d class\n", __FUNCTION__, length, length);

				}

				for (endpoint = 0; endpoint < alternate_instance->endpoints; endpoint++) {

                                        __u8 * endpoint_descriptor = *(alternate_instance->endpoint_list + endpoint);

					length += endpoint_descriptor[0];
                                        //printk(KERN_INFO"%s: len: %02x:%02d endpoint\n", __FUNCTION__, length, length);
				}

			}
		}
		configuration_descriptor->wTotalLength = cpu_to_le16 (length);
		configuration_instance_array[i].interfaces = configuration_description->interfaces;
                //printk(KERN_INFO"%s: configuration_instance_array[%d]: %p\n", __FUNCTION__, i, 
                //                &configuration_instance_array[i]);
	}
	return configuration_instance_array;
}


/* *
 * usbd_dealloc_function - deallocate a function driver
 * @function_driver: pointer to a function driver structure
 *
 * Find and free all descriptor structures associated with a function structure.
 */
static void __exit usbd_dealloc_function (struct usb_function_driver *function_driver)
{
	int configuration;
	struct usb_configuration_instance *configuration_instance_array = function_driver->configuration_instance_array;

	//dbg_init (1, "%s", function_driver->name);

	// iterate across the descriptors list and de-allocate all structures
	if (function_driver->configuration_instance_array) {
		for (configuration = 0; configuration < function_driver->configurations; configuration++) {
			int interface;
			struct usb_configuration_instance *configuration_instance = configuration_instance_array + configuration;

			//dbg_init (1, "%s configuration: %d", function_driver->name, configuration);

			for (interface = 0; interface < configuration_instance->interfaces; interface++) {

				//int alternate;
				struct usb_interface_instance *interface_instance = 
                                        configuration_instance->interface_instance_array + interface;

				//dbg_init (1, "%s configuration: %d interface: %d", 
                                //                function_driver->name, configuration, interface);
#if 0
				for (alternate = 0; alternate < interface_instance->alternates; alternate++) {
					struct usb_alternate_instance *alternate_instance = 
                                                interface_instance->alternates_instance_array + alternate;

					//dbg_init (1, "%s configuration: %d interface: %d alternate: %d", 
                                        //                function_driver->name, configuration, interface, alternate);

                                        // XXX strings
                                        //usbd_dealloc_descriptor_strings(
                                        //                (struct usb_descriptor *)alternate_instance->interface_descriptor);
				}
#endif
				lkfree (interface_instance->alternates_instance_array);
			}
			lkfree (configuration_instance->interface_instance_array);
			//usbd_dealloc_descriptor_strings( (struct usb_descriptor *)
                        //                configuration_instance_array[configuration].configuration_descriptor);
		}
		function_driver->configuration_instance_array = NULL;
	}
	//usbd_dealloc_descriptor_strings ((struct usb_descriptor *) function_driver->device_descriptor);
	lkfree (configuration_instance_array);
}

__init static int usbd_strings_init (void)
{
        //dbg_init (1, "-");
	if (maxstrings > 254) {
		//dbg_init (1, "setting maxstrings to maximum value of 254");
		maxstrings = 254;
	}
	if (!(usb_strings = ckmalloc (sizeof (struct usb_string_descriptor *) * maxstrings, GFP_KERNEL))) {
		return -1;
	}
	if (usbd_alloc_string_zero (LANGIDs) != 0) {
		lkfree (usb_strings);
		usb_strings = NULL;
		return -1;
	}
	return 0;
}

__exit static void usbd_strings_exit(void)
{
        int i;
        if (registered_functions <= 1 && usb_strings) {
                for (i = 0; i < maxstrings; i++) {
                        //dbg_init (1, "%d %p", i, usb_strings[i]);
#if 1
                        usbd_dealloc_string(i);
#else
			if(usb_strings[i]){
				lkfree(usb_strings[i]);
				usb_strings[i] = NULL;
			}
#endif
                }
                lkfree (usb_strings);
                usb_strings = NULL;
        }
}


/* *
 * usbd_register_function - register a usb function driver
 * @function_driver: pointer to function driver structure
 *
 * Used by a USB Function driver to register itself with the usb device layer.
 *
 * It will create a usb_function_instance structure.
 *
 * The user friendly configuration/interface/endpoint descriptions are compiled into
 * the equivalent ready to use descriptor records.
 *
 * All function drivers must register before any bus interface drivers.
 *
 */
__init int usbd_register_function (struct usb_function_driver *function_driver)
{
	//struct usb_device_descriptor *device_descriptor;

	/* Create a function driver structure, copy the configuration,
	 * interface and endpoint descriptions into descriptors, add to the
	 * function drivers list.
	 */

	//dbg_init (1, "--");

	if (registered_devices) {
		return -EINVAL;
	}
	// initialize the strings pool
	if (!usb_strings && usbd_strings_init ()) {
		//dbg_init (0, "usbd_strings_init failed");
		return -EINVAL;
	}

	list_add_tail (&function_driver->drivers, &function_drivers);
	registered_functions++;
	return 0;
}


/* *
 * usbd_deregister_function - called by a USB FUNCTION driver to deregister itself
 * @function_driver: pointer to struct usb_function_driver
 *
 * Called by a USB Function driver De-register a usb function driver.
 */
void __exit usbd_deregister_function (struct usb_function_driver *function_driver)
{
        usbd_strings_exit();
	list_del (&function_driver->drivers);
	registered_functions--;
}


void usbd_function_init (struct usb_bus_instance *bus, struct usb_device_instance *device, 
                struct usb_function_driver *function_driver, struct usb_function_instance *function_instance)
{
	struct usb_device_description *device_description = function_driver->device_description;
	struct usb_device_descriptor *device_descriptor = device_description->device_descriptor;

	//dbg_init (1, "-");

	// MMM get rid of any old function and install the new one
	usbd_function_close(device);
	device->active_function_instance = function_instance;

	if (function_driver->ops->function_init) {
		function_driver->ops->function_init (bus, device, function_driver, function_instance);
	}

	device_descriptor->bMaxPacketSize0 = bus->driver->maxpacketsize;

	device_descriptor->idVendor = cpu_to_le16(device_description->idVendor);
	device_descriptor->idProduct = cpu_to_le16(device_description->idProduct);
	device_descriptor->bcdDevice = cpu_to_le16(device_description->bcdDevice);
	device_descriptor->bcdUSB = cpu_to_le16(USB_BCD_VERSION);

	device_descriptor->bcdDevice = cpu_to_le16(device_description->bcdDevice);
	device_descriptor->iManufacturer = usbd_alloc_string (device_description->iManufacturer);
	device_descriptor->iProduct = usbd_alloc_string (device_description->iProduct);

	//dbg_init (1, "*      *       *       *");

#ifdef CONFIG_USBD_SERIAL_NUMBER_STR
	dbg_init (1, "SERIAL_NUMBER");
	if (bus->serial_number_str && strlen (bus->serial_number_str)) {
		dbg_init (1, "bus->serial_number_str: %s", bus->serial_number_str);
		device_descriptor->iSerialNumber = usbd_alloc_string (bus->serial_number_str);
	} else {
		dbg_init (1, "function_driver->device_description->iSerialNumber: %s", device_description->iSerialNumber);
		device_descriptor->iSerialNumber = usbd_alloc_string (device_description->iSerialNumber);
	}
	dbg_init (1, "device_descriptor->iSerialNumber: %d", device_descriptor->iSerialNumber);
#else
	// XXX should not need to do anything here
#endif
	device_descriptor->bNumConfigurations = function_driver->configurations;

	dbg_init (3, "configurations: %d", device_descriptor->bNumConfigurations);

	// allocate the configuration descriptor array
	if (!(function_driver->configuration_instance_array = 
                                alloc_function_configurations (function_driver->configurations, 
                                        function_driver->configuration_description))) 
        {
		dbg_init (1, "failed during function configuration %s", function_driver->name);
		//usbd_dealloc_descriptor_strings((struct usb_descriptor *) device_descriptor);
		return;
	}

	function_driver->device_descriptor = device_descriptor;
}


void usbd_function_close (struct usb_device_instance *device)
{
	struct usb_function_instance *function;

	if (device && (function = (device->active_function_instance))) {
		printk(KERN_ERR "%s: closing active function instance #%08x\n",
		       __FUNCTION__,function);
		if (function && function->function_driver &&
		    function->function_driver->ops &&
		    function->function_driver->ops->function_exit) {
			function->function_driver->ops->function_exit (device);
		}
                usbd_dealloc_function (function->function_driver);
		device->active_function_instance = NULL;
	}
}

