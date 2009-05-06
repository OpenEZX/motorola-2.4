#ifndef _SDIO_WPRM_DRV_H_
#define _SDIO_WPRM_DRV_H_
/* sdio_wprm_drv.h --- This is header file for Power management APIs for SDIO driver
 *
 *                        Motorola Confidential Proprietary
 *                  Advanced Technology and Software Operations
 *                (c) Copyright Motorola 2005, All Rights Reserved
 *
 * Revision History:
 * Author        Date         Description
 * Motorola      28/04/2005   Created
 * Motorola      10/25/2006   update interface for poweric driver
 */

/*---------------------------------- Includes --------------------------------*/
#include <linux/power_ic_kernel.h>
/*---------------------------------- Globals ---------------------------------*/

/*---------------------------------- APIs ------------------------------------*/
int wprm_wlan_gpio_init(void);
int wprm_wlan_power_supply(POWER_IC_PERIPH_ONOFF_T onoff);
int wprm_wlan_gpio_exit(void);
#endif
