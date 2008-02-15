/*
 *  linux/drivers/video/pxafb.c
 *
 *  Copyright (C) 1999 Eric A. Thomas
 *   Based on acornfb.c Copyright (C) Russell King.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	        Intel PXA250/210 LCD Controller Frame Buffer Driver
 *
 * Please direct your questions and comments on this driver to the following
 * email address:
 *
 *	linux-arm-kernel@lists.arm.linux.org.uk
 *
 * Clean patches should be sent to the ARM Linux Patch System.  Please see the
 * following web page for more information:
 *
 *	http://www.arm.linux.org.uk/developer/patches/info.shtml
 *
 * Thank you.
 *
 *
 * Code Status:
 * 1999/04/01:
 *	- Driver appears to be working for Brutus 320x200x8bpp mode.  Other
 *	  resolutions are working, but only the 8bpp mode is supported.
 *	  Changes need to be made to the palette encode and decode routines
 *	  to support 4 and 16 bpp modes.  
 *	  Driver is not designed to be a module.  The FrameBuffer is statically
 *	  allocated since dynamic allocation of a 300k buffer cannot be 
 *	  guaranteed. 
 *
 * 1999/06/17:
 *	- FrameBuffer memory is now allocated at run-time when the
 *	  driver is initialized.    
 *
 * 2000/04/10: Nicolas Pitre <nico@cam.org>
 *	- Big cleanup for dynamic selection of machine type at run time.
 *
 * 2000/07/19: Jamey Hicks <jamey@crl.dec.com>
 *	- Support for Bitsy aka Compaq iPAQ H3600 added.
 *
 * 2000/08/07: Tak-Shing Chan <tchan.rd@idthk.com>
 *	       Jeff Sutherland <jsutherland@accelent.com>
 *	- Resolved an issue caused by a change made to the Assabet's PLD 
 *	  earlier this year which broke the framebuffer driver for newer 
 *	  Phase 4 Assabets.  Some other parameters were changed to optimize
 *	  for the Sharp display.
 *
 * 2000/08/09: Cliff Brake <cbrake@accelent.com>
 *      - modified for Accelent
 * 
 * 2000/08/09: Kunihiko IMAI <imai@vasara.co.jp>
 *	- XP860 support added
 *
 * 2000/08/19: Mark Huang <mhuang@livetoy.com>
 *	- Allows standard options to be passed on the kernel command line
 *	  for most common passive displays.
 *
 * 2000/08/29:
 *	- s/save_flags_cli/local_irq_save/
 *	- remove unneeded extra save_flags_cli in sa1100fb_enable_lcd_controller
 *
 * 2000/10/10: Erik Mouw <J.A.K.Mouw@its.tudelft.nl>
 *	- Updated LART stuff. Fixed some minor bugs.
 *
 * 2000/10/30: Murphy Chen <murphy@mail.dialogue.com.tw>
 *	- Pangolin support added
 *
 * 2000/10/31: Roman Jordan <jor@hoeft-wessel.de>
 *	- Huw Webpanel support added
 *
 * 2000/11/23: Eric Peng <ericpeng@coventive.com>
 *	- Freebird add
 *
 * 2001/02/07: Jamey Hicks <jamey.hicks@compaq.com> 
 *	       Cliff Brake <cbrake@accelent.com>
 *	- Added PM callback
 *
 * 2001/05/26: <rmk@arm.linux.org.uk>
 *	- Fix 16bpp so that (a) we use the right colours rather than some
 *	  totally random colour depending on what was in page 0, and (b)
 *	  we don't de-reference a NULL pointer.
 *	- remove duplicated implementation of consistent_alloc()
 *	- convert dma address types to dma_addr_t
 *	- remove unused 'montype' stuff
 *	- remove redundant zero inits of init_var after the initial
 *	  memzero.
 *	- remove allow_modeset (acornfb idea does not belong here)
 *
 * 2001/05/28: <rmk@arm.linux.org.uk>
 *	- massive cleanup - move machine dependent data into structures
 *	- I've left various #warnings in - if you see one, and know
 *	  the hardware concerned, please get in contact with me.
 *
 * 2001/05/31: <rmk@arm.linux.org.uk>
 *	- Fix LCCR1 HSW value, fix all machine type specifications to
 *	  keep values in line.  (Please check your machine type specs)
 *
 * 2001/06/10: <rmk@arm.linux.org.uk>
 *	- Fiddle with the LCD controller from task context only; mainly
 *	  so that we can run with interrupts on, and sleep.
 *	- Convert #warnings into #errors.  No pain, no gain. ;)
 *
 * 2001/06/14: <rmk@arm.linux.org.uk>
 *	- Make the palette BPS value for 12bpp come out correctly.
 *	- Take notice of "greyscale" on any colour depth.
 *	- Make truecolor visuals use the RGB channel encoding information.
 *
 * 2001/07/02: <rmk@arm.linux.org.uk>
 *	- Fix colourmap problems.
 *
 * 2001/08/03: <cbrake@accelent.com>
 *      - Ported from SA1100 to PXA250
 *
 * 2002/06/30: lilij
 *      - Ported to EZX LCD
 *
 * 2003/05/19: zxf
 *	- Add timer to turn on backlight until lcd controller is ready
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
#include <video/lcdctrl.h> /* brightness, contrast, etc. control */

