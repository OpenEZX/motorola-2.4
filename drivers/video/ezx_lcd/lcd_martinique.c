/*
 * linux/drivers/video/ezx_lcd/martinique.c
 * Intel Bulverde/PXA250/210 LCD Controller operations for Moto MARTINIQUE product
 * 
 * Copyright (C) 2004-2005 - Motorola
 * 
 * Copyright 2003 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *	   source@mvista.com
 * Copyright (C) 2003 Intel Corporation (yu.tang@intel.com)
 * Copyright (C) 1999 Eric A. Thomas
 * Based on acornfb.c Copyright (C) Russell King.
 *
 * Please direct your questions and comments on this driver to the following
 * email address:
 *
 *	linux-arm-kernel@lists.arm.linux.org.uk
 *
 * Code Status:
 *
 * 2001/08/03: <cbrake@accelent.com>
 *      - Ported from SA1100 to PXA250
 *      - Added Overlay 1 & Overlay2 & Hardware Cursor support
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
 * 2004/06/28  
 *      - Support Martinique product based on new LCD structure
 * 2005/12/12  Wang limei
 *      - Add null CLI paritial mode interface to support building;
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/device.h>

#include <linux/lights_funlights.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include "pxafb.h"
#include "lcd_martinique.h"

//#undef BARBADOS_P3

extern struct pxafb_info *pxafb_main;
extern struct pxafb_info *pxafb_smart;
extern struct global_state pxafb_global_state;

extern struct device pxafb_main_device_ldm;
extern struct device pxafb_smart_device_ldm;

extern int pxafb_smart_send_display_data(struct pxafb_info *fbi);
extern void pxafb_smart_display_off(struct pxafb_info *fbi);
extern void pxafb_smart_send_init_cmd(struct pxafb_info *fbi);
extern void pxafb_smart_display_on(struct pxafb_info *fbi);

void pxafb_bklight_turn_on(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	
	if (fbi == pxafb_main)
	{
		/* Disable CLI backlight, Enable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_DISPLAY_BL,LIGHTS_FL_COLOR_WHITE);
		
		#if (0)
		bklight_value = 0x40;
		power_ic_write_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		DPRINTK("C_ENABLE(MAIN):POWER_IC_REG_FL_LS0(0x%x)\n",bklight_value);
				
		bklight_value = 0x00;
		power_ic_write_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		DPRINTK("C_ENABLE(MAIN):POWER_IC_REG_FL_LS1(0x%x)\n",bklight_value);
		#endif

	    //Adjust brightness -- configure PWM0 register 
	    CKEN |= CKEN0_PWM0;
	    PWM_CTRL0 = BKLIGHT_PRESCALE;
	    PWM_PERVAL0 = BKLIGHT_PERIOD;
	    // gbklightDutyCycle should got from stup file system 
	    PWM_PWDUTY0   = pxafb_global_state.bklight_main_dutycycle;

	    // configure GPIO for PWM0 
	    GPDR0 |= 0x00010000;  //PWM0 is GPIO16
	    set_GPIO_mode(GPIO16_PWM0_MD);

	    pxafb_global_state.main_state |= 0x1; //0b01
	    pxafb_global_state.smart_state &= 0xE; //0b110
	    
	    DPRINTK("\npxafb_bklight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
	if (fbi == pxafb_smart)
	{
		/* Enable CLI backlight, disable MAIN backlight */
		/* Disable CLI backlight, Enable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_CLI_DISPLAY_BL,LIGHTS_FL_COLOR_WHITE);
		
		#if (0)
		bklight_value = 0x01;
		power_ic_write_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		DPRINTK("C_ENABLE(CLI):POWER_IC_REG_FL_LS1(0x%x)\n",bklight_value);
				
		bklight_value = 0x00;
		power_ic_write_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		DPRINTK("C_ENABLE(CLI):POWER_IC_REG_FL_LS0(0x%x)\n",bklight_value);
		#endif
	    // configure PWM0 register 
	    CKEN |= CKEN0_PWM0;
	    PWM_CTRL0 = BKLIGHT_PRESCALE;
	    PWM_PERVAL0 = BKLIGHT_PERIOD;
	    // gbklightDutyCycle should got from stup file system 
	    PWM_PWDUTY0   = pxafb_global_state.bklight_cli_dutycycle;

	    // configure GPIO for PWM0 
	    GPDR0 |= 0x00010000;  //PWM0 is GPIO16
	    set_GPIO_mode(GPIO16_PWM0_MD);

	    pxafb_global_state.smart_state |= 0x1; //0b01
	    pxafb_global_state.main_state &= 0x6; //0b110
	    
	    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
}

void pxafb_ezx_Backlight_turn_on(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;

	down(&pxafb_global_state.g_sem);
	
	if (fbi == pxafb_main)
	{
                /*Check if Main panel has not been enabled, if so, don't turn on its backlight */
                if ( !(pxafb_global_state.main_state & PXAFB_PANEL_ON) )
                {
                        up(&pxafb_global_state.g_sem);
                        return;
                }
		/* Disable CLI backlight, Enable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_DISPLAY_BL,LIGHTS_FL_COLOR_WHITE);
		
		#if (0)
		bklight_value = 0x40;
		power_ic_write_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		DPRINTK("C_ENABLE(MAIN):POWER_IC_REG_FL_LS0(0x%x)\n",bklight_value);
				
		bklight_value = 0x00;
		power_ic_write_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		DPRINTK("C_ENABLE(MAIN):POWER_IC_REG_FL_LS1(0x%x)\n",bklight_value);
		#endif

	    //Adjust brightness -- configure PWM0 register 
	    CKEN |= CKEN0_PWM0;
	    PWM_CTRL0 = BKLIGHT_PRESCALE;
	    PWM_PERVAL0 = BKLIGHT_PERIOD;
	    // gbklightDutyCycle should got from stup file system 
	    PWM_PWDUTY0   = pxafb_global_state.bklight_main_dutycycle;

	    // configure GPIO for PWM0 
	    GPDR0 |= 0x00010000;  //PWM0 is GPIO16
	    set_GPIO_mode(GPIO16_PWM0_MD);

	    pxafb_global_state.main_state |= 0x1; //0b01
	    pxafb_global_state.smart_state &= 0xE; //0b110
	    
	    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
	if (fbi == pxafb_smart)
	{
                /* Check if there is display on smart panel, if not, don't turn on its backlight */
                if ( !(pxafb_global_state.smart_state & PXAFB_DISP_ON) )
                {
                        up(&pxafb_global_state.g_sem);
                        return;
                }
		/* Enable CLI backlight, disable MAIN backlight */
		/* Disable CLI backlight, Enable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_CLI_DISPLAY_BL,LIGHTS_FL_COLOR_WHITE);
		
		#if (0)
		bklight_value = 0x01;
		power_ic_write_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		DPRINTK("C_ENABLE(CLI):POWER_IC_REG_FL_LS1(0x%x)\n",bklight_value);
				
		bklight_value = 0x00;
		power_ic_write_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		DPRINTK("C_ENABLE(CLI):POWER_IC_REG_FL_LS0(0x%x)\n",bklight_value);
		#endif
	    // configure PWM0 register 
	    CKEN |= CKEN0_PWM0;
	    PWM_CTRL0 = BKLIGHT_PRESCALE;
	    PWM_PERVAL0 = BKLIGHT_PERIOD;
	    // gbklightDutyCycle should got from stup file system 
	    PWM_PWDUTY0   = pxafb_global_state.bklight_cli_dutycycle;

	    // configure GPIO for PWM0 
	    GPDR0 |= 0x00010000;  //PWM0 is GPIO16
	    set_GPIO_mode(GPIO16_PWM0_MD);

	    pxafb_global_state.smart_state |= 0x1; //0b01
	    pxafb_global_state.main_state &= 0x6; //0b110
	    
	    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}

	up(&pxafb_global_state.g_sem);
}


void pxafb_ezx_Backlight_turn_off(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	
    DPRINTK("\npxafb_ezx_Backlight_turn_off.\n");

    down(&pxafb_global_state.g_sem);

	/* Don't need to check if the requested backlight is for pxafb_current */
	/* It is possible when main is enabled, cli backlight is still turned on, after a while, cli's backlight will be turned off */
	if (fbi == pxafb_main)
	{
		/* Disable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_DISPLAY_BL,LIGHTS_FL_COLOR_BLACK);
		
		#if (0)
		bklight_value = 0x01;
		power_ic_write_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS1, &bklight_value);
		DPRINTK("C_ENABLE(MAIN):POWER_IC_REG_FL_LS1(0x%x)\n",bklight_value);
		#endif
	    PWM_PWDUTY0 = 0;
	    GAFR0_U &= 0xFFFFFFFC; 
	    GPDR0 &= 0xFFFEFFFF; //PWM0 is GPIO16, set PWM0 as input when turn off bklight
	    CKEN &= ~CKEN0_PWM0;
	    PWM_PWDUTY0	 = 0;
	    //gbklightDutyCycle = MIN_DUTYCYCLE;

	    pxafb_global_state.main_state &= 0x06;//0b110
	    DPRINTK("\n MAIN panel: CKEN: %x", CKEN);
	    DPRINTK("\nPWM_PWDUTY0: %x", PWM_PWDUTY0);
	}
	
	if (fbi == pxafb_smart)
	{
		/* Disable CLI backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_DISPLAY_BL,LIGHTS_FL_COLOR_BLACK);
		
		#if (0)
		bklight_value = 0x40;
		power_ic_write_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		power_ic_read_reg(POWER_IC_REG_FL_LS0, &bklight_value);
		DPRINTK("C_ENABLE(MAIN):POWER_IC_REG_FL_LS0(0x%x)\n",bklight_value);
		#endif

	    PWM_PWDUTY0 = 0;
	    GAFR0_U &= 0xFFFFFFFC; 
	    GPDR0 &= 0xFFFEFFFF; //PWM0 is GPIO16, set PWM0 as input when turn off bklight
	    CKEN &= ~CKEN0_PWM0;
	    PWM_PWDUTY0	 = 0;
	    //gbklightDutyCycle = MIN_DUTYCYCLE;

	    pxafb_global_state.smart_state &= 0xE;//0b110
	    DPRINTK("\n CLI panel: CKEN: %x", CKEN);
	    DPRINTK("\nPWM_PWDUTY0: %x", PWM_PWDUTY0);
	}

	up(&pxafb_global_state.g_sem);
}

void pxafb_setup_gpio(void *pxafbinfo)
{
	unsigned int lccr0;
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;

	/*
	 * setup is based on type of panel supported
	 */
	if (fbi == pxafb_main)
	{
		DPRINTK("pxafb_setup_gpio(%d),pxafb_main\n",current->pid);

		lccr0 = fbi->reg_lccr0;

		/* 4 bit interface */
		if ((lccr0 & LCCR0_CMS) && (lccr0 & LCCR0_SDS) && !(lccr0 & LCCR0_DPD))
		{
			// bits 58-61
			GPDR1 |= (0xf << 26);
			GAFR1_U = (GAFR1_U & ~(0xff << 20)) | (0xaa << 20);

			// bits 74-77
			GPDR2 |= (0xf << 10);
	        	GAFR2_L = (GAFR2_L & ~(0xff << 20)) | (0xaa << 20);
		}
	    	/* 8 bit interface */
	   	else if (((lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_DPD))) || (!(lccr0 & LCCR0_CMS) && !(lccr0 & LCCR0_PAS) && !(lccr0 & LCCR0_SDS)))
	    	{
			// bits 58-65
			GPDR1 |= (0x3f << 26);
		        GPDR2 |= (0x3);
	
		        GAFR1_U = (GAFR1_U & ~(0xfff << 20)) | (0xaaa << 20);
		        GAFR2_L = (GAFR2_L & ~0xf) | (0xa);
	
		        // bits 74-77
		        GPDR2 |= (0xf << 10);
		        GAFR2_L = (GAFR2_L & ~(0xff << 20)) | (0xaa << 20);
		}
		/* 16bpp, 18bpp, 19bpp, 24bpp and 25bpp interface */
		else if (!(lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_PAS)))
		{
			switch (fbi->max_bpp) 
			{
			case 16:
				// bits 58-77
				GPDR1 |= (0x3f << 26);
				GPDR2 |= 0x00003fff;
				
				GAFR1_U = (GAFR1_U & ~(0xfff << 20)) | (0xaaa << 20);
				GAFR2_L = (GAFR2_L & 0xf0000000) | 0x0aaaaaaa;
				break;
			case 18:
			case 19:
			case 24:
			case 25:
				DPRINTK("Set 18-bit interface");
				// bits 58-77 and 86, 87
				GPDR1 |= (0x3f << 26);
				GPDR2 |= 0x00c03fff;
				
				GAFR1_U = (GAFR1_U & ~(0xfff << 20)) | (0xaaa << 20);
				GAFR2_L = (GAFR2_L & 0xf0000000) | 0x0aaaaaaa;
				GAFR2_U = (GAFR2_U & 0xffff0fff) | 0xa000;
				break;
			default:
				break;
			}
	    	}
	    	else
			printk(KERN_ERR "pxafb_setup_gpio: unable to determine bits per pixel\n");
			
		/* Read & save LCD ID: ID1 = GPIO<18> and ID0 = GPIO<80> */
		/* GPDR set to input at reset */
		fbi->lcd_id = (GPLR0 & 0x00040000) >> 17  | (GPLR2 & 0x00010000) >> 16;

	#ifdef BARBADOS_P3
		/* Setup GPIO<19> as general GPIO OUT HIGH */
		GAFR0_U &= ~(0x3 <<6);
		GPSR0 = 0x80000;
		GPDR0 |= 0x80000;
		mdelay(1);
	#else
		/* Set LCD_MUX to be high for main TFT panel */
		GPSR1 = 0x00020000;   //GPSR(49) = GPIO_bit(49);	/* GPIO<49> */
		GPDR1 |= 0x00020000; //GPDR(49) = GPIO_bit(49);

	#endif
	}
	if (fbi == pxafb_smart)
	{		
		DPRINTK("pxafb_setup_gpio(%d):pxafb_smart\n",current->pid);
		/* We use 8 bit interface */
		/* LDD0 - LDD7 */	
		// bits 58-65 
	        GPDR1 |= (0x3f << 26);
	        GPDR2 |= (0x3);
	
	        GAFR1_U = (GAFR1_U & ~(0xfff << 20)) | (0xaaa << 20);
	        GAFR2_L = (GAFR2_L & ~0xf) | (0xa);

	        // bits 74-76,77
	        GPDR2 |= (0xf << 10);
	        GAFR2_L = (GAFR2_L & ~(0xff << 20)) | (0xaa << 20);
	        //GAFR2_L = (GAFR2_L & ~(0x3f << 20)) | (0x2a << 20);
        
 		/* We don't use L_VSYNC here. */
        
		#ifdef BARBADOS_P3
	       	/* L_CS: gpio19, L_CS for barbados P3 */
	        GPDR0 |= (0x1 << 19);
	        GAFR0_U = (GAFR0_U & ~(0x3 << 6)) | (0x2 << 6);
	        #else
		/* Switch L_PCLK_WR to CS CLI panel */
		GPCR1 = 0x20000;  //GPCR(49) = GPIO_bit(49);
		GPDR1 |= 0x20000; //GPDR(49) = GPIO_bit(49); 
		#endif
	}
}

