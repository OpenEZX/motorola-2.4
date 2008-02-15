/*
 *  linux/drivers/sound/ezx-a780.h
 *
 *
 *  Description:  header file for ezx-a780.c
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
 *  Jin Lihong(w20076) Mar.25,2004,LIBdd90846  a780 new gain setting interface
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  close dsp protection,and add 3d control interface for app
 *  Jin Lihong(w20076) Jun.22,2004,LIBee24284  mixer power save
 *  Cheng Xuefeng(a2491c) Jun.24,2004,LIBdd95397  Add EMU PIHF carkit sound path
 *
 */

#ifndef EZX_A780_H
#define EZX_A780_H

#include <linux/config.h>

#ifdef CONFIG_ARCH_EZX_A780

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


typedef enum{
	PHONE_DEVICE	= 0x01,
	DSP_DEVICE		= 0x02,
	DSP16_DEVICE	= 0x04,
	AUDIO_DEVICE	= 0x08
}audio_dev_type;

typedef enum{
	HW_ATTENUATION_USED,
	HW_ATTENUATION_BYPASSED
}hw_noise_attenuation;


void close_input_carkit(void);
void close_input_handset(void);
void close_input_headset(void);
#if EMU_PIHF_FEATURE
void close_input_pihf_carkit(void);
#endif

void open_input_carkit(void);
void open_input_handset(void);
void open_input_headset(void);
#if EMU_PIHF_FEATURE
void open_input_pihf_carkit(void);
#endif

void close_output_pcap_headset(void);
void close_output_pcap_louderspeaker(void);
void close_output_pcap_earpiece(void);
void close_output_pcap_carkit(void);
void close_output_pcap_headjack(void);
void close_output_pcap_bluetooth(void);
#if EMU_PIHF_FEATURE
void close_output_pcap_pihf_carkit(void);
#endif

int open_output_pcap_headset(long val);
int open_output_pcap_louderspeaker(long val);
int open_output_pcap_earpiece(long val);
int open_output_pcap_carkit(long val);
int open_output_pcap_headjack(long val);
int open_output_pcap_bluetooth(long val);
#if EMU_PIHF_FEATURE
int open_output_pcap_pihf_carkit(long val);
#endif

void set_output_gain_hw_reg(void);
void set_input_gain_hw_reg(void);

void poweron_mixer( audio_dev_type type );
void shutdown_mixer( audio_dev_type type );
void mixer_not_in_use(void);

void use_hw_noise_attenuate(void);
void bypass_hw_noise_attenuate(void);

void pcap_use_ap_13m_clock(void);
void pcap_use_bp_13m_clock(void);

void mute_output_to_avoid_pcap_noise(void);
void undo_mute_output_to_avoid_pcap_noise(void);

int mixer_hw_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data);

#endif


#endif