#ifdef _DEBUG_BOARD
#define _OLD_LCD_ 1
#else
#define _OLD_LCD_ 0
#endif
 
#define EZX_LCD_WIDTH  240
#define EZX_LCD_HEIGHT 320
/* LCD Control Register 0 Bits */
#define LCD0_V_OUM 21
#define LCD0_V_BM  20
#define LCD0_V_PDD 12
#define LCD0_V_QDM 11
#define LCD0_V_DIS 10
#define LCD0_V_DPD 9
#define LCD0_V_BLE 8
#define LCD0_V_PAS 7
#define LCD0_V_EFM 6
#define LCD0_V_IUM 5
#define LCD0_V_SFM 4
#define LCD0_V_LDM 3
#define LCD0_V_SDS 2
#define LCD0_V_CMS 1
#define LCD0_V_ENB 0
/* LCD control Register 1 Bits */
#define LCD1_V_PPL 0
#define LCD1_V_HSW 10
#define LCD1_V_ELW 16
#define LCD1_V_BLW 24

/* LCD control Register 2 Bits */
#define LCD2_V_LPP 0
#define LCD2_V_VSW 10
#define LCD2_V_EFW 16
#define LCD2_V_BFW 24

/* LCD control Register 3 Bits */
#define LCD3_V_PCD 0
#define LCD3_V_ACB 8
#define LCD3_V_API 16
#define LCD3_V_VSP 20
#define LCD3_V_HSP 21
#define LCD3_V_PCP 22
#define LCD3_V_OEP 23
#define LCD3_V_BPP 24
#define LCD3_V_DPC 27

/*
 * debugging?
 */
#define DEBUG 0 
/*
 * Complain if VAR is out of range.
 */
#define DEBUG_VAR 0

#undef ASSABET_PAL_VIDEO

#include "pxafb.h"

void (*pxafb_blank_helper)(int blank);
EXPORT_SYMBOL(pxafb_blank_helper);


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

//#ifdef CONFIG_ARCH_LUBBOCK
static struct pxafb_mach_info ezx_info __initdata = {
	pixclock:	150000,	/* clock period in ps */
#if _OLD_LCD_	
	bpp:		8,
#else
    bpp:        16,
#endif    	
	xres:		EZX_LCD_WIDTH,/*640,*/
	yres:		EZX_LCD_HEIGHT,/*480,*/

#if _OLD_LCD_ 
	hsync_len:	8,/*50, // LCD_HORIZONTAL_SYNC_PULSE_WIDTH, */
	vsync_len:	4,/*0, // LCD_VERTICAL_SYNC_PULSE_WIDTH, */
	left_margin:	16,/*0, // LCD_BEGIN_OF_LINE_WAIT_COUNT, */
	upper_margin:	10,/*8, //LCD_BEGIN_FRAME_WAIT_COUNT, */
	right_margin:	8,/*0, //LCD_END_OF_LINE_WAIT_COUNT, */
	lower_margin:	10,/*1, //LCD_END_OF_FRAME_WAIT_COUNT, */
#else 
	hsync_len:	9,
	vsync_len:	1,
	left_margin:	19,
	upper_margin:	3,//1,
	right_margin:	9,
	lower_margin:	3,//1,
#endif	
	sync:		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	lccr0:		0x003008F8,/*0x0030087C, //LCD_CTL0_VALUE,*/
#if _OLD_LCD_	
	lccr3:		0x0360FF07/* 0x0340FF0C  //LCD_CTL3_VALUE,*/
#else
    lccr3:		0x0460FF08 /* 16 bits*/
#endif	
};
//#endif

#define BKLIGHTON  1
#define BKLIGHTOFF 0

#define BKLIGHT_PRESCALE  2 //4
#define BKLIGHT_PERIOD    49 //99
#define DEFAULT_DUTYCYCLE  25// 49 //25 //50
#define MAX_DUTYCYCLE      (BKLIGHT_PERIOD+1)  //100
#define MIN_DUTYCYCLE      0

static u8 bBklightStatus = BKLIGHTON;
static u8 gbklightDutyCycle = DEFAULT_DUTYCYCLE;
static u8 bLCDOn = 0;

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
    /* configure PWM0 register */
    CKEN |= CKEN0_PWM0;
    PWM_CTRL0 = BKLIGHT_PRESCALE;
    PWM_PERVAL0 = BKLIGHT_PERIOD;
    /* gbklightDutyCycle should got from stup file system */
    PWM_PWDUTY0   = gbklightDutyCycle;

    /* configure GPIO for PWM0 */
    GPDR0 |= 0x00010000;  //PWM0 is GPIO16
    set_GPIO_mode(GPIO16_PWM0_MD);


#if _OLD_LCD_
    GPDR2 |= 0x00008000; //GPIO 79
    set_GPIO_mode(79 | GPIO_OUT);
    GPSR2 = 0x00008000;
