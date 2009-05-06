/*
 * Copyright 2004 Motorola, Inc. 
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  
 * 02111-1307, USA
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/moto_accy.h>
#include <linux/power_ic.h>
#include <linux/lights_funlights.h>

#include "../module/emu.h"
#include "../module/emu_eoc.h"
#include "../module/tcmd_ioctl.h"


/* Number of cycles of read/write tests to be performed. */
#define NUM_READ_WRITE_TESTS   20

/*********************************************************************************************
 *
 * masks, etc. for regulators...these may everntually be replaced by a driver module so we
 * don't have to know all this stuff.
 *
 */
#define REGULATOR_NUM_SW_REGS         3
#define REGULATOR_NUM_V_REGS          10
#define REGULATOR_NUM_AUX_REGS        4

/* SWCTRL register. */
#define REGULATOR_SW1_ONOFF_POS       1
#define REGULATOR_SW1_ONOFF_MASK      0x1
#define REGULATOR_SW1_LEVEL_POS       2
#define REGULATOR_SW1_LEVEL_MASK      0xF

#define REGULATOR_SW2_ONOFF_POS       6
#define REGULATOR_SW2_ONOFF_MASK      0x1
#define REGULATOR_SW2_LEVEL_POS       7
#define REGULATOR_SW2_LEVEL_MASK      0xF

#define REGULATOR_SW3_ONOFF_POS       11
#define REGULATOR_SW3_ONOFF_MASK      0x1
#define REGULATOR_SW3_LEVEL_POS       12
#define REGULATOR_SW3_LEVEL_MASK      0x3

/* VREG1 register. */
#define REGULATOR_V1_ONOFF_POS        1
#define REGULATOR_V1_ONOFF_MASK       0x1
#define REGULATOR_V1_LEVEL_POS        2
#define REGULATOR_V1_LEVEL_MASK       0x7
#define REGULATOR_V1_LOWPWR_POS       0
#define REGULATOR_V1_LOWPWR_MASK      0x1
#define REGULATOR_V1_STBY_CTRL_POS    18
#define REGULATOR_V1_STBY_CTRL_MASK   0x1

#define REGULATOR_V2_ONOFF_POS        5
#define REGULATOR_V2_ONOFF_MASK       0x1
#define REGULATOR_V2_LEVEL_POS        6
#define REGULATOR_V2_LEVEL_MASK       0x1
#define REGULATOR_V2_LOWPWR_POS       22
#define REGULATOR_V2_LOWPWR_MASK      0x1
#define REGULATOR_V2_STBY_CTRL_POS    19
#define REGULATOR_V2_STBY_CTRL_MASK   0x1

#define REGULATOR_V3_ONOFF_POS        7
#define REGULATOR_V3_ONOFF_MASK       0x1
#define REGULATOR_V3_LEVEL_POS        8
#define REGULATOR_V3_LEVEL_MASK       0x7
#define REGULATOR_V3_LOWPWR_POS       23
#define REGULATOR_V3_LOWPWR_MASK      0x1
#define REGULATOR_V3_STBY_CTRL_POS    20
#define REGULATOR_V3_STBY_CTRL_MASK   0x1

#define REGULATOR_V4_ONOFF_POS        11
#define REGULATOR_V4_ONOFF_MASK       0x1
#define REGULATOR_V4_LEVEL_POS        12
#define REGULATOR_V4_LEVEL_MASK       0x7
#define REGULATOR_V4_LOWPWR_POS       24
#define REGULATOR_V4_LOWPWR_MASK      0x1
#define REGULATOR_V4_STBY_CTRL_POS    21
#define REGULATOR_V4_STBY_CTRL_MASK   0x1

#define REGULATOR_V5_ONOFF_POS        15
#define REGULATOR_V5_ONOFF_MASK       0x1
#define REGULATOR_V5_LEVEL_POS        16
#define REGULATOR_V5_LEVEL_MASK       0x3

/* VREG2 register. */
#define REGULATOR_V5_LOWPWR_POS       19
#define REGULATOR_V5_LOWPWR_MASK      0x1
#define REGULATOR_V5_STBY_CTRL_POS    12
#define REGULATOR_V5_STBY_CTRL_MASK   0x1

#define REGULATOR_V6_ONOFF_POS        1
#define REGULATOR_V6_ONOFF_MASK       0x1
#define REGULATOR_V6_LEVEL_POS        2
#define REGULATOR_V6_LEVEL_MASK       0x1
#define REGULATOR_V6_LOWPWR_POS       20
#define REGULATOR_V6_LOWPWR_MASK      0x1
#define REGULATOR_V6_STBY_CTRL_POS    14
#define REGULATOR_V6_STBY_CTRL_MASK   0x1

#define REGULATOR_V7_ONOFF_POS        3
#define REGULATOR_V7_ONOFF_MASK       0x1
#define REGULATOR_V7_LEVEL_POS        4
#define REGULATOR_V7_LEVEL_MASK       0x1
#define REGULATOR_V7_LOWPWR_POS       21
#define REGULATOR_V7_LOWPWR_MASK      0x1
#define REGULATOR_V7_STBY_CTRL_POS    15
#define REGULATOR_V7_STBY_CTRL_MASK   0x1

#define REGULATOR_V8_ONOFF_POS        5
#define REGULATOR_V8_ONOFF_MASK       0x1
#define REGULATOR_V8_LEVEL_POS        6
#define REGULATOR_V8_LEVEL_MASK       0x7
#define REGULATOR_V8_LOWPWR_POS       22
#define REGULATOR_V8_LOWPWR_MASK      0x1
#define REGULATOR_V8_STBY_CTRL_POS    16
#define REGULATOR_V8_STBY_CTRL_MASK   0x1

#define REGULATOR_V9_ONOFF_POS        9
#define REGULATOR_V9_ONOFF_MASK       0x1
#define REGULATOR_V9_LEVEL_POS        10
#define REGULATOR_V9_LEVEL_MASK       0x3
#define REGULATOR_V9_LOWPWR_POS       23
#define REGULATOR_V9_LOWPWR_MASK      0x1
#define REGULATOR_V9_STBY_CTRL_POS    17
#define REGULATOR_V9_STBY_CTRL_MASK   0x1

#define REGULATOR_V10_ONOFF_POS       10
#define REGULATOR_V10_ONOFF_MASK      0x1
#define REGULATOR_V10_LEVEL_POS       0
#define REGULATOR_V10_LEVEL_MASK      0   /* V10's level can't be adjusted. */
#define REGULATOR_V10_LOWPWR_POS      24
#define REGULATOR_V10_LOWPWR_MASK     0x1
#define REGULATOR_V10_STBY_CTRL_POS   18
#define REGULATOR_V10_STBY_CTRL_MASK  0x1

/* AUX_VREG Register. */
#define REGULATOR_AUX1_ONOFF_POS      1
#define REGULATOR_AUX1_ONOFF_MASK     0x1
#define REGULATOR_AUX1_LEVEL_POS      2
#define REGULATOR_AUX1_LEVEL_MASK     0x3
#define REGULATOR_AUX1_LOWPWR_POS     23
#define REGULATOR_AUX1_LOWPWR_MASK    0x1
#define REGULATOR_AUX1_STBY_CTRL_POS  22
#define REGULATOR_AUX1_STBY_CTRL_MASK 0x1

#define REGULATOR_AUX2_ONOFF_POS      4
#define REGULATOR_AUX2_ONOFF_MASK     0x1
#define REGULATOR_AUX2_LEVEL_POS      5
#define REGULATOR_AUX2_LEVEL_MASK     0x3
/* No controls for standby pin or low power mode. */

#define REGULATOR_AUX3_ONOFF_POS      7
#define REGULATOR_AUX3_ONOFF_MASK     0x1
#define REGULATOR_AUX3_LEVEL_POS      8
#define REGULATOR_AUX3_LEVEL_MASK     0xF
/* No controls for standby pin or low power mode. */


#define REGULATOR_AUX4_ONOFF_POS      12
#define REGULATOR_AUX4_ONOFF_MASK     0x1
#define REGULATOR_AUX4_LEVEL_POS      13
#define REGULATOR_AUX4_LEVEL_MASK     0x3
/* No controls for standby pin or low power mode. */


#define REGULATOR_UNUSED 0xDEADBEEF

typedef struct
{
    int onoff_pos;
    int onoff_mask;
    int level_pos;
    int level_mask;
    int lowpwr_pos;
    int lowpwr_mask;
    int stby_ctrl_pos;
    int stby_ctrl_mask;   
} REGULATOR_BITS_T;

/*********************************************************************************************
 *
 * Various tables for regulators...
 *
 */ 
 
static const REGULATOR_BITS_T regulator_sw_bits[REGULATOR_NUM_SW_REGS] =
{
    {   /* Switchmode regulator 1. */
        REGULATOR_SW1_ONOFF_POS, REGULATOR_SW1_ONOFF_MASK,
        REGULATOR_SW1_LEVEL_POS, REGULATOR_SW1_LEVEL_MASK,
        REGULATOR_UNUSED, REGULATOR_UNUSED, 
        REGULATOR_UNUSED, REGULATOR_UNUSED
    },
    
    {   /* Switchmode regulator 2. */
        REGULATOR_SW2_ONOFF_POS, REGULATOR_SW2_ONOFF_MASK, 
        REGULATOR_SW2_LEVEL_POS, REGULATOR_SW2_LEVEL_MASK,
        REGULATOR_UNUSED, REGULATOR_UNUSED, 
        REGULATOR_UNUSED, REGULATOR_UNUSED
    },
    
    {   /* Switchmode regulator 3. */
        REGULATOR_SW3_ONOFF_POS, REGULATOR_SW3_ONOFF_MASK, 
        REGULATOR_SW3_LEVEL_POS, REGULATOR_SW3_LEVEL_MASK,
        REGULATOR_UNUSED, REGULATOR_UNUSED, 
        REGULATOR_UNUSED, REGULATOR_UNUSED
    },  
};

/* Nominal voltages for each switchmode regulator setting. */
static const char * str_tbl_sw_regs[REGULATOR_NUM_SW_REGS][16] = 
{
    /* SW1 */
    {
        "0.90V", "0.95V", "1.00V", "1.10V", "1.15V", "1.20V", "1.25V", "1.30V",
        "1.35V", "1.40V", "1.45V", "1.50V", "1.60V", "1.875V","2.25V", "Vin"
    },
    
    /* SW2 */
    {
        "0.90V", "0.95V", "1.00V", "1.10V", "1.15V", "1.20V", "1.25V", "1.30V",
        "1.35V", "1.40V", "1.45V", "1.50V", "1.60V", "1.875V","2.25V", "Vin"
    },
    
    /* SW3 */
    {
        "4.0V", "4.5V", "5.0V", "5.5V"
    }
};

static const REGULATOR_BITS_T regulator_vreg_bits[REGULATOR_NUM_V_REGS] = 
{
    {   /* V1. */
        REGULATOR_V1_ONOFF_POS, REGULATOR_V1_ONOFF_MASK, 
        REGULATOR_V1_LEVEL_POS, REGULATOR_V1_LEVEL_MASK,
        REGULATOR_V1_LOWPWR_POS, REGULATOR_V1_LOWPWR_MASK, 
        REGULATOR_V1_STBY_CTRL_POS, REGULATOR_V1_STBY_CTRL_MASK
    },
    
    {   /* V2. */
        REGULATOR_V2_ONOFF_POS, REGULATOR_V2_ONOFF_MASK, 
        REGULATOR_V2_LEVEL_POS, REGULATOR_V2_LEVEL_MASK,
        REGULATOR_V2_LOWPWR_POS, REGULATOR_V2_LOWPWR_MASK, 
        REGULATOR_V2_STBY_CTRL_POS, REGULATOR_V2_STBY_CTRL_MASK
    },
    
    {   /* V3. */
        REGULATOR_V3_ONOFF_POS, REGULATOR_V3_ONOFF_MASK, 
        REGULATOR_V3_LEVEL_POS, REGULATOR_V3_LEVEL_MASK,
        REGULATOR_V3_LOWPWR_POS, REGULATOR_V3_LOWPWR_MASK, 
        REGULATOR_V3_STBY_CTRL_POS, REGULATOR_V3_STBY_CTRL_MASK
    },
    
    {   /* V4. */
        REGULATOR_V4_ONOFF_POS, REGULATOR_V4_ONOFF_MASK, 
        REGULATOR_V4_LEVEL_POS, REGULATOR_V4_LEVEL_MASK,
        REGULATOR_V4_LOWPWR_POS, REGULATOR_V4_LOWPWR_MASK, 
        REGULATOR_V4_STBY_CTRL_POS, REGULATOR_V4_STBY_CTRL_MASK
    },
    
    {   /* V5. */
        REGULATOR_V5_ONOFF_POS, REGULATOR_V5_ONOFF_MASK, 
        REGULATOR_V5_LEVEL_POS, REGULATOR_V5_LEVEL_MASK,
        REGULATOR_V5_LOWPWR_POS, REGULATOR_V5_LOWPWR_MASK, 
        REGULATOR_V5_STBY_CTRL_POS, REGULATOR_V5_STBY_CTRL_MASK
    },
    
    {   /* V6. */
        REGULATOR_V6_ONOFF_POS, REGULATOR_V6_ONOFF_MASK, 
        REGULATOR_V6_LEVEL_POS, REGULATOR_V6_LEVEL_MASK,
        REGULATOR_V6_LOWPWR_POS, REGULATOR_V6_LOWPWR_MASK, 
        REGULATOR_V6_STBY_CTRL_POS, REGULATOR_V6_STBY_CTRL_MASK
    },
    
    {   /* V7. */
        REGULATOR_V7_ONOFF_POS, REGULATOR_V7_ONOFF_MASK, 
        REGULATOR_V7_LEVEL_POS, REGULATOR_V7_LEVEL_MASK,
        REGULATOR_V7_LOWPWR_POS, REGULATOR_V7_LOWPWR_MASK, 
        REGULATOR_V7_STBY_CTRL_POS, REGULATOR_V7_STBY_CTRL_MASK
    },
    
    {   /* V8. */
        REGULATOR_V8_ONOFF_POS, REGULATOR_V8_ONOFF_MASK, 
        REGULATOR_V8_LEVEL_POS, REGULATOR_V8_LEVEL_MASK,
        REGULATOR_V8_LOWPWR_POS, REGULATOR_V8_LOWPWR_MASK, 
        REGULATOR_V8_STBY_CTRL_POS, REGULATOR_V8_STBY_CTRL_MASK
    },
    
    {   /* V9. */
        REGULATOR_V9_ONOFF_POS, REGULATOR_V9_ONOFF_MASK, 
        REGULATOR_V9_LEVEL_POS, REGULATOR_V9_LEVEL_MASK,
        REGULATOR_V9_LOWPWR_POS, REGULATOR_V9_LOWPWR_MASK, 
        REGULATOR_V9_STBY_CTRL_POS, REGULATOR_V9_STBY_CTRL_MASK
    },
    
    {   /* V10. */
        REGULATOR_V10_ONOFF_POS, REGULATOR_V10_ONOFF_MASK, 
        REGULATOR_UNUSED, REGULATOR_UNUSED,
        REGULATOR_V10_LOWPWR_POS, REGULATOR_V10_LOWPWR_MASK, 
        REGULATOR_V10_STBY_CTRL_POS, REGULATOR_V10_STBY_CTRL_MASK
    },        
        
};

/* Voltages for all outputs of V1 - V9 (there's no setting available for V10). */
static const char * str_tbl_vregs[REGULATOR_NUM_V_REGS - 1][8] = 
{
    /* V1 */
    {
        "2.775V", "1.275V", "1.60V", "1.725V", "1.825V", "1.925V", "2.075V", "2.275V"
    },

    /* V2 */
    {
        "2.5V", "2.775V"
    },
    
     /* V3 */
    {
        "1.075V", "1.275V", "1.550V", "1.725V", "1.876V", "1.950V", "2.075V", "2.275V"
    },
    
     /* V4 */
    {
        "1.275V", "1.550V", "1.725V", "1.875V", "1.950V", "2.075V", "2.275V", "2.775V"
    },
    
     /* V5 */
    {
        "1.875V", "2.275V", "2.475V", "2.775V"
    },
    
     /* V6 */
    {
        "2.475V", "2.775V"
    },
    
     /* V7 */
    {
        "1.875V", "2.775V"
    },
    
     /* V8 */
    {
        "1.275V", "1.550V", "1.725V", "1.875V", "1.950V", "2.075V", "2.275V", "2.775V"
    },
    
     /* V9 */
    {
        "1.575V", "1.875V", "2.475V", "2.775V"
    },
};

static const REGULATOR_BITS_T regulator_aux_reg_bits[REGULATOR_NUM_AUX_REGS] = 
{
    {   /* AUX1. */
        REGULATOR_AUX1_ONOFF_POS, REGULATOR_AUX1_ONOFF_MASK, 
        REGULATOR_AUX1_LEVEL_POS, REGULATOR_AUX1_LEVEL_MASK,
        REGULATOR_AUX1_LOWPWR_POS, REGULATOR_AUX1_LOWPWR_MASK, 
        REGULATOR_AUX1_STBY_CTRL_POS, REGULATOR_AUX1_STBY_CTRL_MASK
    },
    
    {   /* AUX2. */
        REGULATOR_AUX2_ONOFF_POS, REGULATOR_AUX2_ONOFF_MASK, 
        REGULATOR_AUX2_LEVEL_POS, REGULATOR_AUX2_LEVEL_MASK,
        REGULATOR_UNUSED, REGULATOR_UNUSED,
        REGULATOR_UNUSED, REGULATOR_UNUSED,
    },
    
    {   /* AUX3. */
        REGULATOR_AUX3_ONOFF_POS, REGULATOR_AUX3_ONOFF_MASK, 
        REGULATOR_AUX3_LEVEL_POS, REGULATOR_AUX3_LEVEL_MASK,
        REGULATOR_UNUSED, REGULATOR_UNUSED,
        REGULATOR_UNUSED, REGULATOR_UNUSED,
    },
    
    {   /* AUX4. */
        REGULATOR_AUX4_ONOFF_POS, REGULATOR_AUX4_ONOFF_MASK, 
        REGULATOR_AUX4_LEVEL_POS, REGULATOR_AUX4_LEVEL_MASK,
        REGULATOR_UNUSED, REGULATOR_UNUSED,
        REGULATOR_UNUSED, REGULATOR_UNUSED,
    }
};

/* Nominal voltages for each switchmode regulator setting. */
static const char * str_tbl_aux_regs[REGULATOR_NUM_AUX_REGS][16] = 
{
    /* VAUX 1 */
    {
        "1.875V", "2.475V", "2.775V", "3.0V"
    },
    
    /* VAUX 2 */
    {
        "1.875V", "2.475V", "2.775V", "3.0V"
    },    
    
    /* VAUX 3 */
    {
        "1.20V", "1.20V", "1.20V", "1.20V", "1.40V", "1.60V", "1.80V", "2.00V",
        "2.20V", "2.40V", "2.60V", "2.80V", "3.00V", "3.20V", "3.40V", "3.60V"
    },
    
    /* VAUX 4*/
    {
        "1.8V", "1.8V", "3.0V", "5.0V"
    }
};


static const char * accy_lookup_table[MOTO_ACCY_TYPE__NUM_DEVICES] = 
{
    "MOTO_ACCY_TYPE_NONE",                /*0*/

    /*! Indicates that an invalid or broken accessory is connected */
    "MOTO_ACCY_TYPE_INVALID",             /*1*/

    /*! Indicates that a unsupported (ie. OTG, Smart) accessory is connected */
    "MOTO_ACCY_TYPE_NOT_SUPPORTED",       /*2*/

    /*! Charger-type accessories */
    "MOTO_ACCY_TYPE_CHARGER_MID",         /*3*/
    "MOTO_ACCY_TYPE_CHARGER_MID_MPX",     /*4*/
    "MOTO_ACCY_TYPE_CHARGER_FAST",        /*5*/
    "MOTO_ACCY_TYPE_CHARGER_FAST_MPX",    /*6*/
    "MOTO_ACCY_TYPE_CHARGER_FAST_3G",     /*7*/

    /*! Carkit-type accessories */
    "MOTO_ACCY_TYPE_CARKIT_MID",          /*8*/
    "MOTO_ACCY_TYPE_CARKIT_FAST",         /*9*/
    "MOTO_ACCY_TYPE_CARKIT_SMART",        /*10*/

    /*! Data cable-type accessories */
    "MOTO_ACCY_TYPE_CABLE_USB",           /*11*/
    "MOTO_ACCY_TYPE_CABLE_REGRESSION",    /*12*/
    "MOTO_ACCY_TYPE_CABLE_FACTORY",       /*13*/

    /*! Headset-type accessories */
    "MOTO_ACCY_TYPE_HEADSET_MONO",        /*14*/
    "MOTO_ACCY_TYPE_HEADSET_STEREO",      /*15*/
    "MOTO_ACCY_TYPE_HEADSET_EMU_MONO",    /*16*/
    "MOTO_ACCY_TYPE_HEADSET_EMU_STEREO",  /*17*/
};

