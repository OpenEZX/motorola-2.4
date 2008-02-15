/*
 * linux/drivers/sound/ezx-asspm.c
 *
 * assp interface for the ezx platform
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
 *  Jun 20, 2002 - (Motorola) created
 *  Sep 19, 2002 - (Motorola) according code review meeting minutes.
 *  Oct 30, 2002 - (Motorola) according new requirement for VA.ASSP interface split to
 *                            /dev/dsp (support stereo playback) and /dev/dsp16 (support
 *                            mono playback and record).this file is for mono playback and record
 *  Nov 05, 2002 - (Motorola) according code review meeting minutes.
 *  Mar 04, 2003 - (Motorola) (1) don't close headset interrupt;
 *                            (2) when headset in, output gain decrease 6db
 *  Apr 24, 2003 - (Motorola) no switch for headset insert and remove
 *  Sep 23, 2003 - (Motorola) Port from EZX
 *  Jan 02, 2004 - (Motorola) (1) Port from UDC e680 kernel of jem vob.
 *                            (2) Move audio driver DEBUG macro definition to ezx-audio.h
 *                                header file,and redefine DEBUG to EZX_OSS_DEBUG
 *                            (3) reorganize file header
 *  Jan 13, 2004 - (Motorola) Make the e680 louder speaker work.
 *  Feb.23, 2004 - (Motorola) add e680 audio path switch and gain setting funcs
 *  Mar.15, 2004 - (Motorola) mixer bug fix
 *  Apr.13, 2004 - (Motorola) close dsp protection,and add 3d control interface for app
 *  Jun.08, 2004 - (Motorola) record noise bug fix.
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
#include <linux/sound.h>
#include <linux/soundcard.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "ezx-audio.h"
#include "ezx-common.h"


static DECLARE_MUTEX(cotulla_assp_mono_mutex);

EXPORT_SYMBOL(set_pcap_telephone_codec);
EXPORT_SYMBOL(set_pcap_input_path);
EXPORT_SYMBOL(set_pcap_output_path);

static int assp_mono_init(void);
static void assp_mono_shutdown(void);

void set_pcap_telephone_codec(int port)
{
	unsigned long ssp_pcap_register_val;
        SSP_PCAP_BIT_STATUS phoneClkBit;
	/* set pcap register, telephone codec */
	/* (1) set pcap audio power V2_EN_2(OR WITH V2_EN) */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
	
	/* disable PCAP stereo DAC */
	SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, 0);
	/* (2) set codec sample rate(FS_8K_16K=0) */
	/* CDC_CLK(000=13MHZ) bitclk output(SMB=0) audio IO1(DIG_AUD_IN=1) */
        ssp_pcap_register_val = PCAP_CDC_CLK_IN_13M0;
        phoneClkBit = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
	SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, ssp_pcap_register_val);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);		/* master */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);	/* 8K sample rate */
/*
        if( SSP_PCAP_BIT_ONE == phoneClkBit)
        {
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
        }
        else
        {
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
        }
*/        
	if(port)
        {
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
 		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN);	/* DAI1 */
	}
        else
        {
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN);	/* DAI0 */
        }
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDIHPF);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDOHPF);

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_INV);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_INV);

	/*(3) reset digital filter(DF_RESET=1) */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_ADITH);
	/* (4) enable pcap clk(CDC_CLK_EN=1),enable CODEC(CDC_EN=1)   */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   		
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
	mdelay(1);	/* specified enable time */
}


void set_pcap_output_path(void)
{
	int ret;

	/*  enable output_path */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ST_DAC_SW);   /* close stereo switch */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);   /* close PGA_INR into PGA */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CDC_SW);   /* open telephone  switch */

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);   /* enable right PGA */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);   /* disable left PGA */

	SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);	/* right+left output */

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_output_path = %d\n", codec_output_path);
	printk(EZXOSS_DEBUG "codec_output_base = %d\n", codec_output_base);
#endif
	ret = (*mixer_open_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])(codec_output_base|codec_output_path);
}


