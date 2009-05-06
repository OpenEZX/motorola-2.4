/*
 *  sensor13m_hw.c
 *
 *  Camera Module driver.
 *
 *  Copyright (C) 2004-2006 Motorola Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
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
    6/30/2004      Change for auto-detect
    4/12/2006      Fix sensor name

*/

/*================================================================================
                                 INCLUDE FILES
==================================================================================*/
#include <linux/types.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/dma.h>
#include <asm/irq.h>

#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/wrapper.h>
#include <linux/delay.h>
#include <linux/i2c.h> 

#include "sensor13m_hw.h"
#include "camera.h"


extern int sensor13m_read(u16 addr, u16 *pvalue);
extern int sensor13m_write(u16 addr, u16 value);

static u16 sensorAWidth = MAX_SENSOR_A_WIDTH;
static u16 sensorAHeight = MAX_SENSOR_A_HEIGHT;
static u16 sensorBWidth = MAX_SENSOR_B_WIDTH;
static u16 sensorBHeight = MAX_SENSOR_B_HEIGHT;
static u16 outWidth = DEFT_SENSOR_A_WIDTH;
static u16 outHeight = DEFT_SENSOR_A_HEIGHT;
static u16 captureWidth = DEFT_SENSOR_B_WIDTH;
static u16 captureHeight = DEFT_SENSOR_B_HEIGHT;
static u16 fask = 0;
static int imse = 1;
static int dsap = 0;
static u16 minx = -1;
static u16 maxx = -1;
static u8 ishis = 0;

#ifdef  CONFIG_CAMERA_ROTATE_180
static const int rotate_mirror_rows = 1;
static const int rotate_mirror_columns = 1;
#else
static const int rotate_mirror_rows = 0;
static const int rotate_mirror_columns = 0;
#endif

static int mr;
static int mcal;
static int sflat;

const static u16 autoLight[] =
{ 0x00EE, 0x3923, 0x0724, 0x00CF, 0x004F, 0x0006, 0x004D, 0x00EC,
  0x005B, 0x0018, 0x0080, 0x00ED, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x684F, 0x3D29,
  0x0002 };

const static u16 incandescentLight[] =
{ 0x00EE, 0x3923, 0x0724, 0x00CF, 0x004F, 0x0006, 0x004D, 0x00EC,
  0x005B, 0x0018, 0x0080, 0x00ED, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x863B, 0x0000,
  0x0000 };

const static u16 tl84Light[] =
{ 0x00EE, 0x3923, 0x0724, 0x00CF, 0x004F, 0x0006, 0x004D, 0x00EC,
  0x005B, 0x0018, 0x0080, 0x00ED, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x753A, 0x0000,
  0x0000 };

const static u16 d65Light[] =
{ 0x00EE, 0x3923, 0x0724, 0x00CF, 0x004F, 0x0006, 0x004D, 0x00EC,
  0x005B, 0x0018, 0x0080, 0x00ED, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x4A63, 0x0000,
  0x0000 };

const static u16 cloudyLight[] =
{ 0x00EE, 0x3923, 0x0724, 0x00CF, 0x004F, 0x0006, 0x004D, 0x00EC,
  0x005B, 0x0018, 0x0080, 0x00ED, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x5358, 0x0000,
  0x0000 };


static V4l_PIC_WB       g_light=-1; 
static int              gewa = 0 ;
static int              gfar = 0 ;
static int              gseat = 0 ;
static unsigned long    g_ulAWBJiffies ;
static unsigned long    g_ulFlashJiffies=0;

inline void             DisableAWB( void ) ;
inline void             sensor13m_param9( void ) ;
inline void             sensor13m_param10( void ) ; 
inline void             sensor13m_param11( void ) ;

inline void sensor13m_param11()
{
    sensor13m_write(0x106, 0x708E);   

    DisableAWB() ;
    g_ulAWBJiffies = jiffies ;
    sensor13m_param9() ;
    sensor13m_param10() ;
}

