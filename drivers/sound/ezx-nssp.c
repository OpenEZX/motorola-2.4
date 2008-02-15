/*
 * linux/drivers/sound/ezx-nssp.c
 *
 * nssp interface for the ezx platform
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
 *  Jun 20,2002 - (Motorola) created
 *  Sep 19,2002 - (Motorola) according code review meeting minutes.
 *  Oct 30,2002 - (Motorola) according new requirement for VA.
 *  Nov 05,2002 - (Motorola) according code review meeting minutes.
 *  Mar 04,2003 - (Motorola) (1) don't close headset interrupt;
 *                           (2) when headset in, output gain decrease 6db 
 *  Sep 23,2003 - (Motorola) (1)Port from EZX; (2)Modify the NSSP port inital
 *  Jan 02,2004 - (Motorola) (1) Port from UDC e680 kernel of jem vob.
 *                           (2) Move audio driver DEBUG macro definition to ezx-audio.h
 *                               header file,and redefine DEBUG to EZX_OSS_DEBUG
 *                           (3) reorganize file header
 *  Feb.23,2004 - (Motorola) add e680 audio path switch and gain setting funcs
 *  Mar.15,2004 - (Motorola) mixer bug fix
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


static DECLARE_MUTEX(cotulla_nssp_mutex);

static int nssp_init(void);
static void nssp_shutdown(void);


/*initialize hardware, nssp controller and pcap register*/
static int nssp_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned int ssp_pcap_bit_status;
	volatile unsigned long audiostatus;
	unsigned long timeout;

	down(&cotulla_nssp_mutex); 

	ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);
	if( MONODEVOPENED == 0 )
		pcap_use_ap_13m_clock();
	audioonflag |= AUDIO_DEVICE;
	
#ifdef EZX_OSS_DEBUG
	printk("setup nssp controller register \n");
#endif
	local_irq_save(flags);
	CKEN |= CKEN3_NSSP;	                                /* need enable cken3 */

	set_GPIO_mode(GPIO_BITCLK_IN_NSSP_MD);                  /* BitCLK */
	set_GPIO_mode(GPIO_SYNC_IN_NSSP_MD);                    /* FS */
	/*At E680,the NSSP TX and RX pins are single direction  */
//	set_GPIO_mode(GPIO_SDATA_OUT_NSSP_MD);		            /* TXD ssp2 ALF out 2 */
	set_GPIO_mode(GPIO_SDATA_IN_NSSP_MD);	                /* RXD ssp2 ALF in 1*/
	/* setup nssp port */
	NSSCR0 = NSSCR0_FRF_PSP | NSSCR0_DSS_16bit;             /* PSP mode, 16bit */
	NSSCR1 = NSSCR1_TTE | NSSCR1_EBCEI | NSSCR1_SCLKDIR | NSSCR1_SFRMDIR | NSSCR1_TFTH_4 | NSSCR1_RFTH_14;
	NSSPSP = NSSPSP_SFRMWDTH_1 | NSSPSP_STRTDLY_1 |  NSSPSP_SFRMP_HIGH | NSSPSP_SCMODE;	
	NSSCR1 |= NSSCR1_TSRE | NSSCR1_RSRE;	                /* enable dma request */
	NSSCR0 |= NSSCR0_SSE;	                                /* enable nssp controller */
	local_irq_restore(flags);

#ifdef EZX_OSS_DEBUG
	audiostatus = NSSCR0;
	printk("NSSCR0 = 0x%lx\n", audiostatus);
	audiostatus = NSSCR1;
	printk("NSSCR1 = 0x%lx\n", audiostatus);
	audiostatus = NSSPSP;
	printk("NSSPSP = 0x%lx\n", audiostatus);
#endif

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

	timeout = 0;
	/* check if ssp is ready for slave operation	*/
	while(((audiostatus = NSSSR) & NSSSR_CSS) !=0){
		if((timeout++) > 10000000)
			goto err;
	}
#ifdef EZX_OSS_DEBUG
	printk(" complete all hardware init \n");
#endif
	up(&cotulla_nssp_mutex);
	return 0;

err:
	up(&cotulla_nssp_mutex);
	printk("audio panic: ssp don't ready for slave operation!!! ");
	return -ENODEV;	
}


