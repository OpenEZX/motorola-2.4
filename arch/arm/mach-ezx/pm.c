/*
 * linux/arch/arm/mach-ezx/pm.c
 * 
 * EZX Specific PXA250/210 Power Management Routines
 *
 * Original code for the SA11x0:
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * Modified for the PXA250 by Nicolas Pitre:
 * Copyright (c) 2002 Monta Vista Software, Inc.
 *
 * Based on original code from linux/arch/arm/mach-pxa/pm.c,
 * Modified for the EZX Development Plaform by Motorola:
 * Copyright (c) 2002-2005 Motorola
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * 2002        - (Motorola) Created based on mach-pxa/pm.c
 * 2004-Aug-30 - (Motorola) Intel pm workaround2
 * 
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/errno.h>
#include <linux/timex.h>
#ifdef CONFIG_DPM
#include <linux/device.h>
#endif

#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/leds.h>


/*
 * Debug macros
 */
#undef DEBUG
#define INTEL_PM_91M_WORKAROUND

extern void pxa_cpu_suspend(unsigned int);
extern void pxa_cpu_resume(void);
extern void (*pm_idle)(void);

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

	SLEEP_SAVE_PGSR0, SLEEP_SAVE_PGSR1, SLEEP_SAVE_PGSR2, SLEEP_SAVE_PGSR3,

	SLEEP_SAVE_GPDR0, SLEEP_SAVE_GPDR1, SLEEP_SAVE_GPDR2, SLEEP_SAVE_GPDR3,
	SLEEP_SAVE_GRER0, SLEEP_SAVE_GRER1, SLEEP_SAVE_GRER2, SLEEP_SAVE_GRER3,
	SLEEP_SAVE_GFER0, SLEEP_SAVE_GFER1, SLEEP_SAVE_GFER2, SLEEP_SAVE_GFER3,
	SLEEP_SAVE_GAFR0_L, SLEEP_SAVE_GAFR1_L, SLEEP_SAVE_GAFR2_L, SLEEP_SAVE_GAFR3_L,
	SLEEP_SAVE_GAFR0_U, SLEEP_SAVE_GAFR1_U, SLEEP_SAVE_GAFR2_U, SLEEP_SAVE_GAFR3_U,

	SLEEP_SAVE_FFIER, SLEEP_SAVE_FFLCR, SLEEP_SAVE_FFMCR,
	SLEEP_SAVE_FFSPR, SLEEP_SAVE_FFISR,
	SLEEP_SAVE_FFDLL, SLEEP_SAVE_FFDLH,

	SLEEP_SAVE_STIER, SLEEP_SAVE_STLCR, SLEEP_SAVE_STMCR,
	SLEEP_SAVE_STSPR, SLEEP_SAVE_STISR,
	SLEEP_SAVE_STDLL, SLEEP_SAVE_STDLH,

	SLEEP_SAVE_ICMR,
	SLEEP_SAVE_CKEN,

	SLEEP_SAVE_CKSUM,

	SLEEP_SAVE_SIZE
};

int	pm_do_standby()
{
	unsigned long unused;
	unsigned long	pssr, gpdr;

	cli();
	clf();

	printk(KERN_INFO "In function %s.\n", __FUNCTION__);

	/*	This also works fine by useing SW12 to wake it up.*/	
	/*	Set PSTR.	*/
	PSTR = 0x4;
	/*	Copied from PalmOS Group.	*/
	PWER = 0x80000002;
	PRER = 0x2;
	PFER = 0x2;
	PKWR = 0x000E0000;
	/*	Copied from PalmOS Group end.*/
	
#if 1
	/*	
	 *	Use below's config it will return back from
	 *	standby mode automatically.
	 */

	/*	Set PSTR.	*/
	PSTR = 0x4;
	/*	Program PWER, PKWR, PRER and PFER	*/
	/*	All can wake it up.	*/
	PWER = 0xCE00FE1F;	
	/*	
	 *	No high level on GPIO can wake up it.
 	 *	If allow high level on GPIO to wake it up,
 	 *	set PKWR to 0x000FFFFF 
 	 */
	PKWR = 0x0;			
	/*	Rising edge can wake it up.	*/	
	PRER = 0xFE1B;
	/*	Falling edge can wake it up.*/
	PFER = 0x0FE1B;
#endif	

	/*	Standby the CPU. */
	__asm__ __volatile__("\n\
\n\
\n\
   	ldr r0, 	[%1] \n\
    mov r1, 	#0x18 \n\
	@ orr	r1,	r0,	#0x08 \n\
    mov r2, 	#2      \n\
    b   1f \n\
\n\
   	.align  5 \n\
1: \n\
   	@ All needed values are now in registers. \n\
    @ These last instructions should be in cache \n\
    @ enter standby mode \n\
\n\
   	mcr p14, 0, r2, c7, c0, 0 \n\
\n\
    @ CPU will stop exec until be waked up. \n\
   	@ After be waken, just return. \n\
	str r1, [%1] \n\
	.rept 11 \n\
	nop \n\
	.endr"

	: "=&r" (unused)
	: "r" (&PSSR)
	: "r0", "r1", "r2");

	printk(KERN_INFO "Return from standby mode.\n");

	sti();
	return 0;
}

