/*
 *  linux/arch/arm/mach-ezx/a780.c
 *
 *  Support for the Motorola Ezx A780 Development Platform.
 *  
 *  Author:	Zhuang Xiaofan
 *  Created:	Nov 25, 2003
 *  Copyright:	Motorola Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/bitops.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/irq.h>

#include "generic.h"
#include <linux/tty.h>
#include <linux/apm_bios.h>

#define FIRST_STEP              2
#define LAST_STEP               3
#define BP_RDY_TIMEOUT          0x000c0000

extern void usb_send_readurb();
extern void pm_do_poweroff();

/* check power down condition */
inline void check_power_off()
{
        if (!(GPIO_is_high(GPIO_BB_WDI2))) {
                  pm_do_poweroff();
        }
}

static int step = FIRST_STEP;
void handshake()
{
        /* step 1: check MCU_INT_SW or BP_RDY is low (now it is checked in apboot) */
        if (step == 1) {
                int timeout = BP_RDY_TIMEOUT;

                /* config MCU_INT_SW, BP_RDY as input */
                GPDR(GPIO_MCU_INT_SW) &= ~GPIO_bit(GPIO_MCU_INT_SW);
                GPDR(GPIO_BP_RDY) &= ~GPIO_bit(GPIO_BP_RDY);

                while ( timeout -- ) {
                        if ( (!(GPIO_is_high(GPIO_MCU_INT_SW)))
                                || (!(GPIO_is_high(GPIO_BP_RDY))) ) {
                                step ++;
                                break;
                        }

                        check_power_off();
                }
        }

        /* step 2: wait BP_RDY is low */
        if (step == 2) {
                if (!(GPIO_is_high(GPIO_BP_RDY))) {

                        /* config MCU_INT_SW as output */
                        set_GPIO_mode(GPIO_MCU_INT_SW | GPIO_OUT);
                        clr_GPIO(GPIO_MCU_INT_SW);

                        step ++;
                }
        }

        /* step 3: wait BP_RDY is high */
        if (step == 3) {
                if (GPIO_is_high(GPIO_BP_RDY)) {
                        step ++;
                        delay_bklight();
                        set_GPIO(GPIO_MCU_INT_SW);
                }
        }
}

#ifdef CONFIG_APM
int pm_handle_irq(int irq)
{

        extern unsigned long idle_limit;
        extern int can_idle, can_sleep;
        static unsigned long tmp_jiffy;         /* for temporary store of jiffies */

        /*
         * if idle_limit is zero, never enter idle.
         * if not OS timer, reset idle timer count
         */
        if (idle_limit == 0) {
                tmp_jiffy = jiffies;
                return irq;
        }

        if (irq != IRQ_OST0) {
                tmp_jiffy = jiffies;
                can_idle = 0;
                can_sleep = 0;
        } else if (jiffies > tmp_jiffy + idle_limit) {

                /*
                 * I think this is enough to prevent from reentering here
                 * due to jiffies will be stoped
                 */
                tmp_jiffy = jiffies;

                /* if pm idle timer expired, queue event  */
                queue_apm_event(KRNL_PROC_INACT, NULL);
                can_idle = 1;
        }

        return irq;
}

void bp_wdi_intr(int irq, void *dev_id, struct pt_regs *regs)
{
        queue_apm_event(KRNL_BP_WDI, NULL);
}

static struct irqaction bp_wdi_irq = {
        name:           "BP wdi",
        handler:        bp_wdi_intr
};
#endif

int handshake_pass()
{
        return (step > LAST_STEP);
}

static void bp_rdy_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	static int usbipc_ready = 0;

	if (!usbipc_ready) {
		handshake();
	        if (handshake_pass()) {
	                disable_irq(IRQ_GPIO(GPIO_BB_WDI2));

			/* set bp_rdy handle for usb ipc */
			set_GPIO_IRQ_edge(GPIO_BP_RDY, GPIO_FALLING_EDGE);
			usbipc_ready = 1;
       		}
	} else 
		usb_send_readurb();
}

static struct irqaction bp_rdy_irq = {
        name:           "BP rdy",
        handler:        bp_rdy_intr
};

static void bp_wdi2_intr(int irq, void *dev_id, struct pt_regs *regs)
{
        pm_do_poweroff();
}

static struct irqaction bp_wdi2_irq = {
        name:           "BP wdi2",
        handler:        bp_wdi2_intr
};



