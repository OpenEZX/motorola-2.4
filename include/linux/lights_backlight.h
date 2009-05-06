/*
 * Copyright © 2004-2006, Motorola, All Rights Reserved.
 *
 * This program is licensed under a BSD license with the following terms:
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this list of conditions
 *   and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, this list of
 *   conditions and the following disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * - Neither the name of Motorola nor the names of its contributors may be used to endorse or
 *   promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Motorola 2006-Oct-25 - Update comments
 * Motorola 2006-Apr-12 - Added new interface functions to control backlight brightness.
 * Motorola 2005-Jan-26 - Add user space calls and enums
 * Motorola 2004-Dec-17 - Initial Creation
 */

#ifndef _LIGHTS_BACKLIGHTS_H
#define _LIGHTS_BACKLIGHTS_H

/*!
 * @file lights_backlight.h
 *
 * @brief Global symbols definitions for lights_backlights.c interface.
 *
 * @ingroup poweric_lights
 */

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
/* Defines the enum for the different regions */
typedef enum
{
    LIGHTS_BACKLIGHT_KEYPAD = 0,
    LIGHTS_BACKLIGHT_DISPLAY,
    LIGHTS_BACKLIGHT_CLI_DISPLAY,
    LIGHTS_BACKLIGHT_NAV,
    LIGHTS_BACKLIGHT_ALL
}LIGHTS_BACKLIGHT_T;

/* Defines the brightsness level, currently we set 256 brightness level*/
typedef unsigned char LIGHTS_BACKLIGHT_BRIGHTNESS_T;
/*==================================================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/
/* The structure is for the user space ioctl call */
typedef struct
{
    LIGHTS_BACKLIGHT_T bl_select;  
    LIGHTS_BACKLIGHT_BRIGHTNESS_T bl_brightness;
}LIGHTS_BACKLIGHT_SET_T;
    
/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATION
==================================================================================================*/

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
void lights_backlightset
(
    LIGHTS_BACKLIGHT_T bl_select,  
    LIGHTS_BACKLIGHT_BRIGHTNESS_T bl_brightness   
);
#endif

