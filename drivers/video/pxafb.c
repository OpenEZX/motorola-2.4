/*
 * linux/drivers/video/pxafb.c
 * Intel Bulverde/PXA250/210 LCD Controller Frame Buffer Driver
 * 
 * (c) Copyright Motorola 2003, All rights reserved.
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
 * 2004/06/28  Susan
 *      - add double buffering for LCD controller
 * 2002/06/30: lilij
 *      - Ported to EZX LCD
 *
 * 2003/05/19: zxf
 *	- Add timer to turn on backlight until lcd controller is ready
 * 2003/10/17: sdavis3
 * 	- Added 18bpp/19bpp packed support
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
#include <linux/notifier.h>
#include <linux/cpufreq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/lcdctrl.h> /* brightness, contrast, etc. control */

/*
 * debugging?
 */
#undef DEBUG
#define DBPRINTK1  //
/*
 * Complain if VAR is out of range.
 */

#define DEBUG_VAR 1

#undef ASSABET_PAL_VIDEO

#include "pxafb.h"

void (*pxafb_blank_helper)(int blank);
EXPORT_SYMBOL(pxafb_blank_helper);

extern int handshake_pass();
/* For double buffering supports -- Susan*/
unsigned long double_buffers = 0;
static unsigned long wpaper_enabled = 0;

/* Not local to be accessible from __exit fn, and LDM funcs */
//static struct pxafb_info *pxafbi = 0;
struct pxafb_info *pxafbi = 0;

static int yuv420_enabled = 0;

static void set_ctrlr_state(struct pxafb_info *fbi, u_int state);
static inline int get_pcd(unsigned int pixclock);

#ifdef CONFIG_CPU_BULVERDE
/* Framebuffer in SRAM support */
static unsigned long sram_start, sram_size = 0UL;

/* Overlays and hw cursor support prototypes */
static int overlay1fb_enable(struct fb_info *info);
static int overlay2fb_enable(struct fb_info *info);
static int cursorfb_enable(struct fb_info *info);
static void overlay1fb_disable(struct fb_info *info);
static void overlay2fb_disable(struct fb_info *info);
static void cursorfb_disable(struct fb_info *info);
static void overlay1fb_blank(int blank, struct fb_info *info);
static void overlay2fb_blank(int blank, struct fb_info *info);
static void cursorfb_blank(int blank, struct fb_info *info);

#define WAIT_FOR_END_OF_FRAME	do { \
		int count = 0;	 \
		LCSR0 = LCSR0_EOF0;  \
		while (!(LCSR0 & LCSR0_EOF0) && (count < 100) ) { \
		       mdelay(10); \
		       count ++; \
		} \
	   } \
while(0)

#define DISABLE_OVERLAYS(fbi) do {				\
	if (fbi->overlay1fb->enabled) {				\
		fbi->overlay1fb->enabled = 0;			\
		if (fbi->overlay1fb->state == C_ENABLE) {	\
			overlay1fb_disable((struct fb_info*)fbi->overlay1fb); \
			fbi->overlay1fb->state = C_ENABLE;	\
		}						\
	}							\
	if (fbi->overlay2fb->enabled) {				\
		fbi->overlay2fb->enabled = 0;			\
		if (fbi->overlay2fb->state == C_ENABLE) {	\
			overlay2fb_disable((struct fb_info*)fbi->overlay2fb); \
			fbi->overlay2fb->state = C_ENABLE;	\
		}						\
	}							\
	if (fbi->cursorfb->enabled) {				\
		fbi->cursorfb->enabled = 0;			\
		if (fbi->cursorfb->state == C_ENABLE) {		\
			cursorfb_disable((struct fb_info*)fbi->cursorfb); \
			fbi->cursorfb->state = C_ENABLE;	\
		}						\
	}							\
} while(0)

#define ENABLE_OVERLAYS(fbi) do {				\
	if (fbi->overlay1fb->state == C_ENABLE) 		\
		overlay1fb_enable((struct fb_info*)fbi->overlay1fb); \
	if (fbi->overlay2fb->state == C_ENABLE) 		\
		overlay2fb_enable((struct fb_info*)fbi->overlay2fb); \
	if (fbi->cursorfb->state == C_ENABLE) 			\
		cursorfb_enable((struct fb_info*)fbi->cursorfb); \
} while(0)

#endif /* CONFIG_CPU_BULVERDE */

/*
 * IMHO this looks wrong.  In 8BPP, length should be 8.
 */
static struct pxafb_rgb rgb_8 = {
	red:	{ offset: 0,  length: 4, },
	green:	{ offset: 0,  length: 4, },
	blue:	{ offset: 0,  length: 4, },
	transp:	{ offset: 0,  length: 0, },
};

static struct pxafb_rgb def_rgb_16 = {
	red:	{ offset: 11, length: 5, },
	green:	{ offset: 5,  length: 6, },
	blue:	{ offset: 0,  length: 5, },
	transp:	{ offset: 0,  length: 0, },
};

#ifdef CONFIG_CPU_BULVERDE
/* 16bpp, format 4 */
static struct pxafb_rgb def_rgbt_16 = {
	red:	{ offset: 10, length: 5, },
	green:	{ offset: 5,  length: 5, },
	blue:	{ offset: 0,  length: 5, },
	transp:	{ offset: 15,  length: 1, },
};
 
static struct pxafb_rgb def_rgbt_18 = {
	red:	{ offset: 12, length: 6, },
	green:	{ offset: 6,  length: 6, },
	blue:	{ offset: 0,  length: 6, },
	transp:	{ offset: 0,  length: 0, },
};

static struct pxafb_rgb  def_rgbt_24 = {
	red:	{ offset: 16, length: 8, },
	green:	{ offset: 8,  length: 8, },
	blue:	{ offset: 0,  length: 8, },
	transp:	{ offset: 0,  length: 0, },
};
#endif

static struct pxafb_mach_info pxa_fb_info __initdata = {
	pixclock:	LCD_PIXCLOCK,	/* clock period in ps */
	bpp:		LCD_BPP,
	xres:		LCD_XRES,
	yres:		LCD_YRES,
	hsync_len:	LCD_HORIZONTAL_SYNC_PULSE_WIDTH,
	vsync_len:	LCD_VERTICAL_SYNC_PULSE_WIDTH,
	left_margin:	LCD_BEGIN_OF_LINE_WAIT_COUNT,
	upper_margin:	LCD_BEGIN_FRAME_WAIT_COUNT,
	right_margin:	LCD_END_OF_LINE_WAIT_COUNT,
	lower_margin:	LCD_END_OF_FRAME_WAIT_COUNT,
	sync:		LCD_SYNC,
	lccr0:		LCD_LCCR0,
	lccr3:		LCD_LCCR3
};

static u8 bLCDOn = 0;
static unsigned long br_duration = 0;
#define BKLIGHTON  1
#define BKLIGHTOFF 0

#define BKLIGHT_PRESCALE  2 //4
#define BKLIGHT_PERIOD    49 //99
#define DEFAULT_DUTYCYCLE  25// 49 //25 //50
#define MAX_DUTYCYCLE      (BKLIGHT_PERIOD+1)  //100
#define MIN_DUTYCYCLE      0

static u8 bBklightStatus = BKLIGHTON;
static u8 gbklightDutyCycle = DEFAULT_DUTYCYCLE;

static struct timer_list bklight_timer;
static unsigned long bk_timeout = (300*HZ/1000);
#define BK_TIMEOUT	(200*HZ/1000)

void pxafb_ezx_Backlight_turn_on(void); 
/* Turn on backlight by set the PWM0 register according to the description above.
   Duty cycle is the value last time saved or default value.*/
      
void pxafb_ezx_Backlight_turn_off(void);
    /* Turn off backlight by set PWM_DUTY0 to 0 */

u8 pxafb_ezx_BackLight_Brightness_Intensity(u8 dutyCycle);
/* Adjust brightness by change PWM_DUTY0 according to the input parameter dutyCycle.*/


u8 pxafb_ezx_getBacklight_status(void)
{
	return bBklightStatus;
}
	
u8 pxafb_ezx_getBacklight_dutycycle(void)
{
        u32 dutycycle;
        
        dutycycle = PWM_PWDUTY0;
        DPRINTK("\npxafb_ezx_gtbacklight_status: dutycycle is %d", dutycycle);
	return ((u8)dutycycle);
}	

void pxafb_ezx_Backlight_turn_on(void)
{
    // configure PWM0 register 
    CKEN |= CKEN0_PWM0;
    PWM_CTRL0 = BKLIGHT_PRESCALE;
    PWM_PERVAL0 = BKLIGHT_PERIOD;
    // gbklightDutyCycle should got from stup file system 
    PWM_PWDUTY0   = gbklightDutyCycle;

    // configure GPIO for PWM0 
    GPDR0 |= 0x00010000;  //PWM0 is GPIO16
    set_GPIO_mode(GPIO16_PWM0_MD);

    bBklightStatus = BKLIGHTON;
    DPRINTK("\npxafb_ezx_Backlight_turn_on.\n");
    DPRINTK("\nCKEN: %x", CKEN);
    DPRINTK("\nPWM_CTRL0: %d", PWM_CTRL0);
    DPRINTK("\nPWM_PERVAL0: %d", PWM_PERVAL0);
    DPRINTK("\nPWM_PWDUTY0: %d", PWM_PWDUTY0);
}
      
void pxafb_ezx_Backlight_turn_off(void)
{
    DPRINTK("\npxafb_ezx_Backlight_turn_off.\n");

    // set PWM0 as GPIO 

    PWM_PWDUTY0 = 0;
    GAFR0_U &= 0xFFFFFFFC; 
    GPDR0 &= 0xFFFEFFFF; //PWM0 is GPIO16, set PWM0 as input when turn off bklight
    CKEN &= ~CKEN0_PWM0;
    PWM_PWDUTY0	 = MIN_DUTYCYCLE;
    //gbklightDutyCycle = MIN_DUTYCYCLE;

    bBklightStatus = BKLIGHTOFF;
    DPRINTK("\nCKEN: %x", CKEN);
    DPRINTK("\nPWM_PWDUTY0: %x", PWM_PWDUTY0);

}


u8 pxafb_ezx_BackLight_Brightness_Intensity(u8 dutyCycle)
{
  // setup file system should call this function whn powr on to set the dfault duty cycle
    
    if ( (dutyCycle < MIN_DUTYCYCLE) || (dutyCycle > MAX_DUTYCYCLE) )
    {
        return 0;
    }
    else 
    {
        PWM_PWDUTY0 = dutyCycle;
        gbklightDutyCycle = dutyCycle;
        return 1;
    }

    DPRINTK("\npxafb_ezx_bklight_brightness adjust: PWMDUTY0 %x", PWM_PWDUTY0);
}

EXPORT_SYMBOL(pxafb_ezx_getBacklight_status);
EXPORT_SYMBOL(pxafb_ezx_getBacklight_dutycycle);
EXPORT_SYMBOL(pxafb_ezx_Backlight_turn_on);
EXPORT_SYMBOL(pxafb_ezx_Backlight_turn_off);
EXPORT_SYMBOL(pxafb_ezx_BackLight_Brightness_Intensity);

u8 pxafb_ezx_getLCD_status(void)
{
	DPRINTK("\npxafb_ezx_getLCD_status is %d\n", bLCDOn);
	return bLCDOn;
}

EXPORT_SYMBOL(pxafb_ezx_getLCD_status);

static struct pxafb_mach_info * __init
pxafb_get_machine_info(struct pxafb_info *fbi)
{
	return &pxa_fb_info;
}

#include <linux/device.h>

static void pxafb_blank(int blank, struct fb_info *info);
static int pxafb_suspend(struct device * dev, u32 state, u32 level);
static int pxafb_resume(struct device * dev, u32 level);
static int pxafb_scale(struct bus_op_point * op, u32 level);

#ifdef CONFIG_ARCH_MAINSTONE
#include <asm/arch/bulverde_dpm.h>

static struct constraints pxafb_constraints = {
	/* one constraint */
	count:  1,
	/* constrain the LCD clock to be > 0 */
	param: {{DPM_MD_PLL_LCD, 1, 104000},},
};  
#endif /* CONFIG_ARCH_MAINSTONE */
	  
static struct device_driver pxafb_driver_ldm = {
	name:      	"pxafb",
	devclass:  	NULL,
	probe:     	NULL,
	suspend:   	pxafb_suspend,
	resume:    	pxafb_resume,
	scale:	  	pxafb_scale,
	remove:    	NULL,
#ifdef CONFIG_ARCH_MAINSTONE
	constraints:	&pxafb_constraints,
#endif
};

static struct device pxafb_device_ldm = {
	name:		"PXA FB LCD Controller",
	bus_id:		"lcd",
	driver: 	NULL,
	power_state:	DPM_POWER_OFF,
};

static void pxafb_ldm_register(void)
{
	extern void pxasys_driver_register(struct device_driver *driver);
	extern void pxasys_device_register(struct device *device);
	
	pxasys_driver_register(&pxafb_driver_ldm);
	pxasys_device_register(&pxafb_device_ldm);
}

static void pxafb_ldm_unregister(void)
{
	extern void pxasys_driver_unregister(struct device_driver *driver);
	extern void pxasys_device_unregister(struct device *device);
	
	pxasys_driver_unregister(&pxafb_driver_ldm);
	pxasys_device_unregister(&pxafb_device_ldm);
}

static int pxafb_scale(struct bus_op_point * op, u32 level)
{
	/* Bulverde is not using the passed-in arguments */

	struct pxafb_info *fbi = pxafbi;
	u_int pcd;

	/* NOTE: the cpufreq notifier function in this file
	   dis/enables the LCD controller around updating these
	   values. How important is that for Bulverde? */

	pcd = get_pcd(fbi->fb.var.pixclock);
	fbi->reg_lccr3 = (fbi->reg_lccr3 & ~0xff) | LCCR3_PixClkDiv(pcd);

	return 0;
}

static int pxafb_suspend(struct device * dev, u32 state, u32 level)
{
	switch (level) {
	case SUSPEND_POWER_DOWN:
		set_ctrlr_state(pxafbi, C_DISABLE);
		break;
	}

	return 0;
}

static int pxafb_resume(struct device * dev, u32 level)
{
	switch (level) {
	case RESUME_POWER_ON:
		set_ctrlr_state(pxafbi, C_ENABLE);
		break;
	}
	
	return 0;
}

static int pxafb_activate_var(struct fb_var_screeninfo *var, struct pxafb_info *);

static inline void pxafb_schedule_task(struct pxafb_info *fbi, u_int state)
{
	unsigned long flags;

	local_irq_save(flags);
	/*
	 * We need to handle two requests being made at the same time.
	 * There are two important cases:
	 *  1. When we are changing VT (C_REENABLE) while unblanking (C_ENABLE)
	 *     We must perform the unblanking, which will do our REENABLE for us.
	 *  2. When we are blanking, but immediately unblank before we have
	 *     blanked.  We do the "REENABLE" thing here as well, just to be sure.
	 */
	if (fbi->task_state == C_ENABLE && state == C_REENABLE)
		state = (u_int) -1;
	if (fbi->task_state == C_DISABLE && state == C_ENABLE)
		state = C_REENABLE;

	if (state != (u_int)-1) {
		fbi->task_state = state;
		schedule_task(&fbi->task);
	}
	local_irq_restore(flags);
}

/*
 * Get the VAR structure pointer for the specified console
 */
static inline struct fb_var_screeninfo *get_con_var(struct fb_info *info, int con)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	return (con == fbi->currcon || con == -1) ? &fbi->fb.var : &fb_display[con].var;
}

/*
 * Get the DISPLAY structure pointer for the specified console
 */
static inline struct display *get_con_display(struct fb_info *info, int con)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	return (con < 0) ? fbi->fb.disp : &fb_display[con];
}

/*
 * Get the CMAP pointer for the specified console
 */
static inline struct fb_cmap *get_con_cmap(struct fb_info *info, int con)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	return (con == fbi->currcon || con == -1) ? &fbi->fb.cmap : &fb_display[con].cmap;
}

static inline u_int
chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int
pxafb_setpalettereg(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	u_int val, ret = 1;

	if (regno < fbi->palette_size) {
		val = ((red >> 0) & 0xf800);
		val |= ((green >> 5) & 0x07e0);
		val |= ((blue >> 11) & 0x001f);

		fbi->palette_cpu[regno] = val;
		ret = 0;
	}
	return ret;
}

