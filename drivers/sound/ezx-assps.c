/*
 *  linux/drivers/sound/ezx-assps.c, assp interface for the ezx platform
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
#include "../char/ezx-button.h"

//#define DEBUG	
#undef DEBUG	

static DECLARE_MUTEX(cotulla_assp_mutex);

int codec_output_gain=7, codec_output_path=A2_OUTPUT;	/* A2,loudspeaker,max gain leve */
int codec_input_gain=0x11, codec_input_path=A5_INPUT;	/* A5,mic,gain=+17db */
int audioonflag = 0;
int micinflag = 0;
int completeflag = -1, headsetflag = NO_HEADSET;

int output_gain_matrix[][8] =
{
        { 0, 2, 4, 5, 6, 7, 8, 9 },     // stereo gain, max gain is 3db(PCAP 2.8) 
        { 4, 5, 6, 7, 8, 9, 10, 11 },     // loudspk gain, max gain is 9db, each level decrease 3db
#ifdef A768 
        { 2, 3, 4, 5, 6, 7, 8, 9 },    // headset gain, max gain is 3db
#else
	{ 4, 5, 6, 7, 8, 9, 11, 12 },    // headset gain, max gain is 12db
#endif
        { 0, 0, 1, 2, 3, 4, 5, 6 },     // carkit gain, max gain is -6db
        { 0, 2, 4, 5, 6, 7, 8, 9 }      // headjack gain, max gain is 3db
};

static int codec_dac_rate = 44100;	/* default 44k sample rate */
static struct timer_list audio_timer, mic_timer;

EXPORT_SYMBOL(codec_output_gain);
EXPORT_SYMBOL(codec_output_path);
EXPORT_SYMBOL(codec_input_gain);
EXPORT_SYMBOL(codec_input_path);

EXPORT_SYMBOL(headset_change_interrupt_routine);
EXPORT_SYMBOL(headjack_change_interrupt_routine);
EXPORT_SYMBOL(mic_change_interrupt_routine);
EXPORT_SYMBOL(mixer_ioctl);

static int assp_init(void);
static void assp_shutdown(void);
static void change_input_output(void);
static void open_mic_interrupt(void);

/*initialize hardware, assp controller and pcap register*/
static int assp_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned long ssp_pcap_bit_status;
	unsigned long audiostatus;
	unsigned long timeout;

	down(&cotulla_assp_mutex); 
	audioonflag |= DSP_DEVICE;
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
	ASSCR0 = ASSCR0_FRF_PSP | ASSCR0_EDSS | ASSCR0_DSS_16bit;  /* PSP mode, 32bit */
	ASSCR1 = ASSCR1_EBCEI | ASSCR1_SCLKDIR | ASSCR1_SFRMDIR | ASSCR1_TFTH_4;
	ASSPSP = ASSPSP_SFRMWDTH_16 | ASSPSP_SCMODE;	
	ASSCR1 |= ASSCR1_TSRE;	/* enable transmit dma request */
	ASSCR0 |= ASSCR0_SSE;	/* enable assp controller */
	local_irq_restore(flags);

#ifdef DEBUG
	audiostatus = ASSCR0;
	printk("ASSCR0 = 0x%lx\n", audiostatus);
	audiostatus = ASSCR1;
	printk("ASSCR1 = 0x%lx\n", audiostatus);
	audiostatus = ASSPSP;
	printk("ASSPSP = 0x%lx\n", audiostatus);
#endif

#ifdef DEBUG
		printk("setup pcap audio register\n");
#endif
		/* set pcap register, stereo DAC  */
		/* (1) set pcap audio power V2_EN_2(OR WITH V2_EN)  */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);

		/* (2) set bitclk(BCK=3, two_time_slot) pll clock(ST_CLK=0, 13M) NORMAL mode(DIG_AUD_FS =00) */
		ssp_pcap_register_val = PCAP_ST_BCLK_SLOT_2 | PCAP_ST_CLK_PLL_CLK_IN_13M0 | PCAP_DIGITAL_AUDIO_INTERFACE_NORMAL;	//NORMAL mode
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, ssp_pcap_register_val);
		
		/* set stereo sample rate */
		switch(codec_dac_rate)
		{
		   	 case 48000:	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_48K);
					break;

		   	 case 44100:    SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
					break;

		   	 case 32000: 	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_32K);
					break;

		   	 case 24000: 	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_24K);
					break;

		   	 case 22050:	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_22K);
					break;

		   	 case 16000:	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_16K);
					break;

		   	 case 12000: 	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_12K);
					break;

		   	 case 11025:	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_11K);
					break;

		   	 case 8000: 	SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_8K);
					break;
		   	 default:
		   	 		SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
					break;
		}