static void __init ezx_init_gpio_irq(void)
{
#ifdef CONFIG_APM
        set_GPIO_IRQ_edge(GPIO_BB_WDI, GPIO_FALLING_EDGE);
        setup_arm_irq(IRQ_GPIO(GPIO_BB_WDI), &bp_wdi_irq);
#endif
        set_GPIO_IRQ_edge(GPIO_BP_RDY, GPIO_BOTH_EDGES);
        setup_arm_irq(IRQ_GPIO(GPIO_BP_RDY), &bp_rdy_irq);

        set_GPIO_IRQ_edge(GPIO_BB_WDI2, GPIO_FALLING_EDGE);
        setup_arm_irq(IRQ_GPIO(GPIO_BB_WDI2), &bp_wdi2_irq);
}

static void __init a780_init_irq(void)
{
	int irq;
	
	pxa_init_irq();

        /* init ezx specfic gpio irq */
        ezx_init_gpio_irq();

        check_power_off();
        handshake();
        if (handshake_pass()) {
                disable_irq(IRQ_GPIO(GPIO_BP_RDY));
                disable_irq(IRQ_GPIO(GPIO_BB_WDI2));
        }
}

static void __init
fixup_a780(struct machine_desc *desc, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
	ORIG_VIDEO_COLS = 30;
	ORIG_VIDEO_LINES = 80;
}

static void __init a780_map_io(void)
{
	pxa_map_io();

	CKEN = CKEN9_OSTIMER | CKEN22_MEMC;

        /*  set BB_RESET PIN out put high */
        set_GPIO_mode(GPIO_BB_RESET|GPIO_OUT);
        set_GPIO(GPIO_BB_RESET);
        
	set_GPIO_mode(GPIO_ICL_FFRXD_MD);
	set_GPIO_mode(GPIO_ICL_FFTXD_MD);
	set_GPIO_mode(GPIO_ICL_FFCTS_MD);
	set_GPIO_mode(GPIO_ICL_FFRTS_MD);

	set_GPIO_mode(GPIO42_BTRXD_MD);
	set_GPIO_mode(GPIO43_BTTXD_MD);
	set_GPIO_mode(GPIO44_BTCTS_MD);
	set_GPIO_mode(GPIO45_BTRTS_MD);

        /* clear EMU MUX1/MUX2 (low) to close the audio path to EMU */
	set_GPIO_mode(GPIO_EMU_MUX1|GPIO_OUT);
        clr_GPIO(GPIO_EMU_MUX1);
	set_GPIO_mode(GPIO_EMU_MUX2|GPIO_OUT);
        clr_GPIO(GPIO_EMU_MUX2);

#if defined(CONFIG_ARCH_EZX_E680)
	set_GPIO_mode(GPIO46_STRXD_MD);
	set_GPIO_mode(GPIO47_STTXD_MD);

	/* setup sleep mode values */
	PWER  = 0xc000f803;		// disable usb 0xdc00f803;
	PFER  = 0x0000f803;
	PRER  = 0x00001802;
	// keypad wakeup (PKWR,PGSR3) should be in keypad.c		
	PGSR0 = 0x00000010;
	PGSR1 = 0x02800000;
	PGSR2 = 0x00040000;
	PGSR3 = 0x00000000;
	PCFR  = PCFR_DC_EN | PCFR_FS | PCFR_FP | PCFR_OPDE;
	PSLR  = 0x05800f00;

#elif defined(CONFIG_ARCH_EZX_A780)

	set_GPIO_mode(GPIO46_STRXD_MD);
	set_GPIO_mode(GPIO47_STTXD_MD);

	/* setup sleep mode values */
	PWER  = 0xc0007803;		// disable usb, GPIO15 NC
	PFER  = 0x00007803;
	PRER  = 0x00001802;
	// keypad wakeup (PKWR,PGSR3) should be in keypad.c		
	PGSR0 = 0x00000010;
	PGSR1 = 0x02800000;
	PGSR2 = 0x00040000;
	PGSR3 = 0x00000008;
	PCFR  = PCFR_DC_EN | PCFR_FS | PCFR_FP | PCFR_OPDE;
	PSLR  = 0x05800f00;

#endif

}

MACHINE_START(EZX, "Motorola Ezx Platform")
	MAINTAINER("Motorola Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
	BOOT_PARAMS(0xa0000100)
	FIXUP(fixup_a780)
	MAPIO(a780_map_io)
	INITIRQ(a780_init_irq)
MACHINE_END