inline void DisableAWB( void )
{
    u16 v106 ;
    v106 = sensor13m_reg_read(0x106);
    v106 = v106 & ~0x0002;
    sensor13m_write(0x106, v106);     
    gewa = 1 ;
}

inline void sensor13m_param9()
{
    sensor13m_write(0x25B, 0x0001);  
}

inline void sensor13m_param10()
{
    u16 tea = ((120/15) << 5) & 0x03E0;
    sensor13m_write(0x237, tea);
    gseat = 1 ;
}

/*****************************************************************************
*
*****************************************************************************/
static void sensor13m_reset(void)
{
    minx = -1;
    maxx = -1;
    imse = 1;
    dsap = 0;
    ishis = 0;  
    mr = rotate_mirror_rows;
    mcal = rotate_mirror_columns;
    sflat = 0;

    g_light = -1 ; 
    gewa = 0 ;
    gfar = 0 ;
    gseat = 0 ;
    g_ulFlashJiffies=0;
}

/*****************************************************************************
*									     *
*        I2C Management		 					     *
*									     *
*****************************************************************************/
u16 sensor13m_reg_read(u16 reg_addr)
{
    u16 value=0;
    sensor13m_read(reg_addr, &value);
    return value;
}

void sensor13m_reg_write(u16 reg_addr, u16 reg_value)
{
    sensor13m_write(reg_addr, reg_value);
}

static void sensor13m_reg_write_020(u16 readmode)
{
    readmode = (readmode & 0xfffc) | mr | (mcal<<1);
    sensor13m_write(0x020, readmode);
}

/////////////////////////////////////////////////////////////////////////////////////
//   
//  Programming Guide : Configuration Methods 
//
/////////////////////////////////////////////////////////////////////////////////////

int sensor13m_viewfinder_on()
{
    if((outWidth*2 > sensorAWidth) || (outHeight*2 > sensorAHeight))
    {
	    if (imse || dsap)
	    {
            
	        sensor13m_write(0x2D2, 0x0041);	
    	    sensor13m_write(0x2CC, 0x4);	
    	    u16 v = sensor13m_reg_read(0x2CB);
    	    sensor13m_write(0x2CB, v|1);	
    	    dsap = 0;
	        imse = 0;
	        int count = 0;
	        while ((sensor13m_reg_read(0x00C8) != 0x000B) && (++count < 20))
	        {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
            }
        }
    }
    else
    {

	    if (!imse || dsap)
	    { 
    	    sensor13m_write(0x2D2, 0x0000);	
    	    sensor13m_write(0x2CC, 0x4);	
    	    u16 v = sensor13m_reg_read(0x2CB);	
    	    sensor13m_write(0x2CB, v|1);		
	        imse = 1;
    	    dsap = 0;
	    }
    }

    return 0;
}

int sensor13m_viewfinder_off()
{
    return 0;
}

int sensor13m_snapshot_trigger()
{
    unsigned long diff ;

	sensor13m_write(0x2D2, 0x007F);		
#ifdef CONFIG_CAMERA_STROBE_FLASH
    if(sflat) {
       
        if (0!=g_ulFlashJiffies)
        {
            diff=jiffies-g_ulFlashJiffies ;
            if (diff<500){
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(50-diff/10);
            }
        }

        sensor13m_write(0x2CC, 0x0001);
        g_ulFlashJiffies = jiffies ;
    }
    else {
        sensor13m_write(0x2CC, 0x0004);
    }
#else

    sensor13m_write(0x2CC, 0x0004);
#endif

    u16 v = sensor13m_reg_read(0x2CB);		
    sensor13m_write(0x2CB, v | 1);	        
    dsap = 1;

    return 0;
}

int sensor13m_snapshot_complete()
{
    return 0;
}

int sensor13m_sensor_size(sensor_window_size * window)
{
    sensorAWidth = window->width;
    sensorAHeight = window->height;

    sensor13m_write(0x1A6, sensorAWidth);
    sensor13m_write(0x1A9, sensorAHeight);

    return 0;
}

