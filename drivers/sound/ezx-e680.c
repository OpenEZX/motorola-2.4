/*
 *  linux/drivers/sound/ezx-e680.c
 *
 *
 *  Description:  Motorola e680 phone specific functions implementation for audio drivers
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
 *  Jin Lihong(w20076) Jan 13,2004,LIBdd68327  Created, Make the e680 louder speaker work.
 *  Jin Lihong(w20076) Feb.23,2004,LIBdd79747  add e680 audio path switch and gain setting funcs
 *  Jin Lihong(w20076) Mar.15,2004,LIBdd86574  mixer bug fix
 *  Jin Lihong(w20076) Mar.25,2004,LIBdd90336  play music noise tmp solution
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  close dsp protection,and add 3d control interface for app
 *  Jin Lihong(w20076) Apr.20,2004,LIBee01165  reduce noise
 *  Jin Lihong(w20076) Apr.24,2004,LIBee03164  reduce music noise, add new pathes for haptics
 *  Li  Yong(w19946)   Apr.23.2004.LIBee02702  Add EMU Carkit
 *  Jin Lihong(w20076) Jun.08,2004,libee14656  change boomer input path for mono sound
 *  Jin Lihong(w20076) Jun.15,2004,LIBee21625  boomer's power management
 *  Jin Lihong(w20076) Jun.22,2004,LIBee24284  mixer power save
 *  Wu Hongxing(w20691)Jul.07,2004 LIBee29664  ap 13m clock usage.
 *  Jin Lihong(w20076) Aug.11,2004,LIBff01482  mixer power on/off sequence optimize
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
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "ezx-common.h"


#ifdef CONFIG_ARCH_EZX_E680

EXPORT_SYMBOL(poweron_mixer);
EXPORT_SYMBOL(shutdown_mixer);
EXPORT_SYMBOL(kernel_clr_oscc_13m_clock);
EXPORT_SYMBOL(kernel_set_oscc_13m_clock);

static DECLARE_MUTEX(ezx_mixer_mutex);



#define BOOMER_MONO_VOL_REG_PREFIX 0x00
#define BOOMER_STEREO_LEFT_VOL_REG_PREFIX 0x40
#define BOOMER_STEREO_RIGHT_VOL_REG_PREFIX 0x80
#define BOOMER_MODE_CONTROL_REG_PREFIX 0xC0

#define BOOMER_MONO_GAIN_MIN_REG_VAL 0
#define BOOMER_MONO_GAIN_MAX_REG_VAL 31
#define BOOMER_MONO_GAIN_DEFAULT_REG_VAL 15
#define BOOMER_MONO_GAIN_DEFAULT_FIXED_REG_VAL 23
#define BOOMER_MONO_GAIN_MIXED_INPUT_CASE_REG_VAL 15			/* when in a special use case: mono input for haptics,
														stereo input for audio signals */

#define BOOMER_STEREO_LEFT_GAIN_MIN_REG_VAL 0
#define BOOMER_STEREO_LEFT_GAIN_MAX_REG_VAL 31
#define BOOMER_STEREO_LEFT_GAIN_DEFAULT_REG_VAL 15
#define BOOMER_STEREO_LEFT_GAIN_DEFAULT_FIXED_REG_VAL 27
#define BOOMER_STEREO_LEFT_GAIN_MIXED_INPUT_CASE_REG_VAL 27		/* when in a special use case: mono input for haptics,
														stereo input for audio signals */

#define BOOMER_STEREO_RIGHT_GAIN_MIN_REG_VAL 0
#define BOOMER_STEREO_RIGHT_GAIN_MAX_REG_VAL 31
#define BOOMER_STEREO_RIGHT_GAIN_DEFAULT_REG_VAL 15
#define BOOMER_STEREO_RIGHT_GAIN_DEFAULT_FIXED_REG_VAL 27
#define BOOMER_STEREO_RIGHT_GAIN_MIXED_INPUT_CASE_REG_VAL 27	/* when in a special use case: mono input for haptics,
														stereo input for audio signals */

