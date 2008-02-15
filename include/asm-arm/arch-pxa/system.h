/*
 * linux/include/asm-arm/arch-pxa/system.h
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hardware.h"
#include "timex.h"
#include "../../../drivers/misc/ssp_pcap.h"

static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode)
{
	if (mode == 's') {
		/* Jump into ROM at address 0 */
		cpu_reset(0);
	} else {
#ifdef CONFIG_ARCH_EZX
		extern void pcap_switch_off_usb(void);
		extern void pcap_switch_on_usb(void);

		*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = NO_FLAG;

		pcap_switch_off_usb();
		mdelay(1000);
		pcap_switch_on_usb();

		if (!(GPLR(GPIO_BB_WDI) & GPIO_bit(GPIO_BB_WDI))) {
			*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = WDI_FLAG;
		}
		GPCR(GPIO_BB_RESET) = GPIO_bit(GPIO_BB_RESET);
#endif
		/* Initialize the watchdog and let it fire */
		OWER = OWER_WME;
		OSSR = OSSR_M3;
		OSMR3 = OSCR + CLOCK_TICK_RATE/100;	/* ... in 10 ms */
#ifdef CONFIG_ARCH_EZX
                MDREFR |= MDREFR_SLFRSH;
#endif
		while(1);
	}
}