static int
pxafb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		   u_int trans, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	u_int val;
	int ret = 1;

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no mater what visual we are using.
	 */
	if (fbi->fb.var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (fbi->fb.disp->visual) {
	case FB_VISUAL_TRUECOLOR:
	case FB_VISUAL_DIRECTCOLOR:
		/*
		 * 12 or 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno <= 16) {
			u16 *pal = fbi->fb.pseudo_palette;

			val  = chan_to_field(red, &fbi->fb.var.red);
			val |= chan_to_field(green, &fbi->fb.var.green);
			val |= chan_to_field(blue, &fbi->fb.var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		ret = pxafb_setpalettereg(regno, red, green, blue, trans, info);
		break;
	}

	return ret;
}

/*
 *  pxafb_decode_var():
 *    Get the video params out of 'var'. If a value doesn't fit, round it up,
 *    if it's too big, return -EINVAL.
 *
 *    Suggestion: Round up in the following order: bits_per_pixel, xres,
 *    yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *    bitfields, horizontal timing, vertical timing.
 */
static int pxafb_validate_var(struct fb_var_screeninfo *var,
				 struct pxafb_info *fbi)
{
	int ret = -EINVAL;

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;
	if (var->xres > fbi->max_xres)
		var->xres = fbi->max_xres;
	if (var->yres > fbi->max_yres)
		var->yres = fbi->max_yres;
	var->xres_virtual =
	    var->xres_virtual < var->xres ? var->xres : var->xres_virtual;
	var->yres_virtual =
	    var->yres_virtual < var->yres ? var->yres : var->yres_virtual;

	DPRINTK("var->bits_per_pixel=%d", var->bits_per_pixel);
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:  ret = 0; break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:  ret = 0; break;
#endif
#ifdef FBCON_HAS_CFB16
	case 12:
		/* make sure we are in passive mode */
		if (!(fbi->lccr0 & LCCR0_PAS))
			ret = 0;
		break;

	case 16:
		/* 
		 * 16 bits works apparemtly fine in passive mode for those,
		 * so don't complain
		 */
		if (machine_is_lubbock() ||
		    machine_is_mainstone() ||
		    machine_is_pxa_cerf()) {
			ret = 0;
		} else
			/* make sure we are in active mode */
			if ((fbi->lccr0 & LCCR0_PAS))
				ret = 0;
		break;
#endif
#ifdef CONFIG_CPU_BULVERDE
	case 18:
	case 19:
	case 24:
	case 25:
		ret = 0;
		break;
#endif
	default:
		break;
	}

	return ret;
}

static inline void pxafb_set_truecolor(u_int is_true_color)
{
	DPRINTK("true_color = %d", is_true_color);
}

static void
pxafb_hw_set_var(struct fb_var_screeninfo *var, struct pxafb_info *fbi)
{

	fb_set_cmap(&fbi->fb.cmap, 1, pxafb_setcolreg, &fbi->fb);

	/* Set board control register to handle new color depth */
	pxafb_set_truecolor(var->bits_per_pixel >= 16);

	pxafb_activate_var(var, fbi);

}

/*
 * pxafb_set_var():
 *	Set the user defined part of the display for the specified console
 */
static int
pxafb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	struct fb_var_screeninfo *dvar = get_con_var(&fbi->fb, con);
	struct display *display = get_con_display(&fbi->fb, con);
	int err, chgvar = 0, rgbidx;

	DPRINTK("set_var");

	/*
	 * Decode var contents into a par structure, adjusting any
	 * out of range values.
	 */
	err = pxafb_validate_var(var, fbi);
	if (err)
		return err;

	if (var->activate & FB_ACTIVATE_TEST)
		return 0;

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW)
		return -EINVAL;

	if (dvar->xres != var->xres)
		chgvar = 1;
	if (dvar->yres != var->yres)
		chgvar = 1;
	if (dvar->xres_virtual != var->xres_virtual)
		chgvar = 1;
	if (dvar->yres_virtual != var->yres_virtual)
		chgvar = 1;
	if (dvar->bits_per_pixel != var->bits_per_pixel)
		chgvar = 1;
	if (con < 0)
		chgvar = 0;

	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:
		if (fbi->cmap_static)
			display->visual	= FB_VISUAL_STATIC_PSEUDOCOLOR;
		else
			display->visual	= FB_VISUAL_PSEUDOCOLOR;
		display->line_length	= var->xres / 2;
		display->dispsw		= &fbcon_cfb4;
		rgbidx			= RGB_8;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		if (fbi->cmap_static)
			display->visual	= FB_VISUAL_STATIC_PSEUDOCOLOR;
		else
			display->visual	= FB_VISUAL_PSEUDOCOLOR;
		display->line_length	= var->xres;
		display->dispsw		= &fbcon_cfb8;
		rgbidx			= RGB_8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 12:
	case 16:
		display->visual		= FB_VISUAL_TRUECOLOR;
		display->line_length	= var->xres * 2;
		display->dispsw		= &fbcon_cfb16;
		display->dispsw_data	= fbi->fb.pseudo_palette;
		rgbidx			= RGB_16;
		break;
#endif
#ifdef CONFIG_CPU_BULVERDE
	case 18:
	case 19:
		display->visual		= FB_VISUAL_TRUECOLOR;
		display->line_length	= var->xres * 3;
		display->dispsw = &fbcon_cfb24;
		display->dispsw_data	= fbi->fb.pseudo_palette;
		rgbidx                  = RGB_18;
		break;
	case 24:
	case 25:
		display->visual		= FB_VISUAL_TRUECOLOR;
		display->line_length	= var->xres * 4;
		display->dispsw = &fbcon_dummy;
		display->dispsw_data	= fbi->fb.pseudo_palette;
		rgbidx                  = RGB_24;
		break;
#endif /* CONFIG_CPU_BULVERDE */
	default:
		rgbidx = 0;
		display->dispsw = &fbcon_dummy;
		break;
	}

	display->screen_base	= fbi->screen_cpu;
	display->next_line	= display->line_length;
	display->type		= fbi->fb.fix.type;
	display->type_aux	= fbi->fb.fix.type_aux;
	display->ypanstep	= fbi->fb.fix.ypanstep;
	display->ywrapstep	= fbi->fb.fix.ywrapstep;
	display->can_soft_blank	= 1;
	display->inverse	= 0;

	*dvar			= *var;
	dvar->activate		&= ~FB_ACTIVATE_ALL;

	/*
	 * Copy the RGB parameters for this display
	 * from the machine specific parameters.
	 */
	dvar->red		= fbi->rgb[rgbidx]->red;
	dvar->green		= fbi->rgb[rgbidx]->green;
	dvar->blue		= fbi->rgb[rgbidx]->blue;
	dvar->transp		= fbi->rgb[rgbidx]->transp;

	DPRINTK("RGBT length = %d:%d:%d:%d",
		dvar->red.length, dvar->green.length, dvar->blue.length,
		dvar->transp.length);

	DPRINTK("RGBT offset = %d:%d:%d:%d",
		dvar->red.offset, dvar->green.offset, dvar->blue.offset,
		dvar->transp.offset);

	/*
	 * Update the old var.  The fbcon drivers still use this.
	 * Once they are using fbi->fb.var, this can be dropped.
	 */
	display->var = *dvar;

	/*
	 * If we are setting all the virtual consoles, also set the
	 * defaults used to create new consoles.
	 */
	if (var->activate & FB_ACTIVATE_ALL)
		fbi->fb.disp->var = *dvar;

	/*
	 * If the console has changed and the console has defined
	 * a changevar function, call that function.
	 */
	if (chgvar && info && fbi->fb.changevar)
		fbi->fb.changevar(con);

	/* If the current console is selected, activate the new var. */
	if (con != fbi->currcon)
		return 0;

	pxafb_hw_set_var(dvar, fbi);

	return 0;
}

static int
__do_set_cmap(struct fb_cmap *cmap, int kspc, int con,
	      struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	struct fb_cmap *dcmap = get_con_cmap(info, con);
	int err = 0;

	if (con == -1)
		con = fbi->currcon;

	/* no colormap allocated? (we always have "this" colour map allocated) */
	if (con >= 0)
		err = fb_alloc_cmap(&fb_display[con].cmap, fbi->palette_size, 0);

	if (!err && con == fbi->currcon)
		err = fb_set_cmap(cmap, kspc, pxafb_setcolreg, info);

	if (!err)
		fb_copy_cmap(cmap, dcmap, kspc ? 0 : 1);

	return err;
}

static int
pxafb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	struct display *disp = get_con_display(info, con);

	return (disp->visual == FB_VISUAL_TRUECOLOR) ? -EINVAL :
		__do_set_cmap(cmap, kspc, con, info);
}

static int
pxafb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct display *display = get_con_display(info, con);

	*fix = info->fix;

	fix->line_length = display->line_length;
	fix->visual	 = display->visual;
	return 0;
}

static int
pxafb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	*var = *get_con_var(info, con);
	return 0;
}

static int
pxafb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	struct fb_cmap *dcmap = get_con_cmap(info, con);
	fb_copy_cmap(dcmap, cmap, kspc ? 0 : 2);
	return 0;
}


static int
pxafb_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	       unsigned long arg, int con, struct fb_info *info)
{
	u8 val;

	switch (cmd) {
	case FBIOSETBKLIGHT:
	    DPRINTK("ezx_pxafb_ioctl_test: This is FBIOSETBKLIGHT.\n");
            DPRINTK("ezx_pxafb_ioctl: arg is %d", arg);
		switch (arg){
		case BKLIGHT_OFF:
			del_timer(&bklight_timer);
		    pxafb_ezx_Backlight_turn_off();
			return 0;
        case BKLIGHT_ON:
		del_timer(&bklight_timer);
            pxafb_ezx_Backlight_turn_on();
			return 0;	
		}
	case FBIOGETBKLIGHT:
	    DPRINTK("ezx_pxafb_ioctl_test: This is FBIOGETBKLIGHT.\n");
	    val = pxafb_ezx_getBacklight_status();
	    put_user(val, (u8*)arg);
	    return 0;
	case FBIOSETBRIGHTNESS:
	    DPRINTK("ezx_pxafb_ioctl_test: This is FBIOSETBRIGHTNESS.");
	    if ( (arg < MIN_DUTYCYCLE) || (arg > MAX_DUTYCYCLE) )
        {
            return -EFAULT;
        }    
		if (pxafb_ezx_BackLight_Brightness_Intensity((u8)arg))
		{
			DPRINTK("ezx_pxafb_ioctl_test: brightness is set to %d", (u8)arg);
		    return 0;	
		}    
		else return -EFAULT;   
	case FBIOGETBRIGHTNESS:
	    DPRINTK("ezx_pxafb_ioctl_test: This is FBIOGETBRIGHTNESS.");
	    val = pxafb_ezx_getBacklight_dutycycle();
	    put_user(val, (u8*)arg);
	    DPRINTK("ezx_pxafb_ioctl_test: dutycycle is %d", val);
	    return 0;
	case FBIOCHECK2BFS:
		{
			if (double_buffers == 1)
			{
				unsigned long *p_screen_2_dma = (unsigned long *)arg;
				*p_screen_2_dma = pxafbi->second_fb.screen_dma;
				
				return 1;  //two framebuffers are never contiguous in SRAM or SDRAM //
			}
			else
				return 0;
		}
#if (0)
	case FBIOGETWPSTATE:
		{
			if (wpaper_enabled == 1)
			{
				unsigned long *addr = (unsigned long *)arg;
				(*addr) = pxafbi->second_fb.screen_dma;
				return 1;
			}
			else
				return 0;
		}
#endif

#if CONFIG_FBBASE_DOUBLE_BUFFERS
	case FBIOENABLE2BFS:
		{
			struct pxafb_info *fbi = (struct pxafb_info *)(info);
			unsigned long bytes_per_panel;
			unsigned long secondfb_len;
			struct list_head *tmp;
			struct db_entry_s *db_entry;
			struct db_entry_s *tmp_entry;
			

DBPRINTK("ENABLE2BFS--BEGIN:task(%d) enters into enable2bfs ioctl\n", current->pid);
DBPRINTK("ENABLE2BFS--BEGIN:task(%d-%s) enters into enable2bfs ioctl\n", current->pid,current->comm);
			//Don't need to take semaphore //
			if (double_buffers == 0)  //The second framebuffer is not allocated yet //
			{
				bytes_per_panel = fbi->max_xres * fbi->max_yres * fbi->max_bpp / 8;
				secondfb_len = PAGE_ALIGN(bytes_per_panel + PAGE_SIZE);  //only be available for 16bpp//
				fbi->second_fb.map_cpu = __consistent_alloc(GFP_KERNEL, secondfb_len, &(fbi->second_fb.map_dma), PTE_BUFFERABLE);
				if (!(fbi->second_fb.map_cpu))
				{

DBPRINTK("ENABLE2BFS:allocate second fb fail,double_buffers = 0\n");

					double_buffers = 0;
					return -ENOMEM;
				}

DBPRINTK("consistent_alloc succeed, secondfb_len(%d), double_buffers(%d)\n", secondfb_len, double_buffers);

				memset((void *)fbi->second_fb.map_cpu, 0, secondfb_len);
				
				fbi->second_fb.screen_cpu = fbi->second_fb.map_cpu + PAGE_SIZE;
				fbi->second_fb.screen_dma = fbi->second_fb.map_dma + PAGE_SIZE;

				fbi->second_fb.dmadesc_fb_cpu = fbi->second_fb.screen_cpu -1*16;
				fbi->second_fb.dmadesc_fb_dma = fbi->second_fb.screen_dma - 1*16;

				fbi->second_fb.dmadesc_fb_cpu->fdadr = fbi->second_fb.dmadesc_fb_dma;
				fbi->second_fb.dmadesc_fb_cpu->fsadr = fbi->second_fb.screen_dma;
				fbi->second_fb.dmadesc_fb_cpu->fidr = 0;
				fbi->second_fb.dmadesc_fb_cpu->ldcmd = bytes_per_panel;

				fbi->fb.fix.smem_len = PAGE_ALIGN(bytes_per_panel) * 2;
				// 320*2 + 4 is the number calculated by Peter, for indicating the second fb//
				fbi->fb.var.yres_virtual = fbi->fb.var.yres *2 + 4;
				fbi->fb.var.yoffset = 0;
				
				double_buffers = 1;

DBPRINTK("ENABLE-2-BFS:double_buffers(%d),CURRENT(%s)\n",double_buffers,current->comm);
			}

			list_for_each(tmp,&(fbi->db_task_list))
			{
				tmp_entry = list_entry(tmp,struct db_entry_s,task_list);
		    	if (tmp_entry->pid == current->pid)
		    		goto exists;
			}
			
			db_entry = kmalloc(sizeof(struct db_entry_s),GFP_KERNEL);
			if (!db_entry)
				return -ENOSPC;

			//JUST FOR TESTING		db_entry->task = current; //just for testing
			db_entry->pid = current->pid;
		    list_add(&(db_entry->task_list), &(fbi->db_task_list));

DBPRINTK("Enable dbuffers:EMPTY_LIST(%d),&(fbi->db_task_list)(0x%x),(fbi->db_task_list.prev)(0x%x),(fbi->db_task_list.next)(0x%x),db_entry->task(%d),&(db_entry->task_list)(0x%x)\n",list_empty(&(fbi->db_task_list)),&(fbi->db_task_list),(fbi->db_task_list.prev),(fbi->db_task_list.next),db_entry->pid,&(db_entry->task_list));

//DBPRINTK1("Enable dbuffers:EMPTY_LIST(%d),&(fbi->db_task_list)(0x%x),(fbi->db_task_list.prev)(0x%x),(fbi->db_task_list.next)(0x%x),db_entry->task(%d--%s),&(db_entry->task_list)(0x%x)\n",list_empty(&(fbi->db_task_list)),&(fbi->db_task_list),(fbi->db_task_list.prev),(fbi->db_task_list.next),db_entry->pid,db_entry->task->comm,&(db_entry->task_list));
exists:
			return 0;
		}
	case FBIODISABLE2BFS:
		{
			struct pxafb_info *fbi = (struct pxafb_info *)info;
			unsigned int flags;
			struct db_entry_s *tmp_entry;
			struct list_head *tmp;
			struct list_head *old_tmp;

			
			if ( list_empty(&(fbi->db_task_list)) ) //Nobody is using LCD double buffers//
			{

DBPRINTK("DISABLE2BFS: list empty,double_buffers = 0;\n");

				double_buffers = 0;
				return 0;
			}
			else
			{

DBPRINTK("DISABLE2BFS:list is not empty\n");

				down(&fbi->dbsem);
				
				tmp = fbi->db_task_list.next;
				while ( tmp != &(fbi->db_task_list) )
				{
					tmp_entry = list_entry(tmp,struct db_entry_s,task_list);
					tmp = tmp->next;

DBPRINTK("DISABLE2BFS:current(%s-%d),task(%d),&task_list(0x%x)\n",current->comm,current->pid,tmp_entry->pid,&(tmp_entry->task_list));
//DBPRINTK1("DISABLE2BFS:current(%s-%d),task(%s--%d),&task_list(0x%x)\n",current->comm,current->pid,tmp_entry->task->comm,tmp_entry->pid,&(tmp_entry->task_list));

			    	if (tmp_entry->pid == current->pid)
					{
DBPRINTK("DISABLE2BFS:tmp_entry->task(%d) will be removed\n",tmp_entry->pid);
//DBPRINTK("DISABLE2BFS:tmp_entry->task(%s--%d) will be removed\n",tmp_entry->task->comm,tmp_entry->pid);
			    		list_del(&tmp_entry->task_list);
						kfree(tmp_entry);

						/* If db_task_list contains only one entry, switch to #1 framebuffer */
						if ( (tmp != &(fbi->db_task_list)) && (tmp->next == tmp->prev) )
						{
							DBPRINTK("FBIODISABLE2BFS: current(%s-%d),resume 1# framebuffer\n", current->comm,current->pid);
							/* switch to the first framebuffer */
							if (fbi->fb.var.yoffset > fbi->max_yres)  // The second fb is used for LCD panel //
							{
								local_irq_save(flags);

								fbi->dmadesc_fbhigh_cpu = fbi->first_fb.dmadesc_fb_cpu;
								fbi->dmadesc_fbhigh_cpu->fdadr = fbi->first_fb.dmadesc_fb_dma;
								fbi->dmadesc_fbhigh_dma = fbi->first_fb.dmadesc_fb_dma;
								fbi->fdadr0 = fbi->dmadesc_fbhigh_dma;
		
								FBR0 = fbi->fdadr0 | 0x01; //don't generate interrupt bit LCSR0[BS0] //
						
								local_irq_restore(flags);
						
								mdelay(18);  //PCD is 9, frame freq = 55 Frames/Second //
							}

							fbi->fb.var.yoffset = 0;							
						}							
					}//current task related db_entry_s is removed
				}

				if ( list_empty(&(fbi->db_task_list)) )
				{
DBPRINTK("DISABLE2BFS:After delete, list is empty\n");
DBPRINTK("DISABLE2BFS:releaes #2 framebuffer\n");

					/* release second framebuffer -- 152KB + PAGE_SIZE */
					consistent_free((void *)fbi->second_fb.map_cpu, fbi->map_size, fbi->second_fb.map_dma);

					fbi->second_fb.map_cpu = NULL;
					fbi->second_fb.map_dma = NULL;
					fbi->second_fb.dmadesc_fb_cpu = NULL;
					fbi->second_fb.dmadesc_fb_dma = NULL;
					fbi->second_fb.screen_cpu = NULL;
					fbi->second_fb.screen_dma = NULL;

DBPRINTK("DISABLE2BFS:AFTER releaes #2 framebuffer, double_buffers = 0\n");

 					double_buffers = 0;
					fbi->fb.fix.smem_len = fbi->max_xres * fbi->max_yres * fbi->max_bpp/8;
					fbi->fb.var.yres_virtual = fbi->fb.var.yres;
					fbi->fb.var.yoffset = 0;
				}
				up(&fbi->dbsem);
			}
			return 0;
		}
#if (0)
	case FBIOENABLEWP:
		wpaper_enabled = 1;
		break;
	case FBIODISABLEWP:
		wpaper_enabled = 0;
		break;
#endif

#endif
	}

	return -EINVAL;
}

