#ifndef _LINUX_APM_H
#define _LINUX_APM_H

/*
 * Include file for the interface to an APM BIOS
 * Copyright 1994-2001 Stephen Rothwell (sfr@canb.auug.org.au)
 *
 * Copyright (C) 2005 Motorola
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * 2005-Jan-5 - (Motorola) Added ioctls, definitions, and apm changes for Motorola specific platform
 * 
 */

/* #include <linux/pm-devices.h> */

typedef unsigned short	apm_event_t;
typedef unsigned short	apm_eventinfo_t;

#ifdef __KERNEL__

#define APM_40		0x40
#define APM_CS		(APM_40 + 8)
#define APM_CS_16	(APM_CS + 8)
#define APM_DS		(APM_CS_16 + 8)

struct apm_bios_info {
	unsigned short	version;
	unsigned short	cseg;
	unsigned long	offset;
	unsigned short	cseg_16;
	unsigned short	dseg;
	unsigned short	flags;
	unsigned short	cseg_len;
	unsigned short	cseg_16_len;
	unsigned short	dseg_len;
};

/* Results of APM Installation Check */
#define APM_16_BIT_SUPPORT	0x0001
#define APM_32_BIT_SUPPORT	0x0002
#define APM_IDLE_SLOWS_CLOCK	0x0004
#define APM_BIOS_DISABLED      	0x0008
#define APM_BIOS_DISENGAGED     0x0010

/*
 * Data for APM that is persistant across module unload/load
 */
struct apm_info {
	struct apm_bios_info	bios;
	unsigned short		connection_version;
	int			get_power_status_broken;
	int			get_power_status_swabinminutes;
	int			allow_ints;
	int			realmode_power_off;
	int			disabled;
};

/*
 * The APM function codes
 */
#define	APM_FUNC_INST_CHECK	0x5300
#define	APM_FUNC_REAL_CONN	0x5301
#define	APM_FUNC_16BIT_CONN	0x5302
#define	APM_FUNC_32BIT_CONN	0x5303
#define	APM_FUNC_DISCONN	0x5304
#define	APM_FUNC_IDLE		0x5305
#define	APM_FUNC_BUSY		0x5306
#define	APM_FUNC_SET_STATE	0x5307
#define	APM_FUNC_ENABLE_PM	0x5308
#define	APM_FUNC_RESTORE_BIOS	0x5309
#define	APM_FUNC_GET_STATUS	0x530a
#define	APM_FUNC_GET_EVENT	0x530b
#define	APM_FUNC_GET_STATE	0x530c
#define	APM_FUNC_ENABLE_DEV_PM	0x530d
#define	APM_FUNC_VERSION	0x530e
#define	APM_FUNC_ENGAGE_PM	0x530f
#define	APM_FUNC_GET_CAP	0x5310
#define	APM_FUNC_RESUME_TIMER	0x5311
#define	APM_FUNC_RESUME_ON_RING	0x5312
#define	APM_FUNC_TIMER		0x5313

/*
 * Function code for APM_FUNC_RESUME_TIMER
 */
#define	APM_FUNC_DISABLE_TIMER	0
#define	APM_FUNC_GET_TIMER	1
#define	APM_FUNC_SET_TIMER	2

/*
 * Function code for APM_FUNC_RESUME_ON_RING
 */
#define	APM_FUNC_DISABLE_RING	0
#define	APM_FUNC_ENABLE_RING	1
#define	APM_FUNC_GET_RING	2

/*
 * Function code for APM_FUNC_TIMER_STATUS
 */
#define	APM_FUNC_TIMER_DISABLE	0
#define	APM_FUNC_TIMER_ENABLE	1
#define	APM_FUNC_TIMER_GET	2

/*
 * in arch/i386/kernel/setup.c
 */
extern struct apm_info	apm_info;

#endif	/* __KERNEL__ */

/*
 * Power states
 */
#define APM_STATE_READY		0x0000
#define APM_STATE_STANDBY	0x0001
#define APM_STATE_SUSPEND	0x0002
#define APM_STATE_OFF		0x0003
#define APM_STATE_BUSY		0x0004
#define APM_STATE_REJECT	0x0005
#define APM_STATE_OEM_SYS	0x0020
#define APM_STATE_OEM_DEV	0x0040

