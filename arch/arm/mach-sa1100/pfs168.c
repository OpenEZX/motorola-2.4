/*
 * linux/arch/arm/mach-sa1100/pfs168.c
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/pfs168.h>
#include <asm/hardware/sa1111.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/arch/irq.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <linux/serial_core.h>

#include "generic.h"
#include "sa1111.h"


void PFS168_SYSC1RTS_set(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSC1RTS |= bit;
	local_irq_restore(flags);
}

void PFS168_SYSC1RTS_clear(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSC1RTS &= ~bit;
	local_irq_restore(flags);
}

void PFS168_SYSLED_set(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSLED |= bit;
	local_irq_restore(flags);
}

void PFS168_SYSLED_clear(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSLED &= ~bit;
	local_irq_restore(flags);
}

void PFS168_SYSLCDDE_set(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSLCDDE |= bit;
	local_irq_restore(flags);
}

void PFS168_SYSLCDDE_clear(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSLCDDE &= ~bit;
	local_irq_restore(flags);
}

void PFS168_SYSC3TEN_set(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSC3TEN |= bit;
	local_irq_restore(flags);
}

void PFS168_SYSC3TEN_clear(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSC3TEN &= ~bit;
	local_irq_restore(flags);
}

void PFS168_SYSCTLA_set(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSCTLA |= bit;
	local_irq_restore(flags);
}

void PFS168_SYSCTLA_clear(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSCTLA &= ~bit;
	local_irq_restore(flags);
}

void PFS168_SYSCTLB_set(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSCTLB |= bit;
	local_irq_restore(flags);
}

void PFS168_SYSCTLB_clear(uint bit)
{
	unsigned long flags;
	local_irq_save(flags);
	PFS168_SYSCTLB &= ~bit;
	local_irq_restore(flags);
}


EXPORT_SYMBOL(PFS168_SYSC1RTS_set);
EXPORT_SYMBOL(PFS168_SYSC1RTS_clear);
EXPORT_SYMBOL(PFS168_SYSLED_set);
EXPORT_SYMBOL(PFS168_SYSLED_clear);
EXPORT_SYMBOL(PFS168_SYSLCDDE_set);
EXPORT_SYMBOL(PFS168_SYSLCDDE_clear);
EXPORT_SYMBOL(PFS168_SYSC3TEN_set);
EXPORT_SYMBOL(PFS168_SYSC3TEN_clear);
EXPORT_SYMBOL(PFS168_SYSCTLA_set);
EXPORT_SYMBOL(PFS168_SYSCTLA_clear);
EXPORT_SYMBOL(PFS168_SYSCTLB_set);
EXPORT_SYMBOL(PFS168_SYSCTLB_clear);


static void pfs168_backlight_power(int on)
{
	if (on)
		PFS168_SYSCTLA_set(PFS168_SYSCTLA_BKLT);
	else
		PFS168_SYSCTLA_clear(PFS168_SYSCTLA_BKLT);
}

static void pfs168_lcd_power(int on)
{
#if	defined(CONFIG_PFS168_SASTN)
	if (on)
               	PFS168_SYSLCDDE_set(PFS168_SYSLCDDE_STNDE);
	else
		PFS168_SYSLCDDE_clear(PFS168_SYSLCDDE_STNDE);
#endif
}

static int __init pfs168_init(void)
{
	int ret;

	if (!machine_is_pfs168())
		return -EINVAL;

	sa1100fb_lcd_power = pfs168_lcd_power;
	sa1100fb_backlight_power = pfs168_backlight_power;

	/*
	 * Ensure that the memory bus request/grant signals are setup,
	 * and the grant is held in its inactive state
	 */
	sa1110_mb_disable();

	/*
	 * Probe for SA1111.
	 */
	ret = sa1111_probe(PFS168_SA1111_BASE);
	if (ret < 0)
		return ret;

	/*
	 * We found it.  Wake the chip up.
	 */
	sa1111_wake();

	/*
	 * The SDRAM configuration of the SA1110 and the SA1111 must
	 * match.  This is very important to ensure that SA1111 accesses
	 * don't corrupt the SDRAM.  Note that this ungates the SA1111's
	 * MBGNT signal, so we must have called sa1110_mb_disable()
	 * beforehand.
	 */
	sa1111_configure_smc(1,
			     FExtr(MDCNFG, MDCNFG_SA1110_DRAC0),
			     FExtr(MDCNFG, MDCNFG_SA1110_TDL0));

	/*
	 * We only need to turn on DCLK whenever we want to use the
	 * DMA.  It can otherwise be held firmly in the off position.
	 */
	SKPCR |= SKPCR_DCLKEN;

	/*
	 * Enable the SA1110 memory bus request and grant signals.
	 */
	sa1110_mb_enable();

	sa1111_init_irq(IRQ_GPIO25);	/* SA1111 IRQ on GPIO 25 */

	return 0;
}

