/*
 *  linux/drivers/sound/ezx-e680.h
 *
 *
 *  Description:  header file for ezx-e680.c
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
 *  Jin Lihong(w20076) Jan 12,2004,LIBdd68327  Created,Make the e680 louder speaker work.
 *  Jin Lihong(w20076) Feb.23,2004,LIBdd79747  add e680 audio path switch and gain setting funcs
 *  Jin Lihong(w20076) Mar.15,2004,LIBdd86574  mixer bug fix
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  close dsp protection,and add 3d control interface for app
 *  Jin Lihong(w20076) Apr.24,2004,LIBee03164  reduce music noise, add new pathes for haptics
 *  Jin Lihong(w20076) Jun.15,2004,LIBee21625  boomer's power management
 *  Jin Lihong(w20076) Jun.22,2004,LIBee24284  mixer power save
 *  Wu Hongxing(w20691)Jul.07,2004 LIBee29664  ap 13m clock usage.
 *
 */

#ifndef EZX_E680_H
#define EZX_E680_H
 
#include <linux/config.h>
#include <linux/pm.h>

#ifdef CONFIG_ARCH_EZX_E680

//#define EZX_OSS_DEBUG          /* debug switch for all ezx oss src files excluding ezx-audio.c */
#undef EZX_OSS_DEBUG
//#define EZX_OSS_AUDIO_DEBUG    /* debug switch for src file ezx-audio.c */
#undef EZX_OSS_AUDIO_DEBUG

/*
#ifdef EZX_OSS_DBG
#define OSSPRINTF(fmt,args...) printf(fmt,##args) 
#else 
#define OSSPRINTF(fmt,args...) { }
#endif 
*/


#define MIXERISSTEREO	( audioonflag & (DSP_DEVICE|MIDI_DEVICE|FM_DEVICE) )
#define MIDIUSE13MCLK	( audioonflag & MIDI_DEVICE )
#define DSPUSE13MCLK	( audioonflag & DSP_DEVICE )
#define DSP16USE13MCLK	( audioonflag & DSP16_DEVICE )


typedef enum{
	PHONE_DEVICE	= 0x01,
	DSP_DEVICE		= 0x02,
	DSP16_DEVICE	= 0x04,
	AUDIO_DEVICE	= 0x08,
	MIDI_DEVICE		= 0x10,
	FM_DEVICE		= 0x20
}audio_dev_type;

typedef enum{
	LOUDERSPEAKER_ON_HEADSET_ON_3D,
	LOUDERSPEAKER_ON_HEADSET_OFF_3D,
	LOUDERSPEAKER_OFF_HEADSET_ON_3D,
	LOUDERSPEAKER_OFF_HEADSET_OFF_3D
}boomer_3d_status;


void e680_boomer_init(void);
void e680_boomer_shutdown(void);
void e680_boomer_path_mono_lineout(void);
void e680_boomer_path_tmp_mono_lineout(void);
void e680_boomer_path_tmp_shutdown(void);
void e680_boomer_path_restore(void);

void poweron_mixer( audio_dev_type type );
void shutdown_mixer( audio_dev_type type );
void mixer_not_in_use(void);

void set_output_gain_hw_reg(void);
void set_input_gain_hw_reg(void);

void set_boomer_3d_status(boomer_3d_status status);
boomer_3d_status get_boomer_3d_status(void);

void set_gpio_va_sel_out_high(void);
void set_gpio_va_sel_out_low(void);

void close_input_carkit(void);
void close_input_handset(void);
void close_input_headset(void);
void close_input_funlight(void);

void open_input_carkit(void);
void open_input_handset(void);
void open_input_headset(void);
void open_input_funlight(void);

void close_output_pcap_headset(void);
void close_output_pcap_louderspeaker(void);
void close_output_pcap_earpiece(void);
void close_output_pcap_carkit(void);
void close_output_pcap_headjack(void);
void close_output_pcap_bluetooth(void);
void close_output_pcap_louderspeaker_mixed(void);

void close_output_ma3_headset(void);
void close_output_ma3_louderspeaker(void);
void close_output_ma3_earpiece(void);
void close_output_ma3_carkit(void);
void close_output_ma3_headjack(void);
void close_output_ma3_bluetooth(void);
void close_output_ma3_louderspeaker_mixed(void);

void close_output_fm_headset(void);
void close_output_fm_louderspeaker(void);
void close_output_fm_earpiece(void);
void close_output_fm_carkit(void);
void close_output_fm_headjack(void);
void close_output_fm_bluetooth(void);
void close_output_fm_louderspeaker_mixed(void);

void close_output_mixed_headset(void);
void close_output_mixed_louderspeaker(void);
void close_output_mixed_earpiece(void);
void close_output_mixed_carkit(void);
void close_output_mixed_headjack(void);
void close_output_mixed_bluetooth(void);
void close_output_mixed_louderspeaker_mixed(void);

int open_output_pcap_headset(long val);
int open_output_pcap_louderspeaker(long val);
int open_output_pcap_earpiece(long val);
int open_output_pcap_carkit(long val);
int open_output_pcap_headjack(long val);
int open_output_pcap_bluetooth(long val);
int open_output_pcap_louderspeaker_mixed(long val);

int open_output_ma3_headset(long val);
int open_output_ma3_louderspeaker(long val);
int open_output_ma3_earpiece(long val);
int open_output_ma3_carkit(long val);
int open_output_ma3_headjack(long val);
int open_output_ma3_bluetooth(long val);
int open_output_ma3_louderspeaker_mixed(long val);

int open_output_fm_headset(long val);
int open_output_fm_louderspeaker(long val);
int open_output_fm_earpiece(long val);
int open_output_fm_carkit(long val);
int open_output_fm_headjack(long val);
int open_output_fm_bluetooth(long val);
int open_output_fm_louderspeaker_mixed(long val);

int open_output_mixed_headset(long val);
int open_output_mixed_louderspeaker(long val);
int open_output_mixed_earpiece(long val);
int open_output_mixed_carkit(long val);
int open_output_mixed_headjack(long val);
int open_output_mixed_bluetooth(long val);
int open_output_mixed_louderspeaker_mixed(long val);

void kernel_set_oscc_13m_clock(void);
void kernel_clr_oscc_13m_clock(void);
void pcap_use_ap_13m_clock(void);
void pcap_use_bp_13m_clock(void);

int mixer_hw_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data);
void mute_output_to_avoid_pcap_noise(void);
void undo_mute_output_to_avoid_pcap_noise(void);

void Set_Haptics_GPIO(void);
void Clear_Haptics_GPIO(void);


#endif  /* CONFIG_ARCH_EZX_E680 */


#endif

