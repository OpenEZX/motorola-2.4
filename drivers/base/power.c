/*
 * power.c - power management functions for the device tree.
 * 
 * Copyright (c) 2002 Patrick Mochel
 *		 2002 Open Source Development Lab
 * 
 *  Kai Germaschewski contributed to the list walking routines.
 *
 */

#undef DEBUG

#include <linux/device.h>
#include <linux/module.h>
#include <asm/semaphore.h>
#include "base.h"

#define to_dev(node) container_of(node,struct device,g_list)

/**
 * device_suspend - suspend/remove all devices on the device ree
 * @state:	state we're entering
 * @level:	what stage of the suspend process we're at
 *    (emb: it seems that these two arguments are described backwards of what
 *          they actually mean .. is this correct?)
 *
 * The entries in the global device list are inserted such that they're in a
 * depth-first ordering.  So, simply interate over the list, and call the 
 * driver's suspend or remove callback for each device.
 */
int device_suspend(u32 state, u32 level)
{
	struct list_head * node;
	int error = 0;

#if 0 /* linux-pm */
	printk(KERN_EMERG "Suspending devices\n");
#endif /* linux-pm */

	down(&device_sem);
	list_for_each_prev(node,&global_device_list) {
		struct device * dev = to_dev(node);
		if (dev->driver && dev->driver->suspend) {
			pr_debug("suspending device %s\n",dev->name);
			error = dev->driver->suspend(dev,state,level);
			if (error)
				printk(KERN_ERR "%s: suspend returned %d\n",dev->name,error);
		}
	}
	up(&device_sem);
	return error;
}

/**
 * device_resume - resume all the devices in the system
 * @level:	stage of resume process we're at 
 * 
 * Similar to device_suspend above, though we want to do a breadth-first
 * walk of the tree to make sure we wake up parents before children.
 * So, we iterate over the list backward. 
 */
void device_resume(u32 level)
{
	struct list_head * node;

	down(&device_sem);
	list_for_each(node,&global_device_list) {
		struct device * dev = to_dev(node);
		if (dev->driver && dev->driver->resume &&
			(dev->power_state == DPM_POWER_ON)) {
			pr_debug("resuming device %s\n",dev->name);
			dev->driver->resume(dev,level);
#if 1 /* linux-pm */
			/*
			 * In case the driver resume code does something that
			 * sends a signal to the resume thread (like a
			 * SIGCHLD for /sbin/hotplug exit), flush it so the
			 * next driver's resume code can issue blocking 
			 * operations without the signal interfering.
			 */

			flush_signals(current);
#endif /* linux-pm */
		}
	}
	up(&device_sem);

#if 0 /* linux-pm */
	printk(KERN_EMERG "Devices Resumed\n");
#endif /* linux-pm */
}

/**
 * device_shutdown - call ->remove() on each device to shutdown. 
 */
void device_shutdown(void)
{
	struct list_head * entry;
	
	printk(KERN_EMERG "Shutting down devices\n");

	down(&device_sem);
	list_for_each(entry,&global_device_list) {
		struct device * dev = to_dev(entry);
		if (dev->driver && dev->driver->shutdown) {
			pr_debug("shutting down %s\n",dev->name);
			dev->driver->shutdown(dev);
		}
	}
	up(&device_sem);
}

#if 1 /* linux-pm*/
int validate_constraints(struct bus_type *bus, struct constraints *constraints)
{
	int i, j;

	if (! constraints || ! bus->bus_op)
		return 1;

	for (i = 0; i < constraints->count; i++) {
		for (j = 0; j < bus->bus_op->count; j++) {
			if (constraints->param[i].id == 
			    bus->bus_op->param[j].id)
				if ((constraints->param[i].min >
				     bus->bus_op->param[j].val) ||
				    (constraints->param[i].max <
				     bus->bus_op->param[j].val))
					return 0;
		}
	}

	return 1;
}

int device_powerup(struct device *dev)
{
	return driver_powerup(dev->driver, dev, DPM_POWER_ON);
}

int device_powerdown(struct device *dev)
{
	return driver_powerdown(dev->driver, dev, DPM_POWER_OFF);
}

EXPORT_SYMBOL(validate_constraints);
EXPORT_SYMBOL(device_powerup);
EXPORT_SYMBOL(device_powerdown);
#endif /* linux-pm */

EXPORT_SYMBOL(device_suspend);
EXPORT_SYMBOL(device_resume);
EXPORT_SYMBOL(device_shutdown);
