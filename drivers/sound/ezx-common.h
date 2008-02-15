/*
 * linux/drivers/sound/ezx-common.h
 *
 * header file for ezx-common.c
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
 *  Jan 12,2004 - (Motorola) Created,Make the e680 louder speaker work.
 *  Feb.23,2004 - (Motorola) add e680 audio path switch and gain setting funcs
 *  Mar.15,2004 - (Motorola) mixer bug fix
 *  Mar.25,2004 - (Motorola) a780 new gain setting interface
 *  Apr.24,2004 - (Motorola) reduce music noise, add new pathes for haptics
 *  Apr.22.2004 - (Motorola) Add EMU carKit
 *  Jun.15.2004 - (Motorola) Support EMU PIHF feature
 *
 */

#ifndef EZX_COMMON_H
#define EZX_COMMON_H

#include "ezx-e680.h"
#include "ezx-a780.h"
#include "../misc/ssp_pcap.h"

#define	EZXOSS_EMERG	"Snd driver w20076 <0 EMERG>: "		/* system is unusable */
#define	EZXOSS_ALERT	"Snd driver w20076 <1 ALERT>: "		/* action must be taken immediately */
#define	EZXOSS_CRIT		"Snd driver w20076 <2 CRIT>: "		/* critical conditions */
#define	EZXOSS_ERR		"Snd driver w20076 <3 ERR>: "		/* error conditions */
#define	EZXOSS_WARNING	"Snd driver w20076 <4 WARNING>: "	/* warning conditions */
#define	EZXOSS_NOTICE	"Snd driver w20076 <5 NOTICE>: "	/* normal but significant condition */
#define	EZXOSS_INFO		"Snd driver w20076 <6 INFO>: "		/* informational */
#define	EZXOSS_DEBUG	"Snd driver w20076 <7 DEBUG>: "		/* debug-level messages */

#define MONODEVOPENED	( audioonflag & (PHONE_DEVICE|DSP16_DEVICE|AUDIO_DEVICE) )
#define PHONEDEVOPENED	( audioonflag & PHONE_DEVICE )

#define EZX_OSS_MIN_LOGICAL_GAIN 0
#define EZX_OSS_MAX_LOGICAL_GAIN 100
#define EZX_OSS_DEFAULT_OUTPUT_LOGICAL_GAIN 74		/* +9db, same as a760 */
#define EZX_OSS_DEFAULT_AUDIG_LOGICAL_GAIN  55		/* +17db, same as a760 */
#define EZX_OSS_DEFAULT_MIC2IG_LOGICAL_GAIN 55		/* +17db, same as a760 */

#define PCAP_OUTPUT_GAIN_MIN_REG_VAL 0
#define PCAP_OUTPUT_GAIN_MAX_REG_VAL 15
#define PCAP_INPUT_GAIN_MIN_REG_VAL  0
#define PCAP_INPUT_GAIN_MAX_REG_VAL  31
/*EMU CarKit Mute control signal  */
#define EIHF_UNMUTE            0x01
#define EIHF_MUTE              0x00

#define DEFAULT_USB_MODE       0x00
#define MONO_AUDIO_MODE        0x02
#define SETERO_AUDIO_MODE      0x03

#define PCAP_OUTPUT_GAIN_REG_VAL_FROM_LOGIC  (codec_output_gain*PCAP_OUTPUT_GAIN_MAX_REG_VAL/EZX_OSS_MAX_LOGICAL_GAIN)
#define PCAP_INPUT_AUDIG_REG_VAL_FROM_LOGIC  (codec_input_gain*PCAP_INPUT_GAIN_MAX_REG_VAL/EZX_OSS_MAX_LOGICAL_GAIN)
#define PCAP_INPUT_MIC2IG_REG_VAL_FROM_LOGIC (codec_input_gain*PCAP_INPUT_GAIN_MAX_REG_VAL/EZX_OSS_MAX_LOGICAL_GAIN)

