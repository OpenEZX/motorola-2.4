/*
 * linux/drivers/video/ezx_lcd/barbados.c
 * Intel Bulverde/PXA250/210 LCD Controller operations for Moto BARBADOS product
 * 
 * Copyright (C) 2004-2006 - Motorola
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
 *      - Support Barbados product based on new LCD structure
 * 2005/12/12  
 *      - Add "CLI partial display mode" feature to reduce CLI standby current drain;
 *	 - Clear PGSR.LCD_CM bit to reduce Main panel standby current drain;
 *	 - Change mdelay to schedule_timeout() to improvement system performance;
 *	 - Update GPIO setting up method to make it easy to read;
 *	 - Fix bug in FL setting at BK turnning off;
 * 2006/06/20
 *	 -Change LCD_CS sleep status from low to high in order to drive LCD_PCLK low following
 *	   MUX component curcuit's requirement;	
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
#include <linux/sched.h>

#include <linux/lights_funlights.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#include "lcd_barbados.h"
#include "pxafb.h"


extern struct pxafb_info *pxafb_main;
extern struct pxafb_info *pxafb_smart;
extern struct global_state pxafb_global_state;

extern struct device pxafb_main_device_ldm;
#ifdef CONFIG_PXAFB_CLI
extern struct device pxafb_smart_device_ldm;
#endif

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
	    
	    set_GPIO_mode(GPIO16_PWM0_MD);

	    pxafb_global_state.main_state |= 0x1; //0b01
	    pxafb_global_state.smart_state &= 0xE; //0b1110
	    
	    DPRINTK("\npxafb_bklight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
#ifdef CONFIG_PXAFB_CLI
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
	    
	    set_GPIO_mode(GPIO16_PWM0_MD);

	    pxafb_global_state.smart_state |= 0x1; //0b01
	    pxafb_global_state.main_state &= 0x6; //0b110
	    
	    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
#endif
}

void pxafb_ezx_Backlight_turn_on(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;

	if (fbi == pxafb_main)
	{
		/*Check if Main panel has not been enabled, if so, don't turn on its backlight */
		if ( !(pxafb_global_state.main_state & PXAFB_PANEL_ON) )
		{
			return;
		}

		down(&pxafb_global_state.g_sem);
	
		/* Disable CLI backlight, Enable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_DISPLAY_BL,LIGHTS_FL_COLOR_WHITE);
		
	        //Adjust brightness -- configure PWM0 register 
	       CKEN |= CKEN0_PWM0;
   	       PWM_CTRL0 = BKLIGHT_PRESCALE;
	       PWM_PERVAL0 = BKLIGHT_PERIOD;
	       // gbklightDutyCycle should got from stup file system 
	       PWM_PWDUTY0   = pxafb_global_state.bklight_main_dutycycle;

	       // configure GPIO for PWM0 
	    
	       set_GPIO_mode(GPIO16_PWM0_MD);

	       pxafb_global_state.main_state |= 0x1; //0b01
	       pxafb_global_state.smart_state &= 0xE; //0b1110

	       up(&pxafb_global_state.g_sem);	
	    
	    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
#ifdef CONFIG_PXAFB_CLI
	if (fbi == pxafb_smart)
	{
		/* Check if there is display on smart panel, if not, don't turn on its backlight */
		if ( !(pxafb_global_state.smart_state & PXAFB_DISP_ON) )
		{
			return;
		}
		
		down(&pxafb_global_state.g_sem);
		
		/* Enable CLI backlight, disable MAIN backlight */
		/* Disable CLI backlight, Enable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_CLI_DISPLAY_BL,LIGHTS_FL_COLOR_WHITE);
		
	        // configure PWM0 register 
	        CKEN |= CKEN0_PWM0;
	        PWM_CTRL0 = BKLIGHT_PRESCALE;
	        PWM_PERVAL0 = BKLIGHT_PERIOD;
	        // gbklightDutyCycle should got from stup file system 
	        PWM_PWDUTY0   = pxafb_global_state.bklight_cli_dutycycle;

	        // configure GPIO for PWM0 
	    
	        set_GPIO_mode(GPIO16_PWM0_MD);

	         pxafb_global_state.smart_state |= 0x1; //0b01
	         pxafb_global_state.main_state &= 0x6; //0b110

	         up(&pxafb_global_state.g_sem);	
		
	    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
	    DPRINTK("\nCKEN: %x", CKEN);
	    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
	    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
	    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
	}
#endif
	
}


void pxafb_ezx_Backlight_turn_off(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	
    DPRINTK("\npxafb_ezx_Backlight_turn_off.\n");

	/* Don't need to check if the requested backlight is for pxafb_current */
	/* It is possible when main is enabled, cli backlight is still turned on, after a while, cli's backlight will be turned off */
	if (fbi == pxafb_main)
	{
		/*Check if Main panel has not been enabled, if so, don't turn on its backlight */
		if ( !(pxafb_global_state.main_state & PXAFB_PANEL_ON) )
		{
			return;
		}

	       down(&pxafb_global_state.g_sem);
		
		/* Disable MAIN backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_DISPLAY_BL,LIGHTS_FL_COLOR_BLACK);
		
	       PWM_PWDUTY0 = 0;
	       set_GPIO_mode(GPIO16_PWM0|GPIO_OUT);
	       clr_GPIO(GPIO16_PWM0);	//PWM0 is GPIO16, make PWM0 output low level to save power when turn off bklight
	       CKEN &= ~CKEN0_PWM0;
	       PWM_PWDUTY0	 = 0;
	
	       pxafb_global_state.main_state &= 0x06;//0b110

              up(&pxafb_global_state.g_sem);
		
	    DPRINTK("\n MAIN panel: CKEN: %x", CKEN);
	    DPRINTK("\nPWM_PWDUTY0: %x", PWM_PWDUTY0);
	}
	
#ifdef CONFIG_PXAFB_CLI
	if (fbi == pxafb_smart)
	{
		/* Check if there is display on smart panel, if not, don't turn on its backlight */
		if ( !(pxafb_global_state.smart_state & PXAFB_DISP_ON) )
		{
			return;
		}

	       down(&pxafb_global_state.g_sem);
		   
		/* Disable CLI backlight */
		lights_fl_update(LIGHTS_FL_APP_CTL_DEFAULT,1,LIGHTS_FL_REGION_CLI_DISPLAY_BL,LIGHTS_FL_COLOR_BLACK);
				

	        PWM_PWDUTY0 = 0;
	        set_GPIO_mode(GPIO16_PWM0|GPIO_OUT);
	        clr_GPIO(GPIO16_PWM0);	//PWM0 is GPIO16, make PWM0 output low level to save power when turn off bklight
  	        CKEN &= ~CKEN0_PWM0;
	        PWM_PWDUTY0	 = 0;

		 pxafb_global_state.smart_state &= 0xE; //0b1110

    	    	 up(&pxafb_global_state.g_sem);

	    DPRINTK("\n CLI panel: CKEN: %x", CKEN);
	    DPRINTK("\nPWM_PWDUTY0: %x", PWM_PWDUTY0);
	}
