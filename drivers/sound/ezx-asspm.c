/*
 *  linux/drivers/sound/ezx-asspm.c, assp interface for the ezx platform
 *
 *  Copyright (C) 2002 Motorola Inc.
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
/* modified by zq, add PCAP interface header */
#include "../ssp/ssp_pcap.h"  

//#define DEBUG	
#undef DEBUG

static DECLARE_MUTEX(cotulla_assp_mono_mutex);

EXPORT_SYMBOL(set_pcap_telephone_codec);
EXPORT_SYMBOL(set_pcap_input_path);
EXPORT_SYMBOL(set_pcap_output_path);

static int assp_mono_init(void);
static void assp_mono_shutdown(void);

void set_pcap_telephone_codec(int port)
{
	unsigned long ssp_pcap_register_val;

		/* set pcap register, telephone codec */
		/* (1) set pcap audio power V2_EN_2(OR WITH V2_EN) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
	
		/* disable PCAP stereo DAC */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, 0);
	
		/* (2) set codec sample rate(FS_8K_16K=0) */
		/* CDC_CLK(000=13MHZ) bitclk output(SMB=0) audio IO1(DIG_AUD_IN=1) */
	        ssp_pcap_register_val = PCAP_CDC_CLK_IN_13M0;
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, ssp_pcap_register_val);

		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);		/* master */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);	/* 8K sample rate */
		if(port)
			SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN);	/* DAI1 */
		else
			SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN);	/* DAI0 */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDIHPF);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDOHPF);

		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_INV);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_INV);

		/*(3) reset digital filter(DF_RESET=1) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);

		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_ADIT);
		/* (4) enable pcap clk(CDC_CLK_EN=1),enable CODEC(CDC_EN=1)   */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   		
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
		mdelay(1);	/* specified enable time */
		 
		return;
}

void set_pcap_output_path(void)
{
	unsigned long ssp_pcap_bit_status;
		
		/*  enable output_path */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ST_DAC_SW);   /* close stereo switch */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);   /* close PGA_INR into PGA */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CDC_SW);   /* open telephone  switch */

		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);   /* enable right PGA */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);   /* disable left PGA */

		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);	/* right+left output */
#ifdef DEBUG
		printk("codec_output_path = %d\n", codec_output_path);
#endif
		switch(codec_output_path)
                {
                         case STEREO_OUTPUT:
                                         /* set pcap output path register AR_EN=1, AL_EN=1  */
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
                                         break;
                          case HEADJACK_OUTPUT:           
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                         break;
                          case A1_OUTPUT:          /* set pcap output path register A1_EN=1 */
                                         SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);
                                         break;
                          case A4_OUTPUT:         /* set pcap output path register A4_EN=1  */
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);
                                         break;
                          case A2_OUTPUT:
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
                                         break;
                          default:
                                          break;
                }
		/*  according output_gain and output_path, set the output gain */
		SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);
#ifdef DEBUG
		printk("codec_output_gain=%d\n", codec_output_gain);
		printk("output gain=%d\n", output_gain_matrix[codec_output_path][codec_output_gain]);
#endif
		return;
}

void set_pcap_input_path(void)
{
	unsigned int ssp_pcap_bit_status;
	unsigned long ssp_pcap_register_val;
#ifdef DEBUG
        printk("codec_input_path=%d\n", codec_input_path);
#endif
		 switch(codec_input_path)
                 {
                         case EXTMIC_INPUT:
                         	  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
                                  break;

                         case A3_INPUT:
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);
                                  break;

                         case A5_INPUT:
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON1);
                                  SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);
                                  SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);
                                  break;
                         default:
                                  break;
		}
                SSP_PCAP_AUDIG_set(codec_input_gain);
#ifdef DEBUG
                SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
                printk("pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
	return;	
}

/*initialize hardware, assp controller and pcap register*/
static int assp_mono_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned int ssp_pcap_bit_status;
	unsigned int audiostatus;
	unsigned long timeout;

	down(&cotulla_assp_mono_mutex); 
	audioonflag |= DSP16_DEVICE;
//	ssp_pcap_init();
	ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
#ifdef DEBUG
	printk("setup assp controller register \n");
#endif
	local_irq_save(flags);
	CKEN |= CKEN10_ASSP;	/* need enable cken10  */

	set_GPIO_mode(GPIO31_SYNC_IN_ASSP_MD);
	set_GPIO_mode(GPIO30_SDATA_OUT_ASSP_MD);
	set_GPIO_mode(GPIO28_BITCLK_IN_ASSP_MD);
	set_GPIO_mode(GPIO29_SDATA_IN_ASSP_MD);

	/* setup assp port */
	ASSCR0 = ASSCR0_FRF_PSP | ASSCR0_DSS_16bit;  /* psp mode, 16bit */
	ASSCR1 = ASSCR1_TTE | ASSCR1_EBCEI | ASSCR1_SCLKDIR | ASSCR1_SFRMDIR | ASSCR1_RFTH_14 | ASSCR1_TFTH_4;
	ASSPSP = ASSPSP_SFRMWDTH_1 | ASSPSP_STRTDLY_1 | ASSPSP_SFRMP_HIGH | ASSPSP_SCMODE;
	ASSCR1 |= ASSCR1_RSRE | ASSCR1_TSRE;	/* enable transmit and receive dma request */
	ASSCR0 |= ASSCR0_SSE;	/* enable assp controller  */
	local_irq_restore(flags);

