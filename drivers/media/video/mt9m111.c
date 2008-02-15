 
/*================================================================================
                                                                               
                      Header Name: mt9m111.c

General Description: Camera module mt9m111 interface source file
 
==================================================================================
                     Motorola Confidential Proprietary
                 Advanced Technology and Software Operations
               (c) Copyright Motorola 1999, All Rights Reserved
 
Revision History:
                            Modification     Tracking
Author                 Date          Number     Description of Changes
----------------   ------------    ----------   -------------------------
Ma Zhiqiang          06/29/2004                 Change for auto-detect
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
#include "mt9m111.h"
#include "mt9m111_hw.h"

//#define LOG_TIME_STAMP   //If defined, the time stamp log will be printed out
//#define MT9M111_LOG      //If defined, the mt9m111_mt9m111_dbg_print logs will be printed out
//#define MT9M111_ECHO_CMD //If defined, the "echo {address, value} > cam" commands will be enabled.

#ifdef  MT9M111_LOG
#define mt9m111_dbg_print(fmt, args...) dbg_print(fmt, ##args)
#else
#define mt9m111_dbg_print(fmt, args...) ;
#endif

#define MCLK_DEFT           (24)         /* Default Master clock*/
#define MSCWR1_CAMERA_ON    (0x1 << 15)     /* Camera Interface Power Control */
#define MSCWR1_CAMERA_SEL   (0x1 << 14)     /* Camera Interface Mux control */

#define MAX_WIDTH     1280
#define MAX_HEIGHT    1024

#define MIN_WIDTH     40
#define MIN_HEIGHT    30

#define VIEW_FINDER_WIDTH_DEFT     320
#define VIEW_FINDER_HEIGHT_DEFT    240

#define FRAMERATE_DEFT    15
#define BUF_SIZE_DEFT     ((PAGE_ALIGN(MAX_WIDTH * MAX_HEIGHT) + (PAGE_ALIGN(MAX_WIDTH*MAX_HEIGHT/2)*2)))

extern int mt9m111_read(u16 addr, u16 *pvalue);
extern int mt9m111_write(u16 addr, u16 value);
extern int i2c_mt9m111_cleanup(void);
extern int i2c_mt9m111_init(void);

extern void stop_dma_transfer(p_camera_context_t camera_context);
extern int camera_ring_buf_init(p_camera_context_t camera_context);
extern void camera_gpio_init(void);
extern void camera_gpio_deinit(void);
extern void start_dma_transfer(p_camera_context_t camera_context, unsigned block_id);

/***********************************************************************
 *
 * MT9M111 Functions
 *
 ***********************************************************************/
int camera_func_mt9m111_init(p_camera_context_t);
int camera_func_mt9m111_deinit(p_camera_context_t);
int camera_func_mt9m111_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param);
int camera_func_mt9m111_set_capture_format(p_camera_context_t);
int camera_func_mt9m111_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_mt9m111_stop_capture(p_camera_context_t);
int camera_func_mt9m111_pm_management(p_camera_context_t, int);


camera_function_t  mt9m111_func = 
{
    init:                camera_func_mt9m111_init,
    deinit:              camera_func_mt9m111_deinit,
    command:             camera_func_mt9m111_docommand,
    set_capture_format:  camera_func_mt9m111_set_capture_format,
    start_capture:       camera_func_mt9m111_start_capture,
    stop_capture:        camera_func_mt9m111_stop_capture,
    pm_management:       camera_func_mt9m111_pm_management
};