#define BOOMER_OUTPUT_GAIN_MONO_REG_VAL_FROM_LOGIC (codec_output_gain*BOOMER_MONO_GAIN_MAX_REG_VAL/EZX_OSS_MAX_LOGICAL_GAIN)
#define BOOMER_OUTPUT_GAIN_STEREO_LEFT_REG_VAL_FROM_LOGIC (codec_output_gain*BOOMER_STEREO_LEFT_GAIN_MAX_REG_VAL/EZX_OSS_MAX_LOGICAL_GAIN)
#define BOOMER_OUTPUT_GAIN_STEREO_RIGHT_REG_VAL_FROM_LOGIC (codec_output_gain*BOOMER_STEREO_RIGHT_GAIN_MAX_REG_VAL/EZX_OSS_MAX_LOGICAL_GAIN)

#define BOOMER_LOUDER_SPEAKER_3D_OFF 0X00		/* bit LD5 */
#define BOOMER_LOUDER_SPEAKER_3D_ON 0X20
#define BOOMER_HEADPHONE_3D_OFF 0X00			/* bit RD5 */
#define BOOMER_HEADPHONE_3D_ON 0X20

#define BOOMER_SHUTDOWN 0X00					/* bit CD3 ~ CD0 */
#define BOOMER_IN_MONO_OUT_MONO_EARPIECE 0X01
#define BOOMER_IN_MONO_OUT_LOUDER_SPEAKER 0X02
#define BOOMER_IN_MONO_OUT_LOUDER_SPEAKER_AND_HEADPHONE 0X03
#define BOOMER_IN_MONO_OUT_HEADPHONE 0X04
#define BOOMER_IN_MONO_OUT_MONO_LINEOUT 0X05
#define BOOMER_IN_STEREO_OUT_MONO_EARPIECE 0X06
#define BOOMER_IN_STEREO_OUT_LOUDER_SPEAKER 0X07
#define BOOMER_IN_STEREO_OUT_LOUDER_SPEAKER_AND_HEADPHONE 0X08
#define BOOMER_IN_STEREO_OUT_HEADPHONE 0X09
#define BOOMER_IN_STEREO_OUT_MONO_LINEOUT 0X0a
#define BOOMER_IN_MONO_AND_STEREO_OUT_MONO_EARPIECE 0X0b
#define BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER 0X0c
#define BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER_AND_HEADPHONE 0X0d
#define BOOMER_IN_MONO_AND_STEREO_OUT_HEADPHONE 0X0e
#define BOOMER_IN_MONO_AND_STEREO_OUT_MONO_LINEOUT 0X0f

#define BOOMER_FAST_WAKEUP 0X00					/* bit CD5 */
#define BOOMER_SLOW_WAKEUP 0X20

#define BOOMER_EARPIECE_GAIN_SMALL_OPTION 0X00	/* bit CD4 */
#define BOOMER_EARPIECE_GAIN_LARGE_OPTION 0X10


typedef struct {
	u8 monovol;				/* mono volume */
	u8 leftvol;				/* stereo left volume */
	u8 rightvol;			/* stereo right volume */
	u8 path;				/*  boomer path setup */
	u8 speaker3d;
	u8 headphone3d;
	u8 wakeuptime;			/* fast or slow wakeup setting */
	u8 earpiecegaintable;	/* earpiece gain table select */
} e680_boomer;


static e680_boomer e680boomer;
static u32 gpio_flt_sel_status;
u32 gpio_va_sel_status;

static void set_boomer_mono_vol_reg( void );
static void set_boomer_stereo_left_vol_reg(void);
static void set_boomer_stereo_right_vol_reg(void);
static void set_boomer_mode_control_reg(void);

static void write_boomer(const char *buf, size_t count);
extern int mixer_write(const char *buf, size_t count);


