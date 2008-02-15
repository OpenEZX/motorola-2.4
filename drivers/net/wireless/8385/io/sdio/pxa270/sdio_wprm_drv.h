#ifndef _SDIO_WPRM_DRV_H_
#define _SDIO_WPRM_DRV_H_
/* sdio_wprm_drv.h --- This is header file for Power management APIs for SDIO driver
 *
 * (c) Copyright Motorola 2005.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Revision History:
 * Author           Date            Description
 * Motorola         28/04/2005      Created
 */

/*---------------------------------- Includes --------------------------------*/
#include <linux/power_ic.h>
/*---------------------------------- Globals ---------------------------------*/

/*---------------------------------- APIs ------------------------------------*/
int wprm_wlan_gpio_init(void);
int wprm_wlan_power_supply(POWER_IC_PERIPH_ONOFF_T onoff);
int wprm_wlan_gpio_exit(void);
#endif