__initcall(pfs168_init);


static void __init pfs168_init_irq(void)
{
	sa1100_init_irq();

	/*
	 * Need to register these as rising edge interrupts
	 * for standard 16550 serial driver support.
	 */
	set_GPIO_IRQ_edge(GPIO_GPIO(19), GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(GPIO_GPIO(20), GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(GPIO_GPIO(25), GPIO_RISING_EDGE);
	set_GPIO_IRQ_edge(GPIO_UCB1300_IRQ, GPIO_RISING_EDGE);
}

extern void convert_to_tag_list(struct param_struct *params, int mem_init);

static void __init
fixup_pfs168(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	struct tag *t = (struct tag *)params;

	/*
	 * Apparantly bootldr uses a param_struct.  Groan.
	 */
	if (t->hdr.tag != ATAG_CORE)
		convert_to_tag_list(params, 1);

	if (t->hdr.tag != ATAG_CORE) {
		t->hdr.tag = ATAG_CORE;
		t->hdr.size = tag_size(tag_core);
		t->u.core.flags = 0;
		t->u.core.pagesize = PAGE_SIZE;
		t->u.core.rootdev = RAMDISK_MAJOR << 8 | 0;
		t = tag_next(t);

		t->hdr.tag = ATAG_MEM;
		t->hdr.size = tag_size(tag_mem32);
		t->u.mem.start = 0xc0000000;
		t->u.mem.size  = 32 * 1024 * 1024;
		t = tag_next(t);


		t->hdr.tag = ATAG_RAMDISK;
		t->hdr.size = tag_size(tag_ramdisk);
		t->u.ramdisk.flags = 1;
		t->u.ramdisk.size = 8192;
		t->u.ramdisk.start = 0;
		t = tag_next(t);

		t->hdr.tag = ATAG_INITRD;
		t->hdr.size = tag_size(tag_initrd);
		t->u.initrd.start = 0xc0800000;
		t->u.initrd.size = 3 * 1024 * 1024;
		t = tag_next(t);

		t->hdr.tag = ATAG_NONE;
		t->hdr.size = 0;
	}
}

static struct map_desc pfs168_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x10000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* 16C752 DUART port A (COM5) */
  { 0xf0001000, 0x10800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* 16C752 DUART port B (COM6) */
  { 0xf0002000, 0x11000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* COM1 RTS control (SYSC1RTS) */
  { 0xf0003000, 0x11400000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* Status LED control (SYSLED) */
  { 0xf0004000, 0x11800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* DTMF code read (SYSDTMF) */
  { 0xf0005000, 0x11c00000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* LCD configure, enable (SYSLCDDE) */
  { 0xf0006000, 0x12000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* COM1 DSR and motion sense (SYSC1DSR) */
  { 0xf0007000, 0x12800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* COM3 xmit enable (SYSC3TEN) */
  { 0xf0008000, 0x13000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* Control register A (SYSCTLA) */
  { 0xf0009000, 0x13800000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* Control register B (SYSCTLB) */
  { 0xf000a000, 0x18000000, 0x00001000, DOMAIN_IO, 0, 1, 0, 0 }, /* SMC91C96 */
  { 0xf2800000, 0x4b800000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* MQ200 */
  { 0xf4000000, 0x40000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* SA-1111 */
  LAST_DESC
};

static void __init pfs168_map_io(void)
{
	sa1100_map_io();
	iotable_init(pfs168_io_desc);

	sa1100_register_uart(0, 1);
	sa1100_register_uart(1, 3);
}

MACHINE_START(PFS168, "Radisys-Tulsa")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_pfs168)
	MAPIO(pfs168_map_io)
	INITIRQ(pfs168_init_irq)
MACHINE_END
