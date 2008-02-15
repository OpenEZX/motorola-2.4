/*
 *  linux/drivers/sound/ezx-assp.c, assp interface for the ezx platform
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

#undef DEBUG	

static DECLARE_MUTEX(cotulla_assp_mutex);
/*static int cotulla_assp_refcount; */

int codec_output_gain=0xf, codec_output_path=A2_OUTPUT;	/* A2,loudspeaker */
int codec_input_gain=0x15, codec_input_path=A5_INPUT;	/* A5,mic */
int rwflag=0;

static void assp_timeout_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs);

EXPORT_SYMBOL(codec_output_gain);
EXPORT_SYMBOL(codec_output_path);
EXPORT_SYMBOL(codec_input_gain);
EXPORT_SYMBOL(codec_input_path);

static void assp_write_init(void);
static void assp_read_init(void);
static void assp_init(void);
static void assp_shutdown(void);

/*initialize hardware, assp controller and pcap register*/
static void assp_init(void)
{
	down(&cotulla_assp_mutex); 
//	ssp_pcap_init();
	ssp_pcap_open(SSP_PCAP_AUDIO_OPEN);

#ifdef DEBUG
	printk("init assp and pcap hardware\n");
#endif

/* if write , open stereo    */
	if(rwflag ==WRITEFLAG)
	{
		assp_write_init();
	}
/* if read , open mono   */	
	else if(rwflag==READFLAG)
	{
		assp_read_init();
	}		
	 up(&cotulla_assp_mutex);
}

static void assp_write_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned int ssp_pcap_bit_status;
	volatile unsigned long audiostatus;
	unsigned long timeout;

#ifdef DEBUG
	printk("setup assp controller register \n");
#endif
	local_irq_save(flags);
	CKEN |= CKEN9_ASSP | CKEN10_NSSP;	/* need enable cken9 and cken10  */

	set_GPIO_mode(GPIO31_SYNC_IN_ASSP_MD);
	set_GPIO_mode(GPIO30_SDATA_OUT_ASSP_MD);
	set_GPIO_mode(GPIO28_BITCLK_IN_ASSP_MD);
	set_GPIO_mode(GPIO29_SDATA_IN_ASSP_MD);

	/* setup assp port */
		ASSCR0 = 0;
		ASSCR1 = 0;
		ASSPSP = 0;
		ASSCR0 = ASSCR0_FRF_PSP | ASSCR0_EDSS | ASSCR0_DSS_16bit;  /* PSP mode, 32bit */
		ASSCR1 = ASSCR1_EBCEI | ASSCR1_SCLKDIR | ASSCR1_SFRMDIR | ASSCR1_TFTH_4;
		ASSPSP = ASSPSP_SFRMWDTH_16 | ASSPSP_SCMODE;	
		ASSCR1 |= ASSCR1_TSRE;	/* enable dma request */
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

		/* (2) set stereo dac sample rate(44K) bitclk(BCK=3, two_time_slot) pll clock(ST_CLK=0, 13M) NORMAL mode(DIG_AUD_FS =00) */
		ssp_pcap_register_val = PCAP_ST_SAMPLE_RATE_44K | PCAP_ST_BCLK_SLOT_2 | PCAP_ST_CLK_PLL_CLK_IN_13M0 | PCAP_DIGITAL_AUDIO_INTERFACE_NORMAL;	//NORMAL mode
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, ssp_pcap_register_val);

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

		/* (6) according output_gain set the output gain */
		SSP_PCAP_AUDOG_set(codec_output_gain);	

		/* (7) enable output_path */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   /* disable codec bypass	*/	
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ST_DAC_SW);   /* close stereo switch */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CDC_SW);   /* open telephone codec path into right PGA */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);   /* close PGA_INR into PGA */

		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);   /* enable right PGA */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);   /* enable left PGA */

		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_STDET_EN);	/* enable stereo detection circuit */
		/*read pcap register to check if headset is in */
		ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_MSTB);