int sensor13m_capture_sensor_size(sensor_window_size * window)
{
    sensorBWidth = window->width;
    sensorBHeight = window->height;

    sensor13m_write(0x1A0, sensorBWidth);
    sensor13m_write(0x1A3, sensorBHeight);

    return 0;
}

int sensor13m_output_size(sensor_window_size * window)
{
    outWidth = window->width;
    outHeight = window->height;
    
    sensor13m_write(0x1A7, outWidth);
    sensor13m_write(0x1AA, outHeight);

    return 0;
}

int sensor13m_capture_size(sensor_window_size * window)
{
    captureWidth = window->width;
    captureHeight = window->height;
    
    sensor13m_write(0x1A1, captureWidth);
    sensor13m_write(0x1A4, captureHeight);

    return 0;
}

int sensor13m_set_fps(u16 newMaxFPS, u16 newMinFPS)
{

    newMaxFPS = (newMaxFPS > 15) ? 15 : newMaxFPS;
    newMaxFPS = (newMaxFPS < 5) ? 5 : newMaxFPS;

    if (newMaxFPS != maxx)
    {

        const u16 blop[] = 
        { 
            1123,	
            782, 	
            627, 	
            474, 	
            364, 	
            284, 	
            207, 	
            166, 	
            118, 	
            71, 	
            18   	
        };

        const u16 blip[] = 
        { 
            611, 	
            270, 	
            115,    
            5, 	    
            5, 	    
            5, 	    
            5, 	    
            5, 	    
            5, 	    
            5, 	    
            5 		
        };

        sensor13m_write(0x005, 260);
        sensor13m_write(0x006, blip[newMaxFPS-5]);
        sensor13m_write(0x007, 126);
        sensor13m_write(0x008, blop[newMaxFPS-5]);
        sensor13m_reg_write_020(0x0300);
        sensor13m_write(0x021, 0x040C);
        sensor13m_write(0x239, 1548);
        sensor13m_write(0x23A, 1548);
        sensor13m_write(0x23B, 1548);
        sensor13m_write(0x23C, 1548);
        sensor13m_write(0x257, 280);
        sensor13m_write(0x258, 336);
        sensor13m_write(0x259, 280);
        sensor13m_write(0x25A, 336);
        sensor13m_write(0x25C, 0x120D);
        sensor13m_write(0x25D, 0x1712);
        sensor13m_write(0x264, 0x0D1C);
 
        maxx = newMaxFPS;
    }

    return 0;

}

int sensor13m_param12(u16 newMinFPS)
{
    if ( newMinFPS == minx ) {
        return 0 ;
    }

    if ( gseat ) 
    {
        gseat = 0 ;

        if ( 15 == newMinFPS )
        {
            return 0 ;
        }
    }
    
    if (newMinFPS > 5)
    {
        newMinFPS = 15;
    }
    else
    {
        newMinFPS = 5;
    }

    if (newMinFPS != minx)
    {
        if (newMinFPS == 5)
        {
            u16 tea = ((120/5) << 5) & 0x03E0;
            sensor13m_write(0x237, tea);

            if (sensor13m_reg_read(0x23F) == 8)
            {
                u16 v;
                v = sensor13m_reg_read(0x22E);	
                sensor13m_write(0x22E, 0x0000);	
                int count=0;
                while ((sensor13m_reg_read(0x23F) == 8) && (++count < 30))
                {
                    set_current_state(TASK_INTERRUPTIBLE);
                    schedule_timeout(5);
                }

                sensor13m_write(0x22E, v);	
            }
        }
        else
        {
            u16 tea = ((120/15) << 5) & 0x03E0;
            sensor13m_write(0x237, tea);
            
            if (sensor13m_reg_read(0x23F) > 8)
            {
                u16 v;
                v = sensor13m_reg_read(0x22E);	
                sensor13m_write(0x22E, 0x0000);	
                int count=0;
                while ((sensor13m_reg_read(0x23F) > 8) && (++count < 30))
                {
                    set_current_state(TASK_INTERRUPTIBLE);
                    schedule_timeout(5);

                }

                sensor13m_write(0x22E, v);	
            }
        }

        minx = newMinFPS;
    }

    return 0;
}