#endif
	
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
			//bits 58-61, LDD0 -LDD3
			set_GPIO_mode(GPIO58_LDD_0_MD);
			set_GPIO_mode(GPIO59_LDD_1_MD);
			set_GPIO_mode(GPIO60_LDD_2_MD);
			set_GPIO_mode(GPIO61_LDD_3_MD);			
			// bits 74-77,LCD control signals
			set_GPIO_mode(GPIO74_LCD_FCLK_MD);	
			set_GPIO_mode(GPIO75_LCD_LCLK_MD);	
			set_GPIO_mode(GPIO76_LCD_PCLK_MD);	
			set_GPIO_mode(GPIO77_LCD_ACBIAS_MD);
		}
	    	/* 8 bit interface */
	   	else if (((lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_DPD))) || (!(lccr0 & LCCR0_CMS) && !(lccr0 & LCCR0_PAS) && !(lccr0 & LCCR0_SDS)))
	    	{
			//bits 58-65, LDD0-LDD7
			set_GPIO_mode(GPIO58_LDD_0_MD);
			set_GPIO_mode(GPIO59_LDD_1_MD);
			set_GPIO_mode(GPIO60_LDD_2_MD);
			set_GPIO_mode(GPIO61_LDD_3_MD);
			set_GPIO_mode(GPIO62_LDD_4_MD);
			set_GPIO_mode(GPIO63_LDD_5_MD);
			set_GPIO_mode(GPIO64_LDD_6_MD);
			set_GPIO_mode(GPIO65_LDD_7_MD);
			//bits 74-77,LCD control signals
			set_GPIO_mode(GPIO74_LCD_FCLK_MD);	
			set_GPIO_mode(GPIO75_LCD_LCLK_MD);	
			set_GPIO_mode(GPIO76_LCD_PCLK_MD);	
			set_GPIO_mode(GPIO77_LCD_ACBIAS_MD);	
		}
		/* 16bpp, 18bpp, 19bpp, 24bpp and 25bpp interface */
		else if (!(lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_PAS)))
		{
			switch (fbi->max_bpp) 
			{
			case 16:
				// bits 58-73, LDD0-LDD15
				set_GPIO_mode(GPIO58_LDD_0_MD);
				set_GPIO_mode(GPIO59_LDD_1_MD);
				set_GPIO_mode(GPIO60_LDD_2_MD);
				set_GPIO_mode(GPIO61_LDD_3_MD);
				set_GPIO_mode(GPIO62_LDD_4_MD);
				set_GPIO_mode(GPIO63_LDD_5_MD);
				set_GPIO_mode(GPIO64_LDD_6_MD);
				set_GPIO_mode(GPIO65_LDD_7_MD);
				set_GPIO_mode(GPIO66_LDD_8_MD);
				set_GPIO_mode(GPIO67_LDD_9_MD);
				set_GPIO_mode(GPIO68_LDD_10_MD);
				set_GPIO_mode(GPIO69_LDD_11_MD);
				set_GPIO_mode(GPIO70_LDD_12_MD);
				set_GPIO_mode(GPIO71_LDD_13_MD);
				set_GPIO_mode(GPIO72_LDD_14_MD);		
				set_GPIO_mode(GPIO73_LDD_15_MD);
				//bits 74-77 , LCD control signal
				set_GPIO_mode(GPIO74_LCD_FCLK_MD);	
				set_GPIO_mode(GPIO75_LCD_LCLK_MD);	
				set_GPIO_mode(GPIO76_LCD_PCLK_MD);	
				set_GPIO_mode(GPIO77_LCD_ACBIAS_MD);
				break;
			case 18:
			case 19:
			case 24:
			case 25:
				DPRINTK("Set 18-bit interface");

				// bits 58-73 for LDD0-LDD15, bits 86,87 for LDD16,LDD17
				set_GPIO_mode(GPIO58_LDD_0_MD);
				set_GPIO_mode(GPIO59_LDD_1_MD);
				set_GPIO_mode(GPIO60_LDD_2_MD);
				set_GPIO_mode(GPIO61_LDD_3_MD);
				set_GPIO_mode(GPIO62_LDD_4_MD);
				set_GPIO_mode(GPIO63_LDD_5_MD);
				set_GPIO_mode(GPIO64_LDD_6_MD);
				set_GPIO_mode(GPIO65_LDD_7_MD);
				set_GPIO_mode(GPIO66_LDD_8_MD);
				set_GPIO_mode(GPIO67_LDD_9_MD);
				set_GPIO_mode(GPIO68_LDD_10_MD);
				set_GPIO_mode(GPIO69_LDD_11_MD);
				set_GPIO_mode(GPIO70_LDD_12_MD);
				set_GPIO_mode(GPIO71_LDD_13_MD);
				set_GPIO_mode(GPIO72_LDD_14_MD);		
				set_GPIO_mode(GPIO73_LDD_15_MD);
				set_GPIO_mode(GPIO86_LDD_16_MD);		
				set_GPIO_mode(GPIO87_LDD_17_MD);
				
				//bits 74-77 for LCD control signals
				set_GPIO_mode(GPIO74_LCD_FCLK_MD);	
				set_GPIO_mode(GPIO75_LCD_LCLK_MD);	
				set_GPIO_mode(GPIO76_LCD_PCLK_MD);	
				set_GPIO_mode(GPIO77_LCD_ACBIAS_MD);	
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

		/* Setup GPIO<19> as general GPIO OUT HIGH */
		set_GPIO_mode(GPIO19_L_CS|GPIO_OUT);	
		set_GPIO(GPIO19_L_CS);			
		mdelay(1);
	}
#ifdef CONFIG_PXAFB_CLI
	if (fbi == pxafb_smart)
	{		
		DPRINTK("pxafb_setup_gpio(%d):pxafb_smart\n",current->pid);
		/* We use 8 bit interface */
		//bits 58-65, LDD0-LDD7
		set_GPIO_mode(GPIO58_LDD_0_MD);
		set_GPIO_mode(GPIO59_LDD_1_MD);
		set_GPIO_mode(GPIO60_LDD_2_MD);
		set_GPIO_mode(GPIO61_LDD_3_MD);
		set_GPIO_mode(GPIO62_LDD_4_MD);
		set_GPIO_mode(GPIO63_LDD_5_MD);
		set_GPIO_mode(GPIO64_LDD_6_MD);
		set_GPIO_mode(GPIO65_LDD_7_MD);
		//bits 74-77, LCD control signals
		set_GPIO_mode(GPIO74_LCD_FCLK_MD);	
		set_GPIO_mode(GPIO75_LCD_LCLK_MD);	
		set_GPIO_mode(GPIO76_LCD_PCLK_MD);	
		set_GPIO_mode(GPIO77_LCD_ACBIAS_MD);
 		/* We don't use L_VSYNC here. */
        
	       	/* L_CS: gpio19, L_CS for barbados P3 -- Susan*/
	        set_GPIO_mode(GPIO19_L_CS_MD);	
	        								
	}
#endif
}

