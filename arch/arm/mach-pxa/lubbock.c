/*
 *  linux/arch/arm/mach-pxa/lubbock.c
 *
 *  Support for the Intel DBPXA250 Development Platform.
 *  
 *  Author:	Nicolas Pitre
 *  Created:	Jun 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  11/11/2002, zxf
 *	- add function for counting idle and sleep criteria 
 *
 *  04/21/2003, zxf
 *	- add EZX specific gpio interrupt handling
 */
#include <linux/init.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/serial.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/irq.h>

#include "generic.h"

#ifdef CONFIG_SA1111
 #include "sa1111.h"
#endif

#include <linux/apm_bios.h>

int pm_handle_irq(int irq)
{

	extern unsigned long idle_limit;
	extern int can_idle, can_sleep;
	static unsigned long tmp_jiffy;		/* for temporary store of jiffies */
	
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
		APM_DPRINTK("%d clear idle, jiffies %d\n", irq, jiffies);
	} else if (jiffies > tmp_jiffy + idle_limit) {

		/*
		 * I think this is enough to prevent from reentering here 
		 * due to jiffies will be stoped
		 */
		tmp_jiffy = jiffies;

		/* if pm idle timer expired, queue event  */
		queue_apm_event(KRNL_PROC_INACT, NULL);
		can_idle = 1;
		APM_DPRINTK("can idle, jiffies %d\n", jiffies);
	}

	return irq;
}

static unsigned long lubbock_irq_en_mask;

static void lubbock_mask_and_ack_irq(unsigned int irq)
{
	int lubbock_irq = (irq - LUBBOCK_IRQ(0));
	lubbock_irq_en_mask &= ~(1 << lubbock_irq);
	LUB_IRQ_MASK_EN &= ~(1 << lubbock_irq);
	LUB_IRQ_SET_CLR &= ~(1 << lubbock_irq);
}

static void lubbock_mask_irq(unsigned int irq)
{
	int lubbock_irq = (irq - LUBBOCK_IRQ(0));
	lubbock_irq_en_mask &= ~(1 << lubbock_irq);
	LUB_IRQ_MASK_EN &= ~(1 << lubbock_irq);
}

static void lubbock_unmask_irq(unsigned int irq)
{
	int lubbock_irq = (irq - LUBBOCK_IRQ(0));
	lubbock_irq_en_mask |= (1 << lubbock_irq);
	LUB_IRQ_MASK_EN |= (1 << lubbock_irq);
}

void lubbock_irq_demux(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long irq_status;
	int i;

	while ((irq_status = LUB_IRQ_SET_CLR & lubbock_irq_en_mask)) {
		for (i = 0; i < 6; i++) {
			if(irq_status & (1<<i)) 
				do_IRQ(LUBBOCK_IRQ(i), regs);
		}
	}
}

static struct irqaction lubbock_irq = {
	name:		"Lubbock FPGA",
	handler:	lubbock_irq_demux,
	flags:		SA_INTERRUPT
};

void bp_wdi_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	queue_apm_event(KRNL_BP_WDI, NULL);
}

static struct irqaction bp_wdi_irq = {
	name:		"BP wdi",
	handler:	bp_wdi_intr
};

static void __init ezx_init_gpio_irq(void)
{
	set_GPIO_IRQ_edge(GPIO_BB_WDI, GPIO_FALLING_EDGE);
	setup_arm_irq(IRQ_GPIO(GPIO_BB_WDI), &bp_wdi_irq);
}

static void __init lubbock_init_irq(void)
{
	int irq;
	
	pxa_init_irq();

	/* init ezx specfic gpio irq */
	ezx_init_gpio_irq();

	/* setup extra lubbock irqs */
	for(irq = LUBBOCK_IRQ(0); irq <= LUBBOCK_IRQ(5); irq++)
	{
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= lubbock_mask_and_ack_irq;
		irq_desc[irq].mask	= lubbock_mask_irq;
		irq_desc[irq].unmask	= lubbock_unmask_irq;
	}

	set_GPIO_IRQ_edge(GPIO_LUBBOCK_IRQ, GPIO_FALLING_EDGE);
	setup_arm_irq(IRQ_GPIO_LUBBOCK_IRQ, &lubbock_irq);

}

static int __init lubbock_init(void)
{
/*  Disable the SA1111 for Dalhart, Jingze July 22,02 */ 
/*	if (sa1111_probe() < 0)
		return -EINVAL;
	sa1111_wake();
	sa1111_init_irq(LUBBOCK_SA1111_IRQ);*/

	return 0;
}

__initcall(lubbock_init);

static void __init
fixup_lubbock(struct machine_desc *desc, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
	SET_BANK (0, 0xa0000000, 16*1024*1024);
	mi->nr_banks      = 1;
#if 0
	setup_ramdisk (1, 0, 0, 8192);
	setup_initrd (__phys_to_virt(0xa1000000), 4*1024*1024);
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
#endif
}

