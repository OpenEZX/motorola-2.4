/*
 * linux/arch/arm/mach-ezx/sleep.S
 *
 * EZX specific low-level PXA250/210 sleep/wakeUp support
 *
 * Initial SA1110 code:
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * Adapted for PXA by Nicolas Pitre:
 * Copyright (c) 2002 Monta Vista Software, Inc.
 *
 * Based on original code from linux/arch/arm/mach-pxa/sleep.S,
 * Modified for the EZX Development Plaform by Motorola:
 * Copyright (c) 2003-2005 Motorola
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * 2003-Dec-23 - (Motorola) Created based on mach-pxa/sleep.S
 *                          New pxa_cpu_standby ENTRY
 *
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>

		.text
/*
 * pxa_cpu_suspend(unsigned int mode)
 *
 * Forces CPU into sleep state or deep sleep stats
 */

ENTRY(pxa_cpu_suspend)
	mov	r12,	r0					@ keep the mode.
	mra	r2, r3, acc0
	stmfd	sp!, {r2 - r12, lr}		@ save registers on stack

	@ get coprocessor registers
	mrc	p15, 0, r4, c15, c1, 0		@ CP access reg
	mrc	p15, 0, r5, c13, c0, 0		@ PID
	mrc 	p15, 0, r6, c3, c0, 0		@ domain ID
	mrc 	p15, 0, r7, c2, c0, 0		@ translation table base addr
	mrc	p15, 0, r8, c1, c1, 0           @ auxiliary control reg
	mrc 	p15, 0, r9, c1, c0, 0		@ control reg

	@ store them plus current virtual stack ptr on stack
	mov	r10, sp
	stmfd	sp!, {r4 - r10}

	@ preserve phys address of stack
	mov	r0, sp
	bl	sleep_phys_sp
	ldr	r1, =sleep_save_sp
	str	r0, [r1]

	@ clean data cache 
	bl	cpu_xscale_cache_clean_invalidate_all

	@ Put the processor to sleep
	@ (also workaround for sighting 28071)

	@ 	prepare value for sleep mode
	@   mov	r1, #3				@ sleep mode
	mov	r1,	r12				@ sleep mode or other modes.

	@ align execution to a cache line
	b	1f

	.ltorg
	.align	5
1:

	@ All needed values are now in registers. 
	@ These last instructions should be in cache

	@ enter sleep mode
	mcr	p14, 0, r1, c7, c0, 0

	@ add by cain for standby.
	mov	pc, lr

20:	nop
	b	20b				@ loop waiting for sleep

/*
 * cpu_pxa_resume()
 *
 * entry point from bootloader into kernel during resume
 *
 * Note: Yes, part of the following code is located into the .data section.
 *       This is to allow sleep_save_sp to be accessed with a relative load
 *       while we can't rely on any MMU translation.  We could have put
 *       sleep_save_sp in the .text section as well, but some setups might
 *       insist on it to be truely read-only.
 */

	.data
	.align 5
ENTRY(pxa_cpu_resume)
	mov	r0, #I_BIT | F_BIT | MODE_SVC	@ set SVC, irqs off
	msr	cpsr_c, r0

	ldr	r0, sleep_save_sp		@ stack phys addr
	ldr	r2, =resume_after_mmu		@ its absolute virtual address
	ldmfd	r0, {r4 - r9, sp}		@ CP regs + virt stack ptr

	mov	r1, #0
	mcr	p15, 0, r1, c8, c7, 0   	@ invalidate I & D TLBs
	mcr	p15, 0, r1, c7, c7, 0		@ invalidate I & D caches, BTB

#ifdef CONFIG_XSCALE_CACHE_ERRATA
	bic     r9, r9, #0x0004			@ see cpu_xscale_proc_init
#endif

	mcr	p15, 0, r4, c15, c1, 0		@ CP access reg
	mcr	p15, 0, r5, c13, c0, 0		@ PID
	mcr 	p15, 0, r6, c3, c0, 0		@ domain ID
	mcr 	p15, 0, r7, c2, c0, 0		@ translation table base addr
	mcr	p15, 0, r8, c1, c1, 0           @ auxiliary control reg
	b	resume_turn_on_mmu		@ cache align execution

	.align 5
resume_turn_on_mmu:
	mcr 	p15, 0, r9, c1, c0, 0		@ turn on MMU, caches, etc.

	@ Let us ensure we jump to resume_after_mmu only when the mcr above
	@ actually took effect.  They call it the "cpwait" operation.
	mrc	p15, 0, r1, c2, c0, 0		@ queue a dependency on CP15
	sub	pc, r2, r1, lsr #32		@ jump to virtual addr 
	nop
	nop
	nop

sleep_save_sp:
	.word	0				@ preserve stack phys ptr here

	.text
resume_after_mmu:
#ifdef CONFIG_XSCALE_CACHE_ERRATA
	bl	cpu_xscale_proc_init
#endif
	ldmfd	sp!, {r2, r3}
	mar	acc0, r2, r3
	ldmfd	sp!, {r4 - r12, pc}		@ return to caller


/*
 * pxa_cpu_standby(int param)
 * Forces CPU into  standby.
 */
	.align	5
/*
ENTRY(pxa_cpu_standby)

	@ prepare value for sleep mode
	mov	r1, #2				@ standby mode

	@ All needed values are now in registers. 
	@ These last instructions should be in cache
	@ enter standby mode
	mcr	p14, 0, r1, c7, c0, 0

	@ CPU will stop exec until be waked up.
	@ After be waken, just return.
	mov	pc, lr

	nop
	nop
	nop
	nop
*/