#if CONFIG_FBBASE_DOUBLE_BUFFERS
void pxafb_2ndfb_init(void)
{
	struct pxafb_info *fbi = (struct pxafb_info *)(pxafbi);
	unsigned long bytes_per_panel;
	unsigned long secondfb_len;
	struct list_head *tmp;
	struct db_entry_s *db_entry;
	struct db_entry_s *tmp_entry;

	bytes_per_panel = fbi->max_xres * fbi->max_yres * fbi->max_bpp / 8;
	secondfb_len = PAGE_ALIGN(bytes_per_panel + PAGE_SIZE);  //only be available for 16bpp//
	fbi->second_fb.map_cpu = __consistent_alloc(GFP_KERNEL, secondfb_len, &(fbi->second_fb.map_dma), PTE_BUFFERABLE);
	if (!(fbi->second_fb.map_cpu))
	{
DBPRINTK("ENABLE2BFS:allocate second fb fail,double_buffers = 0\n");

		double_buffers = 0;
		return -ENOMEM;
	}

DBPRINTK("consistent_alloc succeed, secondfb_len(%d), double_buffers(%d)\n", secondfb_len, double_buffers);

	memset((void *)fbi->second_fb.map_cpu, 0, secondfb_len);
				
	fbi->second_fb.screen_cpu = fbi->second_fb.map_cpu + PAGE_SIZE;
	fbi->second_fb.screen_dma = fbi->second_fb.map_dma + PAGE_SIZE;

	fbi->second_fb.dmadesc_fb_cpu = fbi->second_fb.screen_cpu -1*16;
	fbi->second_fb.dmadesc_fb_dma = fbi->second_fb.screen_dma - 1*16;

	fbi->second_fb.dmadesc_fb_cpu->fdadr = fbi->second_fb.dmadesc_fb_dma;
	fbi->second_fb.dmadesc_fb_cpu->fsadr = fbi->second_fb.screen_dma;
	fbi->second_fb.dmadesc_fb_cpu->fidr = 0;
	fbi->second_fb.dmadesc_fb_cpu->ldcmd = bytes_per_panel;

	db_entry = kmalloc(sizeof(struct db_entry_s),GFP_KERNEL);
	if (!db_entry)
		return -ENOSPC;

	//JUST FOR TESTING	db_entry->task = current; //just for testing
	db_entry->pid = current->pid;
	list_add(&(db_entry->task_list), &(fbi->db_task_list));

	//fbi->fb.fix.smem_len = PAGE_ALIGN(bytes_per_panel) * 2;
	//fbi->fb.var.yres_virtual = fbi->fb.var.yres *2 + 4;  // 320*2 + 4 is the number calculated by Peter, for indicating the second fb//
				
	//double_buffers = 1;

DBPRINTK1("2NDFB_INIT:double_buffers(%d),CURRENT(%s)\n",double_buffers,current->comm);
DBPRINTK1("2NDFB_INIT:first_fb.screen_dma(0x%x)\n",fbi->first_fb.dmadesc_fb_cpu->fsadr);
DBPRINTK1("2NDFB_INIT:second_fb.screen_dma(0x%x)\n",fbi->second_fb.dmadesc_fb_cpu->fsadr);

}

int pxafb_pan_display(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	unsigned long flags;
	
	struct pxafb_info *fbi = (struct pxafb_info *)(info);
	wait_queue_t *tmp_entry;

	/* Checking if LCD controller is turned off */
	if (pxafb_device_ldm.power_state == DPM_POWER_OFF)
	{
		DBPRINTK1("LCD is off, just return -EAGAIN");
		return -EAGAIN;
	}
	
	DECLARE_WAITQUEUE(wait, current);
	
	/* Try to enters into framebuffer switching routine */
	down(&fbi->dbsem);

	if (var->yoffset == 0)  //switch to the first framebuffer
	{
		//DBPRINTK("LCD panel shifts to be the first framebuffer\n");
		local_irq_save(flags);

		fbi->dmadesc_fbhigh_cpu = fbi->first_fb.dmadesc_fb_cpu;
		fbi->dmadesc_fbhigh_cpu->fdadr = fbi->first_fb.dmadesc_fb_dma;
		fbi->dmadesc_fbhigh_dma = fbi->first_fb.dmadesc_fb_dma;
		fbi->fdadr0 = fbi->dmadesc_fbhigh_dma;

		//br_duration = OSCR;
		FBR0 = fbi->fdadr0 | 0x3; //don't set LCSR0[BS0]  //BRA should be set, BRINT is disabled //

		fbi->fb.var.yoffset = 0;  //mark that the first frame buffer is dedicated to LCD panel //
		
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&fbi->ctrlr_wait, &wait);

		local_irq_restore(flags);
		
		/* method 1 for performance testing */
		//	mdelay(18);  //PCD is 9, so FCLK is 55Hz //
		
		/* method 2 for performance testing */
		schedule();
		remove_wait_queue(&fbi->ctrlr_wait, &wait);


		
#if (0)
DBPRINTK("PAN_DISPLAY: switch to #1 framebuffer\n");
DBPRINTK("fbi->screen_cpu = 0x%x\n", fbi->screen_cpu);
DBPRINTK("fbi->screen_dma = 0x%x\n", fbi->screen_dma);
DBPRINTK("fbi->dmadesc_fbhigh_cpu = 0x%x\n", fbi->dmadesc_fbhigh_cpu);
DBPRINTK("fbi->dmadesc_fbhigh_dma = 0x%x\n", fbi->dmadesc_fbhigh_dma);
DBPRINTK("fbi->fdadr0 = 0x%x\n", fbi->fdadr0);
DBPRINTK("FBR0 = 0x%x\n", FBR0);
DBPRINTK("FDADR0 = 0x%x\n", FDADR0);
#endif
	    DBPRINTK1("fbi->screen_dma = 0x%x\n", fbi->dmadesc_fbhigh_cpu->fsadr);
	    DBPRINTK1("fbi->dmadesc_fbhigh_dma = 0x%x\n", fbi->dmadesc_fbhigh_dma);
	    DBPRINTK1("fbi->fdadr0 = 0x%x\n", fbi->fdadr0);
	    DBPRINTK1("FBR0 = 0x%x\n", FBR0);
	    DBPRINTK1("FDADR0 = 0x%x\n", FDADR0);
	}

	if (var->yoffset > fbi->fb.var.yres)  //switch to the second framebuffer
	{
		//DBPRINTK("LCD panel shifts to the second framebuffer\n");
		local_irq_save(flags);

		fbi->dmadesc_fbhigh_cpu = fbi->second_fb.dmadesc_fb_cpu;
		fbi->dmadesc_fbhigh_cpu->fdadr = fbi->second_fb.dmadesc_fb_dma;
		fbi->dmadesc_fbhigh_dma = fbi->second_fb.dmadesc_fb_dma;
		fbi->fdadr0 = fbi->dmadesc_fbhigh_dma;

//		br_duration = OSCR;
		FBR0 = fbi->fdadr0 | 0x3;  //BRA & BINT should be set //

		fbi->fb.var.yoffset = fbi->max_yres + 4;  //Mark that the second framebuffer is dedicated to LCD panel //

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&fbi->ctrlr_wait, &wait);

		local_irq_restore(flags);
		
		/* The first method for performance comparing */
		//	mdelay(18);
		
		/* The second method */
		schedule();
		remove_wait_queue(&fbi->ctrlr_wait, &wait);


#if (0)
DBPRINTK("switch to #2 framebuffer\n");
DBPRINTK("fbi->dmadesc_fbhigh_cpu = 0x%x\n", fbi->dmadesc_fbhigh_cpu);
DBPRINTK("fbi->dmadesc_fbhigh_dma = 0x%x\n", fbi->dmadesc_fbhigh_dma);
DBPRINTK("fbi->fdadr0 = 0x%x\n", fbi->fdadr0);
DBPRINTK("FBR0 = 0x%x\n", FBR0);
DBPRINTK("FDADR0 = 0x%x\n", FDADR0);
#endif

	    	DBPRINTK1("fbi->screen_dma = 0x%x\n", fbi->dmadesc_fbhigh_cpu->fsadr);
		DBPRINTK1("fbi->dmadesc_fbhigh_dma = 0x%x\n", fbi->dmadesc_fbhigh_dma);
		DBPRINTK1("fbi->fdadr0 = 0x%x\n", fbi->fdadr0);
		DBPRINTK1("FBR0 = 0x%x\n", FBR0);
		DBPRINTK1("FDADR0 = 0x%x\n", FDADR0);
	}

	up(&fbi->dbsem);

	return 0;
}

int pxafb_release(struct fb_info *info, int user)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	unsigned int flags;
	struct db_entry_s *tmp_entry;
	struct list_head *tmp;	

DBPRINTK("PXAFB_RELEASE: current(0x%x--%s), EMPTY list(%d)\n", current, current->comm,list_empty(&(fbi->db_task_list)) );

	if ( list_empty(&(fbi->db_task_list)) ) //Nobody is using LCD double buffers//
		return 0;
	else
	{
		down(&fbi->dbsem);
		tmp = fbi->db_task_list.next;
		while ( tmp != &(fbi->db_task_list) )
		{
			tmp_entry = list_entry(tmp,struct db_entry_s,task_list);
			tmp = tmp->next;

DBPRINTK("DISABLE2BFS:current(%s-%d),task(%d),task_list(0x%x)\n",current->comm,current->pid,tmp_entry->pid,&(tmp_entry->task_list));
//DBPRINTK1("DISABLE2BFS:current(%s-%d),task(%s--%d),task_list(0x%x)\n",current->comm,current->pid,tmp_entry->task->comm,tmp_entry->pid,&(tmp_entry->task_list));


			if (tmp_entry->pid == current->pid)
			{
DBPRINTK("DISABLE2BFS:tmp_entry->task(%d) will be removed\n",tmp_entry->pid);
//DBPRINTK1("DISABLE2BFS:tmp_entry->task(%d--%s) will be removed\n",tmp_entry->pid,tmp_entry->task->comm);
			    list_del(&tmp_entry->task_list);
				kfree(tmp_entry);

				/* If db_task_list contains only one entry, switch to #1 framebuffer */
				if ( (tmp != &(fbi->db_task_list)) && (tmp->next == tmp->prev) )
				{
					DBPRINTK("PXAFB_RELEASE: current(%s-%d),resume 1# framebuffer\n", current->comm,current->pid);

					/* switch to the first framebuffer */
					if (fbi->fb.var.yoffset > fbi->max_yres)  // The second fb is used for LCD panel //
					{
						local_irq_save(flags);

						fbi->dmadesc_fbhigh_cpu = fbi->first_fb.dmadesc_fb_cpu;
						fbi->dmadesc_fbhigh_cpu->fdadr = fbi->first_fb.dmadesc_fb_dma;
						fbi->dmadesc_fbhigh_dma = fbi->first_fb.dmadesc_fb_dma;
						fbi->fdadr0 = fbi->dmadesc_fbhigh_dma;
		
						FBR0 = fbi->fdadr0 | 0x01; //don't generate interrupt bit LCSR0[BS0] //
						
						local_irq_restore(flags);
						
						mdelay(18);  //PCD is 9, frame freq = 55 Frames/Second //

						fbi->fb.var.yoffset = 0;
					}
				}							
			}//current task related db_entry_s is removed
		}

		if ( list_empty(&(fbi->db_task_list)) )
		{
			/* release second framebuffer -- 152KB + PAGE_SIZE */
			consistent_free((void *)fbi->second_fb.map_cpu, fbi->map_size, fbi->second_fb.map_dma);

			fbi->second_fb.map_cpu = NULL;
			fbi->second_fb.map_dma = NULL;
			fbi->second_fb.dmadesc_fb_cpu = NULL;
			fbi->second_fb.dmadesc_fb_dma = NULL;
			fbi->second_fb.screen_cpu = NULL;
			fbi->second_fb.screen_dma = NULL;

DBPRINTK("PXAFB_RELEASE:double_buffers = 0\n");

			double_buffers = 0;
			fbi->fb.fix.smem_len = fbi->max_xres * fbi->max_yres * fbi->max_bpp/8;
			fbi->fb.var.yres_virtual = fbi->fb.var.yres;
			fbi->fb.var.yoffset = 0;
		}
		up(&fbi->dbsem);
	}
	return 0;
}

void pxafb_db_proc(unsigned long *fb_data)
{
	fb_data[0] = pxafbi->fb.fix.smem_len;
	fb_data[1] = pxafbi->fb.var.yres_virtual;
	fb_data[2] = pxafbi->fb.var.yoffset;
	fb_data[3] = pxafbi->first_fb.dmadesc_fb_cpu->fsadr;
	fb_data[4] = pxafbi->second_fb.dmadesc_fb_cpu->fsadr;
}
#endif

static struct fb_ops pxafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	pxafb_get_fix,
	fb_get_var:	pxafb_get_var,
	fb_set_var:	pxafb_set_var,
	fb_get_cmap:	pxafb_get_cmap,
	fb_set_cmap:	pxafb_set_cmap,
#if CONFIG_FBBASE_DOUBLE_BUFFERS
	fb_release:  pxafb_release,
	fb_pan_display:  pxafb_pan_display,
#endif
	fb_ioctl:   pxafb_ioctl,
};

/*
 *  pxafb_switch():       
 *	Change to the specified console.  Palette and video mode
 *      are changed to the console's stored parameters.
 *
 *	Uh oh, this can be called from a tasklet (IRQ)
 */
static int pxafb_switch(int con, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	struct display *disp;
	struct fb_cmap *cmap;

	DPRINTK("con=%d info->modename=%s", con, fbi->fb.modename);

	if (con == fbi->currcon)
		return 0;

	if (fbi->currcon >= 0) {
		disp = fb_display + fbi->currcon;

		/*
		 * Save the old colormap and video mode.
		 */
		disp->var = fbi->fb.var;

		if (disp->cmap.len)
			fb_copy_cmap(&fbi->fb.cmap, &disp->cmap, 0);
	}

	fbi->currcon = con;
	disp = fb_display + con;

	/*
	 * Make sure that our colourmap contains 256 entries.
	 */
	fb_alloc_cmap(&fbi->fb.cmap, 256, 0);

	if (disp->cmap.len)
		cmap = &disp->cmap;
	else
		cmap = fb_default_cmap(1 << disp->var.bits_per_pixel);

	fb_copy_cmap(cmap, &fbi->fb.cmap, 0);

	fbi->fb.var = disp->var;
	fbi->fb.var.activate = FB_ACTIVATE_NOW;

	pxafb_set_var(&fbi->fb.var, con, info);
	return 0;
}

