/*
 * include/asm-arm/arch-omap/preem_latency.h
 * timing support for preempt-stats patch
 */

#include "hardware.h"
#include <asm/arch/ck.h>

extern unsigned int read_mputimer1(void);

#define readclock_init()
#define readclock(x)		x = read_mputimer1()
#define clock_to_usecs(x)	((x) / 6)

#define PSR_F_BIT               0x40    /* FIQ unmask/mask (0/1) */
#define PSR_I_BIT               0x80    /* IRQ unmask/mask (0/1) */
#define INTERRUPTS_ENABLED(x)   (!(x & PSR_I_BIT))
/* mputimer 1 runs @ 6Mhz  6 ticks = 1 microsecond */
#define TICKS_PER_USEC		6

