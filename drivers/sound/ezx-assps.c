/*
 *  linux/drivers/sound/ezx-assps.c
 *
 *
 *  Description:  assp interface for the ezx platform
 *
 *
 *  Copyright:	BJDC motorola.
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *
 *  History:
 *  zhouqiong          Jun 20,2002             created
 *  zhouqiong          Sep 19,2002             according code review meeting minutes.
 *  zhouqiong          Oct 30,2002             according new requirement for VA.ASSP interface split to
 *                                             /dev/dsp (support stereo playback) and /dev/dsp16 (support
 *                                             mono playback and record) this file is for stereo playback.
 *  zhouqiong          Nov 05,2002             according code review meeting minutes.
 *  zhouqiong          Jan 13,2003             (1) add audio panic return value
 *                                             (2) modify sample frequency to standard
 *  zhouqiong          Mar 03,2003             (1) open headset interrupt
 *                                             (2) change gain when headset is in
 *                                             (3) add ioctl to get headset status
 *  zhouqiong          Apr 17,2003             (1) according codec_dac_rate init pcap
 *  zhouqiong          Apr 18,2003             (1) change output gain according output path
 *  zhouqiong          Apr 24,2003             (1) no switch when headset insert and remove
 *  zhouqiong          May 21,2003             (1) modify loudspk gain max 0db, for audio-shaping
 *  LiYong             Sep 23,2003             (1)Port from EZX; (2)Modify the ASSP port inital
 *  Jin Lihong(w20076) Jan 02,2004,Libdd66088  (1) Port from UDC e680 kernel of jem vob.
 *                                             (2) Move audio driver DEBUG macro definition to ezx-audio.h
 *                                                 header file,and redefine DEBUG to EZX_OSS_DEBUG
 *                                             (3) reorganize file header
 *  Jin Lihong(w20076) Jan 13,2004,LIBdd68327  Make the e680 louder speaker work.
 *  Jia Tong(w19836)   Feb 04,2004,LIBdd67717  haptics feature added
 *  Jin Lihong(w20076) Feb.23,2004,LIBdd79747  add e680 audio path switch and gain setting funcs
 *  Jia Tong(w19836)   Feb 23,2004,LIBdd79841  haptics GPIO initialization change
 *  Li  Yong(w19946)   Feb 26,2004 LIBdd80614  Add DAI test
 *                                             Add control to switch PCAP CODEC mode from master to slave mode
 *  Jin Lihong(w20076) Mar.15,2004,LIBdd86574  mixer bug fix
 *  Jia Tong(w19836)   Mar 17,2004,LIBdd87621  GPIO change for haptics filter & boomer mute while setting haptics.
 *  Jin Lihong(w20076) Mar.25,2004,LIBdd90846  a780 new gain setting interface
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  close dsp protection,and add 3d control interface for app
 *   Li  Yong(w19946)  Apr.23.2004.LIBee02702  Add EMU Carkit 
 *   Li  Yong(w19946)  May.23.2004.LIBee12065  Add the EMU audio test
 *  lin weiqiang       Jun.08,2004,LIBee14656  record noise bug fix.
 *  Jin Lihong(w20076) Jun.22,2004,LIBee24284  mixer power save
 *  Jin Lihong(w20076) Aug.11,2004,LIBff01482  audio pcap LOW_POWER bit initialize
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
#include <linux/pm.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "ezx-audio.h"
#include "ezx-common.h"


static DECLARE_MUTEX(cotulla_assp_mutex);

static int codec_dac_rate = STEREO_CODEC_44K_RATE;	 /* default 44k sample rate */
static struct timer_list audio_timer,mic_timer;

#ifdef CONFIG_PM
static struct pm_dev *mixer_hw_pm_dev;
#endif


EXPORT_SYMBOL(headjack_change_interrupt_routine);
EXPORT_SYMBOL(mic_change_interrupt_routine);
EXPORT_SYMBOL(mixer_ioctl);