int camera_func_mt9m111_init(  p_camera_context_t cam_ctx )
{
    u16 device_id = 0 ;

    // init context status
    cam_ctx->dma_channels[0] = 0xFF;
    cam_ctx->dma_channels[1] = 0xFF;
    cam_ctx->dma_channels[2] = 0xFF;
    
    cam_ctx->capture_width  = VIEW_FINDER_WIDTH_DEFT;
    cam_ctx->capture_height = VIEW_FINDER_HEIGHT_DEFT;
    
    cam_ctx->capture_input_format  = CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR;
    cam_ctx->capture_output_format = CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR;
    
    cam_ctx->frame_rate = cam_ctx->fps = FRAMERATE_DEFT;
    
    cam_ctx->mini_fps = FRAMERATE_DEFT;
    
    cam_ctx->mclk = MCLK_DEFT;
    
    cam_ctx->buf_size     = BUF_SIZE_DEFT;
    cam_ctx->dma_descriptors_size = (cam_ctx->buf_size/PAGE_SIZE + 10);
    strcpy (cam_ctx->vc.name, "Micron MT9M111");
    cam_ctx->vc.maxwidth  = MAX_WIDTH;
    cam_ctx->vc.maxheight = MAX_HEIGHT;
    cam_ctx->vc.minwidth  = MIN_WIDTH; 
    cam_ctx->vc.minheight = MIN_HEIGHT;

    camera_gpio_init();
    ci_init();
 
    // Configure CI according to MT9M111's hardware
    // master parallel with 8 data pins
    ci_set_mode(CI_MODE_MP, CI_DATA_WIDTH8); 

    // enable pixel clock(sensor will provide pclock) 
    ci_set_clock(cam_ctx->clk_reg_base, 1, 1, cam_ctx->mclk);

    // data sample on rising and h,vsync active high
    ci_set_polarity(1, 0, 0);
    
    // fifo control
    ci_set_fifo(0, CI_FIFO_THL_32, 1, 1); // quality

    // Turn on M_CLK and wait for 150 ms.
    ci_enable(1);
    //mdelay(150);

    i2c_mt9m111_init();

    // read out device id
    mt9m111_get_device_id(&device_id);
    if(device_id != 0x1419 && device_id != 0x1429 && device_id != 0x143A) 
    {
        //ci_disable(1);
        //camera_gpio_deinit();
        return -1;
    }
    else
    {
        mt9m111_dbg_print("Micron MT9M111 camera module detected!");
    }


    /* To resolve the vertical line in view finder issue (LIBff11930)
     * The solution is:
     * AP Kernel camera driver: set TCMM_EN to low when camera is running and TCMM_EN to high when camera stops.
     * BP Software: if TCMM_EN is low, BP do not shut off 26M clock ,but BP can sleep itself.*/ 

    set_GPIO_mode(99|GPIO_OUT);//It's GPIO99 for the "TCMM_EN"
    GPCR(99) = GPIO_bit(99);
    
    cam_ctx->sensor_type = CAMERA_TYPE_MT9M111;

    mt9m111_default_settings();
    
    mt9m111_dbg_print("mt9m111 init success!");

#ifdef MT9M111_ECHO_CMD
    /*for test commmand throught terminal*/
    struct proc_dir_entry *pw;
    static ssize_t test_command_write(struct file *file, const char *buf, size_t count, loff_t *pos);

    static struct file_operations test_command_funcs = 
    {
        read:NULL,
        write:test_command_write,
    };

    if ((pw = create_proc_entry ("cam", 0666, 0)) != NULL)
    {
        pw->proc_fops = &test_command_funcs;
    }
#endif
    return 0;
}
    
static void mt9m111_gpio_deinit(void)
{
    GPSR(CIF_PD) = GPIO_bit(CIF_PD);
}

int camera_func_mt9m111_deinit(  p_camera_context_t camera_context )
{
    mt9m111_write(0x1B3, 0);
    mdelay(5);

    i2c_mt9m111_cleanup();

    /* disable CI */
    ci_disable(1);
	
    mt9m111_gpio_deinit();

#ifdef MT9M111_ECHO_CMD
    /*for test commmand throught terminal*/
    remove_proc_entry ("cam", NULL);
#endif

    /* To resolve the vertical line in view finder issue (LIBff11930)
     * The solution is:
     * AP Kernel camera driver: set TCMM_EN to low when camera is running and TCMM_EN to high when camera stops.
     * BP Software: if TCMM_EN is low, BP do not shut off 26M clock ,but BP can sleep itself.*/ 

    set_GPIO_mode(99|GPIO_OUT);//It's GPIO99 for the "TCMM_EN"
    GPSR(99) = GPIO_bit(99);

    return 0;
}