#define APM_STATE_DISABLE	0x0000
#define APM_STATE_ENABLE	0x0001

#define APM_STATE_DISENGAGE	0x0000
#define APM_STATE_ENGAGE	0x0001

/*
 * Events (results of Get PM Event)
 */
#define APM_SYS_STANDBY		0x0001
#define APM_SYS_SUSPEND		0x0002
#define APM_NORMAL_RESUME	0x0003
#define APM_CRITICAL_RESUME	0x0004
#define APM_LOW_BATTERY		0x0005
#define APM_POWER_STATUS_CHANGE	0x0006
#define APM_UPDATE_TIME		0x0007
#define APM_CRITICAL_SUSPEND	0x0008
#define APM_USER_STANDBY	0x0009
#define APM_USER_SUSPEND	0x000a
#define APM_STANDBY_RESUME	0x000b
#define APM_CAPABILITY_CHANGE	0x000c

/*
 * Error codes
 */
#define APM_SUCCESS		0x00
#define APM_DISABLED		0x01
#define APM_CONNECTED		0x02
#define APM_NOT_CONNECTED	0x03
#define APM_16_CONNECTED	0x05
#define APM_16_UNSUPPORTED	0x06
#define APM_32_CONNECTED	0x07
#define APM_32_UNSUPPORTED	0x08
#define APM_BAD_DEVICE		0x09
#define APM_BAD_PARAM		0x0a
#define APM_NOT_ENGAGED		0x0b
#define APM_BAD_FUNCTION	0x0c
#define APM_RESUME_DISABLED	0x0d
#define APM_NO_ERROR		0x53
#define APM_BAD_STATE		0x60
#define APM_NO_EVENTS		0x80
#define APM_NOT_PRESENT		0x86

/*
 * APM Device IDs
#define APM_DEVICE_BIOS		0x0000
#define APM_DEVICE_ALL		0x0001
#define APM_DEVICE_DISPLAY	0x0100
#define APM_DEVICE_STORAGE	0x0200
#define APM_DEVICE_PARALLEL	0x0300
#define APM_DEVICE_SERIAL	0x0400
#define APM_DEVICE_NETWORK	0x0500
#define APM_DEVICE_PCMCIA	0x0600
#define APM_DEVICE_BATTERY	0x8000
#define APM_DEVICE_OEM		0xe000
#define APM_DEVICE_OLD_ALL	0xffff
#define APM_DEVICE_CLASS	0x00ff
#define APM_DEVICE_MASK		0xff00
 */
/*
 *  APM devices IDs for non-x86
 */
#define APM_DEVICE_ALL		PM_SYS_DEV
#define APM_DEVICE_DISPLAY      PM_DISPLAY_DEV
#define APM_DEVICE_STORAGE	PM_STORAGE_DEV
#define APM_DEVICE_PARALLEL	PM_PARALLEL_DEV
#define APM_DEVICE_SERIAL	PM_SERIAL_DEV
#define APM_DEVICE_NETWORK	PM_NETWORK_DEV
#define APM_DEVICE_PCMCIA	PM_PCMCIA_DEV
#define APM_DEVICE_BATTERY	PM_BATTERY_DEV
#define APM_DEVICE_TPANEL	PM_TPANEL_DEV


#ifdef __KERNEL__
/*
 * This is the "All Devices" ID communicated to the BIOS
 */
#define APM_DEVICE_BALL		((apm_info.connection_version > 0x0100) ? \
				 APM_DEVICE_ALL : APM_DEVICE_OLD_ALL)
#endif

/*
 * Battery status
 */
#define APM_MAX_BATTERIES	2

/*
 * APM defined capability bit flags
 */
#define APM_CAP_GLOBAL_STANDBY		0x0001
#define APM_CAP_GLOBAL_SUSPEND		0x0002
#define APM_CAP_RESUME_STANDBY_TIMER	0x0004 /* Timer resume from standby */
#define APM_CAP_RESUME_SUSPEND_TIMER	0x0008 /* Timer resume from suspend */
#define APM_CAP_RESUME_STANDBY_RING	0x0010 /* Resume on Ring fr standby */
#define APM_CAP_RESUME_SUSPEND_RING	0x0020 /* Resume on Ring fr suspend */
#define APM_CAP_RESUME_STANDBY_PCMCIA	0x0040 /* Resume on PCMCIA Ring	*/
#define APM_CAP_RESUME_SUSPEND_PCMCIA	0x0080 /* Resume on PCMCIA Ring	*/

