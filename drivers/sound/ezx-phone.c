/*
 * linux/drivers/sound/ezx-phone, phone interface for EZX
 * 
 * Copyright (C) 2002 Motorola Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include "ezx-audio.h"
#include "../ssp/ssp_pcap.h"

//#define DEBUG
#undef DEBUG

#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

static DECLARE_MUTEX(phone_mutex);

#ifdef CONFIG_PM
static int phone_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch(req)
	{
		case PM_SUSPEND:
			break;
		case PM_RESUME:
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
#ifdef DEBUG
		printk("bluetooth audio on\n");
#endif
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
#ifdef DEBUG
		SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, &ssp_pcap_register_val);
		printk("AUD_CODEC=0x%x\n", ssp_pcap_register_val);
#endif
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
                switch(codec_output_path)
                {
                        case STEREO_OUTPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
                                  break;
                        case HEADJACK_OUTPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                  break;
                        case A1_OUTPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);
                                  break;
                        case A4_OUTPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);
                                  break;
                        case A2_OUTPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
                                  break;
                        default:
                                  break;
                }
		break;
	case BLUETOOTH_AUDIO_OFF:
#ifdef DEBUG
		printk("bluetooth audio off\n");
#endif
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
#ifdef DEBUG
		SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, &ssp_pcap_register_val);
		printk("AUD_CODEC=0x%x\n", ssp_pcap_register_val);
#endif
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
		switch(codec_output_path)
                {
                        case STEREO_OUTPUT:
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
                                  break;
                        case HEADJACK_OUTPUT:
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                  break;
                        case A1_OUTPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);
                                  break;
                        case A4_OUTPUT:
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);
                                  break;
                        case A2_OUTPUT:
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
                                  break;
                        default:
                                  break;
                }

		break;
	default:
		return mixer_ioctl(inode, file, cmd, arg);
	}
	return 0;
}

/*
 *	there is a suspection:
 *  when run phone_open, other audio device is stopped, pcap is free
*/
static int count = 0;
static int phone_open(struct inode *inode, struct file *file)
{
	unsigned int ssp_pcap_register_val, ssp_pcap_bit_status;

	if(!count)
	{
		count ++;

		down(&phone_mutex);
		audioonflag |= PHONE_DEVICE;
	//	ssp_pcap_init();
		ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);

#ifdef DEBUG
		printk("init pcap register\n");	
#endif
		set_pcap_telephone_codec(0);
		set_pcap_output_path();
		set_pcap_input_path();

#ifdef DEBUG
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
	if(!count)
	{
		down(&phone_mutex);
		audioonflag &= ~PHONE_DEVICE;

		/* close pcap input path */
		if(micinflag)
			SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER, 0x400);	
		else
			SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER, 0);	
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, 0x20000);	
		/* disable PCAP mono codec  */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);

		/* set fsync, tx, bitclk are tri-stated */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CD_TS);
		up(&phone_mutex);

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
#ifdef DEBUG
	printk("register phone device with kernel ok \n");
#endif
	return 0;
}

static void __exit phone_exit(void)
{
	unregister_chrdev(MYPHONE_MAJOR, "phone");
#ifdef DEBUG
	printk("unregister phone device \n");
#endif
}


module_init(phone_init);
module_exit(phone_exit);