/*********************************************************************************************
 *
 * Menu options.
 *
 */

/* Main Menu. */
enum
{
    MAIN_MENU_OPTION_QUIT = 0,
    MAIN_MENU_OPTION_REG_ACCESS,
    MAIN_MENU_OPTION_PERIPH,
    MAIN_MENU_OPTION_VREG,
    MAIN_MENU_OPTION_RTC,
    MAIN_MENU_OPTION_AUDIO,
    MAIN_MENU_OPTION_ATOD,
    MAIN_MENU_OPTION_CHARGER,
    MAIN_MENU_OPTION_TCMD,
    MAIN_MENU_OPTION_ACCY,
    MAIN_MENU_OPTION_TOUCHSCREEN,
    MAIN_MENU_OPTION_TIMING,
    
    MAIN_MENU_NUM_OPTIONS
};

/* General register access. */
enum
{
    REG_ACCESS_MENU_OPTION_QUIT = 0,
    REG_ACCESS_MENU_OPTION_READ_ALL_REGS,
    REG_ACCESS_MENU_OPTION_READ_SINGLE_REG,
    REG_ACCESS_MENU_OPTION_READ_REG_VALUE,   
    REG_ACCESS_MENU_OPTION_WRITE_REG,
    REG_ACCESS_MENU_OPTION_WRITE_REG_VALUE,
    REG_ACCESS_MENU_OPTION_CHECK_EMU_SENSE_BIT,
    REG_ACCESS_MENU_OPTION_GET_HW,
    
    REG_ACCESS_MENU_NUM_OPTIONS,
};

/* Peripheral options. */
enum
{
    PERIPH_MENU_OPTION_QUIT = 0,
    PERIPH_MENU_OPTION_GET_STATUS,
    PERIPH_MENU_OPTION_VIBRATOR_ON,
    PERIPH_MENU_OPTION_VIBRATOR_OFF,
    PERIPH_MENU_OPTION_VIBRATOR_SET_LEVEL,
    PERIPH_MENU_OPTION_MAIN_BACKLIGHT_ON,
    PERIPH_MENU_OPTION_MAIN_BACKLIGHT_OFF,
    PERIPH_MENU_OPTION_AUX_BACKLIGHT_ON,
    PERIPH_MENU_OPTION_AUX_BACKLIGHT_OFF,
    PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_ON,
    PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_OFF,
    PERIPH_MENU_OPTION_SOL_LED_ON,
    PERIPH_MENU_OPTION_SOL_LED_OFF,
    PERIPH_MENU_OPTION_WIFI_STATUS_LED_ON,
    PERIPH_MENU_OPTION_WIFI_STATUS_LED_OFF,  
    PERIPH_MENU_OPTION_WLAN_ON,
    PERIPH_MENU_OPTION_WLAN_OFF,
    PERIPH_MENU_OPTION_WLAN_LOW_POWER_STATE_ON,
    
    PERIPH_MENU_NUM_OPTIONS
};

/* Voltage regulator access. */

enum
{
    VREG_MENU_OPTION_QUIT = 0,
    VREG_MENU_OPTION_GET_SWITCHMODE_STATUS,
    VREG_MENU_OPTION_GET_VREG1_STATUS,
    VREG_MENU_OPTION_GET_VREG2_STATUS,
    VREG_MENU_OPTION_GET_AUX_STATUS,
    VREG_MENU_OPTION_SET_SWITCHMODE_REG,
    VREG_MENU_OPTION_SET_V_REG,
    VREG_MENU_OPTION_SET_AUX_REG,
    VREG_MENU_OPTION_SET_CORE_VOLTAGE,    
          
    VREG_MENU_NUM_OPTIONS
};

enum
{
    RTC_MENU_OPTION_QUIT = 0,
    RTC_MENU_OPTION_GET_STATUS,
    RTC_MENU_OPTION_SET_TIME,
    RTC_MENU_NUM_OPTIONS
};

enum
{
    AUDIO_MENU_OPTION_QUIT=0,
    AUDIO_MENU_OPTION_SET_OUT_PATH,
    AUDIO_MENU_OPTION_SET_IN_PATH,
    AUDIO_MENU_OPTION_SET_CODEC_ON,
    AUDIO_MENU_OPTION_SET_CODEC_OFF,
    AUDIO_MENU_OPTION_SET_OUT_GAIN,

    AUDIO_MENU_OPTION_NUM_OPTIONS
};
    
enum
{
    ATOD_MENU_OPTION_QUIT = 0,
    ATOD_MENU_OPTION_SINGLE_CHANNEL,
    ATOD_MENU_OPTION_GENERAL,
    ATOD_MENU_OPTION_BATT_AND_CURR_IMMEDIATE,
    ATOD_MENU_OPTION_BATT_AND_CURR_PHASED_IMMEDIATE,
    ATOD_MENU_OPTION_BATT_AND_CURR_DELAYED,
    ATOD_MENU_OPTION_RAW,
    ATOD_MENU_OPTION_NONBLOCK_START,
    ATOD_MENU_OPTION_NONBLOCK_POLL,
    ATOD_MENU_OPTION_NONBLOCK_CANCEL,
    ATOD_MENU_OPTION_PHASING_SET,
    ATOD_MENU_OPTION_PHASING_SET_DEFAULT,
    ATOD_MENU_OPTION_PHASING_SET_INVALID
};

enum
{
    CHARGER_MENU_OPTION_QUIT = 0,
    CHARGER_MENU_OPTION_CHARGE_VOLTAGE,
    CHARGER_MENU_OPTION_CHARGE_CURRENT,
    CHARGER_MENU_OPTION_TRICKLE_CURRENT,
    CHARGER_MENU_OPTION_POWER_PATH,
    CHARGER_MENU_OPTION_GET_OVERVOLTAGE,
    CHARGER_MENU_OPTION_RESET_OVERVOLTAGE,
    CHARGER_MENU_OPTION_GET_SETUP
};

/* TCMD ioctl options. */

enum
{
    TCMD_MENU_OPTION_QUIT = 0,
    TCMD_MENU_OPTION_SET_EMU_TRANS_STATE,
    TCMD_MENU_OPTION_SET_MONO_ADDER_STATE,
    TCMD_MENU_OPTION_GET_IDFLOATS,
    TCMD_MENU_OPTION_GET_IDGNDS,
    TCMD_MENU_OPTION_GET_A1SNS,
    TCMD_MENU_OPTION_GET_MB2SNS,
    TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_ON,
    TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_OFF,
    TCMD_MENU_OPTION_GET_VUSBIN,
    TCMD_MENU_OPTION_SET_VUSBIN_STATE,
    TCMD_MENU_OPTION_GET_CLKSTAT,
    TCMD_MENU_OPTION_SET_COINCHEN_ON,
    TCMD_MENU_OPTION_SET_COINCHEN_OFF,
    TCMD_MENU_OPTION_SET_EMU_CONN_STATE,
    
    TCMD_MENU_NUM_OPTIONS
};




enum
{
    ACCY_MENU_QUIT = 0,
    ACCY_MENU_GET_ALL_DEVICES
};

enum
{
    TOUCHSCREEN_MENU_OPTION_QUIT = 0,
    TOUCHSCREEN_MENU_OPTION_ENABLE,
    TOUCHSCREEN_MENU_OPTION_DISABLE,
    TOUCHSCREEN_MENU_OPTION_SET_POLL_TIME
};

enum
{
    TIMING_MENU_OPTION_QUIT = 0,
    TIMING_MENU_OPTION_SHOW_INFO,
    TIMING_MENU_OPTION_RESET_INFO
};

/********************************************************************************************/
int num_bits_in_mask(int mask);
int write_bits(int power_ic_file, POWER_IC_REG_ACCESS_T * reg_access);
int write_reg(int power_ic_file, int reg, int value);
int read_reg(int power_ic_file, int reg);
int write_reg_value(int power_ic_file, int reg, int data, int index, int width);

void reg_access_read_all_regs(int power_ic_file);
void reg_access_read_single_reg(int power_ic_file);
void reg_access_read_reg_value(int power_ic_file);
void reg_access_write_reg(int power_ic_file);
void reg_access_write_reg_value(int power_ic_file);
void reg_access_check_emu_sense_bit(int power_ic_file);
void reg_access_get_hw(int power_ic_file);

void periph_vibrator_on(int power_ic_file);
void periph_vibrator_off(int power_ic_file);
void periph_vibrator_set_level(int power_ic_file);
int  periph_main_backlight_set_level(int power_ic_file, int level);
void periph_main_backlight_on(int power_ic_file);
void periph_main_backlight_off(int power_ic_file);
int  periph_aux_backlight_set_level(int power_ic_file, int level);
void periph_aux_backlight_on(int power_ic_file);
void periph_aux_backlight_off(int power_ic_file);
void periph_keypad_backlight_on(int power_ic_file);
void periph_keypad_backlight_off(int power_ic_file);
void periph_sol_led_on(int power_ic_file);
void periph_sol_led_off(int power_ic_file);
void periph_wifi_status_led_on(int power_ic_file);
void periph_wifi_status_led_off(int power_ic_file);
void periph_wlan_on(int power_ic_file);
void periph_wlan_off(int power_ic_file);
void periph_wlan_low_power_state_on(int power_ic_file);

void sw_reg_get_status(int power_ic_file);
void vreg1_get_status(int power_ic_file);
void vreg2_get_status(int power_ic_file);
void aux_reg_get_status(int power_ic_file);
void regulator_set_sw_reg(int power_ic_file);
void regulator_set_vreg(int power_ic_file);
void regulator_set_aux_reg(int power_ic_file);
void set_core_voltage(int power_ic_file);

void rtc_get_status(int power_ic_file);
void rtc_set_times(int power_ic_file);

void atod_convert_single_channel(int power_ic_file);
void atod_convert_general_channels(int power_ic_file);
void atod_convert_batt_and_curr(int power_ic_file);
void atod_convert_raw(int power_ic_file);
void atod_convert_nonblock_start(int power_ic_file);
void atod_convert_nonblock_poll(int power_ic_file);
void atod_convert_nonblock_cancel(int power_ic_file);
void atod_phasing_set(int power_ic_file);
void atod_phasing_set_default(int power_ic_file);
void atod_phasing_set_invalid(int power_ic_file);

void charger_set_charge_voltage(int power_ic_file);
void charger_set_charge_current(int power_ic_file);
void charger_set_trickle_current(int power_ic_file);
void charger_set_power_path(int power_ic_file);
void charger_get_overvoltage(int power_ic_file);
void charger_reset_overvoltage(int power_ic_file);
void charger_get_setup(int power_ic_file);

void accy_get_all_devices(int accy_file);

void touchscreen_enable(int power_ic_file);
void touchscreen_disable(int power_ic_file);
void touchscreen_set_poll_time(int power_ic_file);

void tcmd_set_emu_trans_state(int power_ic_file);
void tcmd_set_mono_adder_state(int power_ic_file);
void tcmd_get_idfloats(int power_ic_file);
void tcmd_get_idgnds(int power_ic_file);
void tcmd_get_a1sns(int power_ic_file);
void tcmd_get_mb2sns(int power_ic_file);
void tcmd_set_reverse_mode_state_on(int power_ic_file);
void tcmd_set_reverse_mode_state_off(int power_ic_file);
void tcmd_get_vusbin(int power_ic_file);
void tcmd_set_vusbin_state(int power_ic_file);
void tcmd_get_clkstat(int power_ic_file);
void tcmd_set_coinchen_on(int power_ic_file);
void tcmd_set_coinchen_off(int power_ic_file);
void tcmd_set_emu_conn_state(int power_ic_file);


int get_value_from_user(int * value);

void display_reg_access_menu(void);
void reg_access_menu(int power_ic_file);
void display_periph_menu(void);
void periph_menu(int power_ic_file);
void display_rtc_menu(void);
void rtc_menu(int power_ic_file);
void display_atod_menu(void);
void atod_menu(int power_ic_file);
void display_main_menu(void);
void display_audio_menu(void);
void audio_menu(int power_ic_file);
void display_touchscreen_menu(void);
void touchscreen_menu(int power_ic_file);
void display_tcmd_menu(void);
void tcmd_menu(int power_ic_file);
void display_timing_menu(void);
void timing_menu(int power_ic_file);
/**********************************************************************************************
 *
 * General register access functions.
 *
 *********************************************************************************************/

/* We really need an interface that takes a value and a mask to be applied to a register... */
int num_bits_in_mask(int mask)
{
    int num_bits = 0;
    while(mask)
    {
        num_bits += (mask & 1);
        mask >>= 1;
    }
    
    return num_bits;
}
 
int write_bits(int power_ic_file, POWER_IC_REG_ACCESS_T * reg_access)
{
    int set_value;
    int ioctl_result;

    /* Remember what we set so we can verify it later. */
    set_value = reg_access->value;
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_WRITE_REG_BITS, reg_access);
        
    if(ioctl_result != 0)
    {
        printf("ioctl() failed - returned %7d\n", ioctl_result);
        printf("      (attempted ioctl cmd 0x%X on file %d)\n", POWER_IC_IOCTL_WRITE_REG_BITS, power_ic_file);
        return ioctl_result;
    }
    else
    {
        /* Check that value in bit is set correctly. */
        reg_access->value = 0xFFFFFFFF;
        
        ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, reg_access);

        if(ioctl_result != 0)
        {
            printf("ioctl() failed - returned %7d\n", ioctl_result);
            printf("      (attempted ioctl cmd 0x%X on file %d)\n", POWER_IC_IOCTL_READ_REG_BITS, power_ic_file);
            return ioctl_result;
        }
        else if(reg_access->value != set_value)
        {
            printf("Verify data failed: wrote 0x%7X, got 0x%7X\n", set_value, reg_access->value);
        }
    }
    
    return 0;
}

int write_reg(int power_ic_file, int reg, int value)
{
    int ioctl_result;
    POWER_IC_REG_ACCESS_T reg_access;
    
    /* Set up to read complete register. */
    reg_access.reg = reg;
    reg_access.value = value;

    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_WRITE_REG, &reg_access);
        
    if(ioctl_result != 0)
    {
        printf("ioctl() read register failed - returned %7d\n", ioctl_result);
        printf("      (attempted ioctl cmd 0x%X on file %d)\n", POWER_IC_IOCTL_WRITE_REG, power_ic_file);
        return ioctl_result;
    }

    return reg_access.value;
}


int read_reg(int power_ic_file, int reg)
{
    int ioctl_result;
    POWER_IC_REG_ACCESS_T reg_access;
    
    /* Set up to read complete register. */
    reg_access.reg = reg;

    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &reg_access);
        
    if(ioctl_result != 0)
    {
        printf("ioctl() read register failed - returned %7d\n", ioctl_result);
        printf("      (attempted ioctl cmd 0x%X on file %d)\n", POWER_IC_IOCTL_READ_REG, power_ic_file);
        return ioctl_result;
    }

    return reg_access.value;
}



void reg_access_read_all_regs(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int i;
    int ioctl_result;
    
    /****************************************************************
     * Try to read all registers from power IC.
     ***************************************************************/
    printf("\nTest %d: read all power IC registers\n", REG_ACCESS_MENU_OPTION_READ_ALL_REGS);
    printf("----------------------------------\n");
    
    for(i = 0; i<POWER_IC_REG_NUM_REGS_PCAP; i++)
    {
        printf("    Reading register %2d: ", i);
        reg_access.reg = i;
        
        ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &reg_access);
        
        if(ioctl_result == 0)
        {
            printf("register content: 0x%X\n", reg_access.value);
        }
        else
        {
            printf("ioctl() failed - returned %d\n", ioctl_result);
            printf("      (attempted ioctl cmd 0x%X on file %d)\n", POWER_IC_IOCTL_READ_REG, power_ic_file);
        }
    }
}

void reg_access_read_single_reg(int power_ic_file)
{
    int reg;
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;
    
    printf("\nTest %d: read single power IC register\n", REG_ACCESS_MENU_OPTION_READ_SINGLE_REG);
    printf("--------------------------------------\n");
    
    printf("\nThis will read the contents of a single register. \n\n");
    
    /* Get the register and the value to set it to. */
    printf("Which register do you wish to read from? (0..%d) > ", POWER_IC_REG_NUM_REGS_PCAP - 1);
    
    if( get_value_from_user(&reg) != 0)
    {
        printf("\nRegister not valid.\n\n");
        return;
    }
    
    if((reg < POWER_IC_REG_PCAP_ISR) || (reg >= POWER_IC_REG_NUM_REGS_PCAP))
    {
        printf("\nRegister %d does not exist\n", reg);
        return;
    }
    
    /* Set up structure for read. */
    reg_access.reg = reg;
    
    printf("\nReading register %d.\n", reg_access.reg );
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &reg_access);
        
    if(ioctl_result == 0)
    {
        printf("register content: 0x%X\n", reg_access.value);
    }
    else
    {
        printf("Error %d reading register %d\n", ioctl_result, reg_access.reg);
    }
}

void reg_access_read_reg_value(int power_ic_file)
{
    int reg;
    int index;
    int width;
    int ioctl_result;
    POWER_IC_REG_ACCESS_T reg_access;
    
    printf("\nTest %d: read power IC register value\n", REG_ACCESS_MENU_OPTION_READ_REG_VALUE);
    printf("-------------------------------------\n");
    
    printf("\nThis will read any contiguous set of bits of data\n");
    printf("from a power IC register.\n\n");
    
    /* Get the register and the value to set it to. */
    printf("Which register do you wish to read from? (0..%d) > ", POWER_IC_REG_NUM_REGS_PCAP - 1);
    
    if( get_value_from_user(&reg) != 0)
    {
        printf("\nRegister not valid.\n\n");
        return;
    }
    
    if((reg < POWER_IC_REG_PCAP_ISR) || (reg >= POWER_IC_REG_NUM_REGS_PCAP))
    {
        printf("\nRegister %d does not exist\n", reg);
        return;
    }
    
    printf("What is the index of the first bit you wish to read in register %2d? > ", reg);
    
    if(get_value_from_user(&index) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    printf("How many bits (including bit %d) do you wish to read from register %2d? > ", index, reg);
    
    if(get_value_from_user(&width) != 0 )
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    reg_access.reg = reg;
    reg_access.num_bits = width;
    reg_access.index = index;
        
    printf("\nReading %d bits from (and including) bit % in register %d.\n", width, index, reg);
    
    
    if((ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access)) != 0)
    {
        printf("\n Error %d reading from register %d\n", ioctl_result, reg);
        return;
    }
    else
    {
        printf("\n Read 0x%X from register %d.\n", reg_access.value, reg_access.reg);
    }
}