void pxafb_enable_controller(void *pxafbinfo)
{
	static unsigned int init_phase = 0;
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	unsigned long expire;
	
	DPRINTK("Enabling LCD controller \n");

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
		
		set_GPIO(GPIO78_LCD_SD);		/* Turn on LCD_SD(GPIO<78>) */
		clr_GPIO(GPIO78_LCD_SD);		/* LCD is enabled by setting LCD_SD low*/
		set_GPIO_out(GPIO78_LCD_SD);	

		clr_GPIO(GPIO79_LCD_CM);		/* Set LCD_CM(GPIO<79>) to be normal mode */ 
		set_GPIO_out(GPIO79_LCD_CM);	

		/*Waiting for display on, according to the spec 72R89341N spec requires 166ms, based on my testing, it should be at least 200ms*/
		set_current_state(TASK_UNINTERRUPTIBLE);
		expire = schedule_timeout(TFT_PANEL_TURNON_DELAY_IN_JIFFIES);

		//remove it -- bLCDOn = 1;

		//DPRINTK("FDADR0 = 0x%08x\n", (unsigned int)FDADR0);
		//DPRINTK("FDADR1 = 0x%08x\n", (unsigned int)FDADR1);
		//DPRINTK("LCCR0 = 0x%08x\n", (unsigned int)LCCR0);
		//DPRINTK("LCCR1 = 0x%08x\n", (unsigned int)LCCR1);
		//DPRINTK("LCCR2 = 0x%08x\n", (unsigned int)LCCR2);
		//DPRINTK("LCCR3 = 0x%08x\n", (unsigned int)LCCR3);
		//DPRINTK("LCCR4 = 0x%08x", (unsigned int)LCCR4);
	}