int sensor13m_output_format(u16 format)
{
    u16 value;

    if(format == 0)
    {
        sensor13m_write(0x13A, 0);
    }
    else
    {
        value = (1<<8)|((format-1)<<6);
        sensor13m_write(0x13A, value);
    }
    return 0;
}

int sensor13m_param13(void)
{

    int count=0;
    while(((sensor13m_reg_read(0x25B) & 0x8000) != fask) && (++count < 50))
    {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(2);
    }

    sensor13m_write(0x25B, 0x0002);	

    return 0;
}

int sensor13m_param14(int fice)
{
   
    if ( gfar == fice ) {
        return 0 ;
    }

    fask = (fice == 50) ? 0x0000 : 0x8000;

    if (fice == 50)
    {
        if ( 60 == gfar ) {
            sensor13m_write(0x25B, 0x0001);   
        }
    }
    else if (fice == 60)
    {
        sensor13m_write(0x25B, 0x0003);   
    }
    else
    { 
        return -2;
    }

    sensor13m_param13();
    gfar = fice ;
    return 0;
}

int sensor13m_set_contrast(int contrast)
{
    return 0;
}

int sensor13m_set_style(V4l_PIC_STYLE style)
{
  switch(style)
  {
  case V4l_STYLE_BLACK_WHITE:
       sensor13m_write(0x1E2,0x7001);
       break;
  case V4l_STYLE_SEPIA:
       sensor13m_write(0x1E2,0x7002);
       sensor13m_write(0x1E3,0xB023);
       break;
  case V4l_STYLE_SOLARIZE:
       sensor13m_write(0x1E2,0x7004);
       break;
  case V4l_STYLE_NEG_ART:
       sensor13m_write(0x1E2,0x7003);
       break;
  case V4l_STYLE_BLUISH:
        sensor13m_write( 0x1E2 , 0x7002 ) ;
        sensor13m_write( 0x1E3 , 0x40a0 ) ;
        break ;
  case V4l_STYLE_REDDISH:
        sensor13m_write( 0x1E2 , 0x7002 ) ;
        sensor13m_write( 0x1E3 , 0xa040 ) ;
        break ;
  case V4l_STYLE_GREENISH:
        sensor13m_write( 0x1E2 , 0x7002 ) ;
        sensor13m_write( 0x1E3 , 0xc0c0 ) ;
        break ;
  default:
       sensor13m_write(0x1E2,0x7000);
       break;
  }

  return 0;
}

        
int sensor13m_set_light(V4l_PIC_WB light)
{ 
    u16 *m;
    u16 value, v106;
    int awb, i;
    unsigned long   diff_jiffies ;

    if ( g_light == light ) {
        return 0 ;
    }

    awb = 0;
    switch(light)
    {
        case V4l_WB_DIRECT_SUN:
           m = (u16 *)d65Light;
            break;       

        case V4l_WB_CLOUDY:
            m = ( u16 * ) cloudyLight ;
            break ;

        case V4l_WB_INCANDESCENT:
            m = (u16 *)incandescentLight;
            break;       

        case V4l_WB_FLUORESCENT:
            m = (u16 *)tl84Light;
            break;

        default:
            m = (u16 *)autoLight;
            awb = 1;
            break;

    }

    if ( gewa ) {
        diff_jiffies = jiffies - g_ulAWBJiffies ;
        if ( ( diff_jiffies ) < 30 ) {
            set_current_state( TASK_INTERRUPTIBLE ) ;
            schedule_timeout( 30 - diff_jiffies ) ;
        }
        gewa = 0 ;    
    } else {
        DisableAWB() ;
        set_current_state( TASK_INTERRUPTIBLE ) ;
        schedule_timeout( 30 ) ;
    }

    v106 = sensor13m_reg_read(0x106);

    sensor13m_write(0x202, m[0]);  
    sensor13m_write(0x209, m[3]);  
    sensor13m_write(0x20A, m[4]);  
    sensor13m_write(0x20B, m[5]);  
    sensor13m_write(0x20C, m[6]);  
    sensor13m_write(0x20D, m[7]);  
    sensor13m_write(0x20E, m[8]);  
    sensor13m_write(0x20F, m[9]);  
    sensor13m_write(0x210, m[10]); 
    sensor13m_write(0x211, m[11]); 
    sensor13m_write(0x215, m[12]); 
    sensor13m_write(0x216, m[13]); 
    sensor13m_write(0x217, m[14]); 
    sensor13m_write(0x218, m[15]); 
    sensor13m_write(0x219, m[16]); 
    sensor13m_write(0x21A, m[17]); 
    sensor13m_write(0x21B, m[18]); 
    sensor13m_write(0x21C, m[19]); 
    sensor13m_write(0x21D, m[20]); 
    sensor13m_write(0x21E, m[21]); 
    sensor13m_write(0x25E, m[22]); 
    sensor13m_write(0x25F, m[23]); 
    sensor13m_write(0x260, m[24]); 
    sensor13m_write(0x203, m[1]);  
    sensor13m_write(0x204, m[2]);  

    if(awb)
    {   
        sensor13m_read(0x148, &value);
        sensor13m_write(0x148, (value & ~0x0080));
        v106 = (v106 | 0x8000) & ~0x0002;
        sensor13m_write(0x106, v106);

        i = 0;
        do
        {
            sensor13m_read(0x261, &value );
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(7);
        }while((value != m[22]) && ((++i) < 100));
        v106 = (v106 & ~0x8000) | 0x0002;
        sensor13m_write(0x106, v106);
    }
    else {
        sensor13m_read(0x148, &value);
        sensor13m_write(0x148, (value | 0x0080));
        v106 = (v106 | 0x8000) & ~0x0002;
        sensor13m_write(0x106, v106);
    }

    return 0;
}

    
int sensor13m_param15(int bright)
{
    const u16 target[] = 
    { 
        21,      
        28,      
        34,      
        48,      
        68,      
        96,      
        136,     
        167,     
        221      
    };

    if(bright < -4 || bright > 4)
    {
        return -2;
    }

    sensor13m_write(0x22E, 0x0500 + target[bright+4]);
    return 0;
}