void reg_access_write_reg(int power_ic_file)
{
    int reg;
    int data;
    int verify_data;
    
    printf("\nTest %d: write single power IC register\n", REG_ACCESS_MENU_OPTION_READ_ALL_REGS);
    printf("---------------------------------------\n");
    
    printf("\nThis will write any data (hex, binary or decimal) to any \n");
    printf("register without first checking whether the data is valid.\n\n");
    
    /* Get the register and the value to set it to. */
    printf("Which register do you wish to write to? (0..%d) > ", POWER_IC_REG_NUM_REGS_PCAP - 1);
    
    if( get_value_from_user(&reg) != 0)
    {
        printf("\nRegister not valid.\n\n");
        return;
    }
    
    if((reg < POWER_IC_REG_PCAP_ISR) || (reg >= POWER_IC_REG_NUM_REGS_PCAP))
    {
        printf("\nRegister %d does not exist\n", reg);
        return;
    }
    
    printf("What value do you want to write to register %2d? > ", reg);
    
    if( get_value_from_user(&data) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    printf("\nWriting 0x%X to register %d.\n", data, reg);
    if(write_reg(power_ic_file, reg, data) != 0)
    {
        printf("\n Error writing to register %d\n", reg);
        return;
    }
    
    printf("\nVerifying data...");
    verify_data = read_reg(power_ic_file, reg);
    
    if(verify_data == data)
    {
        printf("0x%X successfully written.\n", data);
    }
    else
    {
        printf(" Error: read back data 0x%X.\n", verify_data);
    }
    
}

void reg_access_write_reg_value(int power_ic_file)
{
    int reg;
    int index;
    int width;
    int data;
    POWER_IC_REG_ACCESS_T reg_access;
    
    printf("\nTest %d: write single power IC register values\n", REG_ACCESS_MENU_OPTION_READ_ALL_REGS);
    printf("---------------------------------------\n");
    
    printf("\nThis will write any bit of data (hex, binary or decimal) to any \n");
    printf("register without changing any other bit of information.\n\n");
    
    /* Get the register and the value to set it to. */
    printf("Which register do you wish to write a mask to? (0..%d) > ", POWER_IC_REG_NUM_REGS_PCAP - 1);
    
    if( get_value_from_user(&reg) != 0)
    {
        printf("\nRegister not valid.\n\n");
        return;
    }
    
    if((reg < POWER_IC_REG_PCAP_ISR) || (reg >= POWER_IC_REG_NUM_REGS_PCAP))
    {
        printf("\nRegister %d does not exist\n", reg);
        return;
    }
    
    printf("What is the index of the bit you wish to alter for register %2d? > ", reg);
    
    if(get_value_from_user(&index) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    printf("How many bits do you wish to alter for register %2d? > ", reg);
    
    if(get_value_from_user(&width) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    printf("What value do you want to write to those bits for register %2d? > ", reg);
    
    if( get_value_from_user(&data) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    reg_access.reg = reg;
    reg_access.value = data;
    reg_access.num_bits = width;
    reg_access.index = index;
        
    printf("\nWriting 0x%X with an index of %d and a width of %d to register %d.\n", data, index, width, reg);
    if(write_bits(power_ic_file, &reg_access) != 0)
    {
        printf("\n Error writing to register %d\n", reg);
        return;
    }
}

void reg_access_check_emu_sense_bit(int power_ic_file)
{
    int bit, loops;
    int i;
    POWER_IC_REG_ACCESS_T access;

    printf("\nCheck EMU sense bit\n");
    printf("-------------------\n");

    printf("This will read a sense bits from EMU.\n");
    printf("Which sense bit do you wish to be read? > ");
    if(get_value_from_user(&bit) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }    
    
    printf("How many times do you want the sense bit register to be read? > ");
    
    if(get_value_from_user(&loops) != 0)
    {
        printf("Input not valid.\n\n");
        return;
    }
    
    printf("\nReading EMU sense bit %d %d times.\n", bit, loops);
    
    access.reg = POWER_IC_REG_EOC_INT_SENSE;
    
    for(i = 1; i <= loops; i++)
    {
        if(ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &access))
        {
            printf("   Error reading EMU sense register. Exiting.\n");
            break;
        }
        
        printf("%6d: register 0x%0X, sense bit %d = %d\n", i, access.value, bit,
         (access.value & ((1<<(bit+1)) - 1)) >> bit );
    }
}

void reg_access_get_hw(int power_ic_file)
{
    POWER_IC_HARDWARE_T info;

    printf("\nGet hardware info\n");
    printf("-----------------\n");

    printf("Getting type/revision information from hardware.\n");
    
    if(ioctl(power_ic_file, POWER_IC_IOCTL_GET_HARDWARE_INFO, &info))
    {
        printf("   Error retrieving hardware info. Exiting.\n");
        return;
    }
    
    switch(info.chipset)
    {
        case POWER_IC_CHIPSET_PCAP2_EOC:
            printf("   Chipset reported as PCAP/EMU one-chip.\n\n");
            printf("         PCAP revision: 0x%X\n", info.revision1);
            printf("          EMU revision: 0x%X\n", info.revision2);
            break;
    
        case POWER_IC_CHIPSET_ATLAS:
            printf("   Chipset reported as Atlas.\n\n");
            printf("              Revision: 0x%X\n", info.revision1);
            break;
        
        default:
            printf("   Unknwon chipset reported.\n");
            break;
    }
}

/**********************************************************************************************
 *
 * Peripheral control functions.
 *
 *********************************************************************************************/
void periph_get_status(int power_ic_file)
{
    int periph_reg_value = 0;
    int aux_vreg_value = 0;
    int vreg2_value = 0;
    int status;
    
    printf("\nPeripheral status\n");
    printf("-----------------\n\n");
    
    /* Backlights are in register 21. */
    periph_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_PERIPH);

    /* Vibrator controlled in register 7. */
    aux_vreg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_AUX_VREG);
    
    /* WLAN is controlled by register 6 and register 7. */ 
    vreg2_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_VREG2);
         
    /* Register contents. */
    printf("     VREG2 register contents: 0x%7X\n", vreg2_value);
    printf("  AUX_VREG register contents: 0x%7X\n", aux_vreg_value);
    printf("    PERIPH register contents: 0x%7X\n\n", periph_reg_value);   
    
    /* Main backlight status */
    status = periph_reg_value & 0x1F;
    printf("      Main backlight setting: 0x%X\n", status);
    
    /* Aux backlight setting. */
    status = (periph_reg_value >> 20) & 0x1F;
    printf("       Aux backlight setting: 0x%X\n", status);
    
    /* Vibrator status. */
    status = (aux_vreg_value >> 19) & 0x1;
    printf("            Vibrator setting: %d\n", status);
    
    /* Might as well report the vibrator regulator level while we're here... */
    status = (aux_vreg_value >> 20) & 0x3;
    printf("  Vibrator reg level setting: 0x%X\n", status);
    
    /* WLAN status. */
    status = (((vreg2_value >> 1) & 0x1) & ((aux_vreg_value >> 4) & 0x1));
    printf("            WLAN setting: %d\n", status);
    
    /* Might as well report the WLAN regulator levels while we're here... */
    status = (vreg2_value >> 2) & 0x1;
    printf("  WLAN reg V6 level setting: 0x%X\n", status);
    status = (aux_vreg_value >> 5) & 0x3;
    printf("  WLAN reg VAUX2 level setting: 0x%X\n", status);
    
    
}


void periph_vibrator_on(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;

    printf("\nPeripheral test %d: Turn vibrator on\n", PERIPH_MENU_OPTION_VIBRATOR_ON);
    printf("------------------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PERIPH_SET_VIBRATOR_ON, POWER_IC_PERIPH_ON);
    
    if(ioctl_result == 0)
    {
    
        /* Vibrator is controlled by bit 19 in register 7. Read this to verify that the
         * correct bit was set.*/
        reg_access.reg = POWER_IC_REG_PCAP_AUX_VREG;
        reg_access.num_bits = 1;
        reg_access.index = 19;
        reg_access.value = 0xFFFFFFFF;
        
        ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
        
        if(ioctl_result == 0)
        {
            if(reg_access.value == POWER_IC_PERIPH_ON)
            {
                printf("    Vibrator on.\n");
            }
            else
            {
                printf("    Error verifying vibrator setting: set %d, read %d.\n", 
                        POWER_IC_PERIPH_ON, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying vibrator setting - ioctl() returned %d.\n", ioctl_result);
        }
    
    }
    else
    {
        printf("   Error turning vibrator on - ioctl() returned %d.\n", ioctl_result);
    }
}

void periph_vibrator_off(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;

    printf("\nPeripheral test %d: Turn vibrator off\n", PERIPH_MENU_OPTION_VIBRATOR_OFF);
    printf("-------------------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PERIPH_SET_VIBRATOR_ON, POWER_IC_PERIPH_OFF);
    
    if(ioctl_result == 0)
    {
    
        /* Vibrator is controlled by bit 19 in register 7. Read this to verify that the
         * correct bit was set.*/
        reg_access.reg = POWER_IC_REG_PCAP_AUX_VREG;
        reg_access.num_bits = 1;
        reg_access.index = 19;
        reg_access.value = 0xFFFFFFFF;
        
        ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
        
        if(ioctl_result == 0)
        {
            if(reg_access.value == POWER_IC_PERIPH_OFF)
            {
                printf("    Vibrator off.\n");
            }
            else
            {
                printf("    Error verifying vibrator setting: set %d, read %d.\n", 
                        POWER_IC_PERIPH_ON, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying vibrator setting - ioctl() returned %d.\n", ioctl_result);
        }
    
    }
    else
    {
        printf("   Error turning vibrator off - ioctl() returned %d.\n", ioctl_result);
    }
}

void periph_vibrator_set_level(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;
    int new_level;

    printf("\nPeripheral test %d: set vibrator level\n", PERIPH_MENU_OPTION_VIBRATOR_OFF);
    printf("--------------------------------------\n");
    
    /* Get a new level for the vibrator. */
    printf("Set vibrator level? (0..%d) > ", 3);
    if(get_value_from_user(&new_level) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    else if((new_level < 0) || (new_level > 3))
    {
        printf("    Error: %d is not a valid level for the vibrator.\n", new_level);
    }
    
    /* Write new level to power IC. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PERIPH_SET_VIBRATOR_LEVEL, new_level);
    
    if(ioctl_result == 0)
    {
    
        /* Vibrator level is controlled by bit 20..21 in register 7. Read this to verify that the
         * correct bit was set.*/
        reg_access.reg = POWER_IC_REG_PCAP_AUX_VREG;
        reg_access.num_bits = 2;
        reg_access.index = 20;
        reg_access.value = 0xFFFFFFFF;
        
        ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
        
        if(ioctl_result == 0)
        {
            if(reg_access.value == new_level)
            {
                printf("    Vibrator level set.\n");
            }
            else
            {
                printf("    Error verifying vibrator setting: set %d, read %d.\n", 
                        new_level, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying vibrator setting - ioctl() returned %d.\n", ioctl_result);
        }
    
    }
    else
    {
        printf("   Error setting vibrator level - ioctl() returned %d.\n", ioctl_result);
    }
}

int periph_funlight_set_level(int power_ic_file, int region , int level)
{
    int ioctl_result;
    LIGHTS_FL_UPDATE_T funlight_update; 

    funlight_update.nApp = LIGHTS_FL_APP_CTL_DEFAULT;
    funlight_update.nRegion = region;
    funlight_update.nColor = level;
    
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_LIGHTS_FL_UPDATE, &funlight_update);
    if(ioctl_result != 0)
    {
        printf("ioctl() failed - returned %7d\n", ioctl_result);
        printf("      (attempted ioctl cmd 0x%X on file %d)\n", POWER_IC_IOCTL_WRITE_REG_BITS, power_ic_file);
        return ioctl_result;
    }
    return 0;
}


void periph_main_backlight_on(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn main backlight on\n", PERIPH_MENU_OPTION_MAIN_BACKLIGHT_ON);
    printf("------------------------------------------\n");
    
    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_DISPLAY_BL, 0x1F) == 0)
    {
        printf("   Main backlight on.\n");       
    }
    else
    {
        printf("   Error turning main backlight on.\n");
    }
}

void periph_main_backlight_off(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn main backlight off\n", PERIPH_MENU_OPTION_MAIN_BACKLIGHT_OFF);
    printf("-------------------------------------------\n");
    
    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_DISPLAY_BL, 0x00) == 0)
    {
        printf("   Main backlight off.\n");       
    }
    else
    {
        printf("   Error turning main backlight off.\n");
    }
}


void periph_aux_backlight_on(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn aux backlight on\n", PERIPH_MENU_OPTION_AUX_BACKLIGHT_ON);
    printf("------------------------------------------\n");
    
    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_CLI_DISPLAY_BL, 0x1F) == 0)
    {
        printf("   Main backlight on.\n");       
    }
    else
    {
        printf("   Error turning main backlight on.\n");
    }
}

void periph_aux_backlight_off(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn aux backlight off\n", PERIPH_MENU_OPTION_AUX_BACKLIGHT_OFF);
    printf("-------------------------------------------\n");
   
    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_CLI_DISPLAY_BL, 0x00) == 0)
    {
        printf("   Main backlight off.\n");       
    }
    else
    {
        printf("   Error turning main backlight off.\n");
    }
}

void periph_keypad_backlight_on(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn keypad backlight on\n", PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_ON);
    printf("-------------------------------------------\n");

    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_KEYPAD_NAV_BL, 0x1F) == 0)
    {
        printf("   Keypad backlight on.\n");       
    }
    else
    {
        printf("   Error turning keypad backlight on.\n");
    }
}

void periph_keypad_backlight_off(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn keypad backlight off\n",PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_OFF);
    printf("-------------------------------------------\n");

    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_KEYPAD_NAV_BL, 0x00) == 0)
    {
        printf("   Keypad backlight off.\n");       
    }
    else
    {
        printf("   Error turning keypad backlight off.\n");
    }
}

void periph_sol_led_on(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn Sign Of Life LED on\n",PERIPH_MENU_OPTION_SOL_LED_ON);
    printf("-------------------------------------------\n");

    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_SOL_LED, 0x1F) == 0)
    {
        printf("   Sign Of Life LED on.\n");       
    }
    else
    {
        printf("   Error turning Sign Of Life LED on.\n");
    }
}

void periph_sol_led_off(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn Sign Of Life LED off\n",PERIPH_MENU_OPTION_SOL_LED_OFF);
    printf("-------------------------------------------\n");

    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_SOL_LED, 0x00) == 0)
    {
        printf("   Sign Of Life LED off.\n");       
    }
    else
    {
        printf("   Error turning Sign Of Life LED off.\n");
    }
}

void periph_wifi_status_led_on(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn Wifi Status LED on\n",PERIPH_MENU_OPTION_SOL_LED_ON);
    printf("-------------------------------------------\n");

    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_WIFI_STATUS, 0x1F) == 0)
    {
        printf("   Wifi Status LED on.\n");       
    }
    else
    {
        printf("   Error turning Wifi Status LED on.\n");
    }
}

void periph_wifi_status_led_off(int power_ic_file)
{
    printf("\nPeripheral test %d: Turn Wifi Status LED off\n",PERIPH_MENU_OPTION_SOL_LED_OFF);
    printf("-------------------------------------------\n");

    if(periph_funlight_set_level(power_ic_file, LIGHTS_FL_REGION_WIFI_STATUS, 0x00) == 0)
    {
        printf("   Wifi Status LED off.\n");       
    }
    else
    {
        printf("   Error turning Wifi Status LED off.\n");
    }
}

void periph_wlan_on(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;
    int ioctl_result1;
    int ioctl_result2;

    printf("\nWLAN test %d: Turn WLAN on\n", PERIPH_MENU_OPTION_WLAN_ON);
    printf("---------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PERIPH_SET_WLAN_ON, POWER_IC_PERIPH_ON);
    
    if(ioctl_result == 0)
    {
    
        /* The WLAN is powered by two supplies V6 which is controlled by bit 1 on register 6 and VAUX2 
	 * which is controlled by bit 19 in register 7. Read this to verify that the correct bits were set.*/
	
	reg_access.reg = POWER_IC_REG_PCAP_VREG2;
        reg_access.num_bits = 1;
        reg_access.index = 1;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
	reg_access.reg = POWER_IC_REG_PCAP_AUX_VREG;
        reg_access.num_bits = 1;
        reg_access.index = 4;
        reg_access.value = 0xFFFFFFFF;
        		
        ioctl_result2 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
        	
	
        if(ioctl_result1 == 0 && ioctl_result2 == 0)
        {
            if(reg_access.value == POWER_IC_PERIPH_ON)
            {
                printf("    WLAN on.\n");
            }
            else
            {
                printf("    Error verifying WLAN setting: set %d, read %d.\n", 
                        POWER_IC_PERIPH_ON, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying WLAN setting - ioctl() returned %d and %d. \n", ioctl_result1, ioctl_result2);
        }
    
    }
    else
    {
        printf("   Error turning WLAN on - ioctl() returned %d. \n", ioctl_result);
    }
}

void periph_wlan_off(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;
    int ioctl_result1;
    int ioctl_result2;

    printf("\nPeripheral test %d: Turn WLAN off\n", PERIPH_MENU_OPTION_WLAN_OFF);
    printf("----------------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PERIPH_SET_WLAN_ON, POWER_IC_PERIPH_OFF);
    
    if(ioctl_result == 0)
    {
    
        /* WLAN is powered by two supplies V6 which is controlled by bit 1 on register 6 and VAUX2 
	 * which is controlled by bit 19 in register 7. Read this to verify that the correct bits were set.*/
	 
	reg_access.reg = POWER_IC_REG_PCAP_VREG2;
        reg_access.num_bits = 1;
        reg_access.index = 1;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
	reg_access.reg = POWER_IC_REG_PCAP_AUX_VREG;
        reg_access.num_bits = 1;
        reg_access.index = 4;
        reg_access.value = 0xFFFFFFFF;
        		
        ioctl_result2 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
        	
	
        if(ioctl_result1 == 0 && ioctl_result2 == 0)
        {
            if(reg_access.value == POWER_IC_PERIPH_OFF)
            {
                printf("    WLAN off.\n");
            }
            else
            {
                printf("    Error verifying wlan setting: set %d, read %d.\n", 
                        POWER_IC_PERIPH_OFF, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying WLAN setting - ioctl() returned %d and %d.\n", ioctl_result1, ioctl_result2);
        }
    
    }
    else
    {
        printf("   Error turning WLAN off - ioctl() returned %d.\n", ioctl_result);
    }
}

void periph_wlan_low_power_state_on(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    POWER_IC_REG_ACCESS_T reg_access1;
    int ioctl_result;
    int ioctl_result1;
    int ioctl_result2;

    printf("\nWLAN test %d: Turn WLAN low power state on\n", PERIPH_MENU_OPTION_WLAN_LOW_POWER_STATE_ON);
    printf("------------------------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PERIPH_SET_WLAN_LOW_POWER_STATE_ON);
    
    if(ioctl_result == 0)
    {
    
        /* The WLAN is powered by two supplies V6 which is controlled by bit 1 on register 6 and VAUX2 
	 * which is controlled by bit 19 in register 7. Read this to verify that the correct bits were set.*/
	
	reg_access.reg = POWER_IC_REG_PCAP_VREG2;
        reg_access.num_bits = 1;
        reg_access.index = 1;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
	reg_access1.reg = POWER_IC_REG_PCAP_AUX_VREG;
        reg_access1.num_bits = 1;
        reg_access1.index = 4;
        reg_access1.value = 0xFFFFFFFF;
        		
        ioctl_result2 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access1);
        	
	
        if(ioctl_result1 == 0 && ioctl_result2 == 0)
        {
            if(reg_access.value == POWER_IC_PERIPH_ON && reg_access1.value == POWER_IC_PERIPH_OFF)            {
                printf("    WLAN low power mode is on.\n");
            }
            else
            {
                printf("    Error verifying WLAN setting: set %d, V6 %d VAUX2 %d.\n", 
                        POWER_IC_PERIPH_ON, reg_access.value, reg_access1.value );
            }
        }
        else
        {
            printf("   Error verifying WLAN setting for low power mode - ioctl() returned %d and %d. \n", ioctl_result1, ioctl_result2);
        }
    
    }
    else
    {
        printf("   Error turning WLAN low poer state on - ioctl() returned %d. \n", ioctl_result);
    }
}






/**********************************************************************************************
 *
 * Voltage regulator functions.
 *
 *********************************************************************************************/
void sw_reg_get_status(int power_ic_file)
{
    int swctrl_reg_value = 0;
    int status;
    int i;
    
    printf("\nSwitchmode regulator status\n");
    printf("---------------------------\n\n");
    
    /* Get the current register values. */
    swctrl_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_SWCTRL);
    
    /* Register contents. */
    printf("    SWCTRL register contents: 0x%7X\n", swctrl_reg_value);
    
    printf("\n\n");
    
    for (i = 0; i < REGULATOR_NUM_SW_REGS; i++)
    {
        
        printf("              SW%d: ", i+1);
        status = (swctrl_reg_value >> regulator_sw_bits[i].onoff_pos) 
                  & regulator_sw_bits[i].onoff_mask;
        if(status)
        {
            printf("On\n");
        }
        else
        {
            printf("Off\n");
        }  
        status = (swctrl_reg_value >> regulator_sw_bits[i].level_pos) 
                                    & regulator_sw_bits[i].level_mask;
        printf("            level: 0x%X (%s)\n", status, str_tbl_sw_regs[i][status]);
        
        /* Only display low power setting if control is available. */
        if(regulator_sw_bits[i].lowpwr_pos != REGULATOR_UNUSED)
        {
            status = (swctrl_reg_value >> regulator_sw_bits[i].lowpwr_pos) 
                       & regulator_sw_bits[i].lowpwr_mask;
            printf("        low power: %d\n", status);            
        }
        
        /* Only display low power setting if control is available. */
        if(regulator_sw_bits[i].stby_ctrl_pos != REGULATOR_UNUSED)
        {
            status = (swctrl_reg_value >> regulator_vreg_bits[i].stby_ctrl_pos) 
                       & regulator_vreg_bits[i].stby_ctrl_mask;
            printf("  standby control: %d\n", status);            
        }
        
        printf("\n");
    }
}
 
void vreg1_get_status(int power_ic_file)
{
    int vreg1_reg_value = 0;
    int vreg2_reg_value = 0; /* A fre of the VREG1 control bits are in VREG2. Sneaky... */
    int status;
    
    printf("\nVREG1 status\n");
    printf("------------\n\n");
    
    /* Get the current register values. */
    vreg1_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_VREG1);
    vreg2_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_VREG2);
    
    /* Register contents. */
    printf("     VREG1 register contents: 0x%7X\n", vreg1_reg_value);  
    printf("     VREG2 register contents: 0x%7X\n", vreg2_reg_value);
    
    printf("\n\n");
    /* 
     * Vreg values - VREG1
     ******************************************************************************/
     
    /* V1 */
    printf("               V1: ");
    status = (vreg1_reg_value >> REGULATOR_V1_ONOFF_POS) & REGULATOR_V1_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    } 
    status = (vreg1_reg_value >> REGULATOR_V1_LEVEL_POS) & REGULATOR_V1_LEVEL_MASK;
    printf("       V1 setting: 0x%X (%s)\n", status, str_tbl_vregs[0][status]),
    status = (vreg1_reg_value >> REGULATOR_V1_LOWPWR_POS) & REGULATOR_V1_LOWPWR_MASK;
    printf("  V1 Low Pwr Mode: %d\n", status);
    status = (vreg1_reg_value >> REGULATOR_V1_STBY_CTRL_POS) & REGULATOR_V1_STBY_CTRL_MASK;
    printf("     V1 Stby Ctrl: %d\n\n", status);    
            
    /* V2 */
    printf("               V2: ");
    status = (vreg1_reg_value >> REGULATOR_V2_ONOFF_POS) & REGULATOR_V2_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }   
    status = (vreg1_reg_value >> REGULATOR_V2_LEVEL_POS) & REGULATOR_V2_LEVEL_MASK;
    printf("       V2 setting: 0x%X (%s)\n", status, str_tbl_vregs[1][status]),  
    status = (vreg1_reg_value >> REGULATOR_V2_LOWPWR_POS) & REGULATOR_V2_LOWPWR_MASK;
    printf("       V2 Low Pwr: %d\n", status);
    status = (vreg1_reg_value >> REGULATOR_V2_STBY_CTRL_POS) & REGULATOR_V2_STBY_CTRL_MASK;    
    printf("     V2 Stby Ctrl: %d\n\n", status);

  
    /* V3 */
    printf("               V3: ");
    status = (vreg1_reg_value >> REGULATOR_V3_ONOFF_POS) & REGULATOR_V3_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    } 
    status = (vreg1_reg_value >> REGULATOR_V3_LEVEL_POS) & REGULATOR_V3_LEVEL_MASK;
    printf("       V3 setting: 0x%X (%s)\n", status, str_tbl_vregs[2][status]),  
    status = (vreg1_reg_value >> REGULATOR_V3_LOWPWR_POS) & REGULATOR_V3_LOWPWR_MASK;
    printf("       V3 Low Pwr: %d\n", status);
    status = (vreg1_reg_value >> REGULATOR_V3_STBY_CTRL_POS) & REGULATOR_V3_STBY_CTRL_MASK;    
    printf("     V3 Stby Ctrl: %d\n\n", status);

    /* V4 */
    printf("               V4: ");
    status = (vreg1_reg_value >> REGULATOR_V4_ONOFF_POS) & REGULATOR_V4_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }
    status = (vreg1_reg_value >> REGULATOR_V4_LEVEL_POS) & REGULATOR_V4_LEVEL_MASK;
    printf("       V4 setting: 0x%X (%s)\n", status, str_tbl_vregs[3][status]),  
    status = (vreg1_reg_value >> REGULATOR_V4_LEVEL_POS) & REGULATOR_V4_LEVEL_MASK;
    printf("       V4 Low Pwr: %d\n", status);
    status = (vreg1_reg_value >> REGULATOR_V4_STBY_CTRL_POS) & REGULATOR_V4_STBY_CTRL_MASK;    
    printf("     V4 Stby Ctrl: %d\n\n", status);
    
    /* V5 */
    printf("               V5: ");
    status = (vreg1_reg_value >> REGULATOR_V5_ONOFF_POS) & REGULATOR_V5_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }  
    status = (vreg1_reg_value >> REGULATOR_V5_LEVEL_POS) & REGULATOR_V5_LEVEL_MASK;
    printf("       V5 setting: 0x%X (%s)\n", status, str_tbl_vregs[4][status]),
    
    /* Careful...these two V5 bits are in VREG2. */
    status = (vreg2_reg_value >> REGULATOR_V5_LOWPWR_POS) & REGULATOR_V5_LOWPWR_MASK;
    printf("       V5 Low Pwr: %d\n", status);
    status = (vreg2_reg_value >> REGULATOR_V5_STBY_CTRL_POS) & REGULATOR_V5_STBY_CTRL_MASK;    
    printf("     V5 Stby Ctrl: %d\n\n", status);
}
   
