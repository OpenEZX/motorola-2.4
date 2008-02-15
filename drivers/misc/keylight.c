/*
 *  linux/drivers/misc/keylight.c --- key backlight driver on ezx
 *
 *  Support for the Motorola Ezx A780 Development Platform.
 *
 *  Author:     Jay Jia
 *  Created:    Feb 16, 2004
 *  Copyright:  Motorola Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include "keylight.h"
#include "ssp_pcap.h"
#include <linux/delay.h>

static int keylight_open_cnt = 0;
static int main_current_brightness = 0xf;
static int aux_current_brightness = 0x1f;

#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

static int keylight_open(struct inode *inode,struct file *file)
{
  	keylight_open_cnt++;

	MOD_INC_USE_COUNT;
   	return 0;
}

static int keylight_close(struct inode *inode,struct file *file)
{
  	keylight_open_cnt--;
  	
	if ( keylight_open_cnt == 0 )
	{	
	  	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL0);
  		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL1);
	  	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL2);
  		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL3);
	  	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL4);
  		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL0);
	  	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL1);
  		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL2);
	  	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL3);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL4);
	}
	
  	MOD_DEC_USE_COUNT;
  	return 0;
}

static void keylight_main_turn_off(void)
{
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL0);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL1);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL2);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL3);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL4);
}

static void keylight_aux_turn_off(void)
{
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL0);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL1);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL2);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL3);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL4);
}

static void keylight_main_turn_on(void)
{
	int i,count;
	
        for( i=0; i<5; i++)
	{
		count = 1<<i;
		switch ( main_current_brightness & count )
                {
                case 1:
	                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL0);
	                break;
	        case 2:
	        	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL1);
			break;
		case 4:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL2);
		        break;
		case 8:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL3);
		        break;
		case 16:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL_CTRL4);
		        break;
		default:
		        break;
		}
	}
}

static void keylight_aux_turn_on(void)
{
        int i,count;

	for( i=0; i<5; i++)
	{
		count = 1<<i;
		switch ( aux_current_brightness & count )
		{
		case 1:
			SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL0);
			break;
		case 2:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL1);
		        break;
		case 4:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL2);
		        break;
		case 8:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL3);
		        break;
		case 16:
		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_PERIPH_BL2_CTRL4);
		        break;
		default:
		        break;
		}
	}
}

static int keylight_ioctl(struct inode *inode,struct file *file,unsigned int cmd,unsigned long arg)
{
        int ret = 0;

	switch( cmd )
	{
	case KEYLIGHT_MAIN_ON:
		keylight_main_turn_on();
		break;
	case KEYLIGHT_MAIN_OFF:
		keylight_main_turn_off();
                break;
	case KEYLIGHT_MAIN_SETTING:
		main_current_brightness = arg & 0x1f;
                break;
        case KEYLIGHT_AUX_ON:
		keylight_aux_turn_on();
                break;
        case KEYLIGHT_AUX_OFF:
		keylight_aux_turn_off();
                break;
        case KEYLIGHT_AUX_SETTING:
		ret = -EINVAL;
                break;
	default:
		break;		
	}	
	
  	return ret;
}

#ifdef CONFIG_PM
static int ezx_keylight_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
       	switch(req)
       	{
        case PM_SUSPEND:
            break;
        case PM_RESUME:
            break;
        default:
            break;
	}
	return 0;
}
#endif

static struct file_operations keylight_fops={
  	owner:		THIS_MODULE,
  	ioctl:		keylight_ioctl,
  	open:		keylight_open,
  	release:	keylight_close,
};

static struct miscdevice keylight_dev = {
        KEYLIGHT_MINOR,
        "keylight",
        &keylight_fops,
};

int init_keylight(void)
{
  	int ret;

  	ret = misc_register(&keylight_dev);
  	if( ret < 0 ){
    		printk("Sorry,registering the keylight device failed.\n");
    		return ret;
  	}
#ifdef CONFIG_PM
        pm_dev = pm_register(PM_SYS_DEV, 0, ezx_keylight_pm_callback);
#endif
#if 0	
	for(ret=0;ret<32;ret++)
	{
		aux_current_brightness=ret;
		keylight_aux_turn_on();
		main_current_brightness=ret;
		keylight_main_turn_on();
		mdelay(2000);
	}
#endif
	return 0;
}
void clean_keylight(void)
{
	misc_deregister(&keylight_dev);
#ifdef CONFIG_PM
        pm_unregister(pm_dev);
#endif	    
}

module_init(init_keylight);
module_exit(clean_keylight);






