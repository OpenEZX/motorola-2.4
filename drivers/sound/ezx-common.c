/*
 * linux/drivers/sound/ezx-common.c
 *
 * common functions implementation for ezx audio drivers
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
 *  Jan 13,2004 - (Motorola) Created, Make the e680 louder speaker work.
 *  Feb.23,2004 - (Motorola) add e680 audio path switch and gain setting funcs
 *  Mar.15,2004 - (Motorola) mixer bug fix
 *  Mar.25,2004 - (Motorola) a780 new gain setting interface
 *  Apr.24,2004 - (Motorola) reduce music noise, add new pathes for haptics
 *  June.24,2004 -(Motorola) Add sound path for EMU PIHF
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

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include "ezx-common.h"


int codec_output_gain = EZX_OSS_DEFAULT_OUTPUT_LOGICAL_GAIN;	/* A2,loudspeaker, gain level */
output_enum codec_output_path = LOUDERSPEAKER_OUT;
output_base_enum codec_output_base = PCAP_BASE;

int codec_input_gain = EZX_OSS_DEFAULT_AUDIG_LOGICAL_GAIN;	/* A5,mic,gain=+17db */
input_enum codec_input_path = HANDSET_INPUT;
int audioonflag = 0;
int micinflag = 0;


#ifdef CONFIG_ARCH_EZX_E680

void (*mixer_close_input_path[INPUT_TOTAL_TYPES])(void) =
{
    	close_input_carkit,
    	close_input_handset,
    	close_input_headset,
    	close_input_funlight
};

void (*mixer_open_input_path[INPUT_TOTAL_TYPES])(void) =
{
    	open_input_carkit,
    	open_input_handset,
    	open_input_headset,
    	open_input_funlight
};

void (*mixer_close_output_path[][OUTPUT_TOTAL_TYPES])(void) =
{
    	{
    		close_output_pcap_headset,
    		close_output_pcap_louderspeaker,
    		close_output_pcap_earpiece,
    		close_output_pcap_carkit,
    		close_output_pcap_headjack,
    		close_output_pcap_bluetooth,
    		close_output_pcap_louderspeaker_mixed
        },
    	{
    		close_output_ma3_headset,
    		close_output_ma3_louderspeaker,
    		close_output_ma3_earpiece,
    		close_output_ma3_carkit,
    		close_output_ma3_headjack,
    		close_output_ma3_bluetooth,
    		close_output_ma3_louderspeaker_mixed
        },
    	{
    		close_output_fm_headset,
    		close_output_fm_louderspeaker,
    		close_output_fm_earpiece,
    		close_output_fm_carkit,
    		close_output_fm_headjack,
    		close_output_fm_bluetooth,
    		close_output_fm_louderspeaker_mixed
        },
    	{
    		close_output_mixed_headset,
    		close_output_mixed_louderspeaker,
    		close_output_mixed_earpiece,
    		close_output_mixed_carkit,
    		close_output_mixed_headjack,
    		close_output_mixed_bluetooth,
    		close_output_mixed_louderspeaker_mixed
        }
};

int (*mixer_open_output_path[][OUTPUT_TOTAL_TYPES])(long val) =
{
    	{
    		open_output_pcap_headset,
    		open_output_pcap_louderspeaker,
    		open_output_pcap_earpiece,
    		open_output_pcap_carkit,
    		open_output_pcap_headjack,
    		open_output_pcap_bluetooth,
    		open_output_pcap_louderspeaker_mixed
        },
    	{
    		open_output_ma3_headset,
    		open_output_ma3_louderspeaker,
    		open_output_ma3_earpiece,
    		open_output_ma3_carkit,
    		open_output_ma3_headjack,
    		open_output_ma3_bluetooth,
    		open_output_ma3_louderspeaker_mixed
        },
    	{
    		open_output_fm_headset,
    		open_output_fm_louderspeaker,
    		open_output_fm_earpiece,
    		open_output_fm_carkit,
    		open_output_fm_headjack,
    		open_output_fm_bluetooth,
    		open_output_fm_louderspeaker_mixed
        },
    	{
    		open_output_mixed_headset,
    		open_output_mixed_louderspeaker,
    		open_output_mixed_earpiece,
    		open_output_mixed_carkit,
    		open_output_mixed_headjack,
    		open_output_mixed_bluetooth,
    		open_output_mixed_louderspeaker_mixed
        }
};
#endif  /* CONFIG_ARCH_EZX_E680 */


#ifdef CONFIG_ARCH_EZX_A780
void (*mixer_close_input_path[INPUT_TOTAL_TYPES])(void) =
{
    	close_input_carkit,
    	close_input_handset,
#if EMU_PIHF_FEATURE
    	close_input_headset,
    	close_input_pihf_carkit
#else
    	close_input_headset
#endif

};

void (*mixer_open_input_path[INPUT_TOTAL_TYPES])(void) =
{
    	open_input_carkit,
   	open_input_handset,
#if EMU_PIHF_FEATURE
    	open_input_headset,
    	open_input_pihf_carkit
#else
    	open_input_headset
#endif
};

void (*mixer_close_output_path[][OUTPUT_TOTAL_TYPES])(void) =
{
    	{
    		close_output_pcap_headset,
    		close_output_pcap_louderspeaker,
    		close_output_pcap_earpiece,
    		close_output_pcap_carkit,
    		close_output_pcap_headjack,
#if EMU_PIHF_FEATURE
    		close_output_pcap_bluetooth,
    		close_output_pcap_pihf_carkit
#else
    		close_output_pcap_bluetooth
#endif
        }
};

int (*mixer_open_output_path[][OUTPUT_TOTAL_TYPES])(long val) =
{
    	{
    		open_output_pcap_headset,
    		open_output_pcap_louderspeaker,
    		open_output_pcap_earpiece,
    		open_output_pcap_carkit,
    		open_output_pcap_headjack,
#if EMU_PIHF_FEATURE
    		open_output_pcap_bluetooth,
    		open_output_pcap_pihf_carkit
#else
    		open_output_pcap_bluetooth
#endif
        }
};
#endif


