/* sdio_wprm_drv.c --- This file includes Power management APIs for SDIO driver
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
 * Motorola         15/09/2005      Config SDIO GPIO state in SLEEP
*/


/*---------------------------------- Includes --------------------------------*/
#include <asm/hardware.h>
#include <linux/power_ic.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include "sdio_wprm_drv.h"

/*---------------------------------- Globals ---------------------------------*/

/*---------------------------------- APIs ------------------------------------*/

/* wprm_wlan_gpio_init --- WLAN module GPIOs clean up in init 
 * Description:
 *    Initialize WLAN GPIOs. 
 * Parameters:
 *    None
 * Return Value:
 *    0  : success
 *    -1 : fail
 */
int wprm_wlan_gpio_init()
{
   /* Workaround for leakage current issue, we config  the wifi gpio here*/
   /* Configure MMCLK bus clock output signal*/
   set_GPIO_mode(GPIO_MMC_CLK | GPIO_ALT_FN_2_OUT);
   /* Configure MMCMD command/response bidirectional signal*/
   set_GPIO_mode(GPIO_MMC_CMD | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
   /* Configure MMDAT[0123] data bidirectional signals*/
   set_GPIO_mode(GPIO_MMC_DATA0 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
   set_GPIO_mode(GPIO_MMC_DATA1 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
   set_GPIO_mode(GPIO_MMC_DATA2 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
   set_GPIO_mode(GPIO_MMC_DATA3 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);

   set_GPIO_mode(GPIO_WLAN_RESETB_MD);               
   set_GPIO_mode(GPIO_HOST_WLAN_WAKEB_MD);
   set_GPIO_mode(GPIO_WLAN_HOST_WAKEB_MD);
   set_GPIO(GPIO_HOST_WLAN_WAKEB);
   
   /* Configure these GPIO state in SLEEP Mode */
   PGSR(GPIO_HOST_WLAN_WAKEB) |= GPIO_bit(GPIO_HOST_WLAN_WAKEB);
   PGSR(GPIO_WLAN_RESETB) &= ~GPIO_bit(GPIO_WLAN_RESETB);
   
   PGSR(GPIO_MMC_CMD)   &= ~GPIO_bit(GPIO_MMC_CMD);
   PGSR(GPIO_MMC_DATA0) &= ~GPIO_bit(GPIO_MMC_DATA0);
   PGSR(GPIO_MMC_DATA1) &= ~GPIO_bit(GPIO_MMC_DATA1);
   PGSR(GPIO_MMC_DATA2) &= ~GPIO_bit(GPIO_MMC_DATA2);
   PGSR(GPIO_MMC_DATA3) &= ~GPIO_bit(GPIO_MMC_DATA3);
   
  /* Hardware issue workaround 
   * - On Martinique P3 wingboard, before WLAN chip could work, must hold WLAN_RESETB
   *   as high for at least 20ms.
   */
  set_GPIO(GPIO_WLAN_RESETB);        // reset WLAN
  mdelay(40);
  clr_GPIO(GPIO_WLAN_RESETB);        // clear WLAN reset


  /* Register WLAN_HOST_WAKEB as wakeup source to Bulverde.
   *  This setting applies to Bartinique P1B and later only, which has GPIO-94 defined
   *  as WLAN_HOST_WAKEB.
   */
  printk("PKWR = 0x%x.\n", PKWR);
  PKWR |= 0x00000800;
  printk("PKWR = 0x%x.\n", PKWR);

  return 0;
}

/* wprm_wlan_power_supply --- WLAN module power supply turn on/off
 * Description:
 *     WLAN module power supply turn on/off.
 * Parameters:
 *     POWER_IC_PERIPH_ONOFF_T onoff =: 
 *         POWER_IC_PERIPH_OFF - turn off
 *         POWER_IC_PERIPH_ON  - turn on
 * Return Value:
 *     This function returns 0 if successful.
 */
int wprm_wlan_power_supply(POWER_IC_PERIPH_ONOFF_T onoff)
{
  return power_ic_periph_set_wlan_on(onoff);
}


/* wprm_wlan_gpio_exit --- WLAN module GPIOs clean up in exit
 * Description:
 *    Clean up WLAN GPIOs when exit. 
 * Parameters:
 *    None
 * Return Value:
 *    0  : success
 *    -1 : fail
 */
  /* WLAN module GPIO clean up */
int wprm_wlan_gpio_exit()
{
  /*Workaround for leakage current issue , we drive below WiFi GPIO LOW when WLAN off*/ 
  set_GPIO_mode(GPIO_MMC_CLK | GPIO_OUT);
  set_GPIO_mode(GPIO_MMC_CMD | GPIO_OUT);
  set_GPIO_mode(GPIO_MMC_DATA0 | GPIO_OUT);
  set_GPIO_mode(GPIO_MMC_DATA1 | GPIO_OUT);
  set_GPIO_mode(GPIO_MMC_DATA2 | GPIO_OUT);
  set_GPIO_mode(GPIO_MMC_DATA3 | GPIO_OUT);
  clr_GPIO(GPIO_MMC_CLK);
  clr_GPIO(GPIO_MMC_CMD);
  clr_GPIO(GPIO_MMC_DATA0);
  clr_GPIO(GPIO_MMC_DATA1);
  clr_GPIO(GPIO_MMC_DATA2);
  clr_GPIO(GPIO_MMC_DATA3);

  set_GPIO_mode(GPIO_HOST_WLAN_WAKEB  | GPIO_OUT);
  set_GPIO_mode(GPIO_WLAN_RESETB  | GPIO_OUT);
  clr_GPIO(GPIO_HOST_WLAN_WAKEB);
  clr_GPIO(GPIO_WLAN_RESETB);
  /* De-Register WLAN_HOST_WAKEB as wakeup source to Bulverde.
   *  This setting applies to Bartinique P1B and later only, which has GPIO-94 defined
   *  as WLAN_HOST_WAKEB.
   */
  printk("PKWR = 0x%x.\n", PKWR);
  PKWR &= (~0x00000800);
  printk("PKWR = 0x%x.\n", PKWR);

  return 0;
}