#endif   

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
   
    /* set PWM0 as GPIO */

    PWM_PWDUTY0 = 0;
    GAFR0_U &= 0xFFFFFFFC; 
    GPDR0 &= 0xFFFEFFFF; //PWM0 is GPIO16, set PWM0 as input when turn off bklight
    CKEN &= ~CKEN0_PWM0;
    PWM_PWDUTY0	 = MIN_DUTYCYCLE;
    /*gbklightDutyCycle = MIN_DUTYCYCLE;*/
#if _OLD_LCD_
	GPCR2 = 0x00008000;
#endif
    bBklightStatus = BKLIGHTOFF;
    DPRINTK("\nCKEN: %x", CKEN);
    DPRINTK("\nPWM_PWDUTY0: %x", PWM_PWDUTY0);
}

u8 pxafb_ezx_BackLight_Brightness_Intensity(u8 dutyCycle)
{
    /* setup file system should call this function whn powr on to set the dfault duty cycle*/
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

u8 pxafb_ezx_getLCD_status(void)
{
	DPRINTK("\npxafb_ezx_getLCD_status is %d\n", bLCDOn);
	return bLCDOn;
}

EXPORT_SYMBOL(pxafb_ezx_getLCD_status);
EXPORT_SYMBOL(pxafb_ezx_getBacklight_status);
EXPORT_SYMBOL(pxafb_ezx_getBacklight_dutycycle);
EXPORT_SYMBOL(pxafb_ezx_Backlight_turn_on);
EXPORT_SYMBOL(pxafb_ezx_Backlight_turn_off);
EXPORT_SYMBOL(pxafb_ezx_BackLight_Brightness_Intensity);

static struct pxafb_mach_info * __init
pxafb_get_machine_info(struct pxafb_info *fbi)
{
	struct pxafb_mach_info *inf = NULL;

	/*
	 *            R        G       B       T
	 * default  {11,5}, { 5,6}, { 0,5}, { 0,0}
	 * bitsy    {12,4}, { 7,4}, { 1,4}, { 0,0}
	 * freebird { 8,4}, { 4,4}, { 0,4}, {12,4}
	 */
//#ifdef CONFIG_ARCH_LUBBOCK
/*	if (machine_is_lubbock()) {*/
		inf = &ezx_info;
		DPRINTK("\npxafb_get_machine_info.\n");
/*	} else if (machine_is_pxa_idp()) {
		inf = &pxa_idp_info;
	}*/
//#endif
	return inf;
}

static int pxafb_activate_var(struct fb_var_screeninfo *var, struct pxafb_info *);
static void set_ctrlr_state(struct pxafb_info *fbi, u_int state);

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
		DPRINTK("\npxafb_schedule_task.\n");
	}
	local_irq_restore(flags);
}

/*
 * Get the VAR structure pointer for the specified console
 */
static inline struct fb_var_screeninfo *get_con_var(struct fb_info *info, int con)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	DPRINTK("\npxafb get_con_var.\n");
	return (con == fbi->currcon || con == -1) ? &fbi->fb.var : &fb_display[con].var;
}

/*
 * Get the DISPLAY structure pointer for the specified console
 */
static inline struct display *get_con_display(struct fb_info *info, int con)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	DPRINTK("\npxafb get_con_display.\n");
	return (con < 0) ? fbi->fb.disp : &fb_display[con];
}

/*
 * Get the CMAP pointer for the specified console
 */
static inline struct fb_cmap *get_con_cmap(struct fb_info *info, int con)
{
	struct pxafb_info *fbi = (struct pxafb_info *)info;
	DPRINTK("\npxafb get_con_cmap.\n");
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
    {
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;
        DPRINTK("\nred=green=blue: %x\n", red);
    }

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

	DPRINTK("var->bits_per_pixel=%d\n", var->bits_per_pixel);
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
		//if (!(fbi->lccr0 & LCCR0_PAS))
//		if (fbi->lccr0 & LCCR0_PAS) // our 12 bits color LCD is active mode
			ret = 0;
		break;

	case 16:
		/* 
		 * 16 bits works apparemtly fine in passive mode for those,
		 * so don't complain
		 */
		if (machine_is_lubbock() ||
		    machine_is_pxa_cerf()) {
			ret = 0;
		} else
			/* make sure we are in active mode */
			if ((fbi->lccr0 & LCCR0_PAS))
				ret = 0;
			DPRINTK("pxafb_validate_var: case 16.\n");
		break;
#endif
	default:
		break;
	}

	return ret;
}

static inline void pxafb_set_truecolor(u_int is_true_color)
{
	DPRINTK("true_color = %d\n", is_true_color);
#ifdef CONFIG_SA1100_ASSABET
	if (machine_is_assabet()) {
#if 1
		// phase 4 or newer Assabet's
		DRPINTK("\npxafb_set_truecolor machine is assabet.\n");
		if (is_true_color)
			BCR_set(BCR_LCD_12RGB);
		else
			BCR_clear(BCR_LCD_12RGB);
#else
		// older Assabet's
		if (is_true_color)
			BCR_clear(BCR_LCD_12RGB);
		else
			BCR_set(BCR_LCD_12RGB);
#endif
	}
#endif
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

	DPRINTK("pxafb_set_var\n");

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
		display->line_length	= var->xres*2;
		display->dispsw		= &fbcon_cfb16;
		display->dispsw_data	= fbi->fb.pseudo_palette;
		rgbidx			= RGB_16;
		break;
#endif
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

	DPRINTK("RGBT length = %d:%d:%d:%d\n",
		dvar->red.length, dvar->green.length, dvar->blue.length,
		dvar->transp.length);

	DPRINTK("RGBT offset = %d:%d:%d:%d\n",
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

	if (disp->visual == FB_VISUAL_TRUECOLOR)
		return -EINVAL;
    DPRINTK("\npxafb_set_cmap entered");
	return __do_set_cmap(cmap, kspc, con, info);
}

static int
pxafb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
	struct display *display = get_con_display(info, con);

	*fix = info->fix;

	fix->line_length = display->line_length;
	fix->visual	 = display->visual;
	DPRINTK("\npxafb_get_fix entered");
	return 0;
}

