/*
 *  pxa_camera.c
 *
 *  Bulverde Processor Camera Interface driver.
 *
 *  Copyright (C) 2003, Intel Corporation
 *  Copyright (C) 2003, Montavista Software Inc.
 *  Copyright (C) 2004  Motorola
 *
 *  Author: Intel Corporation Inc.
 *          MontaVista Software, Inc.
 *           source@mvista.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  June 23,2004 - (Motorola) Created new file based on the pxa_camera.c file
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <linux/wrapper.h>
#include <linux/videodev.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/poll.h>
#include <linux/wait.h>
#ifdef CONFIG_DPM
#include <linux/device.h>
#include <asm/arch/ldm.h>
#endif

#include <linux/types.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>


#include "camera.h"


#ifdef CONFIG_CAMERA_OV9640
#define   OV9640
#elif defined CONFIG_CAMERA_ADCM2700
#define   ADCM2700
#elif defined CONFIG_CAMERA_ADCM2650
#define   ADCM2650
#endif

#ifdef ADCM2650
#include "pxa_camera.h"
#include "adcm2650.h"
#include "adcm2650_hw.h"
#endif

#ifdef ADCM2700
#include "adcm2700.h"
#include "adcm2700_hw.h"
#endif

#ifdef OV9640
#include "ov9640.h"
#include "ov9640_hw.h"
#endif

#define PREFIX "PXA camera: "

#ifdef ADCM2650
#define MAX_WIDTH	480
#define MAX_HEIGHT	640
#define MIN_WIDTH	72
#define MIN_HEIGHT	72
#endif

#ifdef ADCM2700
#define MAX_WIDTH	480
#define MAX_HEIGHT	640
#define MIN_WIDTH	72
#define MIN_HEIGHT	72
#endif

#ifdef OV9640
/* in ov9640.h */
#endif

#define MAX_BPP		32
#define WIDTH_DEFT	320
#define HEIGHT_DEFT	240
#define FRAMERATE_DEFT	0x0


/*
 * It is required to have at least 3 frames in buffer
 * in current implementation
 */
#define FRAMES_IN_BUFFER	3
#define MIN_FRAMES_IN_BUFFER	3
#define MAX_FRAME_SIZE		(MAX_WIDTH * MAX_HEIGHT * (MAX_BPP >> 3))
#define BUF_SIZE_DEFT		(MAX_FRAME_SIZE * MIN_FRAMES_IN_BUFFER)
#define SINGLE_DESC_TRANS_MAX  	PAGE_SIZE
#define MAX_DESC_NUM		((MAX_FRAME_SIZE / SINGLE_DESC_TRANS_MAX + 1) *\
				 MIN_FRAMES_IN_BUFFER*2)

#define MAX_BLOCK_NUM	20

static camera_context_t *g_camera_context = NULL;
#ifdef ADCM2650
static camera_function_t adcm2650_func;
#endif
#ifdef ADCM2700
static camera_function_t adcm2700_func;
#endif
#ifdef OV9640
static camera_function_t ov9640_func;
#endif
wait_queue_head_t camera_wait_q;

/* /dev/videoX registration number */
static int minor = 0;
static int ci_dma_y = -1;
static int ci_dma_cb = -1;
static int ci_dma_cr = -1;
volatile int task_waiting = 0;
static int still_image_mode = 0;
static int still_image_rdy = 0;
static int first_video_frame = 0;

void pxa_ci_dma_irq_y(int channel, void *data, struct pt_regs *regs);
void pxa_ci_dma_irq_cb(int channel, void *data, struct pt_regs *regs);
void pxa_ci_dma_irq_cr(int channel, void *data, struct pt_regs *regs);

static unsigned long ci_regs_base = 0;	/* for CI registers IOMEM mapping */

#define CI_REG(x)             (* (volatile u32*)(x) )
#define CI_REG_SIZE             0x40	/* 0x5000_0000 --- 0x5000_0038 * 64K */
#define CI_REGS_PHYS            0x50000000	/* Start phyical address of CI registers */
/* placed in include/asm/arch/pxa_regs.h
#define CICR0		        CI_REG((u32)(ci_regs_base) + 0x00)
#define CICR1           	CI_REG((u32)(ci_regs_base) + 0x04)
#define CICR2 		        CI_REG((u32)(ci_regs_base) + 0x08)
#define CICR3           	CI_REG((u32)(ci_regs_base) + 0x0c)
#define CICR4             	CI_REG((u32)(ci_regs_base) + 0x10)
#define CISR 	        	CI_REG((u32)(ci_regs_base) + 0x14)
#define CIFR              	CI_REG((u32)(ci_regs_base) + 0x18)
#define CITOR             	CI_REG((u32)(ci_regs_base) + 0x1c)
#define CIBR0             	CI_REG((u32)(ci_regs_base) + 0x28)
#define CIBR1             	CI_REG((u32)(ci_regs_base) + 0x30)
#define CIBR2             	CI_REG((u32)(ci_regs_base) + 0x38)
*/
/***********************************************************************
 *
 * Declarations
 *
 ***********************************************************************/

// map of camera image format (camera.h) ==> capture interface format (ci.h)
static const CI_IMAGE_FORMAT FORMAT_MAPPINGS[] = {
	CI_RAW8,		//RAW
	CI_RAW9,
	CI_RAW10,

	CI_RGB444,		//RGB
	CI_RGB555,
	CI_RGB565,
	CI_RGB666_PACKED,	//RGB Packed 
	CI_RGB666,
	CI_RGB888_PACKED,
	CI_RGB888,
	CI_RGBT555_0,		//RGB+Transparent bit 0
	CI_RGBT888_0,
	CI_RGBT555_1,		//RGB+Transparent bit 1  
	CI_RGBT888_1,

	CI_INVALID_FORMAT,
	CI_YCBCR422,		//YCBCR
	CI_YCBCR422_PLANAR,	//YCBCR Planaried
	CI_INVALID_FORMAT,
	CI_INVALID_FORMAT
};

static int update_dma_chain(p_camera_context_t cam_ctx);
void start_dma_transfer(p_camera_context_t cam_ctx, unsigned block_id);
void stop_dma_transfer(p_camera_context_t cam_ctx);
static int start_capture(p_camera_context_t cam_ctx, unsigned int block_id, unsigned int frames);

static void pxa_dma_repeat(camera_context_t * cam_ctx);
static void pxa_dma_continue(camera_context_t * cam_ctx);

#ifdef ADCM2650
extern int adcm2650_pipeline_read(u16 reg_addr, u16 * reg_value);
extern int adcm2650_pipeline_write(u16 reg_addr, u16 reg_value);
#define sensor_pipeline_read adcm2650_pipeline_read
#define sensor_pipeline_write adcm2650_pipeline_write

#endif

#ifdef ADCM2700
extern int adcm2700_pipeline_read(u16 reg_addr, u16 * reg_value);
extern int adcm2700_pipeline_write(u16 reg_addr, u16 reg_value);
#define sensor_pipeline_read adcm2700_pipeline_read
#define sensor_pipeline_write adcm2700_pipeline_write
#endif

#ifdef OV9640
//int ov9640_pipeline_read( u16 reg_addr, u16 * reg_value){return 0;}
//int ov9640_pipeline_write( u16 reg_addr, u16 reg_value){return 0;}
#define sensor_pipeline_read ov9640_read
#define sensor_pipeline_write ov9640_write
#endif
/***************
 *
 * DPM functions
 *
 ***************/
#ifdef CONFIG_DPM

static int last_buffer_id;

static int pxa_camera_dpm_suspend(struct device *dev, u32 state, u32 level)
{
	DPRINTK(KERN_DEBUG PREFIX "DPM suspend (state %d, level %d)\n", state, level);
	switch (level) {
	case SUSPEND_POWER_DOWN:
		if (g_camera_context->dma_started) {
			DPRINTK(KERN_DEBUG PREFIX "DMA running, suspended\n");
			last_buffer_id = camera_get_last_frame_buffer_id(g_camera_context);
			stop_dma_transfer(g_camera_context);
		}
		disable_irq(IRQ_CAMERA);
		CKEN &= ~CKEN24_CAMERA;
		break;
	}
	return 0;
}

static int pxa_camera_dpm_resume(struct device *dev, u32 level)
{
	DPRINTK(KERN_DEBUG PREFIX "DPM resume (level %d)\n", level);
	switch (level) {
	case RESUME_POWER_ON:
		CKEN |= CKEN24_CAMERA;
		enable_irq(IRQ_CAMERA);
		if (g_camera_context->dma_started) {
			DPRINTK(KERN_DEBUG PREFIX "resume DMA\n");
			start_dma_transfer(g_camera_context, last_buffer_id);
		}
		break;
	}
	return 0;
}

static int pxa_camera_dpm_scale(struct bus_op_point *op, u32 level)
{
	DPRINTK(KERN_DEBUG PREFIX "DPM scale (level %d)\n", level);
	/* CCCR is changed - adjust clock */
	ci_set_clock(g_camera_context->clk_reg_base, 1, 1, 7 /* MCLK_DEFT in adcm2650.c */ );
	return 0;
}

static struct device_driver pxa_camera_driver_ldm = {
	name:"camera",
	devclass:NULL,
	probe:NULL,
	suspend:pxa_camera_dpm_suspend,
	resume:pxa_camera_dpm_resume,
	scale:pxa_camera_dpm_scale,
	remove:NULL,
	constraints:NULL
};

static struct device pxa_camera_device_ldm = {
	name:"PXA camera",
	bus_id:"video",
	driver:NULL,
	power_state:DPM_POWER_ON
};

static void pxa_camera_ldm_register(void)
{
	pxaebc_driver_register(&pxa_camera_driver_ldm);
	pxaebc_device_register(&pxa_camera_device_ldm);
}

static void pxa_camera_ldm_unregister(void)
{
	pxaebc_driver_unregister(&pxa_camera_driver_ldm);
	pxaebc_device_unregister(&pxa_camera_device_ldm);
}
#endif				/* CONFIG_DPM */
#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
static int resume_dma = 0;

static int pxa_camera_pm_suspend()
{
	if (g_camera_context != NULL) {
		if (g_camera_context->dma_started) {
			dbg_print("camera running, suspended");
			stop_dma_transfer(g_camera_context);
			resume_dma = 1;
		}
	}

	disable_irq(IRQ_CAMERA);
	CKEN &= ~CKEN24_CAMERA;
	return 0;
}

