/*
 * File: sdioioctl.h
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the license.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
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
