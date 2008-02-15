/*
 * PXA250/210 Power Management Routines
 *
 * Original code for the SA11x0:
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * Modified for the PXA250 by Nicolas Pitre:
 * Copyright (c) 2002 Monta Vista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/leds.h>


/*
 * Debug macros
 */
#undef DEBUG

extern void pxa_cpu_suspend(void);
extern void pxa_cpu_resume(void);

#define SAVE(x)		sleep_save[SLEEP_SAVE_##x] = x
#define RESTORE(x)	x = sleep_save[SLEEP_SAVE_##x]

/*
 * List of global PXA peripheral registers to preserve.
 * More ones like CP and general purpose register values are preserved
 * with the stack pointer in sleep.S.
 */
enum {	SLEEP_SAVE_START = 0,

	SLEEP_SAVE_OSCR, SLEEP_SAVE_OIER,
	SLEEP_SAVE_OSMR0, SLEEP_SAVE_OSMR1, SLEEP_SAVE_OSMR2, SLEEP_SAVE_OSMR3,

	SLEEP_SAVE_GPDR0, SLEEP_SAVE_GPDR1, SLEEP_SAVE_GPDR2,
	SLEEP_SAVE_GRER0, SLEEP_SAVE_GRER1, SLEEP_SAVE_GRER2,
	SLEEP_SAVE_GFER0, SLEEP_SAVE_GFER1, SLEEP_SAVE_GFER2,
	SLEEP_SAVE_GAFR0_L, SLEEP_SAVE_GAFR1_L, SLEEP_SAVE_GAFR2_L,
	SLEEP_SAVE_GAFR0_U, SLEEP_SAVE_GAFR1_U, SLEEP_SAVE_GAFR2_U,

	SLEEP_SAVE_FFIER, SLEEP_SAVE_FFLCR, SLEEP_SAVE_FFMCR,
	SLEEP_SAVE_FFSPR, SLEEP_SAVE_FFISR,
	SLEEP_SAVE_FFDLL, SLEEP_SAVE_FFDLH,

	SLEEP_SAVE_ICMR,
	SLEEP_SAVE_CKEN,

	/* by zxf, 12/09/02, for user off */
	SLEEP_SAVE_PMCR, SLEEP_SAVE_PCFR, SLEEP_SAVE_PWER, 
	SLEEP_SAVE_PRER, SLEEP_SAVE_PFER, SLEEP_SAVE_PSPR,
	SLEEP_SAVE_PGSR0, SLEEP_SAVE_PGSR1, SLEEP_SAVE_PGSR2,

	SLEEP_SAVE_CKSUM,

	SLEEP_SAVE_SIZE
};