static int pxa_camera_pm_resume()
{
	CKEN |= CKEN24_CAMERA;
	enable_irq(IRQ_CAMERA);

	DPRINTK(" in %s, camera running, resumed", __FUNCTION__);
	if (g_camera_context != NULL) {
		/*
		 */
		struct video_window vw;
		vw.width = g_camera_context->capture_width;
		vw.height = g_camera_context->capture_height;
		ov9640_set_window(&vw);

		// set_bright(g_camera_context->capture_bright);
		ov9640_set_expose_compensation(g_camera_context->capture_bright);
		// set_fps(g_camera_context->fps, g_camera_context->mini_fps);
		ov9640_set_fps(g_camera_context->fps);
		//  set_light(g_camera_context->capture_light);
		ov9640_set_light_environment(g_camera_context->capture_light);
		// set_style(g_camera_context->capture_style);
		ov9640_set_special_effect(g_camera_context->capture_style);

		if (resume_dma == 1) {
			camera_start_video_capture(g_camera_context, 0);
			resume_dma = 0;
		}

	}

	return 0;
}
static int camera_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		pxa_camera_pm_suspend();
		break;

	case PM_RESUME:
		pxa_camera_pm_resume();
		break;

	default:
		break;
	}
	return 0;
}
#endif

/***********************************************************************
 *
 * Private functions
 *
 ***********************************************************************/

static int pxa_dma_buffer_init(p_camera_context_t cam_ctx)
{
	struct page *page;
	unsigned int pages;
	unsigned int page_count;

	cam_ctx->pages_allocated = 0;

	pages = (PAGE_ALIGN(cam_ctx->buf_size) / PAGE_SIZE);

	cam_ctx->page_array = (struct page **) kmalloc(pages * sizeof(struct page *), GFP_KERNEL);
	if (cam_ctx->page_array == NULL) {
		return -ENOMEM;
	}
	memset(cam_ctx->page_array, 0, pages * sizeof(struct page *));

	for (page_count = 0; page_count < pages; page_count++) {
		page = alloc_page(GFP_KERNEL);
		if (page == NULL) {
			goto error;
		}
		cam_ctx->page_array[page_count] = page;
		set_page_count(page, 1);
		SetPageReserved(page);
	}
	cam_ctx->buffer_virtual = remap_page_array(cam_ctx->page_array, pages, GFP_KERNEL);

	if (cam_ctx->buffer_virtual == NULL) {
		goto error;
	}

	cam_ctx->pages_allocated = pages;

	return 0;

      error:
	for (page_count = 0; page_count < pages; page_count++) {
		if ((page = cam_ctx->page_array[page_count]) != NULL) {
			ClearPageReserved(page);
			set_page_count(page, 1);
			put_page(page);
		}
	}
	kfree(cam_ctx->page_array);

	return -ENOMEM;
}

static void pxa_dma_buffer_free(p_camera_context_t cam_ctx)
{
	struct page *page;
	int page_count;

	if (cam_ctx->buffer_virtual == NULL)
		return;

	vfree(cam_ctx->buffer_virtual);

	for (page_count = 0; page_count < cam_ctx->pages_allocated; page_count++) {
		if ((page = cam_ctx->page_array[page_count]) != NULL) {
			ClearPageReserved(page);
			set_page_count(page, 1);
			put_page(page);
		}
	}
	kfree(cam_ctx->page_array);
}

/*
Generate dma descriptors
Pre-condition: these variables must be set properly
                block_number, fifox_transfer_size 
                dma_descriptors_virtual, dma_descriptors_physical, dma_descirptors_size
Post-condition: these variables will be set
                fifox_descriptors_virtual, fifox_descriptors_physical              
                fifox_num_descriptors 
*/
int update_dma_chain(p_camera_context_t cam_ctx)
{
	pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
	int des_transfer_size, remain_size;
	unsigned int i, j;

	int target_page_num;

	// clear descriptor pointers
	cam_ctx->fifo0_descriptors_virtual = cam_ctx->fifo0_descriptors_physical = 0;
	cam_ctx->fifo1_descriptors_virtual = cam_ctx->fifo1_descriptors_physical = 0;
	cam_ctx->fifo2_descriptors_virtual = cam_ctx->fifo2_descriptors_physical = 0;

	// calculate how many descriptors are needed per frame
	cam_ctx->fifo0_num_descriptors = cam_ctx->pages_per_fifo0;

	cam_ctx->fifo1_num_descriptors = cam_ctx->pages_per_fifo1;

	cam_ctx->fifo2_num_descriptors = cam_ctx->pages_per_fifo2;

	// check if enough memory to generate descriptors
	if ((cam_ctx->fifo0_num_descriptors + cam_ctx->fifo1_num_descriptors +
	     cam_ctx->fifo2_num_descriptors) * cam_ctx->block_number > cam_ctx->dma_descriptors_size)
		return -1;

	// generate fifo0 dma chains
	cam_ctx->fifo0_descriptors_virtual = (unsigned) cam_ctx->dma_descriptors_virtual;
	cam_ctx->fifo0_descriptors_physical = (unsigned) cam_ctx->dma_descriptors_physical;
	cur_des_virtual = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual;
	cur_des_physical = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_physical;

	for (i = 0; i < cam_ctx->block_number; i++) {
		// in each iteration, generate one dma chain for one frame
		remain_size = cam_ctx->fifo0_transfer_size;

		// assume the blocks are stored consecutively
		target_page_num = cam_ctx->pages_per_block * i;

		for (j = 0; j < cam_ctx->fifo0_num_descriptors; j++) {
			// set descriptor
			if (remain_size > SINGLE_DESC_TRANS_MAX)
				des_transfer_size = SINGLE_DESC_TRANS_MAX;
			else
				des_transfer_size = remain_size;
			cur_des_virtual->ddadr = (unsigned) cur_des_physical + sizeof(pxa_dma_desc);
			cur_des_virtual->dsadr = CIBR0_PHY;	// FIFO0 physical address
			cur_des_virtual->dtadr = page_to_bus(cam_ctx->page_array[target_page_num]);
			cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

			// advance pointers
			remain_size -= des_transfer_size;
			cur_des_virtual++;
			cur_des_physical++;
			target_page_num++;
		}

		// stop the dma transfer on one frame captured
		last_des_virtual = cur_des_virtual - 1;
		//last_des_virtual->ddadr |= 0x1;
	}

	last_des_virtual->ddadr = ((unsigned) cam_ctx->fifo0_descriptors_physical);

	// generate fifo1 dma chains
	if (cam_ctx->fifo1_transfer_size) {
		// record fifo1 descriptors' start address
		cam_ctx->fifo1_descriptors_virtual = (unsigned) cur_des_virtual;
		cam_ctx->fifo1_descriptors_physical = (unsigned) cur_des_physical;

		for (i = 0; i < cam_ctx->block_number; i++) {
			// in each iteration, generate one dma chain for one frame
			remain_size = cam_ctx->fifo1_transfer_size;

			target_page_num = cam_ctx->pages_per_block * i + cam_ctx->pages_per_fifo0;

			for (j = 0; j < cam_ctx->fifo1_num_descriptors; j++) {
				// set descriptor
				if (remain_size > SINGLE_DESC_TRANS_MAX)
					des_transfer_size = SINGLE_DESC_TRANS_MAX;
				else
					des_transfer_size = remain_size;
				cur_des_virtual->ddadr = (unsigned) cur_des_physical + sizeof(pxa_dma_desc);
				cur_des_virtual->dsadr = CIBR1_PHY;	// FIFO1 physical address
				cur_des_virtual->dtadr = page_to_bus(cam_ctx->page_array[target_page_num]);
				cur_des_virtual->dcmd =
				    des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

				// advance pointers
				remain_size -= des_transfer_size;
				cur_des_virtual++;
				cur_des_physical++;

				target_page_num++;
			}

			// stop the dma transfer on one frame captured
			last_des_virtual = cur_des_virtual - 1;
			//last_des_virtual->ddadr |= 0x1;
		}
		last_des_virtual->ddadr = ((unsigned) cam_ctx->fifo1_descriptors_physical);
	}
	// generate fifo2 dma chains
	if (cam_ctx->fifo2_transfer_size) {
		// record fifo1 descriptors' start address
		cam_ctx->fifo2_descriptors_virtual = (unsigned) cur_des_virtual;
		cam_ctx->fifo2_descriptors_physical = (unsigned) cur_des_physical;

		for (i = 0; i < cam_ctx->block_number; i++) {
			// in each iteration, generate one dma chain for one frame
			remain_size = cam_ctx->fifo2_transfer_size;

			target_page_num = cam_ctx->pages_per_block * i +
			    cam_ctx->pages_per_fifo0 + cam_ctx->pages_per_fifo1;

			for (j = 0; j < cam_ctx->fifo2_num_descriptors; j++) {
				// set descriptor
				if (remain_size > SINGLE_DESC_TRANS_MAX)
					des_transfer_size = SINGLE_DESC_TRANS_MAX;
				else
					des_transfer_size = remain_size;
				cur_des_virtual->ddadr = (unsigned) cur_des_physical + sizeof(pxa_dma_desc);
				cur_des_virtual->dsadr = CIBR2_PHY;	// FIFO2 physical address
				cur_des_virtual->dtadr = page_to_bus(cam_ctx->page_array[target_page_num]);
				cur_des_virtual->dcmd =
				    des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

				// advance pointers
				remain_size -= des_transfer_size;
				cur_des_virtual++;
				cur_des_physical++;
				target_page_num++;
			}

			// stop the dma transfer on one frame captured
			last_des_virtual = cur_des_virtual - 1;
			//last_des_virtual->ddadr |= 0x1;
		}
		last_des_virtual->ddadr = ((unsigned) cam_ctx->fifo2_descriptors_physical);
	}
	return 0;
}

void start_dma_transfer(p_camera_context_t cam_ctx, unsigned block_id)
{
	pxa_dma_desc *des_virtual, *des_physical;

	DPRINTK("in %s,cam_ctx->block_number =%d\n", __FUNCTION__, cam_ctx->block_number);
	/*
	   if (block_id >= cam_ctx->block_number)
	   return;
	 */
	// start channel 0
	des_virtual = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual + block_id * cam_ctx->fifo0_num_descriptors;
	des_physical = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_physical + block_id * cam_ctx->fifo0_num_descriptors;

	DDADR(cam_ctx->dma_channels[0]) = des_physical;
	DCSR(cam_ctx->dma_channels[0]) |= DCSR_RUN;

	// start channel 1
	if (cam_ctx->fifo1_descriptors_virtual) {
		des_virtual = (pxa_dma_desc *) cam_ctx->fifo1_descriptors_virtual +
		    block_id * cam_ctx->fifo1_num_descriptors;
		des_physical = (pxa_dma_desc *) cam_ctx->fifo1_descriptors_physical +
		    block_id * cam_ctx->fifo1_num_descriptors;
		DDADR(cam_ctx->dma_channels[1]) = des_physical;
		DCSR(cam_ctx->dma_channels[1]) |= DCSR_RUN;
	}
	// start channel 2
	if (cam_ctx->fifo2_descriptors_virtual) {
		des_virtual = (pxa_dma_desc *) cam_ctx->fifo2_descriptors_virtual +
		    block_id * cam_ctx->fifo2_num_descriptors;
		des_physical = (pxa_dma_desc *) cam_ctx->fifo2_descriptors_physical +
		    block_id * cam_ctx->fifo2_num_descriptors;
		DDADR(cam_ctx->dma_channels[2]) = des_physical;
		DCSR(cam_ctx->dma_channels[2]) |= DCSR_RUN;
	}
	cam_ctx->dma_started = 1;
}

