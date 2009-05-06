/*
 *  sensor2m.c
 *
 *  Copyright (C) 2005,2006 Motorola Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Revision History:
 *               Modification  
 *  Author         Date         Description of Changes
 *  ----------   ------------   -------------------------
 *  Motorola     04/30/2005     Created, modified from mt9m111.c
 *  Motorola     01/06/2006     Update for camera4uvc interface
 *  Motorola     04/12/2006     Fix sensor name
 *  Motorola     08/17/2006     Update comments for GPL
 
*/

/*
==================================================================================
                                 INCLUDE FILES
==================================================================================*/
#include <linux/types.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <linux/wrapper.h>

#include <asm/pgtable.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <linux/wrapper.h>
#include <linux/videodev.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include "camera.h"
#include "sensor2m.h"
#include "sensor2m_hw.h"

#define LOG_TIME_STAMP
       
#define MSCWR1_CAMERA_ON    (0x1 << 15)
#define MSCWR1_CAMERA_SEL   (0x1 << 14)

#define MAX_WIDTH     MAX_SENSOR_B_WIDTH
#define MAX_HEIGHT    MAX_SENSOR_B_HEIGHT

#define MIN_WIDTH	64
#define MIN_HEIGHT	48

#define BUF_SIZE_DEFT     (PAGE_ALIGN(MAX_WIDTH * MAX_HEIGHT * 3 / 4))

static unsigned int mclk_out_hz;

extern int i2c_sensor2m_cleanup(void);
extern int i2c_sensor2m_init(void);

/***********************************************************************
 *
 * Functions
 *
 ***********************************************************************/
int camera_func_sensor2m_init(p_camera_context_t);
int camera_func_sensor2m_deinit(p_camera_context_t);
int camera_func_sensor2m_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param);
int camera_func_sensor2m_set_capture_format(p_camera_context_t);
int camera_func_sensor2m_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_sensor2m_stop_capture(p_camera_context_t);
int camera_func_sensor2m_camera4uvc_command(p_camera_context_t cam_ctx, unsigned int cmd, void *param);
int camera_func_sensor2m_pm_management(p_camera_context_t, int);

camera_function_t  sensor2m_func = 
{
    init:                camera_func_sensor2m_init,
    deinit:              camera_func_sensor2m_deinit,
    command:             camera_func_sensor2m_docommand,
    set_capture_format:  camera_func_sensor2m_set_capture_format,
    start_capture:       camera_func_sensor2m_start_capture,
    stop_capture:        camera_func_sensor2m_stop_capture,
    pm_management:       camera_func_sensor2m_pm_management,
    camera4uvc_command:  camera_func_sensor2m_camera4uvc_command
};

static void sensor2m_init(int dma_en)
{
    camera_gpio_init();
    ci_enable(dma_en);
    udelay(1);                
    GPCR(GPIO_CAM_EN)  = GPIO_bit(GPIO_CAM_EN);   
}

static void sensor2m_hw_deinit(void)
{
#if 1
    GPSR(GPIO_CAM_EN) = GPIO_bit(GPIO_CAM_EN);     
#endif
    
    udelay(200);
    ci_disable(1);
    ci_deinit();
    set_GPIO_mode( GPIO_CIF_MCLK);
    GPCR(GPIO_CIF_MCLK) = GPIO_bit(GPIO_CIF_MCLK);
}

static void sensor2m_deinit(void)
{
    sensor2m_enter_9();
    i2c_sensor2m_cleanup();
    sensor2m_hw_deinit();
}

int camera_func_sensor2m_s9(void)
{
    int ret;
    u16 device_id = 0 ;
    u16 revision = 0 ;

    ci_init();
    ci_set_clock(0, 0, 1, 26);
    camera_gpio_init();

    GPCR(GPIO_CAM_EN)  = GPIO_bit(GPIO_CAM_EN);
    GPCR(GPIO_CAM_RST) = GPIO_bit(GPIO_CAM_RST);

#if 1
    PGSR(GPIO_CAM_EN) |= GPIO_bit(GPIO_CAM_EN);
#endif
    PGSR(GPIO_CAM_RST)|= GPIO_bit(GPIO_CAM_RST);
  
    ci_enable(0);

    udelay(10);
    GPSR(GPIO_CAM_RST) = GPIO_bit(GPIO_CAM_RST);
   
    udelay(2);
   
    ret = i2c_sensor2m_init();
    if(ret >= 0)
    {
        sensor2m_reset_init();
        sensor2m_enter_9(); 
    }
    else
        err_print("error: i2c initialize fail!");
    i2c_sensor2m_cleanup();

    sensor2m_hw_deinit();

    return ret;
}