#ifdef DEBUG
		printk("stereo status=%d\n", ssp_pcap_bit_status);
#endif
		if(( ssp_pcap_bit_status == 0 ))	/* headset is in */
		{
#ifdef DEBUG
			printk("headset is in\n");
#endif
		   SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);	/* stereo output */		
		   codec_output_path = STEREO_OUTPUT;

		   /* set pcap output path AR and AL	   */
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN); 
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN); 	
		}
		else 
		{
#ifdef DEBUG
		   printk("headset is out\n");
		   printk("codec_output_path=%d\n", codec_output_path);
#endif
		   SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);	/* right+left output */
		   switch(codec_output_path)
		   {
			case A1_OUTPUT:		 /* set pcap output path register A1_EN=1  */
					SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);
				   	break;	
			case A4_OUTPUT:	/* set pcap output path register A4_EN=1 */
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  	
					break;
			default:
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
					codec_output_path = A2_OUTPUT;	
		   }
		}
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_STI);		/* clear interrupt  */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_STM);		/* unmask interrupt  */
 
#ifdef DEBUG
		SSP_PCAP_read_data_from_PCAP(0x0d,&ssp_pcap_register_val);
		printk("pcap register 13 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
		printk("pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x0b,&ssp_pcap_register_val);
		printk("pcap register 11 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x1b,&ssp_pcap_register_val);
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
		return;
err:
	printk("audio panic: ssp don't ready for slave operation!!! ");
	return;	
}

static void assp_read_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned int ssp_pcap_bit_status;
	volatile unsigned int audiostatus;
	unsigned long timeout;

#ifdef DEBUG
	printk("setup assp controller register \n");
#endif
	local_irq_save(flags);
	CKEN |= CKEN9_ASSP | CKEN10_NSSP;	/* need enable cken9 and cken10  */

	set_GPIO_mode(GPIO31_SYNC_IN_ASSP_MD);
	set_GPIO_mode(GPIO30_SDATA_OUT_ASSP_MD);
	set_GPIO_mode(GPIO28_BITCLK_IN_ASSP_MD);
	set_GPIO_mode(GPIO29_SDATA_IN_ASSP_MD);

		/* setup assp port */
		ASSCR0 = 0;
		ASSCR1 = 0;
		ASSPSP = 0;
		ASSCR0 = ASSCR0_FRF_PSP | ASSCR0_DSS_16bit;  /* spi mode, 16bit */
		ASSTO = ASSTO_1S;   /* timeout is 1s */
		ASSCR1 = ASSCR1_TTE | ASSCR1_SCLKDIR | ASSCR1_SFRMDIR | ASSCR1_TINTE |ASSCR1_RFTH_14;
		ASSPSP = ASSPSP_SFRMWDTH_16 | ASSPSP_SFRMP_HIGH | ASSPSP_SCMODE;
		ASSCR1 |= ASSCR1_RSRE;	/* enable receive dma request */
		ASSCR0 |= ASSCR0_SSE;	/* enable assp controller  */
		local_irq_restore(flags);
#ifdef DEBUG		
		printk("setup pcap audio register\n");
#endif
		/* set pcap register, telephone codec */
		/* (1) set pcap audio power V2_EN_2(OR WITH V2_EN) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_V2_EN_2);
	
		/* (2) set codec sample rate(FS_8K_16K=0) */
		/* CDC_CLK(000=13MHZ) bitclk output(SMB=0) audio IO1(DIG_AUD_IN=1) enalbe audio input high pass filter(AUDIHPF=1) */
	    ssp_pcap_register_val = PCAP_CDC_CLK_IN_13M0 | SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN;
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, ssp_pcap_register_val);

		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);		/* master */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);	/* 8K sample rate */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDIHPF);

		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_INV);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_INV);	

		/*(3) reset digital filter(DF_RESET=1) */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);

		/* (4) enable pcap clk(CDC_CLK_EN=1),enable CODEC(CDC_EN=1)   */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);   		
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
		mdelay(1);	/* specified enable time */

		/*(5) according input_gain and output_gain set the input and output gain */
		SSP_PCAP_AUDIG_set(codec_input_gain);

		/* (6) enable input_path, check if external mic is in */
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);	/*open MIC_BIAS2  */
		/* read pcap register to check if headset is in  */
		ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_MB2SNS);
