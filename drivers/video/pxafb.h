/*
 * linux/drivers/video/pxafb.h
 * Intel Bulverde/PXA250/210 LCD Controller Frame Buffer Device
 *
 * Copyright (C) Motorola 2002-2004
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
 * 2002-Jun-30 - (Motorola) Ported to the LCD hardware on Motorola phone
 * 2003-May-19 - (Motorola) Added timer to turn on backlight until LCD controller is ready
 * 2003-Oct-17 - (Motorola) Added 18bpp/19bpp packed support
 * 2004-Jun-28 - (Motorola) Added double buffering for LCD controller
 */

#ifndef _PXA_FB_H_
#define _PXA_FB_H_

#include <linux/types.h>
/*
 * These are the bitfields for each
 * display depth that we support.
 */
struct pxafb_rgb {
	struct fb_bitfield	red;
	struct fb_bitfield	green;
	struct fb_bitfield	blue;
	struct fb_bitfield	transp;
};

/*
 * This structure describes the machine which we are running on.
 */
struct pxafb_mach_info {
	u_long		pixclock;

	u_short		xres;
	u_short		yres;

	u_char		bpp;
	u_char		hsync_len;
	u_char		left_margin;
	u_char		right_margin;

	u_char		vsync_len;
	u_char		upper_margin;
	u_char		lower_margin;
	u_char		sync;

	u_int		cmap_greyscale:1,
			cmap_inverse:1,
			cmap_static:1,
			unused:29;

	u_int		lccr0;
	u_int		lccr3;
};

/* Shadows for LCD controller registers */
struct pxafb_lcd_reg {
	unsigned int lccr0;
	unsigned int lccr1;
	unsigned int lccr2;
	unsigned int lccr3;
};

/* PXA LCD DMA descriptor */
struct pxafb_dma_descriptor {
	unsigned int fdadr;
	unsigned int fsadr;
	unsigned int fidr;
	unsigned int ldcmd;
};

#define RGB_8	(0)
#define RGB_16	(1)
#ifdef CONFIG_CPU_BULVERDE
#define RGB_18  (2)
#define RGB_24  (3)
#define NR_RGB  4
#else
#define NR_RGB	2
#endif

#ifdef CONFIG_CPU_BULVERDE
struct overlayfb_info;
#endif

#ifdef CONFIG_ARCH_EZX_A780
#define CONFIG_FBBASE_DOUBLE_BUFFERS 1
#endif

#ifdef CONFIG_ARCH_EZX_E680
#define CONFIG_FBBASE_DOUBLE_BUFFERS 0
#endif

struct db_entry_s
{
	pid_t pid;
	//JUST FOR TESIING	struct task_struct *task;
	struct list_head task_list;
};

struct fb_proto
{
	u_char *		map_cpu;	/* virtual */
	dma_addr_t		map_dma;	/* physical -- this map contains dma descriptor */

	/* addresses of dma descriptor corresponding to this framebuffer */
	struct pxafb_dma_descriptor * 	dmadesc_fb_cpu;
	dma_addr_t				dmadesc_fb_dma;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of frame buffer */
	dma_addr_t		screen_dma;	/* physical address of frame buffer */
	
};

struct pxafb_info {
	struct fb_info		fb;
	signed int		currcon;

	struct pxafb_rgb	*rgb[NR_RGB];

	u_int			max_bpp;
	u_int			max_xres;
	u_int			max_yres;

	/*
	 * These are the addresses we mapped
	 * the framebuffer memory region to.
	 */

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of frame buffer */
	dma_addr_t		screen_dma;	/* physical address of frame buffer */
	u16 *			palette_cpu;	/* virtual address of palette memory */
	dma_addr_t		palette_dma;	/* physical address of palette memory */
	u_int			palette_size;

	/* DMA descriptors */
	struct pxafb_dma_descriptor * 	dmadesc_fblow_cpu;
	dma_addr_t				dmadesc_fblow_dma;
	struct pxafb_dma_descriptor * 	dmadesc_fbhigh_cpu;
	dma_addr_t				dmadesc_fbhigh_dma;
	struct pxafb_dma_descriptor *	dmadesc_palette_cpu;
	dma_addr_t				dmadesc_palette_dma;

    /* Added by Motorola for double buffering of baseframe */
	struct semaphore dbsem;  
	struct fb_proto first_fb;   
	struct fb_proto second_fb;	
	struct list_head db_task_list;  

	dma_addr_t		fdadr0;
	dma_addr_t		fdadr1;

	u_int			lccr0;
	u_int			lccr3;
	u_int			cmap_inverse:1,
				cmap_static:1,
				unused:30;