#ifdef CONFIG_PXAFB_CLI
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
		/* TODO:  need to setup corresponding GPIO pins -- Susan */
		/* TODO:  need to enable LCD controller */
		DPRINTK("pxafb_enable_controller: SMART \n");
		DPRINTK("LCCR0 = 0x%08x \n", (unsigned int)LCCR0);
		DPRINTK("LCCR1 = 0x%08x \n", (unsigned int)LCCR1);
		DPRINTK("LCCR2 = 0x%08x \n", (unsigned int)LCCR2);
		DPRINTK("LCCR3 = 0x%08x \n", (unsigned int)LCCR3);
		DPRINTK("LCCR4 = 0x%08x \n", (unsigned int)LCCR4);
		DPRINTK("LCCR5 = 0x%08x \n", (unsigned int)LCCR5);
		
		if (!init_phase ++)
		{
			DPRINTK("Default state: GPIO<19> D: GPDR0(0x%x)\n", GPDR0);
			DPRINTK("Default state: GPIO<19> L: GPLR0(0x%x)\n", GPLR0);
			DPRINTK("Default state: GPIO<49> D: GPDR1(0x%x)\n", GPDR1);
		        DPRINTK("Default state: GPIO<49> L: GPLR1(0x%x)\n", GPLR1);
			DPRINTK("Default state: GPIO<76> D: GPDR2(0x%x)\n", GPDR2);
		        DPRINTK("Default state: GPIO<76> F: GAFR2_L(0x%x)\n", GAFR2_L);

	            		set_GPIO_mode(GPIO49_CLI_RESET|GPIO_OUT);	
				/* Pulse CLI active low hardware reset signal */
				set_GPIO(GPIO49_CLI_RESET);			/* set GPIO<19> high  */
	 	        	udelay(10);
				clr_GPIO(GPIO49_CLI_RESET);			/* CLI is reset by setting low */
				/*According to spec 51R88450M28, 5.2.3, the min reset pulse width is 10uSec, it is set to 20usec for safe*/
				udelay(20);		/* assert for ~20usec */
			    	DPRINTK("Check GPIO<49> direction: GPDR1(0x%x)\n", GPDR1);
			    	DPRINTK("Check GPIO<49> level: GPLR1(0x%x)\n", GPLR1);
				set_GPIO(GPIO49_CLI_RESET);			/* set GPIO<19> high  */

				//mdelay(30);
			
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
#endif  //#ifdef CONFIG_PXAFB_CLI
}