static struct map_desc lubbock_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xf0000000, 0x08000000, 0x00100000, DOMAIN_IO, 1, 1, 0, 0 }, /* CPLD */
  { 0xf1000000, 0x0c000000, 0x00100000, DOMAIN_IO, 1, 1, 0, 0 }, /* LAN91C96 IO */
  { 0xf1100000, 0x0e000000, 0x00100000, DOMAIN_IO, 1, 1, 0, 0 }, /* LAN91C96 Attr */
  { 0xf4000000, 0x10000000, 0x00400000, DOMAIN_IO, 1, 1, 0, 0 }, /* SA1111 */
  LAST_DESC
};

DC_state dc_state =  DC_DISABLE;

static void __init lubbock_map_io(void)
{
	pxa_map_io();
	iotable_init(lubbock_io_desc);
	
/* add BT module control GPIO pins config default as high output by maddy */
	GPSR0 = 0x000A0000;
	GPDR0 |= 0x000A0000;	

#ifdef	_DEBUG_BOARD
	/* This enables the BTUART */
	CKEN |= CKEN7_BTUART;
	set_GPIO_mode(GPIO42_BTRXD_MD);
	set_GPIO_mode(GPIO43_BTTXD_MD);
	set_GPIO_mode(GPIO44_BTCTS_MD);
	set_GPIO_mode(GPIO45_BTRTS_MD);
#else
	GPSR0 = 0x00000100;
	GPSR1 = 0x00092A80;
	/* This enables the HWUART */
	CKEN |= CKEN4_HWUART;
	set_GPIO_mode(GPIO48_HWTXD_MD);
	set_GPIO_mode(GPIO49_HWRXD_MD);
	set_GPIO_mode(GPIO50_HWCTS_MD);
	set_GPIO_mode(GPIO51_HWRTS_MD);
	
	/* This enables the FFUART */
	CKEN |= CKEN6_FFUART;
	set_GPIO_mode(GPIO34_FFRXD_MD);
	set_GPIO_mode(GPIO9_FFCTS_MD);
	set_GPIO_mode(GPIO36_FFDCD_MD);
	set_GPIO_mode(GPIO37_FFDSR_MD);
	set_GPIO_mode(GPIO38_FFRI_MD);
	set_GPIO_mode(GPIO39_FFTXD_MD);
	set_GPIO_mode(GPIO8_FFDTR_MD);
//	set_GPIO_mode(GPIO35_FFCTS_MD);
	set_GPIO_mode(GPIO41_FFRTS_MD);
	
	/* This enables the BTUART */
	CKEN |= CKEN7_BTUART;
	set_GPIO_mode(GPIO42_BTRXD_MD);
	set_GPIO_mode(GPIO43_BTTXD_MD);
	set_GPIO_mode(GPIO44_BTCTS_MD);
	set_GPIO_mode(GPIO45_BTRTS_MD);
	
	/* This disable DC chipset, IRDA chipset and STUART */
	// disable DC; GPIO35 as low output
	GPCR1 = 0x00000008;
	GPDR1 |= 0x00000008;

#ifdef A768
	// disable IO of DC
	// change GPIO20 to GPIO53
	GPCR1 = 0x00200000;
	GPDR1 |= 0x00200000;
#else
	// disable IO of DC
        GPCR0 = 0x00100000;
        GPDR0 |= 0x00100000;
#endif
	// turn off 3.6M clock
	set_GPIO_mode(GPIO11_INPUT_MD);			

	// turn off STUART
	CKEN &= ~CKEN5_STUART;
	set_GPIO_mode(GPIO46_INPUT_MD);
	set_GPIO_mode(GPIO47_INPUT_MD);	
	
	// disable IRDA by set GPIO21 as high output
	GPSR0 = 0x00200000;
	GPDR0 |= 0x00200000;
	
	if(dc_state !=  DC_DISABLE)
		dc_state =  DC_DISABLE;
#endif

	/* This is for the SMC chip select */
	set_GPIO_mode(GPIO79_nCS_3_MD);

	/* setup sleep mode values */
	PWER  = 0x800064FF; /* the following registers need to double check in the future */
	PFER  = 0x000064FD;
	PRER  = 0x000004C2;
	PGSR0 = 0x486a0000;  // 0x08408000; /* GPIO27 should be 1 when sleep */
	PGSR1 = 0x0009a802;  // 0x00000002;
	PGSR2 = 0x02800000;	/* GPIO89,GPIO87 should be 1 when sleep */
	PCFR |= PCFR_OPDE;
}

MACHINE_START(LUBBOCK, "Intel DBPXA250 Development Platform")
	MAINTAINER("MontaVista Software Inc.")
	BOOT_MEM(0xa0000000, 0x40000000, io_p2v(0x40000000))
	BOOT_PARAMS(0xa0000100)
//	FIXUP(fixup_lubbock)
	MAPIO(lubbock_map_io)
	INITIRQ(lubbock_init_irq)
MACHINE_END