/*
 * kernel event definitions for our ezx platform
 */
enum
{
	KRNL_ACCS_ATTACH,
	KRNL_ACCS_DETACH,
	KRNL_BLUETOOTH,
	KRNL_TOUCHSCREEN,
	KRNL_KEYPAD,
	KRNL_RTC,
	KRNL_FLIP_ON,
	KRNL_FLIP_OFF,
	KRNL_VOICE_REC,
	KRNL_ICL,
	KRNL_BP,
	KRNL_BP_WDI,
	KRNL_PROC_INACT,
	KRNL_IDLE_TIMEOUT,
	KRNL_SCREEN_LOCK,
	KRNL_SCREEN_UNLOCK
};

/*
 * ioctl operations
 */
#include <linux/ioctl.h>

#define APM_IOC_STANDBY		_IO('A', 1)
#define APM_IOC_SUSPEND		_IO('A', 2)
#define APM_IOC_SET_WAKEUP	_IO('A', 3)

/*
 * Added by Motorola - some new io control commands
 */
#define APM_IOC_IDLE			_IOW('A', 4, int)
#define APM_IOC_SLEEP			_IOW('A', 5, int)
#define APM_IOC_SET_IDLETIMER		_IOW('A', 6, unsigned long)
#define APM_IOC_SET_SLEEPTIMER		_IOW('A', 7, unsigned long)
#define APM_IOC_WAKEUP_ENABLE		_IOW('A', 8, int)
#define APM_IOC_WAKEUP_DISABLE		_IOW('A', 9, int)
#define APM_IOC_POWEROFF		_IO('A', 10)
#define APM_IOC_RESET_BP		_IO('A', 11)
#define APM_IOC_USEROFF_ENABLE		_IOW('A', 12, int)
#define APM_IOC_NOTIFY_BP		_IOW('A', 13, int)
#define APM_IOC_REFLASH                 _IO('A', 14)
#define APM_IOC_PASSTHRU                _IO('A', 15)

/* parameters passed to APM_IOC_NOTIFY_BP in order to signal BP */
#define APM_NOTIFY_BP_QUIET		1
#define APM_NOTIFY_BP_UNHOLD		0

//#define APM_DEBUG

#ifdef APM_DEBUG
#define APM_DPRINTK(format, args...)	printk(format, ##args)
#else
#define APM_DPRINTK(format, args...)
#endif


#if	defined(CONFIG_ARCH_SA1100)
#define APM_AC_OFFLINE 0
#define APM_AC_ONLINE 1
#define APM_AC_BACKUP 2
#define APM_AC_UNKNOWN 0xFF

#define APM_BATTERY_STATUS_HIGH 0
#define APM_BATTERY_STATUS_LOW  1
#define APM_BATTERY_STATUS_CRITICAL 2
#define APM_BATTERY_STATUS_CHARGING 3
#define APM_BATTERY_STATUS_UNKNOWN 0xFF

#define APM_BATTERY_LIFE_UNKNOWN 0xFFFF
#define APM_BATTERY_LIFE_MINUTES 0x8000
#define APM_BATTERY_LIFE_VALUE_MASK 0x7FFF
#endif

/*
 * Maximum number of events stored
 */
#define APM_MAX_EVENTS          20

/*
 * The per-file APM data
 */
struct apm_user {
	int		magic;
	struct apm_user *next;
	int		suser: 1;
	int		suspend_wait: 1;
	int		suspend_result;
	int		suspends_pending;
	int		standbys_pending;
	int		suspends_read;
	int		standbys_read;
	int		event_head;
	int		event_tail;
	apm_event_t	events[APM_MAX_EVENTS];
};

extern void queue_apm_event(apm_event_t event, struct apm_user *sender);

#endif	/* LINUX_APM_H */