void set_pcap_input_path(void)
{
	unsigned long ssp_pcap_register_val;
	
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_input_path=%d\n", codec_input_path);
#endif

	(*mixer_open_input_path[codec_input_path])();
	set_input_gain_hw_reg();
	
#ifdef EZX_OSS_DEBUG
	SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
	printk(EZXOSS_DEBUG "pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
}


/*initialize hardware, assp controller and pcap register*/
static int assp_mono_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned int audiostatus;
	unsigned long timeout;

/*
#ifdef CONFIG_ARCH_EZX_E680
	if( audioonflag & FM_DEVICE ){
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "E680 open dsp16 EBUSY because 0x%X device is using the sound hardware.\n",audioonflag );
#endif
		return -EBUSY;
	}
#endif
*/

	audioonflag |= DSP16_DEVICE;
	
	down(&cotulla_assp_mono_mutex);
		
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "setup assp controller register \n");
#endif
	local_irq_save(flags);
	CKEN |= CKEN4_ASSP;	/* need enable cken4  */

	set_GPIO_mode(GPIO_SYNC_IN_ASSP_MD);
	set_GPIO_mode(GPIO_SDATA_OUT_ASSP_MD);
	set_GPIO_mode(GPIO_BITCLK_IN_ASSP_MD);
	set_GPIO_mode(GPIO_SDATA_IN_ASSP_MD);

	/* setup assp port */
	ASSCR0 = ASSCR0_FRF_PSP | ASSCR0_DSS_16bit;  /* psp mode, 16bit */
	ASSCR1 = ASSCR1_TTE | ASSCR1_EBCEI | ASSCR1_SCLKDIR | ASSCR1_SFRMDIR | ASSCR1_RFTH_14 | ASSCR1_TFTH_4;
	ASSPSP = ASSPSP_SFRMWDTH_1 | ASSPSP_STRTDLY_1 | ASSPSP_SFRMP_HIGH | ASSPSP_SCMODE;
	ASSCR1 |= ASSCR1_RSRE | ASSCR1_TSRE;	/* enable transmit and receive dma request */
	ASSCR0 |= ASSCR0_SSE;	/* enable assp controller  */
	local_irq_restore(flags);

#ifdef EZX_OSS_DEBUG
	audiostatus = ASSCR0;
	printk(EZXOSS_DEBUG "ASSCR0 = 0x%lx\n", audiostatus);
	audiostatus = ASSCR1;
	printk(EZXOSS_DEBUG "ASSCR1 = 0x%lx\n", audiostatus);
	audiostatus = ASSPSP;
	printk(EZXOSS_DEBUG "ASSPSP = 0x%lx\n", audiostatus);
#endif

	mute_output_to_avoid_pcap_noise();
	
	ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
	if( MONODEVOPENED == DSP16_DEVICE )
		pcap_use_ap_13m_clock();

/* not in phone call, PCAP telephone */
	if((audioonflag & PHONE_DEVICE)==0){
#ifdef EZX_OSS_DEBUG		
		printk(EZXOSS_DEBUG "setup pcap audio register\n");
#endif
		set_pcap_telephone_codec(1);
		set_pcap_output_path();
		set_pcap_input_path();
	}
	else{
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "there is phone call\n");
#endif
		/* set pcap register, stereo DAC  */
		//SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, &ssp_pcap_register_val);
		//ssp_pcap_register_val |= PCAP_ST_SAMPLE_RATE_8K | PCAP_ST_BCLK_SLOT_4 | PCAP_ST_CLK_PLL_CLK_IN_BITCLK | PCAP_DIGITAL_AUDIO_INTERFACE_NETWORK;	//NETWORK mode
		//SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, ssp_pcap_register_val);
		SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_8K);
                SSP_PCAP_BCLK_set(PCAP_ST_BCLK_SLOT_4);
                SSP_PCAP_STCLK_set(PCAP_ST_CLK_PLL_CLK_IN_FSYNC);
                SSP_PCAP_DIG_AUD_FS_set(PCAP_DIGITAL_AUDIO_INTERFACE_NETWORK);

		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_INV);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_FS_INV);	

		/* (3) reset digital filter(DF_RESET_ST_DAC=1) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DF_RESET_ST_DAC);

		/* (4)set  bitclk output(SMB_ST_DAC=0), audio IO=part1(DIG_AUD_IN_ST_DAC=1) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);	/* input, slave mode */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DIG_AUD_IN_ST_DAC);

		/* (5) enable pcap clk(ST_CLK_EN=1),enable dac(ST_DAC_EN=1)  */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
		mdelay(1);	/* specified enable time according spec */
		
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ST_DAC_SW);   /* close stereo switch */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);   /* enable left PGA */
	}

	undo_mute_output_to_avoid_pcap_noise();