static int
pxafb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	*var = *get_con_var(info, con);
	DPRINTK("\npxafb_get_var entered");
	return 0;
}

static int
pxafb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
	struct fb_cmap *dcmap = get_con_cmap(info, con);
	fb_copy_cmap(dcmap, cmap, kspc ? 0 : 2);
	DPRINTK("\npxafb_get_cmap entered");
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
	}
	return -EINVAL;
}

static struct fb_ops pxafb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	pxafb_get_fix,
	fb_get_var:	pxafb_get_var,
	fb_set_var:	pxafb_set_var,
	fb_get_cmap:	pxafb_get_cmap,
	fb_set_cmap:	pxafb_set_cmap,
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

	DPRINTK("con=%d info->modename=%s\n", con, fbi->fb.modename);

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

	DPRINTK("pxafb_blank: blank=%d info->modename=%s\n", blank,
		fbi->fb.modename);

	switch (blank) {
	case VESA_POWERDOWN:
	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
		if (fbi->fb.disp->visual == FB_VISUAL_PSEUDOCOLOR ||
		    fbi->fb.disp->visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
			for (i = 0; i < fbi->palette_size; i++)
				pxafb_setpalettereg(i, 0, 0, 0, 0, info);
		pxafb_schedule_task(fbi, C_DISABLE);
		if (pxafb_blank_helper)
			pxafb_blank_helper(blank);
		DPRINTK("\npxafb_blank: VESA_POWERDOWN");
		break;

	case VESA_NO_BLANKING:
		if (pxafb_blank_helper)
			pxafb_blank_helper(blank);
		if (fbi->fb.disp->visual == FB_VISUAL_PSEUDOCOLOR ||
		    fbi->fb.disp->visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
			fb_set_cmap(&fbi->fb.cmap, 1, pxafb_setcolreg, info);
		pxafb_schedule_task(fbi, C_ENABLE);
		DPRINTK("\npxafb_blank: VESA_NO_BLANKING");
	}
}

static int pxafb_updatevar(int con, struct fb_info *info)
{
	DPRINTK("updatevar entered\n");
	return 0;
}

/*
 * Calculate the PCD value from the clock rate (in picoseconds).
 * We take account of the PPCR clock setting.
 */
static inline int get_pcd(unsigned int pixclock)
{
	unsigned int pcd;

	if (pixclock) {
		pcd = get_lclk_frequency_10khz() * pixclock;
		pcd /= 100000000;
		pcd += 1;	/* make up for integer math truncations */
	} else {
		printk(KERN_WARNING "Please convert me to use the PCD calculations\n");
		pcd = 0;
	}
	return pcd;
}

/*
 * pxafb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are      
 *      only written to the controller if changes were made.  
 */
static int pxafb_activate_var(struct fb_var_screeninfo *var, struct pxafb_info *fbi)
{
	struct pxafb_lcd_reg new_regs;
	u_int pcd = get_pcd(var->pixclock);
	u_long flags;

	DPRINTK("Configuring PXA LCD\n");

	DPRINTK("var: xres=%d hslen=%d lm=%d rm=%d\n",
		var->xres, var->hsync_len,
		var->left_margin, var->right_margin);
	DPRINTK("var: yres=%d vslen=%d um=%d bm=%d\n",
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
#endif

#if defined (CONFIG_PXA_CERF_PDA)
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
	new_regs.lccr3 = fbi->lccr3;
#else
	// FIXME using hardcoded values for now
	// not use hard value for give more clear value setting
/*
	new_regs.lccr0 = fbi->lccr0;
//		|
//		LCCR0_LEN | LCCR0_LDM | LCCR0_BAM |
//		LCCR0_ERM | LCCR0_LtlEnd | LCCR0_DMADel(0);
*/
    new_regs.lccr0 = (0   << LCD0_V_OUM)   |
					 (1   << LCD0_V_BM)    |
					 (0x0 << LCD0_V_PDD)   |
					 (0   << LCD0_V_QDM)   |
					 (0   << LCD0_V_DIS)   |
					 (0   << LCD0_V_DPD)   |
					 (1   << LCD0_V_PAS)   |
					 (1   << LCD0_V_EFM)   |
					 (1   << LCD0_V_IUM)   |
					 (1   << LCD0_V_SFM)   |
					 (1   << LCD0_V_LDM)   |
					 (0   << LCD0_V_SDS)   |
					 (0   << LCD0_V_CMS)   |
					 (0   << LCD0_V_ENB);	// don't enable LCD right now.
/*
		new_regs.lccr1 = 0x3030A7F;
//		LCCR1_DisWdth(var->xres) +
//		LCCR1_HorSnchWdth(var->hsync_len) +
//		LCCR1_BegLnDel(var->left_margin) +
//		LCCR1_EndLnDel(var->right_margin);
*/

#if _OLD_LCD_
	new_regs.lccr1 = (16 << LCD1_V_BLW) | 
	                 ( 8 << LCD1_V_ELW) | 
	                 ( 8 << LCD1_V_HSW) | 
	                 ((EZX_LCD_WIDTH-1) << LCD1_V_PPL); // 0x100820EF;
#else
	new_regs.lccr1 = (19 << LCD1_V_BLW) | 
	                 ( 9 << LCD1_V_ELW) | 
	                 ( 9 << LCD1_V_HSW) | 
	                 ((EZX_LCD_WIDTH-1) << LCD1_V_PPL); 
#endif
/*                    
	new_regs.lccr2 = 0x4EF;
//		LCCR2_DisHght(var->yres) +
//		LCCR2_VrtSnchWdth(var->vsync_len) +
//		LCCR2_BegFrmDel(var->upper_margin) +
//		LCCR2_EndFrmDel(var->lower_margin);
*/

#if _OLD_LCD_
	new_regs.lccr2 = (10 << LCD2_V_BFW) | 
                     (10 << LCD2_V_EFW) | 
                     (4 << LCD2_V_VSW) | 
                     ((EZX_LCD_HEIGHT-1) << LCD2_V_LPP); //0x0A0A113F;
#else
	new_regs.lccr2 = (3 << LCD2_V_BFW) | 
                     (3 << LCD2_V_EFW) | 
                     (1 << LCD2_V_VSW) | 
                     ((EZX_LCD_HEIGHT-1) << LCD2_V_LPP);
#endif
/*                    
	new_regs.lccr3 = fbi->lccr3;
//	|
//		(var->sync & FB_SYNC_HOR_HIGH_ACT ? LCCR3_HorSnchH : LCCR3_HorSnchL) |
//		(var->sync & FB_SYNC_VERT_HIGH_ACT ? LCCR3_VrtSnchH : LCCR3_VrtSnchL) |
//		LCCR3_ACBsCntOff;

//	if (pcd)
//		new_regs.lccr3 |= LCCR3_PixClkDiv(pcd);
*/
#endif
#if _OLD_LCD_
	new_regs.lccr3 = (0    << LCD3_V_DPC) | 
					 (3    << LCD3_V_BPP) | 
					 (0    << LCD3_V_OEP) | 
	                 (1    << LCD3_V_PCP) | 			// opposite of SA1110
	                 (1    << LCD3_V_HSP) | 
	                 (0    << LCD3_V_VSP) | 
	                 (0    << LCD3_V_API) | 
	                 (0xFF << LCD3_V_ACB) | 
	                 (7   << LCD3_V_PCD);			// depends on mem clock!!!
#else
	new_regs.lccr3 = (0    << LCD3_V_DPC) | 
					 (4   << LCD3_V_BPP) | 
					 (0    << LCD3_V_OEP) | 
	                 (0    << LCD3_V_PCP) | 			// opposite of SA1110
	                 (1    << LCD3_V_HSP) | 
	                 (1    << LCD3_V_VSP) | 
	                 (0    << LCD3_V_API) | 
	                 (0xFF << LCD3_V_ACB) | 
	                 (8   << LCD3_V_PCD);			// depends on mem clock!!!
#endif
	DPRINTK("nlccr0 = 0x%08x\n", new_regs.lccr0);
	DPRINTK("nlccr1 = 0x%08x\n", new_regs.lccr1);
	DPRINTK("nlccr2 = 0x%08x\n", new_regs.lccr2);
	DPRINTK("nlccr3 = 0x%08x\n", new_regs.lccr3);

	/* Update shadow copy atomically */
	local_irq_save(flags);

	/* setup dma descriptors */
	fbi->dmadesc_fblow_cpu = (struct pxafb_dma_descriptor *)((unsigned int)fbi->palette_cpu - 3*16);
	fbi->dmadesc_fbhigh_cpu = (struct pxafb_dma_descriptor *)((unsigned int)fbi->palette_cpu - 2*16);
	fbi->dmadesc_palette_cpu = (struct pxafb_dma_descriptor *)((unsigned int)fbi->palette_cpu - 1*16);

	fbi->dmadesc_fblow_dma = fbi->palette_dma - 3*16;
	fbi->dmadesc_fbhigh_dma = fbi->palette_dma - 2*16;
	fbi->dmadesc_palette_dma = fbi->palette_dma - 1*16;


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
	
	#define BYTES_PER_PANEL	((fbi->lccr0 & LCCR0_SDS) ? (var->xres * var->yres * var->bits_per_pixel / 8 / 2) : \
			                                    (var->xres * var->yres * var->bits_per_pixel / 8))
	
	/* populate descriptors */
	fbi->dmadesc_fblow_cpu->fdadr = fbi->dmadesc_fblow_dma;
	fbi->dmadesc_fblow_cpu->fsadr = fbi->screen_dma + BYTES_PER_PANEL;
	fbi->dmadesc_fblow_cpu->ldcmd = BYTES_PER_PANEL;
    if (fbi->fb.var.bits_per_pixel <= 8) 
    {		
	    fbi->dmadesc_fbhigh_cpu->fdadr = fbi->dmadesc_palette_dma;
	}
	else if ((fbi->fb.var.bits_per_pixel == 16) || (fbi->fb.var.bits_per_pixel == 12))          
	{
		fbi->dmadesc_fbhigh_cpu->fdadr = fbi->dmadesc_fbhigh_dma;
	}
	
	fbi->dmadesc_fbhigh_cpu->fsadr = fbi->screen_dma;
	fbi->dmadesc_fbhigh_cpu->ldcmd = BYTES_PER_PANEL;
    if (fbi->fb.var.bits_per_pixel <= 8) 
    {
	    fbi->dmadesc_palette_cpu->fdadr = fbi->dmadesc_fbhigh_dma;
    }
    else if ((fbi->fb.var.bits_per_pixel == 16) || (fbi->fb.var.bits_per_pixel == 12))
	{
	    fbi->dmadesc_palette_cpu->fdadr = fbi->dmadesc_palette_dma;
	}
	fbi->dmadesc_palette_cpu->fsadr = fbi->palette_dma;
	fbi->dmadesc_palette_cpu->ldcmd = (fbi->palette_size * 2) | LDCMD_PAL;
    if (fbi->fb.var.bits_per_pixel <= 8) 	
    {
	fbi->fdadr0 = fbi->dmadesc_palette_dma;
    }
    else if ((fbi->fb.var.bits_per_pixel == 16) || (fbi->fb.var.bits_per_pixel == 12))
    {
        fbi->fdadr0 = fbi->dmadesc_fbhigh_dma;
    }
	fbi->fdadr1 = fbi->dmadesc_fblow_dma;
	
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
	DPRINTK("backlight on\n");

#ifdef CONFIG_ARCH_PXA_IDP
	if(machine_is_pxa_idp()) {	
		FB_BACKLIGHT_ON();
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
	DPRINTK("backlight off\n");

#ifdef CONFIG_ARCH_PXA_IDP
	if(machine_is_pxa_idp()) {
		FB_BACKLIGHT_OFF();
	}
#endif
	
}
*/
static void pxafb_power_up_lcd(struct pxafb_info *fbi)
{
	DPRINTK("LCD power on\n");
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
}

static void pxafb_power_down_lcd(struct pxafb_info *fbi)
{
	DPRINTK("LCD power off\n");
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

}

static void pxafb_setup_gpio(struct pxafb_info *fbi)
{
	GPDR1 |= 0xFC000000;		// GPIO 63-58 = 1, (1=output)
	GPDR2 |= 0x00007FFF;		// GPIO 78-64 = 1, (1=output)
	
	GAFR1_U = (GAFR1_U & 0x000FFFFF) | 0xAAA00000;	// bit 31,30 = LDD5, bit 21,20 = LDD0
	GAFR2_L = (GAFR2_L & 0xF0000000) | 0x0AAAAAAA;	// bit 27,26 = LBIAS, bit 1,0 = LDD6
	DPRINTK("\npxafb_setup_gpio finished.");
	#if 0
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
		GAFR1_U = (GAFR1_U & (0xff << 20)) | (0xaa << 20);

		// bits 74-77
		GPDR2 |= (0xf << 10);
		GAFR2_L = (0xaa << 20);
	}

	/* 8 bit interface */
	else if (((lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_DPD))) ||
		 (!(lccr0 & LCCR0_CMS) && !(lccr0 & LCCR0_PAS) && !(lccr0 & LCCR0_SDS)))
	{
		GPDR1 |= (0x3f << 26);
		GPDR2 |= (0x3);

		GAFR1_U = (GAFR1_U & (0xfff << 20)) | (0x2aa << 20);
		GAFR2_L = (GAFR2_L & 0xf) + (0xa);

		// bits 74-77
		GPDR2 |= (0xf << 10);
		GAFR2_L |= (0xaa << 20);
	}
	
	/* 16 bit interface */
	else if (!(lccr0 & LCCR0_CMS) && ((lccr0 & LCCR0_SDS) || (lccr0 & LCCR0_PAS)))
	{
		GPDR1 |= 0xFC000000;
		GPDR2 |= 0x00003FFF;

		GAFR1_U = (GAFR1_U & 0x000FFFFF) | 0xAAA00000;
		GAFR2_L = (GAFR2_L & 0xF0000000) | 0x0AAAAAAA;
	}
	#endif
}

