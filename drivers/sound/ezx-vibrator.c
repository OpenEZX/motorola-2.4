/*
 * linux/drivers/sound/vibrator.c
 *
 *
 *  Description: vibrator interface for EZX. for application can't direct call interface in kernel space from
 *               user space.so vibrator still realize a char device.
 *
 *
 * (c) Copyright Motorola 2003, All rights reserved.
 *
 *
 *  History:
 *  zhouqiong          Jun 20,2002             created
 *  Kin Wong           Nov 05,2003             Renamed ezx-vibrator.h to vibrator.h 
 *  Jin Lihong(w20076) Jan 02,2004             (1) Port from UDC e680 kernel of jem vob.
 *                                             (2) Move audio driver DEBUG macro definition to ezx-audio.h
 *                                                 header file,and redefine DEBUG to EZX_OSS_DEBUG
 *                                             (3) reorganize file header
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  reorganise file header
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/fs.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "../misc/ssp_pcap.h"
#include "ezx-vibrator.h"


#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

static int active=0;
static int voltage=0;

static int vibrator_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	int ret;
	long val;

	switch(cmd)
	{
	case VIBRATOR_ENABLE:
#ifdef EZX_OSS_DEBUG
		printk("enable vibrator \n");
#endif
		ret = get_user(val, (int *) arg);
		if (ret)
			return ret;
		switch(val)
		{
		    case 0:
#ifdef EZX_OSS_DEBUG
			printk("vibrator level 0 \n");
#endif
			ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL0);
			ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL0);
			active = 1;
			voltage = 0;
			SSP_vibrate_start_command();
			break;
		    case 1:
#ifdef EZX_OSS_DEBUG
			printk("vibrator level 1 \n");
#endif
		    	ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL1);
		    	ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL1);
			active = 1;
			voltage = 1;
			SSP_vibrate_start_command();
			break;
		    case 2:
#ifdef EZX_OSS_DEBUG
			printk("vibrator level 2 \n");
#endif
		    	ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL2);
		    	ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL2);
			active = 1;
			voltage = 2;
			SSP_vibrate_start_command();
			break;
		    case 3:
#ifdef EZX_OSS_DEBUG
			printk("vibrator level 3 \n");
#endif
		    	ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL3);
		    	ret = SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL3);
			active = 1;
			voltage = 3;
			SSP_vibrate_start_command();
			break;
		    default:
#ifdef EZX_OSS_DEBUG
			printk("vibrator level error \n");
#endif
			return -EINVAL;
		}
		return put_user(ret, (int *) arg);

	case VIBRATOR_DISABLE:
#ifdef EZX_OSS_DEBUG
		printk("disable vibrator \n");
#endif
		ret = 0;
		active = 0;
  		SSP_vibrate_stop_command();
		return put_user(ret, (int *) arg);
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_PM
static int vibrator_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch(req)
	{
		case PM_SUSPEND:
			if(active)
			{
	  			SSP_vibrate_stop_command();
			}
			break;
		case PM_RESUME:
			if(active)
			{
				switch(voltage)
				{
				    case 0:
					SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL0);
					SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL0);
					break;
				    case 1:
				        SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL1);
				        SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL1);
					break;
				    case 2:
			    	        SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL2);
			    	        SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL2);
					break;
				    case 3:
				        SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL3);
				        SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL3);
					break;
				    default:
					break;
				}
				SSP_vibrate_start_command();
			}
			break;
	}
	return 0;
}
#endif

static int count=0;
static int vibrator_open(struct inode *inode, struct file *file)
{
	if(!count)
	{
		count ++;
		//ssp_pcap_init();
		ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
#ifdef CONFIG_PM
		pm_dev = pm_register(PM_SYS_DEV, 0, vibrator_pm_callback);
#endif
#ifdef EZX_OSS_DEBUG
		printk("open vibrator \n");        
#endif
		return 0;
	}
	else
		return -EBUSY;
}

static int vibrator_release(struct inode *inode, struct file *file)
{
	count --;
	if(!count)
	{
#ifdef CONFIG_PM
		pm_unregister(pm_dev);
#endif
#ifdef EZX_OSS_DEBUG
		printk("close vibrator \n");        
#endif
	}
	return 0;
}


static struct file_operations device_fops = {
    owner:		THIS_MODULE,
	open:		vibrator_open,
	release:	vibrator_release,
	ioctl:		vibrator_ioctl,
};

static int __init vibrator_init(void)
{
	int ret;
#ifdef EZX_OSS_DEBUG
	printk("enter vibrator init...\n");
#endif
	ret = register_chrdev(VIBRATOR_MAJOR,"vibrator", &device_fops);
	if(ret){
		printk("can't register vibrator device with kernel");
		}
#ifdef EZX_OSS_DEBUG
	printk("vibrator init ok\n");
#endif
	return 0;
}

static void __exit vibrator_exit(void)
{
	unregister_chrdev(VIBRATOR_MAJOR, "vibrator");
#ifdef EZX_OSS_DEBUG
	printk("vibrator exit ok\n");
#endif
	return;
}


module_init(vibrator_init);
module_exit(vibrator_exit);