void e680_boomer_init(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "e680 boomer init\n");
#endif
	
	e680boomer.monovol				= BOOMER_MONO_GAIN_DEFAULT_REG_VAL;
	e680boomer.leftvol				= BOOMER_STEREO_LEFT_GAIN_DEFAULT_REG_VAL;
	e680boomer.rightvol				= BOOMER_STEREO_RIGHT_GAIN_DEFAULT_REG_VAL;
	e680boomer.path					= BOOMER_IN_MONO_OUT_MONO_LINEOUT;
	e680boomer.speaker3d			= BOOMER_LOUDER_SPEAKER_3D_ON;
	e680boomer.headphone3d			= BOOMER_HEADPHONE_3D_ON;
	e680boomer.wakeuptime			= BOOMER_FAST_WAKEUP;
	e680boomer.earpiecegaintable	= BOOMER_EARPIECE_GAIN_LARGE_OPTION;

	set_boomer_mode_control_reg();
	set_boomer_mono_vol_reg();
	set_boomer_stereo_left_vol_reg();
	set_boomer_stereo_right_vol_reg();
}


void e680_boomer_shutdown(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "e680 boomer shutdown.\n");
#endif

	e680boomer.path	= BOOMER_SHUTDOWN;
	set_boomer_mode_control_reg();
}


void e680_boomer_path_mono_lineout(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " set E680 boomer path to mono input mono lineout.\n");
#endif

	e680boomer.path	= BOOMER_IN_MONO_OUT_MONO_LINEOUT;
	set_boomer_mode_control_reg();
}


void e680_boomer_path_tmp_mono_lineout(void)
{
	u8 mode;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " mute E680 boomer.\n");
#endif

	mode = BOOMER_MODE_CONTROL_REG_PREFIX | BOOMER_IN_MONO_OUT_MONO_LINEOUT
			| e680boomer.wakeuptime | e680boomer.earpiecegaintable;

	write_boomer(&mode, 1);
}


void e680_boomer_path_tmp_shutdown(void)
{
	u8 mode;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " E680 boomer path set to tmp shutdown..\n");
#endif

	mode = BOOMER_MODE_CONTROL_REG_PREFIX | BOOMER_SHUTDOWN
			| e680boomer.wakeuptime | e680boomer.earpiecegaintable;

	write_boomer(&mode, 1);
}


void e680_boomer_path_restore(void)
{
	u8 mode;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " E680 boomer path restore: path = 0X%X \n", e680boomer.path);
#endif
	mode = BOOMER_MODE_CONTROL_REG_PREFIX | e680boomer.path | e680boomer.wakeuptime | e680boomer.earpiecegaintable;
	write_boomer(&mode, 1);
}


static void set_boomer_mono_vol_reg( void )
{
	u8 vol;

	vol = BOOMER_MONO_VOL_REG_PREFIX | e680boomer.monovol;
	write_boomer(&vol, 1);

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "set e680 boomer mono volume register,vol=0x%2X \n", vol);
#endif
}


static void set_boomer_stereo_left_vol_reg(void)
{
	u8 vol;

	vol = BOOMER_STEREO_LEFT_VOL_REG_PREFIX | e680boomer.leftvol | e680boomer.speaker3d;
	write_boomer(&vol, 1);

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "set e680 boomer stereo left volume register,vol=0x%2X \n", vol);
#endif
}


static void set_boomer_stereo_right_vol_reg(void)
{
	u8 vol;

	vol = BOOMER_STEREO_RIGHT_VOL_REG_PREFIX | e680boomer.rightvol | e680boomer.headphone3d;
	write_boomer(&vol, 1);

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "set e680 boomer stereo right volume register,vol=0x%2X \n", vol);
#endif
}


static void set_boomer_mode_control_reg(void)
{
	u8 mode;

	mode = BOOMER_MODE_CONTROL_REG_PREFIX | e680boomer.path | e680boomer.wakeuptime | e680boomer.earpiecegaintable;
	write_boomer(&mode, 1);

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "set e680 boomer mode control register,mode=0x%2X \n", mode);
#endif
}