static int assp_init(void);
static void assp_shutdown(void);
static void change_input_output(void);
static void open_mic_interrupt(void);

void Set_EMU_Mux_Switch(int mode);
void EIHF_Mute(int Flag);

static int EMU_AUD_test_flag = 0;

static struct timer_list EMU_timer;

/*initialize hardware, assp controller and pcap register*/
static int assp_init(void)
{
	unsigned long flags;
	unsigned long ssp_pcap_register_val;
	unsigned long audiostatus;
	unsigned long timeout;
	int ret;
	
	down(&cotulla_assp_mutex); 
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "setup assp controller register \n");
#endif
	local_irq_save(flags);
	CKEN |= CKEN4_ASSP;	                           /* need enable cken4  */

	set_GPIO_mode(GPIO_SYNC_IN_ASSP_MD);             /* Assp Frame sync */
	set_GPIO_mode(GPIO_SDATA_OUT_ASSP_MD);           /* Assp TX */
	set_GPIO_mode(GPIO_BITCLK_IN_ASSP_MD);           /* ASSP BitCLK  */
	set_GPIO_mode(GPIO_SDATA_IN_ASSP_MD);            /* ASsp RX */

	/* setup assp port */
	ASSCR0 = ASSCR0_FRF_PSP | ASSCR0_EDSS | ASSCR0_DSS_16bit;  /* PSP mode, 32bit */
	ASSCR1 = ASSCR1_EBCEI | ASSCR1_SCLKDIR | ASSCR1_SFRMDIR | ASSCR1_TFTH_4;
	ASSPSP = ASSPSP_SFRMWDTH_16 | ASSPSP_SCMODE;	
	ASSCR1 |= ASSCR1_TSRE;	                           /* enable transmit dma request */
	ASSCR0 |= ASSCR0_SSE;	                           /* enable assp controller */
	local_irq_restore(flags);

#ifdef EZX_OSS_DEBUG
	audiostatus = ASSCR0;
	printk(EZXOSS_DEBUG "ASSCR0 = 0x%lx\n", audiostatus);
	audiostatus = ASSCR1;
	printk(EZXOSS_DEBUG "ASSCR1 = 0x%lx\n", audiostatus);
	audiostatus = ASSPSP;
	printk(EZXOSS_DEBUG "ASSPSP = 0x%lx\n", audiostatus);
#endif

#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "setup pcap audio register\n");
#endif

	mute_output_to_avoid_pcap_noise();
	poweron_mixer(DSP_DEVICE);

	/* (1) set bitclk(BCK=3, two_time_slot) pll clock(ST_CLK=0, 13M) NORMAL mode(DIG_AUD_FS =00) */
	ssp_pcap_register_val = PCAP_ST_BCLK_SLOT_2 | PCAP_ST_CLK_PLL_CLK_IN_13M0 | PCAP_DIGITAL_AUDIO_INTERFACE_NORMAL;	//NORMAL mode
	SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER, ssp_pcap_register_val);
	/* here dsp device must use AP 13Mhz clock  */
	pcap_use_ap_13m_clock();

	/* set stereo sample rate */
	switch(codec_dac_rate)
	{
	   	 case STEREO_CODEC_48K_RATE:	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_48K);
		      break;

	   	 case STEREO_CODEC_44K_RATE:    
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
		      break;

	   	 case STEREO_CODEC_32K_RATE: 	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_32K);
		      break;

	   	 case STEREO_CODEC_24K_RATE: 	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_24K);
	                     break;

	   	 case STEREO_CODEC_22K_RATE:	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_22K);
		      break;

	   	 case STEREO_CODEC_16K_RATE:	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_16K);
		      break;

	   	 case STEREO_CODEC_12K_RATE: 	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_12K);
		      break;

	   	 case STEREO_CODEC_11K_RATE:	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_11K);
		      break;

	   	 case STEREO_CODEC_8K_RATE: 	
		      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_8K);
		      break;
	   	 default:
	   	      SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
		      break;
	}
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_dac_rate=%d\n", codec_dac_rate);
#endif
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_INV);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_FS_INV);

	/* (3) reset digital filter(DF_RESET_ST_DAC=1) */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DF_RESET_ST_DAC);

	/* (4)set  bitclk output(SMB_ST_DAC=0), audio IO=part1(DIG_AUD_IN_ST_DAC=1) */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_DIG_AUD_IN_ST_DAC);

	/* (5) enable pcap clk(ST_CLK_EN=1),enable dac(ST_DAC_EN=1)  */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
	mdelay(1);	/* specified enable time according spec */		