void pxafb_enable_controller(void *pxafbinfo)
{
	static unsigned int init_phase = 0;
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	
	DPRINTK("Enabling LCD controller");

	/* power_state is initialized in this structure to
	   DPM_POWER_OFF; make sure to initialize it to ON when the
	   controller is enabled.

	   Must use the global since this function has no other access
	   to pxafb_device_ldm.
	*/
	if (fbi == pxafb_main) 
	{
		DPRINTK("pxafb_enable_controller(%d):pxafb_main\n",current->pid);

		pxafb_main_device_ldm.power_state = DPM_POWER_ON;
	#ifdef CONFIG_CPU_BULVERDE
		/* workaround for insight 41187 */
		OVL1C2 = 0;
		OVL1C1 = 0;
		OVL2C2 = 0;
		OVL2C1 = 0;
		CCR = 0;

		LCCR4 |= (1<<31) | (1<<25) | (4<<17);
		LCCR3 = fbi->reg_lccr3;
		LCCR2 = fbi->reg_lccr2;
		LCCR1 = fbi->reg_lccr1;
		LCCR0 = fbi->reg_lccr0 & ~LCCR0_ENB;

		LCCR0 |= LCCR0_ENB;
		LCCR0 |= LCCR0_DIS;

		LCCR3 = fbi->reg_lccr3;
		LCCR2 = fbi->reg_lccr2;
		LCCR1 = fbi->reg_lccr1;
		LCCR0 = fbi->reg_lccr0 & ~LCCR0_ENB;

		LCCR0 |= LCCR0_ENB;
		LCCR0 &= ~LCCR0_ENB;

		FDADR0 = fbi->fdadr0;
		FDADR1 = fbi->fdadr1;
		

		if (PXAFB_MAIN_VALID_FB == PXAFB_MAIN_OVL1)  //overlay1fb is significant fb of _main_ panel
		{
			LCCR4 = (LCCR4 & (~(0x3<<15))) | (0x2<<15);/* PAL_FOR = "10" */		
			/* PDFOR = "11" */
			LCCR3 = (LCCR3 & (~(0x3<<30))) | (0x3<<30);
		}

		LCCR0 |= LCCR0_ENB;

	#else
		/* Sequence from 11.7.10 */
		LCCR3 = fbi->reg_lccr3;
		LCCR2 = fbi->reg_lccr2;
		LCCR1 = fbi->reg_lccr1;
		LCCR0 = fbi->reg_lccr0 & ~LCCR0_ENB;

		/* FIXME we used to have LCD power control here */

		FDADR0 = fbi->fdadr0;
		FDADR1 = fbi->fdadr1;
		LCCR0 |= LCCR0_ENB;
	#endif

		//mdelay(1); /* new delay time from xushiwei.  LCD timing delay*/
		
		GPSR2 = 0x00004000;	/* Turn on LCD_SD(GPIO<78>) */
		mdelay(1);		/* LCD timing delay*/
		GPCR2 = 0x00004000; 	/* LCD is enabled by setting LCD_SD low*/
		GPDR2 |= 0x00004000;

		GPCR2 = 0x00008000;  	/* Set LCD_CM(GPIO<79>) to be normal mode */ 
		GPDR2 |= 0x00008000;

		mdelay(TFT_PANEL_TURNON_DELAY);  //72R89341N spec requires 166ms, based on my testing, it should be at least 200ms//

		//remove it -- bLCDOn = 1;

		//DPRINTK("FDADR0 = 0x%08x\n", (unsigned int)FDADR0);
		//DPRINTK("FDADR1 = 0x%08x\n", (unsigned int)FDADR1);
		//DPRINTK("LCCR0 = 0x%08x\n", (unsigned int)LCCR0);
		//DPRINTK("LCCR1 = 0x%08x\n", (unsigned int)LCCR1);
		//DPRINTK("LCCR2 = 0x%08x\n", (unsigned int)LCCR2);
		//DPRINTK("LCCR3 = 0x%08x\n", (unsigned int)LCCR3);
		//DPRINTK("LCCR4 = 0x%08x", (unsigned int)LCCR4);
	}

	if (fbi == pxafb_smart)
	{
		DPRINTK("pxafb_enable_controller(%d):pxafb_smart\n",current->pid);
		pxafb_smart_device_ldm.power_state = DPM_POWER_ON;

		LCCR5 = fbi->reg_lccr5;
		LCCR4 = fbi->reg_lccr4;
		LCCR3 = fbi->reg_lccr3;
		LCCR2 = fbi->reg_lccr2;
		LCCR1 = fbi->reg_lccr1;
		LCCR0 = fbi->reg_lccr0 & ~LCCR0_ENB;
				
		/* Stop processor when it executed "wait for sync" command */
		CMDCR = 0x1;
		/* TODO:  need to setup corresponding GPIO pins  */
		/* TODO:  need to enable LCD controller */
		DPRINTK("LCCR0 = 0x%08x", (unsigned int)LCCR0);
		DPRINTK("LCCR1 = 0x%08x", (unsigned int)LCCR1);
		DPRINTK("LCCR2 = 0x%08x", (unsigned int)LCCR2);
		DPRINTK("LCCR3 = 0x%08x", (unsigned int)LCCR3);
		DPRINTK("LCCR4 = 0x%08x", (unsigned int)LCCR4);
		DPRINTK("LCCR5 = 0x%08x", (unsigned int)LCCR5);
		
		if (!init_phase ++)
		{
			DPRINTK("Default state: GPIO<19> D: GPDR0(0x%x)\n", GPDR0);
			DPRINTK("Default state: GPIO<19> L: GPLR0(0x%x)\n", GPLR0);
			DPRINTK("Default state: GPIO<49> D: GPDR1(0x%x)\n", GPDR1);
		        DPRINTK("Default state: GPIO<49> L: GPLR1(0x%x)\n", GPLR1);
			DPRINTK("Default state: GPIO<76> D: GPDR2(0x%x)\n", GPDR2);
		        DPRINTK("Default state: GPIO<76> F: GAFR2_L(0x%x)\n", GAFR2_L);

			#ifdef BARBADOS_P3
	            		set_GPIO_mode(49|GPIO_OUT);
				/* Pulse CLI active low hardware reset signal */
				GPSR(49) = GPIO_bit(49);	/* set GPIO<19> high  */
	 	        	mdelay(1);
				GPCR(49) = GPIO_bit(49); 	/* CLI is reset by setting low */
				mdelay(20);		/* assert for ~20msec */
			    	DPRINTK("Check GPIO<49> direction: GPDR1(0x%x)\n", GPDR1);
			    	DPRINTK("Check GPIO<49> level: GPLR1(0x%x)\n", GPLR1);
				GPSR(49) = GPIO_bit(49);	/* set GPIO<19> high  */

				//mdelay(30);
			#else
	            		set_GPIO_mode(GPIO_CLI_RESETB|GPIO_OUT);
				/* Pulse CLI active low hardware reset signal */
				GPSR(GPIO_CLI_RESETB) = GPIO_bit(GPIO_CLI_RESETB);	
	 	        	mdelay(1);
				GPCR(GPIO_CLI_RESETB) = GPIO_bit(GPIO_CLI_RESETB); 	/* CLI is reset by setting low */
				mdelay(20);		/* assert for ~20msec */
				GPSR(GPIO_CLI_RESETB) = GPIO_bit(GPIO_CLI_RESETB);	

				//mdelay(30);
			#endif
			
			/* Initialize the LCD panel. Need to do initialize only once. */	
			pxafb_smart_send_init_cmd(pxafb_smart);	
		}
		else
		{
			/* Initialize the LCD panel. Need to do initialize only once. */	
			pxafb_smart_display_on(pxafb_smart);
		}

		if (1 == init_phase ++)
		{
			DPRINTK("pxafb_smart_load_init_image\n");
			pxafb_smart_load_init_image(pxafb_smart);
		}
		
		/*Make sure CLI will not be reset by sleep/deep sleep mode */
		PGSR(49) |= GPIO_bit(49);
	}
}