static void write_boomer(const char *buf, size_t count)
{
    int ret, i = 0;
    size_t written = 0;

    while(written<count)
    {
        ret = mixer_write(buf+written, count-written);
        written+=ret;
        i++;
        if( i>2 )
        	break;
    }
    if( written < count )
        printk(EZXOSS_ERR "write boomer failed.\n");
}


void poweron_mixer( audio_dev_type type )
{
	down(&ezx_mixer_mutex);
	
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "E680 No. 0x%X device wants to power on the mixer hardware.\n", type);
	printk(EZXOSS_DEBUG "E680 No. 0x%X device has already powered on the mixer hardware.\n", audioonflag);
#endif

	audioonflag |= type;
	
	if( MIXERISSTEREO == type ){
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "E680 No. 0x%X device is powering on the mixer hardware.\n", type);
#endif

		if( (type==MIDI_DEVICE) || (type==FM_DEVICE) )
			mute_output_to_avoid_pcap_noise();

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

		if( (type==MIDI_DEVICE) || (type==FM_DEVICE) )
			undo_mute_output_to_avoid_pcap_noise();
	}
	
	up(&ezx_mixer_mutex);
}


void shutdown_mixer( audio_dev_type type )
{
	down(&ezx_mixer_mutex);
	
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "E680 No. 0x%X device wants to shut down the mixer hardware.\n", type);
#endif

	if( audioonflag == type ){
#ifdef EZX_OSS_DEBUG
		printk(EZXOSS_DEBUG "E680 No. 0x%X device is shutting down the mixer hardware.\n", type);
#endif

		e680_boomer_path_mono_lineout();	/* mute hw noise and save power for e680 */
			
		/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
	}
	
	audioonflag &= ~type;
	
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "E680 No. 0x%X device is still using the mixer hardware.\n", audioonflag);
#endif

	up(&ezx_mixer_mutex);
}


void mixer_not_in_use(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " E680 mixer not in use.\n");
#endif

	(*mixer_close_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])();
	(*mixer_close_input_path[codec_input_path])();
	
	if( micinflag == 0 )	/* close pcap output path */
		SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER, SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);

	e680_boomer_path_mono_lineout();
}


void set_output_gain_hw_reg(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "set output gain hw register. \n");
#endif

	SSP_PCAP_AUDOG_set( PCAP_OUTPUT_GAIN_REG_VAL_FROM_LOGIC );
		
	if( e680boomer.path == BOOMER_SHUTDOWN ){
		;
	}
	else if( e680boomer.path <= BOOMER_IN_MONO_OUT_MONO_LINEOUT ){
		e680boomer.monovol = BOOMER_MONO_GAIN_DEFAULT_FIXED_REG_VAL;
		set_boomer_mono_vol_reg();
	}
	else if( e680boomer.path <= BOOMER_IN_STEREO_OUT_MONO_LINEOUT ){
		e680boomer.leftvol = BOOMER_STEREO_LEFT_GAIN_DEFAULT_FIXED_REG_VAL;
		e680boomer.rightvol = BOOMER_STEREO_RIGHT_GAIN_DEFAULT_FIXED_REG_VAL;
		set_boomer_stereo_left_vol_reg();
		set_boomer_stereo_right_vol_reg();
	}
	else{
		if( e680boomer.path == BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER ){
			e680boomer.monovol = BOOMER_MONO_GAIN_MIXED_INPUT_CASE_REG_VAL;
			e680boomer.leftvol = BOOMER_STEREO_LEFT_GAIN_MIXED_INPUT_CASE_REG_VAL;
			e680boomer.rightvol = BOOMER_STEREO_RIGHT_GAIN_MIXED_INPUT_CASE_REG_VAL;
		}
		else{
			e680boomer.monovol = BOOMER_MONO_GAIN_DEFAULT_FIXED_REG_VAL;
			e680boomer.leftvol = BOOMER_STEREO_LEFT_GAIN_DEFAULT_FIXED_REG_VAL;
			e680boomer.rightvol = BOOMER_STEREO_RIGHT_GAIN_DEFAULT_FIXED_REG_VAL;
		}
		set_boomer_mono_vol_reg();
		set_boomer_stereo_left_vol_reg();
		set_boomer_stereo_right_vol_reg();
	}

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "codec_output_gain=%d\n", codec_output_gain);
#endif
}


