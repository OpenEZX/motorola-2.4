/*
 *  linux/drivers/media/video/pxa_camera.c
 *
 *  Bulverde Processor Camera Interface driver.
 *
 *  Copyright (C) 2003-2004 Motorola
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
 * Dec 19,2003 - (Motorola) Created
 * Jan 1,2004  - (Motorola) Modified
 * Feb 5,2004 - (Motorola) Set frame rate in video mode
 * Feb 26,2004 - (Motorola) New chip id support
 *                           Update algorithm for DMA transfer
 *                           Update strategy for memory management
 *                           Fix still picture capture failed sometime
 *                           New Agilent sensor chip ID support
 *                           Make output height in an even multiple of 8
 *                           Dynamic power management feature
 * March 8,2004 - (Motorola) Photo effects setting
 *                            Fix segmentation fault in rmmod
 *                            Adjust default image buffer size
 * Apr 26,2004 - (Motorola) Power Management added
 *                          Photo effects setting bug fix
 * May 28,2004 - (Motorola) Add two new interface:
 *                                                 1 get ready frames
 *                                                 2 set frame buffer count
 * June 10,2004 - (Motorola) Add mt9v111 support
 * 
 */

/*==================================================================================
                                 INCLUDE FILES
================================================================================*/  
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

#include <linux/types.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>

#include "camera.h"
//#include "adcm2700_hw.h"

#define CIBR0_PHY	(0x50000000 + 0x28)
#define CIBR1_PHY	(0x50000000 + 0x30)
#define CIBR2_PHY	(0x50000000 + 0x38)
/*
 * It is required to have at least 3 frames in buffer
 * in current implementation
 */
#define FRAMES_IN_BUFFER    	3
#define SINGLE_DESC_TRANS_MAX  	 PAGE_SIZE


#define DRCMR68		__REG(0x40001110)  /* Request to Channel Map Register for Camera FIFO 0 Request */
#define DRCMR69		__REG(0x40001114)  /* Request to Channel Map Register for Camera FIFO 1 Request */
#define DRCMR70		__REG(0x40001118)  /* Request to Channel Map Register for Camera FIFO 2 Request */

static camera_context_t  *g_camera_context;
#ifdef  CONFIG_ARCH_EZX_E680
extern camera_function_t  e680_camera_func;
#endif
#ifdef CONFIG_ARCH_EZX_A780
  #ifdef CONFIG_CAMERA_MT9M111
  extern camera_function_t  mt9m111_func;
  #endif
  #ifdef CONFIG_CAMERA_OV9640
  extern camera_function_t  ov9640_func;
  #endif
  #ifdef CONFIG_CAMERA_OV9650
  extern camera_function_t  camera_ov9650_func;
  #endif
  #ifdef CONFIG_CAMERA_ADCM3800
  extern camera_function_t  camera_adcm3800_func;
  #endif
#endif
wait_queue_head_t  camera_wait_q;	

/* /dev/videoX registration number */
static int    minor     = 0;
static int    ci_dma_y  = -1;
static int    ci_dma_cb = -1;
static int    ci_dma_cr = -1;
static int    task_waiting      = 0;
static int    still_image_mode  = 0;
static int    still_image_rdy   = 0;
static int    first_video_frame = 0;
static int    detected_sensor_type = 0;
// map of camera image format (camera.h) ==> capture interface format (ci.h)
static const CI_IMAGE_FORMAT FORMAT_MAPPINGS[] = 
{
        CI_RAW8,                   //RAW
        CI_RAW9,
        CI_RAW10,

        CI_RGB444,                 //RGB
        CI_RGB555,
        CI_RGB565,
        CI_RGB666_PACKED,          //RGB Packed 
        CI_RGB666,
        CI_RGB888_PACKED,
        CI_RGB888,
        CI_RGBT555_0,              //RGB+Transparent bit 0
        CI_RGBT888_0,
        CI_RGBT555_1,              //RGB+Transparent bit 1  
        CI_RGBT888_1,
    
        CI_INVALID_FORMAT,
        CI_YCBCR422,               //YCBCR
        CI_YCBCR422_PLANAR,        //YCBCR Planaried
        CI_INVALID_FORMAT,
        CI_INVALID_FORMAT
};


void pxa_ci_dma_irq_y(int channel, void *data, struct pt_regs *regs);
void pxa_ci_dma_irq_cb(int channel, void *data, struct pt_regs *regs);
void pxa_ci_dma_irq_cr(int channel, void *data, struct pt_regs *regs);
void stop_dma_transfer(p_camera_context_t camera_context);
int  start_capture(p_camera_context_t camera_context, unsigned int block_id, unsigned int frames);
int  camera_init(p_camera_context_t camera_context);
static int update_dma_chain(p_camera_context_t camera_context);

static int pxa_camera_open(struct video_device *dev, int flags);
static void pxa_camera_close(struct video_device *dev);
static long pxa_camera_read(struct video_device *, char *,  unsigned long , int);
unsigned int pxa_camera_poll(struct video_device *dev, struct file *file, poll_table *wait);
static int pxa_camera_ioctl(struct video_device *dev, unsigned int cmd, void *param);
static int pxa_camera_mmap(struct video_device *dev, const char *adr, unsigned long size);
int pxa_camera_video_init(struct video_device *vdev);

void start_dma_transfer(p_camera_context_t camera_context, unsigned block_id);
void pxa_dma_repeat(camera_context_t  *cam_ctx);
void pxa_dma_continue(camera_context_t *cam_ctx);

int pxa_camera_mem_deinit(void);
int pxa_camera_mem_init(void);

static struct video_device vd = {
	owner:		THIS_MODULE,
	name:		"E680 camera",
	type:		VID_TYPE_CAPTURE,
	hardware:	VID_HARDWARE_PXA_CAMERA,      /* FIXME */
	open:		pxa_camera_open,
	close:		pxa_camera_close,
	read:		pxa_camera_read,
	poll:		pxa_camera_poll,
	ioctl:		pxa_camera_ioctl,
	mmap:		pxa_camera_mmap,
	initialize:	pxa_camera_video_init,
	minor:		-1,
};


/***********************************************************************
 *
 * Declarations
 *
 ***********************************************************************/


#ifdef CONFIG_PM
    static struct pm_dev *pm_dev;
#endif   