#ifdef EZX_OSS_DEBUG
	SSP_PCAP_read_data_from_PCAP(0x0d,&ssp_pcap_register_val);
	printk(EZXOSS_DEBUG "pcap register 13 = 0x%lx\n", ssp_pcap_register_val);
	SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
	printk(EZXOSS_DEBUG "pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
	SSP_PCAP_read_data_from_PCAP(0x0b,&ssp_pcap_register_val);
	printk(EZXOSS_DEBUG "pcap register 11 = 0x%lx\n", ssp_pcap_register_val);
	SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
	printk(EZXOSS_DEBUG "pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
	timeout = 0;
	/* check if ssp is ready for slave operation	*/
	while(((audiostatus = ASSSR) & ASSSR_CSS) !=0){
		if((timeout++) > 10000000)
			goto err;
	}
	
	up(&cotulla_assp_mono_mutex);

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " complete all hardware init \n");
#endif
	return 0;

err:
	up(&cotulla_assp_mono_mutex);
	printk(EZXOSS_DEBUG "audio panic2: ssp don't ready for slave operation!!! ");
	return -ENODEV;
}


static void assp_mono_shutdown(void)
{
	unsigned long ssp_pcap_register_val;
	
	down(&cotulla_assp_mono_mutex); 

	/* clear ASSP port */
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close assp port\n");
#endif
	ASSCR0 = 0;
	ASSCR1 = 0;
	ASSPSP = 0;
	CKEN &= ~CKEN4_ASSP; 

	set_GPIO_mode(GPIO_ASSP_SCLK3 | GPIO_IN);             /* Assp Frame sync */
        set_GPIO_mode(GPIO_ASSP_TXD3  | GPIO_IN);
        set_GPIO_mode(GPIO_ASSP_RXD3  | GPIO_IN);           /* ASSP BitCLK  */
        set_GPIO_mode(GPIO_ASSP_SFRM3 | GPIO_IN);            /* ASsp RX */

#ifdef EZX_OSS_DEBUG
	 printk(EZXOSS_DEBUG "close pcap register\n");
#endif

	mute_output_to_avoid_pcap_noise();	/* mute hw noise and save power */
	
	if( MONODEVOPENED == DSP16_DEVICE )
		pcap_use_bp_13m_clock();
	
        if((audioonflag & PHONE_DEVICE) == 0){		
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER,SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
		/* close pcap input path */
		if(micinflag)
                        SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER,SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
		else
		        SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER, 0);	
		/* disable PCAP mono codec */
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
                SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN);	/* DAI0 */
		/* set fsync, tx, bitclk are tri-stated */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CD_TS);
        }
	else{
		/* disable PCAP stereo DAC */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);	
	}

#ifdef CONFIG_ARCH_EZX_E680
	e680_boomer_path_mono_lineout();	/* mute hw noise and save power for e680 */
#endif

	audioonflag &= ~DSP16_DEVICE;
	
#ifdef EZX_OSS_DEBUG
	SSP_PCAP_read_data_from_PCAP(0x0d,&ssp_pcap_register_val);
	printk("pcap register 13 = 0x%lx\n", ssp_pcap_register_val);
	SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
	printk("pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
	SSP_PCAP_read_data_from_PCAP(0x0b,&ssp_pcap_register_val);
	printk("pcap register 11 = 0x%lx\n", ssp_pcap_register_val);
	SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
	printk("pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
	up(&cotulla_assp_mono_mutex);
}


/*
 * ASSP codec ioctls
 */
static int codec_adc_rate = PHONE_CODEC_DEFAULT_RATE;	/* default 8k sample rate */
static int codec_dac_rate = PHONE_CODEC_DEFAULT_RATE;	/* default 8k sample rate */

static int assp_mono_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	int ret;
	long val;
	int audiostatus, timeout;

	switch(cmd) {
	case SNDCTL_DSP_STEREO:
#ifdef EZX_OSS_DEBUG
		printk(" check if support stereo\n");
#endif
		ret = get_user(val, (int *) arg);
		if (ret)
			return ret;
	
		if(val) 
			ret = -EINVAL;	/* not support stereo */
		else  
		   	ret = 1;
		return put_user(ret, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
#ifdef EZX_OSS_DEBUG
		printk(" check if 2 channels \n");
#endif
		return put_user(1, (long *) arg);

	case SNDCTL_DSP_SPEED:
#ifdef EZX_OSS_DEBUG
		printk(" set sample frequency \n");
#endif
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;

	    down(&cotulla_assp_mono_mutex); 
	    ASSCR0 &= ~ASSCR0_SSE; 
	    SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
	    SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
	    switch(val)
		{
		   	 case PHONE_CODEC_16K_RATE: 
					ret= SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);
					codec_adc_rate = val;	
					codec_dac_rate = val;	
					break;
		   	 case PHONE_CODEC_DEFAULT_RATE: 
					ret = SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);
					codec_adc_rate = val;	
					codec_dac_rate = val;	
					break;
		   	 default:
					ret = -EINVAL;
 					break;	
		}	
		/* reset digital filter(DF_RESET=1)  */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);	
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
		
		ASSCR0 |= ASSCR0_SSE;	/* enable assp controller  */
		timeout = 0;
		/* check if ssp is ready for slave operation	*/
		while(((audiostatus = ASSSR) & ASSSR_CSS) !=0)
		{
			if((timeout++) > 10000000)
			{
				printk("audio panic3: can't be slave mode!!!");
				ret = -ENODEV;
				break;
			}
		}