#ifdef DEBUG
		printk("codec_dac_rate=%d\n", codec_dac_rate);
#endif
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_INV);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_FS_INV);

		/* (3) reset digital filter(DF_RESET_ST_DAC=1) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DF_RESET_ST_DAC);

		/* (4)set  bitclk output(SMB_ST_DAC=0), audio IO=part1(DIG_AUD_IN_ST_DAC=1) */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DIG_AUD_IN_ST_DAC);

		/* (5) enable pcap clk(ST_CLK_EN=1),enable dac(ST_DAC_EN=1)  */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
		mdelay(1);	/* specified enable time according spec */

		/* (6) enable output_path */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   /* disable codec bypass	*/	
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ST_DAC_SW);   /* close stereo switch */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CDC_SW);   /* open telephone codec path into right PGA */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);   /* close PGA_INR into PGA */

		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);   /* enable right PGA */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);   /* enable left PGA */
#ifdef DEBUG
                printk("codec_output_path=%d\n", codec_output_path);		
#endif
	        switch(codec_output_path)
	        {
			 case STEREO_OUTPUT:             
                                         /* set pcap output path register AR_EN=1, AL_EN=1  */
                                         SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
                                         break;    
                          case HEADJACK_OUTPUT:           
					 SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);	/*  right+left-6db output  */
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
                                         break;
                          case A1_OUTPUT:          /* set pcap output path register A1_EN=1 */
					 SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);	/*  right+left-6db output  */
                                         SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);                  
                                         break;
                          case A4_OUTPUT:         /* set pcap output path register A4_EN=1  */
					 SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);	/*  right+left-6db output  */
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);
                                         break;
                          case A2_OUTPUT:
					 SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);	/*  right+left-6db output  */
                                         SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
                                         break;
                          default:
                                          break;
		}
		/* (7) according output_gain and output_path, set the output gain */
		SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);
#ifdef DEBUG
		printk("codec_output_gain=%d\n", codec_output_gain);
		printk("output gain=%d\n", output_gain_matrix[codec_output_path][codec_output_gain]);
#endif	
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
#ifdef DEBUG
		printk(" complete all hardware init \n");
#endif
		up(&cotulla_assp_mutex);
		return 0;
err:
	up(&cotulla_assp_mutex);
	printk("audio panic: ssp don't ready for slave operation!!! ");
	return -ENODEV;	
}

static void assp_shutdown(void)
{
	down(&cotulla_assp_mutex);
	audioonflag &= ~DSP_DEVICE; 
	/* clear ASSP port */
#ifdef DEBUG
	 printk("close assp port\n");
#endif
	ASSCR0 = 0;
	ASSCR1 = 0;
	ASSPSP = 0;
	CKEN &= ~CKEN10_ASSP; 
	GAFR0_U &= 0x00FFFFFF;	/* set gpio28,29,30,31 */ 
	GPDR0 &=0x0FFFFFFF; 	/* set gpio28,29,30,31 input */
#ifdef DEBUG
	 printk("close pcap register\n");
#endif
	/* close pcap output path */
	SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, 0x20000);	
	/* disable PCAP stereo DAC */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);
	up(&cotulla_assp_mutex);
}

void headset_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{}

/*
 *  for stereo headset and mono headset insert, it will take A1I interrupt
 *  in A1I interrupt handler, open MB_ON2 to check if mic connect
 */
#ifdef DEBUG
static int 	i=0;
#endif
void headjack_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int     bdelay = 20;
#ifdef DEBUG
	printk("interrupt i=%d\n", i++);
#endif	
	 del_timer(&audio_timer);         
         init_timer(&audio_timer);           
         audio_timer.function = change_input_output;
         audio_timer.expires = (jiffies + bdelay);
         add_timer(&audio_timer); 
}

static void open_mic_interrupt(void)
{
#ifdef DEBUG
	printk("open mic interrupt \n");
#endif
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);
        udelay(10);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);
}