void pxafb_disable_controller(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	DPRINTK("Disabling LCD controller");

	/* Think about CLI/MAIN panel case, take semaphore in <set_ctrlr_state> before panel switchings  */
	if (fbi == pxafb_main)
	{
		DPRINTK("pxafb_disable_controller(%d):pxafb_main\n",current->pid);

		DBPRINTK("pxafb_disable_controller: FBR0 (0x%x)\n", FBR0);
		while(FBR0 & 0x1)  //waiting for the completion of concurrent FBR branch cmd//
		;

		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		
		/* needed to turn off LCD screen */
		DPRINTK("pxafb_disable_controller(%d):disable GPIOs for LCD_72R89341N\n",current->pid);

		GPCR2 = 0x00004000;//GPCR(78) = GPIO_bit(78);   /* LCD_SD(GPIO<78>), rising edge for turn off */
		mdelay(1);
		GPSR2 = 0x00004000;//GPSR(78) = GPIO_bit(78); 
		GPDR2 |= 0x00004000; //GPDR(78) = GPIO_bit(78);
		PGSR2 |= 0x00004000;  /* set sleep state of GPIO<78> */
		mdelay(TFT_PANEL_TURNOFF_DELAY);  // According to TFT spec //

		GPSR2 = 0x00008000;  //GPSR(79) = GPIO_bit(79); /*  LCD_CM(GPIO<79>), high for partial mode */

		#if (0) //Based on my testing, this will cause CLI display disappear //		
		#ifdef BARBADOS_P3
		/*Do nothing */
		#else
		GPCR1 = 0x00020000; //GPCR(49) = GPIO_bit(49);  /* switch LCD_MUX to CLI panel */
		GPDR1 |= 0x00020000; //GPDR(49) = GPIO_bit(49);
		#endif		
		#endif

		LCCR0 &= ~LCCR0_LDM;	/* Enable LCD Disable Done Interrupt */
	//	LCCR0 &= ~LCCR0_ENB;	/* Disable LCD Controller */
		LCCR0 |= LCCR0_DIS;   //Normal disable LCD //
		mdelay(18);
	
		pxafb_main_device_ldm.power_state = DPM_POWER_OFF;
	}

	if (fbi == pxafb_smart)
	{
		DPRINTK("pxafb_disable_controller(%d):pxafb_smart\n",current->pid);
		pxafb_smart_device_ldm.power_state = DPM_POWER_OFF;
		
		/* Firstly, we need to display off CLI panel, then remove its CS */
		pxafb_smart_display_off(pxafb_smart);

		#ifdef BARBADOS_P3
		/* Enable GPIO<19> as general GPIO OUT HIGH */
		GAFR0_U &= ~(0x3 << 6);
		GPSR0 = 0x80000;
		GPDR0 |= 0x80000;
		mdelay(1);
		#else
		/* Set LCD_MUX to be HIGH to switch to main panel*/
		GPSR1 = 0x00020000;  //GPSR(49) = GPIO_bit(49); 	/* GPIO<49> */
		GPDR1 |= 0x00020000;//GPDR(49) = GPIO_bit(49); 
		#endif

		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		LCCR0 &= ~LCCR0_LDM;
		LCCR0 |= LCCR0_DIS;	/* Disable LCD Controller */
		mdelay(2);
		//Since CLI panel is passive, we don't need to wait the completion of current frame //
		
		/* make sure GPIO<49> will not be pulled down during CLI is disabled */
		PGSR(49) |= GPIO_bit(49); //PGSR1 &= ~(1<<17);
	}
}