	u_int			reg_lccr0;
	u_int			reg_lccr1;
	u_int			reg_lccr2;
	u_int			reg_lccr3;

	volatile u_char		state;
	volatile u_char		task_state;
	struct semaphore	ctrlr_sem;
	wait_queue_head_t	ctrlr_wait;
	struct tq_struct	task;

#ifdef CONFIG_PM
	struct pm_dev		*pm;
#endif
#ifdef CONFIG_CPU_FREQ
	struct notifier_block	clockchg;
#endif
#ifdef CONFIG_CPU_BULVERDE
	struct overlayfb_info  *overlay1fb;
	struct overlayfb_info  *overlay2fb;
	struct overlayfb_info  *cursorfb;
#endif
};

#ifdef CONFIG_CPU_BULVERDE
struct overlayfb_info
{
	struct fb_info	fb;

        struct semaphore mutex;
        unsigned long    refcount;

	struct pxafb_info *basefb;

	unsigned long	map_cpu;
	unsigned long 	screen_cpu;
	unsigned long	palette_cpu;
	unsigned long 	map_size;
	unsigned long   palette_size;

	dma_addr_t 	screen_dma;
	dma_addr_t	map_dma;
	dma_addr_t	palette_dma;

	unsigned long enabled;

	volatile u_char	state;

	/* overlay specific info */
	unsigned long	xpos;		/* screen position (x, y)*/
	unsigned long	ypos;		
	unsigned long 	format;

	/* additional */
	union {
		struct pxafb_dma_descriptor *dma0;
		struct pxafb_dma_descriptor *dma1;
		struct {
			struct pxafb_dma_descriptor *dma2;
			struct pxafb_dma_descriptor *dma3;
			struct pxafb_dma_descriptor *dma4;
		};
		struct {
			struct pxafb_dma_descriptor *dma5_pal;
			struct pxafb_dma_descriptor *dma5_frame;
		};
	};
};
#endif /* CONFIG_CPU_BULVERDE */

#define __type_entry(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))

#define TO_INF(ptr,member)	__type_entry(ptr,struct pxafb_info,member)

/*
 * These are the actions for set_ctrlr_state
 */
#define C_DISABLE		(0)
#define C_ENABLE		(1)
#define C_DISABLE_CLKCHANGE	(2)
#define C_ENABLE_CLKCHANGE	(3)
#define C_REENABLE		(4)

#define PXA_NAME	"PXA"

/*
 *  Debug macros 
 */
#if DEBUG
#  define DPRINTK(fmt, args...)	printk("PXA FB:%d: " fmt "\n", __LINE__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/*
 * Minimum X and Y resolutions
 */
#define MIN_XRES	64
#define MIN_YRES	64

/*
 * Default pixclock frequencies for some displays
 */
#define LTM04C380K_PIXEL_CLOCK_FREQUENCY       2518
#define LM8V31_PIXEL_CLOCK_FREQUENCY           454
#define LQ64D341_PIXEL_CLOCK_FREQUENCY         385
#define LTM035A776C_PIXEL_CLOCK_FREQUENCY      910

/*
 * Are we configured for 8 or 16 bits per pixel?
 */
#if defined(CONFIG_FB_PXA_8BPP)
#  define PXAFB_BPP		8
#  define PXAFB_BPP_BITS	0x03
#elif defined(CONFIG_FB_PXA_16BPP)
#  define PXAFB_BPP		16
#  define PXAFB_BPP_BITS	0x04
#elif defined(CONFIG_FB_PXA_18BPP)
#  define PXAFB_BPP		18
#elif defined(CONFIG_FB_PXA_19BPP)
#  define PXAFB_BPP		19
#elif defined(CONFIG_FB_PXA_24BPP)
#  define PXAFB_BPP		24
#elif defined(CONFIG_FB_PXA_25BPP)
#  define PXAFB_BPP		25
#endif

#if defined(CONFIG_ARCH_LUBBOCK)
#define LCD_PIXCLOCK			150000
#define LCD_BPP				PXAFB_BPP
#ifdef CONFIG_FB_PXA_QVGA
#define LCD_XRES			320
#define LCD_YRES			240
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	51
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	1
#define LCD_BEGIN_FRAME_WAIT_COUNT 	8
#define LCD_END_OF_LINE_WAIT_COUNT	1
#define LCD_END_OF_FRAME_WAIT_COUNT	1
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			0x003008F8
#define LCD_LCCR3 			(0x0040FF0C | (PXAFB_BPP_BITS << 24))
#else
#define LCD_XRES			640
#define LCD_YRES			480
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	1
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	3
#define LCD_BEGIN_FRAME_WAIT_COUNT 	0
#define LCD_END_OF_LINE_WAIT_COUNT	3
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			0x0030087C
#define LCD_LCCR3 			(0x0040FF0C | (PXAFB_BPP_BITS << 24))
#endif