static void change_input_output(void)
{
	unsigned long	ssp_pcap_bit_status;
	unsigned long	ssp_pcap_register;

#ifdef DEBUG
	printk("enter headjack change interrupt routine \n");
#endif
	/* read pcap register to check if headjack is in */
	ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
	if(ssp_pcap_bit_status==0)	/* headset is in */
	{
#ifdef DEBUG
		printk("headset insert\n");
#endif
		headset_in_handler(0, NULL, NULL);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);	
		micinflag = 1;
		completeflag = 1;
	 	del_timer(&mic_timer);         
	        init_timer(&mic_timer);           
        	mic_timer.function = open_mic_interrupt;
	        mic_timer.expires = (jiffies + 100);
        	add_timer(&mic_timer); 
	}
	else	/* headset is out */
	{
#ifdef DEBUG
 		printk("headset remove\n");
#endif
		headset_out_handler(0, NULL, NULL);
		micinflag = 0;	
		completeflag = 1;
	 	del_timer(&mic_timer);         
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);
	}
}

void mic_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int 	ssp_pcap_bit_status;
#ifdef DEBUG
	printk("enter mic change interrupt routine \n");
#endif
	/* read pcap register to check if headjack is in */
	ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
   	if( ssp_pcap_bit_status == 0 )	/* headjack is in */
	{
		answer_button_handler(0, NULL, NULL);
	}
}

/*
 * Audio Mixer stuff
 */

int mixer_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	int i, j;
	long val;
	unsigned long ssp_pcap_register_val;

	switch(cmd) 
	{
		case SOUND_MIXER_READ_IGAIN:
			/* read pcap igain register */
			 val = codec_input_gain*8;	/* change to 0-0xFF */
#ifdef DEBUG
			printk(" read input gain=%d\n", val);
#endif
			 return put_user(val, (long *)arg);
			
		case SOUND_MIXER_WRITE_IGAIN:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			val = (val & 0xFF)/8;	/* val maybe 0-255, change to 0-31 */
#ifdef DEBUG
			printk(" write input gain=%d\n", val);
#endif
			codec_input_gain = val;
			SSP_PCAP_AUDIG_set(codec_input_gain);	
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_OGAIN:
			/* read pcap ogain register  */
			val = codec_output_gain;	
#ifdef DEBUG
			printk(" read output gain=%d\n", val);
#endif
			return put_user(val, (long *)arg);
	
		case SOUND_MIXER_WRITE_VOLUME:			
		case SOUND_MIXER_WRITE_OGAIN:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if((val >= 0)&&(val <=7))
			{
				codec_output_gain = val;
				/* write pcap ogain register */
				SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);	
#ifdef DEBUG
				printk("codec_output_gain=%d\n", codec_output_gain);
				printk("set output gain=%d\n", output_gain_matrix[codec_output_path][codec_output_gain]);
#endif
			}
			else
			{
				ret = -EINVAL; 
#ifdef DEBUG
				printk("value is invalid\n");
#endif
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_RECSRC:
#ifdef DEBUG
			printk(" read input path\n");
#endif
			/* read pcap input status, 0-extmic, 1-A5, 2-A3 */
			val = codec_input_path;
			return put_user(val, (long *)arg);

		case SOUND_MIXER_WRITE_RECSRC:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
#ifdef DEBUG
			printk(" force write input path=%d\n", val);
#endif
			/* close old input path */
			switch(codec_input_path)
			{
					case EXTMIC_INPUT:
						/* close external mic path */
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
						break;

					case A3_INPUT:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);
						break;

					case A5_INPUT:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON1);
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);
						break;
					default:
						break;
			}
			/* open input path  */
			switch(val)
			{
					case EXTMIC_INPUT:
						codec_input_path = val;
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
						break;

					case A3_INPUT:
						codec_input_path = val;
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);	
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	
						break;

					case A5_INPUT:
						codec_input_path = val;
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON1);
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	
						break;
					default:
						ret = -EINVAL;
						break;
			}