/*
 * Formal definition of the VESA spec:
 *  On
 *  	This refers to the state of the display when it is in full operation
 *  Stand-By
 *  	This defines an optional operating state of minimal power reduction with
 *  	the shortest recovery time
 *  Suspend
 *  	This refers to a level of power management in which substantial power
 *  	reduction is achieved by the display.  The display can have a longer 
 *  	recovery time from this state than from the Stand-by state
 *  Off
 *  	This indicates that the display is consuming the lowest level of power
 *  	and is non-operational. Recovery from this state may optionally require
 *  	the user to manually power on the monitor
 *
 *  Now, the fbdev driver adds an additional state, (blank), where they
 *  turn off the video (maybe by colormap tricks), but don't mess with the
 *  video itself: think of it semantically between on and Stand-By.
 *
 *  So here's what we should do in our fbdev blank routine:
 *
 *  	VESA_NO_BLANKING (mode 0)	Video on,  front/back light on
 *  	VESA_VSYNC_SUSPEND (mode 1)  	Video on,  front/back light off
 *  	VESA_HSYNC_SUSPEND (mode 2)  	Video on,  front/back light off
 *  	VESA_POWERDOWN (mode 3)		Video off, front/back light off
 *
 *  This will match the matrox implementation.
 */
/*
 * pxafb_blank():
 *	Blank the display by setting all palette values to zero.  Note, the 
 * 	12 and 16 bpp modes don't really use the palette, so this will not
 *      blank the display in all modes.  
 */
static void pxafb_blank(int blank, struct fb_info *info)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	int i;

	DPRINTK("pxafb_blank: blank=%d info->modename=%s", blank,
		fbi->fb.modename);

	switch (blank) {
	case VESA_POWERDOWN:
	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
	  /* blank the screen */
	  if (fbi->fb.disp->visual == FB_VISUAL_PSEUDOCOLOR ||
		  fbi->fb.disp->visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
	     for (i = 0; i < fbi->palette_size; i++)
	       pxafb_setpalettereg(i, 0, 0, 0, 0, info);

#if 1
	  /* is this the proper time to call pxafb_blank_helper() */
	  if (pxafb_blank_helper)
	    pxafb_blank_helper(blank);
	  
	  printk("pxafb_blank(): calling device_powerdown()\n");
	  /* if DPM is in use, need to register the fact that the
	     driver disabled the console */
	  device_powerdown(&pxafb_device_ldm);
#else
	  pxafb_schedule_task(fbi, C_DISABLE);
	  if (pxafb_blank_helper)
            pxafb_blank_helper(blank);
#endif
	  break;

	case VESA_NO_BLANKING:
	  if (pxafb_blank_helper)
	    pxafb_blank_helper(blank);
	  if (fbi->fb.disp->visual == FB_VISUAL_PSEUDOCOLOR ||
		  fbi->fb.disp->visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
	    fb_set_cmap(&fbi->fb.cmap, 1, pxafb_setcolreg, info);

#if 1 /* MVL-CEE */
	  device_powerup(&pxafb_device_ldm);
#else
	  pxafb_schedule_task(fbi, C_ENABLE);
#endif
	  break;
	}
}

static int pxafb_updatevar(int con, struct fb_info *info)
{
	DPRINTK("entered");
	return 0;
}

#if defined(CONFIG_CPU_BULVERDE)
static inline int get_lcd_clock_10khz( void )
{
	unsigned int L;

	/* NOTE: calculating the clock from L might be easier than
	   maintaining this array. For example:

	   int k=1;
	   if (L > 16) k = 4;
	   else if (L > 7) k = 2;
	   return (L * 1300)/k);
	*/

	static int lclk_10khz[32] =
		{0, 1300, 2600, 3900, 5200, 6500, 7800, 9100, 5200, 5850,
		 6500, 7150, 7800, 8450, 9100, 9750, 10400, 5525, 5850,
		 6225, 6500, 6825, 7150, 7475, 7800, 8125, 8450, 8775,
		 9100, 9425, 9750, 10075 };

	L = (CCCR&0x1f);
	return	lclk_10khz[L];
}
#endif

/*
 * Calculate the PCD value from the clock rate (in picoseconds).
 * We take account of the PPCR clock setting.
 */
static inline int get_pcd(unsigned int pixclock)
{
	unsigned int pcd;

	if (pixclock) {
#if defined(CONFIG_CPU_BULVERDE)
		DPRINTK("pixclock = %d\n", pixclock );
		pcd = ((get_lcd_clock_10khz() / pixclock) / 2);
		if ( pcd <= 0 ) pcd = 1;
		DPRINTK("lcd clock = %d\n", get_lcd_clock_10khz() );
		DPRINTK("pcd = %d\n", pcd );
#else
		pcd = get_memclk_frequency_10khz() * pixclock;
		pcd /= 100000000;
		pcd += 1;	/* make up for integer math truncations */
#endif
	} else {
		printk(KERN_WARNING "Please convert me to use the PCD calculations\n");
		pcd = 0;
	}
	return pcd;
}

static inline int pxafb_bpp_bits(u_char bpp)
{
	switch (bpp) {
	case 2:
		return 0x1;
	case 4:
		return 0x2;
	case 8:
		return 0x3;
	case 16:
		return 0x4;
#ifdef CONFIG_CPU_BULVERDE
	case 18:
		return 0x6;  /* packed pixel format */
	case 19:
		return 0x8;  /* packed pixel format */
	case 24:
		return 0x9;
	case 25:
		return 0xa;
#endif
	default:
		printk(KERN_ERR "PXA FB: unsupported bpp mode %u\n", bpp);
		return 0x4;
	}
}

/*
 * pxafb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are      
 *      only written to the controller if changes were made.  
 */
static int pxafb_activate_var(struct fb_var_screeninfo *var, struct pxafb_info *fbi)
{
	struct pxafb_lcd_reg new_regs;
#ifdef CONFIG_CPU_BULVERDE	
	u_int pcd = get_pcd(var->pixclock);
#endif
	u_int bytes_per_panel;
	u_long flags;

	DPRINTK("Configuring PXA LCD");

	DPRINTK("var: xres=%d hslen=%d lm=%d rm=%d",
		var->xres, var->hsync_len,
		var->left_margin, var->right_margin);
	DPRINTK("var: yres=%d vslen=%d um=%d bm=%d",
		var->yres, var->vsync_len,
		var->upper_margin, var->lower_margin);

#if DEBUG_VAR
	if (var->xres < 16        || var->xres > 1024)
		printk(KERN_ERR "%s: invalid xres %d\n",
			fbi->fb.fix.id, var->xres);
	if (var->hsync_len < 1    || var->hsync_len > 64)
		printk(KERN_ERR "%s: invalid hsync_len %d\n",
			fbi->fb.fix.id, var->hsync_len);
	if (var->left_margin < 1  || var->left_margin > 255)
		printk(KERN_ERR "%s: invalid left_margin %d\n",
			fbi->fb.fix.id, var->left_margin);
	if (var->right_margin < 1 || var->right_margin > 255)
		printk(KERN_ERR "%s: invalid right_margin %d\n",
			fbi->fb.fix.id, var->right_margin);
	if (var->yres < 1         || var->yres > 1024)
		printk(KERN_ERR "%s: invalid yres %d\n",
			fbi->fb.fix.id, var->yres);
	if (var->vsync_len < 1    || var->vsync_len > 64)
		printk(KERN_ERR "%s: invalid vsync_len %d\n",
			fbi->fb.fix.id, var->vsync_len);
	if (var->upper_margin < 0 || var->upper_margin > 255)
		printk(KERN_ERR "%s: invalid upper_margin %d\n",
			fbi->fb.fix.id, var->upper_margin);
	if (var->lower_margin < 0 || var->lower_margin > 255)
		printk(KERN_ERR "%s: invalid lower_margin %d\n",
			fbi->fb.fix.id, var->lower_margin);
#endif /* DEBUG_VAR */

#if defined (CONFIG_CPU_BULVERDE) 
	new_regs.lccr0 = fbi->lccr0;
	new_regs.lccr1 = LCCR1_BegLnDel(var->left_margin) | LCCR1_EndLnDel(var->right_margin)
		| LCCR1_HorSnchWdth(var->hsync_len) | LCCR1_DisWdth(var->xres);
	new_regs.lccr2 = LCCR2_VrtSnchWdth(var->vsync_len) | LCCR2_BegFrmDel(var->upper_margin)
		| LCCR2_EndFrmDel(var->lower_margin) | LCCR2_DisHght(var->yres);
	new_regs.lccr3 = fbi->lccr3 | pcd | LCCR3_Bpp(pxafb_bpp_bits(var->bits_per_pixel));
#elif defined (CONFIG_PXA_CERF_PDA)
	new_regs.lccr0 = fbi->lccr0;
	new_regs.lccr1 =
		LCCR1_DisWdth(var->xres) +
		LCCR1_HorSnchWdth(var->hsync_len) +
		LCCR1_BegLnDel(var->left_margin) +
		LCCR1_EndLnDel(var->right_margin);
		
	new_regs.lccr2 =
		LCCR2_DisHght(var->yres) +
		LCCR2_VrtSnchWdth(var->vsync_len) +
		LCCR2_BegFrmDel(var->upper_margin) +
		LCCR2_EndFrmDel(var->lower_margin);

	new_regs.lccr3 = fbi->lccr3
		|
		(var->sync & FB_SYNC_HOR_HIGH_ACT ? LCCR3_HorSnchH : LCCR3_HorSnchL) |
		(var->sync & FB_SYNC_VERT_HIGH_ACT ? LCCR3_VrtSnchH : LCCR3_VrtSnchL);
#elif defined (CONFIG_FB_PXA_QVGA)
	new_regs.lccr0 = fbi->lccr0;
	new_regs.lccr1 = LCCR1_BegLnDel(var->left_margin) 
		       | LCCR1_EndLnDel(var->right_margin) 
		       | LCCR1_HorSnchWdth(var->hsync_len) 
		       | LCCR1_DisWdth(var->xres);
	new_regs.lccr2 = LCCR2_VrtSnchWdth(var->vsync_len) 
		       | LCCR2_BegFrmDel(var->upper_margin)
		       | LCCR2_EndFrmDel(var->lower_margin) 
		       | LCCR2_DisHght(var->yres);
	new_regs.lccr3 = fbi->lccr3 
		       | pcd 
		       | LCCR3_Bpp(pxafb_bpp_bits(var->bits_per_pixel));
#else
	/* FIXME using hardcoded values for now */
	new_regs.lccr0 = fbi->lccr0;
        /*	|
		LCCR0_LEN | LCCR0_LDM | LCCR0_BAM |
		LCCR0_ERM | LCCR0_LtlEnd | LCCR0_DMADel(0); */

	new_regs.lccr1 = 0x3030A7F;
        /*	LCCR1_DisWdth(var->xres) +
		LCCR1_HorSnchWdth(var->hsync_len) +
		LCCR1_BegLnDel(var->left_margin) +
		LCCR1_EndLnDel(var->right_margin); */

	new_regs.lccr2 = 0x4EF;
	/*      | LCCR2_DisHght(var->yres) +
		LCCR2_VrtSnchWdth(var->vsync_len) +
		LCCR2_BegFrmDel(var->upper_margin) +
		LCCR2_EndFrmDel(var->lower_margin); */

	new_regs.lccr3 = fbi->lccr3;
	/*      |
		(var->sync & FB_SYNC_HOR_HIGH_ACT ? LCCR3_HorSnchH : LCCR3_HorSnchL) |
		(var->sync & FB_SYNC_VERT_HIGH_ACT ? LCCR3_VrtSnchH : LCCR3_VrtSnchL) |
		LCCR3_ACBsCntOff; */
#endif
	/* if (pcd)
	           new_regs.lccr3 |= LCCR3_PixClkDiv(pcd); */

	DPRINTK("LCCR0:0x%x LCCR1:0x%x LCCR2:0x%x LCCR3:0x%x\n",
		new_regs.lccr0, new_regs.lccr1, new_regs.lccr2, new_regs.lccr3);
	
	/* Update shadow copy atomically */
	local_irq_save(flags);

	/* Setting up DMA descriptors */
	fbi->dmadesc_fblow_cpu = (struct pxafb_dma_descriptor *)((unsigned int)fbi->palette_cpu - 3*16);
	fbi->dmadesc_fbhigh_cpu = (struct pxafb_dma_descriptor *)((unsigned int)fbi->palette_cpu - 2*16);
	fbi->dmadesc_palette_cpu = (struct pxafb_dma_descriptor *)((unsigned int)fbi->palette_cpu - 1*16);

	fbi->dmadesc_fblow_dma = fbi->palette_dma - 3*16;
	fbi->dmadesc_fbhigh_dma = fbi->palette_dma - 2*16;
	fbi->dmadesc_palette_dma = fbi->palette_dma - 1*16;

	if (var->bits_per_pixel <= 16)
		bytes_per_panel = var->xres * var->yres * var->bits_per_pixel / 8;
	else if (var->bits_per_pixel > 19)
		bytes_per_panel = var->xres * var->yres * 4;
	else
		/* 18bpp and 19bpp, packed pixel format */
		bytes_per_panel = var->xres * var->yres * 3;

	if ( fbi->lccr0 & LCCR0_SDS )
		bytes_per_panel >>= 1;
	
	/* Populate descriptors */
	fbi->dmadesc_fblow_cpu->fdadr = fbi->dmadesc_fblow_dma;
	fbi->dmadesc_fblow_cpu->fsadr = fbi->screen_dma + bytes_per_panel;
	fbi->dmadesc_fblow_cpu->fidr  = 0;
	fbi->dmadesc_fblow_cpu->ldcmd = bytes_per_panel;

	fbi->fdadr1 = fbi->dmadesc_fblow_dma; /* only used in dual-panel mode */
		
	fbi->dmadesc_fbhigh_cpu->fsadr = fbi->screen_dma;
	fbi->dmadesc_fbhigh_cpu->fidr = 0;
	fbi->dmadesc_fbhigh_cpu->ldcmd = bytes_per_panel;

	fbi->dmadesc_palette_cpu->fsadr = fbi->palette_dma;
	fbi->dmadesc_palette_cpu->fidr  = 0;
	fbi->dmadesc_palette_cpu->ldcmd = (fbi->palette_size * 2) | LDCMD_PAL;

	if( var->bits_per_pixel < 12)
	{
		/* Assume any mode with <12 bpp is palette driven */
		fbi->dmadesc_palette_cpu->fdadr = fbi->dmadesc_fbhigh_dma;
		fbi->dmadesc_fbhigh_cpu->fdadr = fbi->dmadesc_palette_dma;
		fbi->fdadr0 = fbi->dmadesc_palette_dma; /* flips back and forth between pal and fbhigh */
	}
	else
	{
		/* Palette shouldn't be loaded in true-color mode ! */
		fbi->dmadesc_fbhigh_cpu->fdadr = fbi->dmadesc_fbhigh_dma;
		fbi->fdadr0 = fbi->dmadesc_fbhigh_dma; /* no pal just fbhigh */
	}

#if CONFIG_FBBASE_DOUBLE_BUFFERS
	fbi->first_fb.dmadesc_fb_cpu = fbi->dmadesc_fbhigh_cpu;
	fbi->first_fb.dmadesc_fb_dma = fbi->dmadesc_fbhigh_dma;

//	fbi->second_fb.dmadesc_fb_cpu = fbi->second_fb.screen_cpu -1*16;
//	fbi->second_fb.dmadesc_fb_dma = fbi->second_fb.screen_dma - 1*16;

//	fbi->second_fb.dmadesc_fb_cpu->fdadr = fbi->second_fb.dmadesc_fb_dma;
//	fbi->second_fb.dmadesc_fb_cpu->fsadr = fbi->second_fb.screen_dma;
//	fbi->second_fb.dmadesc_fb_cpu->fidr = 0;
//	fbi->second_fb.dmadesc_fb_cpu->ldcmd = bytes_per_panel;
#endif

#if (0)
	DPRINTK("fbi->dmadesc_fblow_cpu = %p", fbi->dmadesc_fblow_cpu);
	DPRINTK("fbi->dmadesc_fbhigh_cpu = %p", fbi->dmadesc_fbhigh_cpu);
	DPRINTK("fbi->dmadesc_palette_cpu = %p", fbi->dmadesc_palette_cpu);
	DPRINTK("fbi->dmadesc_fblow_dma = 0x%x", fbi->dmadesc_fblow_dma);
	DPRINTK("fbi->dmadesc_fbhigh_dma = 0x%x", fbi->dmadesc_fbhigh_dma);
	DPRINTK("fbi->dmadesc_palette_dma = 0x%x", fbi->dmadesc_palette_dma);

	DPRINTK("fbi->dmadesc_fblow_cpu->fdadr = 0x%x", fbi->dmadesc_fblow_cpu->fdadr);
	DPRINTK("fbi->dmadesc_fbhigh_cpu->fdadr = 0x%x", fbi->dmadesc_fbhigh_cpu->fdadr);
	DPRINTK("fbi->dmadesc_palette_cpu->fdadr = 0x%x", fbi->dmadesc_palette_cpu->fdadr);

	DPRINTK("fbi->dmadesc_fblow_cpu->fsadr = 0x%x", fbi->dmadesc_fblow_cpu->fsadr);
	DPRINTK("fbi->dmadesc_fbhigh_cpu->fsadr = 0x%x", fbi->dmadesc_fbhigh_cpu->fsadr);
	DPRINTK("fbi->dmadesc_palette_cpu->fsadr = 0x%x", fbi->dmadesc_palette_cpu->fsadr);

	DPRINTK("fbi->dmadesc_fblow_cpu->ldcmd = 0x%x", fbi->dmadesc_fblow_cpu->ldcmd);
	DPRINTK("fbi->dmadesc_fbhigh_cpu->ldcmd = 0x%x", fbi->dmadesc_fbhigh_cpu->ldcmd);
	DPRINTK("fbi->dmadesc_palette_cpu->ldcmd = 0x%x", fbi->dmadesc_palette_cpu->ldcmd);

#if CONFIG_FBBASE_DOUBLE_BUFFERS
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu = %p", fbi->second_fb.dmadesc_fb_cpu);
	DPRINTK("fbi->second_fb.dmadesc_fb_dma = 0x%x", fbi->second_fb.dmadesc_fb_dma);
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu->fdadr = 0x%x", fbi->second_fb.dmadesc_fb_cpu->fdadr);
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu->fsadr = 0x%x", fbi->second_fb.dmadesc_fb_cpu->fsadr);
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu->ldcmd = 0x%x", fbi->second_fb.dmadesc_fb_cpu->ldcmd);

#endif

#endif

	DPRINTK("fbi->dmadesc_fblow_cpu = 0x%x\n", fbi->dmadesc_fblow_cpu);
	DPRINTK("fbi->dmadesc_fbhigh_cpu = 0x%x\n", fbi->dmadesc_fbhigh_cpu);
	DPRINTK("fbi->dmadesc_palette_cpu = 0x%x\n", fbi->dmadesc_palette_cpu);
	DPRINTK("fbi->dmadesc_fblow_dma = 0x%x\n", fbi->dmadesc_fblow_dma);
	DPRINTK("fbi->dmadesc_fbhigh_dma = 0x%x\n", fbi->dmadesc_fbhigh_dma);
	DPRINTK("fbi->dmadesc_palette_dma = 0x%x\n", fbi->dmadesc_palette_dma);

	DPRINTK("fbi->dmadesc_fblow_cpu->fdadr = 0x%x\n", fbi->dmadesc_fblow_cpu->fdadr);
	DPRINTK("fbi->dmadesc_fbhigh_cpu->fdadr = 0x%x\n", fbi->dmadesc_fbhigh_cpu->fdadr);
	DPRINTK("fbi->dmadesc_palette_cpu->fdadr = 0x%x\n", fbi->dmadesc_palette_cpu->fdadr);

	DPRINTK("fbi->dmadesc_fblow_cpu->fsadr = 0x%x\n", fbi->dmadesc_fblow_cpu->fsadr);
	DPRINTK("fbi->dmadesc_fbhigh_cpu->fsadr = 0x%x\n", fbi->dmadesc_fbhigh_cpu->fsadr);
	DPRINTK("fbi->dmadesc_palette_cpu->fsadr = 0x%x\n", fbi->dmadesc_palette_cpu->fsadr);

	DPRINTK("fbi->dmadesc_fblow_cpu->ldcmd = 0x%x\n", fbi->dmadesc_fblow_cpu->ldcmd);
	DPRINTK("fbi->dmadesc_fbhigh_cpu->ldcmd = 0x%x\n", fbi->dmadesc_fbhigh_cpu->ldcmd);
	DPRINTK("fbi->dmadesc_palette_cpu->ldcmd = 0x%x\n", fbi->dmadesc_palette_cpu->ldcmd);

#if CONFIG_FBBASE_DOUBLE_BUFFERS
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu = 0x%x\n", fbi->second_fb.dmadesc_fb_cpu);
	DPRINTK("fbi->second_fb.dmadesc_fb_dma = 0x%x\n", fbi->second_fb.dmadesc_fb_dma);
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu->fdadr = 0x%x\n", fbi->second_fb.dmadesc_fb_cpu->fdadr);
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu->fsadr = 0x%x\n", fbi->second_fb.dmadesc_fb_cpu->fsadr);
	DPRINTK("fbi->second_fb.dmadesc_fb_cpu->ldcmd = 0x%x\n", fbi->second_fb.dmadesc_fb_cpu->ldcmd);

#endif


	fbi->reg_lccr0 = new_regs.lccr0;
	fbi->reg_lccr1 = new_regs.lccr1;
	fbi->reg_lccr2 = new_regs.lccr2;
	fbi->reg_lccr3 = new_regs.lccr3;

	local_irq_restore(flags);

	/*
	 * Only update the registers if the controller is enabled
	 * and something has changed.
	 */
	if ((LCCR0 != fbi->reg_lccr0)       || (LCCR1 != fbi->reg_lccr1) ||
	    (LCCR2 != fbi->reg_lccr2)       || (LCCR3 != fbi->reg_lccr3) ||
	    (FDADR0 != fbi->fdadr0) || (FDADR1 != fbi->fdadr1))
		pxafb_schedule_task(fbi, C_REENABLE);

	return 0;
}

/*
 * NOTE!  The following functions are purely helpers for set_ctrlr_state.
 * Do not call them directly; set_ctrlr_state does the correct serialisation
 * to ensure that things happen in the right way 100% of time time.
 *	-- rmk
 */

/*
 * FIXME: move LCD power stuff into pxafb_power_up_lcd()
 * Also, I'm expecting that the backlight stuff should
 * be handled differently.
 */
/*
static void pxafb_backlight_on(struct pxafb_info *fbi)
{
	DPRINTK("backlight on");

#ifdef CONFIG_ARCH_PXA_IDP
	if(machine_is_pxa_idp()) {	
		FB_BACKLIGHT_ON();
	}
#elif defined(CONFIG_CPU_BULVERDE)
	if (machine_is_mainstone()) {
		CKEN |= CKEN0_PWM0;
		PWM_CTRL0 = 0;
		PWM_PWDUTY0 = 0x3FF;
		PWM_PERVAL0 = 0x3FF;
	}
#endif
}
*/

/*
 * FIXME: move LCD power stuf into pxafb_power_down_lcd()
 * Also, I'm expecting that the backlight stuff should
 * be handled differently.
 */
/*
static void pxafb_backlight_off(struct pxafb_info *fbi)
{
	DPRINTK("backlight off");

#ifdef CONFIG_ARCH_PXA_IDP
	if(machine_is_pxa_idp()) {
		FB_BACKLIGHT_OFF();
	}
#elif defined(CONFIG_CPU_BULVERDE)
	if (machine_is_mainstone()) {
		PWM_CTRL0 = 0;
		PWM_PWDUTY0 = 0x0;
		PWM_PERVAL0 = 0x3FF;
		CKEN &= ~CKEN0_PWM0;
	}
#endif
	
}
*/

static void pxafb_power_up_lcd(struct pxafb_info *fbi)
{
	DPRINTK("LCD power on");
	CKEN |= CKEN16_LCD;

	if(machine_is_pxa_cerf()) {
		lcdctrl_enable();
	}

#if CONFIG_ARCH_PXA_IDP
	/* set GPIOs, etc */
	if(machine_is_pxa_idp()) {
		// FIXME need to add proper delays
		FB_PWR_ON();
		FB_VLCD_ON();	// FIXME this should be after scanning starts
	}
#endif
#ifdef CONFIG_CPU_BULVERDE
	CKEN |= CKEN0_PWM0;
#endif
}

static void pxafb_power_down_lcd(struct pxafb_info *fbi)
{
	DPRINTK("LCD power off");
	CKEN &= ~CKEN16_LCD;

	if(machine_is_pxa_cerf()) {
		lcdctrl_disable();
	}

	/* set GPIOs, etc */
#if CONFIG_ARCH_PXA_IDP
	if(machine_is_pxa_idp()) {
		// FIXME need to add proper delays
		FB_PWR_OFF();
		FB_VLCD_OFF();	// FIXME this should be before scanning stops
	}
#endif
#ifdef CONFIG_CPU_BULVERDE
	CKEN &= ~CKEN0_PWM0;
#endif
}

static void pxafb_setup_gpio(struct pxafb_info *fbi)
{
	unsigned int lccr0;

	/*
	 * setup is based on type of panel supported
	 */

	lccr0 = fbi->lccr0;

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
        else if (((lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_DPD))) ||
                 (!(lccr0 & LCCR0_CMS) && !(lccr0 & LCCR0_PAS) && !(lccr0 & LCCR0_SDS)))
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
		switch (fbi->max_bpp) {
		case 16:
			// bits 58-77
			GPDR1 |= (0x3f << 26);
			GPDR2 |= 0x00003fff;
			
			GAFR1_U = (GAFR1_U & ~(0xfff << 20)) | (0xaaa << 20);
			GAFR2_L = (GAFR2_L & 0xf0000000) | 0x0aaaaaaa;
			break;
#ifdef CONFIG_CPU_BULVERDE
		case 18:
		case 19:
		case 24:
		case 25:
			// bits 58-77 and 86, 87
			GPDR1 |= (0x3f << 26);
			GPDR2 |= 0x00c03fff;
			
			GAFR1_U = (GAFR1_U & ~(0xfff << 20)) | (0xaaa << 20);
			GAFR2_L = (GAFR2_L & 0xf0000000) | 0x0aaaaaaa;
			GAFR2_U = (GAFR2_U & 0xffff0fff) | 0xa000;
			break;
#endif
		default:
			break;
		}
        }
	else
		printk(KERN_ERR "pxafb_setup_gpio: unable to determine bits per pixel\n");
}