int pm_do_useroff(void)
{
	unsigned long sleep_save[SLEEP_SAVE_SIZE];
	unsigned long checksum = 0;
	int i;

	cli();
	clf();

	leds_event(led_stop);

	/* preserve current time */
	RCNR = xtime.tv_sec;

	/* save vital registers */
	SAVE(OSCR);
	SAVE(OSMR0);
	SAVE(OSMR1);
	SAVE(OSMR2);
	SAVE(OSMR3);
	SAVE(OIER);

        /* save the FF UART */
	SAVE(FFIER);
	SAVE(FFLCR);
	SAVE(FFMCR);
	SAVE(FFSPR);
	SAVE(FFISR);
	FFLCR |= 0x80;
	SAVE(FFDLL);
	SAVE(FFDLH);
	FFLCR &= 0xef;

	SAVE(GPDR0); SAVE(GPDR1); SAVE(GPDR2);
	SAVE(GRER0); SAVE(GRER1); SAVE(GRER2);
	SAVE(GFER0); SAVE(GFER1); SAVE(GFER2);
	SAVE(GAFR0_L); SAVE(GAFR0_U);
	SAVE(GAFR1_L); SAVE(GAFR1_U);
	SAVE(GAFR2_L); SAVE(GAFR2_U);

	SAVE(ICMR);
	ICMR = 0;

	SAVE(CKEN);
	CKEN = 0;

	/* by zxf, 12/09/02, save for user off */
	SAVE(PMCR); SAVE(PCFR); SAVE(PWER); 
	SAVE(PRER); SAVE(PFER); SAVE(PSPR);
	SAVE(PGSR0); SAVE(PGSR1); SAVE(PGSR2);

	/* Note: wake up source are set up in each machine specific files */

	/* clear GPIO transition detect  bits */
	GEDR0 = GEDR0; GEDR1 = GEDR1; GEDR2 = GEDR2;

	/* by zxf, 12/09/02, for enter user off */
	/* Clear reset status */
	RCSR = RCSR;

	/* set resume return address */
	//PSPR = virt_to_phys(pxa_cpu_resume);
	*(unsigned long *)(phys_to_virt(RESUME_ADDR)) = virt_to_phys(pxa_cpu_resume);
	*(unsigned long *)(phys_to_virt(FLAG_ADDR)) = USER_OFF_FLAG;

	// before sleeping, calculate and save a checksum
	for (i = 0; i < SLEEP_SAVE_SIZE - 1; i++)
		checksum += sleep_save[i];
	sleep_save[SLEEP_SAVE_CKSUM] = checksum;

	/* *** go zzz *** */
	pxa_cpu_suspend();

	/* after sleeping, validate the checksum */
	checksum = 0;
	for (i = 0; i < SLEEP_SAVE_SIZE - 1; i++)
		checksum += sleep_save[i];

	/* if invalid, display message and wait for a hardware reset */
	if (checksum != sleep_save[SLEEP_SAVE_CKSUM]) {
#ifdef CONFIG_ARCH_LUBBOCK
		LUB_HEXLED = 0xbadbadc5;
#endif
		while (1);
	}

	/* ensure not to come back here if it wasn't intended */
	/* by zxf, 12/09/02, for exit user off */
	//PSPR = 0;
	*(unsigned long *)(phys_to_virt(RESUME_ADDR)) = 0;
	*(unsigned long *)(phys_to_virt(FLAG_ADDR)) = OFF_FLAG;

	/* restore registers */
	RESTORE(GPDR0); RESTORE(GPDR1); RESTORE(GPDR2);
	RESTORE(GRER0); RESTORE(GRER1); RESTORE(GRER2);
	RESTORE(GFER0); RESTORE(GFER1); RESTORE(GFER2);
	RESTORE(GAFR0_L); RESTORE(GAFR0_U);
	RESTORE(GAFR1_L); RESTORE(GAFR1_U);
	RESTORE(GAFR2_L); RESTORE(GAFR2_U);

	PSSR = PSSR_PH;

	RESTORE(OSMR0);
	RESTORE(OSMR1);
	RESTORE(OSMR2);
	RESTORE(OSMR3);
	RESTORE(OSCR);
	RESTORE(OIER);

	RESTORE(CKEN);

	/* by zxf, 12/09/02, restore for user off */
	RESTORE(PMCR); RESTORE(PCFR); RESTORE(PWER); 
	RESTORE(PRER); RESTORE(PFER); RESTORE(PSPR);
	RESTORE(PGSR0); RESTORE(PGSR1); RESTORE(PGSR2);

	ICLR = 0;
	ICCR = 1;
	RESTORE(ICMR);

        /* restore FF UART */
	RESTORE(FFMCR);
	RESTORE(FFSPR);
	RESTORE(FFLCR);
	FFLCR |= 0x80;
	RESTORE(FFDLH);
	RESTORE(FFDLL);
	RESTORE(FFLCR);
	RESTORE(FFISR);
	FFFCR = 0x07;
	RESTORE(FFIER);

	/* restore current time */
	xtime.tv_sec = RCNR;

#ifdef DEBUG
#error this requires more registers to be preserved/restored to work
	printk(KERN_DEBUG "*** made it back from resume\n");
#endif

	leds_event(led_start);

	sti();

	return 0;
}


