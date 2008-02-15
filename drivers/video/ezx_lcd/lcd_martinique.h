/*
 * linux/drivers/video/ezx_lcd/martinique.h
 * Intel Bulverde/PXA250/210 LCD Controller Configuration for Moto MARTINIQUE product
 *
 * Copyright (C) 2004 - Motorola
 *
 * Copyright 2003 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *	   source@mvista.com
 * Copyright (C) 1999 Eric A. Thomas
 * Based on acornfb.c Copyright (C) Russell King.
 *
 * 2001-08-03: Cliff Brake <cbrake@acclent.com>
 *	- ported SA1100 code to PXA
 *  
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  2004/06/28  Susan
 *      - Support Martinique product based on new LCD structure
 */

#ifndef _PXA_FB_MARTINIQUE_H_
#define _PXA_FB_MARTINIQUE_H_

#include <linux/types.h>
#include "72r89403y01.h"
#include "72r88154y01.h"

/* Bulverde LCD controller feature set  */
#define CONFIG_FBBASE_DOUBLE_BUFFERS 1
#define CONFIG_PXAFB_CLI 1
#define CONFIG_OVL2_CHANNEL 1
//for the Martinique P2 debug board 
//#define BARBADOS_P3 1  //For Chris re-work board //

#ifdef CONFIG_FB_PXA_24BPP
#define PXAFB_MAIN_VALID_FB     PXAFB_MAIN_OVL1
#else
#define PXAFB_MAIN_VALID_FB     PXAFB_MAIN_BASE
#endif

#define PXA_DEFAULT_PANEL       PXA_MAIN_PANEL

#define LCD_BUFFER_STRENGTH      0x5

/* Backlight dutycycle settings */
#define BKLIGHT_PRESCALE  1
#define BKLIGHT_PERIOD    64
#define DEFAULT_DUTYCYCLE   33
#define MAX_DUTYCYCLE      (BKLIGHT_PERIOD+1)  //100
#define MIN_DUTYCYCLE      0

/* GPIO setting */
#define GPIO_CLI_RESETB 28


/* Barbados related function prototype */
void pxafb_bklight_turn_on(void *pxafbinfo);
void pxafb_ezx_Backlight_turn_off(void *pxafbinfo);
void pxafb_ezx_Backlight_turn_on(void *pxafbinfo);
void pxafb_setup_gpio(void *pxafbinfo);
void pxafb_enable_controller(void *pxafbinfo);
void pxafb_disable_controller(void *pxafbinfo);
void pxafb_suspend_controller(void *pxafbinfo);
void pxafb_powersave_controller(void *pxafbinfo);

void pxafb_smart_load_init_image(void *fbi);
int pxafb_smart_update(void *fbi);
void pxafb_smart_refresh(void *dummy);
int pxafb_smart_exit_partial_mode(void *pxafbinfo);
#endif /* _PXA_FB_MARTINIQUE_H_ */
