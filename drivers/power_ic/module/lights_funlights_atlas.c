/*
 * Copyright (C) 2005-2006 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Motorola 2006-May-22 - Add LED enable control
 * Motorola 2006-May-15 - Fix typecasting warning.
 * Motorola 2006-Apr-12 - Added new interface functions to control backlight brightness.
 * Motorola 2006-Apr-07 - Add new region for EL NAV Keypad
 * Motorola 2006-Apr-07 - Change camera torch GPIO for Ascension P2
 * Motorola 2006-Apr-04 - Move all GPIO functionality to gpio.c
 * Motorola 2006-Jan-12 - Add in Main Display, EL Keypad, and Camera functionality 
 *                        for Ascension
 * Motorola 2005-Nov-15 - Rewrote the software for ATLAS.
 * Motorola 2005-Jun-16 - Updated the region table for new hardware.
 */

/*!
 * @file lights_funlights_atlas.c
 *
 * @ingroup poweric_lights
 *
 * @brief Funlight module
 *
 *  In this file, there are interface functions between the funlights driver and outside world.
 *  These functions implement a cached priority based method of controlling fun lights regions.
 *  Upon powerup the regions are assigned to the default app, which can be looked at as the old functionality.
 *  This allows the keypad and display backlights to operate as they do today, when they are not in use by fun
 *  lights or KJAVA.  If a new application needs to be added it must be done in the corresponding
 *  header file.

 */
#include <stdbool.h>
#include <linux/kernel.h>
#include <linux/power_ic.h>
#include <linux/lights_funlights.h>
#include <linux/lights_backlight.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include "emu.h"
#include "../core/gpio.h"
#include "../core/os_independent.h"

/*******************************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************************/
static bool lights_fl_region_gpio(const LIGHTS_FL_LED_CTL_T *pCtlData, LIGHTS_FL_COLOR_T nStep);
static bool lights_fl_region_cli_display(const LIGHTS_FL_LED_CTL_T *pCtlData, LIGHTS_FL_COLOR_T nStep);
static bool lights_fl_region_tri_color(const LIGHTS_FL_LED_CTL_T *pCtlData, LIGHTS_FL_COLOR_T nColor);
static bool lights_fl_region_sol_led(const LIGHTS_FL_LED_CTL_T *pCtlData, LIGHTS_FL_COLOR_T nColor); 
#ifdef CONFIG_MACH_ASCENSION
static bool lights_fl_region_main_display(const LIGHTS_FL_LED_CTL_T *pCtlData, LIGHTS_FL_COLOR_T nStep);
#else
static bool lights_fl_region_keypad(const LIGHTS_FL_LED_CTL_T *pCtlData, LIGHTS_FL_COLOR_T nStep);
#endif

/*******************************************************************************************
 * LOCAL CONSTANTS
 ******************************************************************************************/
/*!
 * @brief Define the number of backlight steps which are supported for the different
 * backlights.  This value includes off, so the minimum value is 2, 1 for on 1 for off.
 *
 * @{
 */
#ifdef CONFIG_MACH_ASCENSION
# define LIGHTS_NUM_KEYPAD_STEPS      2
# define LIGHTS_NUM_DISPLAY_STEPS     7
# define LIGHTS_NUM_CLI_DISPLAY_STEPS 2
# define LIGHTS_NUM_NAV_KEYPAD_STEPS  2
#else
# define LIGHTS_NUM_KEYPAD_STEPS      2
# define LIGHTS_NUM_DISPLAY_STEPS     2
# define LIGHTS_NUM_CLI_DISPLAY_STEPS 2
# define LIGHTS_NUM_NAV_KEYPAD_STEPS  2
#endif
/* @} */

/* ATLAS Registers */
/* Register LED Control 0 */
#define LIGHTS_FL_CHRG_LED_EN                18
#define LIGHTS_FL_LEDEN                      0
/* Register LED Control 2 */
#define LIGHTS_FL_MAIN_DISP_DC_MASK          0x001E00
#define LIGHTS_FL_MAIN_DISP_CUR_MASK         0x000007
#define LIGHTS_FL_CLI_DISP_DC_MASK           0x00E038
#define LIGHTS_FL_KEYPAD_DC_MASK             0x1E01C0