static void pxafb_enable_controller(struct pxafb_info *fbi)
{	
	DPRINTK("Enabling LCD controller\n");
	/* Sequence from 11.7.10 */
	LCCR3 = fbi->reg_lccr3;
	LCCR2 = fbi->reg_lccr2;
	LCCR1 = fbi->reg_lccr1;
	LCCR0 = fbi->reg_lccr0 & ~LCCR0_ENB;

	/* FIXME we used to have LCD power control here */

	FDADR0 = fbi->fdadr0;
	FDADR1 = fbi->fdadr1;
	LCCR0 |= LCCR0_ENB;
    
    GPSR2 = 0x00004000;
	mdelay(20); /* LCD timing delay*/
#if _OLD_LCD_
	GPSR2 =  0x00004000; /* LCD on/off enable: GPIO78// old LCD is enable by high level*/
#else
	GPCR2 =  0x00004000; /*new lCD is enabled by low level*/
#endif	
	GPDR2 |=  0x00004000;

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
	DECLARE_WAITQUEUE(wait, current);

	DPRINTK("Disabling LCD controller\n");

	/* FIXME add power down GPIO stuff here */

	add_wait_queue(&fbi->ctrlr_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);

	LCSR = 0xffffffff;	/* Clear LCD Status Register */
#if _OLD_LCD_	
    GPCR2 = 0x00004000;/* LCD on/off diable: GPIO78// old LCD is disable by low level*/
#else
    GPSR2 = 0x00004000;/* new LCD is disabled by high level*/
#endif    
    mdelay(41);
	LCCR0 &= ~LCCR0_LDM;	/* Enable LCD Disable Done Interrupt */
	enable_irq(IRQ_LCD);	/* Enable LCD IRQ */
	LCCR0 &= ~LCCR0_ENB;	/* Disable LCD Controller */

    bLCDOn = 0;
	schedule_timeout(20 * HZ / 1000);
	current->state = TASK_RUNNING;
	remove_wait_queue(&fbi->ctrlr_wait, &wait);
}