void pxafb_disable_controller(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	unsigned long expire;
	
	DPRINTK("Disabling LCD controller");

	/* Think about CLI/MAIN panel case, take semaphore in <set_ctrlr_state> before panel switchings -- Susan */
	if (fbi == pxafb_main)
	{
		DPRINTK("pxafb_disable_controller(%d):pxafb_main\n",current->pid);

		DPRINTK("pxafb_disable_controller: FBR0 (0x%x)\n", FBR0);
		while(FBR0 & 0x1)  //waiting for the completion of concurrent FBR branch cmd//
		;

		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		
		/* needed to turn off LCD screen */
		DPRINTK("pxafb_disable_controller(%d):disable GPIOs for LCD_72R89341N\n",current->pid);

		clr_GPIO(GPIO78_LCD_SD);		//GPCR(78) = GPIO_bit(78);   /* LCD_SD(GPIO<78>), rising edge for turn off */
		set_GPIO(GPIO78_LCD_SD);		//GPSR(78) = GPIO_bit(78); 
		set_GPIO_out(GPIO78_LCD_SD);	//GPDR(78) = GPIO_bit(78);
		PGSR2 |= 0x00004000;  /* set sleep state of GPIO<78> */
		
		/*Waiting for display off according to the spec*/
		set_current_state(TASK_UNINTERRUPTIBLE);
		expire = schedule_timeout(TFT_PANEL_TURNOFF_DELAY_IN_JIFFIES);
		
		PGSR2 &= ~0x00008000; 			//clear GPIO79 in PGSR2	
		set_GPIO(GPIO79_LCD_CM);		//GPSR(79) = GPIO_bit(79); /* Susan: LCD_CM(GPIO<79>), high for partial mode */

		/* set one of the MUX component input:GPIO19 LCD_CS to high to drive LCD_PCLK low in sleep*/
		set_GPIO_mode(GPIO19_L_CS|GPIO_OUT);
		PGSR0 |= 0x00080000;
		
		#if (0) //Based on my testing, this will cause CLI display disappear //		
		#ifdef BARBADOS_P3
		/*Do nothing */
		#else
		GPCR1 = 0x00020000; //GPCR(49) = GPIO_bit(49);  /* Susan:switch LCD_MUX to CLI panel */
		GPDR1 |= 0x00020000; //GPDR(49) = GPIO_bit(49);
		#endif		
		#endif

		LCCR0 &= ~LCCR0_LDM;	/* Enable LCD Disable Done Interrupt */
	//	LCCR0 &= ~LCCR0_ENB;	/* Disable LCD Controller */
		LCCR0 |= LCCR0_DIS;   //Normal disable LCD //
		
		/* Wait for the completing of current frame*/
		set_current_state(TASK_UNINTERRUPTIBLE);
		expire = schedule_timeout(TO_JIFFIES(18));//18msec
		
		pxafb_main_device_ldm.power_state = DPM_POWER_OFF;
	}

#ifdef CONFIG_PXAFB_CLI
	if (fbi == pxafb_smart)
	{
		DPRINTK("pxafb_disable_controller(%d):pxafb_smart\n",current->pid);
		pxafb_smart_device_ldm.power_state = DPM_POWER_OFF;
		
		/* Firstly, we need to display off CLI panel, then remove its CS */
		pxafb_smart_display_off(pxafb_smart);

		/* Enable GPIO<19> as general GPIO OUT HIGH */
		set_GPIO_mode(GPIO19_L_CS|GPIO_OUT);	
		set_GPIO(GPIO19_L_CS);			
		mdelay(1);

		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		LCCR0 &= ~LCCR0_LDM;
		LCCR0 |= LCCR0_DIS;	/* Disable LCD Controller */
		mdelay(2);
		//Since CLI panel is passive, we don't need to wait the completion of current frame -- Susan//
		
		/* make sure GPIO<49> will not be pulled down during CLI is disabled */
		PGSR(49) |= GPIO_bit(49); //PGSR1 &= ~(1<<17);
	}
#endif
}

