/*
 * (c) Copyright 2004-2006 Motorola, Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  
 * 02111-1307, USA
 *
 * Motorola 2006-Apr-12 - Added new interface functions to control backlight brightness.
 * Motorola 2005-Jan-26 - Add user space calls and enums
 * Motorola 2004-Dec-17 - Initial Creation
 */
#include <linux/version.h>

/*!
 * @file lights_backlight.h
 *
 * @brief Global symbols definitions for lights_backlights.c interface.
 *
 * @ingroup poweric_lights
 */
#ifndef _LIGHTS_BACKLIGHTS_H
#define _LIGHTS_BACKLIGHTS_H
/*==================================================================================================
                                         INCLUDE FILES
==================================================================================================*/

/*==================================================================================================
                                           CONSTANTS
==================================================================================================*/
    
/*==================================================================================================
                                            MACROS
==================================================================================================*/

/*==================================================================================================
                                             ENUMS
==================================================================================================*/
/*!
 * @brief The regions which can be used with the backlight functions.
 *
 * The following enumeration defines the regions which can be used with the backlight functions
 * defined within this file.  Please note LIGHTS_BACKLIGHT_ALL is not supported by all functions.
 * Please check the function documentation before attempting to use LIGHTS_BACKLIGHT_ALL.
 */
typedef enum
{
    LIGHTS_BACKLIGHT_KEYPAD = 0,
    LIGHTS_BACKLIGHT_DISPLAY,
    LIGHTS_BACKLIGHT_CLI_DISPLAY,
    LIGHTS_BACKLIGHT_NAV,
    LIGHTS_BACKLIGHT_ALL,
    LIGHTS_BACKLIGHT__NUM
} LIGHTS_BACKLIGHT_T;

/*!
 * @brief Defines the brightness levels which can be used with the backlight functions.
 * 
 * Defines the brightness level which can be used with the backlight functions.  The brightness
 * can range from 0 to 100 and can be considered to be a percentage.  Not every value will change
 * the intensity of the backlight, since the physical hardware only supports small number of steps.
 * If a change is desired the step functions (eg. lights_backlightset_step()) must be used.
 */
typedef unsigned char LIGHTS_BACKLIGHT_BRIGHTNESS_T;

/*!
 * @brief Defines the brightness steps which can be used with the backlight functions.
 * 
 * Defines the brightness steps which can be used with the backlight functions.   This is not a
 * percentage, but the a step which translates  directly to what the hardware can do.  This is
 * provided to allow the apps controlling the backlight control such that an actual change can be
 * seen when the step is changed, unlike the percentage values.
 */
typedef unsigned char LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T;

/*==================================================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/
/*!
 * @brief The structure is for the user space IOCTL calls which handle step and percentage functions.
 *
 * The following structure is used to pass information to the kernel from user space and vica verca.
 * The fields are used differently based on the IOCTL being used.  Please see the documentation for
 * the IOCTLS (located in power_ic.h) for the details on which are used for a specific IOCTL.
 */
typedef struct
{
    LIGHTS_BACKLIGHT_T bl_select;                /*!< @brief The backlight to update. */
    LIGHTS_BACKLIGHT_BRIGHTNESS_T bl_brightness; /*!< @brief The intensity to set with 0 being off and 100
                                                      being full intensity.  */
    LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T bl_step;  /*!< @brief The intensity to set with 0 being off and 
                                                      the maximum being the number of steps the hardware
                                                      supports. */
} LIGHTS_BACKLIGHT_IOCTL_T;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
/*!
 * @brief This typedef is for backwards compatibility and should not be used for new code.
 *
 * @note This typedef must not be used as it will be removed in a future release of the code.
 */
typedef LIGHTS_BACKLIGHT_IOCTL_T LIGHTS_BACKLIGHT_SET_T;
#endif

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATION
==================================================================================================*/
#ifdef __KERNEL__
/*!
 * @brief Table to determine the number of brightness steps supported by hardware.
 *
 * Table which is indexed by the backlight select field to obtain the number of steps for
 * a specific backlight.  LIGHTS_BACKLIGHT_ALL cannot be used with this table.
 *
 * @note This is internal to the kernel driver and should never be used outside of the lights code.
 */
extern const uint8_t lights_bl_num_steps_tb[];

/*!
 * @brief Table to determine the percentage step size for backlight brightness.
 *
 * Table which is indexed by the backlight select field to obtain the percentage step size
 * for a specific backlight.  LIGHTS_BACKLIGHT_ALL cannot be used with this table.
 *
 * @note This is internal to the kernel driver and should never be used outside of the lights code.
 */
extern const uint8_t lights_bl_percentage_steps[];
#endif

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T lights_backlightget_steps
(
    LIGHTS_BACKLIGHT_T bl_select
);

void lights_backlightset_step
(
    LIGHTS_BACKLIGHT_T bl_select,
    LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T bl_step
);

LIGHTS_BACKLIGHT_BRIGHTNESS_T lights_backlights_step_to_percentage
(
    LIGHTS_BACKLIGHT_T bl_select,
    LIGHTS_BACKLIGHT_BRIGHTNESS_STEP_T bl_step
);

void lights_backlightset
(
    LIGHTS_BACKLIGHT_T bl_select,
    LIGHTS_BACKLIGHT_BRIGHTNESS_T bl_brightness
);
#endif