int camera_func_sensor2m_init(  p_camera_context_t cam_ctx )
{
    u16 device_id = 0 ;
    u16 revision = 0 ;
    int ret;
   
    cam_ctx->sensor_width  = MAX_WIDTH;
    cam_ctx->sensor_height = MAX_HEIGHT;
    
    cam_ctx->capture_width  = DEFT_SENSOR_A_WIDTH;
    cam_ctx->capture_height = DEFT_SENSOR_A_HEIGHT;
    
    cam_ctx->still_width  = MAX_WIDTH;
    cam_ctx->still_height = MAX_HEIGHT;
    
    cam_ctx->frame_rate = cam_ctx->fps = 15;
    cam_ctx->mini_fps = 15;
    
    cam_ctx->buf_size     = BUF_SIZE_DEFT;
    cam_ctx->dma_descriptors_size = (cam_ctx->buf_size/PAGE_SIZE + 10);
    strcpy (cam_ctx->vc.name, "2MP Camera");
    cam_ctx->vc.maxwidth  = MAX_WIDTH;
    cam_ctx->vc.maxheight = MAX_HEIGHT;
    cam_ctx->vc.minwidth  = MIN_WIDTH; 
    cam_ctx->vc.minheight = MIN_HEIGHT;

    ci_init();
   
    ci_set_mode(CI_MODE_MP, CI_DATA_WIDTH8); 

    mclk_out_hz = ci_set_clock(cam_ctx->clk_reg_base, 1, 1, 26);
    if(mclk_out_hz != 26*1000000)
    {
        err_print("error: ci clock is not correct!");
        return -EIO;
    }
    cam_ctx->mclk = mclk_out_hz/1000000;
 
    ci_set_polarity(1, 0, 0);
      
    ci_set_fifo(0, CI_FIFO_THL_32, 1, 1);

    sensor2m_init(1);

    ret = i2c_sensor2m_init();
    if(ret < 0)
    {   
        err_print("error: i2c initialize fail!");
        sensor2m_hw_deinit();
        return ret;
    }

    cam_ctx->sensor_type = CAMERA_TYPE_SENSOR2M;

    sensor2m_exit_9();

    sensor2m_default_settings();

    return 0;
}
    
int camera_func_sensor2m_deinit(  p_camera_context_t camera_context )
{
    sensor2m_deinit();
    return 0;
}