void vreg2_get_status(int power_ic_file)      
{

    int vreg2_reg_value = 0;
    int status;
    
    printf("\nVREG2 regulator status\n");
    printf("----------------------\n\n");
    
    /* Get the current register values. */
    vreg2_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_VREG2);
    
    /* Register contents. */ 
    printf("     VREG2 register contents: 0x%7X\n", vreg2_reg_value);
    
    printf("\n\n");    
    
    /* V6 */
    printf("               V6: ");
    status = (vreg2_reg_value >> 1) & 0x1;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }
    status = (vreg2_reg_value >> REGULATOR_V6_ONOFF_POS) & REGULATOR_V6_ONOFF_MASK;
    printf("       V6 setting: 0x%X (%s)\n", status, str_tbl_vregs[5][status]),  
    status = (vreg2_reg_value >> REGULATOR_V6_LEVEL_POS) & REGULATOR_V6_LEVEL_MASK;
    printf("       V6 Low Pwr: %d\n", status);
    status = (vreg2_reg_value >> REGULATOR_V6_LOWPWR_POS) & REGULATOR_V6_LOWPWR_MASK;    
    printf("     V6 Stby Ctrl: %d\n\n", status);
    
    /* V7 */
    printf("               V7: ");
    status = (vreg2_reg_value >> REGULATOR_V7_ONOFF_POS) & REGULATOR_V7_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    } 
    status = (vreg2_reg_value >> REGULATOR_V7_LEVEL_POS) & REGULATOR_V7_LEVEL_MASK;
    printf("       V7 setting: 0x%X (%s)\n", status, str_tbl_vregs[6][status]),  
    status = (vreg2_reg_value >> REGULATOR_V7_LOWPWR_POS) & REGULATOR_V7_LOWPWR_MASK;
    printf("       V7 Low Pwr: %d\n", status);
    status = (vreg2_reg_value >> REGULATOR_V7_STBY_CTRL_POS) & REGULATOR_V7_STBY_CTRL_MASK;    
    printf("     V7 Stby Ctrl: %d\n\n", status);   
    
    /* V8 */
    printf("               V8: ");
    status = (vreg2_reg_value >> REGULATOR_V8_ONOFF_POS) & REGULATOR_V8_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }  
    status = (vreg2_reg_value >> REGULATOR_V8_LEVEL_POS) & REGULATOR_V8_LEVEL_MASK;
    printf("       V8 setting: 0x%X (%s)\n", status, str_tbl_vregs[7][status]),  
    status = (vreg2_reg_value >> REGULATOR_V8_LOWPWR_POS) & REGULATOR_V8_LOWPWR_MASK;
    printf("       V8 Low Pwr: %d\n", status);
    status = (vreg2_reg_value >> REGULATOR_V8_STBY_CTRL_POS) & REGULATOR_V8_STBY_CTRL_MASK;    
    printf("     V8 Stby Ctrl: %d\n\n", status);    

    /* V9 */
    printf("               V9: ");
    status = (vreg2_reg_value >> REGULATOR_V9_ONOFF_POS) & REGULATOR_V9_ONOFF_MASK;
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }  
    status = (vreg2_reg_value >> REGULATOR_V9_LEVEL_POS) & REGULATOR_V9_LEVEL_MASK;
    printf("       V9 setting: 0x%X (%s)\n", status, str_tbl_vregs[8][status]),  
    status = (vreg2_reg_value >> REGULATOR_V9_LOWPWR_POS) & REGULATOR_V9_LOWPWR_MASK;
    printf("       V9 Low Pwr: %d\n", status);
    status = (vreg2_reg_value >> REGULATOR_V9_STBY_CTRL_POS) & REGULATOR_V9_STBY_CTRL_MASK;    
    printf("     V9 Stby Ctrl: %d\n\n", status); 
    
    /* V10 */
    status = (vreg2_reg_value >> REGULATOR_V10_ONOFF_POS) & REGULATOR_V10_ONOFF_MASK;
    printf("              V10: ");
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }   
    printf("      V10 setting: n/a\n"),  
    status = (vreg2_reg_value >> REGULATOR_V10_LOWPWR_POS) & REGULATOR_V10_LOWPWR_MASK;
    printf("      V10 Low Pwr: %d\n", status);
    status = (vreg2_reg_value >> REGULATOR_V10_STBY_CTRL_POS) & REGULATOR_V10_STBY_CTRL_MASK;    
    printf("    V10 Stby Ctrl: %d\n\n", status);
}   
    

void aux_reg_get_status(int power_ic_file)     
{     
    int aux_vreg_value = 0;
    int aux_vreg_mask_reg_value = 0;
    int status;
    int i;
    
    printf("\nAux regulator status\n");
    printf("--------------------\n\n");
    
    /* Get the current register values. */
    aux_vreg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_AUX_VREG);
    aux_vreg_mask_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_AUX_VREG_MASK);
    
    printf("  AUX_VREG register contents: 0x%7X\n", aux_vreg_value);      
    printf("  AUX_VREG_MASK register contents: 0x%7X\n", aux_vreg_mask_reg_value);
    
    printf("\n\n");
    
    for (i = 0; i < REGULATOR_NUM_AUX_REGS; i++)
    {
        
        printf("            VAUX%d: ", i+1);
        status = (aux_vreg_value >> regulator_aux_reg_bits[i].onoff_pos) 
                  & regulator_aux_reg_bits[i].onoff_mask;
        if(status)
        {
            printf("On\n");
        }
        else
        {
            printf("Off\n");
        }  
        status = (aux_vreg_value >> regulator_aux_reg_bits[i].level_pos) 
                                    & regulator_aux_reg_bits[i].level_mask;
        printf("            level: 0x%X (%s)\n", status, str_tbl_aux_regs[i][status]);
        
        /* Only display low power setting if control is available. */
        if(regulator_aux_reg_bits[i].lowpwr_pos != REGULATOR_UNUSED)
        {
            status = (aux_vreg_value >> regulator_aux_reg_bits[i].lowpwr_pos) 
                       & regulator_aux_reg_bits[i].lowpwr_mask;
            printf("        low power: %d\n", status);            
        }
        
        /* Only display low power setting if control is available. */
        if(regulator_aux_reg_bits[i].stby_ctrl_pos != REGULATOR_UNUSED)
        {
            status = (aux_vreg_value >> regulator_aux_reg_bits[i].stby_ctrl_pos) 
                       & regulator_aux_reg_bits[i].stby_ctrl_mask;
            printf("  standby control: %d\n", status);            
        }
        
        printf("\n");
    }

    
    /* Vibrator. */
    status = (aux_vreg_value >> 19) & 0x1;
    printf("         Vibrator: ");
    if(status)
    {
        printf("On\n");
    }
    else
    {
        printf("Off\n");
    }  
    status = (aux_vreg_value >> 20) & 0x3;
    printf("   Vibe reg level: 0x%X\n\n", status);
    
    /* Other bits... */
    status = (aux_vreg_value >> 17) & 0x1;
    printf("      VSIM enable: %d\n", status);
    status = (aux_vreg_value >> 16) & 0x1;
    printf("    VSIM 2 enable: %d\n", status);  
    status = (aux_vreg_value >> 18) & 0x1;
    printf("  VSIM 1.875/3.0V: %d\n", status);
}