/*
 *  pxafb_handle_irq: Handle 'LCD DONE' interrupts.
 */
static void pxafb_handle_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct pxafb_info *fbi = dev_id;
	unsigned int lcsr = LCSR;

	if (lcsr & LCSR_LDD) {
		LCCR0 |= LCCR0_LDM;
		wake_up(&fbi->ctrlr_wait);
		DPRINTK("\npxafb_handle_irq: lcsr==LCSR_LDD. ");
	}

	LCSR = lcsr;
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
			DPRINTK("\npxafb_set_ctrl_state: enter C_DISABLE_CLKCHANGE state.");
			fbi->state = state;
			pxafb_disable_controller(fbi);
		}
		break;

	case C_DISABLE:
		/*
		 * Disable controller
		 */
		if (old_state != C_DISABLE) {
			fbi->state = state;
            DPRINTK("\npxafb_set_ctrl_state: enter C_DISABLE state.");
			//pxafb_backlight_off(fbi);
			del_timer(&bklight_timer);
			pxafb_ezx_Backlight_turn_off();
			if (old_state != C_DISABLE_CLKCHANGE)
				pxafb_disable_controller(fbi);
			pxafb_power_down_lcd(fbi);
		}
		break;

	case C_ENABLE_CLKCHANGE:
		/*
		 * Enable the controller after clock change.  Only
		 * do this if we were disabled for the clock change.
		 */
		if (old_state == C_DISABLE_CLKCHANGE) {
			DPRINTK("\npxafb_set_ctrl_state: enter C_ENABLE_CLKCHANGE state.");
			fbi->state = C_ENABLE;
			pxafb_enable_controller(fbi);
		}
		break;

	case C_REENABLE:
		/*
		 * Re-enable the controller only if it was already
		 * enabled.  This is so we reprogram the control
		 * registers.
		 */
		if (old_state == C_ENABLE) {
			DPRINTK("\npxafb_set_ctrl_state: enter C_REENABLE state.");
			pxafb_disable_controller(fbi);
			pxafb_setup_gpio(fbi);
			pxafb_enable_controller(fbi);
		}
		break;

	case C_ENABLE:
		/*
		 * Power up the LCD screen, enable controller, and
		 * turn on the backlight.
		 */
		if (old_state != C_ENABLE) {
			DPRINTK("\npxafb_set_ctrl_state: enter C_ENABLE state.");
			fbi->state = C_ENABLE;
			pxafb_setup_gpio(fbi);
			pxafb_power_up_lcd(fbi);
			pxafb_enable_controller(fbi);
			//pxafb_backlight_on(fbi);
			mod_timer(&bklight_timer, jiffies + bk_timeout);
			//pxafb_ezx_Backlight_turn_on();
		}
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
    DPRINTK("\npxafb_task entered!");
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

	DPRINTK("pm_callback: %d\n", req);

	if (req == PM_SUSPEND || req == PM_RESUME) {
		int state = (int)data;
#if 0
//we enable/disable fb in PM, not here.
		if (state == 0) {
			/* Enter D0. */
			DPRINTK("\nenter pxafb_pm_callback: state==0");
			set_ctrlr_state(fbi, C_ENABLE);
		} else {
			/* Enter D1-D3.  Disable the LCD controller.  */
			DPRINTK("\nenter pxafb_pm_callback: state!=0");
			set_ctrlr_state(fbi, C_DISABLE);
		}
#endif		
	}
	DPRINTK("pxafb_pm_callback done\n");
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
	fbi->map_cpu = consistent_alloc(GFP_KERNEL, fbi->map_size,
					&fbi->map_dma);

	if (fbi->map_cpu) {
		fbi->screen_cpu = fbi->map_cpu + PAGE_SIZE;
		fbi->screen_dma = fbi->map_dma + PAGE_SIZE;
		fbi->fb.fix.smem_start = fbi->screen_dma;

		fbi->palette_size = fbi->fb.var.bits_per_pixel == 8 ? 256 : 16;

		palette_mem_size = fbi->palette_size * sizeof(u16);

		DPRINTK("palette_mem_size = 0x%08lx\n", (u_long) palette_mem_size);

		fbi->palette_cpu = (u16 *)(fbi->map_cpu + PAGE_SIZE - palette_mem_size);
		fbi->palette_dma = fbi->map_dma + PAGE_SIZE - palette_mem_size;
        DPRINTK("\npxafb: screen_cpu: %x", fbi->screen_cpu);
        DPRINTK("\npxafb: screen_dma: %x", fbi->screen_dma);
        DPRINTK("\npxafb: palette_cpu: %x", fbi->palette_cpu);
        DPRINTK("\npxafb: palette_dma: %x", fbi->palette_dma);
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	30000, 70000, 50, 65, 0	/* Generic */
};


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

	inf = pxafb_get_machine_info(fbi);

	fbi->max_xres			= inf->xres;
	fbi->fb.var.xres		= inf->xres;
	fbi->fb.var.xres_virtual	= inf->xres;
	fbi->max_yres			= inf->yres;
	fbi->fb.var.yres		= inf->yres;
	fbi->fb.var.yres_virtual	= inf->yres;
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
	fbi->fb.fix.smem_len		= fbi->max_xres * fbi->max_yres *
					  fbi->max_bpp / 8;

	init_waitqueue_head(&fbi->ctrlr_wait);
	INIT_TQUEUE(&fbi->task, pxafb_task, fbi);
	init_MUTEX(&fbi->ctrlr_sem);

	return fbi;
}

