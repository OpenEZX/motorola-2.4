/*
 * arch/arm/mach-pxa/mainstone-dpm.c
 * 
 * Mainstone-specific DPM support
 *
 * Author: <source@mvista.com>
 *
 * 2003 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 */

#include <linux/config.h>
#include <linux/dpm.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <asm/hardirq.h>
#include <asm/page.h>
#include <asm/uaccess.h>

#include "bulverde_voltage.h"

/****************************************************************************
 * Initialization/Exit
 ****************************************************************************/

int dpm_mainstone_bd_init(void)
{
	return 0;
}

void dpm_mainstone_bd_exit(void)
{
	return;
}

unsigned int 
dpm_mainstone_bd_set_v(unsigned cur_mv, unsigned target_mv, int flags)
{
	if (bulverde_validate_voltage(target_mv) > 0) {
		bulverde_set_voltage(target_mv);
	}
	return 0;
}	

void dpm_bulverde_board_setup(void)
{
	dpm_bd.init = dpm_mainstone_bd_init;
	dpm_bd.exit = dpm_mainstone_bd_exit;
	dpm_bd.set_v_pre = dpm_mainstone_bd_set_v;
	dpm_bd.set_v_post = dpm_mainstone_bd_set_v;
}