#ifdef DEBUG
	audiostatus = ASSCR0;
	printk("ASSCR0 = 0x%lx\n", audiostatus);
	audiostatus = ASSCR1;
	printk("ASSCR1 = 0x%lx\n", audiostatus);
	audiostatus = ASSPSP;
	printk("ASSPSP = 0x%lx\n", audiostatus);
#endif
/* not in phone call, PCAP telephone */
	if((audioonflag & PHONE_DEVICE)==0)	
	{
#ifdef DEBUG		
		printk("setup pcap audio register\n");
#endif
		set_pcap_telephone_codec(1);
		set_pcap_output_path();
		set_pcap_input_path();
	}
	else
	{
#ifdef DEBUG
		printk("there is phone call\n");
#endif
		/* set pcap register, stereo DAC  */
		SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, &ssp_pcap_register_val);
		ssp_pcap_register_val |= PCAP_ST_SAMPLE_RATE_8K | PCAP_ST_BCLK_SLOT_4 | PCAP_ST_CLK_PLL_CLK_IN_BITCLK | PCAP_DIGITAL_AUDIO_INTERFACE_NETWORK;	//NETWORK mode
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, ssp_pcap_register_val);
		
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

#ifdef DEBUG
		SSP_PCAP_read_data_from_PCAP(0x0d,&ssp_pcap_register_val);
		printk("pcap register 13 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
		printk("pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x0b,&ssp_pcap_register_val);
		printk("pcap register 11 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
		printk("pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
		timeout = 0;
		/* check if ssp is ready for slave operation	*/
		while(((audiostatus = ASSSR) & ASSSR_CSS) !=0)
		{
			if((timeout++) > 10000000)
				goto err;
		}

		up(&cotulla_assp_mono_mutex);
#ifdef DEBUG
		printk(" complete all hardware init \n");
#endif
		return 0;

err:
	up(&cotulla_assp_mono_mutex);
	printk("audio panic: ssp don't ready for slave operation!!! ");
	return -ENODEV;	
}

static void assp_mono_shutdown(void)
{
	unsigned long ssp_pcap_register_val;

	down(&cotulla_assp_mono_mutex); 
	/* clear ASSP port */
#ifdef DEBUG
	 printk("close assp port\n");
#endif
	audioonflag &= ~DSP16_DEVICE;
	ASSCR0 = 0;
	ASSCR1 = 0;
	ASSPSP = 0;
	CKEN &= ~CKEN10_ASSP; 
	GAFR0_U &= 0x00FFFFFF;	/* set gpio28,29,30,31 */ 
	GPDR0 &=0x0FFFFFFF; 	/* set gpio28,29,30,31 input */

#ifdef DEBUG
	 printk("close pcap register\n");
#endif
        if((audioonflag & PHONE_DEVICE) == 0)
        {		
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, 0x20000);
		/* close pcap input path */
		if(micinflag)
			SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER, 0x400);	
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
	else
	{
		/* disable PCAP stereo DAC */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);	
	}
#ifdef DEBUG
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
static int codec_adc_rate = 8000;	/* default 8k sample rate */
static int codec_dac_rate = 8000;	/* default 8k sample rate */

static int assp_mono_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
	int ret;
	long val;
	int audiostatus, timeout;

	switch(cmd) {
	case SNDCTL_DSP_STEREO:
#ifdef DEBUG
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
#ifdef DEBUG
		printk(" check if 2 channels \n");
#endif
		return put_user(1, (long *) arg);

	case SNDCTL_DSP_SPEED:
#ifdef DEBUG
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
		   	 case 16000: 
					ret= SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);
					codec_adc_rate = val;	
					codec_dac_rate = val;	
					break;
		   	 case 8000: 
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
				printk("audio panic: can't be slave mode!!!");
				ret = -ENODEV;
				break;
			}
		}
#ifdef DEBUG
		printk("AD sample freq = %d\n", codec_adc_rate);
		printk("DA sample freq = %d\n", codec_dac_rate);
#endif
	        up(&cotulla_assp_mono_mutex); 
		return put_user(ret, (long *) arg);

	case SOUND_PCM_READ_RATE:
		if (file->f_mode & FMODE_WRITE)
		{
#ifdef DEBUG
			printk("read DA sample freq\n");
#endif
			val = codec_dac_rate;
		}
		if (file->f_mode & FMODE_READ)
		{
#ifdef DEBUG
			printk("read AD sample freq\n");
#endif
			val = codec_adc_rate;
		}
		return put_user(val, (long *) arg);

	case SNDCTL_DSP_SETFMT:
	case SNDCTL_DSP_GETFMTS: 
		/* SUPPORT little endian signed 16 */
#ifdef DEBUG
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
	hw_init:			assp_mono_init,
	hw_shutdown:		assp_mono_shutdown,
	sem:			__MUTEX_INITIALIZER(assp_mono_audio_state.sem),
};

static int assp_mono_audio_open(struct inode *inode, struct file *file)
{
#ifdef DEBUG
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
#ifdef DEBUG
	printk("/dev/dsp16 init ok\n");
#endif
	return 0;
}

static void __exit cotulla_assp_mono_exit(void)
{
	unregister_sound_dsp16(assp_mono_audio_state.dev_dsp);
#ifdef DEBUG
	printk("/dev/dsp16 exit ok\n");
#endif
}

module_init(cotulla_assp_mono_init);
module_exit(cotulla_assp_mono_exit);