void pxafb_suspend_controller(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	DPRINTK("Disabling LCD controller");

	/* Think about CLI/MAIN panel case, take semaphore in <set_ctrlr_state> before panel switchings */
	if (fbi == pxafb_main)
	{
		/* Do nothing, just return */
		return;
	}

	if (fbi == pxafb_smart)
	{
		DPRINTK("pxafb_suspend_controller(%d):pxafb_smart\n",current->pid);
		pxafb_smart_device_ldm.power_state = DPM_SUSPEND_FOR_OP;

		#ifdef BARBADOS_P3
		/* Enable GPIO<19> as general GPIO OUT HIGH */
		GAFR0_U &= ~(0x3 << 6);
		GPSR0 = 0x80000;
		GPDR0 |= 0x80000;
		mdelay(1);
		#else
		/* Set LCD_MUX to be HIGH to switch to main panel*/
		GPSR1 = 0x00020000; //GPSR(49) = GPIO_bit(49) ; 	/* GPIO<49> */
		GPDR1 |= 0x00020000;//GPDR(49) = GPIO_bit(49); 
		#endif

		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		LCCR0 &= ~LCCR0_LDM;
		LCCR0 |= LCCR0_DIS;	/* Disable LCD Controller */
		mdelay(2);
		//Since CLI panel is passive, we don't need to wait until the completion of current frame //
		/* Make sure CLI will not be reset during sleep/deep sleep mode */
		PGSR(49) |= GPIO_bit(49);
	}
}
void pxafb_powersave_controller(void *pxafbinfo)
{
}
int pxafb_smart_exit_partial_mode(void *pxafbinfo)
{
	return 0;
}
void pxafb_smart_load_init_image(void *fbi)
{ // for testing CLI display transfer byte-by-byte //
	unsigned long i, count, smart_offset, main_offset, this_one;
//	unsigned long j;
	unsigned char r, g, b;
//	unsigned short *data;
//	unsigned short cmd;
//	unsigned char *source;
//	int x, fblen, ret;
//	unsigned int cmd_size;
			
	DPRINTK("pxafb_smart_load_init_image(%d), begin show picture in SMART panel\n", current->pid);

	// temp: just clean the screen, later implementation will add init image
	memset(pxafb_smart->screen_cpu, 0x0, 96*80*2);
	pxafb_smart_send_display_data(pxafb_smart);
	return;
	/* This is for unpacked 18bpp */
	//for (i = 0; i < pxafb_smart->max_yres; i ++)
	//{
	//	for (j = 0; j < pxafb_smart->max_xres; j ++)
	//	{
	//		smart_offset = i * pxafb_smart->max_xres * 4 + j * 4;
	//		main_offset = i * pxafb_main->max_xres * 3 + j * 3;
	//		pxafb_smart->screen_cpu[smart_offset] = pxafb_main->screen_cpu[main_offset];
	//		pxafb_smart->screen_cpu[smart_offset + 1] = pxafb_main->screen_cpu[main_offset + 1];
	//		pxafb_smart->screen_cpu[smart_offset + 2] = pxafb_main->screen_cpu[main_offset + 2];
	//		pxafb_smart->screen_cpu[smart_offset + 3] = 0x0;
	//	}
	//}

	/* For packed 18bpp */
	if (1)  //For frame transfer //
	{
	for (i = 0; i < pxafb_smart->max_yres; i ++)
	{
		smart_offset = i * pxafb_smart->max_xres * 3;  //Only for packed 18bpp //
		main_offset = i * pxafb_main->max_xres * 3;
		memcpy(&pxafb_smart->screen_cpu[smart_offset], &pxafb_main->screen_cpu[main_offset], pxafb_smart->max_xres * 3);
	}

	count = pxafb_smart->max_xres * pxafb_smart->max_yres;
	for ( i = 0; i < count; i ++ )  //For each pixel //
	{
		this_one = i % pxafb_smart->max_xres;

		if ( (this_one >= 0) && (this_one <= pxafb_smart->max_xres/3) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0x00;
			pxafb_smart->screen_cpu[i*3 + 1] = 0xF0;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x03;
		}

		if ( (this_one > pxafb_smart->max_xres/3) && (this_one <= pxafb_smart->max_xres/3*2) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0xC0;
			pxafb_smart->screen_cpu[i*3 + 1] = 0x0F;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x00;
		}

		if ( (this_one > pxafb_smart->max_xres/3*2) && (this_one < pxafb_smart->max_xres) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0x3F;
			pxafb_smart->screen_cpu[i*3 + 1] = 0x00;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x00;
		}
	}

//	pxafb_smart_send_frame_data(pxafb_smart);  //Or pxafb_smart_send_display_data -- according to your smart panel model
	pxafb_smart_send_display_data(pxafb_smart);	
	}
	
	if (0)  //For byte-to-byte transfer //
	{
	for (i = 0; i < pxafb_smart->max_yres; i ++)
	{
		smart_offset = i * pxafb_smart->max_xres * 3;  //Only for packed 18bpp //
		main_offset = i * pxafb_main->max_xres * 3;
		memcpy(&pxafb_smart->screen_cpu[smart_offset], &pxafb_main->screen_cpu[main_offset], pxafb_smart->max_xres * 3);
	}
	
	count = pxafb_smart->max_xres * pxafb_smart->max_yres;

	for ( i = 0; i < count; i ++ )
	{
		b = pxafb_smart->screen_cpu[3*i];
		g = pxafb_smart->screen_cpu[3*i+1];
		r = pxafb_smart->screen_cpu[3*i+2];
		pxafb_smart->screen_cpu[3*i] = b<<2;
		pxafb_smart->screen_cpu[3*i+1] = (g<<4)|((b>>6)<<2);
		pxafb_smart->screen_cpu[3*i+2] = (r<<6) | ((g>>4)<<2);
	}

	for ( i = 0; i < count; i ++ )  //For each pixel //
	{
		this_one = i % pxafb_smart->max_xres;

		if ( (this_one >= 0) && (this_one <= pxafb_smart->max_xres/3) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 1] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 2] = 0xFC;
		}

		if ( (this_one > pxafb_smart->max_xres/3) && (this_one <= pxafb_smart->max_xres/3*2) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 1] = 0xFC;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x0;
		}

		if ( (this_one > pxafb_smart->max_xres/3*2) && (this_one < pxafb_smart->max_xres) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0xFC;
			pxafb_smart->screen_cpu[i*3 + 1] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x0;
		}
	}
	pxafb_smart_send_display_data(pxafb_smart);
	}

	#if (0)		
	r = pxafb_smart->max_xres/3;
	g = r * 2;
	b = pxafb_smart->max_xres;

	for ( i = 0; i < (pxafb_smart->max_xres * pxafb_smart->max_yres); i ++ )  //For each pixel //
	{
		this_one = i % pxafb_smart->max_xres;

		if ( (this_one >= 0) && (this_one <= r) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 1] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 2] = 0xFC;
		}

		if ( (this_one > r) && (this_one <= g) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 1] = 0xFC;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x0;
		}

		if ( (this_one > g) && (this_one < b) )
		{
			pxafb_smart->screen_cpu[i * 3] = 0xFC;
			pxafb_smart->screen_cpu[i*3 + 1] = 0x0;
			pxafb_smart->screen_cpu[i*3 + 2] = 0x0;
		}
	}
	DPRINTK("pxafb_init(%d), finish filling the framebuffer of the SMART panel\n", current->pid);
			
	DPRINTK("pxafb_init(%d), before pxafb_smart_send_frame_data\n", current->pid);