#ifdef DEBUG
		printk("external mic status=%d\n", ssp_pcap_bit_status);
#endif
		if(( ssp_pcap_bit_status == 1 ))	/* external mic connect */
		{
#ifdef DEBUG
		   printk("external mic in\n");
#endif
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);	/* enable external mux */
		   codec_input_path = EXTMIC_INPUT;
		}
		else
		{
#ifdef DEBUG
		   printk("external mic out\n");
		   printk("codec_input_path=%d\n", codec_input_path);
#endif
			if(codec_input_path == A3_INPUT)	/* A3 path */
			{
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	/* enable A3  */
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	/* enable A3_mux	*/	
			}
			else		/* default A5 path  */
			{
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	/* enable A5 */
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	/* enable A5_mux */
				codec_input_path == A5_INPUT;
			}
		}
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);		/* clear MIC_BIAS2  interrupt */
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);		/* unmask MIC_BIAS2  interrupt */

#ifdef DEBUG
		SSP_PCAP_read_data_from_PCAP(0x0d,&ssp_pcap_register_val);
		printk("pcap register 13 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
		printk("pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x0b,&ssp_pcap_register_val);
		printk("pcap register 11 = 0x%lx\n", ssp_pcap_register_val);
		SSP_PCAP_read_data_from_PCAP(0x1b,&ssp_pcap_register_val);
		printk("pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
		timeout = 0;
		/* check if ssp is ready for slave operation	*/
		while(((audiostatus = ASSSR) & ASSSR_CSS) !=0)
		{
			if((timeout++) > 10000000)
				goto err;
		}
	/* request timeout irq, irq 15  */
#ifdef DEBUG
		printk("request assp timeout interrupt \n");
#endif
		request_irq(IRQ_ASSP, assp_timeout_interrupt_routine, 0, "assp timeout irq ",NULL);
#ifdef DEBUG
		printk(" complete all hardware init \n");
#endif
		return;
err:
	printk("audio panic: ssp don't ready for slave operation!!! ");
	return;	
}

static void assp_shutdown(void)
{
#ifdef DEBUG
	printk("close interrupt\n");
#endif
	down(&cotulla_assp_mutex); 
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_STM);	/* mask interrupt external headset detect */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_STI);	/* clear interrupt */

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);	/* mask interrupt external mic detect */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);	/* clear interrupt */
	
	/* clear ASSP port */
#ifdef DEBUG
	 printk("close assp port\n");
#endif
	ASSCR0 = 0;
	ASSCR1 = 0;
	CKEN &= ~( CKEN9_ASSP | CKEN10_NSSP ); 
	GAFR0_U &= 0x00FFFFFF;	/* set gpio28,29,30,31 */ 
	GPDR0 &=0x0FFFFFFF; 	/* set gpio28,29,30,31 input */
	if( rwflag==READFLAG )
		free_irq(IRQ_ASSP, NULL);
#ifdef DEBUG
	 printk("close pcap register\n");
#endif
	if(rwflag == WRITEFLAG)
	{
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER, 0);	
		/* disable PCAP stereo DAC */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, 0);	
	}
	else if(rwflag == READFLAG)
	{
		/* close pcap input path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, 0);
		/* disable PCAP mono codec */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, 0);	
	}
	 up(&cotulla_assp_mutex);
}

static int old_output_path=A2_OUTPUT;
void headset_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int	ssp_pcap_bit_status;

#ifdef DEBUG
	printk("enter headset change interrupt routine \n");