#ifdef CONFIG_PM
static int camera_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
    if(g_camera_context == NULL)
    {
      return 0;
    }
    
    if(g_camera_context->camera_functions == NULL)
    {
      return 0;
    }
   
    switch(req)
    {
   	case PM_SUSPEND:
           g_camera_context->camera_functions->pm_management(g_camera_context, 1);
	   	break;
        
       case PM_RESUME:
           g_camera_context->camera_functions->pm_management(g_camera_context, 0);
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
 
static int pxa_dma_buffer_init(p_camera_context_t camera_context)
{
	struct page    *page;
	unsigned int	pages;
	unsigned int	page_count;

	camera_context->pages_allocated = 0;

	pages = (PAGE_ALIGN(camera_context->buf_size) / PAGE_SIZE);

	camera_context->page_array = (struct page **)
                                 kmalloc(pages * sizeof(struct page *),
                                 GFP_KERNEL);
                               
	if(camera_context->page_array == NULL)
	{
		return -ENOMEM;
	}
	memset(camera_context->page_array, 0, pages * sizeof(struct page *));

	for(page_count = 0; page_count < pages; page_count++)
	{
		page = alloc_page(GFP_KERNEL);
		if(page == NULL)
		{
			goto error;
		}
		camera_context->page_array[page_count] = page;
		set_page_count(page, 1);
		SetPageReserved(page);
	}
	camera_context->buffer_virtual = remap_page_array(camera_context->page_array, 
                                                      pages,
 	                                                  GFP_KERNEL);
	if(camera_context->buffer_virtual == NULL)
	{
		goto error;
	}

	camera_context->pages_allocated = pages;

	return 0;

error:
	for(page_count = 0; page_count < pages; page_count++)
	{
		if((page = camera_context->page_array[page_count]) != NULL)
		{
			ClearPageReserved(page);
			set_page_count(page, 1);
			put_page(page);
		}
	}
	kfree(camera_context->page_array);

	return -ENOMEM;
}

static void pxa_dma_buffer_free(p_camera_context_t camera_context)
{
	struct page *page;
	int page_count;

	if(camera_context->buffer_virtual == NULL)
		return;

	vfree(camera_context->buffer_virtual);

	for(page_count = 0; page_count < camera_context->pages_allocated; page_count++)
	{
		if((page = camera_context->page_array[page_count]) != NULL)
		{
			ClearPageReserved(page);
			set_page_count(page, 1);
			put_page(page);
		}
	}
	kfree(camera_context->page_array);
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
//#ifdef  CONFIG_ARCH_EZX_E680
static int generate_fifo2_dma_chain(p_camera_context_t camera_context, pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
    int des_transfer_size, remain_size, target_page_num;
    unsigned int i,j;

    cur_des_virtual  = (pxa_dma_desc *)camera_context->fifo2_descriptors_virtual;
    cur_des_physical = (pxa_dma_desc *)camera_context->fifo2_descriptors_physical;

    for(i=0; i<camera_context->block_number; i++) 
    {
        // in each iteration, generate one dma chain for one frame
        remain_size = camera_context->fifo2_transfer_size;

        target_page_num = camera_context->pages_per_block * i +
        camera_context->pages_per_fifo0 +
        camera_context->pages_per_fifo1;

        if (camera_context->pages_per_fifo1 > 1) 
        {
            for(j=0; j<camera_context->fifo2_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                    
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR2_PHY;      // FIFO2 physical address
                cur_des_virtual->dtadr =
                page_to_bus(camera_context->page_array[target_page_num]);
                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                target_page_num++;
            }
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
        }
        else 
        {
            for(j=0; j<camera_context->fifo2_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX/2) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX/2;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR2_PHY;      // FIFO2 physical address
                if (!(j % 2))
                {
                    cur_des_virtual->dtadr =
                    page_to_bus(camera_context->page_array[target_page_num]);
                }
                else 
                {
                    cur_des_virtual->dtadr =
                    page_to_bus(camera_context->page_array[target_page_num]) + (PAGE_SIZE/2);
                }

                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
    
                ddbg_print(" CR: the ddadr = %8x, dtadr = %8x, dcmd = %8x, page = %8x \n", 
                            cur_des_virtual->ddadr,
                            cur_des_virtual->dtadr,
                            cur_des_virtual->dcmd,
                            page_to_bus(camera_context->page_array[target_page_num]) );
    
                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
        
                if (j % 2)
                {
                    target_page_num++;
                }
            }

            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
            //last_des_virtual->ddadr |= 0x1;
        }
    }
    last_des_virtual->ddadr = ((unsigned)camera_context->fifo2_descriptors_physical);
    
    *cur_vir = cur_des_virtual;
    *cur_phy = cur_des_physical;
    return 0;
}
static int generate_fifo1_dma_chain(p_camera_context_t camera_context, pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
    int des_transfer_size, remain_size, target_page_num;
    unsigned int i,j;

    cur_des_virtual  = (pxa_dma_desc *)camera_context->fifo1_descriptors_virtual;
    cur_des_physical = (pxa_dma_desc *)camera_context->fifo1_descriptors_physical;

    for(i=0; i<camera_context->block_number; i++)
    {
        // in each iteration, generate one dma chain for one frame
        remain_size = camera_context->fifo1_transfer_size;

        target_page_num = camera_context->pages_per_block * i +
        camera_context->pages_per_fifo0;

        if (camera_context->pages_per_fifo1 > 1) 
        {
            for(j=0; j<camera_context->fifo1_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX) 
                    des_transfer_size = SINGLE_DESC_TRANS_MAX;
                else
                    des_transfer_size = remain_size;
                
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR1_PHY;      // FIFO1 physical address
                cur_des_virtual->dtadr =
                page_to_bus(camera_context->page_array[target_page_num]);
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
        else  
        {
            for(j=0; j<camera_context->fifo1_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX/2) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX/2;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR1_PHY;      // FIFO1 physical address
                if(!(j % 2))
                {
                    cur_des_virtual->dtadr =
                    page_to_bus(camera_context->page_array[target_page_num]);
                }
                else 
                {
                    cur_des_virtual->dtadr =
                    page_to_bus(camera_context->page_array[target_page_num]) + (PAGE_SIZE/2);
                }

                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

                ddbg_print("CB: the ddadr = %8x, dtadr = %8x, dcmd = %8x, page = %8x \n", 
                        cur_des_virtual->ddadr,
                        cur_des_virtual->dtadr,
                        cur_des_virtual->dcmd,
                        page_to_bus(camera_context->page_array[target_page_num]));

                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                if (j % 2)
                {
                    target_page_num++;
                }
            }//end of for j...
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
          }// end of else
    }//end of for i...
    last_des_virtual->ddadr = ((unsigned)camera_context->fifo1_descriptors_physical);
    *cur_vir = cur_des_virtual;
    *cur_phy = cur_des_physical;

    return 0;
}
static int generate_fifo0_dma_chain(p_camera_context_t camera_context, pxa_dma_desc ** cur_vir, pxa_dma_desc ** cur_phy)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
    int des_transfer_size, remain_size, target_page_num;
    unsigned int i,j;
    
    cur_des_virtual = (pxa_dma_desc *)camera_context->fifo0_descriptors_virtual;
    cur_des_physical = (pxa_dma_desc *)camera_context->fifo0_descriptors_physical;

    for(i=0; i<camera_context->block_number; i++) 
    {
        // in each iteration, generate one dma chain for one frame
        remain_size = camera_context->fifo0_transfer_size;

        // assume the blocks are stored consecutively
        target_page_num = camera_context->pages_per_block * i;

        if (camera_context->pages_per_fifo0 > 2) 
        {
            for(j=0; j<camera_context->fifo0_num_descriptors; j++) 
            {
                // set descriptor
                if (remain_size > SINGLE_DESC_TRANS_MAX)
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR0_PHY;       // FIFO0 physical address
                cur_des_virtual->dtadr =
                page_to_bus(camera_context->page_array[target_page_num]);
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
        else
        {
            for(j=0; j<camera_context->fifo0_num_descriptors; j++) 
            {
                // set descriptor
                if(remain_size > SINGLE_DESC_TRANS_MAX/2) 
                {
                    des_transfer_size = SINGLE_DESC_TRANS_MAX/2;
                }
                else
                {
                    des_transfer_size = remain_size;
                }
                cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
                cur_des_virtual->dsadr = CIBR0_PHY;       // FIFO0 physical address
                if(!(j % 2))
                {
                    cur_des_virtual->dtadr = page_to_bus(camera_context->page_array[target_page_num]);
                }
                else 
                {
                    cur_des_virtual->dtadr = page_to_bus(camera_context->page_array[target_page_num]) + (PAGE_SIZE / 2);
                }

                cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
 
                ddbg_print(" Y: the ddadr = %8x, dtadr = %8x, dcmd = %8x, page = %8x, page_num = %d \n", 
                            cur_des_virtual->ddadr,
                            cur_des_virtual->dtadr,
                            cur_des_virtual->dcmd,
                            page_to_bus(camera_context->page_array[target_page_num]),
                            target_page_num);
                // advance pointers
                remain_size -= des_transfer_size;
                cur_des_virtual++;
                cur_des_physical++;
                if (j % 2)
                {
                    target_page_num++;
                }
            }//end of for j..
            // stop the dma transfer on one frame captured
            last_des_virtual = cur_des_virtual - 1;
            //last_des_virtual->ddadr |= 0x1;
        }// end of else	
    }
    last_des_virtual->ddadr = ((unsigned)camera_context->fifo0_descriptors_physical);
    *cur_vir = cur_des_virtual;
    *cur_phy = cur_des_physical;
    return 0;
}

int update_dma_chain(p_camera_context_t camera_context)
{
    pxa_dma_desc *cur_des_virtual, *cur_des_physical; 
    // clear descriptor pointers
    camera_context->fifo0_descriptors_virtual = camera_context->fifo0_descriptors_physical = 0;
    camera_context->fifo1_descriptors_virtual = camera_context->fifo1_descriptors_physical = 0;
    camera_context->fifo2_descriptors_virtual = camera_context->fifo2_descriptors_physical = 0;

    // calculate how many descriptors are needed per frame
    camera_context->fifo0_num_descriptors =
    camera_context->pages_per_fifo0 > 2 ? camera_context->pages_per_fifo0 : (camera_context->fifo0_transfer_size / (PAGE_SIZE/2) + 1);

    camera_context->fifo1_num_descriptors =
    camera_context->pages_per_fifo1  > 1 ? camera_context->pages_per_fifo1 : (camera_context->fifo1_transfer_size / (PAGE_SIZE/2) + 1);

    camera_context->fifo2_num_descriptors =
    camera_context->pages_per_fifo2  > 1 ? camera_context->pages_per_fifo2 : (camera_context->fifo2_transfer_size / (PAGE_SIZE/2) + 1);

    // check if enough memory to generate descriptors
    if((camera_context->fifo0_num_descriptors + camera_context->fifo1_num_descriptors + 
        camera_context->fifo2_num_descriptors) * camera_context->block_number 
         > camera_context->dma_descriptors_size)
    {
      return -1;
    }

    // generate fifo0 dma chains
    camera_context->fifo0_descriptors_virtual = (unsigned)camera_context->dma_descriptors_virtual;
    camera_context->fifo0_descriptors_physical = (unsigned)camera_context->dma_descriptors_physical;
    // generate fifo0 dma chains
    generate_fifo0_dma_chain(camera_context, &cur_des_virtual, &cur_des_physical);
     
    // generate fifo1 dma chains
    if(!camera_context->fifo1_transfer_size)
    {
       return 0;
    }
    // record fifo1 descriptors' start address
    camera_context->fifo1_descriptors_virtual = (unsigned)cur_des_virtual;
    camera_context->fifo1_descriptors_physical = (unsigned)cur_des_physical;
    // generate fifo1 dma chains
    generate_fifo1_dma_chain(camera_context, &cur_des_virtual, &cur_des_physical);
    
    if(!camera_context->fifo2_transfer_size) 
    {
      return 0;
    }
    // record fifo1 descriptors' start address
    camera_context->fifo2_descriptors_virtual = (unsigned)cur_des_virtual;
    camera_context->fifo2_descriptors_physical = (unsigned)cur_des_physical;
    // generate fifo2 dma chains
    generate_fifo2_dma_chain(camera_context,  &cur_des_virtual, &cur_des_physical);
        
    return 0;   
}
//#else
#if 0
int update_dma_chain(p_camera_context_t camera_context)
{
    
	pxa_dma_desc *cur_des_virtual, *cur_des_physical, *last_des_virtual = NULL;
	int des_transfer_size, remain_size;
	unsigned int i,j;
	int target_page_num;
    
    dbg_print("");

	// clear descriptor pointers
	camera_context->fifo0_descriptors_virtual = camera_context->fifo0_descriptors_physical = 0;
	camera_context->fifo1_descriptors_virtual = camera_context->fifo1_descriptors_physical = 0;
	camera_context->fifo2_descriptors_virtual = camera_context->fifo2_descriptors_physical = 0;

	// calculate how many descriptors are needed per frame
	camera_context->fifo0_num_descriptors =
		camera_context->pages_per_fifo0;

	camera_context->fifo1_num_descriptors =
		camera_context->pages_per_fifo1;

	camera_context->fifo2_num_descriptors =
		camera_context->pages_per_fifo2;

	// check if enough memory to generate descriptors
	if((camera_context->fifo0_num_descriptors + 
        camera_context->fifo1_num_descriptors +
		camera_context->fifo2_num_descriptors) * camera_context->block_number
		> camera_context->dma_descriptors_size)
    {
		return -1;
    }

    dbg_print("1");
	// generate fifo0 dma chains
	camera_context->fifo0_descriptors_virtual  = (unsigned)camera_context->dma_descriptors_virtual;
	camera_context->fifo0_descriptors_physical = (unsigned)camera_context->dma_descriptors_physical;
    
	cur_des_virtual  = (pxa_dma_desc *)camera_context->fifo0_descriptors_virtual;
	cur_des_physical = (pxa_dma_desc *)camera_context->fifo0_descriptors_physical;

	for(i=0; i<camera_context->block_number; i++) 
    {
		// in each iteration, generate one dma chain for one frame
		remain_size = camera_context->fifo0_transfer_size;

		// assume the blocks are stored consecutively
		target_page_num = camera_context->pages_per_block * i;

		for(j=0; j<camera_context->fifo0_num_descriptors; j++)
        {
			// set descriptor
		        if(remain_size > SINGLE_DESC_TRANS_MAX)
        			des_transfer_size = SINGLE_DESC_TRANS_MAX;
        		else
		       		des_transfer_size = remain_size;
                    
		        cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
        		cur_des_virtual->dsadr = CIBR0_PHY;       // FIFO0 physical address
        		cur_des_virtual->dtadr = page_to_bus(camera_context->page_array[target_page_num]);
        		cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;

		        // advance pointers
        		remain_size -= des_transfer_size;
        		cur_des_virtual++;
		        cur_des_physical++;
			    target_page_num++;
		}

		// stop the dma transfer on one frame captured
		last_des_virtual = cur_des_virtual - 1;
	}

	last_des_virtual->ddadr = ((unsigned)camera_context->fifo0_descriptors_physical);

	// generate fifo1 dma chains
	if(camera_context->fifo1_transfer_size) 
    {
		// record fifo1 descriptors' start address
		camera_context->fifo1_descriptors_virtual = (unsigned)cur_des_virtual;
		camera_context->fifo1_descriptors_physical = (unsigned)cur_des_physical;

		for(i=0; i<camera_context->block_number; i++) 
        {
			// in each iteration, generate one dma chain for one frame
			remain_size = camera_context->fifo1_transfer_size;

			target_page_num = camera_context->pages_per_block * i +
				              camera_context->pages_per_fifo0;

        		for(j=0; j<camera_context->fifo1_num_descriptors; j++)
                {
        		        // set descriptor
        		    if(remain_size > SINGLE_DESC_TRANS_MAX)
        		        des_transfer_size = SINGLE_DESC_TRANS_MAX;
        		    else
        		        des_transfer_size = remain_size;
                            
        	        cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
        	        cur_des_virtual->dsadr = CIBR1_PHY;      // FIFO1 physical address
        	        cur_des_virtual->dtadr = page_to_bus(camera_context->page_array[target_page_num]);
        	        cur_des_virtual->dcmd  = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
       		     // advance pointers
       		     
       		     remain_size -= des_transfer_size;
       		     cur_des_virtual++;
       		     cur_des_physical++;
 	      		 target_page_num++;
       			}

        		// stop the dma transfer on one frame captured
        		last_des_virtual = cur_des_virtual - 1;
		}
		last_des_virtual->ddadr = ((unsigned)camera_context->fifo1_descriptors_physical);
	}

	// generate fifo2 dma chains
	if(camera_context->fifo2_transfer_size) 
    {
		// record fifo1 descriptors' start address
		camera_context->fifo2_descriptors_virtual = (unsigned)cur_des_virtual;
		camera_context->fifo2_descriptors_physical = (unsigned)cur_des_physical;

		for(i=0; i<camera_context->block_number; i++) 
        {
			// in each iteration, generate one dma chain for one frame
			remain_size = camera_context->fifo2_transfer_size;

			target_page_num = camera_context->pages_per_block * i +
				              camera_context->pages_per_fifo0 +
  	                  	  camera_context->pages_per_fifo1;

	        for(j=0; j<camera_context->fifo2_num_descriptors; j++) 
            {
                		// set descriptor
              	if(remain_size > SINGLE_DESC_TRANS_MAX)
              		des_transfer_size = SINGLE_DESC_TRANS_MAX;
              	else
              		des_transfer_size = remain_size;
              	cur_des_virtual->ddadr = (unsigned)cur_des_physical + sizeof(pxa_dma_desc);
              	cur_des_virtual->dsadr = CIBR2_PHY;      // FIFO2 physical address
              	cur_des_virtual->dtadr = page_to_bus(camera_context->page_array[target_page_num]);
              	cur_des_virtual->dcmd = des_transfer_size | DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_BURST32;
              	// advance pointers
              	remain_size -= des_transfer_size;
              	cur_des_virtual++;
              	cur_des_physical++;
  		        target_page_num++;
	        }

	        // stop the dma transfer on one frame captured
       		last_des_virtual = cur_des_virtual - 1;
       	}
		last_des_virtual->ddadr = ((unsigned)camera_context->fifo2_descriptors_physical);
	}
    
    dbg_print(" success!");
	return 0;  
}
#endif
void start_dma_transfer(p_camera_context_t camera_context, unsigned block_id)
{
	pxa_dma_desc *des_virtual, *des_physical;
    /*
	if(block_id >= camera_context->block_number)
    {
       	return;
    }
      */  
	// start channel 0
	des_virtual = (pxa_dma_desc *)camera_context->fifo0_descriptors_virtual +
		           block_id * camera_context->fifo0_num_descriptors;

	des_physical = (pxa_dma_desc *)camera_context->fifo0_descriptors_physical +
		           block_id * camera_context->fifo0_num_descriptors;

    DDADR(camera_context->dma_channels[0]) = des_physical;
    DCSR(camera_context->dma_channels[0]) |= DCSR_RUN;

	// start channel 1
	if(camera_context->fifo1_descriptors_virtual) 
    {
		des_virtual = (pxa_dma_desc *)camera_context->fifo1_descriptors_virtual + 
			           block_id * camera_context->fifo1_num_descriptors;

		des_physical = (pxa_dma_desc *)camera_context->fifo1_descriptors_physical + 
			           block_id * camera_context->fifo1_num_descriptors;

        DDADR(camera_context->dma_channels[1]) = des_physical;
        DCSR(camera_context->dma_channels[1]) |= DCSR_RUN;
	}

	// start channel 2
	if(camera_context->fifo2_descriptors_virtual) 
    {
		des_virtual = (pxa_dma_desc *)camera_context->fifo2_descriptors_virtual + 
			           block_id * camera_context->fifo2_num_descriptors;

		des_physical = (pxa_dma_desc *)camera_context->fifo2_descriptors_physical + 
			           block_id * camera_context->fifo2_num_descriptors;

        DDADR(camera_context->dma_channels[2]) = des_physical;
        DCSR(camera_context->dma_channels[2]) |= DCSR_RUN;
	}

	camera_context->dma_started = 1;
}

void set_still_image_ready(int rdy)
{
	still_image_rdy = rdy;
}
void stop_dma_transfer(p_camera_context_t camera_context)
{	
    int ch0, ch1, ch2;

    ch0 = camera_context->dma_channels[0];
    ch1 = camera_context->dma_channels[1];
    ch2 = camera_context->dma_channels[2];

    DCSR(ch0) &= ~DCSR_RUN;
    DCSR(ch1) &= ~DCSR_RUN;
    DCSR(ch2) &= ~DCSR_RUN;
	camera_context->dma_started = 0;

    return;
}

int start_capture(p_camera_context_t camera_context, unsigned int block_id, unsigned int frames)
{
    int status;
    status = camera_context->camera_functions->start_capture(camera_context, frames);

	return status;
}
 
/***********************************************************************
 *
 * Init/Deinit APIs
 *
 ***********************************************************************/
int camera_init(p_camera_context_t camera_context)
{
    int ret=0;
    dbg_print(""); 
   
    /*init gpio pins*/
    //camera_gpio_init();
    
    /*init cif*/
  //	ci_init();
	
    /*init sensor */
    if(detected_sensor_type == 0) {
    /* test sensor type */
#ifdef  CONFIG_ARCH_EZX_E680
    camera_context->camera_functions = &e680_camera_func;
    if((ret = camera_context->camera_functions->init(camera_context)) ==0)
    {
	    goto test_success;
    }
#endif
#ifdef CONFIG_ARCH_EZX_A780

  #ifdef CONFIG_CAMERA_MT9M111
    camera_context->camera_functions = &mt9m111_func;
    if((ret = camera_context->camera_functions->init(camera_context)) ==0)
    {
	    goto test_success;
    }
  #endif

  #ifdef CONFIG_CAMERA_ADCM3800
    dbg_print("detect ADCM3800...");
    camera_context->camera_functions = &camera_adcm3800_func;
    if((ret = camera_context->camera_functions->init(camera_context)) ==0)
    {
	    goto test_success;
    }
  #endif

  #ifdef CONFIG_CAMERA_OV9650
    dbg_print("detect OV9650...");
    camera_context->camera_functions = &camera_ov9650_func;
    if((ret = camera_context->camera_functions->init(camera_context)) ==0)
    {
	    goto test_success;
    }
  #endif

  #ifdef CONFIG_CAMERA_OV9640
    camera_context->camera_functions = &ov9640_func;
    if((ret = camera_context->camera_functions->init(camera_context)) ==0)
    {
	    goto test_success;
    }
  #endif

#endif

        camera_context->camera_functions = 0;
	    
        dbg_print("camera function init error!!");
        goto camera_init_err;
    }
    else 
    {
        switch (detected_sensor_type) {
#ifdef  CONFIG_ARCH_EZX_E680
                case CAMERA_TYPE_ADCM_2700:
                    camera_context->camera_functions = &e680_camera_func;
                break;
                case CAMERA_TYPE_MT9V111:
                    camera_context->camera_functions = &e680_camera_func;
                break;
#endif
#ifdef CONFIG_ARCH_EZX_A780
    #ifdef CONFIG_CAMERA_OV9640
                case CAMERA_TYPE_OMNIVISION_9640:
                    camera_context->camera_functions = &ov9640_func;
                break;
    #endif
    #ifdef CONFIG_CAMERA_MT9M111
                case CAMERA_TYPE_MT9M111:
                    camera_context->camera_functions = &mt9m111_func;
                break;
    #endif
    #ifdef CONFIG_CAMERA_ADCM3800
                case CAMERA_TYPE_ADCM3800:
                    camera_context->camera_functions = &camera_adcm3800_func;
                break;
    #endif
    #ifdef CONFIG_CAMERA_OV9650
                case CAMERA_TYPE_OV9650:
                    camera_context->camera_functions = &camera_ov9650_func;
                break;
    #endif
#endif
        }
        camera_context->camera_functions->init(camera_context);
    }
test_success:
    camera_context->dma_channels[0] = ci_dma_y;
    camera_context->dma_channels[1] = ci_dma_cb;
    camera_context->dma_channels[2] = ci_dma_cr;
    DRCMR68 = ci_dma_y | DRCMR_MAPVLD;
    DRCMR69 = ci_dma_cb | DRCMR_MAPVLD;
    DRCMR70 = ci_dma_cr | DRCMR_MAPVLD;	

    detected_sensor_type = camera_context->sensor_type;
#ifdef DEBUG
	ci_dump();
#endif
    return 0;

camera_init_err:
    camera_deinit(camera_context);
    return -1; 
}

void camera_gpio_init()
{

    set_GPIO_mode(  CIF_PD_MD );         /*CIF_PD*/
    GPCR(CIF_PD)  = GPIO_bit(CIF_PD);    /*set to low*/
    set_GPIO_mode(  CIF_RST_MD );        /*CIF_RST*/
    GPSR(CIF_RST) = GPIO_bit(CIF_RST);   /*set to high*/
    
    set_GPIO_mode( CIF_DD0_MD );  /* CIF_DD[0] */
    set_GPIO_mode( CIF_DD1_MD );  /* CIF_DD[1] */
    set_GPIO_mode( CIF_DD2_MD );  /* CIF_DD[2] */
    set_GPIO_mode( CIF_DD3_MD );  /* CIF_DD[3] */
    set_GPIO_mode( CIF_DD4_MD );  /* CIF_DD[4] */
    set_GPIO_mode( CIF_DD5_MD );  /* CIF_DD[5] */
    set_GPIO_mode( CIF_DD6_MD );  /* CIF_DD[6] */
    set_GPIO_mode( CIF_DD7_MD );  /* CIF_DD[7] */   
    set_GPIO_mode( CIF_MCLK_MD ); /* CIF_MCLK  */
    set_GPIO_mode( CIF_PCLK_MD ); /* CIF_PCLK  */
    set_GPIO_mode( CIF_LV_MD );   /* CIF_LV    */
    set_GPIO_mode( CIF_FV_MD );   /* CIF_FV    */
}   


void camera_gpio_deinit()
{
    /* Turn off M_VCC  CIF_PD*/
    GPSR(CIF_PD) = GPIO_bit(CIF_PD);   /* Set PD to low */
    set_GPIO_mode( CIF_MCLK | GPIO_IN); /*trun off MCLK*/      
}

int camera_deinit( p_camera_context_t camera_context )
{
    // deinit sensor
	if(camera_context->camera_functions)
		camera_context->camera_functions->deinit(camera_context);  
	
	// capture interface deinit
	ci_deinit();
	//camera_gpio_deinit();
	return 0;
}

int camera_ring_buf_init(p_camera_context_t camera_context)
{
    dbg_print("");    
	unsigned         frame_size;
    switch(camera_context->capture_output_format)
    {
    case CAMERA_IMAGE_FORMAT_RGB565:
        frame_size = camera_context->capture_width * camera_context->capture_height * 2;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
        frame_size = camera_context->capture_width * camera_context->capture_height * 2;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
        frame_size = camera_context->capture_width * camera_context->capture_height * 2;
        camera_context->fifo0_transfer_size = frame_size / 2;
        camera_context->fifo1_transfer_size = frame_size / 4;
        camera_context->fifo2_transfer_size = frame_size / 4;
        break;
    case CAMERA_IMAGE_FORMAT_RGB666_PLANAR:
        frame_size = camera_context->capture_width * camera_context->capture_height * 4;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    case CAMERA_IMAGE_FORMAT_RGB666_PACKED:
        frame_size = camera_context->capture_width * camera_context->capture_height * 3;
        camera_context->fifo0_transfer_size = frame_size;
        camera_context->fifo1_transfer_size = 0;
        camera_context->fifo2_transfer_size = 0;
        break;
    default:
        return STATUS_WRONG_PARAMETER;
        break;
    }

    camera_context->block_size = frame_size;

	camera_context->pages_per_fifo0 =
		(PAGE_ALIGN(camera_context->fifo0_transfer_size) / PAGE_SIZE);
	camera_context->pages_per_fifo1 =
		(PAGE_ALIGN(camera_context->fifo1_transfer_size) / PAGE_SIZE);
	camera_context->pages_per_fifo2 =
		(PAGE_ALIGN(camera_context->fifo2_transfer_size) / PAGE_SIZE);

	camera_context->pages_per_block =
		camera_context->pages_per_fifo0 +
		camera_context->pages_per_fifo1 +
		camera_context->pages_per_fifo2;

	camera_context->page_aligned_block_size =
		camera_context->pages_per_block * PAGE_SIZE;

	camera_context->block_number_max =
		camera_context->pages_allocated /
		camera_context->pages_per_block;


    //restrict max block number 
    if(camera_context->block_number_max > FRAMES_IN_BUFFER)
    {
       camera_context->block_number = FRAMES_IN_BUFFER; 
    }
    else
    {
       camera_context->block_number = camera_context->block_number_max;
    }
	camera_context->block_header = camera_context->block_tail = 0;
	// generate dma descriptor chain
	return update_dma_chain(camera_context);

}
/***********************************************************************
 *
 * Capture APIs
 *
 ***********************************************************************/
// Set the image format
int camera_set_capture_format(p_camera_context_t camera_context)
{

	int status=-1;
	CI_IMAGE_FORMAT  ci_input_format, ci_output_format;
	CI_MP_TIMING     timing;

	if(camera_context->capture_input_format >  CAMERA_IMAGE_FORMAT_MAX ||
	   camera_context->capture_output_format > CAMERA_IMAGE_FORMAT_MAX )
    {
		return STATUS_WRONG_PARAMETER;
    }

	ci_input_format  = FORMAT_MAPPINGS[camera_context->capture_input_format];
	ci_output_format = FORMAT_MAPPINGS[camera_context->capture_output_format];
    
	if(ci_input_format == CI_INVALID_FORMAT || ci_output_format == CI_INVALID_FORMAT)
    {
	  return STATUS_WRONG_PARAMETER;
    }
    
	ci_set_image_format(ci_input_format, ci_output_format);

 
    timing.BFW = 0;
    timing.BLW = 0;
 
    ci_configure_mp(camera_context->capture_width-1, camera_context->capture_height-1, &timing);

    if(camera_context == NULL || camera_context->camera_functions == NULL || 
       camera_context->camera_functions->set_capture_format == NULL)
    {
	  dbg_print("camera_context point NULL!!!");
	  return -1;
    } 
    status = camera_ring_buf_init(camera_context);
	// set sensor setting
	if(camera_context->camera_functions->set_capture_format(camera_context))
    {
 	   return -1;
    }
   
    // ring buffer init
    return status;
    
}

// take a picture and copy it into the ring buffer
int camera_capture_still_image(p_camera_context_t camera_context, unsigned int block_id)
{
	// init buffer status & capture
    camera_set_int_mask(camera_context, 0x3ff | 0x0400);
    still_image_mode = 1;
    first_video_frame = 0;
    task_waiting = 0;
	camera_context->block_header   = camera_context->block_tail = block_id;  
	camera_context->capture_status = 0;
    still_image_rdy = 0;
	return  start_capture(camera_context, block_id, 1);
    
}

// capture motion video and copy it to the ring buffer
int camera_start_video_capture( p_camera_context_t camera_context, unsigned int block_id )
{
	//init buffer status & capture
    camera_set_int_mask(camera_context, 0x3ff | 0x0400);
    still_image_mode = 0;
    first_video_frame = 1;
	camera_context->block_header   = camera_context->block_tail = block_id; 
	camera_context->capture_status = CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS;
	return start_capture(camera_context, block_id, 0);
}

// disable motion video image capture
void camera_stop_video_capture( p_camera_context_t camera_context )
{
	
	//stop capture
	camera_context->camera_functions->stop_capture(camera_context);
  
	//stop dma
	stop_dma_transfer(camera_context);
    
	//update the flag
	if(!(camera_context->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
    {
		camera_context->capture_status &= ~CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS;
    }
}


/***********************************************************************
 *
 * Flow Control APIs
 *
 ***********************************************************************/
// continue capture image to next available buffer
void camera_continue_transfer( p_camera_context_t camera_context )
{
	// don't think we need this either.  JR
	// continue transfer on next block
	start_dma_transfer( camera_context, camera_context->block_tail );
}

// Return 1: there is available buffer, 0: buffer is full
int camera_next_buffer_available( p_camera_context_t camera_context )
{
	camera_context->block_header = (camera_context->block_header + 1) % camera_context->block_number;
	if(((camera_context->block_header + 1) % camera_context->block_number) != camera_context->block_tail)
	{
		return 1;
	}

	camera_context->capture_status |= CAMERA_STATUS_RING_BUFFER_FULL;
	return 0;
}

// Application supplies the FrameBufferID to the driver to tell it that the application has completed processing of 
// the given frame buffer, and that buffer is now available for re-use.
void camera_release_frame_buffer(p_camera_context_t camera_context, unsigned int frame_buffer_id)
{

	camera_context->block_tail = (camera_context->block_tail + 1) % camera_context->block_number;

	// restart video capture only ifvideo capture is in progress and space is available for image capture
	if((camera_context->capture_status & CAMERA_STATUS_RING_BUFFER_FULL ) && 
	   (camera_context->capture_status & CAMERA_STATUS_VIDEO_CAPTURE_IN_PROCESS))
	{
		if(((camera_context->block_header + 2) % camera_context->block_number) != camera_context->block_tail)
		{
			camera_context->capture_status &= ~CAMERA_STATUS_RING_BUFFER_FULL;
			start_capture(camera_context, camera_context->block_tail, 0);
		}
	}
}

// Returns the FrameBufferID for the first filled frame
// Note: -1 represents buffer empty
int camera_get_first_frame_buffer_id(p_camera_context_t camera_context)
{
	// not sure ifthis routine makes any sense.. JR

	// check whether buffer is empty
	if((camera_context->block_header == camera_context->block_tail) && 
		 !(camera_context->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
    {
	    return -1;
    }

	// return the block header
	return camera_context->block_header;
}

// Returns the FrameBufferID for the last filled frame, this would be used ifwe were polling for image completion data, 
// or we wanted to make sure there were no frames waiting for us to process.
// Note: -1 represents buffer empty
int camera_get_last_frame_buffer_id(p_camera_context_t camera_context)
{

	// check whether buffer is empty
	if((camera_context->block_header == camera_context->block_tail) && 
	     !(camera_context->capture_status & CAMERA_STATUS_RING_BUFFER_FULL))
    {
		return -1;
    }

	// return the block before the block_tail
	return (camera_context->block_tail + camera_context->block_number - 1) % camera_context->block_number;
}


/***********************************************************************
 *
 * Buffer Info APIs
 *
 ***********************************************************************/
// Return: the number of frame buffers allocated for use.
unsigned int camera_get_num_frame_buffers(p_camera_context_t camera_context)
{
	return camera_context->block_number;
}

// FrameBufferID is a number between 0 and N-1, where N is the total number of frame buffers in use.  Returns the address of
// the given frame buffer.  The application will call this once for each frame buffer at application initialization only.
void * camera_get_frame_buffer_addr(p_camera_context_t camera_context, unsigned int frame_buffer_id)
{
	return (void*)((unsigned)camera_context->buffer_virtual +
		 camera_context->page_aligned_block_size * frame_buffer_id);
}

// Return the block id
int camera_get_frame_buffer_id(p_camera_context_t camera_context, void* address)
{
	if(((unsigned)address >= (unsigned)camera_context->buffer_virtual) && 
	   ((unsigned)address <= (unsigned)camera_context->buffer_virtual + camera_context->buf_size))
    {
		return ((unsigned)address - 
                (unsigned)camera_context->buffer_virtual) / 
                camera_context->page_aligned_block_size;
    }

	return -1;
}


/***********************************************************************
 *
 * Frame rate APIs
 *
 ***********************************************************************/
// Set desired frame rate
void camera_set_capture_frame_rate(p_camera_context_t camera_context)
{
	ci_set_frame_rate(camera_context->fps);
} 

// return current setting
void camera_get_capture_frame_rate(p_camera_context_t camera_context)
{
	camera_context->fps = ci_get_frame_rate();
} 


/***********************************************************************
 *
 * Interrupt APIs
 *
 ***********************************************************************/
// set interrupt mask 
void camera_set_int_mask(p_camera_context_t cam_ctx, unsigned int mask)
{
	pxa_dma_desc * end_des_virtual;
	int dma_interrupt_on, i;

	// set CI interrupt
	ci_set_int_mask( mask & CI_CICR0_INTERRUPT_MASK );

	// set dma end interrupt
	if( mask & CAMERA_INTMASK_END_OF_DMA )
		dma_interrupt_on = 1;
	else
		dma_interrupt_on = 0;

	// set fifo0 dma chains' flag
	end_des_virtual = (pxa_dma_desc*)cam_ctx->fifo0_descriptors_virtual + cam_ctx->fifo0_num_descriptors - 1;

    for(i=0; i<cam_ctx->block_number; i++) 
    {
        if(dma_interrupt_on)
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
	end_des_virtual = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual + cam_ctx->fifo0_num_descriptors - 1;

	if(end_des_virtual->dcmd & DCMD_ENDIRQEN)
    {
		ret |= CAMERA_INTMASK_END_OF_DMA;
    }
	return ret;   
} 

// clear interrupt status
void camera_clear_int_status( p_camera_context_t camera_context, unsigned int status )
{
	ci_clear_int_status( (status & 0xFFFF) );   
}

static void camera_free_dma_irq()
{
     if(ci_dma_y>=0) 
    {
        pxa_free_dma(ci_dma_y);
        ci_dma_y = 0;
    }
    if(ci_dma_cb>=0) 
    {
        pxa_free_dma(ci_dma_cb);
        ci_dma_cb = 0;
    }
    if(ci_dma_cr>=0) 
    {
        pxa_free_dma(ci_dma_cr);
        ci_dma_cr = 0;
    }
	DRCMR68 = 0;
	DRCMR69 = 0;
	DRCMR70 = 0;
}
/***********************************************************************************
* Application interface 							   *
***********************************************************************************/
static int pxa_camera_open(struct video_device *dev, int flags)
{
    dbg_print("start...");
    camera_context_t *cam_ctx;
    /*
      Move the code of request camera IRQ and DMQ channel to here
    */     
    /* 1. mapping CI registers, so that we can access the CI */
   	
    ci_dma_y = pxa_request_dma("CI_Y",DMA_PRIO_HIGH, pxa_ci_dma_irq_y, &vd);
    if(ci_dma_y < 0) 
    {
	    camera_free_dma_irq();
      dbg_print( "PXA_CAMERA: Cann't request DMA for Y\n");
      return -EIO;
    }
    dbg_print( "PXA_CAMERA: Request DMA for Y successfully [%d]\n",ci_dma_y);
   
    ci_dma_cb = pxa_request_dma("CI_Cb",DMA_PRIO_HIGH, pxa_ci_dma_irq_cb, &vd);
    if(ci_dma_cb < 0) 
    {
	    camera_free_dma_irq();
	dbg_print( "PXA_CAMERA: Cann't request DMA for Cb\n");
	return -EIO;
    } 
    dbg_print( "PXA_CAMERA: Request DMA for Cb successfully [%d]\n",ci_dma_cb);
    
    ci_dma_cr = pxa_request_dma("CI_Cr",DMA_PRIO_HIGH, pxa_ci_dma_irq_cr, &vd);
    if(ci_dma_cr < 0) 
    {
	    camera_free_dma_irq();
	dbg_print( "PXA_CAMERA: Cann't request DMA for Cr\n");
	return -EIO;
    }
    
    dbg_print( "PXA_CAMERA: Request DMA for Cr successfully [%d]\n",ci_dma_cr);
   
    DRCMR68 = ci_dma_y | DRCMR_MAPVLD;
    DRCMR69 = ci_dma_cb | DRCMR_MAPVLD;
    DRCMR70 = ci_dma_cr | DRCMR_MAPVLD;	


    init_waitqueue_head(&camera_wait_q);
   
    /*alloc memory for camera context*/
    if(pxa_camera_mem_init())
    {
	    camera_free_dma_irq();
      dbg_print("memory allocate failed!");
      return -1;
    }

    cam_ctx = g_camera_context;
    
    /*
     camera_init call init function for E680
    */
    if(camera_init(cam_ctx))
    {
       dbg_print("camera_init faile!");
	    camera_free_dma_irq();
       pxa_camera_mem_deinit();
       return -1;
    }
    
    /*
       allocate memory for dma descriptors 
       init function of each sensor should set proper value for cam_ctx->buf_size 
    */
   	cam_ctx->dma_started = 0;
    cam_ctx->dma_descriptors_virtual = consistent_alloc(GFP_KERNEL, 
                                                        (cam_ctx->dma_descriptors_size) * sizeof(pxa_dma_desc),
                                                        (void *)&(cam_ctx->dma_descriptors_physical));
    if(cam_ctx->dma_descriptors_virtual == NULL)
    {
       dbg_print("consistent alloc memory for dma_descriptors_virtual fail!");
	    camera_free_dma_irq();
       pxa_camera_mem_deinit();
       return -1;
    }


    /*
      alloc memory for picture buffer 
      init function of each sensor should set proper value for cam_ctx->buf_size 
    */    
    if(pxa_dma_buffer_init(cam_ctx) != 0)
    {
      dbg_print("alloc memory for buffer_virtual  %d bytes fail!", g_camera_context->buf_size);
	    camera_free_dma_irq();
      pxa_camera_mem_deinit();
      return -1;
    }
     
    /*
      set default size and capture format
      init function of each sensor should set proper value 
      for capture_width, capture_height, etc. of camera context 
    */    
	if(camera_set_capture_format(cam_ctx) != 0)
	{
		dbg_print("camera function init error! capture format!");
	    camera_free_dma_irq();
		pxa_camera_mem_deinit();
		return -1;
    }
    dbg_print("PXA_CAMERA: pxa_camera_open success!");
    return 0;
}

static void pxa_camera_close(struct video_device *dev)
{
    camera_deinit(g_camera_context);
    pxa_camera_mem_deinit();
    camera_free_dma_irq();
    dbg_print("PXA_CAMERA: pxa_camera_close\n");
}

#define PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, size) \
do { \
	unsigned int len; \
	unsigned int remain_size = size; \
	while (remain_size > 0) { \
		if(remain_size > PAGE_SIZE) \
			len = PAGE_SIZE; \
		else \
			len = remain_size; \
		if(copy_to_user(buf, page_address(*p_page), len)) \
			return -EFAULT; \
		remain_size -= len; \
		buf += len; \
		p_page++; \
	} \
} while (0);


static long pxa_camera_read(struct video_device *dev, 
                            char   *buf,
                            unsigned long count, 
                            int noblock)
{

	struct page **p_page;

	camera_context_t *cam_ctx = g_camera_context;

	if(still_image_mode == 1 && still_image_rdy == 1) 
    {
		p_page = &cam_ctx->page_array[cam_ctx->block_tail * cam_ctx->pages_per_block];

		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo0_transfer_size);
		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo1_transfer_size);
		PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo2_transfer_size);

		still_image_rdy = 0;
		return cam_ctx->block_size;
	}

	if(still_image_mode == 0)
	{
		if(first_video_frame == 1)
			cam_ctx->block_tail = cam_ctx->block_header;
		else
			cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;
	}

	first_video_frame = 0;

	if(cam_ctx->block_header == cam_ctx->block_tail)  
    {
		task_waiting = 1;
		interruptible_sleep_on (&camera_wait_q);
	}

	p_page = &cam_ctx->page_array[cam_ctx->block_tail * cam_ctx->pages_per_block];

	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo0_transfer_size);
	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo1_transfer_size);
	PXA_CAMERA_BUFFER_COPY_TO_USER(buf, p_page, cam_ctx->fifo2_transfer_size);

	return cam_ctx->block_size;
}

struct reg_set_s 
{
	int  val1;
	int  val2;
};

/*ioctl sub functions*/
static int pxa_camera_VIDIOCGCAP(p_camera_context_t cam_ctx, void * param)
{
    dbg_print("VIDIOCGCAP");
    /*
      add a vc member to camera context
    */
    if(copy_to_user(param, &(cam_ctx->vc), sizeof(struct video_capability)))
    {
        return -EFAULT;
    }
    return 0;
}
static int pxa_camera_VIDIOCGWIN(p_camera_context_t cam_ctx, void * param)
{
    struct video_window vw;
    dbg_print("VIDIOCGWIN");
    vw.width  = cam_ctx->capture_width;
    vw.height = cam_ctx->capture_height;
    if(copy_to_user(param, &vw, sizeof(struct video_window)))
    {
        return -EFAULT;
    }
    return 0;
}
static int pxa_camera_VIDIOCSWIN(p_camera_context_t cam_ctx, void * param)
{
    struct video_window vw;
    dbg_print("VIDIOCSWIN");
    if(copy_from_user(&vw, param, sizeof(vw))) 
    {
        dbg_print("VIDIOCSWIN get parameter error!");
        return  -EFAULT;
    } 
    
    if(vw.width > cam_ctx->vc.maxwidth  ||
       vw.height > cam_ctx->vc.maxheight || 
       vw.width < cam_ctx->vc.minwidth   || 
       vw.height < cam_ctx->vc.minheight) 
    {
        dbg_print("VIDIOCSWIN error parameter!");
        dbg_print("vw.width:%d, MAX_WIDTH:%d, MIN_WIDTH:%d", vw.width, cam_ctx->vc.maxwidth, cam_ctx->vc.minwidth);
        dbg_print("vw.height:%d, MAX_HEIGHT:%d, MIN_HEIGHT:%d", vw.width, cam_ctx->vc.maxheight, cam_ctx->vc.minheight);	
        return  -EFAULT;
    }
    
    //make it in an even multiple of 8 
    
    cam_ctx->capture_width  = (vw.width+7)/8;
    cam_ctx->capture_width  *= 8;
    
    cam_ctx->capture_height = (vw.height+7)/8;
    cam_ctx->capture_height *= 8;
    
    return camera_set_capture_format(cam_ctx);
}
static int pxa_camera_VIDIOCSPICT(p_camera_context_t cam_ctx, void * param)
{
    struct video_picture vp;
    dbg_print("VIDIOCSPICT");
    if(copy_from_user(&vp, param, sizeof(vp))) 
    {
        return  -EFAULT;
    }
    cam_ctx->capture_output_format = vp.palette;

    return  camera_set_capture_format(cam_ctx);
    
}

static int pxa_camera_VIDIOCGPICT(p_camera_context_t cam_ctx, void * param)
{
    struct video_picture vp;
    dbg_print("VIDIOCGPICT");
    vp.palette = cam_ctx->capture_output_format;
    if(copy_to_user(param, &vp, sizeof(struct video_picture)))
    {
        return  -EFAULT;
    }
    return 0;
}

static int pxa_camera_VIDIOCCAPTURE(p_camera_context_t cam_ctx, void * param)
{
    int capture_flag = (int)param;
    dbg_print("VIDIOCCAPTURE");
    if(capture_flag > 0) 
    {			
        dbg_print("Still Image capture!");
        camera_capture_still_image(cam_ctx, 0);
    }
    else if(capture_flag == 0) 
    {
        dbg_print("Video Image capture!");
        camera_start_video_capture(cam_ctx, 0);
    }
    else if(capture_flag == -1) 
    {
        dbg_print("Capture stop!"); 
        camera_set_int_mask(cam_ctx, 0x3ff);
        camera_stop_video_capture(cam_ctx);
    }
    else 
    {
        return  -EFAULT;
    }
    return 0;
}

static int pxa_camera_VIDIOCGMBUF(p_camera_context_t cam_ctx, void * param)
{
    struct video_mbuf vm;
    int i;

    dbg_print("VIDIOCGMBUF");

    memset(&vm, 0, sizeof(vm));
    vm.size   = cam_ctx->buf_size;
    vm.frames = cam_ctx->block_number;
    for(i = 0; i < vm.frames; i++)
    {
        vm.offsets[i] = cam_ctx->page_aligned_block_size * i;
    }
    if(copy_to_user((void *)param, (void *)&vm, sizeof(vm)))
    {
        return  -EFAULT;
    }
    return 0;
}
static int pxa_camera_WCAM_VIDIOCSINFOR(p_camera_context_t cam_ctx, void * param)
{

    struct reg_set_s reg_s;
    int ret;
    dbg_print("WCAM_VIDIOCSINFOR");

    if(copy_from_user(&reg_s, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    
    cam_ctx->capture_input_format = reg_s.val1;
    cam_ctx->capture_output_format = reg_s.val2;
    ret=camera_set_capture_format(cam_ctx);
    return  ret;
}
static int pxa_camera_WCAM_VIDIOCGINFOR(p_camera_context_t cam_ctx, void * param)
{
    struct reg_set_s reg_s;
    dbg_print("WCAM_VIDIOCGINFOR");
    reg_s.val1 = cam_ctx->capture_input_format;
    reg_s.val2 = cam_ctx->capture_output_format;
    if(copy_to_user(param, &reg_s, sizeof(int) * 2)) 
    {
        return -EFAULT;
    }
    return 0;
}
static int pxa_camera_WCAM_VIDIOCGCIREG(p_camera_context_t cam_ctx, void * param)
{

    int reg_value, offset;
    dbg_print("WCAM_VIDIOCGCIREG");
    if(copy_from_user(&offset, param, sizeof(int))) 
    {
        return  -EFAULT;
    }
    reg_value = ci_get_reg_value (offset);
    if(copy_to_user(param, &reg_value, sizeof(int)))
    {
        return -EFAULT;
    }
    return 0;
}

static int pxa_camera_WCAM_VIDIOCSCIREG(p_camera_context_t cam_ctx, void * param)
{

    struct reg_set_s reg_s;
    dbg_print("WCAM_VIDIOCSCIREG");
    if(copy_from_user(&reg_s, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    ci_set_reg_value (reg_s.val1, reg_s.val2);
    return 0;

}
    
/*get sensor size */
static int pxa_cam_WCAM_VIDIOCGSSIZE(p_camera_context_t cam_ctx, void * param)
{
   struct adcm_window_size{u16 width, height;} size;
   dbg_print("WCAM_VIDIOCGSSIZE");  
   size.width = cam_ctx->sensor_width;
   size.height = cam_ctx->sensor_height;

   if(copy_to_user(param, &size, sizeof(struct adcm_window_size)))
   {
       return -EFAULT;
   }
  return 0;
}
         
/*get output size*/
static int pxa_cam_WCAM_VIDIOCGOSIZE(p_camera_context_t cam_ctx, void * param)
{

   struct adcm_window_size{u16 width, height;}size;
   dbg_print("WCAM_VIDIOCGOSIZE");  
   size.width  = cam_ctx->capture_width;
   size.height = cam_ctx->capture_height;
   if(copy_to_user(param, &size, sizeof(struct adcm_window_size)))
   {
       return -EFAULT;
   }
   return 0;
}



/*set frame buffer count*/
static int pxa_cam_WCAM_VIDIOCSBUFCOUNT(p_camera_context_t cam_ctx, void * param)
{
   dbg_print("");
   int count;
   if(copy_from_user(&count, param, sizeof(int)))
   {
     return -EFAULT;
   }
   
   if(cam_ctx->block_number_max == 0)
   {
     dbg_print("windows size or format not setting!!");
     return -EFAULT;
   }
   
   if(count < FRAMES_IN_BUFFER)
   {
      count = FRAMES_IN_BUFFER;
   }
   
   if(count > cam_ctx->block_number_max)
   {
      count = cam_ctx->block_number_max;
   }
      

   cam_ctx->block_number = count;
   cam_ctx->block_header = cam_ctx->block_tail = 0;
   //generate dma descriptor chain
   update_dma_chain(cam_ctx);
     
   if(copy_to_user(param, &count, sizeof(int)))
   {
     return -EFAULT;
   }
   
   return 0;
}
         
/*get cur avaliable frames*/     
static int pxa_cam_WCAM_VIDIOCGCURFRMS(p_camera_context_t cam_ctx, void * param)
{
  //dbg_print("");
  struct {int first, last;}pos;
  
  pos.first = cam_ctx->block_tail;
  pos.last  = cam_ctx->block_header;
  
  if(copy_to_user(param, &pos, sizeof(pos)))
  {
     return -EFAULT;
  }
  return 0;
}

/*get sensor type*/
static int pxa_cam_WCAM_VIDIOCGSTYPE(p_camera_context_t cam_ctx, void * param)
{
  dbg_print("");
  if(copy_to_user(param, &(cam_ctx->sensor_type), sizeof(cam_ctx->sensor_type)))
  {
    return -EFAULT;
  }
  return 0;
}

static int pxa_camera_ioctl(struct video_device *dev, unsigned int cmd, void *param)
{
   	switch (cmd) 
    {
        /*get capture capability*/
    case VIDIOCGCAP:
        return pxa_camera_VIDIOCGCAP(g_camera_context, param);
        /* get capture size */
    case VIDIOCGWIN:
        return  pxa_camera_VIDIOCGWIN(g_camera_context, param);

        /* set capture size. */
    case VIDIOCSWIN:
        return pxa_camera_VIDIOCSWIN(g_camera_context, param);
        /*set capture output format*/
    case VIDIOCSPICT:
        return pxa_camera_VIDIOCSPICT(g_camera_context, param);

        /*get capture output format*/
    case VIDIOCGPICT:
        return pxa_camera_VIDIOCGPICT(g_camera_context, param);

        /*start capture */
    case VIDIOCCAPTURE:
        return pxa_camera_VIDIOCCAPTURE(g_camera_context, param);

        /* mmap interface */
    case VIDIOCGMBUF:
         return pxa_camera_VIDIOCGMBUF(g_camera_context, param);

        /* Application extended IOCTL.  */
        /* Register access interface	*/
    case WCAM_VIDIOCSINFOR:
         return pxa_camera_WCAM_VIDIOCSINFOR(g_camera_context, param);

        /*get capture format*/
    case WCAM_VIDIOCGINFOR:
         return pxa_camera_WCAM_VIDIOCGINFOR(g_camera_context, param);

        /*get ci reg value*/
    case WCAM_VIDIOCGCIREG:
        return pxa_camera_WCAM_VIDIOCGCIREG(g_camera_context, param);

        /*set ci reg*/
    case WCAM_VIDIOCSCIREG:
        return pxa_camera_WCAM_VIDIOCSCIREG(g_camera_context, param);
            
        /*get sensor size */  
    case WCAM_VIDIOCGSSIZE:
         return pxa_cam_WCAM_VIDIOCGSSIZE(g_camera_context, param);
    
         /*get output size*/
    case WCAM_VIDIOCGOSIZE:
         return pxa_cam_WCAM_VIDIOCGOSIZE(g_camera_context, param);

         /*set frame buffer count*/     
    case WCAM_VIDIOCSBUFCOUNT:
         return pxa_cam_WCAM_VIDIOCSBUFCOUNT(g_camera_context, param);
         
         /*get cur avaliable frames*/     
    case WCAM_VIDIOCGCURFRMS:
         return pxa_cam_WCAM_VIDIOCGCURFRMS(g_camera_context, param);
         
         /*get cur sensor type*/
    case WCAM_VIDIOCGSTYPE:
         return pxa_cam_WCAM_VIDIOCGSTYPE(g_camera_context, param);
    
    default:
         return  g_camera_context->camera_functions->command(g_camera_context, cmd, param);
    }
    
   return 0;
}

static int pxa_camera_mmap(struct video_device *dev, const char *adr, unsigned long size)
{
   	unsigned long start = (unsigned long)adr;
	camera_context_t *cam_ctx = g_camera_context;
	struct page **p_page = cam_ctx->page_array;

	size = PAGE_ALIGN(size);
	while (size > 0) 
    {
		if(remap_page_range(start, page_to_phys(*p_page), PAGE_SIZE, PAGE_SHARED)) 
        {
			return -EFAULT;
		}
		start += PAGE_SIZE;
		p_page++;
		size -= PAGE_SIZE;
	}
	return 0;
 }

unsigned int pxa_camera_poll(struct video_device *dev, struct file *file, poll_table *wait) 
{
    static int waited = 0;
    camera_context_t *cam_ctx = g_camera_context;

    poll_wait(file, &camera_wait_q, wait);
    
    if(still_image_mode == 1 && still_image_rdy == 1) 
    {
        still_image_rdy = 0;
        waited = 0;
        return POLLIN | POLLRDNORM;
	}
    
    if(first_video_frame == 1)
    {
       first_video_frame = 0;
    }
    else if(still_image_mode == 0 && waited != 1)
    {
       cam_ctx->block_tail = (cam_ctx->block_tail + 1) % cam_ctx->block_number;
    }

    if(cam_ctx->block_header == cam_ctx->block_tail)  
    {
        task_waiting = 1;
        waited = 1;
        return 0;
    }
    waited = 0;
    return POLLIN | POLLRDNORM;
}

int pxa_camera_mem_deinit(void)
{
    if(g_camera_context)
    {
        if(g_camera_context->dma_descriptors_virtual != NULL) 
        {
         consistent_free(g_camera_context->dma_descriptors_virtual, 
                         g_camera_context->dma_descriptors_size * sizeof(pxa_dma_desc),  
                         (int)g_camera_context->dma_descriptors_physical);
		      
          g_camera_context->dma_descriptors_virtual = NULL;

        }
       if(g_camera_context->buffer_virtual != NULL)  
       {
        pxa_dma_buffer_free(g_camera_context);		     
        g_camera_context->buffer_virtual = NULL;
       }
       kfree(g_camera_context);
       g_camera_context = NULL;
    }
    
    return 0;
}

int pxa_camera_mem_init(void)
{
   g_camera_context = kmalloc(sizeof(struct camera_context_s), GFP_KERNEL);

    if(g_camera_context == NULL)
    {
    	dbg_print( "PXA_CAMERA: Cann't allocate buffer for camera control structure \n");
        return -1;
    }
	
    memset(g_camera_context, 0, sizeof(struct camera_context_s));
    
    dbg_print("success!"); 
    return 0;
}

int pxa_camera_video_init(struct video_device *vdev)
{
  return 0;
}


static int __init pxa_camera_init(void)
{
	camera_context_t cam_ctx;
 
    if(request_irq(IRQ_CAMERA, pxa_camera_irq, 0, "PXA Camera", &vd)) 
    {
     dbg_print ("Camera interrupt register failed failed number \n");
     return -EIO;
    } 
    dbg_print ("Camera interrupt register successful \n");
    
    minor =0 ;
 
    if(video_register_device(&vd, VFL_TYPE_GRABBER, minor) < 0) 
    {
	dbg_print("PXA_CAMERA: video_register_device failed\n");
	return -EIO;
    }
#ifdef CONFIG_ARCH_EZX_A780
    PGSR1 |= GPIO_bit(50);
    PGSR0 |= GPIO_bit(19);
#endif

#ifdef CONFIG_PM    
    pm_dev = pm_register(PM_SYS_DEV, 0, camera_pm_callback);
#endif

    dbg_print("PXA_CAMERA: video_register_device successfully. /dev/video%d \n",minor);
  
    return 0;
}

static void __exit pxa_camera_exit(void)
{
    free_irq(IRQ_CAMERA,  &vd);
    
#ifdef CONFIG_PM
     pm_unregister(pm_dev);
#endif
    
    video_unregister_device(&vd);
    
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
	value |= (unsigned)frate << CI_CICR4_FR_RATE_SHIFT;
	CICR4 = value;
}

CI_FRAME_CAPTURE_RATE ci_get_frame_rate(void)
{
	unsigned int value;
	value = CICR4;
	return (CI_FRAME_CAPTURE_RATE)((value >> CI_CICR4_FR_RATE_SHIFT) & CI_CICR4_FR_RATE_SMASK);
}

void ci_set_image_format(CI_IMAGE_FORMAT input_format, CI_IMAGE_FORMAT output_format)
{

    unsigned int value, tbit, rgbt_conv, rgb_conv, rgb_f, ycbcr_f, rgb_bpp, raw_bpp, cspace;
    // write cicr1: preserve ppl value and data width value
    dbg_print("0");
    value = CICR1;
    dbg_print("1");
    value &= ( (CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT) | ((CI_CICR1_DW_SMASK) << CI_CICR1_DW_SHIFT));
    tbit = rgbt_conv = rgb_conv = rgb_f = ycbcr_f = rgb_bpp = raw_bpp = cspace = 0;
    switch(input_format) 
    {
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
        if(output_format == CI_YCBCR422_PLANAR) 
        {
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
        if(output_format == CI_RGBT555_0) 
        {
            rgbt_conv = 2;
            tbit = 0;
        } 
        else if(output_format == CI_RGBT555_1) 
        {
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
        if(output_format == CI_RGB666_PACKED) 
        {
            rgb_f = 1;
        }
        break;  
    case CI_RGB888:
    case CI_RGB888_PACKED:
        cspace = 1;
        rgb_bpp = 4;
        switch(output_format) 
        {
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
            // RGB666 PACKED
        case CI_RGB666_PACKED:
            rgb_conv = 1;
            rgb_f = 1;
            break;
            // end
        case CI_RGB565:
            dbg_print("format : 565");
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
        dbg_print("2");
    value |= (tbit==1) ? CI_CICR1_TBIT : 0;
    value |= rgbt_conv << CI_CICR1_RGBT_CONV_SHIFT;
    value |= rgb_conv << CI_CICR1_RGB_CONV_SHIFT;
    value |= (rgb_f==1) ? CI_CICR1_RBG_F : 0;
    value |= (ycbcr_f==1) ? CI_CICR1_YCBCR_F : 0;
    value |= rgb_bpp << CI_CICR1_RGB_BPP_SHIFT;
    value |= raw_bpp << CI_CICR1_RAW_BPP_SHIFT;
    value |= cspace << CI_CICR1_COLOR_SP_SHIFT;
    CICR1 = value;   

}

void ci_set_mode(CI_MODE mode, CI_DATA_WIDTH data_width)
{
	unsigned int value;
	
	// write mode field in cicr0
	value = CICR0;
	value &= ~(CI_CICR0_SIM_SMASK << CI_CICR0_SIM_SHIFT);
	value |= (unsigned int)mode << CI_CICR0_SIM_SHIFT;
	CICR0 = value;   
	
	// write data width cicr1
	value = CICR1;
	value &= ~(CI_CICR1_DW_SMASK << CI_CICR1_DW_SHIFT);
	value |= ((unsigned)data_width) << CI_CICR1_DW_SHIFT;
	CICR1 = value;   
	return; 
}

void ci_configure_mp(unsigned int ppl, unsigned int lpf, CI_MP_TIMING* timing)
{
	unsigned int value;
	// write ppl field in cicr1
	value = CICR1;
	value &= ~(CI_CICR1_PPL_SMASK << CI_CICR1_PPL_SHIFT);
	value |= (ppl & CI_CICR1_PPL_SMASK) << CI_CICR1_PPL_SHIFT;
	CICR1 = value;   
	
	// write BLW, ELW in cicr2  
	value = CICR2;
	value &= ~(CI_CICR2_BLW_SMASK << CI_CICR2_BLW_SHIFT | CI_CICR2_ELW_SMASK << CI_CICR2_ELW_SHIFT );
	value |= (timing->BLW & CI_CICR2_BLW_SMASK) << CI_CICR2_BLW_SHIFT;
	CICR2 = value;   
	
	// write BFW, LPF in cicr3
	value = CICR3;
	value &= ~(CI_CICR3_BFW_SMASK << CI_CICR3_BFW_SHIFT | CI_CICR3_LPF_SMASK << CI_CICR3_LPF_SHIFT );
	value |= (timing->BFW & CI_CICR3_BFW_SMASK) << CI_CICR3_BFW_SHIFT;
	value |= (lpf & CI_CICR3_LPF_SMASK) << CI_CICR3_LPF_SHIFT;
	CICR3 = value;   

}

void ci_configure_sp(unsigned int ppl, unsigned int lpf, CI_SP_TIMING* timing)
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

void ci_configure_ms(unsigned int ppl, unsigned int lpf, CI_MS_TIMING* timing)
{
	// the operation is same as Master-Parallel
	ci_configure_mp(ppl, lpf, (CI_MP_TIMING*)timing);
}

void ci_configure_ep(int parity_check)
{
	unsigned int value;

	// write parity_enable field in cicr0   
	value = CICR0;
	if(parity_check) 
    {
		value |= CI_CICR0_PAR_EN;
	}
	else 
    {
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
	unsigned int ciclk,  value, div, cccr_l, K;

	// determine the LCLK frequency programmed into the CCCR.
	cccr_l = (CCCR & 0x0000001F);

	if(cccr_l < 8)
		K = 1;
	else if(cccr_l < 17)
		K = 2;
	else 
		K = 3;

	ciclk = (13 * cccr_l) / K;
	
	div = (ciclk + mclk_khz) / ( 2 * mclk_khz ) - 1;
	dbg_print("cccr=%xciclk=%d,cccr_l=%d,K=%d,div=%d,mclk=%d\n",CCCR,ciclk,cccr_l,K,div,mclk_khz);
	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_PCLK_EN | CI_CICR4_MCLK_EN | CI_CICR4_DIV_SMASK<<CI_CICR4_DIV_SHIFT);
	value |= (pclk_enable) ? CI_CICR4_PCLK_EN : 0;
	value |= (mclk_enable) ? CI_CICR4_MCLK_EN : 0;
	value |= div << CI_CICR4_DIV_SHIFT;
	CICR4 = value;   
	return; 
}

void ci_set_polarity(int pclk_sample_falling, int hsync_active_low, int vsync_active_low)
{
          dbg_print(""); 
	unsigned int value;
	
	// write cicr4
	value = CICR4;
	value &= ~(CI_CICR4_PCP | CI_CICR4_HSP | CI_CICR4_VSP);
	value |= (pclk_sample_falling)? CI_CICR4_PCP : 0;
	value |= (hsync_active_low) ? CI_CICR4_HSP : 0;
	value |= (vsync_active_low) ? CI_CICR4_VSP : 0;
	CICR4 = value;   
	return; 
}

void ci_set_fifo(unsigned int timeout, CI_FIFO_THRESHOLD threshold, int fifo1_enable,
               int fifo2_enable)
{
	unsigned int value;
        dbg_print("");
	// write citor
	CITOR = timeout; 
	
	// write cifr: always enable fifo 0! also reset input fifo 
	value = CIFR;
	value &= ~(CI_CIFR_FEN0 | CI_CIFR_FEN1 | CI_CIFR_FEN2 | CI_CIFR_RESETF | 
	CI_CIFR_THL_0_SMASK<<CI_CIFR_THL_0_SHIFT);
	value |= (unsigned int)threshold << CI_CIFR_THL_0_SHIFT;
	value |= (fifo1_enable) ? CI_CIFR_FEN1 : 0;
	value |= (fifo2_enable) ? CI_CIFR_FEN2 : 0;
	value |= CI_CIFR_RESETF | CI_CIFR_FEN0;
	CIFR = value;
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
	dbg_print("-----------value=0x%x\n",value);
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

	return  value;
}

void ci_set_reg_value(unsigned int reg_offset, unsigned int value)
{
    switch(reg_offset)
    {
      case 0:
        CICR0 = value;
        break;
      case 1:
        CICR1 = value;
        break;
      case 2:
        CICR2 = value;
        break;
      case 3:
        CICR3 = value;
        break;
      case 4:
        CICR4 = value;
        break;        
      case 5:
        CISR  = value;
        break;        
      case 6:
        CIFR = value;
        break;        
      case 7:
        CITOR = value;
        break;
    }
}

int ci_get_reg_value(unsigned int reg_offset)
{
    
    unsigned values[] = {CICR0, CICR1, CICR2, CICR3, CICR4, CISR, CIFR, CITOR};
    
    if(reg_offset >=0 && reg_offset <= 7)
    	return values[reg_offset];
     else
        return CISR;
}

//-------------------------------------------------------------------------------------------------------
//  Control APIs
//-------------------------------------------------------------------------------------------------------
int ci_init()
{

	// clear all CI registers  disable all interrupts
	CICR0 = 0x3FF;   
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
	CICR4 = 0;
	mdelay(1);
	CKEN &= ~CKEN24_CAMERA;
}
void ci_reset()
{
  ci_disable(1);
  ci_enable(1);
  mdelay(30);
}
void ci_enable(int dma_en)
{        
	unsigned int value;

	// write mask in cicr0  
	value = CICR0;
	value |= CI_CICR0_ENB;
	if(dma_en) {
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
	if(quick)
        {
		value &= ~CI_CICR0_ENB;
		mask = CI_CISR_CQD;
	}
	else 
        {
		value |= CI_CICR0_DIS;
		mask = CI_CISR_CDD;
	}
	CICR0 = value;   
	
	// wait shutdown complete
	retry = 50;
	while ( retry-- > 0 ) 
    {
 	value = CISR;
	 if( value & mask ) 
     {
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

    static int dma_repeated = 0;
    camera_context_t  *cam_ctx = g_camera_context;
    int        dcsr;
    
    dcsr = DCSR(channel);
    DCSR(channel) = dcsr & ~DCSR_STOPIRQEN;
 
    if(still_image_mode == 1) 
    {
        if(task_waiting == 1) 
        {
            wake_up_interruptible (&camera_wait_q);
            task_waiting = 0;
        }
        still_image_rdy = 1;
    	stop_dma_transfer(cam_ctx);
    } 
    else if(dma_repeated == 0 &&
           (cam_ctx->block_tail == ((cam_ctx->block_header + 2) % cam_ctx->block_number)))  
    {
        dma_repeated = 1;
        pxa_dma_repeat(cam_ctx);
        //dbg_print ("DMA repeated.");
        cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
    }
    else if(dma_repeated == 1 && 
        (cam_ctx->block_tail != ((cam_ctx->block_header + 1) % cam_ctx->block_number)) && 
        (cam_ctx->block_tail != ((cam_ctx->block_header + 2) % cam_ctx->block_number)))  
    {
        pxa_dma_continue(cam_ctx);
        //dbg_print ("DMA continue.");
        dma_repeated = 0;
    }
    else if(dma_repeated == 0) 
    {
        cam_ctx->block_header = (cam_ctx->block_header + 1) % cam_ctx->block_number;
    }
    
    if(task_waiting == 1 && !(cam_ctx->block_header == cam_ctx->block_tail)) 
    {
        wake_up_interruptible (&camera_wait_q);
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


void pxa_camera_irq(int irq, void *dev_id, struct pt_regs *regs)
{
    int cisr;
    disable_irq(IRQ_CAMERA);
    cisr = CISR;
    if(cisr & CI_CISR_SOF) 
    {
        CISR |= CI_CISR_SOF;
    }
    if(cisr & CI_CISR_EOF) 
    {
        CISR |= CI_CISR_EOF;
    }
    enable_irq(IRQ_CAMERA);
}

void pxa_dma_repeat(camera_context_t  *cam_ctx)
{
    pxa_dma_desc *cnt_head, *cnt_tail;
	int cnt_block;

	cnt_block = (cam_ctx->block_header + 1) % cam_ctx->block_number;
    
    // FIFO0
	(pxa_dma_desc *)cnt_head = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual + 
                                cnt_block * cam_ctx->fifo0_num_descriptors;
                                
	cnt_tail = cnt_head + cam_ctx->fifo0_num_descriptors - 1;
	cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
    
    // FIFO1
	if(cam_ctx->fifo1_transfer_size) 
    {
		cnt_head = (pxa_dma_desc *)cam_ctx->fifo1_descriptors_virtual + 
                    cnt_block * cam_ctx->fifo1_num_descriptors;
                    
		cnt_tail = cnt_head + cam_ctx->fifo1_num_descriptors - 1;
		cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
	}
    
    // FIFO2
	if(cam_ctx->fifo2_transfer_size) 
    {
		cnt_head = (pxa_dma_desc *)cam_ctx->fifo2_descriptors_virtual + 
                    cnt_block * cam_ctx->fifo2_num_descriptors;
                    
		cnt_tail = cnt_head + cam_ctx->fifo2_num_descriptors - 1;
		cnt_tail->ddadr = cnt_head->ddadr - sizeof(pxa_dma_desc);
	}
 }

void pxa_dma_continue(camera_context_t *cam_ctx)
{
   	pxa_dma_desc *cnt_head, *cnt_tail;
	pxa_dma_desc *next_head;
	int cnt_block, next_block;

	cnt_block = cam_ctx->block_header;
	next_block = (cnt_block + 1) % cam_ctx->block_number;
    
    // FIFO0
	cnt_head  = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual + 
                cnt_block * cam_ctx->fifo0_num_descriptors;
                
	cnt_tail  = cnt_head + cam_ctx->fifo0_num_descriptors - 1;
    
	next_head = (pxa_dma_desc *)cam_ctx->fifo0_descriptors_virtual +
                next_block * cam_ctx->fifo0_num_descriptors;

    cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
    
    // FIFO1
	if(cam_ctx->fifo1_transfer_size) 
    {
		cnt_head  = (pxa_dma_desc *)cam_ctx->fifo1_descriptors_virtual + 
                    cnt_block * cam_ctx->fifo1_num_descriptors;
                    
		cnt_tail  = cnt_head + cam_ctx->fifo1_num_descriptors - 1;
        
		next_head = (pxa_dma_desc *)cam_ctx->fifo1_descriptors_virtual + 
                     next_block * cam_ctx->fifo1_num_descriptors;
                     
		cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
	}
    
    // FIFO2
	if(cam_ctx->fifo2_transfer_size) 
    {
		cnt_head  = (pxa_dma_desc *)cam_ctx->fifo2_descriptors_virtual + 
                   cnt_block * cam_ctx->fifo2_num_descriptors;
                   
		cnt_tail  = cnt_head + cam_ctx->fifo2_num_descriptors - 1;
        
		next_head = (pxa_dma_desc *)cam_ctx->fifo2_descriptors_virtual + 
                    next_block * cam_ctx->fifo2_num_descriptors;
                    
		cnt_tail->ddadr = next_head->ddadr - sizeof(pxa_dma_desc);
	}
	return;

}

void ci_dump(void)
{
    dbg_print ("CICR0 = 0x%8x ", CICR0);
    dbg_print ("CICR1 = 0x%8x ", CICR1);
    dbg_print ("CICR2 = 0x%8x ", CICR2);
    dbg_print ("CICR3 = 0x%8x ", CICR3);
    dbg_print ("CICR4 = 0x%8x ", CICR4);
    dbg_print ("CISR  = 0x%8x ", CISR);
    dbg_print ("CITOR = 0x%8x ", CITOR);
    dbg_print ("CIFR  = 0x%8x ", CIFR);
}


module_init(pxa_camera_init);
module_exit(pxa_camera_exit);

MODULE_DESCRIPTION("Bulverde Camera Interface driver");
MODULE_LICENSE("GPL");