#if EMU_PIHF_FEATURE
typedef enum{
    HEADSET_OUT,			/* ear-phone : stereo headset */
    LOUDERSPEAKER_OUT,			/* A2_OUTPUT in a760 */
    EARPIECE_OUT,		        /* A1_OUTPUT in a760 */
    CARKIT_OUT,				/* A4_OUTPUT in a760 */
    HEADJACK_OUT,			/* mono headjack, HEADJACK_OUTPUT in a760 */
    BLUETOOTH_OUT,			/* bluetooth headset */
    PIHF_CARKIT_OUT			/* A4_OUTPUT in a760 */
}output_enum;
#else /* EMU_PIHF_FEATURE */
typedef enum{
    HEADSET_OUT,			/* ear-phone : stereo headset */
    LOUDERSPEAKER_OUT,			/* A2_OUTPUT in a760 */
    EARPIECE_OUT,		        /* A1_OUTPUT in a760 */
    CARKIT_OUT,				/* A4_OUTPUT in a760 */
    HEADJACK_OUT,			/* mono headjack, HEADJACK_OUTPUT in a760 */
    BLUETOOTH_OUT,			/* bluetooth headset */
    LOUDERSPEAKER_MIXED_OUT		/* e680 louderspeaker out, boomer input is stereo+mono, mono is aimed for haptics signals */
}output_enum;
#endif /* EMU_PIHF_FEAURE */

typedef enum{
    PCAP_BASE	= 0x0000 ,		/* used to tell that we are setting the PCAP */
    MA3_BASE	= 0x0100 ,		/* MA3 Midi device */
    FM_BASE	= 0x0200 ,		/* FM radio device */
    MIXED_BASE	= 0x0300 		/* Both MA3 device and PCAP device are used */
}output_base_enum;

#if EMU_PIHF_FEATURE
typedef enum{
    CARKIT_INPUT,		/* EXT_MIC */
    HANDSET_INPUT ,		/* A5 */
    HEADSET_INPUT ,		/* A3 */
    PIHF_CARKIT_INPUT		/* EXT_MIC */
}input_enum;
#else /* EMU_PIHF_FEATURE */
typedef enum{
    CARKIT_INPUT,		/* EXT_MIC */
    HANDSET_INPUT ,		/* A5 */
    HEADSET_INPUT ,		/* A3 */
    FUNLIGHT_INPUT		/* E680 funlight */
}input_enum;
#endif /* EMU_PIHF_FEATURE */


#define INPUT_PATH_MIN CARKIT_INPUT

#ifdef CONFIG_ARCH_EZX_A780
#if EMU_PIHF_FEATURE
#define INPUT_PATH_MAX PIHF_CARKIT_INPUT
#define INPUT_TOTAL_TYPES 4							/* = HEADSET_INPUT+1 */
#define OUTPUT_PATH_MAX PIHF_CARKIT_OUT
#define OUTPUT_BASE_MAX PCAP_BASE
#define OUTPUT_TOTAL_TYPES 7						/* = BLUETOOTH_OUT+1 */
#else /* EMU_PIHF_FEATURE */
#define INPUT_PATH_MAX HEADSET_INPUT
#define INPUT_TOTAL_TYPES 3							/* = HEADSET_INPUT+1 */
#define OUTPUT_PATH_MAX BLUETOOTH_OUT
#define OUTPUT_BASE_MAX PCAP_BASE
#define OUTPUT_TOTAL_TYPES 6						/* = BLUETOOTH_OUT+1 */
#endif /* EMU_PIHF_FEATURE */
#endif

#ifdef CONFIG_ARCH_EZX_E680
#define INPUT_PATH_MAX FUNLIGHT_INPUT
#define INPUT_TOTAL_TYPES 4							/* = FUNLIGHT_INPUT + 1 */
#define OUTPUT_PATH_MAX LOUDERSPEAKER_MIXED_OUT
#define OUTPUT_BASE_MAX MIXED_BASE
#define OUTPUT_TOTAL_TYPES 7						/* = LOUDERSPEAKER_MIXED_OUT+1 */
#endif

#define OUTPUT_BASE_MEANINGFUL_START_BIT 8
#define OUTPUT_BASE_TYPE(x) (x >>OUTPUT_BASE_MEANINGFUL_START_BIT)
#define OUTPUT_BASE_TOTAL_TYPES (OUTPUT_BASE_MAX>>OUTPUT_BASE_MEANINGFUL_START_BIT +1)
#define GET_OUTPUT_BASE(x) (x&0xFF00)
#define GET_OUTPUT_PATH(x) (x&0x00FF)


extern int codec_output_gain, codec_input_gain;
extern output_enum codec_output_path;
extern input_enum codec_input_path;
extern output_base_enum codec_output_base;
extern int audioonflag;
extern int micinflag;

extern void (*mixer_close_output_path[][OUTPUT_TOTAL_TYPES])(void);
extern int  (*mixer_open_output_path[][OUTPUT_TOTAL_TYPES])(long val);
extern void (*mixer_close_input_path[INPUT_TOTAL_TYPES])(void);
extern void (*mixer_open_input_path[INPUT_TOTAL_TYPES])(void);


#endif