#ifdef EZX_OSS_DEBUG
		printk("AD sample freq = %d\n", codec_adc_rate);
		printk("DA sample freq = %d\n", codec_dac_rate);
#endif
	        up(&cotulla_assp_mono_mutex); 
		return put_user(codec_adc_rate, (long *) arg);

	case SOUND_PCM_READ_RATE:
		if (file->f_mode & FMODE_WRITE)
		{
#ifdef EZX_OSS_DEBUG
			printk("read DA sample freq\n");
#endif
			val = codec_dac_rate;
		}
		if (file->f_mode & FMODE_READ)
		{
#ifdef EZX_OSS_DEBUG
			printk("read AD sample freq\n");
#endif
			val = codec_adc_rate;
		}
		return put_user(val, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS: 
		/* SUPPORT little endian signed 16 */
#ifdef EZX_OSS_DEBUG
		printk("data format is AFMT_S16_LEd\n");
#endif
		return put_user(AFMT_S16_LE, (long *) arg); 
		
	default:
		return mixer_ioctl(inode, file, cmd, arg);
	}
	return 0;
}


/*
 * Audio stuff
 */
static audio_stream_t assp_mono_audio_out = {
	name:			"assp mono audio out",
	dcmd:			DCMD_TXASSDRM,
	drcmr:			&DRCMRTXASSDR,  /* ASSP dma map register */
	dev_addr:		__PREG(ASSDR),
};

static audio_stream_t assp_mono_audio_in = {
	name:			"assp mono audio in",
	dcmd:			DCMD_RXASSDR,
	drcmr:			&DRCMRRXASSDR,  /* ASSP dma map register */
	dev_addr:		__PREG(ASSDR),
};

static audio_state_t assp_mono_audio_state = {
	output_stream:		&assp_mono_audio_out,
	input_stream:		&assp_mono_audio_in,
	client_ioctl:		assp_mono_ioctl,
	hw_init:		assp_mono_init,
	hw_shutdown:		assp_mono_shutdown,
	sem:			__MUTEX_INITIALIZER(assp_mono_audio_state.sem),
};

static int assp_mono_audio_open(struct inode *inode, struct file *file)
{
#ifdef EZX_OSS_DEBUG
	printk("assp mono audio open \n");
#endif

	return cotulla_audio_attach(inode, file, &assp_mono_audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to cotulla_audio_attach().
 */

static struct file_operations assp_mono_audio_fops = {
	open:		assp_mono_audio_open,
	owner:		THIS_MODULE
};

static int __init cotulla_assp_mono_init(void)
{
	assp_mono_audio_state.dev_dsp = register_sound_dsp16(&assp_mono_audio_fops, -1);

#ifdef EZX_OSS_DEBUG
	printk("/dev/dsp16 init ok\n");
#endif
	return 0;
}

static void __exit cotulla_assp_mono_exit(void)
{
	unregister_sound_dsp16(assp_mono_audio_state.dev_dsp);
#ifdef EZX_OSS_DEBUG
	printk("/dev/dsp16 exit ok\n");
#endif
}

module_init(cotulla_assp_mono_init);
module_exit(cotulla_assp_mono_exit);