void regulator_set_sw_reg(int power_ic_file)
{
    int status;
    int user_regulator, regulator;
    int onoff = REGULATOR_UNUSED;
    int level = REGULATOR_UNUSED;
    POWER_IC_REG_ACCESS_T reg_access;
    
    printf("\nSet switchmode regulator\n");
    printf("------------------------\n\n");
    
    printf("Which regulator do you want to change? (1..%d) > ", REGULATOR_NUM_SW_REGS);
    
    /* Get the regulator that's to be altered. */
    if(get_value_from_user(&user_regulator) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid sw regulator? */
    if((user_regulator < 1) || (user_regulator > REGULATOR_NUM_SW_REGS))
    {
        printf("    Error: switchmode regulator %d doesn't exist.\n", user_regulator);
        return;
    }

    /* We see the regulator used in the range 0..MAX-1, whereas the user specifies one in the range
     * 1..MAX. Compensate for this. */
    regulator = user_regulator - 1;

    /* Get a new value for on/off. */
    printf("Turn SW%d on/off? (0..%d) > ", user_regulator, regulator_sw_bits[regulator].onoff_mask);
    if(get_value_from_user(&onoff) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Abandon this if invalid on/off specified. */
    if((onoff > regulator_sw_bits[regulator].onoff_mask) || (onoff < 0))
    {
        printf("    Error: %d is not a valid on/off setting for SW%d.\n", onoff, user_regulator);
        return;
    }
    
    /* Get a new value for level. */
    printf("Set SW%d level? (0..%d) > ", user_regulator, regulator_sw_bits[regulator].level_mask);
    if(get_value_from_user(&level) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Abandon this if invalid level specified. */
    if((level > regulator_sw_bits[regulator].level_mask) || (level < 0))
    {
        printf("    Error: %d is not a valid level for SW%d.\n", level, user_regulator);
        return;
    }
    
    /* Do it. */
    printf("Setting regulator...\n");
    reg_access.reg = POWER_IC_REG_PCAP_SWCTRL;
    reg_access.value = onoff;
    reg_access.index = regulator_sw_bits[regulator].onoff_pos;
    reg_access.num_bits = num_bits_in_mask(regulator_sw_bits[regulator].onoff_mask);
    write_bits(power_ic_file, &reg_access);
     
    reg_access.value = level;
    reg_access.index = regulator_sw_bits[regulator].level_pos;
    reg_access.num_bits = num_bits_in_mask(regulator_sw_bits[regulator].level_mask);
    write_bits(power_ic_file, &reg_access);
    
    /* Verify it. */
    printf("Verifying regulator settings...\n");  
    status = read_reg(power_ic_file, POWER_IC_REG_PCAP_SWCTRL);
    if( ((status >> regulator_sw_bits[regulator].onoff_pos) & regulator_sw_bits[regulator].onoff_mask)
       != onoff )
    {
        printf("Error: regulator on/off does not match set value. Register value 0x%X.\n", status);
    }
    
    if( ((status >> regulator_sw_bits[regulator].level_pos) & regulator_sw_bits[regulator].level_mask)
       != onoff )
    {
        printf("Error: regulator level does not match set value. Register value 0x%X.\n", status);
    }
    
    printf("Regulator set.\n");
}

void regulator_set_vreg(int power_ic_file)
{
    int status;
    int user_regulator, regulator;
    int power_ic_register;
    int onoff;
    int level = REGULATOR_UNUSED;
    int low_power = REGULATOR_UNUSED;
    int stby_ctrl = REGULATOR_UNUSED;
    
    POWER_IC_REG_ACCESS_T reg_access;
    
    printf("\nSet voltage regulator\n");
    printf("---------------------\n\n");
    
    printf("Which regulator do you want to change? (1..%d) > ", REGULATOR_NUM_V_REGS);
    
    /* Get the regulator that's to be altered. */
    if(get_value_from_user(&user_regulator) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid voltage regulator? */
    if((user_regulator < 1) || (user_regulator > REGULATOR_NUM_V_REGS))
    {
        printf("    Error: regulator V%d doesn't exist.\n", user_regulator);
        return;
    }
    
    /* We see the regulator used in the range 0..MAX-1, whereas the user specifies one in the range
     * 1..MAX. Compensate for this. */
    regulator = user_regulator - 1;

    /* Get a new value for on/off. */
    printf("Turn V%d on/off? (0..%d) > ", user_regulator, regulator_vreg_bits[regulator].onoff_mask);
    if(get_value_from_user(&onoff) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Abandon this if invalid on/off specified. */
    if((onoff > regulator_vreg_bits[regulator].onoff_mask) || (onoff < 0))
    {
        printf("    Error: %d is not a valid on/off setting for V%d.\n", onoff, user_regulator);
        return;
    }
    
    /* Get a new value for level. */
    if(regulator_vreg_bits[regulator].level_pos != REGULATOR_UNUSED)
    {
        printf("Set V%d level? (0..%d) > ", user_regulator, regulator_vreg_bits[regulator].level_mask);
        if(get_value_from_user(&level) != 0)
        {
            printf("    Error: not a valid input.\n");
            return;
        }
    
        /* Abandon this if invalid level specified. */
        if((level > regulator_vreg_bits[regulator].level_mask) || (level < 0))
        {
            printf("    Error: %d is not a valid level for V%d.\n", level, user_regulator);
            return;
        }
    }
    
    /* Get a new value for low power mode. */
    if(regulator_vreg_bits[regulator].lowpwr_pos != REGULATOR_UNUSED)
    {
        printf("Set V%d lowpower mode? (0..%d) > ", user_regulator, 
                                                       regulator_vreg_bits[regulator].lowpwr_mask);
        if(get_value_from_user(&low_power) != 0)
        {
            printf("    Error: not a valid input.\n");
            return;
        }
    
        /* Abandon this if invalid low power mode specified. */
        if((low_power > regulator_vreg_bits[regulator].lowpwr_mask) || (low_power < 0))
        {
            printf("    Error: %d is not a valid low power mode for V%d.\n", low_power, user_regulator);
            return;
        }
    }
    
    /* Get a new value for standby control. */
    if(regulator_vreg_bits[regulator].stby_ctrl_pos != REGULATOR_UNUSED)
    {
        printf("Set V%d standby control? (0..%d) > ", user_regulator, 
                                                       regulator_vreg_bits[regulator].stby_ctrl_mask);
        if(get_value_from_user(&stby_ctrl) != 0)
        {
            printf("    Error: not a valid input.\n");
            return;
        }
    
        /* Abandon this if invalid low power mode specified. */
        if((stby_ctrl > regulator_vreg_bits[regulator].lowpwr_mask) || (stby_ctrl < 0))
        {
            printf("    Error: %d is not a valid standby contol for V%d.\n", low_power, user_regulator);
            return;
        }
    }    
    
    /* Do it. */
    printf("Setting regulator...\n");
    
    /* Regulators are split across two registers. Regulator 5 is a bit of a bugger, as we'll find
     * out in a sec... */
    if(regulator < 5)
    {
        reg_access.reg = POWER_IC_REG_PCAP_VREG1;
    }
    else
    {
        reg_access.reg = POWER_IC_REG_PCAP_VREG2;
    }    
    
    /* On/off is available for all regulators. */
    reg_access.value = onoff;
    reg_access.index = regulator_vreg_bits[regulator].onoff_pos;
    reg_access.num_bits = num_bits_in_mask(regulator_vreg_bits[regulator].onoff_mask);
    
    printf("\n\n\nWriting onoff... register %d, value %X, index %d, num bits %d\n\n\n", 
            reg_access.reg, reg_access.value, reg_access.index, reg_access.num_bits);
    
    write_bits(power_ic_file, &reg_access);
     
    if(level != REGULATOR_UNUSED)
    {
        reg_access.value = level;
        reg_access.index = regulator_vreg_bits[regulator].level_pos;
        reg_access.num_bits = num_bits_in_mask(regulator_vreg_bits[regulator].level_mask);
        write_bits(power_ic_file, &reg_access);
        
        printf("\n\n\nWriting level... register %d, value %X, index %d, num bits %d\n\n\n", 
        reg_access.reg, reg_access.value, reg_access.index, reg_access.num_bits);
        
    }
    
    /* Here's where the fun begins. If we are setting V5, then the remaining bits for this are 
     * in VREG2...jump over to the other register now. */
    if(user_regulator == 5)
    {
        reg_access.reg = POWER_IC_REG_PCAP_VREG2;
    }
    
    /* Set low power mode, if available. */
    if(low_power != REGULATOR_UNUSED)
    {
        reg_access.value = low_power;
        reg_access.index = regulator_vreg_bits[regulator].lowpwr_pos;
        reg_access.num_bits = num_bits_in_mask(regulator_vreg_bits[regulator].lowpwr_mask);
        write_bits(power_ic_file, &reg_access);
    }
    
    /* Set standby mode, if available. */
    if(stby_ctrl != REGULATOR_UNUSED)
    {
        reg_access.value = stby_ctrl;
        reg_access.index = regulator_vreg_bits[regulator].stby_ctrl_pos;
        reg_access.num_bits = num_bits_in_mask(regulator_vreg_bits[regulator].stby_ctrl_mask);
        write_bits(power_ic_file, &reg_access);
        
        printf("\n\n\nWriting standby... register %d, value %X, index %d, num bits %d\n\n\n", 
        reg_access.reg, reg_access.value, reg_access.index, reg_access.num_bits);
    }    
        
    /* Verify it. */
    printf("Verifying regulator settings...\n");
    
    /* Again, gotta set which register we care about. */
    if(regulator < 5)
    {
        power_ic_register = POWER_IC_REG_PCAP_VREG1;
    }
    else
    {
        power_ic_register = POWER_IC_REG_PCAP_VREG2;
    }
      
    status = read_reg(power_ic_file, power_ic_register);
    
    
    printf("\n\n\nRead 0x%X from register %d\n\n\n", status, power_ic_register);
    
    if( ((status >> regulator_vreg_bits[regulator].onoff_pos) & regulator_vreg_bits[regulator].onoff_mask)
       != onoff )
    {
        printf("Error: regulator on/off does not match set value. Register value 0x%X.\n", status);
    }
    
    if( (level != REGULATOR_UNUSED) && 
        (((status >> regulator_vreg_bits[regulator].level_pos) & regulator_vreg_bits[regulator].level_mask)
                != level) )
    {
        printf("Error: regulator level does not match set value. Register value 0x%X.\n", status);
    }
    
    /* Do we have to jump over to the other register now? */
    if(user_regulator == 4)
    {
        power_ic_register = POWER_IC_REG_PCAP_VREG2;
        status = read_reg(power_ic_file, power_ic_register);
        
        printf("\n\n\nRead 0x%X from register %d\n\n\n", status, power_ic_register);
    }
    
    if( (low_power != REGULATOR_UNUSED) && 
       (((status >> regulator_vreg_bits[regulator].lowpwr_pos) & regulator_vreg_bits[regulator].lowpwr_mask)
                != low_power) )
    {
        printf("   Error: regulator low power mode does not match set value. Register value 0x%X.\n", status);
    }
    
    if( (stby_ctrl != REGULATOR_UNUSED) && 
       (((status >> regulator_vreg_bits[regulator].stby_ctrl_pos) & regulator_vreg_bits[regulator].stby_ctrl_mask)
                != stby_ctrl) )
    {
        printf("   Error: regulator standby mode does not match set value. Register value 0x%X.\n", status);
    }        
    
    printf("Regulator set.\n");
}


void regulator_set_aux_reg(int power_ic_file)
{
    int status;
    int regulator, user_regulator;
    int onoff;
    int level = REGULATOR_UNUSED;
    int low_power = REGULATOR_UNUSED;
    int stby_ctrl = REGULATOR_UNUSED;
    
    POWER_IC_REG_ACCESS_T reg_access;
    
    printf("\nSet aux regulator\n");
    printf("-----------------\n\n");
    
    printf("Which regulator do you want to change? (1..%d) > ", REGULATOR_NUM_AUX_REGS);
    
    /* Get the regulator that's to be altered. */
    if(get_value_from_user(&user_regulator) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid aux regulator? */
    if((user_regulator < 1) || (user_regulator > REGULATOR_NUM_AUX_REGS))
    {
        printf("    Error: aux regulator %d doesn't exist.\n", regulator);
        return;
    }
    
    /* We see the regulator used in the range 0..MAX-1, whereas the user specifies one in the range
     * 1..MAX. Compensate for this. */
    regulator = user_regulator - 1;

    /* Get a new value for on/off. */
    printf("Turn AUX%d on/off? (0..%d) > ", user_regulator, regulator_aux_reg_bits[regulator].onoff_mask);
    if(get_value_from_user(&onoff) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Abandon this if invalid on/off specified. */
    if((onoff > regulator_aux_reg_bits[regulator].onoff_mask) || (onoff < 0))
    {
        printf("    Error: %d is not a valid on/off setting for AUX%d.\n", onoff, user_regulator);
        return;
    }
    
    /* Get a new value for level. */
    if(regulator_aux_reg_bits[regulator].level_pos != REGULATOR_UNUSED)
    {
        printf("Set AUX%d level? (0..%d) > ", user_regulator, regulator_aux_reg_bits[regulator].level_mask);
        if(get_value_from_user(&level) != 0)
        {
            printf("    Error: not a valid input.\n");
            return;
        }
    
        /* Abandon this if invalid level specified. */
        if((level > regulator_aux_reg_bits[regulator].level_mask) || (level < 0))
        {
            printf("    Error: %d is not a valid level for AUX%d.\n", level, user_regulator);
            return;
        }
    }
    
    /* Get a new value for low power mode. */
    if(regulator_aux_reg_bits[regulator].lowpwr_mask != REGULATOR_UNUSED)
    {
        printf("Set AUX%d lowpower mode? (0..%d) > ", user_regulator, 
                                                       regulator_aux_reg_bits[regulator].lowpwr_mask);
        if(get_value_from_user(&low_power) != 0)
        {
            printf("    Error: not a valid input.\n");
            return;
        }
    
        /* Abandon this if invalid low power mode specified. */
        if((low_power > regulator_aux_reg_bits[regulator].lowpwr_mask) || (low_power < 0))
        {
            printf("    Error: %d is not a valid low power mode for AUX%d.\n", low_power, user_regulator);
            return;
        }
        
        printf("    Lowpwr will be set to %d\n", low_power);
    }
    
    /* Get a new value for standby control. */
    if(regulator_aux_reg_bits[regulator].stby_ctrl_pos != REGULATOR_UNUSED)
    {
        printf("Set AUX%d standby control? (0..%d) > ", user_regulator, 
                                                       regulator_aux_reg_bits[regulator].stby_ctrl_mask);
        if(get_value_from_user(&stby_ctrl) != 0)
        {
            printf("    Error: not a valid input.\n");
            return;
        }
    
        /* Abandon this if invalid standby control specified. */
        if((stby_ctrl > regulator_aux_reg_bits[regulator].stby_ctrl_mask) || (stby_ctrl < 0))
        {
            printf("    Error: %d is not a valid standby contol for AUX%d.\n", stby_ctrl, user_regulator);
            return;
        }
        
        printf("    Stby control will be set to %d\n", stby_ctrl);
    }    
    
    /* Do it. */
    printf("Setting regulator...\n");
    
    reg_access.reg = POWER_IC_REG_PCAP_AUX_VREG;
    
    /* On/off is available for all regulators. */
    reg_access.value = onoff;
    reg_access.index = regulator_aux_reg_bits[regulator].onoff_pos;
    reg_access.num_bits = num_bits_in_mask(regulator_aux_reg_bits[regulator].onoff_mask);
    write_bits(power_ic_file, &reg_access);
     
    if(level != REGULATOR_UNUSED)
    {
        reg_access.value = level;
        reg_access.index = regulator_aux_reg_bits[regulator].level_pos;
        reg_access.num_bits = num_bits_in_mask(regulator_aux_reg_bits[regulator].level_mask);
        write_bits(power_ic_file, &reg_access);
    }
    

    /* Set low power mode, if available. */
    if(low_power != REGULATOR_UNUSED)
    {
        reg_access.value = low_power;
        reg_access.index = regulator_aux_reg_bits[regulator].lowpwr_pos;
        reg_access.num_bits = num_bits_in_mask(regulator_aux_reg_bits[regulator].lowpwr_mask);
        
        printf("Writing low power bit. setting: %d, index:%d, num_bits: %d\n", 
                reg_access.value, reg_access.index, reg_access.num_bits);
        
        write_bits(power_ic_file, &reg_access);
    }
    
    /* Set standby mode, if available. */
    if(stby_ctrl != REGULATOR_UNUSED)
    {
        reg_access.value = stby_ctrl;
        reg_access.index = regulator_aux_reg_bits[regulator].stby_ctrl_pos;
        reg_access.num_bits = num_bits_in_mask(regulator_aux_reg_bits[regulator].stby_ctrl_mask);
        write_bits(power_ic_file, &reg_access);
    }    
        
    /* Verify it. */
    printf("Verifying regulator settings...\n");
      
    status = read_reg(power_ic_file, POWER_IC_REG_PCAP_AUX_VREG);
    
    if( ((status >> regulator_aux_reg_bits[regulator].onoff_pos) & regulator_aux_reg_bits[regulator].onoff_mask)
       != onoff )
    {
        printf("Error: regulator on/off does not match set value. Register value 0x%X.\n", status);
    }
    
    if( (level != REGULATOR_UNUSED) && 
       (((status >> regulator_aux_reg_bits[regulator].level_pos) & regulator_aux_reg_bits[regulator].level_mask)
                != level) )
    {
        printf("Error: regulator level does not match set value. Register value 0x%X.\n", status);
    }
    
    if( (low_power != REGULATOR_UNUSED) && 
       (((status >> regulator_aux_reg_bits[regulator].lowpwr_pos) & regulator_aux_reg_bits[regulator].lowpwr_mask)
                != low_power) )
    {
        printf("   Error: regulator low power mode does not match set value. Register value 0x%X.\n", status);
    }
    
    if( (stby_ctrl != REGULATOR_UNUSED) && 
       (((status >> regulator_aux_reg_bits[regulator].stby_ctrl_pos) & regulator_aux_reg_bits[regulator].stby_ctrl_mask)
                != stby_ctrl) )
    {
        printf("   Error: regulator standby mode does not match set value. Register value 0x%X.\n", status);
    }        
    
    printf("Regulator set.\n");
}

void set_core_voltage(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int ioctl_result;
    int new_level;
    

    printf("\nSet core voltage\n");
    printf("------------------\n");
    
    /* Get a new voltage level target for the core. */
    printf("Set core voltage? (900mV .. 1600mV)");
    if(get_value_from_user(&new_level) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    /*Values above recommended range will be allowed so that functions upper safety limit can be tested*/   
    else if((new_level < 0) || (new_level > 4500))
    {
        printf("    Error: %d is not a valid level for the core.\n", new_level);
    }
    
    /* Write new level to power IC. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_PMM_SET_CORE_VOLTAGE, new_level);
    
    if(ioctl_result == 0)
    {
    
        /* The core voltage level is controlled by bits 2..5 in register 4. Read this to verify that the
         * correct bit was set.*/
        reg_access.reg = POWER_IC_REG_PCAP_SWCTRL;
        reg_access.num_bits = 4;
        reg_access.index = 2;
        reg_access.value = 0xFFFFFFFF;
        
        ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
        
        if(ioctl_result == 0)
        {
            if(reg_access.value <= 0xC)
            {
                printf("    Core was set correctly.\n");
                printf("    The requested core setting: was %d mv, write  %s.\n", 
                        new_level, str_tbl_sw_regs[1][reg_access.value]);
            }
            else
            {
                printf("    Error verifying core setting: set %d, read %s.\n", 
                        new_level, str_tbl_sw_regs[1][reg_access.value]);
            }
        }
        else
        {
            printf("   Error verifying core setting - ioctl() returned %d.\n", ioctl_result);
        }
    
    }
    else
    {
        printf("   Error setting core level - ioctl() returned %d.\n", ioctl_result);
    }
}


/**********************************************************************************************
 *
 * RTC functions.
 *
 *********************************************************************************************/
void rtc_get_status(int power_ic_file)
{
    int time_of_day_reg_value = 0;
    int time_of_day_alarm_reg_value = 0;
    int day_reg_value = 0;
    int day_alarm_reg_value = 0;
    int time_of_day = 0;
    int day = 0;
    int time_of_day_alarm, day_alarm;
    time_t time, time_a;
    char * time_str;
    int err = 0;
    struct timeval power_ic_time;
    struct timeval curr_power_ic_time;
    
    power_ic_time.tv_sec = 31536000;
    curr_power_ic_time.tv_sec = 0;
    printf("\nReal-time clock status\n");
    printf("----------------------\n\n");
    
    /* Get the various RTC registers. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_GET_TIME, &curr_power_ic_time );
    printf("error %d", err);
    time = curr_power_ic_time.tv_sec;
    time_of_day = curr_power_ic_time.tv_sec % 86400;
    day = curr_power_ic_time.tv_sec / 86400;
    err = ioctl(power_ic_file, POWER_IC_IOCTL_GET_ALARM_TIME, &curr_power_ic_time);
    time_a = curr_power_ic_time.tv_sec;
    time_of_day_alarm = curr_power_ic_time.tv_sec % 86400;
    day_alarm = curr_power_ic_time.tv_sec / 86400;
    time_of_day_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_TOD);
    time_of_day_alarm_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_TODA);
    day_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_DAY);
    day_alarm_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_DAYA);
    
    /* TOD Register breakdown. */
    printf("   RTC_TOD register contents: 0x%7X\n", time_of_day_reg_value);
    printf("                      Time of day counter: 0x%X\n", time_of_day);
       
    /* TODA Register breakdown. */    
    printf("  RTC_TODA register contents: 0x%7X\n", time_of_day_alarm_reg_value);
    printf("                        Time of day alarm: 0x%X\n", time_of_day_alarm);     

    /* DAY reister breakdown. */
    printf("   RTC_DAY register contents: 0x%7X\n", day_reg_value);
    printf("                              Day counter: 0x%X\n", day);   

    /* DAYA register breakdown. */    
    printf("  RTC_DAYA register contents: 0x%7X\n\n", day_alarm_reg_value);
    printf("                                Day alarm: 0x%X\n", day_alarm);   

    /* Times these correspond to? */
    //time = (day * 24 * 60 * 60) + time_of_day;
    time_str = ctime(&time);
    time_str = ctime(&time);      
    printf("\n                     Time?: %s", time_str);
    
    //time = (day_alarm * 24 * 60 * 60) + time_of_day_alarm;
    time_str = ctime(&time_a);
    time_str = ctime(&time_a);    
    printf("                    Alarm?: %s", time_str);    
}

void rtc_set_times(int power_ic_file)
{
    int time_of_day_reg_value = 0;
    int time_of_day_alarm_reg_value = 0;
    int day_reg_value = 0;
    int day_alarm_reg_value = 0;
    int time_of_day = 0;
    int day = 0;
    int time_of_day_alarm, day_alarm;
    time_t time, time_a;
    char * time_str;
    int err = 0;
    struct timeval power_ic_time;
    struct timeval power_ic_time_alarm;
    struct timeval curr_power_ic_time;
    
    power_ic_time.tv_sec = 31536000;
    power_ic_time_alarm.tv_sec = 31566000;
    curr_power_ic_time.tv_sec = 0;
    printf("\nReal-time clock set\n");
    printf("----------------------\n\n");
    
    /* Set the various RTC registers. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_SET_TIME, &power_ic_time );
    err = ioctl(power_ic_file, POWER_IC_IOCTL_SET_ALARM_TIME, &power_ic_time_alarm);
    err = ioctl(power_ic_file, POWER_IC_IOCTL_GET_TIME, &curr_power_ic_time );
    time = curr_power_ic_time.tv_sec;
    time_of_day = curr_power_ic_time.tv_sec % 86400;
    day = curr_power_ic_time.tv_sec / 86400;
    err = ioctl(power_ic_file, POWER_IC_IOCTL_GET_ALARM_TIME, &curr_power_ic_time);
    time_a = curr_power_ic_time.tv_sec;
    time_of_day_alarm = curr_power_ic_time.tv_sec % 86400;
    day_alarm = curr_power_ic_time.tv_sec / 86400;
    time_of_day_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_TOD);
    time_of_day_alarm_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_TODA);
    day_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_DAY);
    day_alarm_reg_value = read_reg(power_ic_file, POWER_IC_REG_PCAP_RTC_DAYA);
    
    /* TOD Register breakdown. */
    printf("   RTC_TOD register contents: 0x%7X\n", time_of_day_reg_value);
    printf("                      Time of day counter: 0x%X\n", time_of_day);
       
    /* TODA Register breakdown. */    
    printf("  RTC_TODA register contents: 0x%7X\n", time_of_day_alarm_reg_value);
    printf("                        Time of day alarm: 0x%X\n", time_of_day_alarm);     

    /* DAY reister breakdown. */
    printf("   RTC_DAY register contents: 0x%7X\n", day_reg_value);
    printf("                              Day counter: 0x%X\n", day);   

    /* DAYA register breakdown. */    
    printf("  RTC_DAYA register contents: 0x%7X\n\n", day_alarm_reg_value);
    printf("                                Day alarm: 0x%X\n", day_alarm);   

    /* Times these correspond to? */
    //time = (day * 24 * 60 * 60) + time_of_day;
    time_str = ctime(&time);
    time_str = ctime(&time);      
    printf("\n                     Time?: %s", time_str);
    
    //time = (day_alarm * 24 * 60 * 60) + time_of_day_alarm;
    time_str = ctime(&time_a);
    time_str = ctime(&time_a);    
    printf("                    Alarm?: %s", time_str);    
}

void audio_set_output_path(int power_ic_file)
{
    int err = 0;
    unsigned int path;
    int reg_val;

    path = read_reg(power_ic_file,POWER_IC_REG_PCAP_RX_AUD_AMPS);
    printf("The Initial audio out path is 0x%7x\n",  path);
    /*Set a audio path*/
    printf("The audio path is set to handset path\n");
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_OUTPATH,POWER_IC_HANDSET_SPEAKER_MASK);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_RX_AUD_AMPS);
    printf("The register value is 0x%7x, the path is %d\n", reg_val,path);    
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_OUTPATH,&path);
    printf("The audio path read out: %d\n", path);
    
    printf("The audio path is set to headdset path\n");
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_OUTPATH,POWER_IC_HEADSET_SPEAKER_MASK);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_RX_AUD_AMPS);
    printf("The register value is 0x%7x\n", reg_val);    
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_OUTPATH,&path);
    printf("The audio path is: %d\n", path);
    
    printf("The audio path is set to alert path\n");
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_OUTPATH,POWER_IC_ALERT_SPEAKER_MASK);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_RX_AUD_AMPS);
    printf("The register value is 0x%7x\n", reg_val);    
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_OUTPATH,&path);
    printf("The audio path is: %d\n", path);

    printf("The audio path is set to alert path\n");
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_OUTPATH,POWER_IC_BUS_SPEAKER_MASK);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_RX_AUD_AMPS);
    printf("The register value is 0x%7x\n", reg_val);    
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_OUTPATH,&path);
    printf("The audio path is: %d\n", path);
    
}
void audio_set_input_path(int power_ic_file)
{
    
    int path;
    int reg_val;
    int err = 0;
    /*Set a audio path*/
    printf("The audio in path is set to handset\n");
    path = read_reg(power_ic_file,POWER_IC_REG_PCAP_TX_AUD_AMPS);
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_INPATH,POWER_IC_HANDSET_MIC);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_TX_AUD_AMPS);
    printf("The register value is 0x%7x\n", reg_val);    
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_INPATH,&path);
    printf("The audio input path is: %d\n", path);

    printf("The audio path is set to handset\n");
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_INPATH,POWER_IC_HEADSET_MIC);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_TX_AUD_AMPS);
    printf("The register value is 0x%7x\n", reg_val);    
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_INPATH,&path);
    printf("The audio input path is:%d\n", path);

    printf("The audio path is set to External\n");
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_WRITE_INPATH,POWER_IC_EXTERNAL_MIC);
    reg_val = read_reg(power_ic_file,POWER_IC_REG_PCAP_TX_AUD_AMPS);
    printf("The register value is 0x%7x\n", reg_val);
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_INPATH,&path);
    printf("The audio input path is: %d\n", path);
}

void audio_set_codec_on(int power_ic_file)
{
    int err = 0;
    int val;
    err = ioctl(power_ic_file, POWER_IC_IOCTL_AUDIO_CODEC_EN,1);
    val = read_reg(power_ic_file,POWER_IC_REG_PCAP_AUD_CODEC);
    printf("The codec is %x\n",val);
}
void audio_set_out_gain(int power_ic_file)
{
    int err =0;
    int val;
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_WRITE_OGAIN,2);
    err = ioctl(power_ic_file,POWER_IC_IOCTL_AUDIO_READ_OGAIN,&val);
    printf("The out gain is %d\n", val);
}
/**********************************************************************************************
 *
 * AtoD conversion functions
 *
 *********************************************************************************************/