int camera_func_sensor2m_set_capture_format(  p_camera_context_t camera_context )
{
    sensor_window_size wsize;
    u16 sensor_format;
    int aspect, sen_aspect, w, h, swidth, sheight, azoom, bzoom;

    if(camera_context->still_image_mode)
        return 0;
 
    w = camera_context->capture_width;
    h = camera_context->capture_height;
    if(w < MIN_WIDTH || h < MIN_HEIGHT || w > MAX_SENSOR_A_WIDTH || h > MAX_SENSOR_A_HEIGHT)
        return -EINVAL;
   
    swidth = MAX_SENSOR_A_WIDTH;
    sheight = MAX_SENSOR_A_HEIGHT;
    
    sen_aspect = 10000 * MAX_SENSOR_A_HEIGHT/MAX_SENSOR_A_WIDTH;
    
    aspect = 10000 * h / w;
    if(aspect < sen_aspect)
        sheight = swidth * aspect / 10000;
    else if(aspect > sen_aspect)
        swidth = 10000 * sheight / aspect;
    camera_context->sensor_width = swidth;
    camera_context->sensor_height = sheight;
   
    azoom = camera_context->capture_digital_zoom;
    bzoom = camera_context->still_digital_zoom;

    if(azoom < CAMERA_ZOOM_LEVEL_MULTIPLE)
        azoom = CAMERA_ZOOM_LEVEL_MULTIPLE;
    if(azoom > CAMERA_ZOOM_LEVEL_MULTIPLE)
    {
        swidth = swidth * CAMERA_ZOOM_LEVEL_MULTIPLE / azoom;
        sheight = sheight * CAMERA_ZOOM_LEVEL_MULTIPLE / azoom;
        if(swidth < camera_context->capture_width)
        {
            swidth = camera_context->capture_width;
            azoom = camera_context->sensor_width * CAMERA_ZOOM_LEVEL_MULTIPLE / swidth;
            camera_context->capture_digital_zoom = azoom;
        }
        if(sheight < camera_context->capture_height)
        {
            sheight = camera_context->capture_height;
        }
    }
 
    wsize.width = swidth;
    wsize.height = sheight;
    sensor2m_sensor_size(&wsize);
    
    w = camera_context->still_width;
    h = camera_context->still_height;
    if(w < MIN_WIDTH || h < MIN_HEIGHT || w > MAX_SENSOR_B_WIDTH || h > MAX_SENSOR_B_HEIGHT)
        return -EINVAL;
    
    swidth = MAX_SENSOR_B_WIDTH;
    sheight = MAX_SENSOR_B_HEIGHT;
    
    sen_aspect = 10000 * MAX_SENSOR_B_HEIGHT/MAX_SENSOR_B_WIDTH;
    
    aspect = 10000 * h / w;
    if(aspect < sen_aspect)
        sheight = swidth * aspect / 10000;
    else if(aspect > sen_aspect)
        swidth = 10000 * sheight / aspect;
    camera_context->sensor_width = swidth;
    camera_context->sensor_height = sheight;
   
    if(bzoom < CAMERA_ZOOM_LEVEL_MULTIPLE)
        bzoom = CAMERA_ZOOM_LEVEL_MULTIPLE;
    if(bzoom > CAMERA_ZOOM_LEVEL_MULTIPLE)
    {
        swidth = swidth * CAMERA_ZOOM_LEVEL_MULTIPLE / bzoom;
        sheight = sheight * CAMERA_ZOOM_LEVEL_MULTIPLE / bzoom;
        if(swidth < camera_context->still_width)
        {
            swidth = camera_context->still_width;
            bzoom = camera_context->sensor_width * CAMERA_ZOOM_LEVEL_MULTIPLE / swidth;
            camera_context->still_digital_zoom = bzoom;
        }
        if(sheight < camera_context->still_height)
        {
            sheight = camera_context->still_height;
        }
    }

    wsize.width = swidth;
    wsize.height = sheight;
    sensor2m_capture_sensor_size(&wsize);

    wsize.width = camera_context->capture_width;
    wsize.height = camera_context->capture_height;
    sensor2m_output_size(&wsize);

    wsize.width = camera_context->still_width;
    wsize.height = camera_context->still_height;
    sensor2m_capture_size(&wsize);
 
    switch(camera_context->capture_input_format) {
    case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
    case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
        sensor_format = 0;
        break;
    case CAMERA_IMAGE_FORMAT_RGB565:
        sensor_format = 1;
        break;
    case CAMERA_IMAGE_FORMAT_RGB555:
        sensor_format = 2;
        break;
    case CAMERA_IMAGE_FORMAT_RGB444:
        sensor_format = 3;
        break;
    case CAMERA_IMAGE_FORMAT_JPEG:
    case CAMERA_IMAGE_FORMAT_JPEG_SENSOR:
        sensor_format = 4;
        break;
    default:
        sensor_format = 0;
        break;
    }

    camera_context->camera_mode = 0;
    
    switch(camera_context->still_input_format) {
    case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
    case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
        sensor_format = 0;
        break;
    case CAMERA_IMAGE_FORMAT_RGB565:
        sensor_format = 1;
        break;
    case CAMERA_IMAGE_FORMAT_RGB555:
        sensor_format = 2;
        break;
    case CAMERA_IMAGE_FORMAT_RGB444:
        sensor_format = 3;
        break;
    case CAMERA_IMAGE_FORMAT_JPEG:
    case CAMERA_IMAGE_FORMAT_JPEG_SENSOR:
        sensor_format = 4;
        camera_context->camera_mode = 1;
        break;
    default:
        sensor_format = 0;
        break;
    }
    sensor2m_capture_format(sensor_format);

    if(camera_context->camera_mode)
    {
        wsize.width = camera_context->still_width;
        wsize.height = camera_context->still_height * 3 / 4;
        wsize.width &= 0xffffe0;

        sensor2m_set_camera_size(&wsize);

        camera_context->camera_width = wsize.width;
        camera_context->camera_height = wsize.height;
    }
    return 0;
}

int camera_func_sensor2m_start_capture(  p_camera_context_t cam_ctx, unsigned int frames )
{
    int waitingFrame = 0;

    ci_clear_int_status(0xFFFFFFFF);
       
    if (frames == 0) 
    {
        sensor2m_viewfinder_on();
    }
    else 
    {
        sensor2m_snapshot_trigger();
    }

    if(frames == 1) 
    {
        waitingFrame = 0;
    } 
    else
    { 
        waitingFrame = 0;
    }
    camera_skip_frame(cam_ctx, waitingFrame);

    return 0;
}