#endif
		/* read pcap register to check if headset is in */
		ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_MSTB);
	
		if(( ssp_pcap_bit_status == 0 ))	/* headset is in */
		{
#ifdef DEBUG
	 		printk("headset insert\n");
#endif
			/* close old output path */
			    old_output_path = codec_output_path;
				switch(old_output_path)
				{
					case A1_OUTPUT:		 /* set pcap output path register A1_EN=0 */
						    SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
							SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);		
						   break;	
					case A4_OUTPUT:	/* set pcap output path register A4_EN=0 */
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  					   break;
					default:
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
				}

				/* open stereo output path */
			    codec_output_path = STEREO_OUTPUT;
				SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);	
				/* set pcap output path AR and AL	 */  
			   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
			   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN); 
			   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN); 
		}
		else   /*  headset out */
		{
#ifdef DEBUG
	 		printk("headset remove\n");
#endif
				/* close stereo out */
			   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
			   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN); 
			   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN); 
			   
			   /* open old path */
			   codec_output_path = old_output_path;				
			   SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);	/*  right+left output  */
			   switch(codec_output_path)
			   {
				case A1_OUTPUT:		 /* set pcap output path register A1_EN=1 */
					   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);		
					   break;	
				case A4_OUTPUT:	/* set pcap output path register A4_EN=1  */
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  					   break;
				default:
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
					   codec_output_path = A2_OUTPUT;	
			   }
		}
#ifdef DEBUG
	printk("output path=%d \n", codec_output_path);
#endif
}


static int old_input_path = A5_INPUT;
void mic_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int	ssp_pcap_bit_status;
#ifdef DEBUG
	printk("enter mic change interrupt routine \n");
#endif
		/* read pcap register to check if external mic is in  */
		ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_MB2SNS);
		if(( ssp_pcap_bit_status == 1 ))	/* external mic is in  */
		{
		/* close old input path */
#ifdef DEBUG
	 		printk("external mic insert\n");
#endif
			old_input_path = codec_input_path;
			if(old_input_path == A3_INPUT)	/* A3 path */
			{
					SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	/* disable A3 */
					SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	/*diable A3_mux	*/	
			}
			else		/* default A5 path */
			{
					SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	/* disable A5 */
					SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	/* disable A5_mux */
			}
		/* open external mic path */
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);	/* enable external mux */
		   codec_input_path = EXTMIC_INPUT;
		}
		else
		{
#ifdef DEBUG
	 		printk("external mic remove\n");
#endif
			/* close external mic path  */
		   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
			/* open old input path */
		   codec_input_path = old_input_path;
			if(codec_input_path == A3_INPUT)	
			{
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	/* enable A3 */
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	/* enable A3_mux */		
			}
			else		/* default A5 path */
			{
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	/* enable A5  */
					SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	/* enable A5_mux */
			}
		}
#ifdef DEBUG
	printk("input path=%d \n", codec_input_path);
#endif
}

static void assp_timeout_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int	i, audiostatus, audionum;
	unsigned int audiodata[32];	
#ifdef DEBUG
	printk("enter assp timeout interrupt routine \n");
#endif
	audiostatus = ASSSR;
	/* receive timeout pending */
	if((audiostatus & ASSSR_TINT)!=0)
	{
		ASSSR |= ASSSR_TINT;	/* clear interrupt */
		if((audiostatus & ASSSR_RNE)!=0)  /* receive fifo is not empty */
		{
			audionum = (audiostatus & ASSSR_RFL_MASK) >> 12;	/* read RFL  */
			for(i=0; i<=audionum; i++)	/* valid data is audionum+1 */
				audiodata[i] = ASSDR;
		}
	}
#ifdef DEBUG
	printk("receive %d data\n", i);
#endif
}

/*
 * Audio Mixer stuff
 */