int camera_func_mt9m111_set_capture_format(  p_camera_context_t camera_context )
{
    micron_window_size wsize;
    u16 micron_format;
    mt9m111_dbg_print("");

    // set sensor input/output window
    wsize.width = camera_context->capture_width;
    wsize.height = camera_context->capture_height;
    mt9m111_output_size(&wsize);

    // set sensor format
    switch(camera_context->capture_input_format) {
    case CAMERA_IMAGE_FORMAT_YCBCR422_PLANAR:
    case CAMERA_IMAGE_FORMAT_YCBCR422_PACKED:
        micron_format = O_FORMAT_422_YCbYCr;
        break;
    case CAMERA_IMAGE_FORMAT_RGB565:
        micron_format = O_FORMAT_565_RGB;
        break;
    case CAMERA_IMAGE_FORMAT_RGB555:
        micron_format = O_FORMAT_555_RGB;
        break;
    case CAMERA_IMAGE_FORMAT_RGB444:
        micron_format = O_FORMAT_444_RGB;
        break;
    default:
        micron_format = O_FORMAT_422_YCbYCr;
        break;
    }
    mt9m111_output_format(micron_format);

    return 0;
}

int camera_func_mt9m111_start_capture(  p_camera_context_t cam_ctx, unsigned int frames )
{
    int waitingFrame = 0;

#ifdef LOG_TIME_STAMP
    struct timeval tv0,tv1,tv2,tv3,tv4;
    do_gettimeofday(&tv0);
#endif

    ci_reset_fifo();
    ci_clear_int_status(0xFFFFFFFF);

    // frames=0 means video mode capture    
    if (frames == 0) 
    {
        mt9m111_dbg_print("video capture!"); 
        mt9m111_viewfinder_on();
    }
    else 
    {
        mt9m111_dbg_print("still capture");
        mt9m111_snapshot_trigger();
    }

#ifdef LOG_TIME_STAMP
    do_gettimeofday(&tv1);
#endif

    ci_disable(1);
    ci_enable(1);

#ifdef LOG_TIME_STAMP
    do_gettimeofday(&tv2);
#endif

    if(frames == 1) //Wait 1 frames to begin capture photo
    {
        waitingFrame = 1;
    } 
    else
    { 
        waitingFrame = 1;
    }

    while(waitingFrame--)
    {
        CISR |= (1<<4);
        while(!(CISR&(1<<4))); //Wait a SOF then begin start DMA
    }

#ifdef LOG_TIME_STAMP
    do_gettimeofday(&tv3);
#endif

    ci_disable(1);
    ci_enable(1);
    ci_reset_fifo();
    start_dma_transfer(cam_ctx, 0);
     
#ifdef LOG_TIME_STAMP
    do_gettimeofday(&tv4);

    printk("mt9m111 capture: Begin time is sec: %d , msec: %d\n", tv0.tv_sec, tv0.tv_usec/1000);
    printk("mt9m111 capture: End time is sec: %d , msec: %d\n", tv4.tv_sec, tv4.tv_usec/1000);
    if(frames)
        printk("mt9m111 capture: Total time(photo) is %d ms\n\n", (tv4.tv_sec-tv0.tv_sec)*1000+ (tv4.tv_usec-tv0.tv_usec)/1000);
    else
        printk("mt9m111 capture: Total time(preview) is %d ms\n\n", (tv4.tv_sec-tv0.tv_sec)*1000+ (tv4.tv_usec-tv0.tv_usec)/1000);
    printk("mt9m111 capture: Write reg time is %d ms\n\n", (tv1.tv_sec-tv0.tv_sec)*1000+ (tv1.tv_usec-tv0.tv_usec)/1000);
    printk("mt9m111 capture: Reset ci time is %d ms\n\n", (tv2.tv_sec-tv1.tv_sec)*1000+ (tv2.tv_usec-tv1.tv_usec)/1000);
    printk("mt9m111 capture: Wait sof time is %d ms\n\n", (tv3.tv_sec-tv2.tv_sec)*1000+ (tv3.tv_usec-tv2.tv_usec)/1000);
    printk("mt9m111 capture: Left time is %d ms\n\n", (tv4.tv_sec-tv3.tv_sec)*1000+ (tv4.tv_usec-tv3.tv_usec)/1000);
#endif
    return 0;
}

int camera_func_mt9m111_stop_capture(  p_camera_context_t camera_context )
{
    mt9m111_viewfinder_off();
    stop_dma_transfer(camera_context);

    /* disable CI */
    //ci_disable(1);

    return 0;
}