/* Register LED Control 3, 4, and 5 */
#define LIGHTS_FL_TRI_COLOR_RED_DC_INDEX     6
#define LIGHTS_FL_TRI_COLOR_RED_DC_MASK      0x0007C0
#define LIGHTS_FL_TRI_COLOR_GREEN_DC_INDEX   11
#define LIGHTS_FL_TRI_COLOR_GREEN_DC_MASK    0x00F800
#define LIGHTS_FL_TRI_COLOR_BLUE_DC_INDEX    16
#define LIGHTS_FL_TRI_COLOR_BLUE_DC_MASK     0x1F0000

#define LIGHTS_FL_TRI_COLOR_DC_MASK          0x1FFFC0

#define LIGHTS_FL_DUTY_CYCLE_SHIFT           3

/*******************************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************************/
/*
 * Table to determine the number of brightness steps supported by hardware.  See
 * lights_backlights.h for more details.
 */
const uint8_t lights_bl_num_steps_tb[LIGHTS_BACKLIGHT_ALL] =
{
    LIGHTS_NUM_KEYPAD_STEPS,      /* LIGHTS_BACKLIGHT_KEYPAD      */
    LIGHTS_NUM_DISPLAY_STEPS,     /* LIGHTS_BACKLIGHT_DISPLAY     */
    LIGHTS_NUM_CLI_DISPLAY_STEPS, /* LIGHTS_BACKLIGHT_CLI_DISPLAY */
    LIGHTS_NUM_NAV_KEYPAD_STEPS   /* LIGHTS_BACKLIGHT_NAV         */
};

/*
 * Table to determine the percentage step size for backlight brightness. See lights_backlights.h
 * for more details.
 */
const uint8_t lights_bl_percentage_steps[LIGHTS_BACKLIGHT_ALL] =
{
    100/(LIGHTS_NUM_KEYPAD_STEPS-1),      /* LIGHTS_BACKLIGHT_KEYPAD      */
    100/(LIGHTS_NUM_DISPLAY_STEPS-1),     /* LIGHTS_BACKLIGHT_DISPLAY     */
    100/(LIGHTS_NUM_CLI_DISPLAY_STEPS-1), /* LIGHTS_BACKLIGHT_CLI_DISPLAY */
    100/(LIGHTS_NUM_NAV_KEYPAD_STEPS-1)   /* LIGHTS_BACKLIGHT_NAV         */
};

/*
 * Table of control functions for region LED's.  See lights_funlights.h for more details.
 */
const LIGHTS_FL_REGION_CFG_T LIGHTS_FL_region_ctl_tb[LIGHTS_FL_MAX_REGIONS] =
{
#ifdef CONFIG_MACH_ASCENSION
    {(void*)lights_fl_region_main_display,      {0, NULL}}, /* Display Backlight */
    {(void*)lights_fl_region_gpio,              {0, power_ic_gpio_lights_set_keypad_base}}, /* Keypad Backlight */
    {(void*)lights_fl_region_tri_color,         {1, NULL}}, /* Tri Color LED #1 */
    {(void*)lights_fl_region_tri_color,         {2, NULL}}, /* Tri Color LED #2 */
    {(void*)lights_fl_region_cli_display,       {0, NULL}}, /* CLI Display Backlight */
    {(void*)lights_fl_region_gpio,              {0, power_ic_gpio_lights_set_camera_flash}}, /* Camera Flash */
    {(void*)NULL,                               {0, NULL}}, /* WiFi Status LED */
    {(void*)lights_fl_region_sol_led,           {0, NULL}}, /* SOL */ 
    {(void*)lights_fl_region_tri_color,         {1, NULL}}, /* Bluetooth Status LED */
    {(void*)lights_fl_region_gpio,              {0, power_ic_gpio_lights_set_keypad_slider}}  /* Navigation Keypad Backlight */
#else
    {(void*)lights_fl_region_gpio,              {0, power_ic_gpio_lights_set_main_display}}, /* Display Backlight */
    {(void*)lights_fl_region_keypad,            {0, NULL}}, /* Keypad Backlight */
    {(void*)lights_fl_region_tri_color,         {1, NULL}}, /* Tri Color LED #1 */
    {(void*)lights_fl_region_tri_color,         {2, NULL}}, /* Tri Color LED #2 */
    {(void*)lights_fl_region_cli_display,       {0, NULL}}, /* CLI Display Backlight */
    {(void*)NULL,                               {0, NULL}}, /* Camera Flash */
    {(void*)NULL,                               {0, NULL}}, /* WiFi Status LED */
    {(void*)lights_fl_region_sol_led,           {0, NULL}}, /* SOL */ 
    {(void*)lights_fl_region_tri_color,         {1, NULL}}, /* Bluetooth Status LED */
    {(void*)lights_fl_region_keypad,            {0, NULL}}  /* Navigation Keypad Backlight */
#endif
};