static int mixer_ioctl( struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret;
	long val;

	switch(cmd) {
		case SOUND_MIXER_READ_IGAIN:
#ifdef DEBUG
			printk(" read input gain\n");
#endif
			if (file->f_mode & FMODE_READ)
			{
	     		   /* read pcap igain register */
			   val = codec_input_gain*8;	/* change to 0-0xFF */
			   return put_user(val, (long *)arg);
			}
			if (file->f_mode & FMODE_WRITE)
			{
			   ret = -EINVAL;
   			   return put_user(ret, (int *) arg); 
			}
		case SOUND_MIXER_WRITE_IGAIN:
			if (file->f_mode & FMODE_READ)
			{
				ret = get_user(val, (long *) arg);
				if (ret)
					return ret;
				val = (val & 0xFF)/8;	/* val maybe 0-255, change to 0-31 */
#ifdef DEBUG
				printk(" write input gain=%d\n", val);
#endif
				codec_input_gain = val;
				/* write pcap igain register */
				ret = SSP_PCAP_AUDIG_set(codec_input_gain);
			}
			if (file->f_mode & FMODE_WRITE)
			{
			   ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_OGAIN:
#ifdef DEBUG
			printk(" read output gain\n");
#endif
			if (file->f_mode & FMODE_READ)
			{
				ret = -EINVAL;
				return put_user(ret, (long *)arg);
			}
			if (file->f_mode & FMODE_WRITE)
			{
				/* read pcap ogain register  */
				val = codec_output_gain*16;	/* change to 0-0xFF */
				return put_user(val, (long *)arg);
			}
		case SOUND_MIXER_WRITE_VOLUME:			
		case SOUND_MIXER_WRITE_OGAIN:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if (file->f_mode & FMODE_READ)
			{
   				ret = -EINVAL;
			}
			if (file->f_mode & FMODE_WRITE)
			{
				/* for left and right channel gain equal, input maybe 0-255, change to 0-15 */
				val = (val & 0xFF)/16 ;	
#ifdef DEBUG
				printk(" write output gain=%d\n", val);
#endif
				codec_output_gain = val;
				/* write pcap ogain register */
				ret = SSP_PCAP_AUDOG_set(codec_output_gain);
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_RECSRC:
#ifdef DEBUG
			printk(" read input path\n");
#endif
			/* read pcap input status, 0-A5 1-A3 */
			if (file->f_mode & FMODE_READ)	
			{
				val = codec_input_path;
			}
			if (file->f_mode & FMODE_WRITE)
				val = -EINVAL;
			return put_user(val, (long *)arg);

		case SOUND_MIXER_WRITE_RECSRC:
			if (file->f_mode & FMODE_READ)
			{
				ret = get_user(val, (long *) arg);
				if (ret)
					return ret;
#ifdef DEBUG
				printk(" write input path=%d\n", val);
#endif
				/* close old input path */
				switch(codec_input_path)
				{
					case EXTMIC_INPUT:
						/* close external mic path */
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);	/* disable external mux */
					case A3_INPUT:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	/* disable A3  */ 
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	/* disable A3_mux */		
					default:
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	/* disable A5 */
						SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	/* disable A5_mux */
				}
				/* open input path  */
				switch(val)
				{
					case EXTMIC_INPUT:
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);	/* enable external mux */
					case A3_INPUT:
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_EN);	
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A3_MUX);	
					default:
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_EN);	
						SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_A5_MUX);	
				}
				codec_input_path = val;
			}
			if (file->f_mode & FMODE_WRITE)
			{
				ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_OUTSRC:
#ifdef DEBUG
			printk(" read output path\n");
#endif
			if (file->f_mode & FMODE_WRITE)
			{
				val = codec_output_path;
			}
			if (file->f_mode & FMODE_READ)
			{
				val = -EINVAL;
			}
			return put_user(val, (long *)arg);

		case SOUND_MIXER_WRITE_OUTSRC:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
#ifdef DEBUG
			printk(" write output path=%d\n", val);
#endif
			if(file->f_mode && FMODE_WRITE)
			{
				/* close old output path */
				switch(codec_output_path)
				{
					case STEREO_OUTPUT:
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
		   				   SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);	/* right+left output  */
						   break;
					case A1_OUTPUT:
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);		
						   break;	
					case A4_OUTPUT:
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  	
						   break;
					default:
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
				}
				/* set pcap output register */
				switch(val)
				{
					case STEREO_OUTPUT:		
						/* set pcap output path register AR_EN=1, AL_EN=1  */
		   				   SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);	
						   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
						   break;		
					case A1_OUTPUT:		 /* set pcap output path register A1_EN=1 */
						   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);		
						   break;	
					case A4_OUTPUT:		/* set pcap output path register A4_EN=1  */
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  	
						   break;
					default:
						   ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
						   codec_output_path = A2_OUTPUT;	
				}
				codec_output_path = val;
			}
			if (file->f_mode & FMODE_READ)
			{
				ret = -EINVAL;
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_MUTE:	/* 0-unmute, 1-mute */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if (file->f_mode & FMODE_READ)
				ret = -EINVAL;
			/* according output path mute diff register */
			if(val==1)	/* if mute  */
			{
#ifdef DEBUG
			   	printk(" mute output path\n");
#endif
				switch(codec_output_path)
				{
					case STEREO_OUTPUT:
					   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
					   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
					   break;
					case A1_OUTPUT:
					   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);		
					   break;	
					case A4_OUTPUT:
					   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  	
					   break;
					default:
					   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
				}
			}
			else	/*  unmute  */
			{
#ifdef DEBUG
				printk("unmute output path\n");
#endif 
				switch(codec_output_path)
				{
					case STEREO_OUTPUT:		
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
					   break;		
					case A1_OUTPUT:		 
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);		
					   break;	
					case A4_OUTPUT:		
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A4_EN);		  	
					   break;
					default:
					   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A2_EN);
					   codec_output_path = A2_OUTPUT;	
				}
			}
			return put_user(ret, (int *) arg);

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