void atod_convert_single_channel(int power_ic_file)
{
    int err;
    int channel;
    
    POWER_IC_ATOD_REQUEST_SINGLE_CHANNEL_T request;

    printf("\nConvert single AtoD channel\n");
    printf("---------------------------\n\n");
    
    printf("Which channel do you wish to convert? (0..%d) > ", POWER_IC_ATOD_NUM_CHANNELS-1);
    
    /* Get the regulator that's to be altered. */
    if(get_value_from_user(&channel) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid aux regulator? */
    if((channel < 0) || (channel >= POWER_IC_ATOD_NUM_CHANNELS))
    {
        printf("    Error: AtoD channel %d doesn't exist.\n", channel);
        return;
    }
    
    /* Set up structure for conversion. */
    request.channel = channel;
    request.result = 0x11; /* This is only for visibility - this should make it easier to see if the value returned is sensible. */
    
    /* Perform the conversion. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_SINGLE_CHANNEL, &request);

    printf("\n\n");
    
    /* Show the result of the conversion. */
    if(err == 0)
    {
        printf("   Result of channel %d conversion: 0x%X\n", channel, request.result);
    }
    else
    {
        printf("   Error %d converting channel %d\n", err, channel);
    }
    
}

void atod_convert_general_channels(int power_ic_file)
{
    int err;
    
    POWER_IC_ATOD_RESULT_GENERAL_CONVERSION_T request;

    printf("\nConvert general AtoD channels\n");
    printf("-----------------------------\n\n");
    
    printf("Converting general set of AtoD channels.\n");
    
    /* Perform the conversion. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_GENERAL, &request);
    
    printf("\n\n");
    
    /* Show the result of the conversion. */
    if(err == 0)
    {
        printf("   Results of general conversion:\n");
        printf("                       coin cell: 0x%X\n", request.coin_cell);
        printf("                         battery: 0x%X\n", request.battery);
        printf("                           bplus: 0x%X\n", request.bplus);
        printf("                        mobportb: 0x%X\n", request.mobportb);
        printf("                      charger ID: 0x%X\n", request.charger_id);
        printf("                     temperature: 0x%X\n", request.temperature);
    }
    else
    {
        printf("   Error %d converting general channels.\n", err);
    }
}

void atod_convert_batt_and_curr_immediate(int power_ic_file)
{
    int err;
    
    POWER_IC_ATOD_REQUEST_BATT_AND_CURR_T request;

    printf("\nConvert battery/current AtoD channels (immediate)\n");
    printf("-------------------------------------------------\n\n");
    
    printf("Immediately converting pairs of battery & current AtoD channels.\n");
    
    /* Set up structure for conversion. */
    request.timing = POWER_IC_ATOD_TIMING_IMMEDIATE;
    request.polarity = POWER_IC_ATOD_CURR_POLARITY_DISCHARGE;
    /* This is only for visibility - this should make it easier to see if the value returned is sensible. */
    request.batt_result = 0x11;
    request.curr_result = 0x11;
    
    /* Perform the conversion. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_BATT_AND_CURR, &request);
    
    printf("\n\n");
    
    /* Show the result of the conversion. */
    if(err == 0)
    {
        printf("   Results of batt/curr conversion:\n");
        printf("                           battery: 0x%X\n", request.batt_result);
        printf("                           current: %dmA\n", request.curr_result);
    }
    else
    {
        printf("   Error %d converting battery/current channels.\n", err);
    }
}

void atod_convert_batt_and_curr_phased_immediate(int power_ic_file)
{
    int err;
    
    POWER_IC_ATOD_REQUEST_BATT_AND_CURR_T request;

    printf("\nConvert battery/current AtoD channels (immediate)\n");
    printf("-------------------------------------------------\n\n");
    
    printf("Immediately converting pairs of battery & current AtoD channels.\n");
    
    /* Set up structure for conversion. */
    request.timing = POWER_IC_ATOD_TIMING_IMMEDIATE;
    request.polarity = POWER_IC_ATOD_CURR_POLARITY_DISCHARGE;
    /* This is only for visibility - this should make it easier to see if the value returned is sensible. */
    request.batt_result = 0x11;
    request.curr_result = 0x11;
    
    /* Perform the conversion. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_BATT_AND_CURR_PHASED, &request);
    
    printf("\n\n");
    
    /* Show the result of the conversion. */
    if(err == 0)
    {
        printf("   Results of batt/curr conversion:\n");
        printf("                           battery: 0x%X\n", request.batt_result);
        printf("                           current: %dDAC\n", request.curr_result);
    }
    else
    {
        printf("   Error %d converting battery/current channels.\n", err);
    }
}

void atod_convert_batt_and_curr_delayed(int power_ic_file)
{
    int err;
    int timeout;
    
    POWER_IC_ATOD_REQUEST_BATT_AND_CURR_T request;

    printf("\nConvert battery/current AtoD channels\n");
    printf("-------------------------------------\n\n");
    
    printf("How long a timeout do you want? (secs) ");
    
        /* Get the regulator that's to be altered. */
    if(get_value_from_user(&timeout) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid timeout? */
    if(timeout <= 0)
    {
        printf("    Error: timeout must be greater than zero.\n");
        return;
    }
    
    /* Set up structure for conversion. */
    request.timing = POWER_IC_ATOD_TIMING_IN_BURST;
    request.timeout_secs = timeout;
    request.polarity = POWER_IC_ATOD_CURR_POLARITY_DISCHARGE;
    /* This is only for visibility - this should make it easier to see if the value returned is sensible. */
    request.batt_result = 0x11;
    request.curr_result = 0x11;

    /* Perform the conversion. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_BATT_AND_CURR, &request);
    
    printf("\n\n");
    
    /* Show the result of the conversion. */
    if(err == POWER_IC_ATOD_CONVERSION_COMPLETE)
    {
        printf("   Results of batt/curr conversion:\n");
        printf("                           battery: 0x%X\n", request.batt_result);
        printf("                           current: %dmA\n", request.curr_result);
    }
    else if(err == POWER_IC_ATOD_CONVERSION_TIMEOUT)
    {
        printf("   Conversion timed out without conversion occurring:\n");
    }
    else
    {
        printf("   Error %d converting battery/current channels.\n", err);
    }
}

void atod_convert_raw(int power_ic_file)
{
    int err;
    int channel;
    int i;
    
    POWER_IC_ATOD_REQUEST_RAW_T request;

    printf("\nRaw conversion results for single AtoD channel\n");
    printf("----------------------------------------------\n\n");
    
    printf("Which channel do you wish to convert? (0..%d) > ", POWER_IC_ATOD_NUM_CHANNELS-1);
    
    /* Get the regulator that's to be altered. */
    if(get_value_from_user(&channel) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid aux regulator? */
    if((channel < 0) || (channel >= POWER_IC_ATOD_NUM_CHANNELS))
    {
        printf("    Error: AtoD channel %d doesn't exist.\n", channel);
        return;
    }
    
    /* Set up structure for conversion. */
    request.channel = channel;
    
    /* Perform the conversion. */
    err = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_RAW, &request);
    
    printf("\n\n");
    
    /* Show the result of the conversion. */
    if(err == 0)
    {
        printf("   Result of channel %d test conversion: ", channel);
        for(i = 0; i < POWER_IC_ATOD_NUM_RAW_RESULTS; i++)
        {
            printf(" 0x%X,", request.results[i]);
        }
        printf("\n");
    }
    else
    {
        printf("   Error %d converting channel %d\n", err, channel);
    }
    
}

void atod_convert_nonblock_start(int power_ic_file)
{
    POWER_IC_ATOD_REQUEST_BATT_AND_CURR_T request;
    int timing;
    int ioctl_result;
    
    printf("\nStart non-blocking batt/current conversion\n");
    printf("------------------------------------------\n\n");
    
    printf("Should this conversion be in-burst (%d) or out of burst (%d)? > ", 
            POWER_IC_ATOD_TIMING_IN_BURST, POWER_IC_ATOD_TIMING_OUT_OF_BURST);
    
    /* Get the timing for the conversion (in/out burst). */
    if(get_value_from_user(&timing) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this timing valid? */
    if((timing != POWER_IC_ATOD_TIMING_IN_BURST) && (timing != POWER_IC_ATOD_TIMING_OUT_OF_BURST))
    {
        printf("    Error: invalid timing entered.\n");
        return;
    }
    
    /* Set up structure for conversion. For now, we'll just always look at the discharge current.*/
    request.timing = timing;
    request.polarity = POWER_IC_ATOD_CURR_POLARITY_DISCHARGE;
    
    /* Start the conversion. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_BEGIN_CONVERSION, &request);
    
    /* Indicate the result. */
    if(ioctl_result == 0)
    {
        printf("Conversion started successfully.\n");
    }
    else
    {
        printf("Error %d starting non-blocking conversion.\n", ioctl_result);
    }
    
}

void atod_convert_nonblock_poll(int power_ic_file)
{
    int poll_result;
    struct pollfd poll_info = 
    {
        .fd = power_ic_file,
    };
    
    printf("\nPoll non-blocking batt/current conversion\n");
    printf("-----------------------------------------\n\n");
    
    printf("Checking for completed non-blocking conversion...\n");
    
    /* Poll for the status. The structure is just used to obtain the results if ready. */
    poll_result = poll(&poll_info, 1, 1);
    
    if(poll_result > 0)
    {
        printf("   Results of conversion are available.\n");
    }
    else if (poll_result == 0)
    {
        printf("   Conversion not yet complete.\n");
    }
    else
    {
        printf("   An error occurred while polling for conversion complete.\n");
    }
}

void atod_convert_nonblock_cancel(int power_ic_file)
{
    int unused;
    int ioctl_result;
    
    printf("\nCancel non-blocking batt/current conversion\n");
    printf("-------------------------------------------\n\n");
    
    printf("Cancelling conversion...\n");
    
    /* Cancelling the conversion just needs the ioctl call with no setup. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_CANCEL_CONVERSION, unused);
    
    switch(ioctl_result)
    {
        case -EFAULT:
            printf("   No non-blocking conversion is in progress!\n");
            break;
        
        case 0:
            printf("   Conversion cancelled.\n");
            break;
        
        default:
            printf("Error %d while cancelling conversion.\n", ioctl_result);
            break;
    }
}

void atod_phasing_set(int power_ic_file)
{
    unsigned char phasing[] =
    {
        0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
        0x00, 0x80, 0x00, 0x80
    };
    
    int ioctl_result;
    int i;
    int value;
    unsigned char slope, offset;
    
    printf("\nSet AtoD phasing\n");
    printf("----------------\n\n");
    
    printf("\nThis will set each channel's phasing to the same slope and offset.\n");
    printf("Note that a slope of 0x00 or oxFF will result in the phasing being rejected.\n\n");
    
    /* Get an offset from the user.. */
    printf("Enter offset (0x00 - 0xFF) > ");
    if(get_value_from_user(&value) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    else if((value > 0xFF) || (value < 0))
    {
        printf("    Error: 0x%X is not in the range 0x00 - 0xFF.\n", value);
        return;
    }
    
    offset = (unsigned char) value;
    
     /* Get a slope from the user.. */
    printf("Enter slope (0x00 - 0xFF) > ");
    if(get_value_from_user(&value) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    else if((value > 0xFF) || (value < 0)) 
    {
        printf("    Error: 0x%X is not in the range 0x00 - 0xFF.\n", value);
        return;
    }
    
    slope = (unsigned char) value;   
    
    /* Set up the offset and slope for all entries. */
    printf("\n\nSetting phasing:\n   ");
    for(i=0; i<sizeof(phasing); i+=2)
    {
        phasing[i] = offset;
        phasing[i+1] = slope;
        
        printf("0x%X ", phasing[i]);
        printf("0x%X ", phasing[i+1]);
    }
    
    printf("\n\n");
    
    /* Set the AtoD driver's phasing. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_SET_PHASING, phasing);
    
    /* Indicate the result. */
    if(ioctl_result == 0)
    {
        printf("Phasing set successfully.\n");
    }
    else
    {
        printf("Error %d setting phasing.\n", ioctl_result);
    }
}


void atod_phasing_set_default(int power_ic_file)
{
    unsigned char phasing[] =
    {
        0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
        0x00, 0x80, 0x00, 0x80
    };
    
    int ioctl_result;
    int i;
    
    printf("\nSetting default AtoD phasing\n");
    printf("----------------------------\n\n");
    
    printf("\n\nSetting defaults:\n   ");
    
    for(i=0; i<sizeof(phasing); i++)
    {
        printf("0x%X ", phasing[i]);
    }
    printf("\n\n");
    
    /* Set the AtoD driver's phasing. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_SET_PHASING, phasing);
    
    /* Indicate the result. */
    if(ioctl_result == 0)
    {
        printf("Phasing set successfully.\n");
    }
    else
    {
        printf("Error %d setting phasing.\n", ioctl_result);
    }
}

void atod_phasing_set_invalid(int power_ic_file)
{
    unsigned char phasing[] =
    {
        0x00, 0x80, 0x00, 0xFF, 0x00, 0x80, 0x00, 0x80,
        0x00, 0x80, 0x00, 0x80
    };
    
    int ioctl_result;
    int i;
    
    printf("\nAttempting to set invalid AtoD phasing\n");
    printf("--------------------------------------\n\n");
    
    printf("\n\nSetting invalid phasing - this should fail:\n   ");
    
    for(i=0; i<sizeof(phasing); i++)
    {
        printf("0x%X ", phasing[i]);
    }
    printf("\n\n");
    
    /* Set the AtoD driver's phasing. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_ATOD_SET_PHASING, phasing);
    
    /* Indicate the result. */
    if(ioctl_result == 0)
    {
        printf("Error: driver accepted invalid phasing.\n");
    }
    else
    {
        printf("Driver correctly rejected invalid phasing\n");
    }
}

/**********************************************************************************************
 *
 * Charger control functions
 *
 *********************************************************************************************/

void charger_set_charge_voltage(int power_ic_file)
{
    int voltage;
    int err;

    printf("\nSet charge voltage\n");
    printf("------------------\n\n");
    
    printf("Enter new charge voltage setting > ");
    
    /* Get the setting from the user. */
    if(get_value_from_user(&voltage) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_SET_CHARGE_VOLTAGE, voltage);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Charge voltage set successfully.\n");
    }
    else
    {
        printf("   Error %d setting charge voltage.\n", err);
    }
}

void charger_set_charge_current(int power_ic_file)
{
    int current;
    int err;

    printf("\nSet charge current\n");
    printf("------------------\n\n");
    
    printf("Enter new charge current setting > ");
    
    /* Get the setting from the user. */
    if(get_value_from_user(&current) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_SET_CHARGE_CURRENT, current);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Charge current set successfully.\n");
    }
    else
    {
        printf("   Error %d setting charge current.\n", err);
    }
}

void charger_set_trickle_current(int power_ic_file)
{
    int current;
    int err;

    printf("\nSet trickle current\n");
    printf("-------------------\n\n");
    
    printf("Enter new trickle charge current setting > ");
    
    /* Get the setting from the user. */
    if(get_value_from_user(&current) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_SET_TRICKLE_CURRENT, current);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Trickle charge current set successfully.\n");
    }
    else
    {
        printf("   Error %d setting trickle charge current.\n", err);
    }
}

void charger_set_power_path(int power_ic_file)
{
    int path;
    int err;

    printf("\nSet power path\n");
    printf("--------------\n\n");
    
    printf("Enter new trickle charge current setting.\n");
    printf("\n");
    printf("    %d: Dual path     (hardware controlled)\n", POWER_IC_CHARGER_POWER_DUAL_PATH);
    printf("    %d: Current share (hardware controlled)\n", POWER_IC_CHARGER_POWER_CURRENT_SHARE);
    printf("    %d: Dual path     (software override)\n", POWER_IC_CHARGER_POWER_DUAL_PATH_SW_OVERRIDE);
    printf("    %d: Current share (software override)\n", POWER_IC_CHARGER_POWER_CURRENT_SHARE_SW_OVERRIDE);    
    printf("\n");
    printf("  > ");
    
    /* Get the setting from the user. */
    if(get_value_from_user(&path) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Quit if invalid setting specified. */
    if((path <= POWER_IC_CHARGER_POWER_DUAL_PATH) || (path > POWER_IC_CHARGER_POWER_CURRENT_SHARE_SW_OVERRIDE))
    {
        printf("    Error: %d is not a valid path.\n", path);
        return;
    }    
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_SET_POWER_PATH, path);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Power path set successfully.\n");
    }
    else
    {
        printf("   Error %d setting path.\n", err);
    }
}


void charger_get_overvoltage(int power_ic_file)
{
    int overvoltage;
    int err;

    printf("\nGet overvoltage state\n");
    printf("---------------------\n\n");
    
    printf("Current overvoltage state: ");
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_GET_OVERVOLTAGE, &overvoltage);

    /* Indicate success/failure. */
    if(err == 0)
    {
        if(overvoltage)
        {
            printf("overvoltage condition detected.\n");
        }
        else
        {
            printf("not overvoltage.\n");
        }
    }
    else
    {
        printf(" Error %d getting overvoltage state.\n", err);
    }
}


void charger_reset_overvoltage(int power_ic_file)
{
    int overvoltage;
    int err;

    printf("\nReset overvoltage\n");
    printf("-----------------\n\n");
    
    printf("Current overvoltage state: ");
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_GET_OVERVOLTAGE, &overvoltage);

    /* Indicate success/failure. */
    if(err == 0)
    {
        if(overvoltage)
        {
            printf("overvoltage condition detected.\n");
        }
        else
        {
            printf("not overvoltage.\n");
        }
    }
    else
    {
        printf(" Error %d getting overvoltage state.\n", err);
    }
    
    printf("\nResetting overvoltage state...\n\n");
    
    printf("New overvoltage state: ");
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_CHARGER_GET_OVERVOLTAGE, &overvoltage);

    /* Indicate success/failure. */
    if(err == 0)
    {
        if(overvoltage)
        {
            printf("overvoltage condition still present.\n");
        }
        else
        {
            printf("no overvoltage - reset successful.\n");
        }
    }
    else
    {
        printf(" Error %d getting overvoltage state.\n", err);
    }
}

/**********************************************************************************************
 *
 * Touchscreen control functions
 *
 *********************************************************************************************/
void touchscreen_enable(int power_ic_file)
{
    int err;

    printf("\nEnable Touchscreen\n");
    printf("-------------------\n\n");
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_TS_ENABLE);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Touchscreen enabled successfully.\n");
    }
    else
    {
        printf("   Error %d setting Touchscreen enable.\n", err);
    }
}

void touchscreen_disable(int power_ic_file)
{
    int err;

    printf("\nDisable Touchscreen\n");
    printf("-------------------\n\n");
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_TS_DISABLE);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Touchscreen disabled successfully.\n");
    }
    else
    {
        printf("   Error %d setting Touchscreen disable.\n", err);
    }
}

void touchscreen_set_poll_time(int power_ic_file)
{
    int poll_time;
    int err;

    printf("\nSet touchscreen poll time\n");
    printf("-------------------\n\n");
    
    printf("Enter new touchscreen poll time > ");
    
    /* Get the setting from the user. */
    get_value_from_user(&poll_time);
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_TS_SET_INTERVAL, poll_time);
    
    printf("\n\n");
    
    /* Indicate success/failure. */
    if(err == 0)
    {
        printf("   Touchscreen poll time set successfully.\n");
    }
    else
    {
        printf("   Error %d setting Touchscreen poll time.\n", err);
    }
}

#define EMU_VCHRG_MASK              0x00000007
#define EMU_ICHRG_MASK              0x00000078
#define EMU_ICHRG_SHIFT             3
#define EMU_ICHRG_TR_MASK           0x00000380   
#define EMU_ICHRG_TR_SHIFT          7
#define EMU_FET_OVRD_MASK           0x00000400
#define EMU_FET_CTRL_MASK           0x00000800   

void charger_get_setup(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T access;
    int err;

    printf("\nGet charger setup\n");
    printf("-----------------\n\n");
    
    printf("Getting charger setup.\n");
    access.reg = POWER_IC_REG_EOC_POWER_CONTROL_0;
    
    err = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &access);
 
    if(err != 0)
    {
        printf("   Error %d getting charger setup.\n", err);
        return;
    }
    else
    {
        printf("   Read 0x%X from charger control register.\n", access.value);
    }
    
    printf("\n");
    
    printf("      VCHRG: 0x%X\n", (access.value & EMU_VCHRG_MASK));
    printf("      ICHRG: 0x%X\n", (access.value & EMU_ICHRG_MASK) >> EMU_ICHRG_SHIFT); 
    printf("   ICHRG_TR: 0x%X\n", (access.value & EMU_ICHRG_TR_MASK) >> EMU_ICHRG_TR_SHIFT); 
    printf("   FET OVRD: %d\n",   ((access.value & EMU_FET_OVRD_MASK) ? 1:0) ); 
    printf("   FET CTRL: %d\n",   ((access.value & EMU_FET_CTRL_MASK) ? 1:0) );
    printf("             ---> Looks like ");
    
    if(access.value & EMU_FET_CTRL_MASK)
    {
        printf("Current Share");
        
    }
    else
    {
        printf("Dual path");        
    }
    
    if(access.value & EMU_FET_OVRD_MASK)
    {
        printf(" (software override)");
    } 
    else
    {
        printf(" (hardware controlled)");
    }
    printf("\n\n");
}


/**********************************************************************************************
 *
 * Accessory driver functions
 *
 *********************************************************************************************/
void accy_get_all_devices(int accy_file)
{
    MOTO_ACCY_MASK_T all_devices;
    int device;
    int err;
    
    printf("\nGet all devices\n");
    printf("---------------\n\n");
    
    printf("Getting device status...\n");
    
    /* Get the devices from the driver. */
    err = ioctl(accy_file, MOTO_ACCY_IOCTL_GET_ALL_DEVICES, &all_devices);
     
    if(err != 0)
    {
        printf("   Error %d getting device info.\n", err);
        return;
    }
    
    /* Show what's connected. */
    printf("   Connected device mask: 0x%lX\n", all_devices);
    
    
    /* Show all connected devices. */
    while(all_devices)
    {
        /* Get first device indicated. */
        device = ACCY_BITMASK_FFS(all_devices);
    
        printf("   Device %d connected (%s).\n", device, accy_lookup_table[device]);
        
        ACCY_BITMASK_CLEAR(all_devices, device);
    }
}