int sensor13m_param16(int rows, int columns)
{
    u16 reg;
    if(rows==0)
        mr = rotate_mirror_rows;   
    else
        mr = (!rotate_mirror_rows)&0x01;

    if(columns==0)
        mcal = rotate_mirror_columns;
    else
        mcal = (!rotate_mirror_columns)&0x01;

    reg = sensor13m_reg_read(0x020);
    sensor13m_reg_write_020(reg);

    return 0;
}

int sensor13m_param17(int strobe)
{
    sflat = strobe;
    return 0;
}

static void sensor13m_lsc_settings(void)
{
    u16 value;

    sensor13m_write(0x180, 0x0007);    
    sensor13m_write(0x181, 0xD810);    
    sensor13m_write(0x182, 0xF0E6);    
    sensor13m_write(0x183, 0x01F9);    
    sensor13m_write(0x184, 0xE20C);    
    sensor13m_write(0x185, 0xF6EC);    
    sensor13m_write(0x186, 0x00FA);    
    sensor13m_write(0x187, 0xE00D);    
    sensor13m_write(0x188, 0xF4EA);    
    sensor13m_write(0x189, 0xFFFA);    
    sensor13m_write(0x18A, 0xBB21);    
    sensor13m_write(0x18B, 0xE4D2);    
    sensor13m_write(0x18C, 0xF7EE);    
    sensor13m_write(0x18D, 0x0002);    
    sensor13m_write(0x18E, 0xD816);    
    sensor13m_write(0x18F, 0xECE2);    
    sensor13m_write(0x190, 0xF8F2);    
    sensor13m_write(0x191, 0x00FF);    
    sensor13m_write(0x192, 0xD713);    
    sensor13m_write(0x193, 0xF0E7);    
    sensor13m_write(0x194, 0xFAF5);    
    sensor13m_write(0x195, 0x0001);    
    sensor13m_write(0x1B6, 0x1109);    
    sensor13m_write(0x1B7, 0x3C20);    
    sensor13m_write(0x1B8, 0x0B06);    
    sensor13m_write(0x1B9, 0x2914);    
    sensor13m_write(0x1BA, 0x0A03);    
    sensor13m_write(0x1BB, 0x2312);    
    sensor13m_write(0x1BC, 0x1409);    
    sensor13m_write(0x1BD, 0x3221);    
    sensor13m_write(0x1BE, 0x0058);    
    sensor13m_write(0x1BF, 0x0E06);    
    sensor13m_write(0x1C0, 0x1E16);    
    sensor13m_write(0x1C1, 0x0034);    
    sensor13m_write(0x1C2, 0x0B05);    
    sensor13m_write(0x1C3, 0x1B12);    
    sensor13m_write(0x1C4, 0x002C);    
    
    sensor13m_read(0x106, &value);   
    value |= 0x0400;
    sensor13m_write(0x106, value);
}

