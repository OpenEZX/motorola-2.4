/*
 * include/asm-arm/arch-integrator/preem_latency.h
 *
 * Support for preemptible kernel latency measurement times
 *
 * The readclock() macro must return the contents of a monotonic incremental
 * counter.
 *
 * NOTES:
 *
 * 1. This implementation uses the ARM Integrator TIMER2 exclusively for
 *    preemptible latency measurements. It must not be used for any other
 *    purpose during preemptible kernel latency measurement.
 *
 * 2. This implementation assumes that the TIMER2 clk source is 24MHz.
 *
 * 3. Due to limited range of the timers, it is necessary to make tradeoffs
 *    between min/max latency periods. As currently configured, the min/max
 *    latency which can be measured is approximately 10.67uS/1.4 seconds
 *    respectively. If latency times exceed approx 1.4 seconds, overflow
 *    will occur.
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <asm/hardware.h>

typedef struct itimer {
	volatile u32 load;
	volatile u32 value;
	volatile u32 ctrl;
	volatile u32 clr;
} itimer_t;

#define TIMER2_VA_BASE (IO_ADDRESS(INTEGRATOR_CT_BASE)+0x00000200)

#define readclock_init()	do {					\
	volatile itimer_t *timer =					\
			(itimer_t *)TIMER2_VA_BASE;			\
	timer->ctrl = 0;						\
	timer->load = 0;						\
	timer->clr = 0;							\
	timer->ctrl = 0x88;	/* Enable, Fin / 256 */			\
	} while (0)

#define readclock(x)	do {						\
		/* 32-bit left justify so the math just works. */	\
		x = ~((itimer_t *)TIMER2_VA_BASE)->value << 16;		\
	} while (0)

#define clock_to_usecs(x)	((x >> 8) / 24)	/* Assumes 24Mhz clk */