#ifdef DEBUG
                SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
                printk("pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_OUTSRC:
#ifdef DEBUG
			printk(" read output path\n");
#endif
			val = codec_output_path;
			return put_user(val, (long *)arg);

		case SOUND_MIXER_WRITE_OUTSRC:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
#ifdef DEBUG
			printk("force write output path=%d\n", val);
#endif
			/* close old output path */
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
			/* set pcap output register */
			switch(val)
			{
					case STEREO_OUTPUT:		
						   /* set pcap output path register AR_EN=1, AL_EN=1  */
						   if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
								SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
						   else
								SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
						   codec_output_path = val;
						   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
						   SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);	
						   break;		
					case HEADJACK_OUTPUT:	
						   if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
                                                   else
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
						   codec_output_path = val;
						   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
						   SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);	
						   break;
					case A1_OUTPUT:		 /* set pcap output path register A1_EN=1 */
						   if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
                                                   else
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
						   codec_output_path = val;
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);							 
						   SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);	
						   break;	
					case A4_OUTPUT:		/* set pcap output path register A4_EN=1  */
						   if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
                                                   else
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
						   codec_output_path = val;
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);
						   SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);	
	  					   break;
					case A2_OUTPUT:
						   if((audioonflag & DSP_DEVICE)==DSP_DEVICE)
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
                                                   else
                                                                SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
						   codec_output_path = val;
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
						   SSP_PCAP_AUDOG_set(output_gain_matrix[codec_output_path][codec_output_gain]);	
						   break;
					default:
						ret = -EINVAL;
						break;	
			}
#ifdef DEBUG
                SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
                printk("pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
#endif
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_MUTE:	/* mute output path */
			/* 0-unmute, 1-mute */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if(val == 1)	/* if mute  */
			{
#ifdef DEBUG
			   	printk(" mute PGA\n");
#endif
				   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
				   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
			}
			else if(val == 0)	/*  unmute  */
			{
#ifdef DEBUG
				printk("unmute PGA\n");
#endif
				   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
				   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
			}
			else
			{
				ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_INPUTMUTE:	/* mute input path  for DAI test */
			/* 0-unmute, 1-mute */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if(val == 1)	/* if mute  */
			{
#ifdef DEBUG
			   	printk(" mute input\n");
#endif
				switch(codec_input_path)
				{
					case EXTMIC_INPUT:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
						break;
					case A3_INPUT:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);
						break;
					case A5_INPUT:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);
						break;
					default:
						break;
				}
			}
			else if(val == 0)	/*  unmute  */
			{
#ifdef DEBUG
				printk("unmute input\n");
#endif
				switch(codec_input_path)
				{
					case EXTMIC_INPUT:
					    ret=SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
						break;
					case A3_INPUT:
					    ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);
						break;
					case A5_INPUT:
					    ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);
						break;
					default:
						break;
				}
			}
			else
			{
				ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_LOOPBACK:  /* set loopback mode for DAI test */
			/* 0-unloopback, 1-loopback */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if(val == 1)	
			{
#ifdef DEBUG
			    printk("loopback\n");
#endif
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   
			    	
			}
			else if(val ==0)
			{
#ifdef DEBUG
			    printk("unloopback\n");
#endif
			    SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   
			}
			else
			{
			    ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_AUDOHPF:	/* set audio output High Pass filter for test command */
			/* 0-disable filter, 1-enable filter */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if(val == 1)	
			{
#ifdef DEBUG
			    printk("enable audio output High Pass filter\n");
#endif
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDOHPF);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);   
			    	
			}
			else if(val ==0)
			{
#ifdef DEBUG
			    printk("disable audio output High Pass filter\n");
#endif
			    SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDOHPF);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);   
			}
			else
			{
			    ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_AUDIHPF:	/* set audio input High Pass filter for test command */
			/* 0-disable filter, 1-enable filter */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if(val == 1)	
			{
#ifdef DEBUG
			    printk("enable audio input High Pass filter\n");
#endif
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDIHPF);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);   
			    	
			}
			else if(val ==0)
			{
#ifdef DEBUG
			    printk("disable audio input High Pass filter\n");
#endif
			    SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDIHPF);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);   
			}
			else
			{
			    ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);
		
		case SOUND_MIXER_READ_HEADSET_STATUS:	/* read if headset is in */
#ifdef DEBUG
			printk("check if headset in\n");
#endif
			if((completeflag == -1)||(completeflag == 1))
			{
				completeflag = 0;
				j = 0;
				for(i=0; i<3; i++)
				{
					val = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
#ifdef DEBUG	
					printk("i=%d, A1SNS=%d\n", i, val);
#endif		
					if(val==0)
					    j++;
					set_current_state(TASK_INTERRUPTIBLE);
				        schedule_timeout( (50 * HZ) / 1000);
				}
				if(j>=2)
				{ 	
	                      		headsetflag = STEREO_HEADSET;
			    	        micinflag = 1;
		  		        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);	
				        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);
				}
				else
				{
        		                headsetflag = NO_HEADSET;
					micinflag = 0;	
					SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);
				}
			}
			ret = headsetflag;
			return put_user(ret, (long *)arg);

		default:
			return -EINVAL;
	}
	return 0;

}