/**********************************************************************************************
 *
 * User input stuff.
 *
 *********************************************************************************************/
 
/* This sucks up input from the user and tries to convert it to an integer. It should be 
 * able to cope with hex, binary or decimal input, and strtol() should protect from gibberish
 * in the middle of the input string. This is safer than scanf() as it can't overflow its input 
 * buffer.*/
int get_value_from_user(int * value)
{
    char input[40];
    int length;
    long result = 0;
    char * invalid_char = NULL;
    
    /* Get input from user. */
    fflush(stdin);
    fgets(input, 20, stdin);
    length = strlen(input);
    
    /* Remove the newline character, if present. */
    if((length > 0) && (input[length-1] == '\n'))
    {
        input[length-1] = (char) 0;
        length--;
    }
    
    /* Die here if length is zero. */
    if(length <= 0)
    {
        return -1;
    }
    
    /* Check to see what's up front. Is this hex? */
    if( (length > 2) && (input[0] == '0') && (input[1] == 'x' || (input[1] == 'X')) )
    {
        result = strtol(&(input[2]), &invalid_char, 16);
        
        if(*invalid_char != '\0')
        {
            return -1;
        }
    }
    
    /* Binary? */
    else if( (length > 1) && (input[1] == 'b' || (input[1] == 'B')) )
    {
        result = strtol(&(input[1]), &invalid_char, 2);
        
        if(*invalid_char != '\0')
        {
            printf("Invalid binary value - invalid char %c\n", *invalid_char);
            return -1;
        }    
    }
    
    /* Default to decimal. */
    else
    {   
        result = strtol(input, &invalid_char, 10);

        if(*invalid_char != '\0')
        {
            return -1;
        }            
    }
    
    /* Limit the result. */
    if(result > INT_MAX)
    {
        result = INT_MAX;
    }
    else if(result < INT_MIN)
    {
        result = INT_MIN;
    }
    
    *value = (int)result;
    return 0;
}

/**********************************************************************************************
 *
 * TCMD ioctl functions
 *
 *********************************************************************************************/
