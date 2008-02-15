#ifndef _LINUX_PM_DEV_H
#define _LINUX_PM_DEV_H

/*
 * Copyright 2002 Montavista Software (mlocke@mvista.com)
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
 */



/*
 * Device types
 */
enum
{
        PM_UNKNOWN_DEV = 0, /* generic */
        PM_SYS_DEV,         /* system device (fan, KB controller, ...) */
        PM_PCI_DEV,         /* PCI device */
        PM_USB_DEV,         /* USB device */
        PM_SCSI_DEV,        /* SCSI device */
        PM_ISA_DEV,         /* ISA device */
        PM_MTD_DEV,         /* Memory Technology Device */
	PM_TPANEL_DEV,      /* Touch Panel Device */
        PM_STORAGE_DEV,     /* Storage Device */
        PM_NETWORK_DEV,     /* Network Device */
        PM_PCMCIA_DEV,      /* PCMCIA Device */
        PM_DISPLAY_DEV,     /* Display Device */
        PM_SERIAL_DEV,      /* Serial Device */
        PM_BATTERY_DEV,     /* Battery Device */
};

#endif