void set_still_image_ready(int rdy)
{
	still_image_rdy = rdy;
}

void stop_dma_transfer(p_camera_context_t cam_ctx)
{
	int ch0, ch1, ch2;

	ch0 = cam_ctx->dma_channels[0];
	ch1 = cam_ctx->dma_channels[1];
	ch2 = cam_ctx->dma_channels[2];
	DCSR(ch0) &= ~DCSR_RUN;
	DCSR(ch1) &= ~DCSR_RUN;
	DCSR(ch2) &= ~DCSR_RUN;
	cam_ctx->dma_started = 0;
#ifdef CONFIG_DPM
#endif
}
int start_capture(p_camera_context_t cam_ctx, unsigned int block_id, unsigned int frames)
{
	int status;
	int i;

	// clear ci fifo
	ci_reset_fifo();
	ci_clear_int_status(0xFFFFFFFF);

	// start dma
	start_dma_transfer(cam_ctx, block_id);

	// start capture
	status = cam_ctx->camera_functions->start_capture(cam_ctx, frames);
	return status;
}

/***********************************************************************
 *
 * Init/Deinit APIs
 *
 ***********************************************************************/
int camera_init(p_camera_context_t cam_ctx)
{
	int ret = 0;
	int i;

// parameter check
	if (cam_ctx->buffer_virtual == NULL || cam_ctx->buf_size == 0)
		return STATUS_WRONG_PARAMETER;
	if (cam_ctx->dma_descriptors_virtual == NULL ||
	    cam_ctx->dma_descriptors_physical == NULL || cam_ctx->dma_descriptors_size == 0)
		return STATUS_WRONG_PARAMETER;
	if (cam_ctx->sensor_type > CAMERA_TYPE_MAX)
		return STATUS_WRONG_PARAMETER;
	if (cam_ctx->capture_input_format > CAMERA_IMAGE_FORMAT_MAX ||
	    cam_ctx->capture_output_format > CAMERA_IMAGE_FORMAT_MAX)
		return STATUS_WRONG_PARAMETER;

	// check the function dispatch table according to the sensor type
	if (!cam_ctx->camera_functions)
		return STATUS_WRONG_PARAMETER;
	if (!cam_ctx->camera_functions->init ||
	    !cam_ctx->camera_functions->deinit ||
	    !cam_ctx->camera_functions->set_capture_format ||
	    !cam_ctx->camera_functions->start_capture || !cam_ctx->camera_functions->stop_capture)
		return STATUS_WRONG_PARAMETER;

	// init context status
	for (i = 0; i < 3; i++)
		cam_ctx->dma_channels[i] = 0xFF;
	(int) cam_ctx->fifo0_descriptors_virtual = NULL;
	(int) cam_ctx->fifo1_descriptors_virtual = NULL;
	(int) cam_ctx->fifo2_descriptors_virtual = NULL;
	(int) cam_ctx->fifo0_descriptors_physical = NULL;
	(int) cam_ctx->fifo1_descriptors_physical = NULL;
	(int) cam_ctx->fifo2_descriptors_physical = NULL;

	cam_ctx->fifo0_num_descriptors = 0;
	cam_ctx->fifo1_num_descriptors = 0;
	cam_ctx->fifo2_num_descriptors = 0;

	cam_ctx->fifo0_transfer_size = 0;
	cam_ctx->fifo1_transfer_size = 0;
	cam_ctx->fifo2_transfer_size = 0;

	cam_ctx->block_number = 0;
	cam_ctx->block_size = 0;
	cam_ctx->block_header = 0;
	cam_ctx->block_tail = 0;

	// Enable hardware
	camera_gpio_init();
	DPRINTK("gpio init\n");

	// capture interface init
	ci_init();

	// sensor init
	ret = cam_ctx->camera_functions->init(cam_ctx);
	DPRINTK("after cam_ctx->camera_functions->init\n");
	if (ret)
		goto camera_init_err;


	cam_ctx->dma_channels[0] = ci_dma_y;
	cam_ctx->dma_channels[1] = ci_dma_cb;
	cam_ctx->dma_channels[2] = ci_dma_cr;
	DRCMR68 = ci_dma_y | DRCMR_MAPVLD;
	DRCMR69 = ci_dma_cb | DRCMR_MAPVLD;
	DRCMR70 = ci_dma_cr | DRCMR_MAPVLD;


	// set capture format
	ret = camera_set_capture_format(cam_ctx);
	if (ret)
		goto camera_init_err;

	// set frame rate
	//camera_set_capture_frame_rate(cam_ctx);

	return 0;

      camera_init_err:
	camera_deinit(cam_ctx);
	return -1;
}

void camera_gpio_init()
{
#ifdef ADCM2650
	set_GPIO_mode(27 | GPIO_ALT_FN_3_IN);	/* CIF_DD[0] */
	set_GPIO_mode(114 | GPIO_ALT_FN_1_IN);	/* CIF_DD[1] */
	set_GPIO_mode(116 | GPIO_ALT_FN_1_IN);	/* CIF_DD[2] */
	set_GPIO_mode(115 | GPIO_ALT_FN_2_IN);	/* CIF_DD[3] */
	set_GPIO_mode(90 | GPIO_ALT_FN_3_IN);	/* CIF_DD[4] */
	set_GPIO_mode(91 | GPIO_ALT_FN_3_IN);	/* CIF_DD[5] */
	set_GPIO_mode(17 | GPIO_ALT_FN_2_IN);	/* CIF_DD[6] */
	set_GPIO_mode(12 | GPIO_ALT_FN_2_IN);	/* CIF_DD[7] */
	set_GPIO_mode(23 | GPIO_ALT_FN_1_OUT);	/* CIF_MCLK */
	set_GPIO_mode(26 | GPIO_ALT_FN_2_IN);	/* CIF_PCLK */
	set_GPIO_mode(25 | GPIO_ALT_FN_1_IN);	/* CIF_LV */
	set_GPIO_mode(24 | GPIO_ALT_FN_1_IN);	/* CIF_FV */
#endif

#ifdef OV9640
	set_GPIO_mode(27 | GPIO_ALT_FN_3_IN);	/* CIF_DD[0] */
	set_GPIO_mode(114 | GPIO_ALT_FN_1_IN);	/* CIF_DD[1] */
	set_GPIO_mode(51 | GPIO_ALT_FN_1_IN);	/* CIF_DD[2] */
	set_GPIO_mode(115 | GPIO_ALT_FN_2_IN);	/* CIF_DD[3] */
	set_GPIO_mode(95 | GPIO_ALT_FN_2_IN);	/* CIF_DD[4] */
	set_GPIO_mode(94 | GPIO_ALT_FN_2_IN);	/* CIF_DD[5] */
	set_GPIO_mode(17 | GPIO_ALT_FN_2_IN);	/* CIF_DD[6] */
	set_GPIO_mode(108 | GPIO_ALT_FN_1_IN);	/* CIF_DD[7] */
	set_GPIO_mode(23 | GPIO_ALT_FN_1_OUT);	/* CIF_MCLK */
	set_GPIO_mode(54 | GPIO_ALT_FN_3_IN);	/* CIF_PCLK */
	set_GPIO_mode(85 | GPIO_ALT_FN_3_IN);	/* CIF_LV */
	set_GPIO_mode(84 | GPIO_ALT_FN_3_IN);	/* CIF_FV */
	set_GPIO_mode(50 | GPIO_OUT);	/*CIF_PD */
	set_GPIO_mode(19 | GPIO_OUT);	/*CIF_RST */

#endif

#ifdef ADCM2700
	set_GPIO_mode(CIF_PD_MD);	/*CIF_PD */
	GPCR(CIF_PD) |= GPIO_bit(CIF_PD);	/*set to low */
	set_GPIO_mode(CIF_RST_MD);	/*CIF_RST */
	GPSR(CIF_RST) |= GPIO_bit(CIF_RST);	/*set to high */
	set_GPIO_mode(CIF_DD0_MD);	/* CIF_DD[0] */
	set_GPIO_mode(CIF_DD1_MD);	/* CIF_DD[1] */
	set_GPIO_mode(CIF_DD2_MD);	/* CIF_DD[2] */
	set_GPIO_mode(CIF_DD3_MD);	/* CIF_DD[3] */
	set_GPIO_mode(CIF_DD4_MD);	/* CIF_DD[4] */
	set_GPIO_mode(CIF_DD5_MD);	/* CIF_DD[5] */
	set_GPIO_mode(CIF_DD6_MD);	/* CIF_DD[6] */
	set_GPIO_mode(CIF_DD7_MD);	/* CIF_DD[7] */
	set_GPIO_mode(CIF_MCLK_MD);	/* CIF_MCLK  */
	set_GPIO_mode(CIF_PCLK_MD);	/* CIF_PCLK  */
	set_GPIO_mode(CIF_LV_MD);	/* CIF_LV    */
	set_GPIO_mode(CIF_FV_MD);	/* CIF_FV    */

#endif

	return;
}

int camera_deinit(p_camera_context_t cam_ctx)
{
	int ret = 0;

	ret = cam_ctx->camera_functions->deinit(cam_ctx);

	// capture interface deinit
	ci_deinit();
	return ret;
}

/***********************************************************************
 *
 * Capture APIs
 *
 ***********************************************************************/
// Set the image format