/*******************************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************************/
/*!
 * @brief Update an LED connected to a gpio
 *
 * This function will turn on or off an LED which is connected to gpio.  The control data
 * must include the gpio function to call.
 *
 * @param  pCtlData     Pointer to the control data for the region. 
 * @param  nColor       The color to set the region to.  In this case 0 is off and non
 *                      0 is on.
 *
 * @return Always returns 0.
 */
static bool lights_fl_region_gpio
(
    const LIGHTS_FL_LED_CTL_T *pCtlData, 
    POWER_IC_UNUSED LIGHTS_FL_COLOR_T nColor
)    
{
    tracemsg(_k_d("=>Update gpio light to color %d"), nColor);

    if (pCtlData->pGeneric != NULL)
    {
        (*(void (*)(LIGHTS_FL_COLOR_T))(pCtlData->pGeneric))(nColor != 0);
    }
    return true;
}

/*!
 * @brief Set main display
 *
 * Function to handle a request for the main display backlight on devices which support variable
 * backlight intensity and have display backlights controlled by either a GPIO or a power ic.
 * The backlight intensity is set in lights_led_backlights.c and converted to a hardware
 * value in this routine.
 *
 * @param   pCtlData   Pointer to the control data for the region.
 * @param   nStep      The brightness step to set the region to.
 *
 * @return     true  region updated
 *             false  region not updated
 */
#ifdef CONFIG_MACH_ASCENSION
static bool lights_fl_region_main_display
(
    POWER_IC_UNUSED const LIGHTS_FL_LED_CTL_T *pCtlData, 
    LIGHTS_FL_COLOR_T nStep
)
{
    int error = 0;
    const uint8_t atlas_brightness_tb[LIGHTS_NUM_DISPLAY_STEPS] =
    {
        0x00, /* Off   */
        0x02, /* 6 mA  */
        0x03, /* 9 mA  */
        0x04, /* 12 mA */
        0x05, /* 15 mA */
        0x06, /* 18 mA */
        0x07  /* 21 mA */
    };

    tracemsg(_k_d("=>Update the main display with step %d"), nStep);
    
    if (nStep >= LIGHTS_NUM_DISPLAY_STEPS)
    {
        nStep = LIGHTS_NUM_DISPLAY_STEPS-1;
    }

    if(nStep)
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                      LIGHTS_FL_CLI_DISP_DC_MASK,
                                      0); 
    
        error |= power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                       LIGHTS_FL_MAIN_DISP_DC_MASK | LIGHTS_FL_MAIN_DISP_CUR_MASK,
                                       LIGHTS_FL_MAIN_DISP_DC_MASK | atlas_brightness_tb[nStep]);
    }
    else
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                      LIGHTS_FL_MAIN_DISP_DC_MASK | LIGHTS_FL_MAIN_DISP_CUR_MASK,
                                      0);
    } 
    return (error != 0);
}
#endif
/*!
 * @brief Set CLI display
 *
 * Function to handle a request for the cli display backlight on devices which support variable
 * backlight intensity and have CLI display backlights controlled by either a GPIO port or a power
 * IC.
 *
 * @param  pCtlData     Pointer to the control data for the region. 
 * @param  nStep        The brightness step to set the region to. 
 *
 * @return     true  region updated
 *             false region not updated
 */