static void pxafb_enable_controller(struct pxafb_info *fbi)
{
	DPRINTK("Enabling LCD controller");

	/* power_state is initialized in this structure to
	   DPM_POWER_OFF; make sure to initialize it to ON when the
	   controller is enabled.

	   Must use the global since this function has no other access
	   to pxafb_device_ldm.
	*/
	pxafb_device_ldm.power_state = DPM_POWER_ON;
#ifdef CONFIG_CPU_BULVERDE
	/* workaround for insight 41187 */
	OVL1C2 = 0;
	OVL1C1 = 0;
	OVL2C2 = 0;
	OVL2C1 = 0;
	CCR = 0;

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

	FDADR0 = fbi->fdadr0;
	FDADR1 = fbi->fdadr1;

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
//	mdelay(20); /* LCD timing delay*/
	mdelay(1); /* new delay time from xushiwei.  LCD timing delay*/
	/* This code needed to enable the LCD screen */
	GPSR3 = 0x00100000; /* enable GPIO<116> (LCD_OFF) */
	mdelay(10);
	GPCR3 = 0x00100000; /* new lCD is enabled by low level*/
	GPDR3 |= 0x00100000;

	bLCDOn = 1;
	
	DPRINTK("FDADR0 = 0x%08x\n", (unsigned int)FDADR0);
	DPRINTK("FDADR1 = 0x%08x\n", (unsigned int)FDADR1);
	DPRINTK("LCCR0 = 0x%08x\n", (unsigned int)LCCR0);
	DPRINTK("LCCR1 = 0x%08x\n", (unsigned int)LCCR1);
	DPRINTK("LCCR2 = 0x%08x\n", (unsigned int)LCCR2);
	DPRINTK("LCCR3 = 0x%08x\n", (unsigned int)LCCR3);
}

static void pxafb_disable_controller(struct pxafb_info *fbi)
{
#if 0
	/* Unnecessary if nothing is put on the wait_queue */
	DECLARE_WAITQUEUE(wait, current);
#endif

	DPRINTK("Disabling LCD controller");

	pxafb_device_ldm.power_state = DPM_POWER_OFF;

	/* FIXME add power down GPIO stuff here */

#if 0
	/* This function tried to schedule a 20msec "sleep" which is
	   invalid (intentional kernel panic) when this function is
	   called from the interrupt for screen blanking.

	   The delay appears to not be necessary, so it has been
	   removed (by this #if 0 block) along with accessing the
	   wait_queue
	*/

	add_wait_queue(&fbi->ctrlr_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);
#endif
	DBPRINTK("pxafb_disable_controller: FBR0 (0x%x)\n", FBR0);
	while(FBR0 & 0x1)  //waiting for the completion of concurrent FBR branch cmd//
	;
	LCSR0 = 0xffffffff;	/* Clear LCD Status Register */
	
	/* needed to turn off LCD screen */
	GPSR3 = 0x00100000;/* new LCD is disabled by high level*/
	PGSR3 |= 0x00100000;  // set sleep state of GPIO<116> -- LCD_OFF //
	
	mdelay(41);
	
#if (0)  //Susan -- original sequence //
	LCCR0 &= ~LCCR0_LDM;	/* Enable LCD Disable Done Interrupt */
	LCCR0 &= ~LCCR0_ENB;	/* Disable LCD Controller */
#endif	
	LCCR0 &= ~LCCR0_LDM;	/* Enable LCD Disable Done Interrupt */
//	LCCR0 &= ~LCCR0_ENB;	/* Disable LCD Controller */
	LCCR0 |= LCCR0_DIS;   //Normal disable LCD //
	mdelay(18);

        bLCDOn = 0;

#if 0
	/* Removing the 20msec  delay as described above */
	schedule_timeout(20 * HZ / 1000);
	current->state = TASK_RUNNING;
	remove_wait_queue(&fbi->ctrlr_wait, &wait);
#endif
}

/*
 *  pxafb_handle_irq: Handle 'LCD DONE' interrupts.
 */
static void pxafb_handle_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct pxafb_info *fbi = dev_id;
	unsigned int lcsr0 = LCSR0;

#if CONFIG_FBBASE_DOUBLE_BUFFERS

DBPRINTK("PXAFB_HANDLE_IRQ: LCSR0(0x%x)\n",LCSR0);

	if (lcsr0 & LCSR0_BS)
	{
#if (0)
		/* This interrupt status bit has been set after load the branched DMA descriptor */
		br_duration = OSCR - br_duration;
		DBPRINTK("BR_DURATION is %d OSCR counts\n", br_duration);
#endif		
		LCSR0 |= LCSR0_BS;  //clear BS0 for the sake of subsequent setting of BS0 bit //

DBPRINTK("PXAFB_HANDLE_IRQ: LCSR0_BS is set,wakeup pandisplay\n");

		wake_up(&fbi->ctrlr_wait);
	}
#endif

	if (lcsr0 & LCSR0_LDD) {
		LCCR0 |= LCCR0_LDM;
		wake_up(&fbi->ctrlr_wait);
	}
}

void delay_bklight()
{
	if (bklight_timer.function)
		mod_timer(&bklight_timer, jiffies + bk_timeout);
}

/*
 * This function must be called from task context only, since it will
 * sleep when disabling the LCD controller, or if we get two contending
 * processes trying to alter state.
 */
static void set_ctrlr_state(struct pxafb_info *fbi, u_int state)
{
	u_int old_state;

	down(&fbi->ctrlr_sem);

	old_state = fbi->state;

	switch (state) {
	case C_DISABLE_CLKCHANGE:
		/*
		 * Disable controller for clock change.  If the
		 * controller is already disabled, then do nothing.
		 */
		if (old_state != C_DISABLE) {
			fbi->state = state;
			pxafb_disable_controller(fbi);
#ifdef CONFIG_CPU_BULVERDE
			DISABLE_OVERLAYS(fbi);
#endif
		}
		break;

	case C_DISABLE:
		/*
		 * Disable controller
		 */
		if (old_state != C_DISABLE) 
		{
#if CONFIG_FBBASE_DOUBLE_BUFFERS
			down(&fbi->dbsem);  //make sure Branch cmd has been finished completely //
#endif			
			fbi->state = state;
			DPRINTK("\npxafb_set_ctrl_state: enter C_DISABLE state.");

			del_timer(&bklight_timer);
			pxafb_ezx_Backlight_turn_off();

/*
			pxafb_backlight_off(fbi);
*/

			if (old_state != C_DISABLE_CLKCHANGE)
				pxafb_disable_controller(fbi);
			pxafb_power_down_lcd(fbi);
#ifdef CONFIG_CPU_BULVERDE
			DISABLE_OVERLAYS(fbi);
#endif

#if CONFIG_FBBASE_DOUBLE_BUFFERS
			up(&fbi->dbsem);
#endif
		}
		break;

	case C_ENABLE_CLKCHANGE:
		/*
		 * Enable the controller after clock change.  Only
		 * do this if we were disabled for the clock change.
		 */
		if (old_state == C_DISABLE_CLKCHANGE) {
			fbi->state = C_ENABLE;
			pxafb_enable_controller(fbi);
#ifdef CONFIG_CPU_BULVERDE
			ENABLE_OVERLAYS(fbi);
#endif
		}
		break;

	case C_REENABLE:
		/*
		 * Re-enable the controller only if it was already
		 * enabled.  This is so we reprogram the control
		 * registers.
		 */
		if (old_state == C_ENABLE) {
			pxafb_disable_controller(fbi);
			pxafb_setup_gpio(fbi);
			pxafb_enable_controller(fbi);
#ifdef CONFIG_CPU_BULVERDE
			DISABLE_OVERLAYS(fbi);
			ENABLE_OVERLAYS(fbi);
#endif
		}
		break;

	case C_ENABLE:
		/*
		 * Power up the LCD screen, enable controller, and
		 * turn on the backlight.
		 */
		if (old_state != C_ENABLE) {
			fbi->state = C_ENABLE;
			pxafb_setup_gpio(fbi);
			pxafb_power_up_lcd(fbi);
			pxafb_enable_controller(fbi);

			// if power up, bklight turned on only after handshake finished

			if (handshake_pass())
				mod_timer(&bklight_timer, jiffies + bk_timeout);

		}
#ifdef CONFIG_CPU_BULVERDE
		ENABLE_OVERLAYS(fbi);
#endif
		break;
	}
	up(&fbi->ctrlr_sem);
}

