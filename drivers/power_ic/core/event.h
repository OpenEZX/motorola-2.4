/*
 * Copyright 2004 Freescale Semiconductor, Inc.
 * Copyright 2004-2006 Motorola, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 *  Author     Date                Comment
 * ======== ===========   =====================================================
 * Motorola 2006-Oct-25 - Update comments.
 * Motorola 2006-Apr-20 - Remove IRQ defines.
 * Motorola 2005-Mar-15 - Fix Doxygen documentation.
 * Motorola 2004-Dec-06 - Redesign of the Power IC event handler.
 */

#ifndef __EVENT_H__
#define __EVENT_H__

/*!
 * @file event.h
 *
 * @brief This file contains prototypes for power IC core-internal event handling functions.
 *
 * @ingroup poweric_core
 */ 

#include <linux/power_ic_kernel.h>
#include <linux/version.h>

/* These definitions regarding interrupt handling are only available in linux 2.4.23 and after... */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23))
typedef void irqreturn_t;
#define IRQ_NONE
#define IRQ_HANDLED
#define IRQ_RETVAL(x)
#endif
#endif

#ifdef CONFIG_MOT_POWER_IC_PCAP2
#define POWER_IC_EVENT_NUM_EVENTS POWER_IC_EVENT_NUM_EVENTS_PCAP
#elif defined(CONFIG_MOT_POWER_IC_ATLAS)
#define POWER_IC_EVENT_NUM_EVENTS POWER_IC_EVENT_NUM_EVENTS_ATLAS
#endif

void power_ic_event_initialize (void);

#endif /* __EVENT_H__ */
