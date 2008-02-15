/*
 * Copyright (C) 2005,2006 - Motorola
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 * Motorola 2006-Apr-12 - Added new interface functions to control backlight brightness.
 * Motorola 2005-Feb-28 - Rewrote the software for PCAP.
 *
 * Motorola 2006-Apr-07 - Add support for NAV Keypad backlight
 *
 */

/*!
 * @file lights_backlight.c
 *
 * @brief Backlight setting
 *
 *  This is the main file of the backlight interface which conrols the backlight setting.
 *
 * @ingroup poweric_lights
 */

#include <linux/power_ic.h>
#include <linux/lights_backlight.h>
#include <linux/lights_funlights.h>
#include <linux/module.h>
/*******************************************************************************************
                                           CONSTANTS
 ******************************************************************************************/

/********************************************************************************************
                                 STRUCTURES AND OTHER TYPEDEFS
*******************************************************************************************/

/*******************************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************************/
/*!
 * @brief  Set the backlight to specific step level as defined by the hardware.
 *
 * This function will set the backlight intensity to the step provided. 
 *
 * @param bl_select      Indicates which backlights to enable/disable.
 * @param bl_step        Level of brightness.
 */
void lights_backlightset_step
(
    LIGHTS_BACKLIGHT_T bl_select,  
    LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T bl_step
)
{
    const LIGHTS_FL_REGION_MSK_T bl_to_region[LIGHTS_BACKLIGHT_ALL] =
    {
        LIGHTS_FL_REGION_KEYPAD_NUM_BL,  /* LIGHTS_BACKLIGHT_KEYPAD      */
        LIGHTS_FL_REGION_DISPLAY_BL,     /* LIGHTS_BACKLIGHT_DISPLAY     */
        LIGHTS_FL_REGION_CLI_DISPLAY_BL, /* LIGHTS_BACKLIGHT_CLI_DISPLAY */
        LIGHTS_FL_REGION_KEYPAD_NAV_BL   /* LIGHTS_BACKLIGHT_NAV         */
    };
    
    /*
     * For the backlights the color is used to pass the intensity in steps supported
     * by the hardware.
     */
    if (bl_select >= LIGHTS_BACKLIGHT_ALL)
    {
        lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT, 4,
                         LIGHTS_FL_REGION_KEYPAD_NUM_BL,
                         bl_step,
                         LIGHTS_FL_REGION_KEYPAD_NAV_BL,
                         bl_step,
                         LIGHTS_FL_REGION_CLI_DISPLAY_BL,
                         bl_step,
                         LIGHTS_FL_REGION_DISPLAY_BL,
                         bl_step);
    }
    else
    {
        lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT, 1, bl_to_region[bl_select], bl_step);
    }
}

/*!
 * @brief  Set the backlight intensity.
 *
 * This function activates/deactivates (with intensity for the main and cli displays)
 * the selected backlight(s).  The brightness will be in a range of 0 to 100 with 0 being
 * off and 100 being full intensity.
 *
 * @param bl_select      Indicates which backlights to enable/disable.
 * @param bl_brightness  Levels of brightness from 0 to 100 with 0 being off.
 */
void lights_backlightset(LIGHTS_BACKLIGHT_T bl_select, LIGHTS_BACKLIGHT_BRIGHTNESS_T bl_brightness)
{
    LIGHTS_BACKLIGHT_T i;
    
    if (bl_select >= LIGHTS_BACKLIGHT_ALL)
    {
        i = LIGHTS_BACKLIGHT_NAV;
        do
        {
            lights_backlightset_step(i, bl_brightness/lights_bl_percentage_steps[i]);
        } while(i);
    }
    else
    {
        lights_backlightset_step(bl_select, bl_brightness/lights_bl_percentage_steps[bl_select]);
    }
}

/*!
 * @brief  Return the number of brightness steps supported for a backlight
 *
 * This function will return the number of brightness steps implemented in the hardware
 * driver for a type of backlight. The number of steps will include the value for off.
 *
 * @param bl_select      Indicates which backlights to enable/disable (LIGHTS_BACKLIGHT_ALL
 *                       cannot be used with this function).
 * @return               The number of steps supported.
 */
LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T lights_backlightget_steps
(
    LIGHTS_BACKLIGHT_T bl_select
)
{
    if (bl_select < LIGHTS_BACKLIGHT_ALL)
    {
        return lights_bl_num_steps_tb[bl_select];
    }
    return 0;
}

/*!
 * @brief  Convert a step to a percentage based on the current hardware.
 *
 * This function will convert a step to a percentage suitable for use by
 * lights_backlightset().
 *
 * @param bl_select      Indicates which backlights to run the conversion on
 *                       (LIGHTS_BACKLIGHT_ALL cannot be used with this function).
 * @param bl_step        The brightness step to convert to a percentage.
 * @return               The percentage equivalent to bl_step.
 */
LIGHTS_BACKLIGHT_BRIGHTNESS_T lights_backlights_step_to_percentage
(
    LIGHTS_BACKLIGHT_T bl_select,
    LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T bl_step
)
{
    LIGHTS_BACKLIGHT_BRIGHTNESS_T tmp = 0;

    if (bl_select < LIGHTS_BACKLIGHT_ALL)
    {
        tmp = lights_bl_percentage_steps[bl_select]*bl_step;
        if (tmp > 100)
        {
            tmp = 100;
        }
    }
    return tmp;
}

/*==================================================================================================
                                     EXPORTED SYMBOLS
==================================================================================================*/

#ifndef DOXYGEN_SHOULD_SKIP_THIS
EXPORT_SYMBOL(lights_backlightset_step);
EXPORT_SYMBOL(lights_backlightset);
EXPORT_SYMBOL(lights_backlightget_steps);
EXPORT_SYMBOL(lights_backlights_step_to_percentage);
#endif   
 
