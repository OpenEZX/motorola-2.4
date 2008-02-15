/*
 * linux/drivers/sound/ezx-a780.c
 *
 * Motorola a780 phone specific functions implementation for audio drivers
 *
 * Copyright (C) 2004-2005 Motorola
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
 *  Jan 13,2004 - (Motorola) Created, Make the e680 louder speaker work.
 *  Feb.23,2004 - (Motorola) add e680 audio path switch and gain setting funcs
 *  Mar.15,2004 - (Motorola) mixer bug fix
 *  Mar.25,2004 - (Motorola) a780 new gain setting interface
 *  Apr.13,2004 - (Motorola) close dsp protection,and add 3d control interface for app
 *  Jun.22,2004 - (Motorola) mixer power save
 *  Jun.24,2004 - (Motorola) Add EMU PIHF carkit sound path
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
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


#ifdef CONFIG_ARCH_EZX_A780

extern u32 gpio_hw_attenuate_a780_status;


void close_input_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close input carkit. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}


#if EMU_PIHF_FEATURE
void close_input_pihf_carkit(void)
{
	printk("EMU:%s,%s\n",__FILE__,__FUNCTION__);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}
#endif /* EMU_PIHF_FEATURE */


void close_input_handset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close input handset. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON1);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);
}


void close_input_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close input headset. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);
}


void open_input_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open input carkit. \n");
#endif

	codec_input_path = CARKIT_INPUT;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}


#if EMU_PIHF_FEATURE
void open_input_pihf_carkit(void)
{
	printk("EMU:%s,%s\n",__FILE__,__FUNCTION__);
	codec_input_path = PIHF_CARKIT_INPUT;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}
#endif /* EMU_PIHF_FEATURE */


void open_input_handset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open input handset. \n");
#endif

	codec_input_path = HANDSET_INPUT;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON1);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	
}


void open_input_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open input headset. \n");
#endif

	codec_input_path = HEADSET_INPUT;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	
}


void close_output_pcap_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap headset. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
}


void close_output_pcap_louderspeaker(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap louderspeaker. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
}


void close_output_pcap_earpiece(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap earpiece. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);
}


void close_output_pcap_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap carkit. \n");
#endif
	EIHF_Mute(EIHF_MUTE);              //Mute EIHF 

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	
	Set_EMU_Mux_Switch(DEFAULT_USB_MODE);
}


#if EMU_PIHF_FEATURE
void close_output_pcap_pihf_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap pihf carkit. \n");
	printk("EMU:%s,%s\n",__FILE__,__FUNCTION__);
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
}
#endif /* EMU_PIHF_FEATURE */


void close_output_pcap_headjack(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap headjack. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
}


void close_output_pcap_bluetooth(void)
{
}


int open_output_pcap_headset(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap headset. \n");
#endif

	if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	codec_output_path = val;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	set_output_gain_hw_reg();

	return ret;
}


int open_output_pcap_louderspeaker(long val)
{
	int ret;
    
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap louderspeaker. \n");
#endif

	if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	codec_output_path = val;
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
	set_output_gain_hw_reg();
    
	return ret;
}


int open_output_pcap_earpiece(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap earpiece. \n");
#endif

	if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	codec_output_path = val;
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);							 
	set_output_gain_hw_reg();

	return ret;
}


int open_output_pcap_carkit(long val)
{
	int ret;
 
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap carkit. \n");
#endif

	Set_EMU_Mux_Switch(MONO_AUDIO_MODE);
    
	SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);

	codec_output_path = val;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);					
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);

	set_output_gain_hw_reg();

	EIHF_Mute(EIHF_UNMUTE);
    
	return ret;
}


#if EMU_PIHF_FEATURE
int open_output_pcap_pihf_carkit(long val)
{
	int ret;
 
	printk("EMU:%s,%s\n",__FILE__,__FUNCTION__);
	SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);

	codec_output_path = val;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);					
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);

	set_output_gain_hw_reg();

	return ret;
}
#endif /* EMU_PIHF_FEATURE */


int open_output_pcap_headjack(long val)
{
	int ret;
    
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap headjack. \n");
#endif

	if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	codec_output_path = val;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	set_output_gain_hw_reg();

	return ret;
}


int open_output_pcap_bluetooth(long val)
{
	return 0;
}


