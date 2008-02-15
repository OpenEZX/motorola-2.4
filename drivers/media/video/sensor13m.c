/*
 *  sensor13m.c
 *
 *  Copyright (C) 2004-2006 Motorola Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *

Revision History:
  Modification   
      Date        Description of Changes
  ------------   -------------------------
   06/29/2004     Change for auto-detect
   04/12/2006     Fix sensor name

*/

/*================================================================================
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
#include "sensor13m.h"
#include "sensor13m_hw.h"

#define MSCWR1_CAMERA_ON    (0x1 << 15)     
#define MSCWR1_CAMERA_SEL   (0x1 << 14)     

#define MAX_WIDTH     MAX_SENSOR_B_WIDTH
#define MAX_HEIGHT    MAX_SENSOR_B_HEIGHT

#define MIN_WIDTH     40
#define MIN_HEIGHT    30

#define VIEW_FINDER_WIDTH_DEFT     DEFT_SENSOR_A_WIDTH
#define VIEW_FINDER_HEIGHT_DEFT    DEFT_SENSOR_A_HEIGHT

#define BUF_SIZE_DEFT     ((PAGE_ALIGN(MAX_WIDTH * MAX_HEIGHT) + (PAGE_ALIGN(MAX_WIDTH*MAX_HEIGHT/2)*2)))

static unsigned int mclk_out_hz;

extern int i2c_sensor13m_cleanup(void);
extern int i2c_sensor13m_init(void);

/***********************************************************************
 *
 * sensor13m Functions
 *
 ***********************************************************************/
int camera_func_sensor13m_init(p_camera_context_t);
int camera_func_sensor13m_deinit(p_camera_context_t);
int camera_func_sensor13m_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param);
int camera_func_sensor13m_set_capture_format(p_camera_context_t);
int camera_func_sensor13m_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_sensor13m_stop_capture(p_camera_context_t);
int camera_func_sensor13m_pm_management(p_camera_context_t, int);


camera_function_t  sensor13m_func = 
{
    init:                camera_func_sensor13m_init,
    deinit:              camera_func_sensor13m_deinit,
    command:             camera_func_sensor13m_docommand,
    set_capture_format:  camera_func_sensor13m_set_capture_format,
    start_capture:       camera_func_sensor13m_start_capture,
    stop_capture:        camera_func_sensor13m_stop_capture,
    pm_management:       camera_func_sensor13m_pm_management
};

static void sensor13m_init(int dma_en)
{
    camera_gpio_init();    

    GPCR(GPIO_CAM_EN)  = GPIO_bit(GPIO_CAM_EN);    
    GPCR(GPIO_CAM_RST) = GPIO_bit(GPIO_CAM_RST);   

    PGSR(GPIO_CAM_EN) |= GPIO_bit(GPIO_CAM_EN);    
    PGSR(GPIO_CAM_RST)|= GPIO_bit(GPIO_CAM_RST);   
        
    ci_enable(dma_en);
    udelay(100);            
    GPSR(GPIO_CAM_RST) = GPIO_bit(GPIO_CAM_RST);   
    udelay(1);              
}

static void sensor13m_hw_deinit(void)
{
    GPSR(GPIO_CAM_EN) = GPIO_bit(GPIO_CAM_EN);      
    udelay(2);
    ci_disable(1);
}

static void sensor13m_deinit(void)
{
    sensor13m_reg_write(0x1B3, 0);
    mdelay(5);
    i2c_sensor13m_cleanup();
    sensor13m_hw_deinit();
}

int camera_func_sensor13m_standby(void)
{
    int ret;
    ci_init();
    ci_set_clock(0, 0, 1, 26);
    sensor13m_init(0);
    ret = i2c_sensor13m_init();
    if(ret < 0)
    {   
        err_print("error: i2c initialize fail!");
        sensor13m_hw_deinit();
        ci_deinit();
        return ret;
    }

    udelay(10);
    sensor13m_reg_write(0x1B3, 0);
    mdelay(5);
    i2c_sensor13m_cleanup();
    GPSR(GPIO_CAM_EN) = GPIO_bit(GPIO_CAM_EN);      
    udelay(2);
    ci_disable(1);
    ci_deinit();
    return 0;
}

