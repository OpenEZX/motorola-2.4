/*
 * linux/drivers/sound/ezx-phone
 *
 *  Phone interface for EZX. for application can't direct call interface in 
 *  kernel space from user space.so phone controll realize a char device.
 *
 * Copyright (C) 2002-2005 Motorola
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 *  Aug 02,2002 - (Motorola) created
 *  Mar 03,2003 - (Motorola) (1) don't close headset interrupt;
 *                           (2) move bluetooth to phone device ioctl
 *  Sep 23,2003 - (Motorola) Port from EZX
 *  Jan 02,2004 - (Motorola) (1) Port from UDC e680 kernel of jem vob.
 *                           (2) Move audio driver DEBUG macro definition to ezx-audio.h
 *                               header file,and redefine DEBUG to EZX_OSS_DEBUG
 *                           (3) reorganize file header
 *  Jan 13,2004 - (Motorola) Make the e680 louder speaker work.
 *  Mar.15,2004 - (Motorola) mixer bug fix
 *  Mar.25,2004 - (Motorola) play music noise tmp solution
 *  Mar.25,2004 - (Motorola) a780 new gain setting interface
 *  Apr.20,2004 - (Motorola) va of a phone call for e680
 *  Apr.24,2004 - (Motorola) a780 bitclock restore to normal because pcap2 upgraded
 *
 */
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "ezx-common.h"


#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

static DECLARE_MUTEX(phone_mutex);

#ifdef CONFIG_PM
#ifdef CONFIG_ARCH_EZX_E680
extern u32 gpio_va_sel_status;
#endif


static int phone_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch(req)
	{
		case PM_SUSPEND:
			break;
		case PM_RESUME:
#ifdef CONFIG_ARCH_EZX_E680
                    set_GPIO_mode(GPIO_VA_SEL_BUL | GPIO_OUT);
                    if(gpio_va_sel_status)
                    {
                        set_GPIO(GPIO_VA_SEL_BUL);
                    }
                    else
                    {
	                clr_GPIO(GPIO_VA_SEL_BUL);
                    }
#endif
			break;
	}
	return 0;
}
#endif


void bluetoothaudio_open(void)
{}
void bluetoothaudio_close(void)
{}

static int phone_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	unsigned long ssp_pcap_register_val;

	switch(cmd) {
		case BLUETOOTH_AUDIO_ON:
#ifdef EZX_OSS_DEBUG
		printk("bluetooth audio on\n");
#endif
              SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
#ifdef EZX_OSS_DEBUG
		SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, &ssp_pcap_register_val);
		printk("AUD_CODEC=0x%x\n", ssp_pcap_register_val);
#endif
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
              SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
              (*mixer_close_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])();
		break;
		
	case BLUETOOTH_AUDIO_OFF:
#ifdef EZX_OSS_DEBUG
		printk("bluetooth audio off\n");
#endif
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
#ifdef EZX_OSS_DEBUG
		SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, &ssp_pcap_register_val);
		printk("AUD_CODEC=0x%x\n", ssp_pcap_register_val);
#endif
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
              SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
		(*mixer_open_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])(codec_output_base|codec_output_path );
		break;
		
	default:
		return mixer_ioctl(inode, file, cmd, arg);
	}
	return 0;
}


/*
 *  there is a supposition:
 *  when run phone_open, other audio device is stopped, pcap is free
 */
static int count = 0;
static int phone_open(struct inode *inode, struct file *file)
{
	unsigned int ssp_pcap_register_val;

/*
#ifdef CONFIG_ARCH_EZX_E680
	if( audioonflag & FM_DEVICE ){
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "E680 open phone EBUSY because 0x%X device is using the sound hardware.\n",audioonflag );
#endif
		return -EBUSY;
	}
#endif
*/

	if(!count){	
		count ++;

		down(&phone_mutex);

		mute_output_to_avoid_pcap_noise();
		audioonflag |= PHONE_DEVICE;
		ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
		pcap_use_bp_13m_clock();


#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "open phone device, init pcap register\n");	
#endif
		set_pcap_telephone_codec(0);
		set_pcap_output_path();
		set_pcap_input_path();

		undo_mute_output_to_avoid_pcap_noise();

#ifdef EZX_OSS_DEBUG
		SSP_PCAP_read_data_from_PCAP(0x0b, &ssp_pcap_register_val);
                printk("reg 11=0x%x\n", ssp_pcap_register_val);
                SSP_PCAP_read_data_from_PCAP(0x0d, &ssp_pcap_register_val);
                printk("reg 13=0x%x\n", ssp_pcap_register_val);
                SSP_PCAP_read_data_from_PCAP(0x0c, &ssp_pcap_register_val);
                printk("reg 12=0x%x\n", ssp_pcap_register_val);
                SSP_PCAP_read_data_from_PCAP(0x1a, &ssp_pcap_register_val);
                printk("reg 26=0x%x\n", ssp_pcap_register_val);
#endif
		up(&phone_mutex);

#ifdef CONFIG_ARCH_EZX_E680
		set_gpio_va_sel_out_low();
#endif

#ifdef CONFIG_PM
		pm_dev = pm_register(PM_SYS_DEV, 0, phone_pm_callback);
#endif
		return 0;
      }
      else
		return -EBUSY;
}


static int phone_release(struct inode *inode, struct file *file)
{
	count --;
	if(!count){
		down(&phone_mutex);
		
		mute_output_to_avoid_pcap_noise();
		
		/* close pcap input path */
		if(micinflag)
			SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER,SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);	
		else
			SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER, 0);	
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER,SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);	
		/* disable PCAP mono codec  */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);

		/* set fsync, tx, bitclk are tri-stated */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CD_TS);

		up(&phone_mutex);

#ifdef CONFIG_ARCH_EZX_E680
		set_gpio_va_sel_out_high();
		e680_boomer_path_mono_lineout();	/* mute hw noise and save power for e680 */
#endif

		pcap_use_bp_13m_clock();
		audioonflag &= ~PHONE_DEVICE;

#ifdef CONFIG_PM
		pm_unregister(pm_dev);
#endif
	}
	
	return 0;
}


static struct file_operations device_fops = {
    owner:		THIS_MODULE,
	open:		phone_open,
	release:	phone_release,
	ioctl:		phone_ioctl,
};

static int __init phone_init(void)
{
	int ret;
	ret = register_chrdev(MYPHONE_MAJOR,"phone", &device_fops);
	if(ret)
	{
		printk("can't register phone device with kernel");
	}
#ifdef EZX_OSS_DEBUG
	printk("register phone device with kernel ok \n");
#endif
	return 0;
}

static void __exit phone_exit(void)
{
	unregister_chrdev(MYPHONE_MAJOR, "phone");
#ifdef EZX_OSS_DEBUG
	printk("unregister phone device \n");
#endif
}


module_init(phone_init);
module_exit(phone_exit);