static struct file_operations mixer_fops = {
	ioctl:		mixer_ioctl,
	owner:		THIS_MODULE
};

/*
 * ASSP codec ioctls
 */

static int assp_ioctl(struct inode *inode, struct file *file,
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
		if(file->f_mode & FMODE_WRITE)
		{
		   if(val) /* write only support stereo mode */
		   	ret = 1;
		   else  /* not support mono mode */		
		   	ret = -EINVAL;
		}
		else
		{
		   	ret = -EINVAL;
		}
		return put_user(ret, (int *) arg);

	case SNDCTL_DSP_CHANNELS:
	case SOUND_PCM_READ_CHANNELS:
#ifdef DEBUG
		printk(" check if 2 channels \n");
#endif
		if(file->f_mode & FMODE_WRITE)
		{
			return put_user(2, (long *) arg);
		}
		else
		{
			ret = -EINVAL;
			return put_user(ret, (long *) arg);	
		}

	case SNDCTL_DSP_SPEED:
#ifdef DEBUG
		printk(" set sample frequency \n");
#endif
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;

		if(file->f_mode & FMODE_WRITE)
		{
		   down(&cotulla_assp_mutex);
		   ASSCR0 &= ~ASSCR0_SSE; 

		   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
		   switch(val)
		   {
		   	 case 48000:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_48K);
						 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 44100: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 32000: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_32K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 24000: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_24K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 22050:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_22K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 16000:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_16K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 12000: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_12K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 11025:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_11K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;

		   	 case 8000: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_8K);
							 /* set pcap dac sample rate  */
							codec_dac_rate = val;
							break;
		   	 default:
					ret = -EINVAL;
 					break;	
		   }
		   /* reset digital filter(DF_RESET_ST_DAC=1)   */
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DF_RESET_ST_DAC);

		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);	
#ifdef DEBUG
			printk("DA sample freq = %d\n", codec_dac_rate);
#endif
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
		   up(&cotulla_assp_mutex);
		}
		else
		{
			ret = -EINVAL;
		}
		return put_user(ret, (long *) arg);

	case SOUND_PCM_READ_RATE:
		if (file->f_mode & FMODE_WRITE)
		{
#ifdef DEBUG
			printk("read DA sample freq\n");
#endif
			val = codec_dac_rate;
		}
		else
		{
			val = -EINVAL;
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

static audio_stream_t assp_audio_out = {
	name:			"assp audio out",
	dcmd:			DCMD_TXASSDR,
	drcmr:			&DRCMRTXASSDR,  /* ASSP dma map register */
	dev_addr:		__PREG(ASSDR),
};

static audio_stream_t assp_audio_in = {
	name:			"assp audio in",
	dcmd:			DCMD_RXASSDR,
	drcmr:			&DRCMRRXASSDR,  /* ASSP dma map register */
	dev_addr:		__PREG(ASSDR),
};

static audio_state_t assp_audio_state = {
	output_stream:		&assp_audio_out,
	input_stream:		&assp_audio_in,
	client_ioctl:		assp_ioctl,
	hw_init:			assp_init,
	hw_shutdown:		assp_shutdown,
	sem:			__MUTEX_INITIALIZER(assp_audio_state.sem),
};

static int assp_audio_open(struct inode *inode, struct file *file)
{
#ifdef DEBUG
	printk("assp audio open \n");
#endif
	return cotulla_audio_attach(inode, file, &assp_audio_state);
}

/*
 * Missing fields of this structure will be patched with the call
 * to cotulla_audio_attach().
 */

static struct file_operations assp_audio_fops = {
	open:		assp_audio_open,
	owner:		THIS_MODULE
};

static int mixer_dev_id;

static int __init cotulla_assp_init(void)
{
	assp_audio_state.dev_dsp = register_sound_dsp(&assp_audio_fops, -1);
	mixer_dev_id = register_sound_mixer(&mixer_fops, -1);
#ifdef DEBUG
	printk("cotulla-assp-init ok\n");
#endif
	return 0;
}

static void __exit cotulla_assp_exit(void)
{
	unregister_sound_dsp(assp_audio_state.dev_dsp);
	unregister_sound_mixer(mixer_dev_id);
#ifdef DEBUG
	printk("cotulla-assp-exit ok\n");
#endif
}


module_init(cotulla_assp_init);
module_exit(cotulla_assp_exit);