static int camera_mt9m111_restore_settings(p_camera_context_t cam_ctx)
{
    micron_window_size size;
    
    size.width  = cam_ctx->sensor_width;
    size.height = cam_ctx->sensor_height;
    mt9m111_input_size(&size);
      
    size.width  = cam_ctx->capture_width;
    size.height = cam_ctx->capture_height;
    mt9m111_output_size(&size);
      
    mt9m111_set_bright(cam_ctx->capture_bright);
   
    mt9m111_set_fps(cam_ctx->fps, cam_ctx->mini_fps);
    mt9m111_set_light(cam_ctx->capture_light);
    mt9m111_set_style(cam_ctx->capture_style);
    //mt9m111_set_contrast(cam_ctx->capture_contrast);
    mt9m111_set_autoexposure_zone(cam_ctx->mini_fps);
    mt9m111_set_flicker(cam_ctx->flicker_freq);
  
    return 0;
}

int camera_func_mt9m111_pm_management(p_camera_context_t cam_ctx, int suspend)
{
    static int resume_dma = 0;
    if(suspend)
    {
        if(cam_ctx != NULL )
        {
            if(cam_ctx->dma_started) 
            {
                mt9m111_dbg_print("camera running, suspended");
                stop_dma_transfer(cam_ctx);
                resume_dma = 1;
            }
        }

        disable_irq(IRQ_CAMERA);
        CKEN &= ~CKEN24_CAMERA;
    }
    else
    {
        CKEN |= CKEN24_CAMERA;
        enable_irq(IRQ_CAMERA);

        if(cam_ctx != NULL)
        {  
            mt9m111_dbg_print("camera running, resumed");
            camera_init(cam_ctx);

            camera_mt9m111_restore_settings(cam_ctx);
               
            if(resume_dma == 1)
            {
                camera_start_video_capture(cam_ctx, 0);
                resume_dma = 0;
            }
        }
    }
    return 0;
}

