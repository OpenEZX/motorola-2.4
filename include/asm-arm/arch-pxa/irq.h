/*
 * linux/include/asm-arm/arch-pxa/irq.h
 *
 * Copyright (C) 2004 Motorola
 * 
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2004-Jan-16 - (Motorola) Added definition for Motorola specific platform
 */

#ifdef CONFIG_ARCH_EZX
#define fixup_irq(x)	pm_handle_irq(x)
#else
#define fixup_irq(x)	(x)
#endif

/*
 * This prototype is required for cascading of multiplexed interrupts.
 * Since it doesn't exist elsewhere, we'll put it here for now.
 */
extern void do_IRQ(int irq, struct pt_regs *regs);