int camera_func_sensor13m_init(  p_camera_context_t cam_ctx )
{
    int ret;

    cam_ctx->sensor_width  = MAX_WIDTH;
    cam_ctx->sensor_height = MAX_HEIGHT;
    cam_ctx->capture_width  = VIEW_FINDER_WIDTH_DEFT;
    cam_ctx->capture_height = VIEW_FINDER_HEIGHT_DEFT;
    cam_ctx->still_width  = MAX_WIDTH;
    cam_ctx->still_height = MAX_HEIGHT;
    cam_ctx->frame_rate = cam_ctx->fps = 15;
    cam_ctx->mini_fps = 15;
    cam_ctx->buf_size     = BUF_SIZE_DEFT;
    cam_ctx->dma_descriptors_size = (cam_ctx->buf_size/PAGE_SIZE + 10);
    strcpy (cam_ctx->vc.name, "1.3MP Sensor");
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
    sensor13m_init(1);
    ret = i2c_sensor13m_init();

	if(ret < 0)
    {   
        err_print("error: i2c initialize fail!");
        sensor13m_hw_deinit();
        return ret;
    }

    cam_ctx->sensor_type = CAMERA_TYPE_SENSOR13M;
    sensor13m_default_settings();
    return 0;
}

int camera_func_sensor13m_deinit(  p_camera_context_t camera_context )
{
    sensor13m_deinit();
    return 0;
}

int camera_func_sensor13m_set_capture_format(  p_camera_context_t camera_context )
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
        if(swidth/2 < camera_context->capture_width)
        {
            swidth = camera_context->capture_width * 2;
            azoom = camera_context->sensor_width * CAMERA_ZOOM_LEVEL_MULTIPLE / swidth;
            camera_context->capture_digital_zoom = azoom;
        }
        if(sheight/2 < camera_context->capture_height)
        {
            sheight = camera_context->capture_height * 2;
        }
    }

    wsize.width = swidth;
    wsize.height = sheight;
    sensor13m_sensor_size(&wsize);
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
    sensor13m_capture_sensor_size(&wsize);
    wsize.width = camera_context->capture_width;
    wsize.height = camera_context->capture_height;
    sensor13m_output_size(&wsize);
    wsize.width = camera_context->still_width;
    wsize.height = camera_context->still_height;
    sensor13m_capture_size(&wsize);

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
    default:
        sensor_format = 0;
        break;
    }
    sensor13m_output_format(sensor_format);

	return 0;
}

int camera_func_sensor13m_start_capture(  p_camera_context_t cam_ctx, unsigned int frames )
{
    int waitingFrame = 0;
    ci_clear_int_status(0xFFFFFFFF);

    if (frames == 0) 
    {
          sensor13m_viewfinder_on();
    }
    else 
    {
          sensor13m_snapshot_trigger();
    }

    if(frames == 1) 
    {
        waitingFrame = 1;
    } 
    else
    { 
        waitingFrame = 1;
    }
    camera_skip_frame(cam_ctx, waitingFrame);
    return 0;
}

int camera_func_sensor13m_stop_capture(  p_camera_context_t camera_context )
{
    sensor13m_viewfinder_off();
    stop_dma_transfer(camera_context);

    return 0;
}

int camera_func_sensor13m_pm_management(p_camera_context_t cam_ctx, int suspend)
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
    reg_value = (int)sensor13m_reg_read((u16)offset);

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
    sensor13m_reg_write((u16)reg_s.val1, (u16)reg_s.val2);
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
    sensor13m_set_fps(cam_fps.fps, cam_fps.minfps);
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
  
  return sensor13m_set_style(cam_ctx->capture_style);
}

static int pxa_cam_WCAM_VIDIOCSLIGHT(p_camera_context_t cam_ctx, void * param)
{
   cam_ctx->capture_light = (V4l_PIC_WB)param;

   return  sensor13m_set_light((V4l_PIC_WB)param);
}
    
static int pxa_cam_WCAM_VIDIOCSBRIGHT(p_camera_context_t cam_ctx, void * param)
{
   cam_ctx->capture_bright = (int)param;

   return  sensor13m_param15((int)param);
}

static int pxa_cam_WCAM_VIDIOCSFLICKER(p_camera_context_t cam_ctx, void * param)
{
   cam_ctx->flicker_freq = (int)param;

   return  sensor13m_param14(cam_ctx->flicker_freq);
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
            cam_ctx->mini_fps = (1000000+maxexpotime/2)/maxexpotime;
            sensor13m_param12(cam_ctx->mini_fps);
            break;
        default:
            return -EFAULT;
    }

    return 0;
}