static int pxa_camera_WCAM_VIDIOCGCAMREG(p_camera_context_t cam_ctx, void * param)
{
    int reg_value, offset;
    mt9m111_dbg_print("WCAM_VIDIOCGCAMREG");
    if(copy_from_user(&offset, param, sizeof(int))) 
    {
        return -EFAULT;
    }
    reg_value = (int)mt9m111_reg_read((u16)offset);

    if(copy_to_user(param, &reg_value, sizeof(int))) 
    {
        return -EFAULT;
    } 

    return 0;
}
static int pxa_camera_WCAM_VIDIOCSCAMREG(p_camera_context_t cam_ctx, void * param)
{
    struct reg_set_s{int val1; int val2;} reg_s;
    mt9m111_dbg_print("WCAM_VIDIOCSCAMREG");

    if(copy_from_user(&reg_s, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    mt9m111_reg_write((u16)reg_s.val1, (u16)reg_s.val2);
    return 0;
} 
 
static int pxa_cam_WCAM_VIDIOCSFPS(p_camera_context_t cam_ctx, void * param)
{
    struct {int fps, minfps;} cam_fps;
    mt9m111_dbg_print("WCAM_VIDIOCSFPS");
    if(copy_from_user(&cam_fps, param, sizeof(int) * 2)) 
    {
        return  -EFAULT;
    }
    cam_ctx->fps = cam_fps.fps;
    cam_ctx->mini_fps = cam_fps.minfps;
    mt9m111_set_fps(cam_fps.fps, cam_fps.minfps);
    return 0;
}


/*Set  sensor size*/  
static int pxa_cam_WCAM_VIDIOCSSSIZE(p_camera_context_t cam_ctx, void * param)
{
  micron_window_size size;
  mt9m111_dbg_print("WCAM_VIDIOCSSSIZE");
  
  if(copy_from_user(&size, param, sizeof(micron_window_size))) 
  {
        return  -EFAULT;
  }
  
  size.width = (size.width+3)/4 * 4;
  size.height = (size.height+3)/4 * 4;
  cam_ctx->sensor_width = size.width;
  cam_ctx->sensor_height = size.height;
  mt9m111_input_size(&size);
  return 0;
}

//set  output size
static int pxa_cam_WCAM_VIDIOCSOSIZE(p_camera_context_t cam_ctx, void * param)
{
   micron_window_size size;
   CI_MP_TIMING     timing;
   mt9m111_dbg_print("WCAM_VIDIOCSOSIZE");
  
   if(copy_from_user(&size, param, sizeof(micron_window_size))) 
   {
        return  -EFAULT;
   }

   //make it in an even number
   size.width = (size.width+1)/2 * 2;
   size.height = (size.height+1)/2 * 2;
   mt9m111_output_size(&size);
   
   cam_ctx->capture_width  = size.width;
   cam_ctx->capture_height = size.height;
   timing.BFW = timing.BLW = 0;
   
   ci_configure_mp(cam_ctx->capture_width-1, cam_ctx->capture_height-1, &timing);
   camera_ring_buf_init(cam_ctx);
   
   return 0;
}
    
/*set picture style*/  
static int pxa_cam_WCAM_VIDIOCSSTYLE(p_camera_context_t cam_ctx, void * param)
{
  mt9m111_dbg_print("WCAM_VIDIOCSSTYLE");
  cam_ctx->capture_style = (V4l_PIC_STYLE)param;
  
  return mt9m111_set_style(cam_ctx->capture_style);
}

/*set picture light*/     
static int pxa_cam_WCAM_VIDIOCSLIGHT(p_camera_context_t cam_ctx, void * param)
{
   mt9m111_dbg_print("WCAM_VIDIOCSLIGHT");
   cam_ctx->capture_light = (V4l_PIC_WB)param;

   return  mt9m111_set_light((V4l_PIC_WB)param);
}
    
/*set picture brightness*/
static int pxa_cam_WCAM_VIDIOCSBRIGHT(p_camera_context_t cam_ctx, void * param)
{
   mt9m111_dbg_print("WCAM_VIDIOCSBRIGHT");
   cam_ctx->capture_bright = (int)param;

   return  mt9m111_set_bright((int)param);
}

/*set picture contrast*/
/*static int pxa_cam_WCAM_VIDIOCSCONTRAST(p_camera_context_t cam_ctx, void * param)
{
   mt9m111_dbg_print("WCAM_VIDIOCSCONTRAST");
   cam_ctx->capture_contrast = ((int)param-50)/12;

   return  mt9m111_set_contrast(cam_ctx->capture_contrast);
}*/

/*set flicker frequency*/
static int pxa_cam_WCAM_VIDIOCSFLICKER(p_camera_context_t cam_ctx, void * param)
{
   mt9m111_dbg_print("WCAM_VIDIOCSFLICKER");
   cam_ctx->flicker_freq = (int)param;

   return  mt9m111_set_flicker(cam_ctx->flicker_freq);
}


/*set night mode*/
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
            mt9m111_set_autoexposure_zone(cam_ctx->mini_fps);
            break;
        default:
            return -EFAULT;
    }

    return 0;
}

int camera_func_mt9m111_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param)
{
   switch(cmd)
   {
     /*read mt9m111 registers*/
    case WCAM_VIDIOCGCAMREG:
         return pxa_camera_WCAM_VIDIOCGCAMREG(cam_ctx, param);

    /*write mt9m111 registers*/
    case WCAM_VIDIOCSCAMREG:
          return pxa_camera_WCAM_VIDIOCSCAMREG(cam_ctx, param);
        
    /*set sensor size */  
    case WCAM_VIDIOCSSSIZE:
         return pxa_cam_WCAM_VIDIOCSSSIZE(cam_ctx, param);

    /*set output size*/
    case WCAM_VIDIOCSOSIZE:
         return pxa_cam_WCAM_VIDIOCSOSIZE(cam_ctx, param);
         
    /*set video mode fps*/
    case WCAM_VIDIOCSFPS:
         return pxa_cam_WCAM_VIDIOCSFPS(cam_ctx, param);
            
    /*set picture style*/  
    case WCAM_VIDIOCSSTYLE:
         return pxa_cam_WCAM_VIDIOCSSTYLE(cam_ctx, param);
         
    /*set picture light*/     
    case WCAM_VIDIOCSLIGHT:
         return pxa_cam_WCAM_VIDIOCSLIGHT(cam_ctx, param);
    
    /*set picture brightness*/
    case WCAM_VIDIOCSBRIGHT:
         return pxa_cam_WCAM_VIDIOCSBRIGHT(cam_ctx, param);
    
    /*set picture contrast*/
    //case WCAM_VIDIOCSCONTRAST:
    //     return pxa_cam_WCAM_VIDIOCSCONTRAST(cam_ctx, param);

    /*set flicker frequency*/
    case WCAM_VIDIOCSFLICKER:
         return pxa_cam_WCAM_VIDIOCSFLICKER(cam_ctx, param);

    case WCAM_VIDIOCSNIGHTMODE:
         return pxa_cam_WCAM_VIDIOCSNIGHTMODE(cam_ctx, param);

    default:
         {
           mt9m111_dbg_print("Error cmd=0x%x", cmd);
           return -1;
         }
    }
    return 0;
 
}