int camera_set_capture_format(p_camera_context_t cam_ctx)
{
	int ret;
	unsigned frame_size;
	CI_IMAGE_FORMAT ci_input_format, ci_output_format;
	CI_MP_TIMING timing;

	// set capture interface
	if (cam_ctx->capture_input_format > CAMERA_IMAGE_FORMAT_MAX ||
	    cam_ctx->capture_output_format > CAMERA_IMAGE_FORMAT_MAX)
		return STATUS_WRONG_PARAMETER;
	ci_input_format = FORMAT_MAPPINGS[cam_ctx->capture_input_format];
	ci_output_format = FORMAT_MAPPINGS[cam_ctx->capture_output_format];
	if (ci_input_format == CI_INVALID_FORMAT || ci_output_format == CI_INVALID_FORMAT)
		return STATUS_WRONG_PARAMETER;
	ci_set_image_format(ci_input_format, ci_output_format);

	// ring buffer init
	switch (cam_ctx->capture_output_format) {
	case CAMERA_IMAGE_FORMAT_RGB565:
		frame_size = cam_ctx->capture_width * cam_ctx->capture_height * 2;
		cam_ctx->fifo0_transfer_size = frame_size;
		cam_ctx->fifo1_transfer_size = 0;
		cam_ctx->fifo2_transfer_size = 0;
		break;
	case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
		frame_size = cam_ctx->capture_width * cam_ctx->capture_height * 2;
		cam_ctx->fifo0_transfer_size = frame_size;
		cam_ctx->fifo1_transfer_size = 0;
		cam_ctx->fifo2_transfer_size = 0;
		break;
	case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
		frame_size = cam_ctx->capture_width * cam_ctx->capture_height * 2;
		cam_ctx->fifo0_transfer_size = frame_size / 2;
		cam_ctx->fifo1_transfer_size = frame_size / 4;
		cam_ctx->fifo2_transfer_size = frame_size / 4;
		break;
// RGB666 support - JamesL
	case CAMERA_IMAGE_FORMAT_RGB666_PLANAR:
		frame_size = cam_ctx->capture_width * cam_ctx->capture_height * 4;
		cam_ctx->fifo0_transfer_size = frame_size;
		cam_ctx->fifo1_transfer_size = 0;
		cam_ctx->fifo2_transfer_size = 0;
		break;
	case CAMERA_IMAGE_FORMAT_RGB666_PACKED:
		frame_size = cam_ctx->capture_width * cam_ctx->capture_height * 3;
		cam_ctx->fifo0_transfer_size = frame_size;
		cam_ctx->fifo1_transfer_size = 0;
		cam_ctx->fifo2_transfer_size = 0;
		break;
// RGB888 support - JamesL
	case CAMERA_IMAGE_FORMAT_RGB888_PLANAR:
		frame_size = cam_ctx->capture_width * cam_ctx->capture_height * 4;
		cam_ctx->fifo0_transfer_size = frame_size;
		cam_ctx->fifo1_transfer_size = 0;
		cam_ctx->fifo2_transfer_size = 0;
		break;
//
	default:
		return STATUS_WRONG_PARAMETER;
		break;
	}
	cam_ctx->block_size = frame_size;

	cam_ctx->pages_per_fifo0 = (PAGE_ALIGN(cam_ctx->fifo0_transfer_size) / PAGE_SIZE);
	cam_ctx->pages_per_fifo1 = (PAGE_ALIGN(cam_ctx->fifo1_transfer_size) / PAGE_SIZE);
	cam_ctx->pages_per_fifo2 = (PAGE_ALIGN(cam_ctx->fifo2_transfer_size) / PAGE_SIZE);

	cam_ctx->pages_per_block = cam_ctx->pages_per_fifo0 + cam_ctx->pages_per_fifo1 + cam_ctx->pages_per_fifo2;

	cam_ctx->page_aligned_block_size = cam_ctx->pages_per_block * PAGE_SIZE;

	cam_ctx->block_number = cam_ctx->pages_allocated / cam_ctx->pages_per_block;
	cam_ctx->block_number_max = cam_ctx->pages_allocated / cam_ctx->pages_per_block;
	DPRINTK("in %s,cam_ctx->block_number =%d \n", __FUNCTION__, cam_ctx->block_number);

	if (cam_ctx->block_number > 3)
		cam_ctx->block_number = 3;

	if (cam_ctx->block_number > VIDEO_MAX_FRAME)
		cam_ctx->block_number = VIDEO_MAX_FRAME;

	//cam_ctx->block_header = cam_ctx->block_tail = 0;

	// generate dma descriptor chain
	ret = update_dma_chain(cam_ctx);
	if (ret)
		return -1;
	timing.BFW = timing.BLW = 0;
	ci_configure_mp(cam_ctx->capture_width - 1, cam_ctx->capture_height - 1, &timing);
	DPRINTK("before cam_ctx->camera_functions->set_capture_format \n");
	// set sensor setting
	ret = cam_ctx->camera_functions->set_capture_format(cam_ctx);
	if (ret)
		return ret;

	DPRINTK("after cam_ctx->camera_functions->set_capture_format \n");
	return 0;
}

// take a picture and copy it into the ring buffer
int camera_capture_still_image(p_camera_context_t cam_ctx, unsigned int block_id)
{
	int status;

	// init buffer status & capture
	cam_ctx->block_header = cam_ctx->block_tail = block_id;
	cam_ctx->capture_status = 0;
	status = start_capture(cam_ctx, block_id, 1);

	return status;
}

// capture motion video and copy it to the ring buffer
int camera_start_video_capture(p_camera_context_t cam_ctx, unsigned int block_id)
{
	int status;

	// init buffer status & capture
	cam_ctx->block_header = cam_ctx->block_tail = block_id;
	cam_ctx->capture_status = CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS;
	status = start_capture(cam_ctx, block_id, 0);

	return status;
}

// disable motion video image capture
void camera_stop_video_capture(p_camera_context_t cam_ctx)
{
	int status;

	// stop capture
	status = cam_ctx->camera_functions->stop_capture(cam_ctx);

	// stop dma
	stop_dma_transfer(cam_ctx);

	// update the flag
	if (!(cam_ctx->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
		cam_ctx->capture_status &= ~CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS;
	return;
}


/***********************************************************************
 *
 * Flow Control APIs
 *
 ***********************************************************************/
// continue capture image to next available buffer
void camera_continue_transfer(p_camera_context_t cam_ctx)
{
	// don't think we need this either.  JR
	// continue transfer on next block
	start_dma_transfer(cam_ctx, cam_ctx->block_tail);
}

// Return 1: there is available buffer, 0: buffer is full
int camera_next_buffer_available(p_camera_context_t cam_ctx)
{
	cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
	if (((cam_ctx->block_header + 1) % cam_ctx->block_number) != cam_ctx->block_tail) {
		return 1;
	}
	cam_ctx->capture_status |= CAMERA_STATUS_RING_BUFFER_FULL;

	return 0;
}

// Application supplies the FrameBufferID to the driver to tell it that the application has completed processing of 
// the given frame buffer, and that buffer is now available for re-use.
void camera_release_frame_buffer(p_camera_context_t cam_ctx, unsigned int frame_buffer_id)
{

	cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;

	// restart video capture only if video capture is in progress and space is available for image capture
	if ((cam_ctx->capture_status & CAMERA_STATUS_RING_BUFFER_FULL) &&
	    (cam_ctx->capture_status & CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS)) {
		if (((cam_ctx->block_header + 2) % cam_ctx->block_number) != cam_ctx->block_tail) {
			cam_ctx->capture_status &= ~CAMERA_STATUS_RING_BUFFER_FULL;
			start_capture(cam_ctx, cam_ctx->block_tail, 0);
		}
	}
}

// Returns the FrameBufferID for the first filled frame
// Note: -1 represents buffer empty
int camera_get_first_frame_buffer_id(p_camera_context_t cam_ctx)
{
	// not sure if this routine makes any sense.. JR

	// check whether buffer is empty
	if ((cam_ctx->block_header == cam_ctx->block_tail) &&
	    !(cam_ctx->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
		return -1;

	// return the block header
	return cam_ctx->block_header;
}

// Returns the FrameBufferID for the last filled frame, this would be used if we were polling for image completion data, 
// or we wanted to make sure there were no frames waiting for us to process.
// Note: -1 represents buffer empty
int camera_get_last_frame_buffer_id(p_camera_context_t cam_ctx)
{
	int ret;

	// check whether buffer is empty
	if ((cam_ctx->block_header == cam_ctx->block_tail) &&
	    !(cam_ctx->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
		return -1;

	// return the block before the block_tail
	ret = (cam_ctx->block_tail + cam_ctx->block_number - 1) % cam_ctx->block_number;
	return ret;
}

/***********************************************************************
 *
 * Buffer Info APIs
 *
 ***********************************************************************/
// Return: the number of frame buffers allocated for use.
unsigned int camera_get_num_frame_buffers(p_camera_context_t cam_ctx)
{
	return cam_ctx->block_number;
}

// FrameBufferID is a number between 0 and N-1, where N is the total number of frame buffers in use.  Returns the address of
// the given frame buffer.  The application will call this once for each frame buffer at application initialization only.
void *camera_get_frame_buffer_addr(p_camera_context_t cam_ctx, unsigned int frame_buffer_id)
{
	return (void *) ((unsigned) cam_ctx->buffer_virtual + cam_ctx->page_aligned_block_size * frame_buffer_id);
}

// Return the block id
int camera_get_frame_buffer_id(p_camera_context_t cam_ctx, void *address)
{
	if (((unsigned) address >=
	     (unsigned) cam_ctx->buffer_virtual) &&
	    ((unsigned) address <= (unsigned) cam_ctx->buffer_virtual + cam_ctx->buf_size)) {
		return ((unsigned) address - (unsigned) cam_ctx->buffer_virtual) / cam_ctx->page_aligned_block_size;
	}
	return -1;
}


/***********************************************************************
 *
 * Frame rate APIs
 *
 ***********************************************************************/
// Set desired frame rate
void camera_set_capture_frame_rate(p_camera_context_t cam_ctx)
{
	ci_set_frame_rate(cam_ctx->frame_rate);
	return;
}

// return current setting
void camera_get_capture_frame_rate(p_camera_context_t cam_ctx)
{
	cam_ctx->frame_rate = ci_get_frame_rate();
	return;
}


/***********************************************************************
 *
 * Interrupt APIs
 *
 ***********************************************************************/
// set interrupt mask 
void camera_set_int_mask(p_camera_context_t cam_ctx, unsigned int mask)
{
	pxa_dma_desc *end_des_virtual;
	int dma_interrupt_on;
	unsigned int i;

	// set CI interrupt
	ci_set_int_mask(mask & CI_CICR0_INTERRUPT_MASK);

	// set dma end interrupt
	if (mask & CAMERA_INTMASK_END_OF_DMA)
		dma_interrupt_on = 1;
	else
		dma_interrupt_on = 0;

	// set fifo0 dma chains' flag
	end_des_virtual = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual + cam_ctx->fifo0_num_descriptors - 1;
	for (i = 0; i < cam_ctx->block_number; i++) {
		if (dma_interrupt_on)
			end_des_virtual->dcmd |= DCMD_ENDIRQEN;
		else
			end_des_virtual->dcmd &= ~DCMD_ENDIRQEN;
		end_des_virtual += cam_ctx->fifo0_num_descriptors;
	}
}

// get interrupt mask 
unsigned int camera_get_int_mask(p_camera_context_t cam_ctx)
{
	pxa_dma_desc *end_des_virtual;
	unsigned int ret;

	// get CI mask
	ret = ci_get_int_mask();

	// get dma end mask
	end_des_virtual = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual + cam_ctx->fifo0_num_descriptors - 1;
	if (end_des_virtual->dcmd & DCMD_ENDIRQEN)
		ret |= CAMERA_INTMASK_END_OF_DMA;

	return ret;
}

// clear interrupt status
void camera_clear_int_status(p_camera_context_t cam_ctx, unsigned int status)
{
	ci_clear_int_status((status & 0xFFFF));
}

/***********************************************************************************
* Application interface 							   *
***********************************************************************************/
static int pxa_camera_open(struct video_device *dev, int flags)
{
	int status = -1;
	camera_context_t *cam_ctx;

	init_waitqueue_head(&camera_wait_q);
	if (pxa_camera_mem_init()) {
		DPRINTK("DMA memory allocate failed!");
		return -1;
	}
	DPRINTK("in %s, after pxa_camera_mem_init \n", __FUNCTION__);
	cam_ctx = g_camera_context;
	if (atomic_read(&cam_ctx->refcount))
		return -EBUSY;
	atomic_inc(&cam_ctx->refcount);


#ifdef ADCM2650
	cam_ctx->sensor_type = CAMERA_TYPE_ADCM_2650;
#endif
#ifdef ADCM2700
	cam_ctx->sensor_type = CAMERA_TYPE_ADCM_2700;
#endif
#ifdef OV9640
	cam_ctx->sensor_type = CAMERA_TYPE_OMNIVISION_9640;
#endif
	cam_ctx->capture_width = WIDTH_DEFT;
	cam_ctx->capture_height = HEIGHT_DEFT;
	cam_ctx->capture_input_format = CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR;
	cam_ctx->capture_output_format = CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR;
	cam_ctx->frame_rate = FRAMERATE_DEFT;

	// init function dispatch table
#ifdef ADCM2650
	adcm2650_func.init = camera_func_adcm2650_init;
	adcm2650_func.deinit = camera_func_adcm2650_deinit;
	adcm2650_func.set_capture_format = camera_func_adcm2650_set_capture_format;
	adcm2650_func.start_capture = camera_func_adcm2650_start_capture;
	adcm2650_func.stop_capture = camera_func_adcm2650_stop_capture;
	cam_ctx->camera_functions = &adcm2650_func;
#endif

#ifdef ADCM2700
	adcm2700_func.init = camera_func_adcm2700_init;
	adcm2700_func.deinit = camera_func_adcm2700_deinit;
	adcm2700_func.set_capture_format = camera_func_adcm2700_set_capture_format;
	adcm2700_func.start_capture = camera_func_adcm2700_start_capture;
	adcm2700_func.stop_capture = camera_func_adcm2700_stop_capture;
	cam_ctx->camera_functions = &adcm2700_func;
#endif

#ifdef OV9640
	ov9640_func.init = camera_func_ov9640_init;
	ov9640_func.deinit = camera_func_ov9640_deinit;
	ov9640_func.set_capture_format = camera_func_ov9640_set_capture_format;
	ov9640_func.start_capture = camera_func_ov9640_start_capture;
	ov9640_func.stop_capture = camera_func_ov9640_stop_capture;
	ov9640_func.command = camera_func_ov9640_command;
	cam_ctx->camera_functions = &ov9640_func;
#endif

	cam_ctx->ost_reg_base = 0;
	cam_ctx->gpio_reg_base = 0;
	cam_ctx->ci_reg_base = 0;
	cam_ctx->board_reg_base = 0;
	still_image_rdy = 0;

	/* FIXME: handle camera_init() errors ? */
	status = camera_init(cam_ctx);
	DPRINTK(KERN_DEBUG PREFIX "camera opened\n");
	status = 0;
	return status;
}

static void pxa_camera_close(struct video_device *dev)
{
	camera_context_t *cam_ctx = g_camera_context;

	DPRINTK(KERN_DEBUG PREFIX "camera closed\n");
	atomic_dec(&cam_ctx->refcount);
	camera_deinit(cam_ctx);
	pxa_camera_mem_deinit();
}

#define PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, size) \
do { \
	unsigned int len; \
	unsigned int remain_size = size; \
	while (remain_size > 0) { \
		if (remain_size > PAGE_SIZE) \
			len = PAGE_SIZE; \
		else \
			len = remain_size; \
		if (copy_to_user(buf, page_address(*p_page), len)) \
			return -EFAULT; \
		remain_size -= len; \
		buf += len; \
		p_page++; \
	} \
} while (0);


static long pxa_camera_read(struct video_device *dev, char *buf, unsigned long count, int noblock)
{
	struct page **p_page;

	camera_context_t *cam_ctx = g_camera_context;

	if (still_image_mode == 1) {
		while (still_image_rdy != 1)
			mdelay(1);
		/*
		   if (cam_ctx->capture_width ==1280 && cam_ctx->capture_height==960)
		   cam_ctx->block_tail = cam_ctx->block_header = 0;
		   else if (cam_ctx->capture_width==960 && cam_ctx->capture_height==480)
		   cam_ctx->block_tail = cam_ctx->block_header = 2;
		   DPRINTK("cam_ctx->block_number =%d \n",cam_ctx->block_number);
		   cam_ctx->camera_functions->stop_capture(cam_ctx);
		 */
		p_page = &cam_ctx->page_array[cam_ctx->block_tail * cam_ctx->pages_per_block];

		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo0_transfer_size);
		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo1_transfer_size);
		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo2_transfer_size);

		still_image_rdy = 0;
		return cam_ctx->block_size;
	}
	if (still_image_mode == 0) {
		if (first_video_frame == 1)
			cam_ctx->block_tail = cam_ctx->block_header;
		else
			cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;
	}

	first_video_frame = 0;

	if (cam_ctx->block_header == cam_ctx->block_tail) {
		if (still_image_mode == 0) {
			task_waiting = 1;
			interruptible_sleep_on(&camera_wait_q);
		} else {
			while (still_image_rdy != 1) {
				DPRINTK("wait still_image_rdy =1 \n");
				mdelay(1);
			}
		}
	}

	p_page = &cam_ctx->page_array[cam_ctx->block_tail * cam_ctx->pages_per_block];

	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo0_transfer_size);
	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo1_transfer_size);
	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo2_transfer_size);

	return cam_ctx->block_size;
}