/*
 * Our LCD controller task (which is called when we blank or unblank)
 * via keventd.
 */
static void pxafb_task(void *dummy)
{
	struct pxafb_info *fbi = dummy;
	u_int state = xchg(&fbi->task_state, -1);

	set_ctrlr_state(fbi, state);
}

#ifdef CONFIG_CPU_FREQ
/*
 * CPU clock speed change handler.  We need to adjust the LCD timing
 * parameters when the CPU clock is adjusted by the power management
 * subsystem.
 */
static int
pxafb_clkchg_notifier(struct notifier_block *nb, unsigned long val,
			 void *data)
{
	struct pxafb_info *fbi = TO_INF(nb, clockchg);
	u_int pcd;

	switch (val) {
	case CPUFREQ_MINMAX:
		/* todo: fill in min/max values */
		break;

	case CPUFREQ_PRECHANGE:
		set_ctrlr_state(fbi, C_DISABLE_CLKCHANGE);
		break;

	case CPUFREQ_POSTCHANGE:
		pcd = get_pcd(fbi->fb.var.pixclock);
		fbi->reg_lccr3 = (fbi->reg_lccr3 & ~0xff) | LCCR3_PixClkDiv(pcd);
		set_ctrlr_state(fbi, C_ENABLE_CLKCHANGE);
#ifdef CONFIG_CPU_BULVERDE
		set_ctrlr_state(fbi, C_REENABLE);
#endif
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_PM
/*
 * Power management hook.  Note that we won't be called from IRQ context,
 * unlike the blank functions above, so we may sleep.
 */
static int
pxafb_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	struct pxafb_info *fbi = pm_dev->data;

	DPRINTK("pm_callback: %d", req);

	if (req == PM_SUSPEND || req == PM_RESUME) {
		int state = (int)data;

		if (state == 0) {
			/* Enter D0. */
			set_ctrlr_state(fbi, C_ENABLE);
#ifdef CONFIG_CPU_BULVERDE
			set_ctrlr_state(fbi, C_REENABLE);
#endif
		} else {
			/* Enter D1-D3.  Disable the LCD controller.  */
			set_ctrlr_state(fbi, C_DISABLE);
		}
	}
	DPRINTK("done");
	return 0;
}
#endif

/*
 * pxafb_map_video_memory():
 *      Allocates the DRAM memory for the frame buffer.  This buffer is  
 *	remapped into a non-cached, non-buffered, memory region to  
 *      allow palette and pixel writes to occur without flushing the 
 *      cache.  Once this area is remapped, all virtual memory
 *      access to the video memory should occur at the new region.
 */
static int __init pxafb_map_video_memory(struct pxafb_info *fbi)
{
	u_long palette_mem_size;

	/*
	 * We reserve one page for the palette, plus the size
	 * of the framebuffer.
	 *
	 * layout of stuff in memory
	 *
	 *                fblow descriptor
	 *                fbhigh descriptor
	 *                palette descriptor
	 *                palette
	 *   page boundary->
	 *                frame buffer
	 */

	fbi->map_size = PAGE_ALIGN(fbi->fb.fix.smem_len + PAGE_SIZE);

#if (0)
#if  CONFIG_FBBASE_DOUBLE_BUFFERS
	double_buffers = 1;
	fbi->fb.fix.smem_len = PAGE_ALIGN(fbi->fb.fix.smem_len) * 2;
#endif
#endif

#ifdef CONFIG_CPU_BULVERDE
	if (fbi->map_size < 0x40000) {
		DPRINTK("try to put fb in SRAM...\n");
		if (sram_access_obtain(&sram_start, &sram_size) < 0) {
			printk(KERN_WARNING "pxafb: can't access SRAM for fb\n");
			fbi->map_cpu = __consistent_alloc
				(GFP_KERNEL, fbi->map_size,
				 &fbi->map_dma, PTE_BUFFERABLE);
		}
		else {
			DPRINTK("SRAM fb ok\n");
			fbi->map_cpu = (u_char *)sram_start;
			fbi->map_dma = (dma_addr_t)SRAM_MEM_PHYS;
		}
	}
	else
#endif
		fbi->map_cpu = __consistent_alloc
			(GFP_KERNEL, fbi->map_size,
			 &fbi->map_dma, PTE_BUFFERABLE);

#if (0)  //The second framebuffer should be allocated dynamically//
#if CONFIG_FBBASE_DOUBLE_BUFFERS

		if (fbi->map_cpu)
		{
			fbi->second_fb.map_cpu = __consistent_alloc(GFP_KERNEL,fbi->map_size,&(fbi->second_fb.map_dma),PTE_BUFFERABLE);
			if (!fbi->second_fb.map_cpu)
				fbi->map_cpu = 0;
		}

#endif
#endif

	if (fbi->map_cpu) {
		memset((void *)fbi->map_cpu, 0, fbi->map_size);
		fbi->screen_cpu = fbi->map_cpu + PAGE_SIZE;
		fbi->screen_dma = fbi->map_dma + PAGE_SIZE;
		fbi->fb.fix.smem_start = fbi->screen_dma;

		fbi->palette_size = fbi->fb.var.bits_per_pixel == 256;
		palette_mem_size = fbi->palette_size * sizeof(u16);

		DPRINTK("palette_mem_size = 0x%08lx", (u_long) palette_mem_size);

		fbi->palette_cpu = (u16 *)(fbi->map_cpu + PAGE_SIZE - palette_mem_size);
		fbi->palette_dma = fbi->map_dma + PAGE_SIZE - palette_mem_size;

#if CONFIG_FBBASE_DOUBLE_BUFFERS
		init_MUTEX(&fbi->dbsem);
		INIT_LIST_HEAD(&(fbi->db_task_list));
		fbi->first_fb.map_cpu = fbi->map_cpu;
		fbi->first_fb.map_dma = fbi->map_dma;
		fbi->first_fb.screen_cpu = fbi->screen_cpu;
		fbi->first_fb.screen_dma = fbi->screen_dma;

		fbi->second_fb.map_cpu = NULL;

#endif

	}

#if (0)
#if CONFIG_FBBASE_DOUBLE_BUFFERS
	if (fbi->second_fb.map_cpu)
	{
		memset((void *)fbi->second_fb.map_cpu, 0, fbi->map_size);
		fbi->second_fb.screen_cpu = fbi->second_fb.map_cpu + PAGE_SIZE;
		fbi->second_fb.screen_dma = fbi->second_fb.map_dma + PAGE_SIZE;
	}
	
#endif
#endif


	return fbi->map_cpu ? 0 : -ENOMEM;
}

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	30000, 70000, 50, 65, 0	/* Generic */
};

static int dummy_updatevar(int con, struct fb_info *info)
{
	DPRINTK("entered");
	return 0;
}

#ifdef CONFIG_CPU_BULVERDE
/* 
 * LCD enhancement : Overlay 1 
 *
 * Features:
 * - support 16bpp (No palatte)
 */
static int overlay1fb_open(struct fb_info *info, int user)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	int ret = 0;

	down(&fbi->mutex);

	if (fbi->refcount) 
		ret = -EACCES;
	else
		fbi->refcount ++;

	up(&fbi->mutex);

	/* Initialize the variables in overlay1 framebuffer. */
	fbi->fb.var.xres = fbi->fb.var.yres = 0;
	fbi->fb.var.bits_per_pixel = 0;

	return ret;
}

static int overlay1fb_release(struct fb_info *info, int user)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	down(&fbi->mutex);

	if (fbi->refcount)
		fbi->refcount --;

	up(&fbi->mutex);
	/* disable overlay when released */
	overlay1fb_blank(1, info);

	return 0;
}

/*
 * Overlay 1: 16 bpp, 24 bpp (no palette)
 */
static int 
overlay1fb_map_video_memory(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	if (fbi->map_cpu) 
		consistent_free((void*)fbi->map_cpu, fbi->map_size, fbi->map_dma);

	fbi->map_size = PAGE_ALIGN(fbi->fb.fix.smem_len + PAGE_SIZE);
	fbi->map_cpu = (unsigned long)__consistent_alloc(GFP_KERNEL, fbi->map_size,
							 &fbi->map_dma, PTE_BUFFERABLE);
	
	if (!fbi->map_cpu) return -ENOMEM;

	memset((void *)fbi->map_cpu, 0, fbi->map_size);
	fbi->screen_cpu = fbi->map_cpu + PAGE_SIZE;
	fbi->screen_dma = fbi->map_dma + PAGE_SIZE;
	
	fbi->fb.fix.smem_start = fbi->screen_dma;

	/* setup dma descriptor */
	fbi->dma1 = (struct pxafb_dma_descriptor*)
		(fbi->screen_cpu - sizeof(struct pxafb_dma_descriptor));

	fbi->dma1->fdadr = (fbi->screen_dma - sizeof(struct pxafb_dma_descriptor));
	fbi->dma1->fsadr = fbi->screen_dma;
	fbi->dma1->fidr  = 0;
	fbi->dma1->ldcmd = fbi->fb.fix.smem_len;

	return 0;
}

static int
overlay1fb_enable(struct fb_info *info) 
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	unsigned long bpp1;

	if (!fbi->map_cpu) return -EINVAL;
	switch(fbi->fb.var.bits_per_pixel){
	case 16:
		bpp1 = 0x4;
		break;
	case 18:
		bpp1 = 0x6;
		break;
	case 19:
		bpp1 = 0x8;
		break;
	case 24:
		bpp1 = 0x9;
		break;
	case 25:
		bpp1 = 0xa;
		break;
	default:
		return -EINVAL;
	}

	/* disable branch/start/end of frame interrupt */
	LCCR5 |= (LCCR5_IUM1 | LCCR5_BSM1 | LCCR5_EOFM1 | LCCR5_SOFM1);

	/* disable overlay 1 window */
	OVL1C1 &= ~(1<<31);

	if (!fbi->enabled) {
		FDADR1 = (fbi->dma1->fdadr);
		fbi->enabled = 1;
	}
	else {
		FBR1 = fbi->dma1->fdadr | 0x1;
	}

	OVL1C2 = (fbi->ypos << 10) | fbi->xpos;
	OVL1C1 = (bpp1 << 20) | ((fbi->fb.var.yres-1)<<10) | (fbi->fb.var.xres-1);

	/* enable overlay 1 window */
	OVL1C1 |= (1<<31);

	fbi->state = C_ENABLE;

	return 0;
}

static void overlay1fb_disable(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*)info;

	fbi->state = C_DISABLE;

	/* clear O1EN */
	OVL1C1 &= ~(0x1<<31);
}

static void overlay1fb_blank(int blank, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	switch(blank)
	{
	case 0:
		set_ctrlr_state(fbi->basefb, C_REENABLE);
		overlay1fb_enable(info);
		break;
	case 1:
		fbi->enabled = 0;

		overlay1fb_disable(info);

		/* When you disabled overlay 1, the block still is displayed
		   on LCD. you should re-enable LCD base frame.
		 */
		set_ctrlr_state(fbi->basefb, C_REENABLE);
		break;
	default:
		/* reserved */
		break;
	}
}

static int
overlay1fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	*fix = fbi->fb.fix;

	return 0;
}

static int
overlay1fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	*var = fbi->fb.var;

	var->nonstd = (fbi->ypos << 10) | fbi->xpos;

	return 0;
}

static int 
overlay1fb_validate_var( struct fb_var_screeninfo *var, struct overlayfb_info *fbi)
{
	int xpos, ypos;

	/* must in base frame */
	xpos = (var->nonstd & 0x3ff);
	ypos = ((var->nonstd>>10) & 0x3ff);

	if ( (xpos + var->xres) > fbi->basefb->max_xres )
		return -EINVAL;

	if ( (ypos + var->yres) > fbi->basefb->max_yres ) 
		return -EINVAL;

	return 0;
}

static int
overlay1fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi=(struct overlayfb_info*)info;
	struct fb_var_screeninfo *dvar = &fbi->fb.var;
	int nbytes=0, err=0, pixels_per_line=0;
	int xpos=0, ypos=0;
	int changed = 0;

	/* validate parameters*/
	err = overlay1fb_validate_var(var, fbi);
	if (err) return  err;

	xpos = var->nonstd & 0x3ff;
	ypos = (var->nonstd>>10) & 0x3ff;

	if ( (fbi->xpos != xpos) || (fbi->ypos != ypos) ) {
		fbi->xpos = xpos;
		fbi->ypos = ypos;
	}

	switch (var->bits_per_pixel) {
	case 16:
		if ( var->xres & 0x1 ) {
			printk("xres should be a multiple of 2 pixels!\n");
			return -EINVAL;
		}
		break;
	case 18:
	case 19:
		if ( var->xres & 0x7 ) {
			printk("xres should be a multiple of 8 pixels!\n");
			return -EINVAL;
		}
		break;
	default:
		break;
	}

	if ( (var->xres != dvar->xres)	||
		(var->yres != dvar->yres) ||
		(var->bits_per_pixel != dvar->bits_per_pixel) ) {

		/* update var_screeninfo fields*/
		*dvar = *var;

		switch(var->bits_per_pixel) {
		case 16:
			/* 2 pixels per line */ 
			pixels_per_line = (fbi->fb.var.xres + 0x1) & (~0x1);
			nbytes = 2;

			dvar->red    = def_rgbt_16.red;
			dvar->green  = def_rgbt_16.green;
			dvar->blue   = def_rgbt_16.blue;
			dvar->transp = def_rgbt_16.transp;

			break;
#ifdef CONFIG_CPU_BULVERDE
		case 18:
		case 19:
			pixels_per_line = (fbi->fb.var.xres + 0x3 ) & (~0x3);
			nbytes = 3;

			dvar->red    = def_rgbt_18.red;
			dvar->green  = def_rgbt_18.green;
			dvar->blue   = def_rgbt_18.blue;
			dvar->transp = def_rgbt_18.transp;

			break;
#endif
		case 24:
#ifdef CONFIG_CPU_BULVERDE
		case 25:
#endif
			pixels_per_line = fbi->fb.var.xres;
			nbytes = 4;

			dvar->red    = def_rgbt_24.red;
			dvar->green  = def_rgbt_24.green;
			dvar->blue   = def_rgbt_24.blue;
			dvar->transp = def_rgbt_24.transp;

			break;
		}
	
		/* update fix_screeninfo fields */

		fbi->fb.fix.line_length = nbytes * pixels_per_line;
		fbi->fb.fix.smem_len = fbi->fb.fix.line_length * fbi->fb.var.yres;
		err= overlay1fb_map_video_memory((struct fb_info*)fbi);

		if (err) return err;
		
	}

	if (changed) overlay1fb_enable((struct fb_info*)fbi);

	return 0;
}


/* we don't have cmap */
static int
overlay1fb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	return 0;
}

static int
overlay1fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	return -EINVAL;
}

static struct fb_ops overlay1fb_ops = {
	owner:		THIS_MODULE,
	fb_open:	overlay1fb_open,
	fb_release:	overlay1fb_release,
	fb_get_fix:	overlay1fb_get_fix,
	fb_get_var:	overlay1fb_get_var,
	fb_set_var:	overlay1fb_set_var,
	fb_get_cmap:	overlay1fb_get_cmap,
	fb_set_cmap:	overlay1fb_set_cmap,
};

/*
 * LCD enhancement : Overlay 2
 *
 * Features:
 * - support planar YCbCr420/YCbCr422/YCbCr444;
 */
static int overlay2fb_open(struct fb_info *info, int user)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	int ret = 0;

	down(&fbi->mutex);

	if (fbi->refcount)
		ret = -EACCES;
	else
		fbi->refcount ++;

	up(&fbi->mutex);
	fbi->fb.var.xres = fbi->fb.var.yres = 0;

	yuv420_enabled = 0;

	return ret;
}

static int overlay2fb_release(struct fb_info *info, int user)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	down(&fbi->mutex);

	if (fbi->refcount)
		fbi->refcount --;

	up(&fbi->mutex);

	/* disable overlay when released */
	overlay2fb_blank(1, info);

	return 0;
}

/*
 * Overlay 2 - support planar  YCbCr420/YCbCr422/YCbCr444
 */
