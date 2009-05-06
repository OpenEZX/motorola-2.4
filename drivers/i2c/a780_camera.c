/*
 * Copyright (C) 2004, 2006 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 
 * 03/22/2004   Created to support a780 camera, for EZX platform
 * 04/12/2006   Fix sensor name
 */

#include <linux/miscdevice.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/videodev.h>
#include <linux/pxa_camera.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <asm/semaphore.h>

#include <linux/sound.h>
#include <linux/soundcard.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

/* Major 10, Minor 244, /dev/camerai2c */
#define CAM_NAME        "cami2c"

#define TESTCODE
#undef  TESTCODE
#define I2C_A780_CAMERA 0x86

#define DEBUG 1

/*-----------------------------------------------------------*/
#define err_print(fmt, args...) printk(KERN_ERR "I2C-CAMERA in fun:%s "fmt"\n", __FUNCTION__, ##args)

#ifndef NDEBUG
#define dbg_print(fmt, args...) printk(KERN_INFO "I2C-CAMERA in fun:%s "fmt"\n", __FUNCTION__, ##args)
#if     DEBUG > 1
#define ddbg_print(fmt, args...) dbg_print(fmt, ##args)
#else
#define ddbg_print(fmt, args...) ;
#endif
#else
#define dbg_print(fmt, args...) ;
#define ddbg_print(fmt, args...) ;
#endif
/*-----------------------------------------------------------*/

#define I2C_CLIENT_NONE         100
#define I2C_CLIENT_SENSOR13M    101
#define I2C_CLIENT_OV9640       102
#define I2C_CLIENT_OV9650       103
#define I2C_CLIENT_ADCM3800     104
#define I2C_CLIENT_SENSOR2M     105
static int i2c_camera_client_type = I2C_CLIENT_NONE;
static unsigned long i2c_camera_chipid = 0;


static int a780_camera_adapter_attach(struct i2c_adapter *adap);
static int a780_camera_detach(struct i2c_client *client);
static int a780_camera_client_register(struct i2c_client *client);
static int a780_camera_client_unregister(struct i2c_client *client);
/* ----------------------------------------------------------------------- */
static struct i2c_driver driver = {
	name:            "a780 camera driver",
	id:              I2C_A780_CAMERA,
	flags:           I2C_DF_DUMMY,
	attach_adapter:  a780_camera_adapter_attach,        
	detach_client:   a780_camera_detach,
};

static struct i2c_adapter a780_camera_adapter = {
        name:                   "a780 camera adapter",
        id:                     I2C_A780_CAMERA,
        client_register:        a780_camera_client_register,
        client_unregister:      a780_camera_client_unregister,
};

static struct i2c_client client_template =
{
    name:   "(unset)",        
    adapter:&a780_camera_adapter,
};

struct i2c_client *a780_camera_client;
unsigned int a780_camera_minor;
	
static int a780_camera_open(void)
{
	MOD_INC_USE_COUNT;
	
	return 0;
}

static int a780_camera_release(void)
{
	MOD_DEC_USE_COUNT;
	return 0;
}


int a780_camera_read(char *buf, size_t count)
{
    int ret;
 
	a780_camera_open();	
    ret = i2c_master_recv(a780_camera_client, buf, count);

	a780_camera_release();
	return ret;
			
}
	
int a780_camera_write(const char *buf, size_t count)
{
	int ret;

	a780_camera_open();	

	ret = i2c_master_send(a780_camera_client, buf, count);

	a780_camera_release();
	return ret;
}

int a780_camera_transfer(struct i2c_msg msgs[],int num)
{
   int ret;

   a780_camera_open();

   ret = i2c_transfer(a780_camera_client->adapter, msgs, num);

   a780_camera_release();
   return ret;
}

static int a780_camera_client_register(struct i2c_client *client)
{
	
	return 0;
}

static int a780_camera_client_unregister(struct i2c_client *client)
{
	
	return 0;	
}
/* ----------------------------------------------------------------------- */

static int a780_camera_adapter_attach(struct i2c_adapter *adap)
{
	if(! (a780_camera_client = kmalloc(sizeof(struct i2c_client),GFP_KERNEL)))
		return -ENOMEM;
	memcpy(a780_camera_client,&client_template,sizeof(struct i2c_client));
	a780_camera_client->adapter = adap;
        
	a780_camera_client->addr = 0x5D;
	
	i2c_attach_client(a780_camera_client);
	return 0;
}	

