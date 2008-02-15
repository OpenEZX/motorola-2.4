/*
 * File: include.h
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * (c) Copyright 2006, Motorola, Inc.
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
 *
 * 2006-Jul-07 Motorola - Add power management related changes.
 */

#ifndef _INCLUDE_H_
#define _INCLUDE_H_

#include    "os_headers.h"
#include    "wlan_defs.h"
#include    "wlan_thread.h"
#include    "wlan_types.h"

#ifdef WMM
#include    "wlan_wmm.h"
#endif /* WMM */

#ifdef ENABLE_802_11D
#include    "wlan_11d.h"
#endif /* ENABLE_802_11D */

#ifdef ENABLE_802_11H_TPC
#include    "wlan_11h.h"
#endif /* ENABLE_802_11H_TPC */

#ifdef PASSTHROUGH_MODE
#include    "wlan_passthrough.h"
#endif  /* PASSTHROUGH_MODE */

#ifdef CCX
#include    "wlan_ccx.h"
#endif

#include    "host.h"
#include    "hostcmd.h"

#include    "os_timers.h"
#include    "wlan_dev.h"
#include    "os_macros.h"
#include    "sbi.h"

#ifdef BULVERDE_SDIO
#include    <sdio.h>
#endif /* BULVERDE_SDIO */

#include    "wlan_fw.h"
#include    "wlan_wext.h"
#include    "wlan_decl.h"

#ifdef WPRM_DRV
#include        "wlan_wprm_drv.h"
#endif

#endif /* _INCLUDE_H_ */