static int sensor13m_get_exposure_para(struct V4l_EXPOSURE_PARA *expo_para);

static int pxa_cam_WCAM_VIDIOCGEXPOPARA(p_camera_context_t cam_ctx, void * param)
{
    struct V4l_EXPOSURE_PARA expo_para;

    sensor13m_get_exposure_para(&expo_para);

    if(copy_to_user(param, &expo_para, sizeof(expo_para))) 
    {
        return -EFAULT;
    } 

    return 0;
}

static int sensor13m_WCAM_VIDIOCSMIRROR(p_camera_context_t cam_ctx, void * param)
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

    return  sensor13m_param16(rows, columns);
}

static int sensor13m_WCAM_VIDIOCSSTROBEFLASH(p_camera_context_t cam_ctx, void * param)
{
    int strobe = (int)param;
    
    return  sensor13m_param17(strobe);
}

int camera_func_sensor13m_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param)
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
    return sensor13m_WCAM_VIDIOCSMIRROR(cam_ctx, param);

    case WCAM_VIDIOCSSTROBEFLASH:
    return sensor13m_WCAM_VIDIOCSSTROBEFLASH(cam_ctx, param);

    default:
         {
           err_print("Error cmd=%d", cmd);
           return -1;
         }
    }
    return 0;
 
}

static unsigned int sensor13m_get_param1(void)
{
    u16 r22e, r24c;
    unsigned int par1;    

    r22e = sensor13m_reg_read(0x22e);     
    r24c = sensor13m_reg_read(0x24c);     

    par1 = r24c / 0x4a;                

    return par1;          
}

static unsigned int sensor13m_get_param2(void)
{
    u16 r02f, r041, r23f, r262;
    unsigned int go1, go2, go3, go4 = 1; 

    r02f = sensor13m_reg_read(0x02f);   
    r041 = sensor13m_reg_read(0x041);   
    r23f = sensor13m_reg_read(0x23f);   
    r262 = sensor13m_reg_read(0x262);   

    go1 = r02f & 0x007f;             
    if(r02f & 0x0080)
        go1 *= 2;                    
    if(r02f & 0x0100)
        go1 *= 2;                    
    if(r02f & 0x0200)
        go1 *= 2;                    
    if(r02f & 0x0400)
        go1 *= 2;                    

    go2 = (r262&0x00ff) * ((r262&0xff00)/256);    

    go3 = 16;   

   go4 = (go1*go2/32) * go3/16;

    if(go4<64)
        go4 = 64;
    return go4;
}


static unsigned int sensor13m_get_param3(void)
{
    u16 r021;
    u16 r004, r003, r007, r008;
    u16 r009, r00c;
    unsigned int a, q, par2, par3;   
    unsigned int pm = 2;     
    unsigned int s = 1;          

    r021 = sensor13m_reg_read(0x021);     
    if(r021 & 0x0400)
        pm = 4; 
    if(r021 & 0x000C)
        s = 2;  

    r004 = sensor13m_reg_read(0x004);     
    r003 = sensor13m_reg_read(0x003);     
    r007 = sensor13m_reg_read(0x007);     
    r008 = sensor13m_reg_read(0x008);     

    r009 = sensor13m_reg_read(0x009);     
    r00c = sensor13m_reg_read(0x00C);     

    a = r004/s+8; 
    q = r007;
    par2 = a+q;   
    par3 = par2 * r009 * pm * 1000 / (mclk_out_hz/1000); 
    
    if(par3==0)        
        par3 = 1;
    return par3;
}

static int sensor13m_get_param4(int *lint, int *lual, 
                                struct V4l_EXPOSURE_PARA *expo_para)
{
    unsigned int go5, go7, go6;
    unsigned int lin, lua;

    go5 = sensor13m_get_param3();
    go7 = sensor13m_get_param2();
    go6 = sensor13m_get_param1();

