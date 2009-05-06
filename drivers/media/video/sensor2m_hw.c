/*
 *  sensor2m_hw.c
 *
 *  Camera Module driver.
 *
 *  Copyright (C) 2005,2006 Motorola Inc.
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
 *
 *
 *  Revision History:
 *               Modification
 *  Author         Date           Description of Changes
 *  ----------    ------------    -------------------------
 *  Motorola      04/30/2005      Created, for EZX platform
 *  Motorola      04/07/2006      re-struct camera driver for GPL
 *  Motorola      04/12/2006      Fix sensor name
 *  Motorola      08/17/2006      Update comments for GPL
 *  Motorola      08/28/2006      Updated camera settings
 *  Motorola      09/20/2006      Updated camera settings
 *
 */

/*
 * ==================================================================================
 *                                  INCLUDE FILES
 * ==================================================================================
 */

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

#include "sensor2m_hw.h"
#include "camera.h"


extern int i2c_sensor2m_read(u16 addr, u16 *pvalue);
extern int i2c_sensor2m_write(u16 addr, u16 value);

static int sensor_mnp = 0;

static u16 sensorAWidth = MAX_SENSOR_A_WIDTH;
static u16 sensorAHeight = MAX_SENSOR_A_HEIGHT;
static u16 sensorBWidth = MAX_SENSOR_B_WIDTH;
static u16 sensorBHeight = MAX_SENSOR_B_HEIGHT;
static u16 outWidth = DEFT_SENSOR_A_WIDTH;
static u16 outHeight = DEFT_SENSOR_A_HEIGHT;
static u16 captureWidth = DEFT_SENSOR_B_WIDTH;
static u16 captureHeight = DEFT_SENSOR_B_HEIGHT;
static u16 cameraWidth = MAX_SENSOR_B_WIDTH/2;
static u16 cameraHeight = MAX_SENSOR_B_HEIGHT;

static int position1;
static int status1;
static int status2;
static int position2;

static int rtime = 15;
static int sensor_flicker_freq = 60;

#ifdef  CONFIG_CAMERA_ROTATE_180

static const int rotate_mirror_rows = 1;
static const int rotate_mirror_columns = 1;
#else
static const int rotate_mirror_rows = 0;
static const int rotate_mirror_columns = 0;
#endif

static int mirror_rows;
static int mirror_columns;

const static u16 id2_mcc_reg_off[] = 
{ 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A,
  0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30 };

const static u16 mcc_at[] =
{ 0x0192, 0xFF3E, 0x0030, 0xFF3E, 0x01F7, 0xFFCB, 0xFF5B, 0xFEBB, 0x02E9, 0x001D, 0x003B, 
  0x0071, 0xFFD3, 0xFFBC, 0x007D, 0xFF9F, 0xFFE5, 0x008A, 0x009B, 0xFEDB, 0x0012, 0xFFEA };

const static u16 mcc_idt[] =
{ 0x0192, 0xFF3E, 0x0030, 0xFF3E, 0x01F7, 0xFFCB, 0xFF5B, 0xFEBB, 0x02E9, 0x001C, 0x003F, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }; 

const static u16 mcc_tl[] =
{ 0x01BE, 0xFF2D, 0x0016, 0xFF6F, 0x01D1, 0xFFC1, 0xFF91, 0xFEF6, 0x027A, 0x0022, 0x0034, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }; 