static bool lights_fl_region_cli_display
(
    const LIGHTS_FL_LED_CTL_T *pCtlData, 
    LIGHTS_FL_COLOR_T nStep
)   
{   
    int error;
    
    tracemsg(_k_d("=>Update the cli display with step %d"), nStep);
    if (nStep >= LIGHTS_NUM_CLI_DISPLAY_STEPS)
    {
        nStep = LIGHTS_NUM_CLI_DISPLAY_STEPS-1;
    }
    if(nStep)
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                      LIGHTS_FL_MAIN_DISP_DC_MASK|LIGHTS_FL_MAIN_DISP_CUR_MASK,
                                      0);
        error |= power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                       LIGHTS_FL_CLI_DISP_DC_MASK,
                                       LIGHTS_FL_CLI_DISP_DC_MASK);
    }
    else
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                      LIGHTS_FL_CLI_DISP_DC_MASK,
                                      0);
    }
    return (error != 0);
}

/*!
 * @brief Update keypad backlight
 *
 * Turn on or off the keypad backlight when it is connected to the power IC.
 *
 * @param  pCtlData    Pointer to the control data for the region. 
 * @param  nStep       The brightness step to set the region to. 
 *
 * @return     true  region updated
 *             false region not updated
 */
#ifndef CONFIG_MACH_ASCENSION
static bool lights_fl_region_keypad
(
    const LIGHTS_FL_LED_CTL_T *pCtlData, 
    LIGHTS_FL_COLOR_T nStep
)    
{
    int error = 0;
    
    tracemsg(_k_d("=>Update the keypad backlight with step %d"), nStep);
    if (nStep >= LIGHTS_NUM_KEYPAD_STEPS)
    {
        nStep = LIGHTS_NUM_KEYPAD_STEPS-1;
    }

    if(nStep)
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                      LIGHTS_FL_KEYPAD_DC_MASK,
                                      LIGHTS_FL_KEYPAD_DC_MASK);
    }
    else
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_2,
                                      LIGHTS_FL_KEYPAD_DC_MASK,
                                      0);
    }
    return (error != 0);
}
#endif

/*!
 * @brief Update a tri colored LED
 *
 * Function to handle the enabling tri color leds which have the regions combined.
 *
 * @param  pCtlData     Pointer to the control data for the region. 
 * @param  nColor       The color to set the region to. 
 *
 * @return     true  region updated
 *             false region not updated
 */
static bool lights_fl_region_tri_color 
(
    const LIGHTS_FL_LED_CTL_T *pCtlData, 
    LIGHTS_FL_COLOR_T nColor       
)
{
    unsigned int color_mask;
    int error;
    
    tracemsg(_k_d("=>Update the tricolor display %d"), nColor);

    color_mask = (((nColor & LIGHTS_FL_COLOR_BLUE) >> LIGHTS_FL_COLOR_BLUE_SFT)
                  >> LIGHTS_FL_DUTY_CYCLE_SHIFT)  << LIGHTS_FL_TRI_COLOR_BLUE_DC_INDEX;
    color_mask |= (((nColor & LIGHTS_FL_COLOR_GREEN) >> LIGHTS_FL_COLOR_GREEN_SFT) 
                  >> LIGHTS_FL_DUTY_CYCLE_SHIFT) << LIGHTS_FL_TRI_COLOR_GREEN_DC_INDEX;
    color_mask |= (((nColor & LIGHTS_FL_COLOR_RED) >> LIGHTS_FL_COLOR_RED_SFT) 
                  >> LIGHTS_FL_DUTY_CYCLE_SHIFT) << LIGHTS_FL_TRI_COLOR_RED_DC_INDEX;
                                    
    if(pCtlData->nData == 1)
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_3, LIGHTS_FL_TRI_COLOR_DC_MASK, color_mask);
    }
    else
    {
        error = power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_4, LIGHTS_FL_TRI_COLOR_DC_MASK, color_mask);
        error |= power_ic_set_reg_mask(POWER_IC_REG_ATLAS_LED_CONTROL_5, LIGHTS_FL_TRI_COLOR_DC_MASK, color_mask);
    }
 
    return (error != 0);
}