int __init pxafb_init(void)
{
	struct pxafb_info *fbi;
	int ret;

	fbi = pxafb_init_fbinfo();
	ret = -ENOMEM;
	if (!fbi)
		goto failed;

	if(machine_is_pxa_cerf()) {
		// brightness&contrast is handled via lcdctrl.
		lcdctrl_init();
	}

	/* Initialize video memory */
	ret = pxafb_map_video_memory(fbi);
	if (ret)
		goto failed;

	ret = request_irq(IRQ_LCD, pxafb_handle_irq, SA_INTERRUPT,
			  "LCD", fbi);
	if (ret) {
		printk(KERN_ERR "pxafb: failed in request_irq: %d\n", ret);
		goto failed;
	}

	pxafb_set_var(&fbi->fb.var, -1, &fbi->fb);

	ret = register_framebuffer(&fbi->fb);
	if (ret < 0)
		goto failed;

#ifdef CONFIG_PM
	/*
	 * Note that the console registers this as well, but we want to
	 * power down the display prior to sleeping.
	 */
	fbi->pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, pxafb_pm_callback);
	if (fbi->pm)
		fbi->pm->data = fbi;
#endif
#ifdef CONFIG_CPU_FREQ
	fbi->clockchg.notifier_call = pxafb_clkchg_notifier;
	cpufreq_register_notifier(&fbi->clockchg);
