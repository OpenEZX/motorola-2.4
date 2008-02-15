/*
 *  linux/drivers/sound/ezx-vibrator.h
 *
 *  Copyright:	BJDC motorola.
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *
 *  History:
 *  zhouqiong          Jun 20,2002             created
 *  Jin Lihong(w20076) Jan 02,2004             (1) Port from UDC e680 kernel of jem vob.
 *                                             (2) reorganize file header
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  reorganise file header
 *
 */

#ifndef EZX_VIBRATOR_H
#define EZX_VIBRATOR_H

#include <linux/ioctl.h>


#define VIBRATOR_MAJOR	108
#define VIBRATOR_IOCTL_BASE	0xbb
#define VIBRATOR_ENABLE		_IOW (VIBRATOR_IOCTL_BASE,1,int)
#define VIBRATOR_DISABLE	_IO (VIBRATOR_IOCTL_BASE,2)


#endif