void set_input_gain_hw_reg(void)
{
	SSP_PCAP_AUDIG_set( PCAP_INPUT_AUDIG_REG_VAL_FROM_LOGIC );

#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "codec_input_gain=%d\n", codec_input_gain );
#endif
}


void set_boomer_3d_status(boomer_3d_status status)
{
#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "set boomer 3d status.\n");
#endif

	switch(status)
	{
		case LOUDERSPEAKER_ON_HEADSET_OFF_3D:
			e680boomer.speaker3d	= BOOMER_LOUDER_SPEAKER_3D_ON;
			e680boomer.headphone3d	= BOOMER_HEADPHONE_3D_OFF;
			break;

		case LOUDERSPEAKER_OFF_HEADSET_ON_3D:
			e680boomer.speaker3d	= BOOMER_LOUDER_SPEAKER_3D_OFF;
			e680boomer.headphone3d	= BOOMER_HEADPHONE_3D_ON;
			break;

		case LOUDERSPEAKER_OFF_HEADSET_OFF_3D:
			e680boomer.speaker3d	= BOOMER_LOUDER_SPEAKER_3D_OFF;
			e680boomer.headphone3d	= BOOMER_HEADPHONE_3D_OFF;
			break;

		case LOUDERSPEAKER_ON_HEADSET_ON_3D:
		default:
			e680boomer.speaker3d	= BOOMER_LOUDER_SPEAKER_3D_ON;
			e680boomer.headphone3d	= BOOMER_HEADPHONE_3D_ON;
	}
	
	set_boomer_stereo_left_vol_reg();
	set_boomer_stereo_right_vol_reg();
}


boomer_3d_status get_boomer_3d_status(void)
{
#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "get boomer 3d status.\n");
#endif

	if(e680boomer.speaker3d	== BOOMER_LOUDER_SPEAKER_3D_ON){
		if(e680boomer.headphone3d == BOOMER_HEADPHONE_3D_ON)
			return LOUDERSPEAKER_ON_HEADSET_ON_3D;
		else
			return LOUDERSPEAKER_ON_HEADSET_OFF_3D;
	}
	else{
		if(e680boomer.headphone3d == BOOMER_HEADPHONE_3D_ON)
			return LOUDERSPEAKER_OFF_HEADSET_ON_3D;
		else
			return LOUDERSPEAKER_OFF_HEADSET_OFF_3D;
	}
}


void set_gpio_va_sel_out_high(void)
{
	set_GPIO_mode(GPIO_VA_SEL_BUL | GPIO_OUT);
	set_GPIO(GPIO_VA_SEL_BUL);
        PGSR(GPIO_VA_SEL_BUL) |= GPIO_bit(GPIO_VA_SEL_BUL); 
        gpio_va_sel_status = 1;
#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "set VA_SEL high. \n");
#endif
}


void set_gpio_va_sel_out_low(void)
{
	set_GPIO_mode(GPIO_VA_SEL_BUL | GPIO_OUT);
	clr_GPIO(GPIO_VA_SEL_BUL);
        PGSR(GPIO_VA_SEL_BUL) &= ~GPIO_bit(GPIO_VA_SEL_BUL); 
        gpio_va_sel_status = 0;
#ifdef EZX_OSS_DEBUG
	printk( EZXOSS_DEBUG "set VA_SEL low. \n");
#endif
}


void close_input_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close input carkit. \n");
#endif

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}


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


void close_input_funlight(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close input funlight. \n");
#endif

	/* disable PCAP codec */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_CLK_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_CDC_EN);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);
	/* set fsync, tx, bitclk are tri-stated */
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CD_TS);

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}


void open_input_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open input carkit. \n");
#endif
	codec_input_path = CARKIT_INPUT;
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}


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


