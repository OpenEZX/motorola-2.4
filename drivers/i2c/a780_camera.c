/*
 *  linux/drivers/i2c/a780_camera.c
 *
 *  Support for the Motorola Ezx A780 Development Platform.
 *  
 *  Author:	    Jay Jia
 *  Created:	Nov 25, 2003
 *  Copyright:	Motorola Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *
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
#define CAM_MINOR       244

#define TESTCODE
#undef  TESTCODE
#define I2C_A780_CAMERA 0x86

//#define DEBUG 2

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
#define I2C_CLIENT_MT9M111      101
#define I2C_CLIENT_OV9640       102
#define I2C_CLIENT_OV9650       103
#define I2C_CLIENT_ADCM3800     104
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
    unsigned int flags;
 
	a780_camera_open();	
    local_irq_save(flags);
    enable_irq(IRQ_I2C);
        ret = i2c_master_recv(a780_camera_client, buf, count);
    local_irq_restore(flags);
	a780_camera_release();
	return ret;
			
}
	
int a780_camera_write(const char *buf, size_t count)
{
	int ret;
    unsigned int flags;

	a780_camera_open();	
    local_irq_save(flags);
    enable_irq(IRQ_I2C);
	ret = i2c_master_send(a780_camera_client, buf, count);
    local_irq_restore(flags);
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
#ifdef CONFIG_I2C_MT9M111
        case I2C_CLIENT_MT9M111:
            idlen = snprintf(id, DETECT_BUFLEN-1, "%s %s %lx", 
                            "MICRON", "MT9M111", i2c_camera_chipid);
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


#ifdef  CONFIG_I2C_MT9M111

/*-----------------------------------------------------------*/
#define MT9M111_PAGE(a)    ((u8)(((a) >> 8) & 0xFF))
#define MT9M111_OFFSET(a)   ((u8)((a) & 0xFF))

//#define MT9M111_LOG

#ifdef  MT9M111_LOG
#define mt9m111_dbg_print(fmt, args...) dbg_print(fmt, ##args)
#else
#define mt9m111_dbg_print(fmt, args...) ;
#endif

static int mt9m111_ChangePage(unsigned char page)
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

int mt9m111_read(unsigned short addr, unsigned short *pvalue)
{
	unsigned char	pageaddr = MT9M111_PAGE(addr);
	unsigned char	offset    = MT9M111_OFFSET(addr);
	unsigned short value;
        int     ret;

	if((ret = mt9m111_ChangePage(pageaddr)) != 0)  
	{
	    mt9m111_dbg_print("Change block address failed. block = 0x%2x", pageaddr);
            mt9m111_dbg_print("Error code = 0x%x", ret);
	    return ret;
	}
        if(a780_camera_write(&offset, 1) < 0)
        {
            mt9m111_dbg_print("read error!");
            return ret;
        }
        
        ret = a780_camera_read((char *)&value, 2); 
        if(ret < 0)
        {
           mt9m111_dbg_print("read error");
           return ret;
        } 

	*pvalue = (value >> 8) | (value << 8);
        mt9m111_dbg_print("addr = 0x%04x, value = 0x%04x", addr, *pvalue);
	return  0;
}	

int mt9m111_write(unsigned short addr, unsigned short value)
{
	unsigned char	pageaddr = MT9M111_PAGE(addr);
	unsigned char	offset    = MT9M111_OFFSET(addr);
        char    tmp[3]={offset, (char)(value>>8), (char)(value&0xFF)};
        int     ret;
		
	if((ret = mt9m111_ChangePage(pageaddr)) != 0)  
	{
	   mt9m111_dbg_print("Change block address failed. block = 0x%2x", pageaddr);
           mt9m111_dbg_print("error code = 0x%x", ret);
	   return ret;
	}
	ret = a780_camera_write(tmp, 3);
        if(ret < 0)
        {
          return ret;
        }
        mt9m111_dbg_print("addr = 0x%04x, value = 0x%04x", addr,value);
        return 0;
}

int i2c_mt9m111_cleanup(void)
{
  i2c_camera_client_type = I2C_CLIENT_NONE;
  return 0;
} 
int i2c_mt9m111_init(void)
{
  //unsigned short chipid;
  a780_camera_client->addr = 0x5D;

  //mt9m111_read(0, &chipid);
  //mt9m111_dbg_print("mt9m111 chip id is 0x%x", chipid);

  i2c_camera_client_type = I2C_CLIENT_MT9M111;
  return 0;
}

#endif  // CONFIG_I2C_MT9M111

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
    ddbg_print("read b: addr(0x%x) value(0x%x)", addr, *pvalue);
	
	return  *pvalue;
}

int i2c_adcm3800_read(unsigned short addr, unsigned short *pvalue)
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
        
    ret = a780_camera_read((char *)pvalue, 2); 
    if(ret < 0)
    {
        err_print("i2c read error code = %d", ret);
        return -EIO;
    } 
    ddbg_print("read: addr(0x%x) value(0x%x)", addr, *pvalue);
	
	return  *pvalue;
}

int i2c_adcm3800_write_byte(unsigned short addr, unsigned char value)
{
	unsigned char	blockaddr = ADCM3800_BLOCK(addr);
	unsigned char 	offset    = ADCM3800_OFFSET(addr);;
    char    tmp[3]={offset, value};
    int     ret;

    ddbg_print("write b: addr(0x%x) value(0x%x)", addr, value);
	if((ret = adcm3800_ChgBlock(blockaddr)) != 0)  
	{
	    err_print("Change block address failed. block = 0x%2x", blockaddr);
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

    ddbg_print("write: addr(0x%x) value(0x%x)", addr, value);
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
#ifdef CONFIG_I2C_MT9M111
        case I2C_CLIENT_MT9M111:
            ret = mt9m111_read(addr, pvalue);
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
#ifdef CONFIG_I2C_MT9M111
        case I2C_CLIENT_MT9M111:
            ret = mt9m111_write(addr, value);
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

#ifdef CONFIG_I2C_MT9M111
EXPORT_SYMBOL(i2c_mt9m111_init);
EXPORT_SYMBOL(mt9m111_write);
EXPORT_SYMBOL(mt9m111_read);
EXPORT_SYMBOL(i2c_mt9m111_cleanup);
#endif

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
MODULE_AUTHOR("Jay Jia");
MODULE_LICENSE("GPL");