#ifdef CONFIG_PXAFB_CLI
void pxafb_suspend_controller(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	DPRINTK("Disabling LCD controller");

	/* Think about CLI/MAIN panel case, take semaphore in <set_ctrlr_state> before panel switchings -- Susan */
	if (fbi == pxafb_main)
	{
		/* Do nothing, just return */
		return;
	}

	if (fbi == pxafb_smart)
	{
		DPRINTK("pxafb_suspend_controller(%d):pxafb_smart\n",current->pid);
		pxafb_smart_device_ldm.power_state = DPM_SUSPEND_FOR_OP;

		/* Enable GPIO<19> as general GPIO OUT HIGH */
		set_GPIO_mode(GPIO19_L_CS|GPIO_OUT);		
		set_GPIO(GPIO19_L_CS);				
		mdelay(1);

		/* set one of the MUX component input:GPIO19 LCD_CS to high to drive LCD_PCLK low in sleep*/
		PGSR0 |= 0x00080000;

		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		LCCR0 &= ~LCCR0_LDM;
		LCCR0 |= LCCR0_DIS;	/* Disable LCD Controller */
		mdelay(2);
		//Since CLI panel is passive, we don't need to wait until the completion of current frame -- Susan//
		/* Make sure CLI will not be reset during sleep/deep sleep mode */
		PGSR(49) |= GPIO_bit(49);
	}
}
void pxafb_powersave_controller(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	unsigned long expire;
	int ret;
	/* According to cmd seq from vendor  */
	unsigned short enter_partial_cmd1[16] __attribute__ ((aligned (16))) = {
	DISPLAY_OFF,
	SET_ENABLE_DISCHARGE_PATH_0,ENTER1_SET_ENABLE_DISCHARGE_PATH_1,ENTER1_SET_ENABLE_DISCHARGE_PATH_2,ENTER1_SET_ENABLE_DISCHARGE_PATH_3,ENTER1_SET_ENABLE_DISCHARGE_PATH_4,
	POWERCONTROLSET_0,ENTER_POWERCONTROLSET_1,
	FRAME_FREQ_NLINE_0,ENTER_FRAME_FREQ_NLINE_1,FRAME_FREQ_NLINE_2,
	CONTRAST_CONTROL_0,ENTER_CONTRAST_CONTROL_1,ENTER_CONTRAST_CONTROL_2,
	BIASRATIOSET_0,ENTER_BIASRATIOSET_1
	};
	unsigned short enter_partial_cmd2[20] __attribute__ ((aligned (16))) = {
	SET_ENABLE_DISCHARGE_PATH_0,ENTER2_SET_ENABLE_DISCHARGE_PATH_1,ENTER2_SET_ENABLE_DISCHARGE_PATH_2,ENTER2_SET_ENABLE_DISCHARGE_PATH_3,ENTER2_SET_ENABLE_DISCHARGE_PATH_4,
	DISPLAY_CONTROL_0,ENTER_DISPLAY_CONTROL_1,ENTER_DISPLAY_CONTROL_2,ENTER_DISPLAY_CONTROL_3,
	SET_NONRESET_COUNTER_0,SET_NONRESET_COUNTER_1,SET_NONRESET_COUNTER_2,SET_NONRESET_COUNTER_3,SET_NONRESET_COUNTER_4,
	FIRST_DISPLAY_CMD,ENTER_START_LINE,
	DISPLAY_ON,
	0x0800,	/* Pad with noop */
	0x0800,	/* Pad with noop */
	0x0800	/* Pad with noop */
	};

	/* Think about CLI/MAIN panel case, take semaphore in <set_ctrlr_state> before panel switchings -- Susan */
	if (fbi == pxafb_main)
	{
		/* Do nothing, just return */
		return ;
	}

	if (fbi == pxafb_smart)
	{
		DPRINTK("pxafb_powersave_controller(%d):pxafb_smart\n",current->pid);
		pxafb_smart_device_ldm.power_state = DPM_SUSPEND_FOR_OP;
		/*Enter into partial display mode from normal display mode,If already in partial display mode,do nothing */
		if (( !(pxafb_global_state.smart_state & PXAFB_PARTIAL_DISP_ON) ) &&(pxafb_global_state.smart_state & PXAFB_DISP_ON))
		{
			/* Firstly, we need to enter CLI partial display mode, then remove its CS */
			DPRINTK("pxafb_powersave_controller: step1-- Send partial cmd 1 \n");
			ret = pxafb_smart_send_partial_cmd(fbi,enter_partial_cmd1,16);

			/*According to sw setting seq from hw team, need 500ms for volt discharge */
			set_current_state(TASK_UNINTERRUPTIBLE);
			expire = schedule_timeout(CSTN_VOLT_DISCHARGE_DELAY_IN_JIFFIES);
			
			DPRINTK("pxafb_powersave_controller: step2-- Send partial cmd 2 \n");
			ret = pxafb_smart_send_partial_cmd(fbi,enter_partial_cmd2,20);
			DPRINTK("pxafb_powersave_controller: step3 --DONE \n");
		}
	
		LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
		LCCR0 &= ~LCCR0_LDM;
		LCCR0 |= LCCR0_DIS;	/* Disable LCD Controller */
		mdelay(2);
		//Since CLI panel is passive, we don't need to wait until the completion of current frame -- Susan//
		/* Make sure CLI will not be reset during sleep/deep sleep mode */
		PGSR(49) |= GPIO_bit(49);
		
		/* set one of the MUX component input:GPIO19 LCD_CS to high to drive LCD_PCLK low in sleep*/
		set_GPIO_mode(GPIO19_L_CS|GPIO_OUT);
		PGSR0 |= 0x00080000;

		
		
	}
}
int pxafb_smart_exit_partial_mode(void *pxafbinfo)
{
	struct pxafb_info *fbi = (struct pxafb_info *)pxafbinfo;
	int ret;
	/* According to cmd seq from vendor  */
	unsigned short exit_partial_cmd[20] __attribute__ ((aligned (16))) = {
	DISPLAY_OFF,
	DISPLAY_CONTROL_0,EXIT_DISPLAY_CONTROL_1 ,EXIT_DISPLAY_CONTROL_2,EXIT_DISPLAY_CONTROL_3,	
	POWERCONTROLSET_0,EXIT_POWERCONTROLSET_1,
	FRAME_FREQ_NLINE_0,FRAME_FREQ_NLINE_1,FRAME_FREQ_NLINE_2,
	CONTRAST_CONTROL_0,EXIT_CONTRAST_CONTROL_1,EXIT_CONTRAST_CONTROL_2,
	BIASRATIOSET_0 ,EXIT_BIASRATIOSET_1,
	FIRST_DISPLAY_CMD,EXIT_START_LINE,
	DISPLAY_ON,
	0x0800,	/* Pad with noop */
	0x0800	/* Pad with noop */
	};

	/* Think about CLI/MAIN panel case, take semaphore in <set_ctrlr_state> before panel switchings -- Susan */
	if (fbi == pxafb_main)
	{
		/* Do nothing, just return */
		return  -1;
	}

	if (fbi == pxafb_smart)
	{
		ret = pxafb_smart_send_partial_cmd(fbi,exit_partial_cmd,20);
		DPRINTK("pxafb_smart_exit_partial_mode: DONE \n");

		return ret;
	
	}
}
void pxafb_smart_load_init_image(void *fbi)
{ //Susan for testing CLI display transfer byte-by-byte //
	unsigned long i, count, smart_offset, main_offset;
	#if 0
	unsigned long  this_one;
	unsigned char r, g, b;
	#endif
			
	DPRINTK("pxafb_smart_load_init_image(%d), begin show picture in SMART panel\n", current->pid);

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
	
	#if (1)  //For frame transfer //
	
	for (i = 0; i < pxafb_smart->max_yres; i ++)
	{
		smart_offset = i * pxafb_smart->max_xres * 3;  //Only for packed 18bpp //
		main_offset = i * pxafb_main->max_xres * 3;
		memcpy(&pxafb_smart->screen_cpu[smart_offset], &pxafb_main->screen_cpu[main_offset], pxafb_smart->max_xres * 3);
	}

	count = pxafb_smart->max_xres * pxafb_smart->max_yres;
	for ( i = 0; i < count; i ++ )  //For each pixel //
	{
		/*Fill CLI FB with blue color*/
		pxafb_smart->screen_cpu[i * 3] = 0x3F;
		pxafb_smart->screen_cpu[i*3 + 1] = 0x00;
		pxafb_smart->screen_cpu[i*3 + 2] = 0x00;
	}

	pxafb_smart_send_frame_data(pxafb_smart);  //Or pxafb_smart_send_display_data -- according to your smart panel model
	
	#else	//For byte transfer
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
	#endif

	#if (0)	//RGB strips is not needed	
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
	pxafb_smart_send_frame_data(pxafb_smart);
	#endif
}
int pxafb_smart_update(void *fbi)
{
	int ret;
	ret = pxafb_smart_send_frame_data(fbi);  //Or pxafb_smart_send_display_data -- according to your smart panel model
	return ret;
}


void pxafb_smart_refresh(void *dummy) 
{
	
	DPRINTK("pxafb_smart_refresh(%d, time --%d)\n", current->pid, jiffies);

	/* Check if LCD controller has been removed from pxafb_smart CSTN panel */
	if (pxafb_global_state.smart_state & PXAFB_PANEL_ON)
	{
		if (down_trylock(&pxafb_global_state.g_sem))
		{
			DPRINTK("pxafb_smart_refresh(%d), down_trylock fail\n", current->pid);
			return ; 
		}	
		pxafb_smart_send_frame_data(pxafb_smart);  //Or pxafb_smart_send_display_data -- according to your smart panel model
//		ret = pxafb_smart_send_display_data(fbi);

		schedule_task(&pxafb_smart->smart_refresh_task);  //schedule_task in context_switch kernel thread //
		
 		up(&pxafb_global_state.g_sem);
	}
	
	DPRINTK("pxafb_smart_refresh(%d), return.\n", current->pid);
	
}
#endif //#ifdef CONFIG_PXAFB_CLI