#ifdef CONFIG_ARCH_EZX_A780
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_output_path=%d\n", codec_output_path);
	printk(EZXOSS_DEBUG "codec_output_base=%d\n", codec_output_base);
#endif
	ret = (*mixer_open_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])(codec_output_base|codec_output_path);
#endif

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
	while(((audiostatus = ASSSR) & ASSSR_CSS) !=0){
		if((timeout++) > 10000000)
			goto err;
	}

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " complete all hardware init \n");
#endif
	up(&cotulla_assp_mutex);
		
	return 0;
err:
	up(&cotulla_assp_mutex);
	printk(EZXOSS_DEBUG "audio panic: ssp don't ready for slave operation!!! ");
	return -ENODEV;	
}


static void assp_shutdown(void)
{
	down(&cotulla_assp_mutex);

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

	shutdown_mixer(DSP_DEVICE);

	/* disable PCAP stereo DAC */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_CLK_EN);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_EN);
        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_SMB_ST_DAC);
	pcap_use_bp_13m_clock();
	
	up(&cotulla_assp_mutex);
}


/*
 *  for stereo headset and mono headset insert, it will take A1I interrupt
 *  in A1I interrupt handler, open MB_ON2 to check if mic connect
 */
void headjack_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int     bdelay = 20;
	
	 del_timer(&audio_timer);         
         init_timer(&audio_timer);           
         audio_timer.function = change_input_output;
         audio_timer.expires = (jiffies + bdelay);
         add_timer(&audio_timer); 
}


static void open_mic_interrupt(void)
{
#ifdef EZX_OSS_DEBUG
     printk(EZXOSS_DEBUG "Open mic interrupt\n");
#endif	
     SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I );
     udelay(10);
     SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);
}


void Set_EMU_Mux_Switch(int mode)
{ 
      set_GPIO_mode(GPIO_EMU_MUX1 | GPIO_OUT);
      set_GPIO_mode(GPIO_EMU_MUX2 | GPIO_OUT);

	switch(mode)
	{
		case DEFAULT_USB_MODE:
                    clr_GPIO(GPIO_EMU_MUX1);
                    clr_GPIO(GPIO_EMU_MUX2);         //default mode
			break;

		case MONO_AUDIO_MODE:
			set_GPIO(GPIO_EMU_MUX1); 
			clr_GPIO(GPIO_EMU_MUX2);         //Mono audio mode
			break;

		case SETERO_AUDIO_MODE:
			set_GPIO(GPIO_EMU_MUX1); 
			set_GPIO(GPIO_EMU_MUX2);        //Setero audio mode
			break;

		default:
			break;
													       }	
	           
	printk("::set EMU Mux GPIO.\n");
}

void EIHF_Mute(int Flag)
{

 	set_GPIO_mode(GPIO_SNP_INT_CTL | GPIO_OUT);
        if(EMU_AUD_test_flag)                     //At the audio EMU test mode
        {
	       clr_GPIO(GPIO_SNP_INT_CTL);
	       return;
        }
	
	if(!Flag)
	{
		clr_GPIO(GPIO_SNP_INT_CTL);
	}
	else
	{
		set_GPIO(GPIO_SNP_INT_CTL);          // high is active
	}
				
}