int sensor13m_default_settings()
{
    sensor13m_reset();

    sensor13m_write(0x00D, 0x29);   
    sensor13m_write(0x00D, 0x08);   
    sensor13m_param11() ;
    sensor13m_write(0x23D, 0x17DD);
    sensor13m_write(0x05F, 0x3630);
    sensor13m_write(0x030, 0x043E);
    sensor13m_write(0x13B, 0x043E);
    sensor13m_write(0x238, 0x0840);   
    sensor13m_write(0x282, 0x03FE);
    sensor13m_write(0x285, 0x0061);   
    sensor13m_write(0x286, 0x0080);   
    sensor13m_write(0x287, 0x0061);   
    sensor13m_write(0x288, 0x0061);   
    sensor13m_write(0x289, 0x03E2);   
    sensor13m_write(0x14C, 0x0001);   
    sensor13m_write(0x14D, 0x0001);   
    sensor13m_write(0x13A, 0x0000);   
    sensor13m_write(0x19B, 0x0000);   
    sensor13m_write(0x1a8, 0x4000);   
    sensor13m_write(0x1a5, 0x4000);   
    sensor13m_write(0x1a2, 0x4000);   
    sensor13m_write(0x19F, 0x4000);   
    sensor13m_write(0x2CC, 0x0004);   
    sensor13m_write(0x105, 0x000B);   
    sensor13m_write(0x1AF, 0x0018);   
    sensor13m_write(0x21F, 0x0090);
    sensor13m_write(0x222, 0xb060);   
    sensor13m_write(0x223, 0xb070);   
    sensor13m_write(0x228, 0xEF02);   
    sensor13m_write(0x134, 0x0000);   
    sensor13m_write(0x135, 0xFF01);   
    sensor13m_write(0x22B, 0x8000);   
    sensor13m_write(0x22C, 0x8000);   
    sensor13m_write(0x125, 0x002D);   
    sensor13m_write(0x153, 0x0F04);
    sensor13m_write(0x154, 0x592D);
    sensor13m_write(0x155, 0xA988);
    sensor13m_write(0x156, 0xD9C3);
    sensor13m_write(0x157, 0xFFED);
    sensor13m_write(0x158, 0x0000);
    sensor13m_write(0x1DC, 0x0F04);
    sensor13m_write(0x1DD, 0x592D);
    sensor13m_write(0x1DE, 0xA988);
    sensor13m_write(0x1DF, 0xD9C3);
    sensor13m_write(0x1E0, 0xFFED);
    sensor13m_write(0x1E1, 0x0000);
    sensor13m_lsc_settings();


#ifdef CONFIG_CAMERA_STROBE_FLASH
    sensor13m_write(0x2CE, 0x5E9B);
#endif

    return 0;
}

