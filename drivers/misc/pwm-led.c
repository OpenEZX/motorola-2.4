/*
 * linux/drivers/misc/pwm-led.c --- backlight leds  driver on ezx
 *
 * Copyright (C) 2004-2006 Motorola
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

/*
 * Author   Date            Comment
 * ======   ======          ==========================
 * Motorola 02/16/2004      Initially written for support EZX platform
 * Motorala 12/21/2005      Add Macau support
 * Motorola 06/15/2006      Add Penglai support
 * Motorola 10/25/2006      update interface for poweric driver
 *
 * Support for the Motorola Ezx Development Platform.
 * For Hainan and Sumatra product
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
#include <linux/delay.h>

#include "pwm-led.h"
#include "linux/power_ic_kernel.h"

static int pwmled_open_cnt = 0;
#if defined ( CONFIG_HAINAN_LED ) || ( CONFIG_SUMATRA_LED ) 
static int default_0_brightness = 0x0;
static int default_1_brightness = 0x0;
#endif

static int pwmled_open(struct inode *inode,struct file *file)
{
  	pwmled_open_cnt++;
	MOD_INC_USE_COUNT;
   	return 0;
}

static int pwmled_close(struct inode *inode,struct file *file)
{
  	pwmled_open_cnt--;
  	MOD_DEC_USE_COUNT;
  	return 0;
}

static void pwmled_1_turn_on(void)
{
#ifdef CONFIG_HAINAN_LED
	fl_reg_write((POWER_IC_REG_FL_LS1-POWER_IC_REG_FL_FIRST_REG),0xfc);
#endif
#ifdef CONFIG_SUMATRA_LED
	int i,count;
	for( i=0; i<5; i++)
	{
		count = 1<<i;
		switch ( default_1_brightness & count )
		{
		case 1:
			power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,20,1);
			break;
		case 2:
	       		 power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,21,1);
	       	 	break;
		case 4:
        		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,22,1);
        		break;
		case 8:
        		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,23,1);
        		break;
		case 16:
        		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,24,1);
        		break;
		default:
	       		 break;
		}
	}
#endif
}

static void pwmled_1_turn_off(void)
{
#ifdef CONFIG_HAINAN_LED
	fl_reg_write((POWER_IC_REG_FL_LS1-POWER_IC_REG_FL_FIRST_REG),0x00);
#endif
#ifdef CONFIG_SUMATRA_LED	
       	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,20,0);
       	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,21,0);
       	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,22,0);
       	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,23,0);
       	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,24,0);
#endif
}

static void pwmled_0_turn_on(void)
{
#ifdef CONFIG_HAINAN_LED
	fl_reg_write((POWER_IC_REG_FL_LS0-POWER_IC_REG_FL_FIRST_REG),0x2a);
#endif
#ifdef CONFIG_SUMATRA_LED	
	int i,count;
	for( i=0; i<5; i++)
	{
		count = 1<<i;
		switch ( default_0_brightness & count )
		{
		case 1:
			power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,0,1);
			break;
		case 2:
	       		 power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,1,1);
	       	 	break;
		case 4:
        		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,2,1);
        		break;
		case 8:
        		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,3,1);
        		break;
		case 16:
        		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,4,1);
        		break;
		default:
       		 	break;
		}
	}
#endif	
#if defined(CONFIG_MACAU_LED) || defined(CONFIG_PENGLAI_LED)
        set_GPIO(GPIO_EL_EN);
#endif	
}

static void pwmled_0_turn_off(void)
{
#ifdef CONFIG_HAINAN_LED
	fl_reg_write((POWER_IC_REG_FL_LS0-POWER_IC_REG_FL_FIRST_REG),0x00);
#endif
#ifdef CONFIG_SUMATRA_LED
	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,0,0);
	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,1,0);
	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,2,0);
	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,3,0);
	power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,4,0);
#endif	
#if defined(CONFIG_MACAU_LED) || defined(CONFIG_PENGLAI_LED)
        clr_GPIO(GPIO_EL_EN);
#endif	
}

static int pwmled_ioctl(struct inode *inode,struct file *file,unsigned int cmd,unsigned long arg)
{
#ifdef CONFIG_SUMATRA_LED
	int i,count;
#endif
	switch( cmd )
	{
	case PWMLED_0_ON:
		pwmled_0_turn_on();
		break;
	case PWMLED_0_OFF:
		pwmled_0_turn_off();
               		 break;
	case PWMLED_0_SETTING:
#ifdef CONFIG_HAINAN_LED
//		fl_reg_write((POWER_IC_REG_FL_LS0-POWER_IC_REG_FL_FIRST_REG),0x2a);
		default_0_brightness = arg & 0xff;
		fl_reg_write((POWER_IC_REG_FL_PSC0-POWER_IC_REG_FL_FIRST_REG),default_0_brightness);
#endif
#ifdef CONFIG_SUMATRA_LED
		default_0_brightness = arg & 0x1f;
		for( i=0; i<5; i++)
		{
			count = 1<<i;
			switch ( default_0_brightness & count )
			{
			case 1:
				power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,0,1);
				break;
			case 2:
		       		 power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,1,1);
		       	 	break;
			case 4:
		       		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,2,1);
		       		break;
			case 8:
		       		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,3,1);
		       		break;
			case 16:
		       		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,4,1);
		       		break;
			default:
		       		 break;
			}
		}
		
#endif		
                		break;
       	case PWMLED_1_ON:
		pwmled_1_turn_on();
            	break;
       	case PWMLED_1_OFF:
		pwmled_1_turn_off();
                break;
       	case PWMLED_1_SETTING:
#ifdef CONFIG_HAINAN_LED
//		fl_reg_write((POWER_IC_REG_FL_LS1-POWER_IC_REG_FL_FIRST_REG),0xfc);
		default_1_brightness = arg & 0xff;
		fl_reg_write((POWER_IC_REG_FL_PSC1-POWER_IC_REG_FL_FIRST_REG),default_1_brightness);
#endif
#ifdef CONFIG_SUMATRA_LED
		default_1_brightness = arg & 0x1f;
		for( i=0; i<5; i++)
		{
			count = 1<<i;
			switch ( default_1_brightness & count )
			{
			case 1:
				power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,20,1);
				break;
			case 2:
		       		 power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,21,1);
		       	 	break;
			case 4:
		       		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,22,1);
		       		break;
			case 8:
		       		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,23,1);
		       		break;
			case 16:
		       		power_ic_set_reg_bit(POWER_IC_REG_PCAP_PERIPH,24,1);
		       		break;
			default:
		       		 break;
			}
		}
#endif		
                break;
	default:
		break;		
	}	
	
  	return 0;
}

static struct file_operations pwmled_fops={
  	owner:	THIS_MODULE,
  	ioctl:	pwmled_ioctl,
  	open:	pwmled_open,
  	release:pwmled_close,
};

static struct miscdevice pwmled_dev = {
       	PWMLED_MINOR,
       	"pwmled",
       	&pwmled_fops,
};


#if defined(CONFIG_MACAU_LED) || defined(CONFIG_PENGLAI_LED)
static struct pm_dev *pwdled_pm_dev;

static int pwdled_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
        switch (req) {
        case PM_SUSPEND: 
               /* VAUX4 should be in low power mode during phone standby  */
  		power_ic_set_reg_bit( POWER_IC_REG_PCAP_LOWPWR_CTRL , 5 , 1 );	
                break;
        case PM_RESUME: 
               /* VAUX4 should be in normal power mode during phone resume  */
 	        power_ic_set_reg_bit( POWER_IC_REG_PCAP_LOWPWR_CTRL , 5 , 0 );	
                break;
        }
        return 0;
}
#endif /*  CONFIG_MACAU_LED */