static int 
overlay2fb_map_video_memory( struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	unsigned int ylen, cblen, crlen, aylen, acblen, acrlen;
	unsigned int yoff, cboff, croff;
	unsigned int xres,yres; 
	unsigned int nbytes;

	ylen = cblen = crlen = aylen = acblen = acrlen = 0;
	yoff = cboff = croff = 0;


	if (fbi->map_cpu) 
		consistent_free((void*)fbi->map_cpu, fbi->map_size, fbi->map_dma);

	yres = fbi->fb.var.yres;

	switch(fbi->format) {
	case 0x4: /* YCbCr 4:2:0 planar */
		/* 16 pixels per line */
		xres = (fbi->fb.var.xres + 0xf) & (~0xf);
		fbi->fb.fix.line_length = xres;

		nbytes = xres * yres;
		ylen = nbytes;
		cblen = crlen = (nbytes/4);

		break;
	case 0x3: /* YCbCr 4:2:2 planar */
		/* 8 pixles per line */
		xres = (fbi->fb.var.xres + 0x7) & (~0x7);
		fbi->fb.fix.line_length = xres;

		nbytes = xres * yres;
		ylen  = nbytes;
		cblen = crlen = (nbytes/2);

		break;
	case 0x2: /* YCbCr 4:4:4 planar */
		/* 4 pixels per line */
		xres = (fbi->fb.var.xres + 0x3) & (~0x3);
		fbi->fb.fix.line_length = xres;

		nbytes = xres * yres;
		ylen  = cblen = crlen = nbytes;
		break;
	}

	/* 16-bytes alignment for DMA */
	aylen  = (ylen + 0xf) & (~0xf);
	acblen = (cblen + 0xf) & (~0xf);
	acrlen = (crlen + 0xf) & (~0xf);

	fbi->fb.fix.smem_len = aylen + acblen + acrlen;

	/* alloc memory */

	fbi->map_size = PAGE_ALIGN(fbi->fb.fix.smem_len + PAGE_SIZE);
	fbi->map_cpu = (unsigned long)__consistent_alloc(GFP_KERNEL, fbi->map_size,
							 &fbi->map_dma, PTE_BUFFERABLE);
	
	if (!fbi->map_cpu) return -ENOMEM;

	memset((void *)fbi->map_cpu, 0, fbi->map_size);
	fbi->screen_cpu = fbi->map_cpu + PAGE_SIZE;
	fbi->screen_dma = fbi->map_dma + PAGE_SIZE;
	
	fbi->fb.fix.smem_start = fbi->screen_dma;

	/* setup dma for Planar format */
	fbi->dma2 = (struct pxafb_dma_descriptor*)
		(fbi->screen_cpu - sizeof(struct pxafb_dma_descriptor));
	fbi->dma3 = fbi->dma2 - 1;
	fbi->dma4 = fbi->dma3 - 1;

	/* offset */
	yoff = 0;
	cboff = aylen;
	croff = cboff + acblen;

	/* Y vector */
	fbi->dma2->fdadr = (fbi->screen_dma - sizeof(struct pxafb_dma_descriptor));
	fbi->dma2->fsadr = fbi->screen_dma + yoff;
	fbi->dma2->fidr  = 0;
	fbi->dma2->ldcmd = ylen;

	/* Cb vector */
	fbi->dma3->fdadr = (fbi->dma2->fdadr - sizeof(struct pxafb_dma_descriptor));
	fbi->dma3->fsadr = (fbi->screen_dma + cboff);
	fbi->dma3->fidr  = 0;
	fbi->dma3->ldcmd = cblen;

	/* Cr vector */

	fbi->dma4->fdadr = (fbi->dma3->fdadr - sizeof(struct pxafb_dma_descriptor));
	fbi->dma4->fsadr = (fbi->screen_dma + croff);
	fbi->dma4->fidr  = 0;
	fbi->dma4->ldcmd = crlen;


	/* adjust for user */
	fbi->fb.var.red.length   = ylen;
	fbi->fb.var.red.offset   = yoff;
	fbi->fb.var.green.length = cblen;
	fbi->fb.var.green.offset = cboff;
	fbi->fb.var.blue.length  = crlen;
	fbi->fb.var.blue.offset  = croff;

	return 0;
};

/* set xpos, ypos, PPL and LP to 0 */
static int overlay2fb_enable_RGB(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*)info;
	struct pxafb_dma_descriptor *dma = (struct pxafb_dma_descriptor*) fbi->screen_cpu;
	int timeout = 100;

	/* xpos,ypos = 0,0 */
 	OVL2C2 = 0;
	
	/* 64 pixels, 16bpp */
	OVL2C1 = OVL2C1_O2EN | (63);
	
	dma->fdadr = fbi->screen_dma;
	dma->fsadr = PHYS_OFFSET;
	dma->fidr  = 0;
	dma->ldcmd = LDCMD_EOFINT | 128;

	FDADR2 = fbi->screen_dma;

	/*  Run at least 1 frame with Overlay 2 */
	LCSR1 = LCSR1_EOF2;

	while ( !(LCSR1 & LCSR1_EOF2)) {
		mdelay(10);

		if (!timeout) {
			printk(KERN_INFO "%s: timeout\n", __FUNCTION__);
			break;
		}
		timeout --;
	}
	return 0;
}

static int overlay2fb_disable_RGB(struct fb_info *info)
{
	int count = 0;

	OVL2C1 &= ~OVL2C1_O2EN;

	LCSR1 = LCSR1_BS2;
	FBR2 = 0x3;
	while (!(LCSR1 & LCSR1_BS2) && (count < 100) ) {
		mdelay(10);
		count ++;
	}
	if ( count == 100 )
		printk(KERN_WARNING " overlay2fb_disable_RGB: time out.\n");

	return 0;
}

static int overlay2fb_enable_YUV420(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	unsigned int xres, yres;
	int count = 0;

	xres = fbi->fb.fix.line_length;
	yres = fbi->fb.var.yres;

	/* disable overlay 2 window */
	OVL2C1 &= ~OVL2C1_O2EN;

	/* enable overlay 2 window */
	if ( fbi->format ) {
		OVL2C2 = (fbi->format << 20) | (fbi->ypos << 10) | fbi->xpos;
		OVL2C1 = ((yres-1)<<10) | (xres-1);
	} else
		printk(KERN_INFO __FUNCTION__": fbi->format == 0 ??\n");

	OVL2C1 |= OVL2C1_O2EN;

	LCSR1 = LCSR1_IU2;
	while (!(LCSR1 & LCSR1_IU2) && (count < 100 )) {
		mdelay(10);
		count ++;
	}
	if ( count == 100 )
		printk(KERN_WARNING __FUNCTION__" time out.\n");

	if (!fbi->enabled) {
		FDADR2 = fbi->dma2->fdadr;
		FDADR3 = fbi->dma3->fdadr;
		FDADR4 = fbi->dma4->fdadr;
		fbi->enabled = 1;
	}
	else {
		FBR2 = fbi->dma2->fdadr | 0x1;
		FBR3 = fbi->dma3->fdadr | 0x1;
		FBR4 = fbi->dma4->fdadr | 0x1;
	}

	return 0;
}

static int overlay2fb_disable_YUV420(struct fb_info *info)
{
	int count = 0;

	/* workaround for sightings */
	LCSR1 = LCSR1_BS2;
	FBR2 = 0x3;
	FBR3 = 0x3;
	FBR4 = 0x3;
	while (!(LCSR1 & LCSR1_BS2) && (count < 100)) {
		mdelay(10);
		count ++;
	}
	if ( count == 100 )
		printk(KERN_WARNING __FUNCTION__" time out.\n");
}

static int
overlay2fb_enable(struct fb_info *info) 
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	unsigned long bpp2;
	unsigned int xres, yres;

	if (!fbi->map_cpu) return -EINVAL;

	switch(fbi->fb.var.bits_per_pixel) {
	case 16:
		bpp2 = 0x4;
		break;
#ifdef CONFIG_CPU_BULVERDE
	case 18:
		bpp2 = 0x6;
		break;
	case 19:
		bpp2 = 0x8;
		break;
#endif
	case 24:
		bpp2 = 0x9;
		break;
#ifdef CONFIG_CPU_BULVERDE
	case 25:
		bpp2 = 0xa;
		break;
#endif
	default:
		break;
	}

	xres = fbi->fb.fix.line_length;
	yres = fbi->fb.var.yres;

	/* disable branch/start/end of frame interrupt */
	LCCR5 |= (LCCR5_IUM4 | LCCR5_IUM3 | LCCR5_IUM2 | 
		 LCCR5_BSM4 | LCCR5_BSM3 | LCCR5_BSM2 |
		 LCCR5_EOFM4 | LCCR5_EOFM3 | LCCR5_EOFM2 |
		 LCCR5_SOFM4 | LCCR5_SOFM3 | LCCR5_SOFM2);

	/* workaround for Sightings #49219, #56573 etc */
	if ( fbi->format==0x4 ) {
		overlay2fb_enable_RGB(info);
		/* There is no EOF event? */
#if 0
		WAIT_FOR_END_OF_FRAME;
#endif
		overlay2fb_disable_RGB(info);
#if 0
		for(i = 0; i < 3; i++ ) {
			WAIT_FOR_END_OF_FRAME;
		}
#endif
		overlay2fb_enable_YUV420(info);
		yuv420_enabled = 1;
	} else {
		/* disable overlay 2 window */
		OVL2C1 &= ~OVL2C1_O2EN;

		if (!fbi->enabled) {
			FDADR2 = fbi->dma2->fdadr;
			FDADR3 = fbi->dma3->fdadr;
			FDADR4 = fbi->dma4->fdadr;
			fbi->enabled = 1;
		}
		else {
			FBR2 = fbi->dma2->fdadr | 0x1;
			FBR3 = fbi->dma3->fdadr | 0x1;
			FBR4 = fbi->dma4->fdadr | 0x1;
		}

		/* enable overlay 2 window */
		if ( fbi->format ) {
			OVL2C2 = (fbi->format << 20) |
				(fbi->ypos << 10) | fbi->xpos;
			OVL2C1 = ((yres-1)<<10) | (xres-1);
		} else {
			OVL2C2 = (fbi->ypos << 10) | fbi->xpos;
			OVL2C1 = (bpp2 << 20) | ((yres-1)<<10) | (xres-1);
		}

		OVL2C1 |= OVL2C1_O2EN;
	}

	fbi->state = C_ENABLE;
	return 0;
}

static void 
overlay2fb_disable(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*)info;

	fbi->state = C_DISABLE;

	/* clear O2EN */
	OVL2C1 &= ~(0x1<<31);
	if ( yuv420_enabled && fbi->format == 0x4 )
		overlay2fb_disable_YUV420(info);
}

static void overlay2fb_blank(int blank, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	switch(blank)
	{
	case 0:
		set_ctrlr_state(fbi->basefb, C_REENABLE);
		overlay2fb_enable(info);
		break;
	case 1:
		fbi->enabled = 0;
		overlay2fb_disable(info);
		set_ctrlr_state(fbi->basefb, C_REENABLE);
		break;
	default:
		/* reserved */
		break;
	}
}

static int
overlay2fb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	*fix = fbi->fb.fix;

	return 0;
}

static int
overlay2fb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	*var = fbi->fb.var;
	var->nonstd = (fbi->format<<20) | (fbi->ypos<<10) | fbi->xpos;

	return 0;
}

static int 
overlay2fb_validate_var( struct fb_var_screeninfo *var, struct overlayfb_info *fbi)
{
	int xpos, ypos, xres, yres;
	int format;

	xres=yres=0;

	xpos = (var->nonstd & 0x3ff);
	ypos = (var->nonstd >> 10) & 0x3ff;
	format = (var->nonstd >>20) & 0x7;

	/* Palnar YCbCr444, YCbCr422, YCbCr420 */
	if ( (format != 0x4) && (format != 0x3) && (format != 0x2) ) 
		return -EINVAL;

	/* dummy pixels */
	switch(format) {
	case 0x2: /* 444 */
		xres = (var->xres + 0x3) & ~(0x3);
		break;
	case 0x3: /* 422 */
		xres = (var->xres + 0x7) & ~(0x7);
		break;
	case 0x4: /* 420 */
		xres = (var->xres + 0xf) & ~(0xf);
		break;
	}
	yres = var->yres;

	if ( (xpos + xres) > fbi->basefb->max_xres ) 
		return -EINVAL;

	if ( (ypos + yres) > fbi->basefb->max_yres ) 
		return -EINVAL;


	return 0;

}

/*
 * overlay2fb_set_var()
 *
 * var.nonstd is used as YCbCr format.
 * var.red/green/blue is used as (Y/Cb/Cr) vector 
 */

static int
overlay2fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi=(struct overlayfb_info*)info;
	struct fb_var_screeninfo *dvar = &fbi->fb.var;
	unsigned int xpos, ypos;
	int err;

	/* validate parameters*/
	err = overlay2fb_validate_var(var, fbi);
	if (err) return err;

	xpos = var->nonstd & 0x3ff;
	ypos = (var->nonstd>>10) & 0x3ff;

	/* position */
	if ( (xpos != fbi->xpos) || (ypos != fbi->ypos)){

		fbi->xpos = xpos;
		fbi->ypos = ypos;

	}

	/* resolution */
	if ( (var->xres != dvar->xres) ||
		(var->yres != dvar->yres) ||
		(var->bits_per_pixel != dvar->bits_per_pixel) ||
		( ((var->nonstd>>20) & 0x7) != fbi->format)) {

		/* update var_screeninfo fields*/
		*dvar = *var;

		fbi->format = (var->nonstd>>20) & 0x7;

		err= overlay2fb_map_video_memory(info);
		if (err) return err;

	}
	
#if 0
	if (changed) {
		/* disable LCD controller for Overlay2 */
		if (fbi->enabled) {
			/* set to C_DISABLE to avoid enabling */
			overlay2fb_disable((struct fb_info *)fbi);
			set_ctrlr_state(fbi->basefb, C_REENABLE);
		}

		/* enable Overlay2 */
		overlay2fb_enable(info);
	}
#endif
	overlay2fb_enable(info);
	return 0;
}


/* we don't have cmap */
static int
overlay2fb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	return 0;
}

static int
overlay2fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	return -EINVAL;
}

static struct fb_ops overlay2fb_ops = {
	owner:		THIS_MODULE,
	fb_open:	overlay2fb_open,
	fb_release:	overlay2fb_release,
	fb_get_fix:	overlay2fb_get_fix,
	fb_get_var:	overlay2fb_get_var,
	fb_set_var:	overlay2fb_set_var,
	fb_get_cmap:	overlay2fb_get_cmap,
	fb_set_cmap:	overlay2fb_set_cmap,
};

/*
 * Hardware cursor support
 */
/* Bulverde Cursor Modes */
struct cursor_mode{
	int xres;
	int yres;
	int bpp;
};

static struct cursor_mode cursor_modes[]={
	{ 32,  32, 2},
	{ 32,  32, 2},
	{ 32,  32, 2},
	{ 64,  64, 2},
	{ 64,  64, 2},
	{ 64,  64, 2},
	{128, 128, 1},
	{128, 128, 1}
};

static int cursorfb_enable(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	if (!fbi->map_cpu) return -EINVAL;

	CCR &= ~(1<<31);

	/* set palette format 
	 *
	 * FIXME: if only cursor uses palette
	 */
	LCCR4 = (LCCR4 & (~(0x3<<15))) | (0x1<<15);

	/* disable branch/start/end of frame interrupt */
	LCCR5 |= (LCCR5_IUM5 | LCCR5_BSM5 | LCCR5_EOFM5 | LCCR5_SOFM5);

	/* load palette and frame data */
	if(!fbi->enabled) {
		FDADR5 = fbi->dma5_pal->fdadr;
		udelay(1);
		FDADR5 = fbi->dma5_frame->fdadr;
		udelay(1);

		fbi->enabled = 1;
	}
	else {
		FBR5 = fbi->dma5_pal->fdadr | 0x1;
		udelay(1);
		FBR5 = fbi->dma5_frame->fdadr | 0x1;
		udelay(1);
	}

	CCR = (1<<31) | (fbi->ypos << 15) | (fbi->xpos << 5) | (fbi->format);

	fbi->state = C_ENABLE;

	return 0;
}

static void cursorfb_disable(struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*)info;

	fbi->state = C_DISABLE;

	CCR &= ~(1<<31);

}

static int
cursorfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info *)info;
	u_int val, ret = 1;
	u_int *pal=(u_int*) fbi->palette_cpu;

	/* 25bit with Transparcy for 16bpp format */
	if (regno < fbi->palette_size) {
		val = ((trans << 24)  & 0x1000000);
		val |= ((red << 16)  & 0x0ff0000);
		val |= ((green << 8 ) & 0x000ff00);
		val |= ((blue << 0) & 0x00000ff);

		pal[regno] = val;
		ret = 0;
	}
	return ret;
}

static void cursorfb_blank(int blank, struct fb_info *info)
{
	switch(blank) 
	{
	case 0:
		cursorfb_enable(info);
		break;
	case 1:
		cursorfb_disable(info);
		break;
	default:
		/* reserved */
		break;
	}
}

static int
cursorfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	*fix = fbi->fb.fix;

	return 0;
}

static int
cursorfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	*var = fbi->fb.var;
	var->nonstd = (fbi->ypos<<15) | (fbi->xpos<<5) | fbi->format;

	return 0;
}

