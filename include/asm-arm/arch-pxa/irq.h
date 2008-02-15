/*
 * linux/include/asm-arm/arch-pxa/irq.h
 * 
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * by zxf, 11/11/2002
 * redefine this macro to pre-handle irq for power management to auto idle and sleep
 */
#define fixup_irq(x)	pm_handle_irq(x)

//#define fixup_irq(x)	(x)
/*
 * This prototype is required for cascading of multiplexed interrupts.
 * Since it doesn't exist elsewhere, we'll put it here for now.
 */
extern void do_IRQ(int irq, struct pt_regs *regs);