int camera_sleep()
{
	interruptible_sleep_on(&camera_wait_q);
}

struct reg_set_s {
	int val1;
	int val2;
};

static int pxa_camera_ioctl(struct video_device *dev, unsigned int cmd, void *param)
{
	int retval = 0;
	camera_context_t *cam_ctx = g_camera_context;

	switch (cmd) {
/* V4L Standard IOCTL. */
	case VIDIOCGCAP:
	{
		struct video_capability vc;
		strcpy(vc.name, "Bulverde Camera");
		vc.maxwidth = MAX_WIDTH;
		vc.maxheight = MAX_HEIGHT;
#ifdef OV9640
		vc.maxwidth = 1280;
		vc.maxheight = 960;
#endif
		vc.minwidth = MIN_WIDTH;
		vc.minheight = MIN_HEIGHT;
		if (copy_to_user(param, &vc, sizeof(struct video_capability)))
			return -EFAULT;
		break;
	}


	case VIDIOCSPICT:
	{
		struct video_picture vp;
		if (copy_from_user(&vp, param, sizeof(vp))) {
			retval = -EFAULT;
			break;
		}
		cam_ctx->capture_output_format = vp.palette;
		retval = camera_set_capture_format(cam_ctx);
		break;
	}
	case VIDIOCGPICT:
	{
		struct video_picture vp;
		vp.palette = cam_ctx->capture_output_format;
		if (copy_to_user(param, &vp, sizeof(struct video_picture)))
			retval = -EFAULT;
		break;
	}

	case VIDIOCCAPTURE:
	{
		int capture_flag;

		capture_flag = (int) param;
/* Still Image Capture */
		if (capture_flag == STILL_IMAGE) {
			camera_set_int_mask(cam_ctx, 0x3ff | 0x0400);
			still_image_mode = 1;
			task_waiting = 0;
			camera_capture_still_image(cam_ctx, 0);
			cam_ctx->block_header = 0;
			cam_ctx->block_tail = 0;
			break;
		}
/* Video Capture Start */
		else if (capture_flag == VIDEO_START) {
			camera_set_int_mask(cam_ctx, 0x3ff | 0x0400);
			cam_ctx->block_header = 0;
			cam_ctx->block_tail = 0;
			still_image_mode = 0;
			first_video_frame = 1;
			camera_start_video_capture(cam_ctx, 0);
			break;
		}
/* Video Capture Stop */
		else if (capture_flag == VIDEO_STOP) {
			camera_set_int_mask(cam_ctx, 0x3ff);
			camera_stop_video_capture(cam_ctx);
			break;
		} else {
			retval = -EFAULT;
			break;
		}
	}

/* mmap interface */
	case VIDIOCGMBUF:
	{
		struct video_mbuf vm;
		int i;

		memset(&vm, 0, sizeof(vm));
		vm.size = cam_ctx->buf_size;
		vm.frames = cam_ctx->block_number;
		for (i = 0; i < vm.frames; i++)
			vm.offsets[i] = cam_ctx->page_aligned_block_size * i;

		if (copy_to_user((void *) param, (void *) &vm, sizeof(vm)))
			retval = -EFAULT;
		break;
	}

/* Application extended IOCTL.  */
/* Register access interface	*/
	case WCAM_VIDIOCSINFOR:
	{
		struct reg_set_s reg_s;
		if (copy_from_user(&reg_s, param, sizeof(int) * 2)) {
			retval = -EFAULT;
			break;
		}
		cam_ctx->capture_input_format = reg_s.val1;
		cam_ctx->capture_output_format = reg_s.val2;

		retval = camera_set_capture_format(cam_ctx);
		DPRINTK("WCAM_VIDIOCSINFOR retval=%d\n", retval);
		break;
	}

	case WCAM_VIDIOCGINFOR:
	{
		struct reg_set_s reg_s;
		reg_s.val1 = cam_ctx->capture_input_format;
		reg_s.val2 = cam_ctx->capture_output_format;
		if (copy_to_user(param, &reg_s, sizeof(int) * 2))
			retval = -EFAULT;
		break;
	}

	case WCAM_VIDIOCGCIREG:
	{
		struct reg_set_s reg_s;
		if (copy_from_user(&reg_s, param, sizeof(int) * 2)) {
			retval = -EFAULT;
			break;
		}

		reg_s.val2 = ci_get_reg_value(reg_s.val1);
		if (copy_to_user(param, &reg_s, sizeof(int) * 2))
			retval = -EFAULT;
		break;
	}

	case WCAM_VIDIOCSCIREG:
	{
		struct reg_set_s reg_s;
		if (copy_from_user(&reg_s, param, sizeof(int) * 2)) {
			retval = -EFAULT;
			break;
		}
		ci_set_reg_value(reg_s.val1, reg_s.val2);
		break;
	}

	case WCAM_VIDIOCGCAMREG:
	{
		struct reg_set_s reg_s;
		if (copy_from_user(&reg_s, param, sizeof(int) * 2)) {
			retval = -EFAULT;
			break;
		}
		sensor_pipeline_read((u16) reg_s.val1, (u16 *) & reg_s.val2);
		DPRINTK("WCAM_VIDIOCGCAMREG reg.val1=0x%x,reg.val2=0x%x\n", reg_s.val1, reg_s.val2);
		if (copy_to_user(param, &reg_s, sizeof(int) * 2))
			retval = -EFAULT;
		ci_dump();
		break;
	}

	case WCAM_VIDIOCSCAMREG:
	{
		struct reg_set_s reg_s;
		if (copy_from_user(&reg_s, param, sizeof(int) * 2)) {
			retval = -EFAULT;
			break;
		}
		sensor_pipeline_write((u16) reg_s.val1, (u16) reg_s.val2);
		break;
	}
		/*set frame buffer count */
	case WCAM_VIDIOCSBUFCOUNT:
	{
		int count;
		if (copy_from_user(&count, param, sizeof(int))) {
			return -EFAULT;
		}
		if (cam_ctx->block_number_max == 0) {
			dbg_print("windows size or format not setting!!");
			return -EFAULT;
		}

		if (count >= FRAMES_IN_BUFFER && count <= cam_ctx->block_number_max) {
			cam_ctx->block_number = count;
			cam_ctx->block_header = cam_ctx->block_tail = 0;
			// generate dma descriptor chain
			update_dma_chain(cam_ctx);
		}

		count = cam_ctx->block_number;

		if (copy_to_user(param, &count, sizeof(int))) {
			return -EFAULT;
		}

	}
		break;
		/*get cur avaliable frames */
	case WCAM_VIDIOCGCURFRMS:
	{
		struct {
			int first, last;
		} pos;
		pos.first = cam_ctx->block_tail;
		pos.last = cam_ctx->block_header;

		if (copy_to_user(param, &pos, sizeof(pos))) {
			return -EFAULT;
		}
	}

		break;

	default:
	{
		int ret;
		DPRINTK("in _ioctl default command,param=%d!!!!!!!!!!!!!!!!!!!!\n", param);
		ret = cam_ctx->camera_functions->command(cam_ctx, cmd, param);
		DPRINTK("cam_ctx->camera_functions->command ret=%d\n", ret);
		retval = ret;
		if (ret) {
			DPRINTK(KERN_WARNING PREFIX "invalid ioctl %x\n", cmd);
			retval = -ENOIOCTLCMD;
		}
		break;
	}
	}
	return retval;
}