void set_output_gain_hw_reg(void)
{
	SSP_PCAP_AUDOG_set( PCAP_OUTPUT_GAIN_REG_VAL_FROM_LOGIC );

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_output_gain=%d\n", codec_output_gain);
	printk(EZXOSS_DEBUG "output gain=%d\n",PCAP_OUTPUT_GAIN_REG_VAL_FROM_LOGIC);
#endif
}


void set_input_gain_hw_reg(void)
{
	SSP_PCAP_AUDIG_set( PCAP_INPUT_AUDIG_REG_VAL_FROM_LOGIC ); 

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_input_gain=%d\n", codec_input_gain);
#endif
}


void poweron_mixer( audio_dev_type type )
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "A780 No. 0x%X device wants to power on the mixer hardware.\n", type);
	printk(EZXOSS_DEBUG "A780 No. 0x%X device has already powered on the mixer hardware.\n", audioonflag);
#endif

	audioonflag |= type;
	if( audioonflag == type )
	{
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "A780 No. 0x%X device is powering on the mixer hardware.\n", type);
#endif

		ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
		
		/* (1) set pcap audio power V2_EN_2(OR WITH V2_EN)  */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
		
		/* (6) enable output_path and set gain */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   /* disable codec bypass	*/	
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ST_DAC_SW);   /* close stereo switch */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CDC_SW);   /* open telephone codec path into right PGA */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);   /* close PGA_INR into PGA */

		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);   /* enable right PGA */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);   /* enable left PGA */
	}
}


void shutdown_mixer( audio_dev_type type )
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "A780 No. 0x%X device wants to shut down the mixer hardware.\n", type);
#endif

	audioonflag &= ~type;

	if( audioonflag == 0 )
	{
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "A780 No. 0x%X device is shutting down the mixer hardware.\n", type);
#endif
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
	}
	
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "A780 No. 0x%X device is still using the mixer hardware.\n", audioonflag);
#endif
}


void mixer_not_in_use(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " A780 mixer not in use.\n");
#endif

	(*mixer_close_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])();
	(*mixer_close_input_path[codec_input_path])();
	
	if( micinflag == 0 )	/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
}


u32 gpio_hw_attenuate_a780_status;
void use_hw_noise_attenuate(void)
{
	set_GPIO_mode(GPIO_HW_ATTENUATE_A780 | GPIO_OUT);
	clr_GPIO(GPIO_HW_ATTENUATE_A780);
	PGSR(GPIO_HW_ATTENUATE_A780) &= ~GPIO_bit(GPIO_HW_ATTENUATE_A780);
        gpio_hw_attenuate_a780_status = 0;
#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "set a780 hw noise attenuation gpio low. \n");
#endif
}


void bypass_hw_noise_attenuate(void)
{
	set_GPIO_mode(GPIO_HW_ATTENUATE_A780 | GPIO_OUT);
	set_GPIO(GPIO_HW_ATTENUATE_A780);
	PGSR(GPIO_HW_ATTENUATE_A780) |= GPIO_bit(GPIO_HW_ATTENUATE_A780);
        gpio_hw_attenuate_a780_status = 1;
#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "set a780 hw noise attenuation gpio high. \n");
#endif
}


void pcap_use_ap_13m_clock(void)
{
	OSCC |= 0x00000008;
	set_GPIO_mode(AP_13MHZ_OUTPUT_PIN | GPIO_ALT_FN_3_OUT);

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_CLK_IN_SEL);	
}


void pcap_use_bp_13m_clock(void)
{
	OSCC &= ~0x00000008;
	set_GPIO_mode(AP_13MHZ_OUTPUT_PIN | GPIO_IN);

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_CLK_IN_SEL);
}


#ifdef CONFIG_PM
int mixer_hw_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch(req){
		case PM_SUSPEND:
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " A780 before AP sleep.\n");
#endif
			if( (audioonflag & PHONE_DEVICE) == 0  )
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AUDIO_LOWPWR);
			break;
		case PM_RESUME:
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " A780 after AP sleep.\n");
#endif
			SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AUDIO_LOWPWR);
			set_output_gain_hw_reg();

			if(gpio_hw_attenuate_a780_status)
				bypass_hw_noise_attenuate();
			else
				use_hw_noise_attenuate();
			if(audioonflag)
				(*mixer_open_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])(codec_output_base|codec_output_path);

			break;
	}
	return 0;
}
#endif


void mute_output_to_avoid_pcap_noise(void)
{
}


void undo_mute_output_to_avoid_pcap_noise(void)
{
}


#endif


