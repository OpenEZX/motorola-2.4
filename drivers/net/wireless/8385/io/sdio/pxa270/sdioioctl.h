/**
 * Extension of the MMC driver from the Intel/Montavista patch to the
 * ARM/Linux kernel for Linux SDIO bus controller.
 *
 * Copyright (C) 2000-2003, Marvell Semiconductor, Inc. All rights reserved.
 */
/*
 *  linux/include/linux/mmc/ioctl.h
 *
 *  Author:     Vladimir Shebordaev
 *  Copyright:  MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __SDIOIOCTL_H__
#define __SDIOIOCTL_H__

#include <asm/ioctl.h>

#define SDIO_DIRECT_MAJOR   240  /* char device major number */

/* Card controls */
#define IOCSDCTL_INSERT              _IO(SDIO_DIRECT_MAJOR,   11)
#define IOCSDCTL_EJECT               _IO(SDIO_DIRECT_MAJOR,   12)
#define IOCSDCTL_RECYCLE             _IO(SDIO_DIRECT_MAJOR,   13)

/* SDIO register access */
#define IOCSDIO_RREG                _IOR(SDIO_DIRECT_MAJOR,  201,  int)
#define IOCSDIO_WREG                _IOW(SDIO_DIRECT_MAJOR,  202,  int)
/* SDIO memory access */
#define IOCSDIO_RMEM                _IOR(SDIO_DIRECT_MAJOR,  203,  int)
#define IOCSDIO_WMEM                _IOW(SDIO_DIRECT_MAJOR,  204,  int)
/* SDIO CIS access */
#define IOCSSDIO_GET_FIRST_TUPLE    _IOR(SDIO_DIRECT_MAJOR,  205,  int)
#define IOCSSDIO_GET_NEXT_TUPLE     _IOR(SDIO_DIRECT_MAJOR,  206,  int)
#define IOCSSDIO_GET_TUPLE_DATA     _IOR(SDIO_DIRECT_MAJOR,  207,  int)
#define IOCSSDIO_PARSE_TUPLE        _IOR(SDIO_DIRECT_MAJOR,  208,  int)
#define IOCSSDIO_VALIDATE_CIS       _IOR(SDIO_DIRECT_MAJOR,  209,  int)
#define IOCSSDIO_REPLACE_CIS        _IOR(SDIO_DIRECT_MAJOR,  210,  int)

#endif