void Switch_Audio_To_USB()
{
        set_GPIO_mode(GPIO_SNP_INT_IN | GPIO_IN);

	while(1)                                                //If the EMU ID short to GND
	{
		if(!(GPLR(GPIO_SNP_INT_IN) & GPIO_bit(GPIO_SNP_INT_IN)))
		  break;	
	}
	EIHF_Mute(EIHF_MUTE);
        Set_EMU_Mux_Switch(DEFAULT_USB_MODE);                    // Switch the MUX to USB mode 
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP); // Disable the PCAP loopback
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);     
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PS);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);       //Pull up the D+
        
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_USB4VI);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_USB4VM);

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_USB1VI);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_USB1VM);
				
	EMU_AUD_test_flag = 0;                                  // Restore the default value
}

void Switch_USB_To_Audio()
{
	EMU_AUD_test_flag = 0xff;                                // Enter the EMU audio test mode
        set_GPIO_mode(GPIO_SNP_INT_IN | GPIO_IN);
	

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_USB4VM);

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_USB1VM);
	
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);
        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PS);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
  
	
	del_timer(&EMU_timer);                                   //Start one timer
	init_timer(&EMU_timer);
	EMU_timer.function = Switch_Audio_To_USB;
	EMU_timer.expires = (jiffies+500);
	add_timer(&EMU_timer);
}



static void change_input_output(void)
{
	unsigned long	ssp_pcap_bit_status;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "enter headjack change interrupt routine \n");
#endif
	/* read pcap register to check if headjack is in */
	ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
	if(ssp_pcap_bit_status)	/* headset is in */
	{
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "headset insert\n");
#endif
		headset_in_handler(0, NULL, NULL);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);	
		micinflag = 1;
		del_timer(&mic_timer);
		init_timer(&mic_timer);
		mic_timer.function = open_mic_interrupt;
		mic_timer.expires = (jiffies+100);
		add_timer(&mic_timer);
	}
	else	/* headset is out */
	{
#ifdef EZX_OSS_DEBUG
 		printk(EZXOSS_DEBUG "headset remove\n");
#endif
		headset_out_handler(0, NULL, NULL);
		micinflag = 0;
		del_timer(&mic_timer);
		SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_MB2I);
		SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);
	}
}


void mic_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs)
{
	int 	ssp_pcap_bit_status;
	
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "enter mic change interrupt routine \n");
#endif

	/* read pcap register to check if headjack is in */
	ssp_pcap_bit_status = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
   	if( ssp_pcap_bit_status )	/* headjack is in */
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
	int i,j;
	long val;
	unsigned long ssp_pcap_register_val;

	switch(cmd) 
	{
#ifdef MAKE_FTR_HAPTICS
		case SOUND_MIXER_WRITE_HAPTICS_ON:
			Set_Haptics_GPIO();
			break;
		case SOUND_MIXER_WRITE_HAPTICS_OFF:
			Clear_Haptics_GPIO();
			break;
		case SOUND_MIXER_READ_HAPTICS_FIL:
			if( (GPDR(GPIO_FLT_SEL_BUL) & GPIO_bit(GPIO_FLT_SEL_BUL))==0 )
				val = 0;
			else
				val = 1;
			return put_user(val, (long *)arg);
#endif

		case SOUND_MIXER_READ_IGAIN:
			val = codec_input_gain;
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " read input gain=%d\n", val);
#endif
			return put_user(val, (long *)arg);
			
		case SOUND_MIXER_WRITE_IGAIN:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			
			if( (val<EZX_OSS_MIN_LOGICAL_GAIN) || (val>EZX_OSS_MAX_LOGICAL_GAIN) )
				ret = -EINVAL;
			else{
				codec_input_gain = val;
#ifdef EZX_OSS_DEBUG
				printk(EZXOSS_DEBUG " write input gain=%d\n", codec_input_gain);
#endif
				set_input_gain_hw_reg();
			}
			
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_OGAIN:
			/* read pcap ogain register  */
			val = codec_output_gain;	
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " read output gain=%d\n", val);
#endif
			return put_user(val, (long *)arg);
	
		case SOUND_MIXER_WRITE_VOLUME:
		case SOUND_MIXER_WRITE_OGAIN:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " write output gain=%d\n", val);