int pm_do_suspend(unsigned int mode)
{
	unsigned long sleep_save[SLEEP_SAVE_SIZE];
	unsigned long checksum = 0;
	int i;

	cli();
	clf();

	/* preserve current time */
	RCNR = xtime.tv_sec;

	/* add by Motorola for restore usbh gpio */ 
	GPSR(90) = GPIO_bit(90);	
	GPSR(91) = GPIO_bit(91);
	GPCR(113) = GPIO_bit(113);

	set_GPIO_mode( 90 | GPIO_OUT );
	set_GPIO_mode( 91 | GPIO_OUT );
	set_GPIO_mode(113 | GPIO_OUT );
	/* end Motorola */

	/* 
	 * Temporary solution.  This won't be necessary once
	 * we move pxa support into the serial driver
	 * Save the FF UART 
	 */

	SAVE(FFIER);
	SAVE(FFLCR);
	SAVE(FFMCR);
	SAVE(FFSPR);
	SAVE(FFISR);
	FFLCR |= 0x80;
	SAVE(FFDLL);
	SAVE(FFDLH);
	FFLCR &= 0xef;

	SAVE(STIER);
	SAVE(STLCR);
	SAVE(STMCR);
	SAVE(STSPR);
	SAVE(STISR);
	STLCR |= 0x80;
	SAVE(STDLL);
	SAVE(STDLH);
	STLCR &= 0xef;

	/* save vital registers */
	SAVE(OSCR);
	SAVE(OSMR0);
	SAVE(OSMR1);
	SAVE(OSMR2);
	SAVE(OSMR3);
	SAVE(OIER);

	SAVE(PGSR0); SAVE(PGSR1); SAVE(PGSR2); SAVE(PGSR3);

	SAVE(GPDR0); SAVE(GPDR1); SAVE(GPDR2); SAVE(GPDR3);
	SAVE(GRER0); SAVE(GRER1); SAVE(GRER2); SAVE(GRER3);
	SAVE(GFER0); SAVE(GFER1); SAVE(GFER2); SAVE(GFER3);
	SAVE(GAFR0_L); SAVE(GAFR0_U); 
	SAVE(GAFR1_L); SAVE(GAFR1_U);
	SAVE(GAFR2_L); SAVE(GAFR2_U);
	SAVE(GAFR3_L); SAVE(GAFR3_U);

	SAVE(ICMR);
	ICMR = 0;

	SAVE(CKEN);
	
	CKEN = CKEN22_MEMC;

	/* Note: wake up source are set up in each machine specific files */

	/* clear GPIO transition detect  bits */
	GEDR0 = GEDR0; GEDR1 = GEDR1; GEDR2 = GEDR2; GEDR3 = GEDR3;

	/* Clear sleep reset status */
	RCSR = RCSR_SMR;

	/* set resume return address */
        *(unsigned long *)(phys_to_virt(RESUME_ADDR)) = virt_to_phys(pxa_cpu_resume);
        *(unsigned long *)(phys_to_virt(FLAG_ADDR)) = SLEEP_FLAG;

	/* before sleeping, calculate and save a checksum */
	for (i = 0; i < SLEEP_SAVE_SIZE - 1; i++)
		checksum += sleep_save[i];
	sleep_save[SLEEP_SAVE_CKSUM] = checksum;

	#ifdef INTEL_PM_91M_WORKAROUND
	/*modify MDREFR to meet the requirement of frequency change*/
	MDREFR &= 0xFFFFF000;
	MDREFR |= 0x00000015;
//	MDREFR &= 0xDFF5BFFF;
	MDREFR |= 0x000a4000;
	MDREFR |= 0x20000000;
	
	/*change bulverde from run mode to 91M mode*/
	__asm__ (
		"mov r0, #2\n"
	        "mcr p14, 0, r0, c6, c0, 0\n"
		"\n"   //modify MDREFR register
	        "ldr r2, =0x107\n"
	        "str r2, [%0]\n"
	        "ldr r2, [%0]\n"
	        "mcr p14, 0, r0, c6, c0, 0\n"
		::"r" (&CCCR): "r0", "r2"
		);
	#endif
	//change to run mod
	#if 0
	__asm__ (
		"mov r0, #2\n"
	        "mcr p14, 0, r0, c6, c0, 0\n"
		);
	/*end of mode change*/
        /*generate pulse with 10ms width on signal TC_MM_EN to BP*/
	set_GPIO_mode(GPIO_TC_MM_EN);	
	GPDR(GPIO_TC_MM_EN)   |= GPIO_bit(GPIO_TC_MM_EN); 
	GPSR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);  //output 1
        GPCR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);   //output 0
	udelay(500 * 3);
	GPSR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN); //output 1
	printk("sending TC_MM_EN 0.5ms pulse to BP\n");
	mdelay(10 * 3);
	#endif

	/* *** go zzz *** */
	pxa_cpu_suspend(mode);

	/* after sleeping, validate the checksum */
	checksum = 0;
	for (i = 0; i < SLEEP_SAVE_SIZE - 1; i++)
		checksum += sleep_save[i];

	/* if invalid, display message and wait for a hardware reset */
	if (checksum != sleep_save[SLEEP_SAVE_CKSUM]) {
		while (1);
	}

	/* ensure not to come back here if it wasn't intended */
        *(unsigned long *)(phys_to_virt(RESUME_ADDR)) = 0;
        *(unsigned long *)(phys_to_virt(FLAG_ADDR)) = OFF_FLAG;

	/* add by Motorola for restore usbh gpio */ 
	GPSR(30) = GPIO_bit(30);	
	GPCR(31) = GPIO_bit(31);	
	GPCR(56) = GPIO_bit(56);	
	GPSR(90) = GPIO_bit(90);	
	GPSR(91) = GPIO_bit(91);	
	GPCR(113) = GPIO_bit(113);
	/* end Motorola */

	/* restore registers */
	RESTORE(PGSR0); RESTORE(PGSR1); RESTORE(PGSR2); RESTORE(PGSR3);

	RESTORE(GPDR0); RESTORE(GPDR1); RESTORE(GPDR2); RESTORE(GPDR3);
	RESTORE(GRER0); RESTORE(GRER1); RESTORE(GRER2); RESTORE(GRER3);
	RESTORE(GFER0); RESTORE(GFER1); RESTORE(GFER2); RESTORE(GFER3);

	RESTORE(GAFR0_L); RESTORE(GAFR0_U);
	RESTORE(GAFR1_L); RESTORE(GAFR1_U);
	RESTORE(GAFR2_L); RESTORE(GAFR2_U);
	RESTORE(GAFR3_L); RESTORE(GAFR3_U);

	//PSSR = PSSR_PH;
	PSSR = PSSR_PH | PSSR_RDH;
	
	RESTORE(OSMR0);
	RESTORE(OSMR1);
	RESTORE(OSMR2);
	RESTORE(OSMR3);

	// to avoid os timer interrupt lost
	//RESTORE(OSCR);
	OSCR = OSMR0 - LATCH;
	RESTORE(OIER);

	RESTORE(CKEN);

	ICLR = 0;
	ICCR = 1;
	RESTORE(ICMR);

	/* 
	 * Temporary solution.  This won't be necessary once
	 * we move pxa support into the serial driver.
	 * Restore the FF UART.
	 */