static int
cursorfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	struct cursor_mode *cursor;
	int mode, xpos, ypos;
	int changed,err;

	mode = var->nonstd & 0x7;
	xpos = (var->nonstd>>5) & 0x3ff;
	ypos = (var->nonstd>>15) & 0x3ff;

	changed = 0;
	if (mode != fbi->format) {
		cursor = cursor_modes + mode;

		/* update "var" info */
		fbi->fb.var.xres = cursor->xres;
		fbi->fb.var.yres = cursor->yres;
		fbi->fb.var.bits_per_pixel = cursor->bpp;

		/* alloc video memory 
		 *
		 * 4k is engouh for 128x128x1 cursor, 
		 * - 2k for cursor pixels, 
		 * - 2k for palette data, plus 2 dma descriptor
		 */
		if (!fbi->map_cpu) {
			fbi->map_size = PAGE_SIZE;
			fbi->map_cpu = (unsigned long)__consistent_alloc(GFP_KERNEL, fbi->map_size, 
									 &fbi->map_dma, PTE_BUFFERABLE);
			if (!fbi->map_cpu) return -ENOMEM;
			memset((void *)fbi->map_cpu, 0, fbi->map_size);
		}

		cursor = cursor_modes + mode;

		/* update overlay & fix "info" */
		fbi->screen_cpu  = fbi->map_cpu;
		fbi->palette_cpu = fbi->map_cpu + (PAGE_SIZE/2);
		fbi->screen_dma  = fbi->map_dma;
		fbi->palette_dma = fbi->map_dma + (PAGE_SIZE/2);

		fbi->format = mode; 
		fbi->palette_size = (1<<cursor->bpp) ;
		fbi->fb.fix.smem_start = fbi->screen_dma;
		fbi->fb.fix.smem_len = cursor->xres * cursor->yres * cursor->bpp / 8;
		fbi->fb.fix.line_length = cursor->xres * cursor->bpp / 8 ;

		fbi->dma5_pal     = (struct pxafb_dma_descriptor*)(fbi->map_cpu + PAGE_SIZE - 16 );
		fbi->dma5_pal->fdadr = (fbi->map_dma + PAGE_SIZE - 16);
		fbi->dma5_pal->fsadr = fbi->palette_dma;
		fbi->dma5_pal->fidr  = 0;
		fbi->dma5_pal->ldcmd = (fbi->palette_size<<2) | LDCMD_PAL;

		fbi->dma5_frame   = (struct pxafb_dma_descriptor*)(fbi->map_cpu + PAGE_SIZE - 32 );
		fbi->dma5_frame->fdadr = (fbi->map_dma + PAGE_SIZE - 32);
		fbi->dma5_frame->fsadr = fbi->screen_dma;
		fbi->dma5_frame->fidr  = 0;
		fbi->dma5_frame->ldcmd = fbi->fb.fix.smem_len;

		/* alloc & set default cmap */
		err = fb_alloc_cmap(&fbi->fb.cmap, fbi->palette_size, 0);
		if (err) return err;
		err = fb_set_cmap(&fbi->fb.cmap, 1, cursorfb_setcolreg, info);
		if (err) return err;

		changed = 1;
	}

	/* update overlay info */
	if( (xpos != fbi->xpos) || (ypos != fbi->ypos) ) {
		fbi->xpos = xpos;
		fbi->ypos = ypos;
		changed = 1;
	}

	if (changed) cursorfb_enable(info);
	set_ctrlr_state(fbi->basefb, C_REENABLE);

	return 0;
}

static int
cursorfb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;

	fb_copy_cmap(&fbi->fb.cmap, cmap, kspc ? 0 : 1);

	return 0;
}

static int
cursorfb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	struct overlayfb_info *fbi = (struct overlayfb_info*) info;
	int err;

	err = fb_alloc_cmap(&fbi->fb.cmap, fbi->palette_size, 0);

	if (!err) 
		err = fb_set_cmap(cmap, kspc, cursorfb_setcolreg, info);

	if (!err)
		fb_copy_cmap(cmap, &fbi->fb.cmap, kspc ? 0 : 1);

	return err;
}

static struct fb_ops cursorfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	cursorfb_get_fix,
	fb_get_var:	cursorfb_get_var,
	fb_set_var:	cursorfb_set_var,
	fb_get_cmap:	cursorfb_get_cmap,
	fb_set_cmap:	cursorfb_set_cmap,
};

static struct overlayfb_info * __init overlay1fb_init_fbinfo(void)
{
	struct overlayfb_info *fbi;

	fbi = kmalloc(sizeof(struct overlayfb_info) + sizeof(u16) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct overlayfb_info) );

	fbi->refcount = 0;
	init_MUTEX(&fbi->mutex);

	strcpy(fbi->fb.fix.id, "overlay1");

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	strcpy(fbi->fb.modename, "overlay1");
	strcpy(fbi->fb.fontname, "null");

	fbi->fb.fbops		= &overlay1fb_ops;
	fbi->fb.changevar	= NULL;
	fbi->fb.switch_con	= NULL;
	fbi->fb.updatevar	= dummy_updatevar;
	fbi->fb.blank		= overlay1fb_blank;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= -1;
	fbi->fb.monspecs	= monspecs;
	fbi->fb.disp		= NULL;
	fbi->fb.pseudo_palette	= NULL;

	fbi->xpos   	= 0;
	fbi->ypos   	= 0;
	fbi->format 	= -1;
	fbi->enabled 	= 0;
	fbi->state 	= C_DISABLE;

	return fbi;
}

static struct overlayfb_info * __init overlay2fb_init_fbinfo(void)
{
	struct overlayfb_info *fbi;

	fbi = kmalloc(sizeof(struct overlayfb_info) + sizeof(u16) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct overlayfb_info) );

	fbi->refcount = 0;
	init_MUTEX(&fbi->mutex);

	strcpy(fbi->fb.fix.id, "overlay2");

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	strcpy(fbi->fb.modename, "overlay2");
	strcpy(fbi->fb.fontname, "null");

	fbi->fb.fbops		= &overlay2fb_ops;
	fbi->fb.changevar	= NULL;
	fbi->fb.switch_con	= NULL;
	fbi->fb.updatevar	= dummy_updatevar;
	fbi->fb.blank		= overlay2fb_blank;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= -1;
	fbi->fb.monspecs	= monspecs;
	fbi->fb.disp		= NULL;
	fbi->fb.pseudo_palette	= NULL;

	fbi->xpos   	= 0;
	fbi->ypos   	= 0;
	fbi->format 	= -1;
	fbi->enabled 	= 0;
	fbi->state 	= C_DISABLE;

	return fbi;
}

static struct overlayfb_info * __init cursorfb_init_fbinfo(void)
{
	struct overlayfb_info *fbi;

	fbi = kmalloc(sizeof(struct overlayfb_info) + sizeof(u16) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct overlayfb_info) );

	strcpy(fbi->fb.fix.id, "curosr");

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	strcpy(fbi->fb.modename, "cursor");
	strcpy(fbi->fb.fontname, "null");

	fbi->fb.fbops		= &cursorfb_ops;
	fbi->fb.changevar	= NULL;
	fbi->fb.switch_con	= NULL;
	fbi->fb.updatevar	= dummy_updatevar;
	fbi->fb.blank		= cursorfb_blank;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= -1;
	fbi->fb.monspecs	= monspecs;
	fbi->fb.disp		= NULL;
	fbi->fb.pseudo_palette	= NULL;

	fbi->xpos   	= 0;
	fbi->ypos   	= 0;
	fbi->format 	= -1;
	fbi->enabled 	= 0;
	fbi->state 	= C_DISABLE;

	return fbi;
}

#endif /* CONFIG_CPU_BULVERDE */

static struct pxafb_info * __init pxafb_init_fbinfo(void)
{
	struct pxafb_mach_info *inf;
	struct pxafb_info *fbi;

	fbi = kmalloc(sizeof(struct pxafb_info) + sizeof(struct display) +
		      sizeof(u16) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct pxafb_info) + sizeof(struct display));

	fbi->currcon		= -1;

	strcpy(fbi->fb.fix.id, PXA_NAME);

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	strcpy(fbi->fb.modename, PXA_NAME);
	strcpy(fbi->fb.fontname, "Acorn8x8");

	fbi->fb.fbops		= &pxafb_ops;
	fbi->fb.changevar	= NULL;
	fbi->fb.switch_con	= pxafb_switch;
	fbi->fb.updatevar	= pxafb_updatevar;
	fbi->fb.blank		= pxafb_blank;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= -1;
	fbi->fb.monspecs	= monspecs;
	fbi->fb.disp		= (struct display *)(fbi + 1);
	fbi->fb.pseudo_palette	= (void *)(fbi->fb.disp + 1);

	fbi->rgb[RGB_8]		= &rgb_8;
	fbi->rgb[RGB_16]	= &def_rgb_16;
	fbi->rgb[RGB_18]	= &def_rgbt_18;
	fbi->rgb[RGB_24]	= &def_rgbt_24;

	inf = pxafb_get_machine_info(fbi);

	fbi->max_xres			= inf->xres;
	fbi->fb.var.xres		= inf->xres;
	fbi->fb.var.xres_virtual	= inf->xres;
	fbi->max_yres			= inf->yres;
	fbi->fb.var.yres		= inf->yres;
	
	fbi->fb.var.yres_virtual	= inf->yres; //sensitive for double buffers//
	fbi->max_bpp			= inf->bpp;
	fbi->fb.var.bits_per_pixel	= inf->bpp;
	fbi->fb.var.pixclock		= inf->pixclock;
	fbi->fb.var.hsync_len		= inf->hsync_len;
	fbi->fb.var.left_margin		= inf->left_margin;
	fbi->fb.var.right_margin	= inf->right_margin;
	fbi->fb.var.vsync_len		= inf->vsync_len;
	fbi->fb.var.upper_margin	= inf->upper_margin;
	fbi->fb.var.lower_margin	= inf->lower_margin;
	fbi->fb.var.sync		= inf->sync;
	fbi->fb.var.grayscale		= inf->cmap_greyscale;
	fbi->cmap_inverse		= inf->cmap_inverse;
	fbi->cmap_static		= inf->cmap_static;
	fbi->lccr0			= inf->lccr0;
	fbi->lccr3			= inf->lccr3;
	fbi->state			= C_DISABLE;
	fbi->task_state			= (u_char)-1;

	if (fbi->max_bpp <= 16)
		fbi->fb.fix.smem_len = fbi->max_xres * fbi->max_yres * fbi->max_bpp / 8; //sensitive for double buffers//
	else if (fbi->max_bpp > 19)
		fbi->fb.fix.smem_len = fbi->max_xres * fbi->max_yres * 4;
	else
		/* packed format */
		fbi->fb.fix.smem_len = fbi->max_xres * fbi->max_yres * 3;

	init_waitqueue_head(&fbi->ctrlr_wait);
	INIT_TQUEUE(&fbi->task, pxafb_task, fbi);
	init_MUTEX(&fbi->ctrlr_sem);

	return fbi;
}

int __init pxafb_init(void)
{
	int ret;
#ifdef CONFIG_CPU_BULVERDE
	struct overlayfb_info *overlay1fb, *overlay2fb, *cursorfb;

	overlay1fb = overlay2fb = cursorfb = NULL;
#endif
	pxafbi = pxafb_init_fbinfo();
	ret = -ENOMEM;
	if (!pxafbi)
		goto failed;

	if(machine_is_pxa_cerf()) {
		// brightness&contrast is handled via lcdctrl.
		lcdctrl_init();
	}

	/* Initialize video memory */
	ret = pxafb_map_video_memory(pxafbi);
	if (ret)
		goto failed;

	ret = request_irq(IRQ_LCD, pxafb_handle_irq, SA_INTERRUPT,
			  "LCD", pxafbi);
	if (ret) {
		printk(KERN_ERR "pxafb: failed in request_irq: %d\n", ret);
		goto failed;
	}

	pxafb_set_var(&pxafbi->fb.var, -1, &pxafbi->fb);

	ret = register_framebuffer(&pxafbi->fb);
	if (ret < 0)
		goto failed;

#if (0)
#ifdef CONFIG_PM
	/*
	 * Note that the console registers this as well, but we want to
	 * power down the display prior to sleeping.
	 */
	pxafbi->pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, pxafb_pm_callback);
	if (pxafbi->pm)
		pxafbi->pm->data = pxafbi;
#endif
#endif

#if CONFIG_FBBASE_DOUBLE_BUFFERS
	pxafb_ioctl(0,0,FBIOENABLE2BFS,0,0,pxafbi);
#endif

#if 1 /* MVL-CEE */
	pxafb_ldm_register();
#endif
#ifdef CONFIG_CPU_FREQ
	pxafbi->clockchg.notifier_call = pxafb_clkchg_notifier;
	cpufreq_register_notifier(&pxafbi->clockchg);
#endif

#ifdef CONFIG_CPU_BULVERDE
	/* Overlay 1 windows */
	overlay1fb = overlay1fb_init_fbinfo();

	if(!overlay1fb) {
		ret = -ENOMEM;
		printk(KERN_ERR "PXA FB: overlay1fb_init_fbinfo failed\n");
		goto failed;
	}

	ret = register_framebuffer(&overlay1fb->fb);
	if (ret<0) goto failed;

	/* Overlay 2 window */
	overlay2fb = overlay2fb_init_fbinfo();

	if(!overlay2fb) {
		ret = -ENOMEM;
		printk(KERN_ERR "PXA FB: overlay2fb_init_fbinfo failed\n");
		goto failed;
	}

	ret = register_framebuffer(&overlay2fb->fb);
	if (ret<0) goto failed;

	/* Hardware cursor window */
	cursorfb = cursorfb_init_fbinfo();

	if(!cursorfb) {
		ret = -ENOMEM;
		printk(KERN_ERR "PXA FB: cursorfb_init_fbinfo failed\n");
		goto failed;
	}

	ret = register_framebuffer(&cursorfb->fb);
	if (ret<0) goto failed;

	/* set refernce to Overlays  */
	pxafbi->overlay1fb = overlay1fb;
	pxafbi->overlay2fb = overlay2fb;
	pxafbi->cursorfb = cursorfb;

	/* set refernce to BaseFrame */
	overlay1fb->basefb = pxafbi;
	overlay2fb->basefb = pxafbi;
	cursorfb->basefb = pxafbi;
#endif

	init_timer(&bklight_timer);
	bklight_timer.function = pxafb_ezx_Backlight_turn_on;

	/*
	 * Ok, now enable the LCD controller
	 */
	set_ctrlr_state(pxafbi, C_ENABLE);

	bk_timeout = BK_TIMEOUT;

	return 0;

failed:
	if (pxafbi)
		kfree(pxafbi);
#ifdef CONFIG_CPU_BULVERDE
	if (overlay1fb) 
		kfree(overlay1fb);
	if (overlay2fb) 
		kfree(overlay2fb);
	if (cursorfb)
		kfree(cursorfb);
#endif
	return ret;
}

static void __exit pxafb_exit(void)
{
	/* Disable LCD controller */
	set_ctrlr_state(pxafbi, C_DISABLE);

#if CONFIG_FBBASE_DOUBLE_BUFFERS
	pxafb_ioctl(0,0,FBIODISABLE2BFS,0,0,pxafbi);  //release second framebuffer space//
#endif

#ifdef CONFIG_CPU_BULVERDE
	/* Unregister overlays and cursor framebuffers and free it's memory */
	unregister_framebuffer(&pxafbi->cursorfb->fb);
	consistent_free((void *)pxafbi->cursorfb->map_cpu, pxafbi->cursorfb->map_size, pxafbi->cursorfb->map_dma);
	kfree(pxafbi->cursorfb);

	unregister_framebuffer(&pxafbi->overlay1fb->fb);
	consistent_free((void *)pxafbi->overlay1fb->map_cpu, pxafbi->overlay1fb->map_size, pxafbi->overlay1fb->map_dma);
	kfree(pxafbi->overlay1fb);

	unregister_framebuffer(&pxafbi->overlay2fb->fb);
	consistent_free((void *)pxafbi->overlay2fb->map_cpu, pxafbi->overlay2fb->map_size, pxafbi->overlay2fb->map_dma);
	kfree(pxafbi->overlay2fb);
#endif
#ifdef CONFIG_CPU_FREQ
	cpufreq_unregister_notifier(&pxafbi->clockchg);
#endif
#if 1 /* MVL-CEE */
	pxafb_ldm_unregister();
#endif

#if (0)
#ifdef CONFIG_PM
	pm_unregister(pxafbi->pm);
#endif
#endif
	/* Free LCD IRQ */
	free_irq(IRQ_LCD, pxafbi);
	
	/* Unregister main framebuffer and free it's memory */
	unregister_framebuffer(&pxafbi->fb);
#ifdef CONFIG_CPU_BULVERDE
	if (sram_size) {
		DPRINTK("SRAM release...\n");
		sram_access_release(&sram_start, &sram_size);
	}
	else
#endif
		consistent_free(pxafbi->map_cpu, pxafbi->map_size, pxafbi->map_dma);
	kfree(pxafbi);
	pxafbi = 0;
}

#ifdef MODULE
module_init(pxafb_init);
module_exit(pxafb_exit);
#endif

MODULE_AUTHOR("source@mvista.com, yu.tang@intel.com");
MODULE_DESCRIPTION("Framebuffer driver for Bulverde/PXA");
MODULE_LICENSE("GPL");