static int pxa_camera_mmap(struct video_device *dev, const char *adr, unsigned long size)
{
	unsigned long start = (unsigned long) adr;
	camera_context_t *cam_ctx = g_camera_context;
	struct page **p_page = cam_ctx->page_array;

	size = PAGE_ALIGN(size);
	while (size > 0) {
		if (remap_page_range(start, page_to_phys(*p_page), PAGE_SIZE, PAGE_SHARED)) {
			return -EFAULT;
		}
		start += PAGE_SIZE;
		p_page++;
		size -= PAGE_SIZE;
	}
	return 0;
}

unsigned int pxa_camera_poll(struct video_device *dev, struct file *file, poll_table * wait)
{
	camera_context_t *cam_ctx = g_camera_context;
	static int waited = 0;

	poll_wait(file, &camera_wait_q, wait);

	if (still_image_mode == 1)
		if (still_image_rdy == 1) {
			still_image_rdy = 0;
			write_balance();
			return POLLIN | POLLRDNORM;
		}
	//else
	//    return 0;
	if (first_video_frame == 1)
		first_video_frame = 0;
	else if (still_image_mode == 0 && waited != 1)
		cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;

	if (cam_ctx->block_header == cam_ctx->block_tail) {
		DPRINTK("enter poll waiting, tail = %d, header = %d \n", cam_ctx->block_tail, cam_ctx->block_header);
		task_waiting = 1;
		waited = 1;
		//interruptible_sleep_on(&camera_wait_q);
		return 0;
	} else
		waited = 0;

	write_balance();
	return POLLIN | POLLRDNORM;
}


int pxa_camera_video_init(struct video_device *vdev)
{
	DPRINTK(KERN_DEBUG PREFIX "camera initialized\n");
	return 0;
}


static struct video_device vd = {
	owner:THIS_MODULE,
	name:"PXA Camera",
	type:VID_TYPE_CAPTURE,
	hardware:VID_HARDWARE_PXA_CAMERA,	/* FIXME */
	open:pxa_camera_open,
	close:pxa_camera_close,
	read:pxa_camera_read,
	poll:pxa_camera_poll,
	ioctl:pxa_camera_ioctl,
	mmap:pxa_camera_mmap,
	initialize:pxa_camera_video_init,
	minor:-1,
};

int pxa_camera_mem_deinit()
{
	camera_context_t *cam_ctx = g_camera_context;
	if (ci_regs_base)
		iounmap((unsigned long *) ci_regs_base);
	if (g_camera_context) {
		if (cam_ctx->dma_descriptors_virtual) {
			consistent_free(cam_ctx->dma_descriptors_virtual,
					MAX_DESC_NUM * sizeof(pxa_dma_desc), (int) cam_ctx->dma_descriptors_physical);
			g_camera_context->dma_descriptors_virtual = NULL;
		}

		if (cam_ctx->buffer_virtual) {
			pxa_dma_buffer_free(cam_ctx);
			g_camera_context->buffer_virtual = NULL;

		}

		kfree(g_camera_context);
		g_camera_context = NULL;
	}
	return 0;
}

int pxa_camera_mem_init()
{
	p_camera_context_t cam_ctx;
	(unsigned long *) ci_regs_base = (unsigned long *) ioremap(CI_REGS_PHYS, CI_REG_SIZE);
	if (!ci_regs_base) {
		DPRINTK(KERN_ERR PREFIX "can't remap I/O registers at %x\n", CI_REGS_PHYS);
		return -1;
	}

	cam_ctx = (camera_context_t *)
	    kmalloc(sizeof(struct camera_context_s), GFP_KERNEL);
	if (cam_ctx == NULL) {
		DPRINTK(KERN_WARNING PREFIX "can't allocate buffer for camera control structure\n");
		return -1;
	}
	g_camera_context = cam_ctx;
	memset(g_camera_context, 0, sizeof(struct camera_context_s));
	atomic_set(&cam_ctx->refcount, 0);
	cam_ctx->dma_started = 0;

	cam_ctx->dma_descriptors_virtual =
	    consistent_alloc(GFP_KERNEL, MAX_DESC_NUM * sizeof(pxa_dma_desc),
			     (void *) &cam_ctx->dma_descriptors_physical);
	if (cam_ctx->dma_descriptors_virtual == NULL) {
		DPRINTK(KERN_WARNING PREFIX
		       "memory allocation for DMA descriptors (%ld bytes) failed\n",
		       MAX_DESC_NUM * sizeof(pxa_dma_desc));
		goto err_mem;
	}

	cam_ctx->buf_size = BUF_SIZE_DEFT;

	if (pxa_dma_buffer_init(cam_ctx) != 0) {
		DPRINTK(KERN_WARNING PREFIX
		       "memory allocation for capture buffer (%d bytes) failed\n", cam_ctx->buf_size);
		goto err_mem;
	}

	cam_ctx->dma_descriptors_size = MAX_DESC_NUM;
	return 0;
      err_mem:
	pxa_camera_mem_deinit();
	return -ENXIO;
}

static int __init pxa_camera_init(void)
{
	camera_context_t *cam_ctx = NULL;
	int err;

	/* 1. mapping CI registers, so that we can access the CI */
	if ((err = request_irq(IRQ_CAMERA, pxa_camera_irq, 0, "PXA Camera", &vd))) {
		DPRINTK(KERN_WARNING PREFIX "camera interrupt register failed, error %d\n", err);
		return -ENXIO;
	}
	ci_dma_y = pxa_request_dma("CI_Y", DMA_PRIO_HIGH, pxa_ci_dma_irq_y, &vd);
	if (ci_dma_y < 0) {
		DPRINTK(KERN_WARNING PREFIX "can't request DMA for Y\n");
		goto err_init;
	}
	ci_dma_cb = pxa_request_dma("CI_Cb", DMA_PRIO_HIGH, pxa_ci_dma_irq_cb, &vd);
	if (ci_dma_cb < 0) {
		DPRINTK(KERN_WARNING PREFIX "can't request DMA for Cb\n");
		goto err_init;
	}
	ci_dma_cr = pxa_request_dma("CI_Cr", DMA_PRIO_HIGH, pxa_ci_dma_irq_cr, &vd);
	if (ci_dma_cr < 0) {
		DPRINTK(KERN_WARNING PREFIX "can't request DMA for Cr\n");
		goto err_init;
	}

	DRCMR68 = ci_dma_y | DRCMR_MAPVLD;
	DRCMR69 = ci_dma_cb | DRCMR_MAPVLD;
	DRCMR70 = ci_dma_cr | DRCMR_MAPVLD;


	if (video_register_device(&vd, VFL_TYPE_GRABBER, minor) < 0) {
		DPRINTK(KERN_WARNING PREFIX "can't register video device\n");
		goto err_init;
	}
#ifdef CONFIG_DPM
	pxa_camera_ldm_register();
#endif
#ifdef CONFIG_PM
	pm_dev = pm_register(PM_SYS_DEV, 0, camera_pm_callback);
#endif


	DPRINTK(KERN_NOTICE PREFIX "video device registered, use device /dev/video%d \n", minor);
	return 0;

      err_init:
	free_irq(IRQ_CAMERA, &vd);

	if (ci_dma_y >= 0)
		pxa_free_dma(ci_dma_y);

	if (ci_dma_cb >= 0)
		pxa_free_dma(ci_dma_cb);

	if (ci_dma_cr >= 0)
		pxa_free_dma(ci_dma_cr);

	DRCMR68 = 0;
	DRCMR69 = 0;
	DRCMR70 = 0;

	return 0;
}

static void __exit pxa_camera_exit(void)
{
	camera_context_t *cam_ctx = g_camera_context;
#ifdef CONFIG_DPM
	pxa_camera_ldm_unregister();
#endif
#ifdef CONFIG_PM
	pm_unregister(pm_dev);
#endif

	video_unregister_device(&vd);

	free_irq(IRQ_CAMERA, &vd);

	if (ci_dma_y >= 0)
		pxa_free_dma(ci_dma_y);

	if (ci_dma_cb >= 0)
		pxa_free_dma(ci_dma_cb);

	if (ci_dma_cr >= 0)
		pxa_free_dma(ci_dma_cr);

	DRCMR68 = 0;
	DRCMR69 = 0;
	DRCMR70 = 0;

	pxa_camera_mem_deinit();

}