int camera_func_sensor2m_stop_capture(  p_camera_context_t camera_context )
{
    stop_dma_transfer(camera_context);
    return 0;
}

int camera_func_sensor2m_pm_management(p_camera_context_t cam_ctx, int suspend)
{
    return 0;
}

static int pxa_camera_WCAM_VIDIOCGCAMREG(p_camera_context_t cam_ctx, void * param)
{
    int reg_value, offset;
    if(copy_from_user(&offset, param, sizeof(int))) 
    {
        return -EFAULT;
    }
    reg_value = (int)sensor2m_reg_read((u16)offset);

    if(copy_to_user(param, &reg_value, sizeof(int))) 
    {
        return -EFAULT;
    } 

    return 0;
}
static int pxa_camera_WCAM_VIDIOCSCAMREG(p_camera_context_t cam_ctx, void * param)
{
    struct reg_set_s{int val1; int val2;} reg_s;

    if(copy_from_user(&reg_s, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    sensor2m_reg_write((u16)reg_s.val1, (u16)reg_s.val2);
    return 0;
} 
 
static int pxa_cam_WCAM_VIDIOCSFPS(p_camera_context_t cam_ctx, void * param)
{
    struct {int fps, minfps;} cam_fps;
    if(copy_from_user(&cam_fps, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    cam_ctx->fps = cam_fps.fps;
    cam_ctx->mini_fps = cam_fps.minfps;
    sensor2m_set_time(cam_ctx->fps);
    return 0;
}

  
static int pxa_cam_WCAM_VIDIOCSSSIZE(p_camera_context_t cam_ctx, void * param)
{
  sensor_window_size size;
  
  if(copy_from_user(&size, param, sizeof(sensor_window_size))) 
  {
        return  -EFAULT;
  }
  
  size.width = (size.width+3)/4 * 4;
  size.height = (size.height+3)/4 * 4;
  cam_ctx->sensor_width = size.width;
  cam_ctx->sensor_height = size.height;
  
  return 0;
}

static int pxa_cam_WCAM_VIDIOCSOSIZE(p_camera_context_t cam_ctx, void * param)
{
   sensor_window_size size;
  
   if(copy_from_user(&size, param, sizeof(sensor_window_size))) 
   {
        return  -EFAULT;
   }
   
   size.width = (size.width+1)/2 * 2;
   size.height = (size.height+1)/2 * 2;
     
   cam_ctx->capture_width  = size.width;
   cam_ctx->capture_height = size.height;
   
   return 0;
}
    
  
static int pxa_cam_WCAM_VIDIOCSSTYLE(p_camera_context_t cam_ctx, void * param)
{
  cam_ctx->capture_style = (V4l_PIC_STYLE)param;
  return sensor2m_set_style(cam_ctx->capture_style);
}

     
static int pxa_cam_WCAM_VIDIOCSLIGHT(p_camera_context_t cam_ctx, void * param)
{
   cam_ctx->capture_light = (V4l_PIC_WB)param;
   return  sensor2m_set_light((V4l_PIC_WB)param);
}
    

static int pxa_cam_WCAM_VIDIOCSBRIGHT(p_camera_context_t cam_ctx, void * param)
{
   cam_ctx->capture_bright = (int)param;
   return  sensor2m_set_bright((int)param);
}

static int pxa_cam_WCAM_VIDIOCSFLICKER(p_camera_context_t cam_ctx, void * param)
{
   cam_ctx->flicker_freq = (int)param;
   return  sensor2m_set_flicker(cam_ctx->flicker_freq);
}

static int pxa_cam_WCAM_VIDIOCSNIGHTMODE(p_camera_context_t cam_ctx, void * param)
{
    struct {u32 mode, maxexpotime; } cam_mode;
    u32 maxexpotime;
    
    if (copy_from_user(&cam_mode, param, sizeof(cam_mode))) 
    {
        return -EFAULT;
    }

    maxexpotime = cam_mode.maxexpotime;
    if(maxexpotime == 0)
    {
        return -EFAULT;
    }

    switch (cam_mode.mode)
    {
        case V4l_NM_NIGHT:
        case V4l_NM_ACTION:
        case V4l_NM_AUTO:
            sensor2m_set_exposure_mode(cam_mode.mode, maxexpotime);
            break;
        default:
            return -EFAULT;
    }

    return 0;
}

static int pxa_cam_WCAM_VIDIOCGEXPOPARA(p_camera_context_t cam_ctx, void * param)
{
    struct V4l_EXPOSURE_PARA expo_para;

    if(copy_to_user(param, &expo_para, sizeof(expo_para))) 
    {
        return -EFAULT;
    } 

    return 0;
}

static int sensor2m_WCAM_VIDIOCSJPEGQUALITY(p_camera_context_t cam_ctx, void * param)
{
   return  sensor2m_set_sfact(cam_ctx->jpeg_quality);
}


static int sensor2m_WCAM_VIDIOCSMIRROR(p_camera_context_t cam_ctx, void * param)
{
    int mirror, rows, columns;
    mirror = (int)param;

    if(mirror & CAMERA_MIRROR_VERTICALLY)
        rows = 1;
    else
        rows = 0;

    if(mirror & CAMERA_MIRROR_HORIZONTALLY)
        columns = 1;
    else
        columns = 0;

    return  sensor2m_set_mirror(rows, columns);
}

int camera_func_sensor2m_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param)
{
   switch(cmd)
   {
     
    case WCAM_VIDIOCGCAMREG:
         return pxa_camera_WCAM_VIDIOCGCAMREG(cam_ctx, param);
   
    case WCAM_VIDIOCSCAMREG:
          return pxa_camera_WCAM_VIDIOCSCAMREG(cam_ctx, param);
           
    case WCAM_VIDIOCSSSIZE:
         return pxa_cam_WCAM_VIDIOCSSSIZE(cam_ctx, param);

    case WCAM_VIDIOCSOSIZE:
         return pxa_cam_WCAM_VIDIOCSOSIZE(cam_ctx, param);
            
    case WCAM_VIDIOCSFPS:
         return pxa_cam_WCAM_VIDIOCSFPS(cam_ctx, param);
                 
    case WCAM_VIDIOCSSTYLE:
         return pxa_cam_WCAM_VIDIOCSSTYLE(cam_ctx, param);    
         
    case WCAM_VIDIOCSLIGHT:
         return pxa_cam_WCAM_VIDIOCSLIGHT(cam_ctx, param);
      
    case WCAM_VIDIOCSBRIGHT:
         return pxa_cam_WCAM_VIDIOCSBRIGHT(cam_ctx, param);
    
    case WCAM_VIDIOCSFLICKER:
         return pxa_cam_WCAM_VIDIOCSFLICKER(cam_ctx, param);

    case WCAM_VIDIOCSNIGHTMODE:
         return pxa_cam_WCAM_VIDIOCSNIGHTMODE(cam_ctx, param);

    case WCAM_VIDIOCGEXPOPARA:
         return pxa_cam_WCAM_VIDIOCGEXPOPARA(cam_ctx, param);

    case WCAM_VIDIOCSMIRROR:
         return sensor2m_WCAM_VIDIOCSMIRROR(cam_ctx, param);

    case WCAM_VIDIOCSJPEGQUALITY:
         return sensor2m_WCAM_VIDIOCSJPEGQUALITY(cam_ctx, param);

    default:
         {
           err_print("Error cmd=%d", cmd);
           return -1;
         }
    }
    return 0;
 
}

int camera_func_sensor2m_camera4uvc_command(p_camera_context_t cam_ctx, unsigned int cmd, void *param)
{
   switch(cmd)
   {
    /*write registers*/
    case WCAM_VIDIOCSCAMREG:
	 if (  param == NULL )
		 return -1;
    	 sensor2m_reg_write((u16)((int*)param)[0], (u16)((int*)param)[1]);
         return 0;
        
     /*read registers*/
    case WCAM_VIDIOCGCAMREG:
	 if ( param == NULL)
		 return -1;
	 int reg_value = (int)sensor2m_reg_read((u16)((int*)param)[0]);
	 ((int*)param)[0] = reg_value;
         return 0;

    /*set video mode fps*/
    case WCAM_VIDIOCSFPS:
	 sensor2m_set_time(cam_ctx->fps);
	 return 0;
            
    /*set picture brightness*/
    case WCAM_VIDIOCSBRIGHT:
         return  sensor2m_set_bright( cam_ctx->capture_bright );
    
    /*set flicker frequency*/
    case WCAM_VIDIOCSFLICKER:
      	 return  sensor2m_set_flicker(cam_ctx->flicker_freq);

    default:
         {
           err_print("Error cmd=%d", cmd);
           return -1;
         }
    }
    return 0;
}