#if 10
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

	RESTORE(STMCR);
	RESTORE(STSPR);
	RESTORE(STLCR);
	STLCR |= 0x80;
	RESTORE(STDLH);
	RESTORE(STDLL);
	RESTORE(STLCR);
	RESTORE(STISR);
	STFCR = 0x07;
	RESTORE(STIER);
#endif
	/* restore current time */
	xtime.tv_sec = RCNR;

#ifdef DEBUG
	printk(KERN_DEBUG "*** made it back from resume\n");
#endif

	return 0;
}

unsigned long sleep_phys_sp(void *sp)
{
	return virt_to_phys(sp);
}

#ifdef CONFIG_SYSCTL
/*
 * ARGH!  ACPI people defined CTL_ACPI in linux/acpi.h rather than
 * linux/sysctl.h.
 *
 * This means our interface here won't survive long - it needs a new
 * interface.  Quick hack to get this working - use sysctl id 9999.
 */
#warning ACPI broke the kernel, this interface needs to be fixed up.
#define CTL_ACPI 9999
#define ACPI_S1_SLP_TYP 19

/*
 * Send us to sleep.
 */
/*	New added function for different CPU mode. 	*/
int  m13_enable = 0;
int cpu_mode_set(int mode)
{
	/*  Need further work to support different CPU modes.   */

	if( mode==CPUMODE_STANDBY )
		pm_do_standby();
	else if (mode==CPUMODE_DEEPSLEEP)
		pm_do_suspend(mode);
	else pm_do_suspend(CPUMODE_SLEEP);

	return 0;
}