void open_input_funlight(void)
{
	unsigned long ssp_pcap_register_val;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open input funlight. \n");
#endif

	codec_input_path = FUNLIGHT_INPUT;
	
	set_gpio_va_sel_out_high();

	/* (2) set codec sample rate(FS_8K_16K=0) */
	/* CDC_CLK(000=13MHZ) bitclk output(SMB=0) audio IO1(DIG_AUD_IN=1) */
	ssp_pcap_register_val = PCAP_CDC_CLK_IN_13M0;
	SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER, ssp_pcap_register_val);

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_SMB);		/* codec master */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_FS_8K_16K);	/* 8K sample rate */
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_CODEC_DIG_AUD_IN);	/* DAI0 */
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

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_EXT_MIC_MUX);
}


void close_output_pcap_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap headset. \n");
#endif

	e680_boomer_path_mono_lineout();

	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
}


void close_output_pcap_louderspeaker(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap louderspeaker. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
}


void close_output_pcap_earpiece(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap earpiece. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);
}


void close_output_pcap_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap carkit. \n");
#endif
       e680_boomer_path_mono_lineout();
       EIHF_Mute(EIHF_MUTE);              //Mute EIHF 
	
       SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
       SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
       SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);

       Set_EMU_Mux_Switch(DEFAULT_USB_MODE);
}


void close_output_pcap_headjack(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap headjack. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
}


void close_output_pcap_bluetooth(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap bluetooth. \n");
#endif

}


void close_output_pcap_louderspeaker_mixed(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output pcap louderspeaker mixed. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
}


void close_output_ma3_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 headset. \n");
#endif
	
	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_ma3_louderspeaker(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 louderspeaker. \n");
#endif
	
	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_ma3_earpiece(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 earpiece. \n");
#endif

}


void close_output_ma3_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 carkit. \n");
#endif

}


