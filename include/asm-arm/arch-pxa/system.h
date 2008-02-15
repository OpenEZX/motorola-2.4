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
#define GPIO4_BB_WDI     0x00000010 
#define GPIO27_BB_RESET  0x08000000
static inline void arch_idle(void)
{
	if (!hlt_counter) {
		int flags;
		local_irq_save(flags);
		if(!current->need_resched)
			cpu_do_idle(0);
		local_irq_restore(flags);
	}
}

extern void pcap_switch_off_usb(void);
extern void pcap_switch_on_usb(void);

static inline void arch_reset(char mode)
{
	if (mode == 's') {
		/* Jump into ROM at address 0 */
		cpu_reset(0);
	} else if (mode == 'f') {
		*(unsigned long *)(phys_to_virt(FLAG_ADDR)) = REFLASH_FLAG;
		*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = NO_FLAG;
		
		pcap_switch_off_usb();
	        mdelay(1000);
		pcap_switch_on_usb();
                if(!(GPLR0&GPIO4_BB_WDI))
                {
			*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = WDI_FLAG;
                	GPCR0 = GPIO27_BB_RESET; 
                }

		/* Initialize the watchdog and let it fire */
		OWER = OWER_WME;
		OSSR = OSSR_M3;
		OSMR3 = OSCR + 36864;	/* ... in 10 ms */
		MDREFR |= MDREFR_SLFRSH;
	} else {
		*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = NO_FLAG;

		pcap_switch_off_usb();
	        mdelay(1000);
		pcap_switch_on_usb();
                if(!(GPLR0&GPIO4_BB_WDI))
                {
			*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = WDI_FLAG;
                }

                GPCR0 = GPIO27_BB_RESET; 
		/* Initialize the watchdog and let it fire */
		OWER = OWER_WME;
		OSSR = OSSR_M3;
		OSMR3 = OSCR + 36864;	/* ... in 10 ms */
		MDREFR |= MDREFR_SLFRSH;
	}
}