    lua =  0;
    if(go5<4)
    {   
        unsigned int a1;
        a1 = 65 * (1000000 / 256) / go5;
        lin = (go6 * a1 / go7) * 256;
    }
    else if(go5<16)
    {   
        unsigned int a1;
        a1 = 65 * (1000000 / 64) / go5;
        lin = (go6 * a1 / go7) * 64;
    }
    else if(go5<1024)
    {   
        unsigned int a1;
        a1 = 65 * (1000000 / 16) / go5;
        lin = go6 * a1 / (go7/16);
    }
    else if(go5<131072) {
        
        unsigned int a1;
        a1 = 65 * 1000000 * 4 / go5;
        lin =  go6 * a1 / 4 / go7;
        if(lin<50)
        {
            unsigned int a2;
            a2 = 1000 / 4 * go6 * a1 / go7;
            lua = a2 - lin*1000;
        }
    }
    else {
        
        unsigned int a1;
        a1 = 65 * 1000000 / (go5/500);
        lin =  go6 * a1 / 500 / go7;
        if(lin<50)
        {
            unsigned int a2;
            a2 = 1000 / 500 * go6 * a1 / go7;
            lua = a2 - lin*1000;
        }
    }

    if(lint)
        *lint = lin;
    if(lual)
        *lual = lua;
    if(expo_para) {
        expo_para->luminance = lin;
        expo_para->shutter = go5;
        expo_para->ISOSpeed = go7;    
    }
    return lin;
}

static int sensor13m_get_exposure_para(struct V4l_EXPOSURE_PARA *expo_para)
{
    return sensor13m_get_param4(NULL, NULL, expo_para);
}


static unsigned int start_frameno = 0;   

static int sensor13m_param5(void)
{
    sensor13m_reg_write(0x00D, 0x0029);   
    sensor13m_reg_write(0x00D, 0x0008);   
    sensor13m_reg_write(0x106, 0x7010);
    u16 r22f = sensor13m_reg_read(0x22F);
    sensor13m_reg_write(0x22F, (r22f&0xfe20) | 0x100);    
    sensor13m_reg_write(0x1A7, 0x0140); 
    sensor13m_reg_write(0x1AA, 0x0100); 
    sensor13m_reg_write(0x1A6, 0x0280); 
    sensor13m_reg_write(0x1A9, 0x0200); 
    sensor13m_reg_write(0x1A5, 0x46c0); 
    sensor13m_reg_write(0x1A8, 0x4700); 
    u16 r00d = 0;
    r00d = sensor13m_reg_read(0x00D);
    sensor13m_reg_write(0x00D, 0x8000|r00d); 
    sensor13m_reg_write(0x003, 0x0200); 
    sensor13m_reg_write(0x004, 0x0280); 
    sensor13m_reg_write(0x001, 0x010C); 
    sensor13m_reg_write(0x002, 0x015C); 
    sensor13m_reg_write(0x007, 0x0061); 
    sensor13m_reg_write(0x008, 0x0011); 
    sensor13m_reg_write(0x00D, r00d); 
    sensor13m_reg_write(0x021, 0x040C); 
    sensor13m_reg_write(0x009, 256);      
    sensor13m_reg_write(0x237, 0x0200);   
    sensor13m_reg_write(0x257, 256);      
    sensor13m_reg_write(0x258, 306);      
    sensor13m_reg_write(0x23D, 0x17DD);   
    start_frameno = sensor13m_reg_read(0x19A);    

    return 0;
}
int sensor13m_param6(int *lint, int *lual)
{
    int ram, count;
    unsigned int mclk_hz;

    count = 400;
    ram= sensor13m_reg_read(0x19A);    
    while(ram < start_frameno+8 && --count>0)
    {
        if(mclk_out_hz!=(mclk_hz=ci_get_clock()))
        {
           return -EIO;
        }
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(2);
        ram = sensor13m_reg_read(0x19A);    
    }
	if(mclk_out_hz!=(mclk_hz=ci_get_clock()))
	{
        return -EIO;
	}

    return sensor13m_get_param4(lint, lual, NULL);
}

int sensor13m_param7(void)
{
    int ret;
    ci_init();
 
    mclk_out_hz = ci_set_clock(0, 0, 1, 52);
    if(mclk_out_hz != 52*1000000)
    {
        err_print("error: ci clock is not correct!");
    }

    sensor13m_init(0);
    ret = i2c_sensor13m_init();

    if(ret < 0)
    {   
        sensor13m_hw_deinit();
        return ret;
    }

    sensor13m_param5();
    return 0;
}
    
int sensor13m_param8(void)
{
    sensor13m_deinit();
    ci_deinit();
    return 0;
}