void tcmd_set_emu_trans_state(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    POWER_IC_REG_ACCESS_T reg_access1;
    int new_state; 
    int ioctl_result;
    int ioctl_result1;
    int ioctl_result2;

    printf("\nTCMD test %d: Set EMU TRANS State\n", TCMD_MENU_OPTION_SET_EMU_TRANS_STATE);
    printf("--------------------------------\n");
    
    /* Get a new state for the EMU CONN. */
    printf("EMU_XCVR_OFF               - 0.\n");
    printf("EMU_XCVR_PPD_DETECT        - 1.\n");
    printf("EMU_XCVR_SPD_DETECT        - 2.\n");
    printf("EMU_XCVR_PPD               - 3.\n");
    printf("EMU_XCVR_PPD_AUDIO         - 4.\n");
    printf("EMU_XCVR_SPD               - 5.\n");
    printf("EMU_XCVR_SPD_AUDIO         - 6.\n");
    printf("EMU_XCVR_USB_HOST          - 7.\n");
    printf("EMU_XCVR_USB_FACTORY_MODE  - 8.\n");
    printf("Set EMU XCVR state too? (0..%d) > ", 8);
    
    if(get_value_from_user(&new_state) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    else if((new_state < 0) || (new_state > 8))
    {
        printf("    Error: %d is not a valid state for the EMU XCVR.\n", new_state);
    }
    
    /* Write new state to set the EMU XCVR too is. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_EMU_TRANS_STATE, new_state);
    
    if(ioctl_result != 0)
    {
         printf("   Error setting EMU XCVR - ioctl() returned %d. \n", ioctl_result);
    }

    if(ioctl_result == 0)
    {
        reg_access.reg = POWER_IC_REG_EOC_CONN_CONTROL;
        reg_access1.reg = POWER_IC_REG_EOC_POWER_CONTROL_1;
	    
        ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &reg_access);
        ioctl_result2 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG, &reg_access1);
        
        if(ioctl_result1 == 0 && ioctl_result2 == 0)
        {
            printf("Content of _EOC_CONN_CONTROL: 0x%X\n", reg_access.value);
            printf("Content of _EOC_POWER_CONTROL: 0x%X\n", reg_access1.value);
        }
        else
        {
            printf("Error %d reading POWER_IC_REG_EOC_CONN_CONTROL\n", ioctl_result1);
            printf("Error %d reading POWER_IC_REG_EOC_POWER_CONTROL_1\n", ioctl_result2);        
	}
		
    }
    else
    {
        printf("   Error changing EMU CONN State - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_set_emu_conn_state(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int new_state; 
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Set EMU CONN State\n", TCMD_MENU_OPTION_SET_EMU_CONN_STATE);
    printf("--------------------------------\n");
    
    /* Get a new state for the EMU CONN. */
    printf("EMU_CONN_MODE_USB             - 0.\n");
    printf("EMU_CONN_MODE_UART1           - 1.\n");
    printf("EMU_CONN_MODE_UART2           - 2.\n");
    printf("EMU_CONN_MODE_RESEVERED       - 3.\n");
    printf("EMU_CONN_MODE_MONO_AUDIO      - 4.\n");
    printf("EMU_CONN_MODE_STEREO_AUDIO    - 5.\n");
    printf("EMU_CONN_MODE_LOOPBACK_RIGHT  - 6.\n");
    printf("EMU_CONN_MODE_LOOPBACK_LEFT   - 7.\n");
    printf("Set MONO Adder state too? (0..%d) > ", 7);
    if(get_value_from_user(&new_state) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    else if((new_state < 0) || (new_state > 7))
    {
        printf("    Error: %d is not a valid state for the EMU CONN.\n", new_state);
    }
    
    /* Write new state to set the MONO Adder too. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_EMU_CONN_STATE, new_state);
    
    if(ioctl_result != 0)
    {
         printf("   Error setting VUSBIN - ioctl() returned %d. \n", ioctl_result);
    }

    if(ioctl_result == 0)
    {
	reg_access.reg = POWER_IC_REG_EOC_CONN_CONTROL;
        reg_access.num_bits = 3;
        reg_access.index = 14;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == new_state)
            {
                printf("    EMU CONN State was correctly set too: %d.\n", new_state);
            }
            else
            {
                printf("    Error verifying EMU CONN State setting: set %d, read %d.\n", 
                        new_state, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying EMU CONN State setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
        printf("   Error changing EMU CONN State - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_set_vusbin_state(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int new_state; 
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Set VUSBIN State\n", TCMD_MENU_OPTION_SET_VUSBIN_STATE);
    printf("------------------------------\n");
    
    /* Get a states for the VUSBIN. */
    printf("input = VINBUS  - 0.\n");
    printf("input = VBUS    - 1.\n");
    printf("input = BP      - 2.\n");
    printf("input = VBUS    - 3.\n");
    printf("Set VUSBIN state too? (0..%d) > ", 3);
    if(get_value_from_user(&new_state) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    else if((new_state < 0) || (new_state > 3))
    {
        printf("    Error: %d is not a valid state for the VUSBIN.\n", new_state);
    }
    
    /* Write new state to set the VUSBIN too. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_VUSBIN_STATE, new_state);
    
    if(ioctl_result == 0)
    {
	reg_access.reg = POWER_IC_REG_EOC_POWER_CONTROL_1;
        reg_access.num_bits = 2;
        reg_access.index = 0;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == new_state)
            {
                printf("    VUSBIN State was correctly set too: %d.\n", new_state);
            }
            else
            {
                printf("    Error verifying VUSBIN State setting: set %d, read %d.\n", 
                        new_state, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying VUSBIN State setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
       printf("   Error changing VUSBIN State - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_set_mono_adder_state(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int new_state; 
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Set EMU MONO Adder State\n", TCMD_MENU_OPTION_SET_MONO_ADDER_STATE);
    printf("---------------------------------------\n");
    
    /* Get a new state for the MONO Adder. */
    printf("POWER_IC_TCMD_MONO_ADDER_STEREO       - 0.\n");
    printf("POWER_IC_TCMD_MONO_ADDER_R_L          - 1.\n");
    printf("POWER_IC_TCMD_MONO_ADDER_R_L_3DB_LOSS - 2.\n");
    printf("POWER_IC_TCMD_MONO_ADDER_R_L_6DB_LOSS - 3.\n");
    printf("Set MONO Adder state too? (0..%d) > ", 3);
    if(get_value_from_user(&new_state) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Write new state to set the MONO Adder too. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_MONO_ADDER_STATE, new_state);
    
    if(ioctl_result == -EFAULT)
    {
         printf("   MONO ADDER - error requested state out of range.\n");
    }

    if(ioctl_result != 0 && ioctl_result != -EFAULT)
    {
         printf("   Error setting MONO ADDER - ioctl() returned %d. \n", ioctl_result);
    }


    if(ioctl_result == 0)
    {
    
	reg_access.reg = POWER_IC_REG_PCAP_RX_AUD_AMPS;
        reg_access.num_bits = 2;
        reg_access.index = 19;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == new_state)
            {
                printf("    MONO Adder State was correctly set too: %d.\n", new_state);
            }
            else
            {
                printf("    Error verifying MONO Adder State setting: set %d, read %d.\n", 
                        new_state, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying MONO Adder State setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
        printf("   Error changing MONO Adder State - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_get_idfloats(int power_ic_file)
{
    unsigned int ret_value = 0;
    int ioctl_result;

    printf("\nTCMD test %d: Get IDFLOATS bit\n", TCMD_MENU_OPTION_GET_IDFLOATS);
    printf("------------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_IDFLOATS_READ, &ret_value);
    
    if(ioctl_result == 0)
    {
        printf("    The value of the IDFLOATS setting is: %dn",  ret_value);
    }
    else
    {
        printf("   Error reading IDFLOAT - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_get_idgnds(int power_ic_file)
{
    unsigned int ret_value = 0;
    int ioctl_result;

    printf("\nTCMD test %d: Get IDGNDS bit\n", TCMD_MENU_OPTION_GET_IDGNDS);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_IDGNDS_READ, &ret_value);
    
    if(ioctl_result == 0)
    {
        printf("    The value of the IDGNDS setting is: %dn",  ret_value);
    }
    else
    {
        printf("   Error reading IDGNDS - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_get_a1sns(int power_ic_file)
{
    unsigned int ret_value = 0;
    int ioctl_result;

    printf("\nTCMD test %d: Get A1SNS bit\n", TCMD_MENU_OPTION_GET_A1SNS);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_A1SNS_READ, &ret_value);
    
    if(ioctl_result == 0)
    {
        printf("    The value of the A1SNS setting is: %dn",  ret_value);
    }
    else
    {
        printf("   Error reading A1SNS - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_get_mb2sns(int power_ic_file)
{
    unsigned int ret_value = 0;
    int ioctl_result;

    printf("\nTCMD test %d: Get MB2SNS bit\n", TCMD_MENU_OPTION_GET_MB2SNS);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_MB2SNS_READ, &ret_value);
    
    if(ioctl_result == 0)
    {
        printf("    The value of the MB2SNS setting is: %dn",  ret_value);
    }
    else
    {
        printf("   Error reading MB2SNS - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_get_vusbin(int power_ic_file)
{
    unsigned int ret_value = 0;
    int ioctl_result;

    printf("\nTCMD test %d: Get VUSBIN bit\n", TCMD_MENU_OPTION_GET_VUSBIN);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_VUSBIN_READ, &ret_value);
    
    if(ioctl_result == 0)
    {
        printf("    The value of the VUSBIN setting is: %dn",  ret_value);
    }
    else
    {
        printf("   Error reading VUSBIN - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_get_clkstat(int power_ic_file)
{
    unsigned int ret_value = 0;
    int ioctl_result;

    printf("\nTCMD test %d: Get CLKSTAT bit\n", TCMD_MENU_OPTION_GET_CLKSTAT);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_CLKSTAT_READ, &ret_value);
    
    if(ioctl_result == 0)
    {
        printf("    The value of the CLKSTAT setting is: %dn",  ret_value);
    }
    else
    {
        printf("   Error reading CLKSTAT - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_set_reverse_mode_state_on(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int tcmd_on = 1; 
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Turn Reverse Mode State on\n", TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_ON);
    printf("---------------------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_REVERSE_MODE_STATE, tcmd_on);
    
    if(ioctl_result == 0)
    {
    
	reg_access.reg = POWER_IC_REG_PCAP_BATT_DAC;
        reg_access.num_bits = 1;
        reg_access.index = 15;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == tcmd_on)
            {
                printf("    Reverse Mode State is on.\n");
            }
            else
            {
                printf("    Error verifying Reverse Mode State setting: set %d, read %d.\n", 
                        tcmd_on, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying Reverse Mode State setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
        printf("   Error turning Reverse Mode State on - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_set_reverse_mode_state_off(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int tcmd_off = 0;
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Turn Reverse Mode State off\n",
    TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_OFF);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_REVERSE_MODE_STATE, tcmd_off);
    
    if(ioctl_result == 0)
    {
    
	reg_access.reg = POWER_IC_REG_PCAP_BATT_DAC;
        reg_access.num_bits = 1;
        reg_access.index = 15;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == tcmd_off)
            {
                printf("    Reverse Mode State is off.\n");
            }
            else
            {
                printf("    Error verifying Reverse Mode State setting: set %d, read %d.\n", 
                        tcmd_off, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying Reverse Mode State setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
        printf("   Error turning VUSBIN off - ioctl() returned %d. \n", ioctl_result);
    }
}
void tcmd_set_coinchen_on(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int tcmd_on = 1; 
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Turn COINCHEN on\n", TCMD_MENU_OPTION_SET_COINCHEN_ON);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_COINCHEN_STATE, tcmd_on);
    
    if(ioctl_result == 0)
    {
    
	reg_access.reg = POWER_IC_REG_PCAP_BATT_DAC;
        reg_access.num_bits = 1;
        reg_access.index = 15;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == tcmd_on)
            {
                printf("    COINCHEN is on.\n");
            }
            else
            {
                printf("    Error verifying COINCHEN setting: set %d, read %d.\n", 
                        tcmd_on, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying COINCHEN setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
        printf("   Error turning COINCHEN on - ioctl() returned %d. \n", ioctl_result);
    }
}

void tcmd_set_coinchen_off(int power_ic_file)
{
    POWER_IC_REG_ACCESS_T reg_access;
    int tcmd_off = 0; 
    int ioctl_result;
    int ioctl_result1;

    printf("\nTCMD test %d: Turn COINCHEN off\n", TCMD_MENU_OPTION_SET_COINCHEN_OFF);
    printf("----------------------------\n");
    
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_CMD_TCMD_COINCHEN_STATE, tcmd_off);
    
    if(ioctl_result == 0)
    {
    
	reg_access.reg = POWER_IC_REG_PCAP_BATT_DAC;
        reg_access.num_bits = 1;
        reg_access.index = 15;
        reg_access.value = 0xFFFFFFFF;
	
	ioctl_result1 = ioctl(power_ic_file, POWER_IC_IOCTL_READ_REG_BITS, &reg_access);
		
        if(ioctl_result1 == 0)
        {
            if(reg_access.value == tcmd_off)
            {
                printf("    COINCHEN is off.\n");
            }
            else
            {
                printf("    Error verifying COINCHEN setting: set %d, read %d.\n", 
                        tcmd_off, reg_access.value);
            }
        }
        else
        {
            printf("   Error verifying COINCHEN setting - ioctl() returned %d. \n", ioctl_result1);
        }
    
    }
    else
    {
        printf("   Error turning COINCHEN off - ioctl() returned %d. \n", ioctl_result);
    }
}

/**********************************************************************************************
 *
 * Timing ioctl functions
 *
 *********************************************************************************************/
void timing_show_info(int power_ic_file)
{
    int show_info;
    int ioctl_result;
    
    printf("\nShow timing info\n");
    printf("----------------\n\n");
    
    printf("Show all timing info? (1 = yes, 0 = no) > ");
    
    /* Get the regulator that's to be altered. */
    if(get_value_from_user(&show_info) != 0)
    {
        printf("    Error: not a valid input.\n");
        return;
    }
    
    /* Is this a valid sw regulator? */
    if((show_info < 0) || (show_info > 1))
    {
        printf("    %d is not a valid choice for show info.\n", show_info);
        return;
    }

    /* Get kernel to dump tining information on console. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_TIMING_SHOW_INFO, (bool)show_info);
    
    if(ioctl_result != 0)
    {
         printf("   This kernel does not support timing information.\n");
    }
}

void timing_reset_info(int power_ic_file)
{
    int ioctl_result;
    
    printf("\nReset timing info\n");
    printf("-----------------\n\n");
    
    printf("   Resetting timing info...");

    /* Get kernel to reset all stored timing information. */
    ioctl_result = ioctl(power_ic_file, POWER_IC_IOCTL_TIMING_RESET, NULL);
    
    if(ioctl_result != 0)
    {
         printf("Error: This kernel does not support timing information.\n");
    }
    else
    {
         printf("done.\n");
    }    
}

/**********************************************************************************************
 *
 * Functions for submenus.
 *
 *********************************************************************************************/
void display_reg_access_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC registers.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Read all power IC registers\n", REG_ACCESS_MENU_OPTION_READ_ALL_REGS);
    printf("       %2d: Read a single power IC register\n", REG_ACCESS_MENU_OPTION_READ_ALL_REGS);
    printf("       %2d: Read register value\n", REG_ACCESS_MENU_OPTION_READ_REG_VALUE);        
    printf("       %2d: Write data to a register\n", REG_ACCESS_MENU_OPTION_WRITE_REG);  
    printf("       %2d: Write register mask\n", REG_ACCESS_MENU_OPTION_WRITE_REG_VALUE);
    printf("       %2d: Check EMU sense bit\n", REG_ACCESS_MENU_OPTION_CHECK_EMU_SENSE_BIT);
    printf("\n");
    printf("       %2d: Get hardware info\n", REG_ACCESS_MENU_OPTION_GET_HW);      
    printf("\n");
    printf("       %2d: Back\n", REG_ACCESS_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}


void reg_access_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_reg_access_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case REG_ACCESS_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case REG_ACCESS_MENU_OPTION_READ_ALL_REGS:
                reg_access_read_all_regs(power_ic_file);
                break;
                
            case REG_ACCESS_MENU_OPTION_READ_SINGLE_REG:
                reg_access_read_single_reg(power_ic_file);
                break;
                
            case REG_ACCESS_MENU_OPTION_READ_REG_VALUE:
                reg_access_read_reg_value(power_ic_file);                
                break;
                
            case REG_ACCESS_MENU_OPTION_WRITE_REG:
                reg_access_write_reg(power_ic_file);
                break;                
                
            case REG_ACCESS_MENU_OPTION_WRITE_REG_VALUE:
                reg_access_write_reg_value(power_ic_file);
                break;
                
            case REG_ACCESS_MENU_OPTION_CHECK_EMU_SENSE_BIT:
                reg_access_check_emu_sense_bit(power_ic_file);
                break;
                
            case REG_ACCESS_MENU_OPTION_GET_HW:
                reg_access_get_hw(power_ic_file);
                break;

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}



void display_periph_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Peripheral control.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Get peripheral status\n", PERIPH_MENU_OPTION_GET_STATUS); 
    printf("       %2d: Turn vibrator on\n", PERIPH_MENU_OPTION_VIBRATOR_ON);   
    printf("       %2d: Turn vibrator off\n", PERIPH_MENU_OPTION_VIBRATOR_OFF);
    printf("       %2d: Set vibrator level\n", PERIPH_MENU_OPTION_VIBRATOR_SET_LEVEL);       
    printf("       %2d: Turn main backlight on\n", PERIPH_MENU_OPTION_MAIN_BACKLIGHT_ON);   
    printf("       %2d: Turn main backlight off\n", PERIPH_MENU_OPTION_MAIN_BACKLIGHT_OFF);
    printf("       %2d: Turn aux backlight on\n", PERIPH_MENU_OPTION_AUX_BACKLIGHT_ON);   
    printf("       %2d: Turn aux backlight off\n", PERIPH_MENU_OPTION_AUX_BACKLIGHT_OFF); 
    printf("       %2d: Turn aux backlight off\n", PERIPH_MENU_OPTION_AUX_BACKLIGHT_OFF);
    printf("       %2d: Turn keypad backlight on\n", PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_ON);
    printf("       %2d: Turn keypad backlight off\n", PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_OFF);
    printf("       %2d: Turn Sign Of Life LED on\n", PERIPH_MENU_OPTION_SOL_LED_ON);
    printf("       %2d: Turn Sign Of Life LED off\n", PERIPH_MENU_OPTION_SOL_LED_OFF);
    printf("       %2d: Turn Wifi Status LED on\n", PERIPH_MENU_OPTION_WIFI_STATUS_LED_ON);
    printf("       %2d: Turn Wifi Status LED off\n", PERIPH_MENU_OPTION_WIFI_STATUS_LED_OFF);      
    printf("       %2d: Turn WLAN on\n", PERIPH_MENU_OPTION_WLAN_ON);   
    printf("       %2d: Turn WLAN off\n", PERIPH_MENU_OPTION_WLAN_OFF);
    printf("       %2d: Turn WLAN low power state on\n", PERIPH_MENU_OPTION_WLAN_LOW_POWER_STATE_ON);
    printf("\n");
    printf("       %2d: Back\n", PERIPH_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}


void periph_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_periph_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case PERIPH_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case PERIPH_MENU_OPTION_GET_STATUS:
                periph_get_status(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_VIBRATOR_ON:
                periph_vibrator_on(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_VIBRATOR_OFF:
                periph_vibrator_off(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_VIBRATOR_SET_LEVEL:
                periph_vibrator_set_level(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_MAIN_BACKLIGHT_ON:
                periph_main_backlight_on(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_MAIN_BACKLIGHT_OFF:
                periph_main_backlight_off(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_AUX_BACKLIGHT_ON:
                periph_aux_backlight_on(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_AUX_BACKLIGHT_OFF:
                periph_aux_backlight_off(power_ic_file);
                break;    

            case PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_ON:
                periph_keypad_backlight_on(power_ic_file);
                break; 

            case PERIPH_MENU_OPTION_KEYPAD_BACKLIGHT_OFF:
                periph_keypad_backlight_off(power_ic_file);
                break; 

            case PERIPH_MENU_OPTION_SOL_LED_ON:
                periph_sol_led_on(power_ic_file);
                break; 

	          case PERIPH_MENU_OPTION_SOL_LED_OFF:
                periph_sol_led_off(power_ic_file);
                break; 

	          case PERIPH_MENU_OPTION_WIFI_STATUS_LED_ON:
                periph_wifi_status_led_on(power_ic_file);
                break; 

   	        case PERIPH_MENU_OPTION_WIFI_STATUS_LED_OFF:
                periph_wifi_status_led_off(power_ic_file);
                break;
                      
            case PERIPH_MENU_OPTION_WLAN_ON:
                periph_wlan_on(power_ic_file);
                break;
                
            case PERIPH_MENU_OPTION_WLAN_OFF:
                periph_wlan_off(power_ic_file);
                break;
		
            case PERIPH_MENU_OPTION_WLAN_LOW_POWER_STATE_ON:
                periph_wlan_low_power_state_on(power_ic_file);
                break;
 		         
            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_vreg_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC voltage regulators.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Show switchmode regulator status\n", VREG_MENU_OPTION_GET_SWITCHMODE_STATUS); 
    printf("       %2d: Show regulator bank 1 status\n", VREG_MENU_OPTION_GET_VREG1_STATUS); 
    printf("       %2d: Show regulator bank 2 status\n", VREG_MENU_OPTION_GET_VREG2_STATUS); 
    printf("       %2d: Show aux regulator status\n", VREG_MENU_OPTION_GET_AUX_STATUS);
    printf("\n");    
    printf("       %2d: Set switchmode regulator\n", VREG_MENU_OPTION_SET_SWITCHMODE_REG); 
    printf("       %2d: Set voltage regulator (bank 1 or 2)\n", VREG_MENU_OPTION_SET_V_REG); 
    printf("       %2d: Set Aux regulator\n", VREG_MENU_OPTION_SET_AUX_REG);
    printf("       %2d: Set core voltage\n", VREG_MENU_OPTION_SET_CORE_VOLTAGE);                  
    printf("\n");
    printf("       %2d: Back\n", VREG_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}


void vreg_access_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_vreg_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case REG_ACCESS_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case VREG_MENU_OPTION_GET_SWITCHMODE_STATUS:
                sw_reg_get_status(power_ic_file);
                break;
                
            case VREG_MENU_OPTION_GET_VREG1_STATUS:
                vreg1_get_status(power_ic_file);
                break;
                
            case VREG_MENU_OPTION_GET_VREG2_STATUS:
                vreg2_get_status(power_ic_file);
                break;
                
            case VREG_MENU_OPTION_GET_AUX_STATUS:
                aux_reg_get_status(power_ic_file);
                break;
                
            case VREG_MENU_OPTION_SET_SWITCHMODE_REG:
                regulator_set_sw_reg(power_ic_file);
                break;
                
            case VREG_MENU_OPTION_SET_V_REG:
                regulator_set_vreg(power_ic_file);
                break;
                
            case VREG_MENU_OPTION_SET_AUX_REG:
                regulator_set_aux_reg(power_ic_file);
                break;                                              
                
            case VREG_MENU_OPTION_SET_CORE_VOLTAGE:
                set_core_voltage(power_ic_file);
                break;  
                

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_rtc_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC real-time clock.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Get RTC status\n", RTC_MENU_OPTION_GET_STATUS);   
    printf("       %2d: Set RTC times\n", RTC_MENU_OPTION_SET_TIME);   
    printf("\n");
    printf("       %2d: Back\n", RTC_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}

void rtc_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_rtc_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case RTC_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case RTC_MENU_OPTION_GET_STATUS:
                rtc_get_status(power_ic_file);
                break;

            case RTC_MENU_OPTION_SET_TIME:
                rtc_set_times(power_ic_file);
                break;

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_audio_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC audio menu.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");  
    printf("\n");
    printf("       %2d: Set Handset Path\n",AUDIO_MENU_OPTION_SET_OUT_PATH );   
    printf("       %2d: Set audio in path\n",AUDIO_MENU_OPTION_SET_IN_PATH);
    printf("       %2d: Set audio codec on\n",  AUDIO_MENU_OPTION_SET_CODEC_ON);
    printf("       %2d: Set audio out gain\n", AUDIO_MENU_OPTION_SET_OUT_GAIN);
    printf("\n");
    printf("       %2d: Back\n", AUDIO_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}

void audio_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_audio_menu();
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case AUDIO_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case AUDIO_MENU_OPTION_SET_OUT_PATH:
                audio_set_output_path(power_ic_file);
                break;
            case AUDIO_MENU_OPTION_SET_IN_PATH:
                audio_set_input_path(power_ic_file);
                break;
            
            case AUDIO_MENU_OPTION_SET_CODEC_ON:
                audio_set_codec_on(power_ic_file);
                break;
            case AUDIO_MENU_OPTION_SET_OUT_GAIN:
                audio_set_out_gain(power_ic_file);
                break;
            default:
                printf("Unknown option...\n");
                break;
        }
    }
}
    
void display_atod_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC AtoD converter interface.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Convert single channel\n",                   ATOD_MENU_OPTION_SINGLE_CHANNEL);
    printf("       %2d: Convert general channels\n",                 ATOD_MENU_OPTION_GENERAL);
    printf("       %2d: Convert battery and current mA (immediate)\n",  ATOD_MENU_OPTION_BATT_AND_CURR_IMMEDIATE);
    printf("       %2d: Convert battery and current DACs (immediate)\n",  ATOD_MENU_OPTION_BATT_AND_CURR_PHASED_IMMEDIATE);
    printf("       %2d: Convert battery and current (in-burst, with timeout)\n",  ATOD_MENU_OPTION_BATT_AND_CURR_DELAYED);    
    printf("       %2d: Get raw results from single AtoD channel\n", ATOD_MENU_OPTION_RAW);    
    printf("       %2d: Start nonblocking conversion\n",             ATOD_MENU_OPTION_NONBLOCK_START);
    printf("       %2d: Poll nonblocking conversion\n",              ATOD_MENU_OPTION_NONBLOCK_POLL);    
    printf("       %2d: Cancel nonblocking conversion\n",            ATOD_MENU_OPTION_NONBLOCK_CANCEL);        
    printf("\n");
    printf("       %2d: Set phasing slope/offset\n",                 ATOD_MENU_OPTION_PHASING_SET);
    printf("       %2d: Restore default phasing\n",                  ATOD_MENU_OPTION_PHASING_SET_DEFAULT);
    printf("       %2d: Attempt to set invalid phasing\n",           ATOD_MENU_OPTION_PHASING_SET_INVALID);
    printf("\n");
    printf("       %2d: Back\n", ATOD_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}

void atod_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_atod_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case ATOD_MENU_OPTION_QUIT:
                running = 0;
                break;
        
            case ATOD_MENU_OPTION_SINGLE_CHANNEL:
                atod_convert_single_channel(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_GENERAL:
                atod_convert_general_channels(power_ic_file);
                break;

            case ATOD_MENU_OPTION_BATT_AND_CURR_IMMEDIATE:
                atod_convert_batt_and_curr_immediate(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_BATT_AND_CURR_PHASED_IMMEDIATE:
                atod_convert_batt_and_curr_phased_immediate(power_ic_file);
                break;

            case ATOD_MENU_OPTION_BATT_AND_CURR_DELAYED:
                atod_convert_batt_and_curr_delayed(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_RAW:    
                atod_convert_raw(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_NONBLOCK_START:
                atod_convert_nonblock_start(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_NONBLOCK_POLL:
                atod_convert_nonblock_poll(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_NONBLOCK_CANCEL:    
                atod_convert_nonblock_cancel(power_ic_file);
                break;
                
            case ATOD_MENU_OPTION_PHASING_SET:
                atod_phasing_set(power_ic_file);
                break;
            
            case ATOD_MENU_OPTION_PHASING_SET_DEFAULT:
                atod_phasing_set_default(power_ic_file);
                break;
            
            case ATOD_MENU_OPTION_PHASING_SET_INVALID:
                atod_phasing_set_invalid(power_ic_file);
                break;   

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_charger_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC Charger Control.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Set charge voltage\n", CHARGER_MENU_OPTION_CHARGE_VOLTAGE);
    printf("       %2d: Set charge current\n", CHARGER_MENU_OPTION_CHARGE_CURRENT);  
    printf("       %2d: Set trickle current\n", CHARGER_MENU_OPTION_TRICKLE_CURRENT);
    printf("       %2d: Set power path\n", CHARGER_MENU_OPTION_POWER_PATH);
    printf("       %2d: Get overvotlage state\n", CHARGER_MENU_OPTION_GET_OVERVOLTAGE);
    printf("       %2d: Reset overvoltage\n", CHARGER_MENU_OPTION_RESET_OVERVOLTAGE);       
    printf("\n");
    printf("       %2d: Get charger setup\n", CHARGER_MENU_OPTION_GET_SETUP);       
    printf("\n");    
    printf("       %2d: Back\n", REG_ACCESS_MENU_OPTION_QUIT);
    printf("\n");
    printf("     > ");   

}


void charger_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_charger_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
            case CHARGER_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case CHARGER_MENU_OPTION_CHARGE_VOLTAGE:
                charger_set_charge_voltage(power_ic_file);
                break;
                
            case CHARGER_MENU_OPTION_CHARGE_CURRENT:
                charger_set_charge_current(power_ic_file);
                break;                
                
            case CHARGER_MENU_OPTION_TRICKLE_CURRENT:
                charger_set_trickle_current(power_ic_file);
                break;
                
            case CHARGER_MENU_OPTION_POWER_PATH:
                charger_set_power_path(power_ic_file);
                break;   
                
            case CHARGER_MENU_OPTION_GET_OVERVOLTAGE:
                charger_get_overvoltage(power_ic_file);
                break; 
                  
            case CHARGER_MENU_OPTION_RESET_OVERVOLTAGE:
                charger_reset_overvoltage(power_ic_file);
                break;            
                
            case CHARGER_MENU_OPTION_GET_SETUP:     
                charger_get_setup(power_ic_file);
                break;

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_tcmd_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("TCMD ioctl control.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Set EMU transition state\n", TCMD_MENU_OPTION_SET_EMU_TRANS_STATE); 
    printf("       %2d: Set Mono Adder State\n", TCMD_MENU_OPTION_SET_MONO_ADDER_STATE);   
    printf("       %2d: Get IDFLOATS bit setting\n", TCMD_MENU_OPTION_GET_IDFLOATS);  
    printf("       %2d: Get IDGNDS bit setting\n", TCMD_MENU_OPTION_GET_IDGNDS);
    printf("       %2d: Get A1SNS bit setting\n", TCMD_MENU_OPTION_GET_A1SNS);       
    printf("       %2d: Get MB2SNS bit setting\n", TCMD_MENU_OPTION_GET_MB2SNS);   
    printf("       %2d: Set reverse mode on\n", TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_ON);
    printf("       %2d: Set reverse mode off\n", TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_OFF);
    printf("       %2d: Get VUSBIN bit settings\n", TCMD_MENU_OPTION_GET_VUSBIN);   
    printf("       %2d: Set VUSBIN state\n", TCMD_MENU_OPTION_SET_VUSBIN_STATE);
    printf("       %2d: Get CLKSTAT bit setting\n", TCMD_MENU_OPTION_GET_CLKSTAT);
    printf("       %2d: Set COINCHEN on\n", TCMD_MENU_OPTION_SET_COINCHEN_ON);
    printf("       %2d: Set COINCHEN off\n", TCMD_MENU_OPTION_SET_COINCHEN_OFF);
    printf("       %2d: Set EMU CONN state\n", TCMD_MENU_OPTION_SET_EMU_CONN_STATE); 
    printf("\n");
    printf("       %2d: Back\n", TCMD_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}

  
void tcmd_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_tcmd_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case TCMD_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case TCMD_MENU_OPTION_SET_EMU_TRANS_STATE:
                tcmd_set_emu_trans_state(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_SET_MONO_ADDER_STATE:
                tcmd_set_mono_adder_state(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_GET_IDFLOATS:
                tcmd_get_idfloats(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_GET_IDGNDS:
                tcmd_get_idgnds(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_GET_A1SNS:
                tcmd_get_a1sns(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_GET_MB2SNS:
                tcmd_get_mb2sns(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_ON:
                tcmd_set_reverse_mode_state_on(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_SET_REVERSE_MODE_STATE_OFF:
                tcmd_set_reverse_mode_state_off(power_ic_file);
                break;    

            case TCMD_MENU_OPTION_GET_VUSBIN:
                tcmd_get_vusbin(power_ic_file);
                break;
                       
            case TCMD_MENU_OPTION_SET_VUSBIN_STATE:
                tcmd_set_vusbin_state(power_ic_file);
                break;
		
             case TCMD_MENU_OPTION_GET_CLKSTAT:
                tcmd_get_clkstat(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_SET_COINCHEN_ON:
                tcmd_set_coinchen_on(power_ic_file);
                break;
                
            case TCMD_MENU_OPTION_SET_COINCHEN_OFF:
                tcmd_set_coinchen_off(power_ic_file);
                break;    

            case TCMD_MENU_OPTION_SET_EMU_CONN_STATE:
                tcmd_set_emu_conn_state(power_ic_file);
                break;
                
            default:
                printf("Unknown option...\n");
                break;
        }
    }
}
void display_touchscreen_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Power IC Touchscreen Control.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Enable touchscreen\n", TOUCHSCREEN_MENU_OPTION_ENABLE);
    printf("       %2d: Disable touchscreen\n", TOUCHSCREEN_MENU_OPTION_DISABLE);  
    printf("       %2d: Set touchscreen poll time\n", TOUCHSCREEN_MENU_OPTION_SET_POLL_TIME);
    printf("\n");
    printf("       %2d: Back\n", REG_ACCESS_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}

void touchscreen_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_touchscreen_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
            case TOUCHSCREEN_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case TOUCHSCREEN_MENU_OPTION_ENABLE:
                touchscreen_enable(power_ic_file);
                break;
                
            case TOUCHSCREEN_MENU_OPTION_DISABLE:
                touchscreen_disable(power_ic_file);
                break;                
                
            case TOUCHSCREEN_MENU_OPTION_SET_POLL_TIME:
                touchscreen_set_poll_time(power_ic_file);
                break;
                
            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_accy_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Accessory interface /dev/"MOTO_ACCY_DRIVER_DEV_NAME"\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Get all devices\n", ACCY_MENU_GET_ALL_DEVICES);    
    printf("\n");    
    printf("       %2d: Back\n", REG_ACCESS_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}


void accy_menu(int accy_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_accy_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
            case ACCY_MENU_QUIT:
                running = 0;
                break;
                
            case ACCY_MENU_GET_ALL_DEVICES:
                accy_get_all_devices(accy_file);
                break;

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}

void display_timing_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Timing info\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: Show timing info\n", TIMING_MENU_OPTION_SHOW_INFO);
    printf("       %2d: Reset timing info\n", TIMING_MENU_OPTION_RESET_INFO);    
    printf("\n");    
    printf("       %2d: Back\n", TIMING_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}


void timing_menu(int power_ic_file)
{
    int option_picked;
    int running = 1;

    while(running)
    {
        display_timing_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
            case TIMING_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case TIMING_MENU_OPTION_SHOW_INFO:
                timing_show_info(power_ic_file);
                break;

            case TIMING_MENU_OPTION_RESET_INFO:
                timing_reset_info(power_ic_file);
                break;

            default:
                printf("Unknown option...\n");
                break;
        }
    }
}


void display_main_menu(void)
{
    printf("\n");
    printf("\n");   
    printf("\n");
    printf("Test power IC driver.\n");
    printf("\n");
    printf("\n");
    printf("   Choose an option:\n");
    printf("\n");
    printf("       %2d: General register access\n",   MAIN_MENU_OPTION_REG_ACCESS);
    printf("       %2d: Peripheral control\n",        MAIN_MENU_OPTION_PERIPH);
    printf("       %2d: Voltage regulator control\n", MAIN_MENU_OPTION_VREG);
    printf("       %2d: Real-time clock access\n",    MAIN_MENU_OPTION_RTC);
    printf("       %2d: Audio access\n",              MAIN_MENU_OPTION_AUDIO);
    printf("\n");    
    printf("       %2d: AtoD interface\n",             MAIN_MENU_OPTION_ATOD);  
    printf("       %2d: Charger control interface\n",  MAIN_MENU_OPTION_CHARGER);
    printf("       %2d: TCMD ioctls\n",                MAIN_MENU_OPTION_TCMD);
    printf("       %2d: Accessory driver interface\n", MAIN_MENU_OPTION_ACCY);
    printf("       %2d: Touchscreen Interface\n",      MAIN_MENU_OPTION_TOUCHSCREEN);
    printf("\n");     
    printf("       %2d: Timing info\n", MAIN_MENU_OPTION_TIMING);       
    printf("\n");
    printf("\n");
    printf("       %2d: Quit\n", MAIN_MENU_OPTION_QUIT);
    printf("\n");
    printf("\n");   
    printf("     > ");
}


int main(void)
{
    int power_ic_file;
    int accy_file;
    int option_picked;
    int running = 1;

    printf("Opening power IC device in /dev...");
    
    power_ic_file = open("/dev/" POWER_IC_DEV_NAME, O_RDWR);
    
    if(power_ic_file < 0)
    {
        printf("unable to open /dev/"POWER_IC_DEV_NAME". Exiting.\n");
        return -1;
    }
    else
    {
        printf("opened.\n\n");
    }
    
    printf("Opening accessory device in /dev...");
    
    accy_file = open("/dev/"MOTO_ACCY_DRIVER_DEV_NAME, O_RDWR);
    
    if(accy_file < 0)
    {
        printf("unable to open /dev/"MOTO_ACCY_DRIVER_DEV_NAME". Exiting.\n");
        return -1;
    }
    else
    {
        printf("opened.\n\n");
    }    
    
    while(running)
    {
        display_main_menu();
        
        if (get_value_from_user(&option_picked) != 0)
        {
            option_picked = -1;
        }
        
        switch(option_picked)
        {
        
            case MAIN_MENU_OPTION_QUIT:
                running = 0;
                break;
                
            case MAIN_MENU_OPTION_REG_ACCESS:
                reg_access_menu(power_ic_file);
                break;
                
            case MAIN_MENU_OPTION_PERIPH:
                periph_menu(power_ic_file);
                break;
                
            case MAIN_MENU_OPTION_VREG:
                vreg_access_menu(power_ic_file);
                break;
                      
            case MAIN_MENU_OPTION_RTC:
                rtc_menu(power_ic_file);
                break;
                
            case  MAIN_MENU_OPTION_AUDIO:
                audio_menu(power_ic_file);
                break;
                
            case MAIN_MENU_OPTION_ATOD:
                atod_menu(power_ic_file);
                break; 
                
            case MAIN_MENU_OPTION_CHARGER:
                charger_menu(power_ic_file);
                break;
		
            case MAIN_MENU_OPTION_TCMD:
                tcmd_menu(power_ic_file);
                break;
                
            case MAIN_MENU_OPTION_ACCY:
                accy_menu(accy_file);
                break;
            
            case MAIN_MENU_OPTION_TOUCHSCREEN:
                touchscreen_menu(power_ic_file);
                break;  
                
             case MAIN_MENU_OPTION_TIMING:
                timing_menu(power_ic_file);
                break;       
                
            default:
                printf("Unknown option...\n");
                break;
        }
    }
    
    /****************************************************************
     * End of tests.
     ***************************************************************/
    close(power_ic_file);

    return 0;
}