#ifdef MT9M111_ECHO_CMD
/***********************************************************************
 *
 * MT9M111 test command throught terminal
 *
 ***********************************************************************/

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_WRITE_LEN 12

extern int mt9m111_write(u16 addr, u16 value);

static ssize_t test_command_write (struct file *file, const char *buf, size_t count, loff_t * pos)
{
    char feedback[2*MAX_WRITE_LEN + 1] ={0};
    char feedback1[2*MAX_WRITE_LEN + 1] = {0};
    size_t n = count;
    size_t l;
    char c;
    int i,x,y,t;

    u16 reg, value;

    MOD_INC_USE_COUNT;

    if (n > 0)
    {
        l = MIN (n, MAX_WRITE_LEN);
        if (copy_from_user (feedback, buf, l))
        {
            count = -1;
        }
        else
        {
            if (l > 0 && feedback[l - 1] == '\n')
            {
                l -= 1;
            }
            feedback[l] = 0;
            n -= l;

            while (n > 0)
            {
                if (copy_from_user (&c, buf + (count - n), 1))
                {
                    count = -1;
                    break;
                }
                n -= 1;
            }
        }
    }

    i = x = y = 1;
  
    if (count > 0 && feedback[0] == '{')
    {
        while(feedback[i] != '}' && i<=MAX_WRITE_LEN*2)
        {
            c= feedback[i];
            i++; 
            if(c>='a' && c<='f')
            {
                c -= 0x20;
            }
            if(!((c>='0'&& c<='9')||(c>='A'&&c<='F')||c==','))
            {
                continue;
            }
            feedback1[x++] = c;
        }

        feedback1[x]='}';
        feedback1[0]='{';
    
        for(i=1;i<=x;i++)
        {
            if(feedback1[i] == ',')
            {
                y=3;
                for(t=i-1;t>=1;t--)
                {
                    if(y==0)
                        break;
                    feedback[y--]=feedback1[t];
                }
                if(y>=1)
                {
                    for(t=y;t>=1;t--)
                    {
                        feedback[t] = '0';
                    }
                }
            }
        
            if(feedback1[i] == '}')
            {
                y=8;
                for(t=i-1;feedback1[t]!=','&&t>=1;t--)
                {
                    if(y==4)
                        break;
                    feedback[y--]=feedback1[t];
                }
                if(y>=5)
                {
                    for(t=y;t>=5;t--)
                    {
                        feedback[t] = '0';
                    }
                }
            }
        }

        reg = 16 * 16 * (feedback[1]>='A'?feedback[1]-'A'+10:feedback[1]-0x30) + 16 *  (feedback[2]>='A'?feedback[2]-'A'+10:feedback[2]-0x30) + (feedback[3]>='A'?feedback[3]-'A'+10:feedback[3]-0x30) ;
        value = 16 * 16 * 16 *  (feedback[5]>='A'?feedback[5]-'A'+10:feedback[5]-0x30) + 16 *16*  (feedback[6]>='A'?feedback[6]-'A'+10:feedback[6]-0x30) + 16 *  (feedback[7]>='A'?feedback[7]-'A'+10:feedback[7]-0x30) + (feedback[8]>='A'?feedback[8]-'A'+10:feedback[8]-0x30) ;

        if(reg == 0xFFF && value == 0xFFFF)
        {
            mt9m111_dump_register(0,0x2ff,NULL);
        }
        else if(reg == 0xFFF || reg == 0xFF)
        {
             mt9m111_reg_read(value);
        }
        else
        {
            mt9m111_write(reg, value); 
        }
    }

    MOD_DEC_USE_COUNT;

    return (count);
}
#endif