int init_pwmled(void)
{
  	int ret;

  	ret = misc_register(&pwmled_dev);
  	if( ret < 0 ){
    		printk("Sorry,registering the pwmled device failed.\n");
    		return ret;
  	}
#if defined(CONFIG_MACAU_LED) || defined(CONFIG_PENGLAI_LED)
	/* Actually Macau uses EL instead of LED. 
	   EL is controlled by GPIO 89 and powered by AUX4.
	 */
	power_ic_set_reg_value(POWER_IC_REG_PCAP_AUX_VREG, 13, 2, 2);
	power_ic_set_reg_bit(POWER_IC_REG_PCAP_AUX_VREG ,12, 1);
	set_GPIO_mode(GPIO_EL_EN | GPIO_OUT);
        set_GPIO(GPIO_EL_EN);
	pwdled_pm_dev = pm_register(PM_UNKNOWN_DEV, PM_SYS_UNKNOWN, pwdled_pm_callback);
#endif	
	return 0;
}
void clean_pwmled(void)
{
	misc_deregister(&pwmled_dev);
#if defined(CONFIG_MACAU_LED) || defined(CONFIG_PENGLAI_LED)
	pm_unregister(pwdled_pm_dev);
#endif

}

module_init(init_pwmled);
module_exit(clean_pwmled);