static int a780_camera_detach(struct i2c_client *client)
{	
	i2c_detach_client(a780_camera_client);
	return 0;
}
/* ----------------------------------------------------------------------- */
static int cam_open(struct inode *inode, struct file *file)
{
        if(i2c_camera_client_type == I2C_CLIENT_NONE)
            return -EINVAL;

        MOD_INC_USE_COUNT;
        return 0;
}

static int i2c_camera_readw(unsigned short addr, unsigned short *pvalue);
static int i2c_camera_readb(unsigned short addr, unsigned char *pvalue);
static int i2c_camera_writew(unsigned short addr, unsigned short value);
static int i2c_camera_writeb(unsigned short addr, unsigned char value);

static int cam_close(struct inode * inode, struct file *file)
{
        MOD_DEC_USE_COUNT;
        return 0;
}

#define DETECT_BUFLEN 256
static int cam_ioctl_detectid (void * arg)
{
    int    buflen, idlen;
    char*  id;
    struct camera_i2c_detectid * param = arg;
    if(copy_from_user(&buflen, &(param->buflen), sizeof(buflen)))
    {
        return -EFAULT;
    }
    if(buflen > DETECT_BUFLEN)
    {
        return -ENOMEM;
    }
    id = kmalloc(DETECT_BUFLEN, GFP_KERNEL);
    if(id == NULL)
    {
        return -ENOMEM;
    }

    idlen = 0;
    switch(i2c_camera_client_type)
    {
#ifdef CONFIG_I2C_SENSOR13M
        case I2C_CLIENT_SENSOR13M:
            idlen = snprintf(id, DETECT_BUFLEN-1, "%s %s %lx", 
                            "SENSOR", "SENSOR13M", i2c_camera_chipid);
            break;
#endif
#ifdef CONFIG_I2C_ADCM3800
        case I2C_CLIENT_ADCM3800:
            idlen = snprintf(id, DETECT_BUFLEN-1, "%s %s %lx", 
                            "AGILENT", "ADCM3800", i2c_camera_chipid);
            break;
#endif
#ifdef CONFIG_I2C_OV9640
        case I2C_CLIENT_OV9640:
            idlen = snprintf(id, DETECT_BUFLEN-1, "%s %s %lx", 
                            "OMNIVISION", "OV9640", i2c_camera_chipid);
            break;
#endif
#ifdef CONFIG_I2C_OV9650
        case I2C_CLIENT_OV9650:
            idlen = snprintf(id, DETECT_BUFLEN-1, "%s %s %lx", 
                            "OMNIVISION", "OV9650", i2c_camera_chipid);
            break;
#endif
#ifdef CONFIG_I2C_SENSOR2M
        case I2C_CLIENT_SENSOR2M:
            idlen = snprintf(id, DETECT_BUFLEN-1, "%s %s %lx", 
                            "SENSOR", "SENSOR2M", i2c_camera_chipid);
            break;
#endif
        default:
            break;
    }
    id[DETECT_BUFLEN-1] = 0;
    idlen = strlen(id)+1;
    if(buflen < idlen)
    {
        kfree(id);
        return -ENOMEM;
    }
    if(copy_to_user(param->data, id, idlen))
    {
        kfree(id);
        return -EFAULT;
    }
    kfree(id);
    return 0;
}

static int cam_ioctl_register_rw (unsigned int cmd, void * arg)
{
    int ret = -ENOTSUPP;
    struct camera_i2c_register reg;
    if(copy_from_user(&reg, arg, sizeof(reg)))
    {
        return -EFAULT;
    }
    switch(cmd)
    {
        case CAMERA_I2C_WRITEW:
            ret=i2c_camera_writew(reg.addr, reg.value.w);
            break;
        case CAMERA_I2C_WRITEB:
            ret=i2c_camera_writeb(reg.addr, reg.value.b);
            break;
        case CAMERA_I2C_READW:
            if((ret=i2c_camera_readw(reg.addr, &(reg.value.w)))>=0)
            {
                if(copy_to_user(arg, &reg, sizeof(reg)))
                    ret = -EFAULT;
            }
            break;
        case CAMERA_I2C_READB:
            if((ret=i2c_camera_readb(reg.addr, &(reg.value.b)))>=0)
            {
                if(copy_to_user(arg, &reg, sizeof(reg)))
                    ret = -EFAULT;
            }
            break;
        default:
            break;
    }
    return ret;
}

