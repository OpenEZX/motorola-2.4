/**
 *	File	: include.h
 *
 * Motorola Confidential Proprietary
 * Copyright (C) Motorola 2005. All rights reserved.
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