//-------------------------------------------------------------------------------------------------------
//      Configuration APIs
//-------------------------------------------------------------------------------------------------------
void ci_set_frame_rate(CI_FRAME_CAPTURE_RATE frate)
{
	unsigned int value;

	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_FR_RATE_SMASK << CI_CICR4_FR_RATE_SHIFT);
	value |= (unsigned) frate << CI_CICR4_FR_RATE_SHIFT;
	CICR4 = value;
}

CI_FRAME_CAPTURE_RATE ci_get_frame_rate(void)
{
	unsigned int value;
	value = CICR4;
	return (CI_FRAME_CAPTURE_RATE) ((value >> CI_CICR4_FR_RATE_SHIFT) & CI_CICR4_FR_RATE_SMASK);
}

void ci_set_image_format(CI_IMAGE_FORMAT input_format, CI_IMAGE_FORMAT output_format)
{
	unsigned int value, tbit, rgbt_conv, rgb_conv, rgb_f, ycbcr_f, rgb_bpp, raw_bpp, cspace;

	// write cicr1: preserve ppl value and data width value
	value = CICR1;
	value &= ((CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT) | ((CI_CICR1_DW_SMASK) << CI_CICR1_DW_SHIFT));
	tbit = rgbt_conv = rgb_conv = rgb_f = ycbcr_f = rgb_bpp = raw_bpp = cspace = 0;
	switch (input_format) {
	case CI_RAW8:
		cspace = 0;
		raw_bpp = 0;
		break;
	case CI_RAW9:
		cspace = 0;
		raw_bpp = 1;
		break;
	case CI_RAW10:
		cspace = 0;
		raw_bpp = 2;
		break;
	case CI_YCBCR422:
	case CI_YCBCR422_PLANAR:
		cspace = 2;
		if (output_format == CI_YCBCR422_PLANAR) {
			ycbcr_f = 1;
		}
		break;
	case CI_RGB444:
		cspace = 1;
		rgb_bpp = 0;
		break;
	case CI_RGB555:
		cspace = 1;
		rgb_bpp = 1;
		if (output_format == CI_RGBT555_0) {
			rgbt_conv = 2;
			tbit = 0;
		} else if (output_format == CI_RGBT555_1) {
			rgbt_conv = 2;
			tbit = 1;
		}
		break;
	case CI_RGB565:
		cspace = 1;
		rgb_bpp = 2;
		rgb_f = 1;
		break;
	case CI_RGB666:
		cspace = 1;
		rgb_bpp = 3;
		if (output_format == CI_RGB666_PACKED) {
			rgb_f = 1;
		}
		break;
	case CI_RGB888:
	case CI_RGB888_PACKED:
		cspace = 1;
		rgb_bpp = 4;
		switch (output_format) {
		case CI_RGB888_PACKED:
			rgb_f = 1;
			break;
		case CI_RGBT888_0:
			rgbt_conv = 1;
			tbit = 0;
			break;
		case CI_RGBT888_1:
			rgbt_conv = 1;
			tbit = 1;
			break;
		case CI_RGB666:
			rgb_conv = 1;
			break;
// RGB666 PACKED - JamesL
		case CI_RGB666_PACKED:
			rgb_conv = 1;
			rgb_f = 1;
			break;
// end
		case CI_RGB565:
			rgb_conv = 2;
			break;
		case CI_RGB555:
			rgb_conv = 3;
			break;
		case CI_RGB444:
			rgb_conv = 4;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	value |= (tbit == 1) ? CI_CICR1_TBIT : 0;
	value |= rgbt_conv << CI_CICR1_RGBT_CONV_SHIFT;
	value |= rgb_conv << CI_CICR1_RGB_CONV_SHIFT;
	value |= (rgb_f == 1) ? CI_CICR1_RBG_F : 0;
	value |= (ycbcr_f == 1) ? CI_CICR1_YCBCR_F : 0;
	value |= rgb_bpp << CI_CICR1_RGB_BPP_SHIFT;
	value |= raw_bpp << CI_CICR1_RAW_BPP_SHIFT;
	value |= cspace << CI_CICR1_COLOR_SP_SHIFT;
	CICR1 = value;

	return;
}

void ci_set_mode(CI_MODE mode, CI_DATA_WIDTH data_width)
{
	unsigned int value;

	// write mode field in cicr0
	value = CICR0;
	value &= ~(CI_CICR0_SIM_SMASK << CI_CICR0_SIM_SHIFT);
	value |= (unsigned int) mode << CI_CICR0_SIM_SHIFT;
	CICR0 = value;

	// write data width cicr1
	value = CICR1;
	value &= ~(CI_CICR1_DW_SMASK << CI_CICR1_DW_SHIFT);
	value |= ((unsigned) data_width) << CI_CICR1_DW_SHIFT;
	CICR1 = value;
	return;
}

void ci_configure_mp(unsigned int ppl, unsigned int lpf, CI_MP_TIMING * timing)
{
	unsigned int value;

	// write ppl field in cicr1
	value = CICR1;
	value &= ~(CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT);
	value |= (ppl & CI_CICR1_PPL_SMASK) << CI_CICR1_PPL_SHIFT;
	CICR1 = value;

	// write BLW, ELW in cicr2  
	value = CICR2;
	value &= ~(CI_CICR2_BLW_SMASK << CI_CICR2_BLW_SHIFT | CI_CICR2_ELW_SMASK << CI_CICR2_ELW_SHIFT);
	value |= (timing->BLW & CI_CICR2_BLW_SMASK) << CI_CICR2_BLW_SHIFT;
	CICR2 = value;

	// write BFW, LPF in cicr3
	value = CICR3;
	value &= ~(CI_CICR3_BFW_SMASK << CI_CICR3_BFW_SHIFT | CI_CICR3_LPF_SMASK << CI_CICR3_LPF_SHIFT);
	value |= (timing->BFW & CI_CICR3_BFW_SMASK) << CI_CICR3_BFW_SHIFT;
	value |= (lpf & CI_CICR3_LPF_SMASK) << CI_CICR3_LPF_SHIFT;
	CICR3 = value;
	return;
}

void ci_configure_sp(unsigned int ppl, unsigned int lpf, CI_SP_TIMING * timing)
{
	unsigned int value;

	// write ppl field in cicr1
	value = CICR1;
	value &= ~(CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT);
	value |= (ppl & CI_CICR1_PPL_SMASK) << CI_CICR1_PPL_SHIFT;
	CICR1 = value;

	// write cicr2
	value = CICR2;
	value |= (timing->BLW & CI_CICR2_BLW_SMASK) << CI_CICR2_BLW_SHIFT;
	value |= (timing->ELW & CI_CICR2_ELW_SMASK) << CI_CICR2_ELW_SHIFT;
	value |= (timing->HSW & CI_CICR2_HSW_SMASK) << CI_CICR2_HSW_SHIFT;
	value |= (timing->BFPW & CI_CICR2_BFPW_SMASK) << CI_CICR2_BFPW_SHIFT;
	value |= (timing->FSW & CI_CICR2_FSW_SMASK) << CI_CICR2_FSW_SHIFT;
	CICR2 = value;

	// write cicr3
	value = CICR3;
	value |= (timing->BFW & CI_CICR3_BFW_SMASK) << CI_CICR3_BFW_SHIFT;
	value |= (timing->EFW & CI_CICR3_EFW_SMASK) << CI_CICR3_EFW_SHIFT;
	value |= (timing->VSW & CI_CICR3_VSW_SMASK) << CI_CICR3_VSW_SHIFT;
	value |= (lpf & CI_CICR3_LPF_SMASK) << CI_CICR3_LPF_SHIFT;
	CICR3 = value;
	return;
}

void ci_configure_ms(unsigned int ppl, unsigned int lpf, CI_MS_TIMING * timing)
{
	// the operation is same as Master-Parallel
	ci_configure_mp(ppl, lpf, (CI_MP_TIMING *) timing);
}

void ci_configure_ep(int parity_check)
{
	unsigned int value;

	// write parity_enable field in cicr0   
	value = CICR0;
	if (parity_check) {
		value |= CI_CICR0_PAR_EN;
	} else {
		value &= ~CI_CICR0_PAR_EN;
	}
	CICR0 = value;
	return;
}

void ci_configure_es(int parity_check)
{
	// the operationi is same as Embedded-Parallel
	ci_configure_ep(parity_check);
}

void ci_set_clock(unsigned int clk_regs_base, int pclk_enable, int mclk_enable, unsigned int mclk_khz)
{
	unsigned int ciclk = 0, value, div, cccr_l, K;

	// determine the LCLK frequency programmed into the CCCR.
	cccr_l = (CCCR & 0x0000001F);

	if (cccr_l < 8)		// L = [2 - 7]
		ciclk = (13 * cccr_l) * 100;
	else if (cccr_l < 17)	// L = [8 - 16]
		ciclk = ((13 * cccr_l) * 100) >> 1;
	else if (cccr_l < 32)	// L = [17 - 31]
		ciclk = ((13 * cccr_l) * 100) >> 2;
	DPRINTK("the mclk_khz = %d \n", mclk_khz);

	// want a divisor that gives us a clock rate as close to, but not more than the given mclk.
	div = (ciclk + mclk_khz) / (2 * mclk_khz) - 1;

	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_PCLK_EN | CI_CICR4_MCLK_EN | CI_CICR4_DIV_SMASK << CI_CICR4_DIV_SHIFT);
	value |= (pclk_enable) ? CI_CICR4_PCLK_EN : 0;
	value |= (mclk_enable) ? CI_CICR4_MCLK_EN : 0;
	value |= div << CI_CICR4_DIV_SHIFT;
	CICR4 = value;
	return;
}


void ci_set_polarity(int pclk_sample_falling, int hsync_active_low, int vsync_active_low)
{
	unsigned int value;

	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_PCP | CI_CICR4_HSP | CI_CICR4_VSP);
	value |= (pclk_sample_falling) ? CI_CICR4_PCP : 0;
	value |= (hsync_active_low) ? CI_CICR4_HSP : 0;
	value |= (vsync_active_low) ? CI_CICR4_VSP : 0;
	CICR4 = value;
	return;
}

void ci_set_fifo(unsigned int timeout, CI_FIFO_THRESHOLD threshold, int fifo1_enable, int fifo2_enable)
{
	unsigned int value;

	// write citor
	CITOR = timeout;

	// write cifr: always enable fifo 0! also reset input fifo 
	value = CIFR;
	value &= ~(CI_CIFR_FEN0 | CI_CIFR_FEN1 | CI_CIFR_FEN2 | CI_CIFR_RESETF |
		   CI_CIFR_THL_0_SMASK << CI_CIFR_THL_0_SHIFT);
	value |= (unsigned int) threshold << CI_CIFR_THL_0_SHIFT;
	value |= (fifo1_enable) ? CI_CIFR_FEN1 : 0;
	value |= (fifo2_enable) ? CI_CIFR_FEN2 : 0;
	value |= CI_CIFR_RESETF | CI_CIFR_FEN0;
	CIFR = value;
	return;
}