#endif
	init_timer(&bklight_timer);
	bklight_timer.function = pxafb_ezx_Backlight_turn_on;

	/*
	 * Ok, now enable the LCD controller
	 */
	set_ctrlr_state(fbi, C_ENABLE);

	bk_timeout = BK_TIMEOUT;
	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;

	return 0;

failed:
	if (fbi)
		kfree(fbi);
	return ret;
}

int __init pxafb_setup(char *options)
{
#if 0
	char *this_opt;

	if (!options || !*options)
		return 0;

	for (this_opt = strtok(options, ","); this_opt;
	     this_opt = strtok(NULL, ",")) {

		if (!strncmp(this_opt, "bpp:", 4))
			current_par.max_bpp =
			    simple_strtoul(this_opt + 4, NULL, 0);

		if (!strncmp(this_opt, "lccr0:", 6))
			lcd_shadow.lccr0 =
			    simple_strtoul(this_opt + 6, NULL, 0);
		if (!strncmp(this_opt, "lccr1:", 6)) {
			lcd_shadow.lccr1 =
			    simple_strtoul(this_opt + 6, NULL, 0);
			current_par.max_xres =
			    (lcd_shadow.lccr1 & 0x3ff) + 16;
		}
		if (!strncmp(this_opt, "lccr2:", 6)) {
			lcd_shadow.lccr2 =
			    simple_strtoul(this_opt + 6, NULL, 0);
			current_par.max_yres =
			    (lcd_shadow.
			     lccr0 & LCCR0_SDS) ? ((lcd_shadow.
						    lccr2 & 0x3ff) +
						   1) *
			    2 : ((lcd_shadow.lccr2 & 0x3ff) + 1);
		}
		if (!strncmp(this_opt, "lccr3:", 6))
			lcd_shadow.lccr3 =
			    simple_strtoul(this_opt + 6, NULL, 0);
	}
#endif
	return 0;

}