#elif defined(CONFIG_ARCH_PXA_IDP)
#define LCD_PIXCLOCK			150000
#define LCD_BPP				PXAFB_BPP
#define LCD_XRES			640
#define LCD_YRES			480
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	1
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	3
#define LCD_BEGIN_FRAME_WAIT_COUNT 	0
#define LCD_END_OF_LINE_WAIT_COUNT	3
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			0x0030087C
#define LCD_LCCR3 			(0x0040FF0C | (PXAFB_BPP_BITS << 24))

#elif defined(CONFIG_PXA_CERF_PDA)
#define LCD_PIXCLOCK			171521
#define LCD_BPP				PXAFB_BPP
#define LCD_XRES			240
#define LCD_YRES			320
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	7
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	2
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	17
#define LCD_BEGIN_FRAME_WAIT_COUNT 	0
#define LCD_END_OF_LINE_WAIT_COUNT	17
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			(LCCR0_LDM | LCCR0_SFM | LCCR0_IUM | LCCR0_EFM | LCCR0_QDM | LCCR0_BM  | LCCR0_OUM)
#define LCD_LCCR3 			(LCCR3_PCP | LCCR3_PixClkDiv(0x12) | LCCR3_Bpp(PXAFB_BPP_BITS) | LCCR3_Acb(0x18))

#elif defined(CONFIG_CPU_BULVERDE)

#ifdef CONFIG_FB_PXA_QVGA
#define LCD_PIXCLOCK			150000
#define LCD_BPP				PXAFB_BPP
#define LCD_XRES			240
#define LCD_YRES			320
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH        10
#define LCD_VERTICAL_SYNC_PULSE_WIDTH          2
#define LCD_BEGIN_OF_LINE_WAIT_COUNT           20
#define LCD_BEGIN_FRAME_WAIT_COUNT             3
#define LCD_END_OF_LINE_WAIT_COUNT             10
#define LCD_END_OF_FRAME_WAIT_COUNT            2
#define LCD_SYNC                        (FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0                       0x002008F8 // set OUC for overlays 0x023008F8   //mask-BSM0 0x003008F8
#ifdef CONFIG_FB_PXA_18BPP
#define LCD_LCCR3                       0xC630FF09  /* 18 bpp */
#else
#define LCD_LCCR3			0x0430FF09  /* 16 bpp */
#endif

/*
#elif defined(CONFIG_FB_PXA_QVGA)
#define LCD_PIXCLOCK			150000
#define LCD_BPP				PXAFB_BPP
#define LCD_XRES			240
#define LCD_YRES			320
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	1
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	3
#define LCD_BEGIN_FRAME_WAIT_COUNT	0
#define LCD_END_OF_LINE_WAIT_COUNT	3
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			(LCCR0_OUC | LCCR0_CMDIM | LCCR0_RDSTM | LCCR0_OUM \
					 | LCCR0_BM | LCCR0_QDM | LCCR0_PAS | LCCR0_EFM \
					 | LCCR0_IUM | LCCR0_SFM | LCCR0_LDM)
#define LCD_LCCR3 	                (LCCR3_PCP | LCCR3_HSP | LCCR3_VSP)
*/
#else /* QVGA assumed */
#define LCD_PIXCLOCK			150000
#define LCD_BPP				16
#define LCD_XRES			240
#define LCD_YRES			320
#define LCD_HORIZONTAL_SYNC_PULSE_WIDTH	1
#define LCD_VERTICAL_SYNC_PULSE_WIDTH	1
#define LCD_BEGIN_OF_LINE_WAIT_COUNT	3
#define LCD_BEGIN_FRAME_WAIT_COUNT	0
#define LCD_END_OF_LINE_WAIT_COUNT	3
#define LCD_END_OF_FRAME_WAIT_COUNT	0
#define LCD_SYNC			(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT)
#define LCD_LCCR0			(LCCR0_OUC | LCCR0_CMDIM | LCCR0_RDSTM | LCCR0_OUM | LCCR0_BM | LCCR0_QDM | LCCR0_PAS \
                                         | LCCR0_EFM | LCCR0_IUM | LCCR0_SFM | LCCR0_LDM)
#define LCD_LCCR3 	                (LCCR3_PCP | LCCR3_HSP | LCCR3_VSP )
#endif

#endif

#if defined(CONFIG_CPU_BULVERDE)
/* We can place framebuffer memory into SRAM on Bulverde. */
extern int sram_access_obtain(unsigned long *, unsigned long *);
extern int sram_access_release(unsigned long *, unsigned long *);
#endif

#endif /* _PXA_FB_H_ */