#endif

			if((val >= EZX_OSS_MIN_LOGICAL_GAIN)&&(val <=EZX_OSS_MAX_LOGICAL_GAIN))
			{
				codec_output_gain = val;
				/* write pcap ogain register */
				set_output_gain_hw_reg();
			}
			else
			{
				ret = -EINVAL; 
#ifdef EZX_OSS_DEBUG
				printk(EZXOSS_DEBUG "value is invalid\n");
#endif
			}
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_RECSRC:
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " read input path\n");
#endif
			/* read pcap input status, 0-extmic, 1-A5, 2-A3 */
			val = codec_input_path;
			return put_user(val, (long *)arg);

		case SOUND_MIXER_WRITE_RECSRC:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " force write input path=%d\n", val);
#endif
			/* close old input path */
                     (*mixer_close_input_path[codec_input_path])();
			/* open input path  */
			if( (val>INPUT_PATH_MAX) || (val<INPUT_PATH_MIN) )
				ret = -EINVAL;
			else
				(*mixer_open_input_path[val])();

			if( audioonflag == 0 )
				mixer_not_in_use();		/* for power save */

#ifdef EZX_OSS_DEBUG
                SSP_PCAP_read_data_from_PCAP(0x1a,&ssp_pcap_register_val);
                printk(EZXOSS_DEBUG "pcap register 26 = 0x%lx\n", ssp_pcap_register_val);
#endif
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_READ_OUTSRC:
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " read output path\n");
#endif
			val = codec_output_path | codec_output_base;
			return put_user(val, (long *)arg);

		case SOUND_MIXER_WRITE_OUTSRC:
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG "force write output path=0x%03X\n", val);
#endif
			/* close old output path */
                     (*mixer_close_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])();

			/* set pcap output register */
			if( (GET_OUTPUT_BASE(val)>OUTPUT_BASE_MAX) || (GET_OUTPUT_PATH(val)>OUTPUT_PATH_MAX) )
				ret = -EINVAL;
			else
				ret = (*mixer_open_output_path[OUTPUT_BASE_TYPE(GET_OUTPUT_BASE(val))][GET_OUTPUT_PATH(val)])(val);

			if( audioonflag == 0 )
				mixer_not_in_use();		/* for power save */

#ifdef EZX_OSS_DEBUG
           		SSP_PCAP_read_data_from_PCAP(0x0c,&ssp_pcap_register_val);
              	printk(EZXOSS_DEBUG "pcap register 12 = 0x%lx\n", ssp_pcap_register_val);