const static u16 mcc_dy[] =
{ 0x0203, 0xFF11, 0xFFEC, 0xFFBB, 0x0196, 0xFFB0, 0xFFE5, 0xFF56, 0x01C4, 0x002D, 0x0025, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

const static u16 mcc_cld[] =
{ 0x0203, 0xFF11, 0xFFEC, 0xFFBB, 0x0196, 0xFFB0, 0xFFE5, 0xFF56, 0x01C4, 0x0030, 0x0023, 
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };
    
static void sensor2m_reset_value(void)
{
    position1 = 0;
    status1 = 0;
    status2 = 0;
    position2 = 0;
    rtime = 15;
    sensor_flicker_freq = 60;
    sensor_mnp = 0;
    mirror_rows = rotate_mirror_rows;
    mirror_columns = rotate_mirror_columns;
}
u16 sensor2m_reg_read(u16 reg_addr)
{
    u16 value=0;
    i2c_sensor2m_read(reg_addr, &value);
    return value;
}

void sensor2m_reg_write(u16 reg_addr, u16 reg_value)
{
    i2c_sensor2m_write(reg_addr, reg_value);
}

u16 sensor2m_var_read16(u8 id, u8 offset)
{
    u16 r198, value;
    r198 = 0 | 8192 | ((id & 31)<<8) | offset;
    i2c_sensor2m_write(0x1c6, r198);
    i2c_sensor2m_read(0x1c8, &value);
    return value;
}

u8 sensor2m_var_read8(u8 id, u8 offset)
{
    u16 r198, value;
    r198 = 32768 | 8192 | ((id & 31)<<8) | offset;
    i2c_sensor2m_write(0x1c6, r198);
    i2c_sensor2m_read(0x1c8, &value);
    return value;
}

void sensor2m_var_write16(u8 id, u8 offset, u16 value)
{
    u16 r198;
    r198 = 0 | 8192 | ((id & 31)<<8) | offset;
    i2c_sensor2m_write(0x1c6, r198);
    i2c_sensor2m_write(0x1c8, value);
}

void sensor2m_var_write8(u8 id, u8 offset, u8 value)
{
    u16 r198;
    r198 = 32768 | 8192 | ((id & 31)<<8) | offset;
    i2c_sensor2m_write(0x1c6, r198);
    i2c_sensor2m_write(0x1c8, (u16)value);
}

static void sensor2m_reg_write_020(u16 readmode)
{
    readmode = (readmode & 0xfffc) | mirror_rows | (mirror_columns<<1);
    sensor2m_reg_write(0x020, readmode);
}

static void sensor2m_wait_value(int svalue)
{
    int cur_svalue;

    cur_svalue = sensor2m_var_read8(1, 4);
    while(cur_svalue!=svalue) {
        cur_svalue = sensor2m_var_read8(1, 4);
    }
}

static int sensor2m_set_mode(int mValue)
{

    if(mValue == 0)
    {
        sensor2m_var_write8(1, 32, 0);
        sensor2m_var_write8(1, 3, 1);
        sensor2m_wait_value(3);
        position1 = 0;
    }
    else
    {
        sensor2m_var_write8(1, 32, 2);
        sensor2m_var_write8(1, 3, 2);
        sensor2m_wait_value(7);
        position1 = 1;
    }

    return 0;
}

static void sensor2m_cmd_5(void)
{
    int cmd, svalue;
    svalue = sensor2m_var_read8(1, 4);

    sensor2m_var_write8(1, 3, 5);

    cmd = sensor2m_var_read8(1, 3);
    while(cmd!=0) {
        cmd = sensor2m_var_read8(1, 3);
    }
    status1 = 0;
}

static void sensor2m_cmd_6(void)
{
    int cmd, svalue;
    svalue = sensor2m_var_read8(1, 4);

    sensor2m_var_write8(1, 3, 6);
    cmd = sensor2m_var_read8(1, 3);
    while(cmd!=0) {
        cmd = sensor2m_var_read8(1, 3);
    }
    status2 = 0;
}

int sensor2m_viewfinder_on()
{

    if(status2)
    {
        sensor2m_cmd_6();
    }

    if(status1)
    {
        sensor2m_cmd_5();
    }

    if(position1)
    {
        sensor2m_set_mode(0);
    }
    else
    {
        sensor2m_wait_value(3);
    }
    position2 = 1;

    return 0;
}

int sensor2m_snapshot_trigger()
{
    sensor2m_set_mode(1);
    return 0;
}

int sensor2m_sensor_size(sensor_window_size * window)
{
    sensorAWidth = window->width;
    sensorAHeight = window->height;

    sensor2m_var_write16(7, 39, (MAX_SENSOR_A_WIDTH - sensorAWidth)/2 );
    sensor2m_var_write16(7, 41, (MAX_SENSOR_A_WIDTH + sensorAWidth)/2 );
    sensor2m_var_write16(7, 43, (MAX_SENSOR_A_HEIGHT - sensorAHeight)/2 );
    sensor2m_var_write16(7, 45, (MAX_SENSOR_A_HEIGHT + sensorAHeight)/2 );

    status1 = 1;
    return 0;
}

int sensor2m_capture_sensor_size(sensor_window_size * window)
{
    sensorBWidth = window->width;
    sensorBHeight = window->height;

    sensor2m_var_write16(7, 53, (MAX_SENSOR_B_WIDTH - sensorBWidth)/2 );
    sensor2m_var_write16(7, 55, (MAX_SENSOR_B_WIDTH + sensorBWidth)/2 );
    sensor2m_var_write16(7, 57, (MAX_SENSOR_B_HEIGHT - sensorBHeight)/2 );
    sensor2m_var_write16(7, 59, (MAX_SENSOR_B_HEIGHT + sensorBHeight)/2 );

    status1 = 1;
    return 0;
}

int sensor2m_output_size(sensor_window_size * window)
{
    outWidth = window->width;
    outHeight = window->height;

    sensor2m_var_write16(7, 3, outWidth);
    sensor2m_var_write16(7, 5, outHeight);

    status1 = 1;
    return 0;
}

int sensor2m_capture_size(sensor_window_size * window)
{
    captureWidth = window->width;
    captureHeight = window->height;

    sensor2m_var_write16(7, 7, captureWidth);
    sensor2m_var_write16(7, 9, captureHeight);

    status1 = 1;
    return 0;
}

int sensor2m_set_camera_size( sensor_window_size * window)
{
    cameraWidth = window->width;
    cameraHeight = window->height;

    sensor2m_var_write16(7, 121, cameraWidth);
    sensor2m_var_write16(7, 123, cameraHeight);

    status1 = 1;
    return 0;
}

int sensor2m_get_camera_size( sensor_window_size * window)
{
    window->width = cameraWidth;
    window->height = cameraHeight;

    return 0;
}

int sensor2m_set_exposure_mode(int mode, u32 metime)
{
    int mzone, tvalue, time_1zone;
    if(sensor_flicker_freq == 60)
    {
        time_1zone = 8333;
        tvalue = 8;
    }
    else
    {
        time_1zone = 9966;
        tvalue = 6;
    }
    mzone = metime / time_1zone;

    if(mzone>24)
        mzone = 24;
    if(mzone<=0)
        mzone = 1;

    if(tvalue>mzone)
        tvalue = mzone;
    sensor2m_var_write8(2, 14, mzone);
    sensor2m_var_write8(2, 23, tvalue);
    sensor2m_var_write8(2, 30, mzone);

    if(mode==V4l_NM_NIGHT)
    {
        sensor2m_var_write8(2, 0x10, 0x0080);
        sensor2m_var_write8(2, 0x18, 0x0078);
        sensor2m_var_write16(2, 0x14, 0x0080);
        sensor2m_var_write8(2, 0x16, 0x0020);
        sensor2m_var_write8(1, 0x18, 0x0040);
    }
    else
    {
        sensor2m_var_write8(2, 0x10, 0x0080);
        sensor2m_var_write8(2, 0x18, 0x0040); 
        sensor2m_var_write16(2, 0x14, 0x0080);
        sensor2m_var_write8(2, 0x16, 0x0020);
        sensor2m_var_write8(1, 0x18, 0x0080);
    }

    if(position2)
        sensor2m_cmd_5();

    return 0;
}

int sensor2m_capture_format(u16 format)
{
    switch(format)
    {
    case 0:
        sensor2m_var_write16(7, 11, 0x0030);
        sensor2m_var_write16(7, 114, 0x0027);
        sensor2m_var_write16(7, 116, 0x0001);
        sensor2m_var_write8(7, 118, 0x21);
        break;
    case 4:
        sensor2m_var_write16(7, 11, 0x0010);
        sensor2m_var_write16(7, 114, 0x0067);
        sensor2m_var_write16(7, 116, 0x0406);
        sensor2m_var_write8(7, 118, 0x02);
        break;
    default:
        err_print("unsupported format %d", format);
        break;
    }
    return 0;
}

static const u16 gaps[] = 
{
    1788,
    1290,
    1064,
    841,
    681,
    563,
    452,
    391,
    340,
    278,
    202,
    140,
    103,
    67,
    36,
    11
};

int sensor2m_set_time(u16 time)
{
    u16 gap;

    if (time != rtime)
    {
        if(time>20)
            time = 20;
        else if(time<5)
            time = 5;
        gap = gaps[time-5];
        sensor2m_reg_write(0x008, gap);
        rtime = time;
    }
    return 0;
}

int sensor2m_set_flicker(int flicker)
{
    u8 fd_mode;
    sensor_flicker_freq = flicker;

    fd_mode = sensor2m_var_read8(4, 4);
    fd_mode |= 0x80;
    if(flicker==50)
    {
        sensor2m_var_write8(4, 4, fd_mode | 0x40);
        sensor2m_var_write16(2, 47, 122);
    }
    else if(flicker==60)
    {
        sensor2m_var_write8(4, 4, fd_mode & (~0x40));
        sensor2m_var_write16(2, 47, 102);
    }
    else
        return -EINVAL;

    status1 = 1;
    return 0;
}

int sensor2m_set_style(V4l_PIC_STYLE style)
{
    u16 effects_value;

    switch(style)
    {
    case V4l_STYLE_BLACK_WHITE:
        effects_value = 0x6441;
        break;
    case V4l_STYLE_SEPIA:
        effects_value = 0x6442;
        break;
    case V4l_STYLE_SOLARIZE:
        effects_value = 0x3844;
        break;
    case V4l_STYLE_NEG_ART:
        effects_value = 0x6443;
        break;
    default:
        effects_value = 0x6440;
        break;
    }

    sensor2m_var_write16(7, 127, effects_value);
    sensor2m_var_write16(7, 129, effects_value);

    if(position2)
        sensor2m_cmd_5();

    return 0;
}

int sensor2m_set_light(V4l_PIC_WB light)
{ 
    u16 *m ;
    int i ;
    u8 awb_mode;

    switch(light)
    {
        case V4l_WB_DIRECT_SUN:
            ddbg_print(" %d (direct sun)", light);
            awb_mode = sensor2m_var_read8(1, 0x02);  
            sensor2m_var_write8(1, 0x02, awb_mode | (0x0004));
            sensor2m_var_write8( 1 , 0x2B , 0x3 ) ;  
            awb_mode = sensor2m_var_read8(1, 0x20);  
            sensor2m_var_write8(1, 0x20, awb_mode | (0x0020));
            sensor2m_var_write8( 3 , 0x4F , 0x3F ) ;
            sensor2m_var_write8( 3 , 0x50 , 0x3F ) ;
            udelay(100);
            awb_mode = sensor2m_var_read8(3, 0x53);  
            sensor2m_var_write8(3, 0x53, awb_mode | (0x0020));

            m = mcc_dy;

            for(i=0; i<22; i++)
            {
                sensor2m_var_write16(3, id2_mcc_reg_off[i], m[i]);
            }

            sensor2m_var_write8( 3 , 0x4C , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x4D , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x4E , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x51 , 0x003F ) ;
            break;       

        case V4l_WB_INCANDESCENT:
            ddbg_print(" %d (incandescent)", light);
            awb_mode = sensor2m_var_read8(1, 0x02);  
            sensor2m_var_write8(1, 0x02, awb_mode | (0x0004));
            sensor2m_var_write8( 1 , 0x2B , 0x3 ) ;  
            awb_mode = sensor2m_var_read8(1, 0x20);  
            sensor2m_var_write8(1, 0x20, awb_mode | (0x0020));
            sensor2m_var_write8( 3 , 0x4F , 0x3F ) ;
            sensor2m_var_write8( 3 , 0x50 , 0x3F ) ;
            udelay(100);
            awb_mode = sensor2m_var_read8(3, 0x53);  
            sensor2m_var_write8(3, 0x53, awb_mode | (0x0020));

            m = mcc_idt;
            for(i=0; i<22; i++)
            {
                sensor2m_var_write16(3, id2_mcc_reg_off[i], m[i]);
            }

            sensor2m_var_write8( 3 , 0x4C , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x4D , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x4E , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x51 , 0x003F ) ;

            break;       

        case V4l_WB_FLUORESCENT:
            ddbg_print(" %d (fluorescent)", light);
            awb_mode = sensor2m_var_read8(1, 0x02);  
            sensor2m_var_write8(1, 0x02, awb_mode | (0x0004));
            sensor2m_var_write8( 1 , 0x2B , 0x3 ) ;  
            awb_mode = sensor2m_var_read8(1, 0x20);  
            sensor2m_var_write8(1, 0x20, awb_mode | (0x0020));
            sensor2m_var_write8( 3 , 0x4F , 0x3F ) ;
            sensor2m_var_write8( 3 , 0x50 , 0x3F ) ;
            udelay(100);
            awb_mode = sensor2m_var_read8(3, 0x53);  
            sensor2m_var_write8(3, 0x53, awb_mode | (0x0020));

            m = mcc_tl;
            for(i=0; i<22; i++)
            {
                sensor2m_var_write16(3, id2_mcc_reg_off[i], m[i]);
            }

            sensor2m_var_write8( 3 , 0x4C , 0x0080 ) ; 
            sensor2m_var_write8( 3 , 0x4D , 0x0080 ) ; 
            sensor2m_var_write8( 3 , 0x4E , 0x0080 ) ;
            sensor2m_var_write8( 3 , 0x51 , 0x003F ) ;

            break;

        default:
            sensor2m_var_write8( 3 , 0x4F , 0x0 ) ;
            sensor2m_var_write8( 3 , 0x50 , 0x7F ) ;
            sensor2m_var_write8( 1 , 0x02 , 0x0F ) ; 
            sensor2m_var_write8( 1 , 0x2B , 0x3 ) ;  
            awb_mode = sensor2m_var_read8(1, 0x20);  
            sensor2m_var_write8(1, 0x20, awb_mode | (0x0020));
            awb_mode = sensor2m_var_read8(3, 0x53);  
            sensor2m_var_write8(3, 0x53, awb_mode & (0xFFDF));
            sensor2m_var_write8( 3 , 0x64 , 0x80 ) ;  
            sensor2m_var_write8( 3 , 0x65 , 0x80 ) ;  
            sensor2m_var_write8( 3 , 0x66 , 0x80 ) ;  
            m = mcc_at;
            for(i=0; i<22; i++)
            {
                sensor2m_var_write16(3, id2_mcc_reg_off[i], m[i]);
            }
            sensor2m_var_write8( 3 , 0x61 , 0x00E2 ) ;
            sensor2m_var_write8( 3 , 0x62 , 0x00F0 ) ;
            break;
    }

    return 0;
}

int sensor2m_set_bright(int bright)
{

    static const u16 target[][2] = 
    { 
        {20, 3},
        {27, 3},
        {33, 3},
        {47, 5},    
        {67, 8},    
        {95, 10},
        {134,12},   
        {166,15}, 
        {218,20} 
    };

    if(bright < -4 || bright > 4)
    {
        return -2;
    }
    sensor2m_var_write8(2, 6, target[bright+4][0]);
    sensor2m_var_write8(2, 7, target[bright+4][1]);

    return 0;
}

int sensor2m_set_sfact(int scale)
{
    int sfact1, sfact2, sfact3;

    if(scale<=0 || scale>127)
        return -EINVAL;

    sfact1 = scale;
    if(sfact1>127)
        sfact1 = 127;

    sfact2 = sfact1 ;
    sfact3 = sfact1 ;

    sensor2m_var_write8(9, 10, sfact1 | 0x80);
    sensor2m_var_write8(9, 11, sfact2 | 0x80);
    sensor2m_var_write8(9, 12, sfact3 | 0x80);

    return 0;
}

int sensor2m_set_mirror(int rows, int columns)
{
    u16 reg;
    if(rows==0)
        mirror_rows = rotate_mirror_rows;
    else
        mirror_rows = (!rotate_mirror_rows)&0x01;

    if(columns==0)
        mirror_columns = rotate_mirror_columns;
    else
        mirror_columns = (!rotate_mirror_columns)&0x01;

    reg = sensor2m_reg_read(0x020);
    sensor2m_reg_write_020(reg);

    return 0;
}

static void sensor2m_lsc_settings(void)
{
    u16 u0x108 ;

    sensor2m_reg_write( 0x280 , 0x0168 ) ;
    sensor2m_reg_write( 0x281 , 0x6432 ) ;
    sensor2m_reg_write( 0x282 , 0x3296 ) ;
    sensor2m_reg_write( 0x283 , 0x9664 ) ;
    sensor2m_reg_write( 0x284 , 0x5028 ) ;
    sensor2m_reg_write( 0x285 , 0x2878 ) ;
    sensor2m_reg_write( 0x286 , 0x7850 ) ;
    sensor2m_reg_write( 0x287 , 0x0000 ) ;
    sensor2m_reg_write( 0x288 , 0x0119 ) ;
    sensor2m_reg_write( 0x28B , 0x00C7 ) ;
    sensor2m_reg_write( 0x28E , 0x0C47 ) ;
    sensor2m_reg_write( 0x291 , 0x0DFB ) ;
    sensor2m_reg_write( 0x294 , 0xF8C3 ) ;
    sensor2m_reg_write( 0x297 , 0x156F ) ;
    sensor2m_reg_write( 0x29A , 0x1225 ) ;
    sensor2m_reg_write( 0x29D , 0x1041 ) ;
    sensor2m_reg_write( 0x2A0 , 0x1246 ) ;
    sensor2m_reg_write( 0x2A3 , 0x1328 ) ;
    sensor2m_reg_write( 0x2A6 , 0x1268 ) ;
    sensor2m_reg_write( 0x2A9 , 0xFFA9 ) ;
    sensor2m_reg_write( 0x289 , 0x00FE ) ;
    sensor2m_reg_write( 0x28C , 0x00BA ) ;
    sensor2m_reg_write( 0x28F , 0x0D9E ) ;
    sensor2m_reg_write( 0x292 , 0x0E1E ) ;
    sensor2m_reg_write( 0x295 , 0xF288 ) ;
    sensor2m_reg_write( 0x298 , 0x197A ) ;
    sensor2m_reg_write( 0x29B , 0x132A ) ;
    sensor2m_reg_write( 0x29E , 0x0E38 ) ;
    sensor2m_reg_write( 0x2A1 , 0x0D34 ) ;
    sensor2m_reg_write( 0x2A4 , 0x142D ) ;
    sensor2m_reg_write( 0x2A7 , 0x146F ) ;
    sensor2m_reg_write( 0x2AA , 0x0ABF ) ;
    sensor2m_reg_write( 0x28A , 0x00CB ) ;
    sensor2m_reg_write( 0x28D , 0x006F ) ;
    sensor2m_reg_write( 0x290 , 0x0D13 ) ;
    sensor2m_reg_write( 0x293 , 0x0E5C ) ;
    sensor2m_reg_write( 0x296 , 0x06D0 ) ;
    sensor2m_reg_write( 0x299 , 0x1368 ) ;
    sensor2m_reg_write( 0x29C , 0x0E19 ) ;
    sensor2m_reg_write( 0x29F , 0x0827 ) ;
    sensor2m_reg_write( 0x2A2 , 0x061B ) ;
    sensor2m_reg_write( 0x2A5 , 0x0A19 ) ;
    sensor2m_reg_write( 0x2A8 , 0x1265 ) ;
    sensor2m_reg_write( 0x2AB , 0x00C5 ) ;
    sensor2m_reg_write( 0x2AC , 0x0000 ) ;
    sensor2m_reg_write( 0x2AD , 0x0000 ) ;
    sensor2m_reg_write( 0x2AE , 0x0000 ) ;


    u0x108 = sensor2m_reg_read( 0x108 ) ;
    sensor2m_reg_write( 0x108 , u0x108 | 0x0004 ) ;
}

static void sensor2m_set_mnp(void)
{

#if 1
    sensor2m_reg_write(0x066, 0x1001);
    sensor2m_reg_write(0x067, 0x0501);
#endif

    sensor2m_reg_write(0x065, 0xA000);
    udelay(150);
    sensor2m_reg_write(0x065, 0x2000);

    sensor_mnp = 1;
}

int sensor2m_get_cstatus(int *plength, int *pstatus)
{
    u16 r202, r203, r204;
    u16 var9_16;
    u8  var9_15;
    var9_15 = sensor2m_var_read8(9, 15);
    var9_16 = sensor2m_var_read16(9, 16);

    r202 = sensor2m_reg_read(0x202);
    r203 = sensor2m_reg_read(0x203);
    r204 = sensor2m_reg_read(0x204);
    if(plength)
        *plength = ((int)var9_15 << 8) + (int)var9_16;
    if(pstatus)
        *pstatus = r202 & 0x00ff;
    return 0;
}

int sensor2m_enter_9(void)
{
    int svalue;
    int count;
    u16 value;

    svalue = sensor2m_var_read8(1, 4);
    if(svalue!=3)
        sensor2m_set_mode(0);

    sensor2m_var_write8(1, 3, 3);

    count = 500000;
    svalue = sensor2m_var_read8(1, 4);
    while(svalue!=9 && count>0) {
        svalue = sensor2m_var_read8(1, 4);
        count--;
    }

    if(sensor_mnp)
    {
        sensor2m_reg_write(0x065, 0xE000);
        udelay(150);
    }

    value = sensor2m_reg_read(0x10a);
    sensor2m_reg_write(0x10a, value | 0x0080);

    value = sensor2m_reg_read(0x00D);
    sensor2m_reg_write(0x00D, value | 0x0040);

    sensor2m_reg_write(0x1c6, 0x9078);
    sensor2m_reg_write(0x1c8, 0x0000);
    sensor2m_reg_write(0x1c6, 0x9079);
    sensor2m_reg_write(0x1c8, 0x0000);
    sensor2m_reg_write(0x1c6, 0x9070);
    sensor2m_reg_write(0x1c8, 0x0000);
    sensor2m_reg_write(0x1c6, 0x9071);
    sensor2m_reg_write(0x1c8, 0x0000);

    return 0;
}

int sensor2m_exit_9(void)
{
    int svalue;
    int count;
    u16 value;

    value = sensor2m_reg_read(0x10a);
    sensor2m_reg_write(0x10a, value & (~0x0080));

    value = sensor2m_reg_read(0x00D);
    sensor2m_reg_write(0x00D, value & (~0x0040));

    if(sensor_mnp)
    {
        mdelay(2);
        sensor2m_reg_write(0x065, 0xA000);
        udelay(150);
        sensor2m_reg_write(0x065, 0x2000);
    }

    svalue = sensor2m_var_read8(1, 4);
    if(svalue!=3)
    {
        sensor2m_var_write8(1, 3, 1);

        count = 500000;
        while(svalue!=3 && count>0) {
            svalue = sensor2m_var_read8(1, 4);
            count--;
        }
    }
    return 0;
}

int sensor2m_cadjust( void )
{
    sensor2m_var_write16( 3 , 0x06 , 0x0192 ) ;
    sensor2m_var_write16( 3 , 0x08 , 0xFF3E ) ;
    sensor2m_var_write16( 3 , 0x0A , 0x0030 ) ;
    sensor2m_var_write16( 3 , 0x0C , 0xFF3E ) ;
    sensor2m_var_write16( 3 , 0x0E , 0x01F7 ) ;
    sensor2m_var_write16( 3 , 0x10 , 0xFFCB ) ;
    sensor2m_var_write16( 3 , 0x12 , 0xFF5B ) ;
    sensor2m_var_write16( 3 , 0x14 , 0xFEBB ) ;
    sensor2m_var_write16( 3 , 0x16 , 0x02E9 ) ;
    sensor2m_var_write16( 3 , 0x18 , 0x001D ) ;
    sensor2m_var_write16( 3 , 0x1A , 0x003B ) ;
    sensor2m_var_write16( 3 , 0x1C , 0x0071 ) ;
    sensor2m_var_write16( 3 , 0x1E , 0xFFD3) ;
    sensor2m_var_write16( 3 , 0x20 , 0xFFBC ) ;
    sensor2m_var_write16( 3 , 0x22 , 0x007D ) ;
    sensor2m_var_write16( 3 , 0x24 , 0xFF9F ) ;
    sensor2m_var_write16( 3 , 0x26 , 0xFFE5 ) ;
    sensor2m_var_write16( 3 , 0x28 , 0x008A ) ;
    sensor2m_var_write16( 3 , 0x2A , 0x009B ) ;
    sensor2m_var_write16( 3 , 0x2C , 0xFEDB ) ;
    sensor2m_var_write16( 3 , 0x2E , 0x0012 ) ;
    sensor2m_var_write16( 3 , 0x30 , 0xFFEA ) ;
    sensor2m_var_write8( 3 , 0x61 , 0x00E2 ) ;
    sensor2m_var_write8( 3 , 0x62 , 0x00F0 ) ;
    return 0 ;
}

int sensor2m_default_settings()
{
    u16 value;
    sensor2m_reg_write(0x005, 0x0204);
    sensor2m_reg_write(0x006, 0x000B);
    sensor2m_reg_write(0x007, 0x00FE);
    sensor2m_reg_write(0x008, 202);
    sensor2m_reg_write_020(0x0300); 
    sensor2m_reg_write(0x021, 0x8400);
    sensor2m_var_write8(1, 35, 0x000);
    sensor2m_var_write8(1, 42, 0x002);
    sensor2m_var_write8(1, 48, 0x000);
    sensor2m_var_write8(1, 50, 0x000);
    sensor2m_var_write8(1, 52, 0x000);
    sensor2m_var_write8(1, 54, 0x040);
    sensor2m_var_write16(7, 3, outWidth);
    sensor2m_var_write16(7, 5, outHeight);
    sensor2m_var_write16(7, 7, captureWidth);
    sensor2m_var_write16(7, 9, captureHeight);
    sensor2m_var_write16(7, 23, 0);
    sensor2m_var_write16(7, 25, 0x0011);
    sensor2m_var_write16(7, 35, 0);
    sensor2m_var_write16(7, 37, 0x0011);
	
    sensor2m_var_write8(7,  69, 0x000);
    sensor2m_var_write8(7,  70, 0x014);
    sensor2m_var_write8(7,  71, 0x022);
    sensor2m_var_write8(7,  72, 0x037);
    sensor2m_var_write8(7,  73, 0x058);
    sensor2m_var_write8(7,  74, 0x06F);
    sensor2m_var_write8(7,  75, 0x081);
    sensor2m_var_write8(7,  76, 0x08F);
    sensor2m_var_write8(7,  77, 0x09D);
    sensor2m_var_write8(7,  78, 0x0A9);
    sensor2m_var_write8(7,  79, 0x0B5);
    sensor2m_var_write8(7,  80, 0x0C0);
    sensor2m_var_write8(7,  81, 0x0CA);
    sensor2m_var_write8(7,  82, 0x0D4);
    sensor2m_var_write8(7,  83, 0x0DD);
    sensor2m_var_write8(7,  84, 0x0E6);
    sensor2m_var_write8(7,  85, 0x0EF);
    sensor2m_var_write8(7,  86, 0x0F7);
    sensor2m_var_write8(7,  87, 0x0FF);
    sensor2m_var_write8(7,  88, 0x000);
    sensor2m_var_write8(7,  89, 0x014);
    sensor2m_var_write8(7,  90, 0x022);
    sensor2m_var_write8(7,  91, 0x037);
    sensor2m_var_write8(7,  92, 0x058);
    sensor2m_var_write8(7,  93, 0x06F);
    sensor2m_var_write8(7,  94, 0x081);
    sensor2m_var_write8(7,  95, 0x08F);
    sensor2m_var_write8(7,  96, 0x09D);
    sensor2m_var_write8(7,  97, 0x0A9);
    sensor2m_var_write8(7,  98, 0x0B5);
    sensor2m_var_write8(7,  99, 0x0C0);
    sensor2m_var_write8(7, 100, 0x0CA);
    sensor2m_var_write8(7, 101, 0x0D4);
    sensor2m_var_write8(7, 102, 0x0DD);
    sensor2m_var_write8(7, 103, 0x0E6);
    sensor2m_var_write8(7, 104, 0x0EF);
    sensor2m_var_write8(7, 105, 0x0F7);
    sensor2m_var_write8(7, 106, 0x0FF);
    sensor2m_var_write16(7, 107, 0x0067); 
    sensor2m_var_write16(7, 109, 0x050a);
    sensor2m_var_write8(7, 111, 0x3);
    sensor2m_var_write16(7, 112, 0x0606);
    sensor2m_var_write16(2, 11, 402);
    sensor2m_var_write8(2, 14, 8);
    sensor2m_var_write8(2, 23, 8);
    sensor2m_var_write16(2, 40, 527);
    sensor2m_var_write16(2, 47, 102);
    sensor2m_var_write8(2, 0x10, 0x0080);
    sensor2m_var_write8(2, 0x18, 0x0078);
    sensor2m_var_write16(2, 0x14, 0x0080);
    sensor2m_var_write8(2, 0x16, 0x0020);
    sensor2m_var_write8(4, 8, 19);
    sensor2m_var_write8(4, 8, 21);
    sensor2m_var_write8(4, 10, 23);
    sensor2m_var_write8(4, 11, 25);
    sensor2m_var_write16(4, 17, 102);
    sensor2m_var_write16(4, 19, 122);
    sensor2m_var_write16(9, 8,  0 ) ;


	/* Low light and sharpness settings */

    sensor2m_var_write8(1, 21, 0x067);
    sensor2m_var_write8(1, 22, 0x046);
    sensor2m_var_write8(1, 23, 0x050);
    sensor2m_var_write8(1, 24, 0x080);
    sensor2m_var_write8(1, 25, 0x000);
    sensor2m_var_write8(1, 26, 0x008);
    sensor2m_var_write8(1, 27, 0x02A);
    sensor2m_var_write8(1, 28, 0x004);

	/* Full white adaptation across all lights */
    
	sensor2m_var_write8(3, 100, 0x080);
    sensor2m_var_write8(1, 101, 0x080);
    sensor2m_var_write8(1, 102, 0x080);
    sensor2m_var_write8(1, 3, 0x005);

    sensor2m_lsc_settings() ;
    sensor2m_cadjust() ;
    status2 = 1;
    value = sensor2m_reg_read(0x108);
    sensor2m_reg_write(0x108, (value & 0xFFBF));
    status1 = 1;
    rtime = 15;
    return 0;
}

static const u16 sensor2m_default_settings1[] = {
    0x104C, 0x0001,	0x0000
};

static const u16 sensor2m_default_settings2[] = {
    0x0310, 0x0032,	0x3C3C, 0xCC01, 0x33BD, 0xD43F, 0x30ED, 0x02DC, 
	0xB7A3, 0x0223, 0x0DF6, 0x02BD, 0xF102, 0xBE23, 0x107A, 0x02BD, 
	0x200B, 0xF602, 0xBDF1, 0x02BF, 0x2403, 0x7C02, 0xBDCC, 0x011F, 
	0xED00, 0xF602, 0xBD4F, 0xBDD4, 0x2BCC, 0x0130, 0xBDD4, 0x3FD7, 
	0xB026, 0x04C6, 0x01D7, 0xB0CC, 0x0131, 0xBDD4, 0x3FD7, 0xB126, 
	0x04C6, 0x01D7, 0xB1CC, 0x0132, 0xBDD4, 0x3FD7, 0xB25D, 0x2604, 
	0xC601, 0xD7B2, 0x5F38, 0x3839
};

static const u16 sensor2m_default_settings3[] = {
    0x0400, 0x01FA,	0x308F, 0xC3FF, 0xEF8F, 0x35F6, 0x01D0, 0x860C, 
	0x3DC3, 0x01DD, 0x8FEC, 0x0630, 0xED07, 0xF601, 0xD086, 0x0C3D, 
	0xC301, 0xDD8F, 0xEC04, 0x30ED, 0x05F6, 0x01D0, 0x4FED, 0x02CC, 
	0x0021, 0xA302, 0xBDD4, 0x3F30, 0xED0F, 0x1F0F, 0x800A, 0xEC07, 
	0x04ED, 0x07EC,	0x0504, 0xED05, 0xD65A, 0xC40F, 0xE704, 0xEC07, 
	0xED02, 0xE604,	0x4FED, 0x00CC, 0x0010, 0xBDD3, 0x4330, 0xEC02, 
	0xED0D, 0xD65A, 0x5454, 0x5454, 0xE704, 0xEC05, 0xED02, 0xE604, 
	0x4FED, 0x00CC, 0x0010, 0xBDD3, 0x4330, 0xEC02, 0xED0B, 0xD65B, 
	0xC40F, 0xCB01, 0xE704, 0xEC07, 0xED02, 0xE604, 0x4FED, 0x00CC, 
	0x0010, 0xBDD3, 0x4330, 0xEC02, 0xED07, 0xD65B, 0x5454, 0x5454, 
	0xCB01, 0xE704, 0xEC05, 0xED02, 0xE604, 0x4FED, 0x00CC, 0x0010, 
	0xBDD3, 0x4330, 0xEC02, 0xED05, 0xE30B, 0xED09, 0xC300, 0x0ADD, 
	0x5CEC, 0x0DE3, 0x07ED, 0x02EC, 0x0DED, 0x00C6, 0x08BD, 0xD319, 
	0x30ED, 0x0FCC, 0x012D, 0xED00, 0xEC0F, 0xBDD4, 0x2B30, 0xEC09, 
	0xED02, 0xEC0B, 0xED00, 0xC608, 0xBDD3, 0x1930, 0xED0F, 0xCC01, 
	0x2EED, 0x00EC, 0x0FBD, 0xD42B, 0x30C6, 0x113A, 0x3539, 0x308F, 
	0xC3FF, 0xED8F, 0x35F6, 0x01D0, 0x860E, 0x3DC3, 0x01F5, 0x8FEC, 
	0x0030, 0xED06, 0xF601, 0xD086, 0x0E3D, 0xC301, 0xF58F, 0xEC04, 
	0x30ED, 0x04EC, 0x15ED, 0x0BEC, 0x17ED, 0x09D6, 0x02C4, 0x0FE7, 
	0x08EC, 0x0BED, 0x02E6, 0x084F, 0xED00, 0xCC00, 0x10BD, 0xD343, 
	0x30EC, 0x02ED, 0x0FE3, 0x06ED, 0x0FD6, 0x0254, 0x5454, 0x54E7, 
	0x08EC, 0x09ED, 0x02E6, 0x084F, 0xED00, 0xCC00, 0x10BD, 0xD343, 
	0x30EC, 0x02ED, 0x0DE3, 0x04ED, 0x0DD6, 0x03C4, 0x0FCB, 0x01E7, 
	0x08EC, 0x0BED, 0x02E6, 0x084F, 0xED00, 0xCC00, 0x40BD, 0xD343, 
	0x30EC, 0x02ED, 0x0BD6, 0x0354, 0x5454, 0x54CB, 0x01E7, 0x08EC, 
	0x09ED, 0x02E6, 0x084F, 0xED00, 0xCC00, 0x40BD, 0xD343, 0x30EC, 
	0x02ED, 0x0905, 0x05E3, 0x0DC3, 0x000A, 0xDD04, 0xEC0D, 0xED02, 
	0xEC0F, 0xED00, 0xC608, 0xBDD3, 0x1930, 0xED11, 0xCC02, 0xC0ED, 
	0x00EC, 0x11BD, 0xD42B, 0x30EC, 0x09ED, 0x02EC, 0x0BED, 0x00C6, 
	0x02BD, 0xD319, 0x30ED, 0x11CC, 0x02C1, 0xED00, 0xEC11, 0xBDD4, 
	0x2B7F, 0x10C4, 0x30EC, 0x09C4, 0xFEFD, 0x10C5, 0xEC0B, 0xC4FE, 
	0xFD10, 0xC701, 0x0101, 0xCC02, 0xC2ED, 0x00FC, 0x10C2, 0xBDD4, 
	0x2BFC, 0x10C0, 0xCA06, 0x30ED, 0x11CC, 0x02C3, 0xED00, 0xEC11, 
	0xBDD4, 0x2B30, 0xC613, 0x3A35, 0x393C, 0xDC25, 0x30ED, 0x00BD, 
	0x81BE, 0x7D00, 0x1E27, 0x227F, 0x10C4, 0xD61E, 0x4FFD, 0x10C5, 
	0xDC2F, 0xFD10, 0xC701, 0x0101, 0xFC10, 0xC2DD, 0x25D6, 0x31C1, 
	0x0224, 0x0BC6, 0x02D7, 0x3120, 0x0530, 0xEC00, 0xDD25, 0x3839, 
	0x373C, 0x3CD6, 0x1E30, 0xE701, 0xDC25, 0xED02, 0xE607, 0xE700, 
	0xE604, 0xBD87, 0x01D6, 0x1FD1, 0x1023, 0x04D6, 0x10D7, 0x1F30, 
	0xE607, 0xC101, 0x2612, 0xD61E, 0xE101, 0x240C, 0xD610, 0xD71F, 
	0xE601, 0xD71E, 0xEC02, 0xDD25, 0x3838, 0x3139, 0x3CDE, 0x00EE, 
	0x12AD, 0x00D6, 0x4630, 0xE701, 0xC601, 0xE700, 0xE600, 0x4F8F, 
	0xE646, 0x30EB, 0x01E7, 0x016C, 0x00E6, 0x00C1, 0x0525, 0xED4F, 
	0xE601, 0x2A01, 0x43CE, 0x0005, 0xBD07, 0xD0D7, 0x4E30, 0x6F01, 
	0x96D5, 0x112F, 0x04C6, 0x01E7, 0x01E6, 0x0138, 0x393C, 0x3C3C, 
	0x3C34, 0xC620, 0xF702, 0xBD7F, 0x02BE, 0xF702, 0xBFC6, 0xF6D7, 
	0xBACC, 0x02AB, 0x30ED, 0x06FE, 0x1050, 0xEC06, 0xFD02, 0xA7FE, 
	0x02A7, 0xEC00, 0xFD02, 0xA930, 0x6F08, 0xE608, 0x4F05, 0xF302, 
	0xA98F, 0xEC00, 0x30ED, 0x00E6, 0x084F, 0x05E3, 0x0618, 0x8FEC, 
	0x0018, 0xED00, 0x6C08, 0xE608, 0xC109, 0x25DE, 0xEE06, 0xCC03, 
	0x10ED, 0x0230, 0xEE06, 0xCC04, 0x00ED, 0x04CC, 0x02AB, 0xDD58, 
	0xCC02, 0xC430, 0xED04, 0xFE10, 0x50EC, 0x04FD, 0x02C0, 0xFE02, 
	0xC0EC, 0x00FD, 0x02C2, 0x306F, 0x08E6, 0x084F, 0x05F3, 0x02C2, 
	0x8FEC, 0x0030, 0xED00, 0xE608, 0x4F05, 0xE304, 0x188F, 0xEC00, 
	0x18ED, 0x006C, 0x08E6, 0x08C1, 0x0E25, 0xDEEE, 0x04CC, 0x04FA, 
	0xED04, 0x30EE, 0x04CC, 0x0615, 0xED0A, 0x30EE, 0x04CC, 0x064C, 
	0xED0C, 0xCC02, 0xC4DD, 0x00CC, 0x02E4, 0x30ED, 0x02FE, 0x1050, 
	0xEC02, 0xFD02, 0xE0FE, 0x02E0, 0xEC00, 0xFD02, 0xE230, 0x6F08, 
	0xE608, 0x4F05, 0xF302, 0xE28F, 0xEC00, 0x30ED, 0x00E6, 0x084F, 
	0x05E3, 0x0218, 0x8FEC, 0x0018, 0xED00, 0x6C08, 0xE608, 0xC112, 
	0x25DE, 0xEE02, 0xCC06, 0x88ED, 0x1CCC, 0x02E4, 0xDDC2, 0x30C6, 
	0x093A, 0x3539, 0x8F4D, 0x2C13, 0x4353, 0x8F08, 0x4D2C, 0x0643, 
	0x5302, 0x088F, 0x3902, 0x098F, 0x4353, 0x398F, 0x4D2C, 0x0743, 
	0x5302, 0x8F43, 0x5339, 0x028F
};

static const u16 sensor2m_default_settings4[] = {
    0x87F4, 0x0001, 0x0039
};

static void sensor2m_set_defaults(const u16* values)
{
    u16 value0, value1, value2, nextreg;
    const u16 *vptr;
    int next;
    
    value0 = values[0];
    value1 = values[1];
    vptr = values+2;
    next = 0;
    while(value1>0)
    {
        if(next==0)
        {
            sensor2m_reg_write(0x1C6, value0);
        }
        value2 = *vptr++;
        nextreg = 0x1C8 + next;
        sensor2m_reg_write(nextreg, value2);

        value0 += 2;
        value1--;
        next++;
        if(next==8)
            next = 0;
    }
    return;
}

static void sensor2m_default_values(void)
{    
    sensor2m_set_defaults(sensor2m_default_settings1);
    sensor2m_set_defaults(sensor2m_default_settings2);
    sensor2m_set_defaults(sensor2m_default_settings3);
    sensor2m_set_defaults(sensor2m_default_settings4);
    sensor2m_var_write16(0, 3, 0x06C9);
    sensor2m_var_write8(0, 2, 0x0001);
    return;
}

int sensor2m_reset_init(void)
{    
    sensor2m_reset_value();
    sensor2m_default_values();
    sensor2m_set_mnp();
    return 0;
}