static void nssp_shutdown(void)
{
	down(&cotulla_nssp_mutex); 
	
	audioonflag &= ~AUDIO_DEVICE;
	if( MONODEVOPENED == 0 )
		pcap_use_bp_13m_clock();
	
#ifdef EZX_OSS_DEBUG
	 printk("close nssp port\n");
#endif
	NSSCR0 = 0;
	NSSCR1 = 0;
	NSSPSP = 0;
	CKEN &= ~CKEN3_NSSP;    /* SSP2 need control the CKEN3  */ 

	set_GPIO_mode(GPIO_NSSP_SCLK2 | GPIO_IN);                  /* BitCLK */
        set_GPIO_mode(GPIO_NSSP_SFRM2 | GPIO_IN);                  /* FS */
        set_GPIO_mode(GPIO_NSSP_TXD2  | GPIO_IN);
        set_GPIO_mode(GPIO_NSSP_RXD2  | GPIO_IN);

#ifdef EZX_OSS_DEBUG
	 printk("close pcap register\n");
#endif

	 up(&cotulla_nssp_mutex);	 
}


/*
 * nssp codec ioctls
 */
static int codec_adc_rate = PHONE_CODEC_DEFAULT_RATE;	/* default 8k sample rate */
static int codec_dac_rate = PHONE_CODEC_DEFAULT_RATE;	/* default 8k sample rate */

static int nssp_ioctl(struct inode *inode, struct file *file,
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
	     if(val)                   /*  not support stereo mode  */
		ret = -EINVAL;
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

		down(&cotulla_nssp_mutex); 
		NSSCR0 &= ~NSSCR0_SSE;	
	        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
		switch(val)
		{
		   	 case PHONE_CODEC_16K_RATE:        /*16K sample rate   */
			      ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);
			      codec_adc_rate = val;	
			      codec_dac_rate = val;	
			      break;
		   	 case PHONE_CODEC_DEFAULT_RATE:    /*8K sample rate  */
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
		
		NSSCR0 |= NSSCR0_SSE;	/* enable nssp controller  */
		timeout = 0;
		/* check if ssp is ready for slave operation	*/
		while(((audiostatus = NSSSR) & NSSSR_CSS) !=0)
		{
			if((timeout++) > 10000000)
			{
				printk("audio panic: can't be slave mode!!!");
				ret =-ENODEV;
				break;
			}
		}
	        up(&cotulla_nssp_mutex); 
#ifdef EZX_OSS_DEBUG
			printk("AD sample freq = %d\n", codec_adc_rate);
			printk("DA sample freq = %d\n", codec_dac_rate);
#endif
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

static audio_stream_t nssp_audio_out = {
	name:			"nssp audio out",
	dcmd:			DCMD_TXNSSDR,
	drcmr:			&DRCMRTXNSSDR,  /* nssp dma map register */
	dev_addr:		__PREG(NSSDR),
};

static audio_stream_t nssp_audio_in = {
	name:			"nssp audio in",
	dcmd:			DCMD_RXNSSDR,
	drcmr:			&DRCMRRXNSSDR,  /* nssp dma map register */
	dev_addr:		__PREG(NSSDR),
};

static audio_state_t nssp_audio_state = {
	output_stream:		&nssp_audio_out,
	input_stream:		&nssp_audio_in,
	client_ioctl:		nssp_ioctl,
	hw_init:			nssp_init,
	hw_shutdown:		nssp_shutdown,
	sem:			__MUTEX_INITIALIZER(nssp_audio_state.sem),
};


static int nssp_audio_open(struct inode *inode, struct file *file)
{
#ifdef EZX_OSS_DEBUG
	printk("nssp audio open \n");
#endif
	return cotulla_audio_attach(inode, file, &nssp_audio_state);
}


/*
 * Missing fields of this structure will be patched with the call
 * to cotulla_audio_attach().
 */


static struct file_operations nssp_audio_fops = {
	open:		nssp_audio_open,
	owner:		THIS_MODULE
};


static int __init cotulla_nssp_init(void)
{
	nssp_audio_state.dev_dsp = register_sound_audio(&nssp_audio_fops, -1);

	set_GPIO_mode(GPIO_NSSP_SCLK2 | GPIO_IN);		/* BitCLK */
	set_GPIO_mode(GPIO_NSSP_SFRM2 | GPIO_IN);		/* FS */
	set_GPIO_mode(GPIO_NSSP_TXD2  | GPIO_IN);
	set_GPIO_mode(GPIO_NSSP_RXD2  | GPIO_IN);		/* RXD ssp2 ALF in 1*/

#ifdef EZX_OSS_DEBUG
	printk("cotulla-nssp-init ok\n");
#endif
	return 0;
}


static void __exit cotulla_nssp_exit(void)
{
	unregister_sound_audio(nssp_audio_state.dev_dsp);
#ifdef EZX_OSS_DEBUG
	printk("cotulla-nssp-exit ok\n");
#endif
}


module_init(cotulla_nssp_init);
module_exit(cotulla_nssp_exit);