#endif
			return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_MUTE:	/* mute output path */
			/* 0-unmute, 1-mute */
			ret = get_user(val, (long *) arg);
			if (ret)
				return ret;
			if(val == 1)	/* if mute  */
			{
#ifdef EZX_OSS_DEBUG
			   	printk(EZXOSS_DEBUG " mute PGA\n");
#endif
				   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_R_EN);
				   SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_L_EN);
			}
			else if(val == 0)	/*  unmute  */
			{
#ifdef EZX_OSS_DEBUG
				printk(EZXOSS_DEBUG "unmute PGA\n");
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
#ifdef EZX_OSS_DEBUG
			   	printk(EZXOSS_DEBUG " mute input\n");
#endif
                    		(*mixer_close_input_path[codec_input_path])();
			}
			else if(val == 0)	/*  unmute  */
			{
#ifdef EZX_OSS_DEBUG
				printk(EZXOSS_DEBUG "unmute input\n");
#endif
				(*mixer_open_input_path[codec_input_path])();
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
#ifdef EZX_OSS_DEBUG
			    printk(EZXOSS_DEBUG "loopback\n");
#endif
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_CD_BYP);
					  
			    	
			}
			else if(val ==0)
			{
#ifdef EZX_OSS_DEBUG
			    printk(EZXOSS_DEBUG "unloopback\n");
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
#ifdef EZX_OSS_DEBUG
			    printk(EZXOSS_DEBUG "enable audio output High Pass filter\n");
#endif
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDOHPF);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);   
			    	
			}
			else if(val ==0)
			{
#ifdef EZX_OSS_DEBUG
			    printk(EZXOSS_DEBUG "disable audio output High Pass filter\n");
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
#ifdef EZX_OSS_DEBUG
			    printk("enable audio input High Pass filter\n");
#endif
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_AUDIHPF);   
			    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);   
			    	
			}
			else if(val ==0)
			{
#ifdef EZX_OSS_DEBUG
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
		
		case SOUND_MIXER_WRITE_CODEC_SLAVE:
		       SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);
		       SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
		       SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_DF_RESET);
		       return put_user(ret, (int *) arg);
		case SOUND_MIXER_READ_HEADSET_STATUS:	/* read if headset is in */
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG "read headset status, jiffies=%d\n", jiffies);
#endif

			j=0;
			for(i=0;i<3;i++)
			{
				val = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_A1SNS);
				if(val)
					j++;
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(50*HZ/1000);
			}
			if(j>=2)
			{
				ret = STEREO_HEADSET;
				micinflag=1;
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
				SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_MSR_MB2M);
			}
			else
			{
				ret = NO_HEADSET;
				micinflag=0;
				SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_MB_ON2);
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_MSR_MB2M);
			}
			return put_user(ret, (long *)arg);

                case SOUND_MIXER_WRITE_EIHF_MUX:
			 ret = get_user(val, (long *) arg);
			 if(ret)
			      return ret;
#ifdef EZX_OSS_DEBUG			 
			  printk(EZXOSS_DEBUG " \n user space is writing MUX  status =%d\n", val);
#endif		
		 	  switch(val)
			 {
				 case DEFAULT_USB_MODE:
					Set_EMU_Mux_Switch(DEFAULT_USB_MODE); 
					 break;
				 case MONO_AUDIO_MODE:
					 Set_EMU_Mux_Switch(MONO_AUDIO_MODE);
					 break;
				 case SETERO_AUDIO_MODE:
					 Set_EMU_Mux_Switch(SETERO_AUDIO_MODE);
					 break;
				 default:
					 ret = -EINVAL;
			 }	 
			 return put_user(ret, (int *) arg);

		case SOUND_MIXER_WRITE_EIHF_MUTE:	
			ret = get_user(val, (long *) arg);
			if(ret)
			      return ret;
#ifdef EZX_OSS_DEBUG			
                        printk(EZXOSS_DEBUG " \n user space is writing MUTE status =%d\n", val);
#endif		
			if(val)
                           EIHF_Mute(EIHF_UNMUTE);
			else
                           EIHF_Mute(EIHF_MUTE);
			
         		return put_user(ret, (int *) arg);	
		case SOUND_MIXER_WRITE_EMU_TEST:
			ret = get_user(val, (long *) arg);
			if(ret)
				return ret;
	                Switch_USB_To_Audio();
			return  put_user(ret, (int *) arg);

#ifdef CONFIG_ARCH_EZX_E680
		case SOUND_MIXER_READ_3D_STATUS:
			val = get_boomer_3d_status();
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " read e680 3d status =%d\n", val);
#endif
			return put_user(val, (long *)arg);
			
		case SOUND_MIXER_WRITE_3D_STATUS:
			ret = get_user(val, (long *) arg);
			if(ret)
				return ret;
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " user space is writing 3d status =%d\n", val);
#endif

			if( (val<LOUDERSPEAKER_ON_HEADSET_ON_3D) || (val>LOUDERSPEAKER_OFF_HEADSET_OFF_3D) )
				ret = -EINVAL;
			else
				set_boomer_3d_status((boomer_3d_status)val);
			
			return put_user(ret, (int *) arg);