static int cam_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = -ENOTSUPP;
    switch (cmd)
    {
        case CAMERA_I2C_WRITEW:
        case CAMERA_I2C_WRITEB:
        case CAMERA_I2C_READW:
        case CAMERA_I2C_READB:
            ret = cam_ioctl_register_rw(cmd, (void *)arg);
            break;
        case CAMERA_I2C_DETECTID:
            ret = cam_ioctl_detectid((void *)arg);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static struct file_operations cam_fops = {
        ioctl:          cam_ioctl,
        open:           cam_open,
        release:        cam_close,
};

static struct miscdevice cam_misc_device = {
        CAM_MINOR,
        CAM_NAME,
        &cam_fops,
};

/* ----------------------------------------------------------------------- */
static int a780_camera_init_module(void)
{
	int res;
	
	res = i2c_add_driver(&driver);
	if( res < 0 )
	{
		printk("error in add i2c driver\n");
		return res;
	}
    if (misc_register (&cam_misc_device))
    {
        printk(KERN_ERR "Couldn't register cam driver\n");
        return -EIO;
    }

	return 0;
}

static void a780_camera_cleanup_module(void)
{	
	i2c_del_driver(&driver);
    misc_deregister(&cam_misc_device);
}


#ifdef  CONFIG_I2C_SENSOR13M

/*-----------------------------------------------------------*/
#define SENSOR13M_PAGE(a)    ((u8)(((a) >> 8) & 0xFF))
#define SENSOR13M_OFFSET(a)   ((u8)((a) & 0xFF))

static int sensor13m_ChangePage(unsigned char page)
{
    static unsigned char old_page = 0xFF;
    int res;
    char tmp[3]={0xF0, 0, page};
    
    if(page == old_page)
    {
	return 0;
    }
    
    res = a780_camera_write(tmp, 3);
    if(res < 0)
    {
	return  res;
    }
    old_page = page;
    
    return 0; 
}

int sensor13m_read(unsigned short addr, unsigned short *pvalue)
{
	unsigned char	pageaddr = SENSOR13M_PAGE(addr);
	unsigned char	offset    = SENSOR13M_OFFSET(addr);
	unsigned short value;
        int     ret;
	int	msgnum = 2;
	struct i2c_msg	msg[2];

	if((ret = sensor13m_ChangePage(pageaddr)) != 0)  
	{
            err_print("Change block address failed. block(0x%02x), addr(0x%04x)", pageaddr, addr);
            err_print("Error code = %d", ret);
	    return ret;
	}

	msg[0].addr = a780_camera_client->addr;
	msg[0].flags = a780_camera_client->flags | I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &offset;
	msg[1].addr = a780_camera_client->addr;
	msg[1].flags = a780_camera_client->flags | I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = (char*)&value;
	ret = a780_camera_transfer(msg, msgnum);
	if(ret<0)
        {
            err_print("read error! address=%04x, return code=%d", addr, ret);
            return ret;
        }

	*pvalue = (value >> 8) | (value << 8);
        ddbg_print("addr = 0x%04x, value = 0x%04x", addr, *pvalue);
	return  0;
}	

int sensor13m_write(unsigned short addr, unsigned short value)
{
	unsigned char	pageaddr = SENSOR13M_PAGE(addr);
	unsigned char	offset    = SENSOR13M_OFFSET(addr);
        char    tmp[3]={offset, (char)(value>>8), (char)(value&0xFF)};
        int     ret;
		
	if((ret = sensor13m_ChangePage(pageaddr)) != 0)  
	{
           err_print("Change block address failed. block = 0x%02x", pageaddr);
           err_print("error code = %d", ret);
	   return ret;
	}
	ret = a780_camera_write(tmp, 3);
        if(ret < 0)
        {
          err_print("write error! address=%04x, return code=%d", addr, ret);
          return ret;
        }
        ddbg_print("addr = 0x%04x, value = 0x%04x", addr,value);
        return 0;
}

int i2c_sensor13m_cleanup(void)
{
  ddbg_print("close!");
  i2c_camera_client_type = I2C_CLIENT_NONE;
  return 0;
}

int i2c_sensor13m_init(void)
{
  int ret;
  ddbg_print("open!");

  unsigned short chipid;
  a780_camera_client->addr = 0x5D;

  ret = sensor13m_read(0, &chipid);
  if(ret<0)
  {
    err_print("error: failed to read chipid");
    return -EIO;
  }

  ddbg_print("sensor13m chip id is 0x%x", chipid);

  i2c_camera_client_type = I2C_CLIENT_SENSOR13M;
  i2c_camera_chipid = chipid;
  return 0;
}

#ifdef  CONFIG_I2C_SENSOR2M
int i2c_sensor2m_init(void)
{
  int ret;
  ddbg_print("open!");

  unsigned short chipid;
  a780_camera_client->addr = 0x48;

  ret = sensor13m_read(0, &chipid);
  if(ret<0)
  {
    err_print("error: failed to read chipid");
    return -EIO;
  }

  ddbg_print("sensor2m chip id is 0x%x", chipid);

  i2c_camera_client_type = I2C_CLIENT_SENSOR2M;
  i2c_camera_chipid = chipid;
  return 0;
}

int i2c_sensor2m_cleanup(void)
{
  ddbg_print("close!");
  i2c_camera_client_type = I2C_CLIENT_NONE;
  return 0;
}

int i2c_sensor2m_read(unsigned short addr, unsigned short *pvalue)
{
    return sensor13m_read(addr, pvalue);
}

int i2c_sensor2m_write(unsigned short addr, unsigned short value)
{
    return sensor13m_write(addr, value);
}
#endif  // CONFIG_I2C_SENSOR2M

#endif  // CONFIG_I2C_SENSOR13M

#ifdef CONFIG_I2C_OV9650

#define OV_REG_PID  0x0a
#define OV_REG_VER  0x0b
#define OV9650_CHIPID_M    0x96
#define OV9650_CHIPID_L_0  0x51
#define OV9650_CHIPID_L    0x52

int i2c_ov9650_read(u8 addr, u8 *pvalue)
{
    int     ret;

    if((ret = a780_camera_write(&addr, 1)) < 0)
    {
        err_print("i2c write error code =%d", ret);
        return -EIO;
    }
        
    ret = a780_camera_read((char *)pvalue, 1); 
    if(ret < 0)
    {
        err_print("i2c read error code =%d", ret);
        return -EIO;
    } 

    ddbg_print("addr = 0x%02x, value = 0x%02x", addr, *pvalue);
	return  0;
}	

int i2c_ov9650_write(u8 addr, u8 value)
{
    char    tmp[2]={addr, value};
    int     ret;
		
	ret = a780_camera_write(tmp, 2);
    if(ret < 0)
    {
        err_print("i2c write error code =%d", ret);
        return -EIO;
    }

    ddbg_print("addr = 0x%02x, value = 0x%02x", addr,value);
    return 0;
}

int i2c_ov9650_cleanup(void)
{
    i2c_camera_client_type = I2C_CLIENT_NONE;
    return 0;
}

int i2c_ov9650_init(void)
{
    unsigned char chipid;
    unsigned char ver;
    // setup i2c address
    a780_camera_client->addr = 0x30;    //0x60;

    if( i2c_ov9650_read(OV_REG_PID, &chipid) < 0 )
    {
        err_print("error: failed to read chipid");
        return -EIO;
    }
    if( chipid != OV9650_CHIPID_M )
    {
        err_print("chip is not Omnivision");
        return -EIO;
    }

    i2c_ov9650_read(OV_REG_VER, &ver);
    dbg_print("ov chip id is 0x%x:0x%x", chipid, ver);

    if( ver != OV9650_CHIPID_L && ver != OV9650_CHIPID_L_0 )
    {
        err_print("chip is not OV9650");
        return -EIO;
    }
    i2c_camera_client_type = I2C_CLIENT_OV9650;
    i2c_camera_chipid = (chipid<<8) | ver;
    
    return 0;
}

#ifdef  CONFIG_I2C_OV9640
int ov9640_read(u8 addr, u8 *pvalue)
{
    return i2c_ov9650_read(addr, pvalue);
}

int ov9640_write(u8 addr, u8 value)
{
    return i2c_ov9650_write(addr, value);
}

int i2c_ov9640_cleanup(void)
{
    i2c_camera_client_type = I2C_CLIENT_NONE;
    return 0;
}

#define OV9640_CHIPID_M  0x96
#define OV9640_CHIPID_L  0x49
#define OV9640_CHIPID_L_V2  0x48

int i2c_ov9640_init(void)
{
    unsigned char chipid;
    unsigned char ver;
    // setup i2c address
    a780_camera_client->addr = 0x30;    //0x60;

    if( ov9640_read(OV_REG_PID, &chipid) < 0 )
    {
        err_print("error: failed to read chipid");
        return -EIO;
    }
    if( chipid != OV9640_CHIPID_M )
    {
        err_print("chip is not Omnivision");
        return -EIO;
    }

    ov9640_read(OV_REG_VER, &ver);
    dbg_print("ov chip id is 0x%x:0x%x", chipid, ver);

    if( ver != OV9640_CHIPID_L && ver != OV9640_CHIPID_L_V2 )
    {
        err_print("chip is not OV9640");
        return -EIO;
    }
    i2c_camera_client_type = I2C_CLIENT_OV9640;
    i2c_camera_chipid = (chipid<<8) | ver;
    
    return 0;
}
#endif  // CONFIG_I2C_OV9640

#endif  // CONFIG_I2C_OV9650

#ifdef CONFIG_I2C_ADCM3800

#define ADCM3800_I2C_ADDR           0x56
#define ADCM3800_BLOCK_SWITCH_CMD   0xFE

/* Calculating the Module Block Number */
#define ADCM3800_BLOCK(a)    ((u8)(((a) >> 7) & 0xFF))     /* register's module block address. */
#define ADCM3800_OFFSET(a)   ((u8)(((a) << 1) & 0xFF))     /* register's offset to this block. */
#define ADCM3800_OFFSET_R(a) (ADCM3800_OFFSET(a)|1)                 /* register's offset ot this block to read*/     

static int adcm3800_ChgBlock(unsigned char block)
{
    static unsigned char old_blk = 0xFF;
    int res;
    char tmp[2]={ADCM3800_BLOCK_SWITCH_CMD, block};
    
    if(block == old_blk)
    {
	    return 0;
    }
    
    res = a780_camera_write(tmp, 2);
    if(res < 0)
    {
        err_print("error code = %d", res);
	    return  -EIO;
    }
    old_blk = block;
    
    return 0; 
}

int i2c_adcm3800_read_byte(unsigned short addr, unsigned char *pvalue)
{
	unsigned char	blockaddr = ADCM3800_BLOCK(addr);
	unsigned char	offset    = ADCM3800_OFFSET_R(addr);
    int     ret;

	if((ret = adcm3800_ChgBlock(blockaddr)) != 0)  
	{
	    err_print("Change block address failed. block = 0x%2x", blockaddr);
	    return -EIO;
	}
    if((ret = a780_camera_write(&offset, 1)) < 0)
    {
        err_print("i2c write error code =%d", ret);
        return -EIO;
    }

    ret = a780_camera_read((char *)pvalue, 1); 
    if(ret < 0)
    {
        err_print("i2c read error code = %d", ret);
        return -EIO;
    } 
    ddbg_print("read b: addr(0x%04x) value(0x%02x)", addr, *pvalue);
	
	return  *pvalue;
}

int i2c_adcm3800_read(unsigned short addr, unsigned short *pvalue)
{
	unsigned char	blockaddr = ADCM3800_BLOCK(addr);
	unsigned char	offset    = ADCM3800_OFFSET_R(addr);
    int     ret;

	if((ret = adcm3800_ChgBlock(blockaddr)) != 0)  
	{
	    err_print("Change block address failed. block = 0x%02x", blockaddr);
	    return -EIO;
	}
    if((ret = a780_camera_write(&offset, 1)) < 0)
    {
        err_print("i2c write error code =%d", ret);
        return -EIO;
    }
        
    ret = a780_camera_read((char *)pvalue, 2); 
    if(ret < 0)
    {
        err_print("i2c read error code = %d", ret);
        return -EIO;
    } 
    ddbg_print("read: addr(0x%04x) value(0x%04x)", addr, *pvalue);
	
	return  *pvalue;
}

int i2c_adcm3800_write_byte(unsigned short addr, unsigned char value)
{
	unsigned char	blockaddr = ADCM3800_BLOCK(addr);
	unsigned char 	offset    = ADCM3800_OFFSET(addr);;
    char    tmp[3]={offset, value};
    int     ret;

    ddbg_print("write b: addr(0x%04x) value(0x%02x)", addr, value);
	if((ret = adcm3800_ChgBlock(blockaddr)) != 0)  
	{
	    err_print("Change block address failed. block = 0x%02x", blockaddr);
	    return -EIO;
	}
	ret = a780_camera_write(tmp, 2);
    if(ret < 0)
    {
        err_print("i2c write error code = %d", ret);
        return -EIO;
    }
    return 0;
 
}
int i2c_adcm3800_write(unsigned short addr, unsigned short value)
{
	unsigned char	blockaddr = ADCM3800_BLOCK(addr);
	unsigned char 	offset    = ADCM3800_OFFSET(addr);;
    char    tmp[3]={offset, (char)(value&0xFF), (char)(value>>8)};
    int     ret;

    ddbg_print("write: addr(0x%04x) value(0x%04x)", addr, value);
	if((ret = adcm3800_ChgBlock(blockaddr)) != 0)  
	{
	    err_print("Change block address failed. block = 0x%2x", blockaddr);
	    return -EIO;
	}
	ret = a780_camera_write(tmp, 3);
    if(ret < 0)
    {
        err_print("i2c write error code = %d", ret);
        return -EIO;
    }
    return 0;
}

int i2c_adcm3800_cleanup(void)
{
    i2c_camera_client_type = I2C_CLIENT_NONE;
    return 0;
}

#define ADCM_SREG_PID  0x0000
#define ADCM_EREG_PID  0x0800

#define ADCM3800_PIPE_REV       0x0068
#define ADCM3800_SENSOR_REV     0x68

int i2c_adcm3800_init(void)
{
    unsigned short pid_pipe;
    unsigned char pid_sensor;
    a780_camera_client->addr = ADCM3800_I2C_ADDR;
 
    if(i2c_adcm3800_read(ADCM_SREG_PID, &pid_pipe) < 0)
    {
        return -EIO;
    }
    if(i2c_adcm3800_read_byte(ADCM_EREG_PID, &pid_sensor) < 0)
    {
        return -EIO;
    }

    dbg_print("pipe id is 0x%x, sensor id is 0x%x", pid_pipe, pid_sensor);
    if(pid_pipe != ADCM3800_PIPE_REV && pid_sensor != ADCM3800_SENSOR_REV)
    {
        err_print("sensor is not Agilent ADCM3800");
        return -EIO;
    } 
    i2c_camera_client_type = I2C_CLIENT_ADCM3800;
    i2c_camera_chipid = (pid_pipe<<8) | pid_sensor;

    return 0;
}
#endif // CONFIG_I2C_ADCM3800

static int i2c_camera_readw(unsigned short addr, unsigned short *pvalue)
{
    int ret = -ENOTSUPP;
    switch(i2c_camera_client_type)
    {
#ifdef CONFIG_I2C_SENSOR13M
        case I2C_CLIENT_SENSOR13M:
            ret = sensor13m_read(addr, pvalue);
            break;
#endif
#ifdef CONFIG_I2C_SENSOR2M
        case I2C_CLIENT_SENSOR2M:
            ret = i2c_sensor2m_read(addr, pvalue);
            break;
#endif
#ifdef CONFIG_I2C_ADCM3800
        case I2C_CLIENT_ADCM3800:
            ret = i2c_adcm3800_read(addr, pvalue);
            break;
#endif
        default:
            break;
    }
    return ret;
}

static int i2c_camera_readb(unsigned short addr, unsigned char *pvalue)
{
    int ret = -ENOTSUPP;
    switch(i2c_camera_client_type)
    {
#ifdef CONFIG_I2C_ADCM3800
        case I2C_CLIENT_ADCM3800:
            ret = i2c_adcm3800_read_byte(addr, pvalue);
            break;
#endif
#ifdef CONFIG_I2C_OV9640
        case I2C_CLIENT_OV9640:
            ret = ov9640_read(addr, pvalue);
            break;
#endif
#ifdef CONFIG_I2C_OV9650
        case I2C_CLIENT_OV9650:
            ret = i2c_ov9650_read(addr, pvalue);
            break;
#endif
        default:
            break;
    }
    return ret;
}

static int i2c_camera_writew(unsigned short addr, unsigned short value)
{
    int ret = -ENOTSUPP;
    switch(i2c_camera_client_type)
    {
#ifdef CONFIG_I2C_SENSOR13M
        case I2C_CLIENT_SENSOR13M:
            ret = sensor13m_write(addr, value);
            break;
#endif
#ifdef CONFIG_I2C_SENSOR2M
        case I2C_CLIENT_SENSOR2M:
            ret = i2c_sensor2m_write(addr, value);
            break;
#endif
#ifdef CONFIG_I2C_ADCM3800
        case I2C_CLIENT_ADCM3800:
            ret = i2c_adcm3800_write(addr, value);
            break;
#endif
        default:
            break;
    }
    return ret;
}

static int i2c_camera_writeb(unsigned short addr, unsigned char value)
{
    int ret = -ENOTSUPP;
    switch(i2c_camera_client_type)
    {
#ifdef CONFIG_I2C_ADCM3800
        case I2C_CLIENT_ADCM3800:
            ret = i2c_adcm3800_write_byte(addr, value);
            break;
#endif
#ifdef CONFIG_I2C_OV9640
        case I2C_CLIENT_OV9640:
            ret = ov9640_write(addr, value);
            break;
#endif
#ifdef CONFIG_I2C_OV9650
        case I2C_CLIENT_OV9650:
            ret = i2c_ov9650_write(addr, value);
            break;
#endif
        default:
            break;
    }
    return ret;
}

#ifdef CONFIG_I2C_SENSOR13M
EXPORT_SYMBOL(i2c_sensor13m_init);
EXPORT_SYMBOL(sensor13m_write);
EXPORT_SYMBOL(sensor13m_read);
EXPORT_SYMBOL(i2c_sensor13m_cleanup);
#ifdef CONFIG_I2C_SENSOR2M
EXPORT_SYMBOL(i2c_sensor2m_init);
EXPORT_SYMBOL(i2c_sensor2m_write);
EXPORT_SYMBOL(i2c_sensor2m_read);
EXPORT_SYMBOL(i2c_sensor2m_cleanup);
#endif //CONFIG_I2C_SENSOR2M
#endif //CONFIG_I2C_SENSOR13M

#ifdef CONFIG_I2C_OV9650
EXPORT_SYMBOL(i2c_ov9650_init);
EXPORT_SYMBOL(i2c_ov9650_write);
EXPORT_SYMBOL(i2c_ov9650_read);
EXPORT_SYMBOL(i2c_ov9650_cleanup);
#endif

#ifdef CONFIG_I2C_OV9640
EXPORT_SYMBOL(i2c_ov9640_init);
EXPORT_SYMBOL(ov9640_write);
EXPORT_SYMBOL(ov9640_read);
EXPORT_SYMBOL(i2c_ov9640_cleanup);
#endif

#ifdef CONFIG_I2C_ADCM3800
EXPORT_SYMBOL(i2c_adcm3800_init);
EXPORT_SYMBOL(i2c_adcm3800_write);
EXPORT_SYMBOL(i2c_adcm3800_read);
EXPORT_SYMBOL(i2c_adcm3800_write_byte);
EXPORT_SYMBOL(i2c_adcm3800_read_byte);
EXPORT_SYMBOL(i2c_adcm3800_cleanup);
#endif

module_init(a780_camera_init_module);
module_exit(a780_camera_cleanup_module);
MODULE_AUTHOR("motorola");
MODULE_LICENSE("GPL");