static int codec_adc_rate = 48000;	/* default 48k sample rate */
static int codec_dac_rate = 8000;	/* default 8k sample rate */

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
		if(file->f_mode & FMODE_READ)
		{
		   if(val)  /* read mode, not support stereo mode  */
			ret = -EINVAL;
		   else
			ret = 1;		
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
		if(file->f_mode & FMODE_READ)
		{
			return put_user(1, (long *) arg);
		}

	case SNDCTL_DSP_SPEED:
#ifdef DEBUG
		printk(" set sample frequency \n");
#endif
		ret = get_user(val, (long *) arg);
		if (ret)
			return ret;

		ASSCR0 &= ~ASSCR0_SSE; 
		if(file->f_mode & FMODE_WRITE)
		{
		   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
		   switch(val)
		   {
		   	 case 48000:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_48K);
					break;

		   	 case 44000: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
					break;

		   	 case 22000:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_22K);
					break;

		   	 case 16000:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_16K);
					break;

		   	 case 11000:	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_11K);
					break;

		   	 case 8000: 	ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_8K);
					break;
		   	 default:
 					break;	
		   }
		   /* reset digital filter(DF_RESET_ST_DAC=1)   */
			SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DF_RESET_ST_DAC);

		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);	
		   /* set pcap dac sample rate  */
			codec_dac_rate = val;
#ifdef DEBUG
			printk("DA sample freq = %d\n", codec_dac_rate);
#endif
		}
		if(file->f_mode & FMODE_READ)
		{
	       SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
		   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
		   switch(val)
		   {
		   	 case 16000: ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);
					break;
		   	 case 8000: ret = SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);
					break;
		   	 default:
 					break;	
		   }	
			/* reset digital filter(DF_RESET=1)  */
			SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);	
		   SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
			/* set pcap dac sample rate  */
			codec_adc_rate = val;	
#ifdef DEBUG
			printk("AD sample freq = %d\n", codec_adc_rate);
#endif
		}
		ASSCR0 |= ASSCR0_SSE;	/* enable assp controller  */
		timeout = 0;
		/* check if ssp is ready for slave operation	*/
		while(((audiostatus = ASSSR) & ASSSR_CSS) !=0)
		{
			if((timeout++) > 10000000)
			{
				printk("audio panic: can't be slave mode!!!");
				break;
			}
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