#endif

#ifdef CONFIG_ARCH_EZX_A780
		case SOUND_MIXER_WRITE_HW_ATTENU:
			ret = get_user(val, (long *) arg);
			if(ret)
				return ret;
			
			if(val == HW_ATTENUATION_USED){
				use_hw_noise_attenuate();
				ret = HW_ATTENUATION_USED;
			}
			else if(val == HW_ATTENUATION_BYPASSED){
				bypass_hw_noise_attenuate();
				ret = HW_ATTENUATION_BYPASSED;
			}
			else
				ret = -EINVAL;
			
			return put_user(ret, (int *) arg);
#endif

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
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG " check if support stereo\n");
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
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG " check if 2 channels \n");
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
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG " set sample frequency \n");
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
		   	 case STEREO_CODEC_48K_RATE:	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_48K);
			      			         /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_44K_RATE: 	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_44K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_32K_RATE: 	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_32K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_24K_RATE: 	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_24K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_22K_RATE:	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_22K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_16K_RATE:	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_16K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_12K_RATE: 	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_12K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_11K_RATE:	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_11K);
							 /* set pcap dac sample rate  */
			      codec_dac_rate = val;
			      break;

		   	 case STEREO_CODEC_8K_RATE: 	
			      ret = SSP_PCAP_CDC_SR_set(PCAP_ST_SAMPLE_RATE_8K);
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
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG "DA sample freq = %d\n", codec_dac_rate);
#endif
			ASSCR0 |= ASSCR0_SSE;	/* enable assp controller  */
			timeout = 0;
			/* check if ssp is ready for slave operation	*/
			while(((audiostatus = ASSSR) & ASSSR_CSS) !=0)
			{
				if((timeout++) > 10000000)
				{
					printk(EZXOSS_DEBUG "audio panic: can't be slave mode!!!");
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
		return put_user(codec_dac_rate, (long *) arg);

	case SOUND_PCM_READ_RATE:
		if (file->f_mode & FMODE_WRITE)
		{
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG "read DA sample freq\n");
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
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "data format is AFMT_S16_LEd\n");
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
	hw_init:		assp_init,
	hw_shutdown:		assp_shutdown,
	sem:			__MUTEX_INITIALIZER(assp_audio_state.sem),
};


static int assp_audio_open(struct inode *inode, struct file *file)
{
#ifdef EZX_OSS_DEBUG
    printk(EZXOSS_DEBUG "assp audio open \n");
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

	set_GPIO_mode(GPIO_ASSP_SCLK3 | GPIO_IN);			/* Assp Frame sync */
        set_GPIO_mode(GPIO_ASSP_TXD3  | GPIO_IN);   
	set_GPIO_mode(GPIO_ASSP_RXD3  | GPIO_IN);			/* ASSP BitCLK  */
	set_GPIO_mode(GPIO_ASSP_SFRM3 | GPIO_IN);			/* ASsp RX */

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AUDIO_LOWPWR);

#ifdef CONFIG_ARCH_EZX_E680
	Clear_Haptics_GPIO();
	e680_boomer_init();
#endif

#ifdef CONFIG_PM
	mixer_hw_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, mixer_hw_pm_callback);
#endif

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "cotulla-assp-init ok\n");
#endif
	return 0;
}


static void __exit cotulla_assp_exit(void)
{
	unregister_sound_dsp(assp_audio_state.dev_dsp);
	unregister_sound_mixer(mixer_dev_id);

#ifdef CONFIG_ARCH_EZX_E680
	e680_boomer_shutdown();
#endif

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AUDIO_LOWPWR);

#ifdef CONFIG_PM
	pm_unregister(mixer_hw_pm_dev);
#endif

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "cotulla-assp-exit ok\n");
#endif
}


module_init(cotulla_assp_init);
module_exit(cotulla_assp_exit);


