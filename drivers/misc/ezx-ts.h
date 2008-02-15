/*
 *  linux/drivers/char/ezx-ts.c
 *
 *  Copyright (C) 2002 Lili Jiang
 * 
 *
 *  June, 2002 Lili Jiang, create
 */

#ifndef _EZXTS_H
#define _EZXTS_H

#define EZX_TS_MINOR_ID 14

#define PEN_UP 0
#define PEN_DOWN 0xffff

/* extern functio nfor PCAP */
void ezx_ts_touch_interrupt(int irq, void* dev_id, struct pt_regs *regs);
void ezx_ts_dataReadok_interrupt(int irq, void* dev_id, struct pt_regs *regs);

#endif //_EZXTS_H