void close_output_ma3_headjack(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 headjack. \n");
#endif
	
	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_ma3_bluetooth(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 bluetooth. \n");
#endif

}


void close_output_ma3_louderspeaker_mixed(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output ma3 louderspeaker mixed. \n");
#endif
	
	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_fm_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm headset. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_fm_louderspeaker(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm louderspeaker. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_fm_earpiece(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm earpiece. \n");
#endif

}


void close_output_fm_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm carkit. \n");
#endif

}


void close_output_fm_headjack(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm headjack. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_fm_bluetooth(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm bluetooth. \n");
#endif

}


void close_output_fm_louderspeaker_mixed(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output fm louderspeaker mixed. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_mixed_headset(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed headset. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_mixed_louderspeaker(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed louderspeaker. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_mixed_earpiece(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed earpiece. \n");
#endif

}


void close_output_mixed_carkit(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed carkit. \n");
#endif

}


void close_output_mixed_headjack(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed headjack. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


void close_output_mixed_bluetooth(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed bluetooth. \n");
#endif

}


void close_output_mixed_louderspeaker_mixed(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "close output mixed louderspeaker mixed. \n");
#endif

	e680_boomer_path_mono_lineout();
	
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
}


int open_output_pcap_headset(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap headset. \n");
#endif

	/* set pcap output path register AR_EN=1, AL_EN=1  */
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	set_boomer_mode_control_reg();
    
	set_output_gain_hw_reg();

    return ret;
}


int open_output_pcap_louderspeaker(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap louderspeaker. \n");
#endif
    
	/* set pcap output path register AR_EN=1, AL_EN=1  */
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_LOUDER_SPEAKER;
	
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	set_boomer_mode_control_reg();

	set_output_gain_hw_reg();

	return ret;
}


int open_output_pcap_earpiece(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap earpiece. \n");
#endif
    
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_MONO_OUT_MONO_LINEOUT;
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1_EN);							 
	set_boomer_mode_control_reg();

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
	  
	if(MIXERISSTEREO)
	       SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
	       SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);			 
	e680boomer.path=BOOMER_IN_STEREO_OUT_HEADPHONE;
				
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
						
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);

	set_boomer_mode_control_reg();

	set_output_gain_hw_reg();

	EIHF_Mute(EIHF_UNMUTE);
	 
	return ret;
}


int open_output_pcap_headjack(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap headjack. \n");
#endif
    
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;
		
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	set_boomer_mode_control_reg();

	set_output_gain_hw_reg();

	return ret;
}


int open_output_pcap_bluetooth(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap bluetooth. \n");
#endif

	return 0;
}


int open_output_pcap_louderspeaker_mixed(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output pcap louderspeaker mixed. \n");
#endif
    
	/* set pcap output path register AR_EN=1, AL_EN=1  */
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	set_boomer_mode_control_reg();

	set_output_gain_hw_reg();

	return ret;
}


int open_output_ma3_headset(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 headset. \n");
#endif
    
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();    
	set_output_gain_hw_reg();

	return ret;
}


int open_output_ma3_louderspeaker(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 louderspeaker. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_LOUDER_SPEAKER;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();    
	set_output_gain_hw_reg();

	return ret;
}


int open_output_ma3_earpiece(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 earpiece. \n");
#endif

	return 0;
}


int open_output_ma3_carkit(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 carkit. \n");
#endif

	return 0;
}


int open_output_ma3_headjack(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 headjack. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();    
	set_output_gain_hw_reg();

	return ret;
}


int open_output_ma3_bluetooth(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 bluetooth. \n");
#endif

	return 0;
}


int open_output_ma3_louderspeaker_mixed(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output ma3 louderspeaker mixed. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();    
	set_output_gain_hw_reg();

	return ret;
}


int open_output_fm_headset(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm headset. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);

	set_boomer_mode_control_reg();    
	set_output_gain_hw_reg();

	return ret;
}


int open_output_fm_louderspeaker(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm louderspeaker. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_LOUDER_SPEAKER;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);

	set_boomer_mode_control_reg();
	set_output_gain_hw_reg();

	return ret;
}


int open_output_fm_earpiece(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm earpiece. \n");
#endif

	return 0;
}


int open_output_fm_carkit(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm carkit. \n");
#endif

	return 0;
}


int open_output_fm_headjack(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm headjack. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);

	set_boomer_mode_control_reg();
	set_output_gain_hw_reg();

	return ret;
}


int open_output_fm_bluetooth(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm bluetooth. \n");
#endif

	return 0;
}


int open_output_fm_louderspeaker_mixed(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output fm louderspeaker mixed. \n");
#endif

	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);

	set_boomer_mode_control_reg();
	set_output_gain_hw_reg();

	return ret;
}


int open_output_mixed_headset(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed headset. \n");
#endif
    
	/* set pcap output path register AR_EN=1, AL_EN=1  */
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;
    
	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();    
	set_output_gain_hw_reg();

	return ret;
}


int open_output_mixed_louderspeaker(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed louderspeaker. \n");
#endif
    
	/* set pcap output path register AR_EN=1, AL_EN=1  */
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_LOUDER_SPEAKER;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();
	set_output_gain_hw_reg();

	return ret;
}


int open_output_mixed_earpiece(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed earpiece. \n");
#endif

	return 0;
}


int open_output_mixed_carkit(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed carkit. \n");
#endif

	return 0;
}


int open_output_mixed_headjack(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed headjack. \n");
#endif
    
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL_6DB);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_STEREO_OUT_HEADPHONE;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();
	set_output_gain_hw_reg();

	return ret;
}


int open_output_mixed_bluetooth(long val)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed bluetooth. \n");
#endif

	return 0;
}


int open_output_mixed_louderspeaker_mixed(long val)
{
	int ret;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "open output mixed louderspeaker mixed. \n");