void ci_reset_fifo()
{
	unsigned int value;
	value = CIFR;
	value |= CI_CIFR_RESETF;
	CIFR = value;
}

void ci_set_int_mask(unsigned int mask)
{
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value &= ~CI_CICR0_INTERRUPT_MASK;
	value |= (mask & CI_CICR0_INTERRUPT_MASK);
	CICR0 = value;
	return;
}

unsigned int ci_get_int_mask()
{
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	return (value & CI_CICR0_INTERRUPT_MASK);
}

void ci_clear_int_status(unsigned int status)
{
	// write 1 to clear
	CISR = status;
}

unsigned int ci_get_int_status()
{
	int value;

	value = CISR;

	return value;
}

void ci_set_reg_value(unsigned int reg_offset, unsigned int value)
{
	CI_REG((u32) (ci_regs_base) + reg_offset) = value;
}

int ci_get_reg_value(unsigned int reg_offset)
{
	int value;

	value = CI_REG((u32) (ci_regs_base) + reg_offset);
	return value;
}

//-------------------------------------------------------------------------------------------------------
//  Control APIs
//-------------------------------------------------------------------------------------------------------
int ci_init()
{
	// clear all CI registers
	CICR0 = 0x3FF;		// disable all interrupts
	CICR1 = 0;
	CICR2 = 0;
	CICR3 = 0;
	CICR4 = 0;
	CISR = ~0;
	CIFR = 0;
	CITOR = 0;

	// enable CI clock
	CKEN |= CKEN24_CAMERA;
	return 0;
}

void ci_deinit()
{
	// disable CI clock
	CKEN &= ~CKEN24_CAMERA;
}

void ci_enable(int dma_en)
{
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value |= CI_CICR0_ENB;
	if (dma_en) {
		value |= CI_CICR0_DMA_EN;
	}
	CICR0 = value;
	return;
}

int ci_disable(int quick)
{
	volatile unsigned int value, mask;
	int retry;

	// write control bit in cicr0   
	value = CICR0;
	if (quick) {
		value &= ~CI_CICR0_ENB;
		mask = CI_CISR_CQD;
	} else {
		value |= CI_CICR0_DIS;
		mask = CI_CISR_CDD;
	}
	CICR0 = value;

	// wait shutdown complete
	retry = 50;
	while (retry-- > 0) {
		value = CISR;
		if (value & mask) {
			CISR = mask;
			return 0;
		}
		mdelay(10);
	}

	return -1;
}

void ci_slave_capture_enable()
{
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value |= CI_CICR0_SL_CAP_EN;
	CICR0 = value;
	return;
}

void ci_slave_capture_disable()
{
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value &= ~CI_CICR0_SL_CAP_EN;
	CICR0 = value;
	return;
}

void pxa_ci_dma_irq_y(int channel, void *data, struct pt_regs *regs)
{
	int dcsr;
	static int dma_repeated = 0;
	static unsigned int count = 0;

	camera_context_t *cam_ctx = g_camera_context;

	dcsr = DCSR(channel);
	DCSR(channel) = dcsr & ~DCSR_STOPIRQEN;
	if (!((count++) % 100))
		DPRINTK("int occure\n");
	if (still_image_mode == 1) {
		DPRINTK("pxa_ci_dma_irq_y interrupt,task_waiting =%d, still_image_rdy=%d\n",
		       task_waiting, still_image_rdy);
		if (task_waiting == 1) {
			DPRINTK("task waiting");
			wake_up_interruptible(&camera_wait_q);
			task_waiting = 0;
		}
		still_image_rdy = 1;
	} else if (dma_repeated == 0 && (cam_ctx->block_tail == ((cam_ctx->block_header + 2) % cam_ctx->block_number))) {
		dma_repeated = 1;
		pxa_dma_repeat(cam_ctx);
		cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
	} else if (dma_repeated == 1 && (cam_ctx->block_tail != ((cam_ctx->block_header + 1) % cam_ctx->block_number))
		   && (cam_ctx->block_tail != ((cam_ctx->block_header + 2) % cam_ctx->block_number))) {
		pxa_dma_continue(cam_ctx);
		dma_repeated = 0;
	} else if (dma_repeated == 0) {
		cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
	}
	if (task_waiting == 1 && !(cam_ctx->block_header == cam_ctx->block_tail)) {
		wake_up_interruptible(&camera_wait_q);
		task_waiting = 0;
	}
}

void pxa_ci_dma_irq_cb(int channel, void *data, struct pt_regs *regs)
{
	return;
}

void pxa_ci_dma_irq_cr(int channel, void *data, struct pt_regs *regs)
{
	return;
}

inline static void pxa_ci_dma_stop(camera_context_t * cam_ctx)
{
	int ch0, ch1, ch2;

	ch0 = cam_ctx->dma_channels[0];
	ch1 = cam_ctx->dma_channels[1];
	ch2 = cam_ctx->dma_channels[2];
	DCSR(ch0) &= ~DCSR_RUN;
	DCSR(ch1) &= ~DCSR_RUN;
	DCSR(ch2) &= ~DCSR_RUN;
}


void pxa_dma_start(camera_context_t * cam_ctx)
{
	unsigned char cnt_blk;
	pxa_dma_desc *cnt_desc;

	cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
	cnt_blk = (unsigned char) cam_ctx->block_header;

	cnt_desc = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_physical + cnt_blk * cam_ctx->fifo0_num_descriptors;

	DDADR(cam_ctx->dma_channels[0]) = cnt_desc;
	DCSR(cam_ctx->dma_channels[0]) |= DCSR_RUN;

	if (cam_ctx->fifo1_num_descriptors) {
		cnt_desc =
		    (pxa_dma_desc *) cam_ctx->fifo1_descriptors_physical + cnt_blk * cam_ctx->fifo1_num_descriptors;
		DDADR(cam_ctx->dma_channels[1]) = cnt_desc;
		DCSR(cam_ctx->dma_channels[1]) |= DCSR_RUN;
	}

	if (cam_ctx->fifo2_num_descriptors) {
		cnt_desc =
		    (pxa_dma_desc *) cam_ctx->fifo2_descriptors_physical + cnt_blk * cam_ctx->fifo2_num_descriptors;
		DDADR(cam_ctx->dma_channels[2]) = cnt_desc;
		DCSR(cam_ctx->dma_channels[2]) |= DCSR_RUN;
	}

	return;
}


void pxa_camera_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int cisr;
	static int dma_started = 0;

	disable_irq(IRQ_CAMERA);

	cisr = CISR;
	if (cisr & CI_CISR_SOF) {
		DPRINTK("CI_CISR_SOF \n");
		if (dma_started == 0) {
			dma_started = 1;
		}
		CISR |= CI_CISR_SOF;
	}
	if (cisr & CI_CISR_EOF) {
		DPRINTK("CI_CISR_EOF\n");
		CISR |= CI_CISR_EOF;
		//wake_up_interruptible(&camera_wait_q);
	}
	enable_irq(IRQ_CAMERA);
}

void pxa_dma_repeat(camera_context_t * cam_ctx)
{
	pxa_dma_desc *cnt_head, *cnt_tail;
	int cnt_block;

	cnt_block = (cam_ctx->block_header + 1) % cam_ctx->block_number;
// FIFO0
	(pxa_dma_desc *) cnt_head =
	    (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual + cnt_block * cam_ctx->fifo0_num_descriptors;
	cnt_tail = cnt_head + cam_ctx->fifo0_num_descriptors - 1;
	cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
// FIFO1
	if (cam_ctx->fifo1_transfer_size) {
		cnt_head =
		    (pxa_dma_desc *) cam_ctx->fifo1_descriptors_virtual + cnt_block * cam_ctx->fifo1_num_descriptors;
		cnt_tail = cnt_head + cam_ctx->fifo1_num_descriptors - 1;
		cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
	}
// FIFO2
	if (cam_ctx->fifo2_transfer_size) {
		cnt_head =
		    (pxa_dma_desc *) cam_ctx->fifo2_descriptors_virtual + cnt_block * cam_ctx->fifo2_num_descriptors;
		cnt_tail = cnt_head + cam_ctx->fifo2_num_descriptors - 1;
		cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
	}
	return;
}

void pxa_dma_continue(camera_context_t * cam_ctx)
{
	pxa_dma_desc *cnt_head, *cnt_tail;
	pxa_dma_desc *next_head;
	int cnt_block, next_block;

	cnt_block = cam_ctx->block_header;
	next_block = (cnt_block + 1) % cam_ctx->block_number;
// FIFO0        
	cnt_head = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual + cnt_block * cam_ctx->fifo0_num_descriptors;
	cnt_tail = cnt_head + cam_ctx->fifo0_num_descriptors - 1;
	next_head = (pxa_dma_desc *) cam_ctx->fifo0_descriptors_virtual + next_block * cam_ctx->fifo0_num_descriptors;
	cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
// FIFO1
	if (cam_ctx->fifo1_transfer_size) {
		cnt_head =
		    (pxa_dma_desc *) cam_ctx->fifo1_descriptors_virtual + cnt_block * cam_ctx->fifo1_num_descriptors;
		cnt_tail = cnt_head + cam_ctx->fifo1_num_descriptors - 1;
		next_head =
		    (pxa_dma_desc *) cam_ctx->fifo1_descriptors_virtual + next_block * cam_ctx->fifo1_num_descriptors;
		cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
	}
// FIFO2
	if (cam_ctx->fifo2_transfer_size) {
		cnt_head =
		    (pxa_dma_desc *) cam_ctx->fifo2_descriptors_virtual + cnt_block * cam_ctx->fifo2_num_descriptors;
		cnt_tail = cnt_head + cam_ctx->fifo2_num_descriptors - 1;
		next_head =
		    (pxa_dma_desc *) cam_ctx->fifo2_descriptors_virtual + next_block * cam_ctx->fifo2_num_descriptors;
		cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
	}
	return;

}

void ci_dump(void)
{
	DPRINTK("CICR0 = 0x%8x \n", CICR0);
	DPRINTK("CICR1 = 0x%8x \n", CICR1);
	DPRINTK("CICR2 = 0x%8x \n", CICR2);
	DPRINTK("CICR3 = 0x%8x \n", CICR3);
	DPRINTK("CICR4 = 0x%8x \n", CICR4);
	DPRINTK("CISR  = 0x%8x \n", CISR);
	DPRINTK("CITOR = 0x%8x \n", CITOR);
	DPRINTK("CIFR  = 0x%8x \n", CIFR);
}

module_init(pxa_camera_init);
module_exit(pxa_camera_exit);

MODULE_DESCRIPTION("Bulverde Camera Interface driver");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