/*!
 * @brief Enable or disable the sol LED.
 *
 * Enables or disables the sol LED based on the nColor parameter.  When nColor is 0
 * the led is turned off when it is non 0 it is turned on.
 *
 * @param  pCtlData     Pointer to the control data for the region. 
 * @param  nColor       The color to set the region to.
 *
 * @return true  When the region is update.
 *         false When the region was not updated due to a communication or hardware error.
 */
static bool lights_fl_region_sol_led
(
    const LIGHTS_FL_LED_CTL_T *pCtlData, 
    LIGHTS_FL_COLOR_T nColor       
)
{
    int error;
    tracemsg(_k_d("=>Update the sol led with color %d"), nColor);
    
    error = power_ic_set_reg_bit(POWER_IC_REG_ATLAS_CHARGER_0,LIGHTS_FL_CHRG_LED_EN,(nColor != 0));
        
    return (error != 0);
}

/*******************************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************************/
/*!
 * @brief Enable or disable the LED EN bit
 *
 *  The LEDEN bit in Atlas is the master control bit for all the LED's. 
 *  leden will be set to FALSE if none of the LED's are enabled. Otherwise, 
 *  leden will be set to TRUE if one of more of the LED's are enabled 
 * @param  none
 *
 * @return none
 */
void lights_funlights_enable_led(void)
{
    unsigned int reg_value2;
    unsigned int reg_value3;
    unsigned int reg_value4;
    unsigned int reg_value5;
    int value;
    bool leden;
    /* Read out LED CONTROL 2 ,3,4,5 register value. */
    if((power_ic_read_reg(POWER_IC_REG_ATLAS_LED_CONTROL_2,&reg_value2))||
       (power_ic_read_reg(POWER_IC_REG_ATLAS_LED_CONTROL_3,&reg_value3))||
       (power_ic_read_reg(POWER_IC_REG_ATLAS_LED_CONTROL_4,&reg_value4))||
       (power_ic_read_reg(POWER_IC_REG_ATLAS_LED_CONTROL_5,&reg_value5)))
    {
        return;
    }
    leden = ((reg_value2 & (LIGHTS_FL_MAIN_DISP_DC_MASK |
                            LIGHTS_FL_MAIN_DISP_CUR_MASK |
                            LIGHTS_FL_CLI_DISP_DC_MASK |
                            LIGHTS_FL_KEYPAD_DC_MASK)) ||
             (reg_value3 & LIGHTS_FL_TRI_COLOR_DC_MASK)||
             (reg_value4 & LIGHTS_FL_TRI_COLOR_DC_MASK)||
             (reg_value5 & LIGHTS_FL_TRI_COLOR_DC_MASK));

    if(power_ic_get_reg_value(POWER_IC_REG_ATLAS_LED_CONTROL_0,LIGHTS_FL_LEDEN,&value,1))
    {
        return;
    }
    tracemsg(_k_d("The funlight register0 value is: %d"), value);
    /* If the state of the LEDs/Funlights is not equal to the state of the LEDEN bit
       in Atlas, then set the LEDEN bit to the state of the LEDs/Funlights */
    if(leden != value)
    {
        tracemsg(_k_d("=>Update the leden bit,leden= %d,"),leden);
        power_ic_set_reg_bit(POWER_IC_REG_ATLAS_LED_CONTROL_0,LIGHTS_FL_LEDEN, leden ? ENABLE : DISABLE);
    }
   
}
