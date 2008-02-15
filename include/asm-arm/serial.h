/*
 *  linux/include/asm-arm/serial.h
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   15-10-1996	RMK	Created
 */

#ifndef __ASM_SERIAL_H
#define __ASM_SERIAL_H

#include <asm/arch/serial.h>

#define SERIAL_PORT_DFNS		\
	STD_SERIAL_PORT_DEFNS		\
	EXTRA_SERIAL_PORT_DEFNS

extern void ffuart_gpio_init(void);
extern int stop_ffuart(void);

typedef enum {
	DC_DISABLE,
	DC_ENABLE,
	DC_STANDBY
} DC_state;

extern DC_state dc_state;

extern int dc_disable(void);
extern int dc_enable(void);
extern int dc_standby(void);

#endif