static int
sysctl_pm_do_suspend(ctl_table *ctl, int write, struct file *filp,
        void *buffer, size_t *lenp)
{
    char buf[16], *p;
    int cpu = 0, len, left = *lenp;
   	int retval;

    if (!left || (filp->f_pos && !write)) {
        *lenp = 0;
        return 0;
    }

    if (write) {
       unsigned int mode;

        len = left;
        if (left > sizeof(buf))
            left = sizeof(buf);
        if (copy_from_user(buf, buffer, left))
            return -EFAULT;
        buf[sizeof(buf) - 1] = '\0';

        mode = simple_strtoul(buf, &p, 0);

       if( (mode==CPUMODE_SLEEP) || (mode==CPUMODE_DEEPSLEEP) ||
			(mode ==CPUMODE_STANDBY )||(mode ==0 ) )  { // for Sleep.
#ifdef CONFIG_DPM
	   device_suspend(0, SUSPEND_POWER_DOWN);
	   retval = 0;
#else /* CONFIG_DPM */
           retval = pm_send_all(PM_SUSPEND, (void *)3);
#endif
           if (retval == 0) {
               retval = cpu_mode_set(mode);
#ifdef CONFIG_DPM
	       device_resume(RESUME_POWER_ON);
#else /* CONFIG_DPM */
               pm_send_all(PM_RESUME, (void *)0);
#endif
           }
       }  
	   else {
           /*
            *  for modes that except RUN, SLEEP and DEEPSLEEP.
            *  Need not call pm_send_all to send out notifications?
            *  Maybe it is also needed, the most problem is,no device driver
            *  will reponse to PM_SENSE, PM_STANDBY, etc. so we need to
            *  change those device drivers on by one.  :(
            */
           retval = cpu_mode_set(mode);
       }
   } else {
        len = sprintf(buf, "%d\n", CPUMODE_RUN);
        if (len > left)
            len = left;
        if (copy_to_user(buffer, buf, len))
            return -EFAULT;
    }

    *lenp = len;
    filp->f_pos += len;
    return 0;
}

static int
sysctl_pm_do_suspend_sysctl(ctl_table *table, int *name, int nlen,
           void *oldval, size_t *oldlenp,
           void *newval, size_t newlen, void **context)
{
    int cpu = 0;

    if (oldval && oldlenp) {
        size_t oldlen;

        if (get_user(oldlen, oldlenp))
            return -EFAULT;

        if (oldlen != sizeof(unsigned int))
            return -EINVAL;
        if (put_user(CPUMODE_RUN, (unsigned int *)oldval) ||
            put_user(sizeof(unsigned int), oldlenp))
            return -EFAULT;
    }
    if (newval && newlen) {
        unsigned int mode;

        if (newlen != sizeof(unsigned int))
            return -EINVAL;

        if (get_user(mode, (unsigned int *)newval))
            return -EFAULT;

        cpu_mode_set(mode);
    }
    return 1;
}


static struct ctl_table pm_table[] =
{
	{
        ctl_name:   ACPI_S1_SLP_TYP,
        procname:   "suspend",
        mode:       0644,
        proc_handler:   sysctl_pm_do_suspend,
        strategy:   sysctl_pm_do_suspend_sysctl,
	},
	{ACPI_S1_SLP_TYP, "oscc", &OSCC, sizeof(int), 0644, NULL, (proc_handler *)&proc_dointvec},
	{0}
};

static struct ctl_table pm_dir_table[] =
{
	{CTL_ACPI, "pm", NULL, 0, 0555, pm_table},
	{0}
};

/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	register_sysctl_table(pm_dir_table, 1);
	return 0;
}

__initcall(pm_init);

#endif