#endif
    
	/* set pcap output path register AR_EN=1, AL_EN=1  */
	if(MIXERISSTEREO)
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_R_L_STEREO);
	else
		SSP_PCAP_MONO_set(PCAP_MONO_PGA_RL);
	e680boomer.path = BOOMER_IN_MONO_AND_STEREO_OUT_LOUDER_SPEAKER;

	codec_output_path = GET_OUTPUT_PATH(val);
	codec_output_base = GET_OUTPUT_BASE(val);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AHS_CONFIG);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ARIGHT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_ALEFT_EN);
	ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_PGA_IN_SW);
	
	set_boomer_mode_control_reg();
	set_output_gain_hw_reg();

	return ret;
}


void kernel_set_oscc_13m_clock(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "midi set oscc 13m clock \n" );
#endif

	if( DSPUSE13MCLK == 0 )
		OSCC |= 0x00000008;
}


void kernel_clr_oscc_13m_clock(void)
{
#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG "midi clr oscc 13m clock \n");
#endif

	if( (DSPUSE13MCLK == 0 ) && (DSP16USE13MCLK == 0) )	/* for app will close midi after dsp16 opened under some cases */
		OSCC &= ~0x00000008;
}


void pcap_use_ap_13m_clock(void)
{
	if( MIDIUSE13MCLK == 0 )
		OSCC |= 0x00000008;
	set_GPIO_mode(AP_13MHZ_OUTPUT_PIN | GPIO_ALT_FN_3_OUT);

	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_CODEC_CLK_IN_SEL);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ST_DAC_ST_DAC_CLK_IN_SEL);	
}


void pcap_use_bp_13m_clock(void)
{
	if( MIDIUSE13MCLK == 0 )
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
			printk(EZXOSS_DEBUG " E680 boomer path = 0X%X before AP sleep. \n", e680boomer.path);
#endif
			if( (audioonflag & (PHONE_DEVICE|FM_DEVICE)) == 0  ){
				e680_boomer_path_tmp_shutdown();
				SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AUDIO_LOWPWR);
			}
			break;
		case PM_RESUME:
#ifdef EZX_OSS_DEBUG
			printk(EZXOSS_DEBUG " E680 after AP sleep.\n");
#endif
			SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_TX_AUD_AMPS_AUDIO_LOWPWR);
			set_output_gain_hw_reg();
			if( gpio_flt_sel_status )
				Clear_Haptics_GPIO();
			else
				Set_Haptics_GPIO();
			if(audioonflag)
				(*mixer_open_output_path[OUTPUT_BASE_TYPE(codec_output_base)][codec_output_path])(codec_output_base|codec_output_path);
			e680_boomer_path_restore();
			break;
	}
	return 0;
}
#endif


void mute_output_to_avoid_pcap_noise(void)
{
	e680_boomer_path_tmp_mono_lineout();
}


void undo_mute_output_to_avoid_pcap_noise(void)
{
	e680_boomer_path_restore();
}


void Set_Haptics_GPIO(void)
{
	e680_boomer_path_tmp_mono_lineout();

	set_GPIO_mode(GPIO_FLT_SEL_BUL | GPIO_OUT);
	clr_GPIO(GPIO_FLT_SEL_BUL);
	PGSR(GPIO_FLT_SEL_BUL) &= ~GPIO_bit(GPIO_FLT_SEL_BUL); 
	gpio_flt_sel_status = 0;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " set haptics GPIO \n");
#endif

    e680_boomer_path_restore();
}


void Clear_Haptics_GPIO(void)
{
	e680_boomer_path_tmp_mono_lineout();

	set_GPIO_mode(GPIO_FLT_SEL_BUL | GPIO_OUT);
	set_GPIO(GPIO_FLT_SEL_BUL);
	PGSR(GPIO_FLT_SEL_BUL) |= GPIO_bit(GPIO_FLT_SEL_BUL); 
	gpio_flt_sel_status = 1;

#ifdef EZX_OSS_DEBUG
	printk(EZXOSS_DEBUG " clear haptics GPIO \n");
#endif

	e680_boomer_path_restore();
}


#endif  /* CONFIG_ARCH_EZX_E680 */