//	pxafb_smart_send_frame_data(pxafb_smart);
	pxafb_smart_send_display_data(pxafb_smart);
	#endif
}

int pxafb_smart_update(void *fbi)
{
	int ret;
//	ret = pxafb_smart_send_frame_data(fbi);  //Or pxafb_smart_send_display_data -- according to your smart panel model
	ret = pxafb_smart_send_display_data(fbi);
	return ret;
}


void pxafb_smart_refresh(void *dummy) 
{
	

	DPRINTK("pxafb_smart_refresh(%d, time --%lu)\n", current->pid, jiffies);

	/* Check if LCD controller has been removed from pxafb_smart CSTN panel */
	if (pxafb_global_state.smart_state & PXAFB_PANEL_ON)
	{
		if (down_trylock(&pxafb_global_state.g_sem))
		{
			DPRINTK("pxafb_smart_refresh(%d), down_trylock fail\n", current->pid);
			return ; 
		}	
//		ret = pxafb_smart_send_frame_data(pxafb_smart);  //Or pxafb_smart_send_display_data -- according to your smart panel model
		pxafb_smart_send_display_data(pxafb_smart);

		schedule_task(&pxafb_smart->smart_refresh_task);  //schedule_task in context_switch kernel thread //
		
 		up(&pxafb_global_state.g_sem);
	}
	
	DPRINTK("pxafb_smart_refresh(%d), return.\n", current->pid);
	
}
