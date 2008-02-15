/*
 *  dsplog.c created in 08/6/2004
 *
 *  Copyright (c) 2004 Motoroal
 *  This Program just implements a USB DSP Log driver
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/usb.h>
#include <linux/poll.h>

#include <linux/major.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

/*Macro defined for this driver*/
#define DRIVER_VERSION      ""
#define DRIVER_AUTHOR       ""
#define DRIVER_DESC         "USB DSP LOG Driver"

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");


/* USB device product and vendor ID */
#define MOTO_DSPLOG_VID		0x22b8
#define MOTO_DSPLOG_PID		0x3006

/* the macro for detect the endpoint type */
#define IS_EP_BULK(ep)     ((ep).bmAttributes == USB_ENDPOINT_XFER_BULK ? 1 : 0)
#define IS_EP_BULK_IN(ep)  (IS_EP_BULK(ep) && ((ep).bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)
#define IS_EP_BULK_OUT(ep) (IS_EP_BULK(ep) && ((ep).bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)

/* global values defined */
/* the following value for block read */
static wait_queue_head_t        dl_wait;
static struct fasync_struct     *dl_fasync;
static struct timer_list        resub_wait_timer;
static wait_queue_head_t        resub_wait;

#define MAX_URB_SIZE  16 

/* DSPLOG USB interface number */
#define DSPLOG_IF_INDEX    2

/* DSPLOG minor number */
#define DSPLOG_MINOR       0x15

#define DL_DRV_NAME        "usbdl"

/* Debug macro define */
//#define DL_DEBUG		

#ifdef  DL_DEBUG
#define dl_dbg(format, arg...) printk(__FILE__ ": " format "\n" , ## arg)
#else
#define dl_dbg(format, arg...) do {} while (0)
#endif

#ifdef CONFIG_DEVFS_FS
/* The External Parameter for Register The USB Device */
extern devfs_handle_t usb_devfs_handle;
#else
#define DSPLOG_MAJOR    MISC_MAJOR
#endif

#define MAX_AP_READ   2048
#define MAX_READ_SIZE 4096
unsigned char gBuffer[MAX_READ_SIZE];

/* Internal File-System entry fucntion declaration */
static int dsplog_fasync(int fd, struct file *filp, int mode);
static int dsplog_open(struct inode * inode, struct file * file);
static int dsplog_release(struct inode * inode, struct file * filp);
ssize_t dsplog_read(struct file * filp, char * buf, size_t count, loff_t * l);
static unsigned int dsplog_poll(struct file * filp, struct poll_table_struct * wait);

/* Internal USB entry function declaration */
static void *usb_dl_probe(struct usb_device *usbdev, unsigned int ifnum, const struct usb_device_id *id);
static void usb_dl_disconnect(struct usb_device *usbdev, void *ptr);

/* file operation struction */
static struct file_operations dsplog_fops = {
    owner:          THIS_MODULE,
    open:           dsplog_open,
    release:        dsplog_release,
    read:           dsplog_read,
    poll:           dsplog_poll,
    fasync:         dsplog_fasync,
};

/*file device number*/
static struct miscdevice usbdl_misc_device = {
    DSPLOG_MINOR, "usbdl", &dsplog_fops
};

/* DSPLOG data structure */
struct dl_usb_data
{
	struct usb_device 	 *dl_dev;   /* USB device handle */	
	struct urb 	         dl_urb;	/* urbs */
	char                     *ibuf;
	char                     dl_ep;	    /* Endpoint assignments */
#ifdef CONFIG_DEVFS_FS
	devfs_handle_t           devfs;	    /* devfs device */
#else
	int                      devfs;
#endif
	int                      isopen;
	int                      probe_flag;
	int                      dataNum;
	int                      gNum;
	int                      gRead;
	int                      gWrite;
	int                      bufSize;
        spinlock_t               lock;
} dl_usb;

/* USB device ID information */
static struct usb_device_id usb_dl_id_table [] = {
	{ USB_DEVICE(MOTO_DSPLOG_VID, MOTO_DSPLOG_PID) },
	{ }						/* Terminating entry */
};

/* USB host stack entry fucntion for this driver */
static struct usb_driver usb_dl_driver = {
	name:		DL_DRV_NAME,
	probe:		usb_dl_probe,
	disconnect:	usb_dl_disconnect,
	fops:		&dsplog_fops,
	minor:          DSPLOG_MINOR,
//	id_table:	usb_dl_id_table,
	id_table:	NULL,
};

/******************************************************************************
 * Function Name: comx_get_dsr
 *
 * Input:           pInode    :  file node
 * Value Returned:  int       : -1 = false, other = true
 *
 * Description: This function is used to check whether the file node is correct.
 *
 * Modification History:
 *  
 *****************************************************************************/
static int checkDevice(struct inode *pInode)
{
    int minor;
    kdev_t dev = pInode->i_rdev;

#ifdef CONFIG_DEVFS_FS    
    if( MAJOR(dev) != USB_MAJOR)     {
        dl_dbg("checkDevice bad major = %d\n",MAJOR(dev) );
        return -1;
    }
    minor = MINOR(dev);
    if ( minor !=  DSPLOG_MINOR ) {
        printk("checkDevice bad minor = %d\n",minor );
        return -1;
    }
    return minor;
#else
    if( MAJOR(dev) != DSPLOG_MAJOR)     {
        dl_dbg("checkDevice bad major = %d\n",MAJOR(dev) );
        return -1;
    }

    minor = MINOR(dev);
    if ( minor !=  DSPLOG_MINOR ) {
        printk("checkDevice bad minor = %d\n",minor );
        return -1;
    }
    return minor;
#endif
}


/******************************************************************************
 * Function Name: dsplog_open
 *
 * Input:           pInode    :  file node
 *                  file      :  file pointer
 * Value Returned:  int       : -EBUSY = this node has been opened, 
 *                            : 0 = true
 *
 * Description: This function is used to open this driver.
 *
 * Modification History:
 *  
 *****************************************************************************/
static int
dsplog_open(struct inode * inode, struct file * file)
{
    int retval;

    dl_dbg("open_dsplog");
    if( dl_usb.isopen ) {
	dl_dbg("dsplog open error");
	return -EBUSY;
    }
    
    MOD_INC_USE_COUNT;
    dl_usb.isopen = 1;
	
    dl_usb.dl_urb.dev = dl_usb.dl_dev;
    dl_usb.dl_urb.actual_length = 0;
        
    /*  does not need ??? */
    set_GPIO_mode(GPIO_IN | 41);
    if( GPIO_is_high(41) )  {
	GPCR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
	mdelay(20);
	GPSR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
       	dl_dbg("wakeup BP");
    }
	
    // a2590c 6-24
    if( (UHCRHPS3 & 0x4) == 0x4 )  {
	UHCRHPS3 = 0x8;
	mdelay(40);
	dl_dbg("send RESUME signal");
    }

    if( (retval = usb_submit_urb(&dl_usb.dl_urb) ) ) {
       	// submit URB error, return 0.
       	printk("submit urb is error, retval = %d",retval);
       	return -1;
    }

    return 0;
}

/******************************************************************************
 * Function Name: dsplog_release
 *
 * Input:           pInode    :  file node
 *                  filp      :  file pointer
 * Value Returned:  int       :  always 0 
 *
 * Description: This function is used to close the opened driver.
 *
 * Modification History:
 *  
 *****************************************************************************/
static int
dsplog_release(struct inode * inode, struct file * filp)
{
    dl_dbg("dsp_release\n");
    if( !dl_usb.isopen ) {
    	// not opened, directly return.
    	return 0;
    }
	
    dsplog_fasync(-1, filp, 0);
    dl_usb.isopen = 0;
    MOD_DEC_USE_COUNT;

    return 0;
}

/******************************************************************************
 * Function Name: dsplog_read
 *
 * Input:           filp      :  file pointer
 *                  buf       :  the buffer for save the read data
 *                  count     :  the read buffer size
 *                  l         :  
 * Value Returned:  int       :  the real data number in the read buffer
 *
 * Description: This function is used to read data.
 *
 * Modification History:
 *  
 *****************************************************************************/
ssize_t dsplog_read(struct file * filp, char * buf, size_t count, loff_t * l)
{       
    int nonBlocking = filp->f_flags & O_NONBLOCK;
    int minor,cnt,retval,result,i;
    unsigned long flag;
    
    minor = checkDevice( filp->f_dentry->d_inode );
    if ( minor == - 1)  {
        return -ENODEV;
    }

    // read count<=0, return 0
    if ( count <= 0 )
	return 0;
    
    if ( count < MAX_AP_READ) 
         result = count;
    else 
         result = MAX_AP_READ;
    
    interruptible_sleep_on(&dl_wait);
    if(signal_pending(current)) {
           printk("dsplog_usb signal error\n");
            return -ERESTARTSYS;
    }
    if(dl_usb.gNum < MAX_AP_READ ) {
           printk("dsplog gNum is error\n");
           return 0;
    }    
    if( (dl_usb.gWrite + result) > MAX_READ_SIZE )  {
        cnt = MAX_READ_SIZE - dl_usb.gWrite;
        __copy_to_user(buf, (char*)&gBuffer[dl_usb.gWrite], cnt );
        __copy_to_user(&buf[cnt], (char*)&gBuffer[0], result - cnt );
    } else {
        __copy_to_user(buf, (char*)&gBuffer[dl_usb.gWrite], result );
    }

    dl_usb.gWrite += result;
    dl_usb.gWrite %= MAX_READ_SIZE;

    spin_lock_irqsave(&dl_usb.lock,flag); 
    dl_usb.gNum -= result;
    spin_unlock_irqrestore(&dl_usb.lock,flag);    
    
    return result;
}

/******************************************************************************
 * Function Name: dsplog_fasync
 *
 * Input:           fd        :  
 *                  filp      :  
 *                  mode      :  
 * Value Returned:  int       :  the status value
 *
 * Description: This function is used to for fasync entry.
 *
 * Modification History:
 *  
 *****************************************************************************/
static int dsplog_fasync(int fd, struct file *filp, int mode)
{
    int minor = checkDevice( filp->f_dentry->d_inode);

    if ( minor == - 1)     {
        dl_dbg("smk_ts_fasyn:bad device minor");
        return -ENODEV;
    }
    return( fasync_helper(fd, filp, mode, &dl_fasync) );
}

/******************************************************************************
 * Function Name: dsplog_fasync
 *
 * Input:           filp      :  
 *                  wait      :  
 * Value Returned:  int       :  the status value
 *
 * Description: This function is used to for poll entry.
 *
 * Modification History:
 *  
 *****************************************************************************/
static unsigned int dsplog_poll(struct file * filp, struct poll_table_struct * wait)
{
    int minor;

    minor = checkDevice( filp->f_dentry->d_inode);
    if ( minor == - 1)  {
	dl_dbg("check device major/minor number error");
        return -ENODEV;
    }
    poll_wait(filp, &dl_wait, wait);
    if( dl_usb.dataNum <= 0 )  {
        return 0;
    }
    return (POLLIN | POLLRDNORM);
}

/******************************************************************************
 * Function Name:  usb_dl_callback
 *
 * Input:           urb       :  the current urb pointer
 * Value Returned:  void
 *
 * Description: This function is used as the call back function for DSPLOG bulk IN.
 *
 * Modification History:
 *  
 *****************************************************************************/
static void usb_dl_callback(struct urb *urb)
{
	int retval,i;
        unsigned long flag;
	
 	dl_dbg("usb_dl_callback, %d",urb->actual_length);
	dl_usb.dataNum = urb->actual_length;

        spin_lock_irqsave(&dl_usb.lock,flag);
        for(i = 0; i < dl_usb.dataNum; i++)  {
             if( dl_usb.gNum < MAX_READ_SIZE ) {
                 gBuffer[dl_usb.gRead] = dl_usb.ibuf[i];
                 dl_usb.gRead ++;
                 dl_usb.gRead %= MAX_READ_SIZE;
                 dl_usb.gNum ++;
             } else {
                 printk("dsplog read buffer overflow\n");
                 break;
             }
        }
        spin_unlock_irqrestore(&dl_usb.lock,flag);

        dl_usb.dataNum = 0;
        if(dl_usb.gNum >= MAX_AP_READ) {
	    // have received data by urb
            wake_up_interruptible(&dl_wait);   
       	    if(dl_fasync)  {
               kill_fasync(&dl_fasync, SIGIO, POLL_IN);
            }
        }  
        /*  does not need ??? */
        set_GPIO_mode(GPIO_IN | 41);
	if( GPIO_is_high(41) )  {
		GPCR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
		mdelay(20);
		GPSR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
        	dl_dbg("wakeup BP");
	}
	
	// a2590c 6-24
	if( (UHCRHPS3 & 0x4) == 0x4 )  {
		UHCRHPS3 = 0x8;
		mdelay(40);
		dl_dbg("send RESUME signal");
	}

        dl_usb.dl_urb.dev = dl_usb.dl_dev;
        dl_usb.dl_urb.actual_length = 0;
        usb_submit_urb(&dl_usb.dl_urb);
}

/******************************************************************************
 * Function Name:  usb_dl_probe
 *
 * Input:           usbdev      :  the pointer for USB device structure
 *                  ifnum       :  interface number
 *                  id          :
 * Value Returned:  void *      :  NULL = error.
 *
 * Description: This function is used as the probe function of DSPLOG interface.
 *
 * Modification History:
 *  
 *****************************************************************************/

static void *usb_dl_probe(struct usb_device *usbdev, unsigned int ifnum,
			   const struct usb_device_id *id)
{	
	struct usb_interface_descriptor *dl_interface;
	struct usb_endpoint_descriptor *dl_endpoint;		
	int readsize;

	dl_dbg("Beginning usb_dl_probe\n");
	
	if( dl_usb.probe_flag ) {
		dl_dbg("dsplog has been probed");
		return NULL;
	}

	dl_dbg("usb_dl_probe: vendor id 0x%x, device id 0x%x", usbdev->descriptor.idVendor, usbdev->descriptor.idProduct);
	
	if ((usbdev->descriptor.idVendor != MOTO_DSPLOG_VID) || (usbdev->descriptor.idProduct != MOTO_DSPLOG_PID) ||
	    (ifnum != DSPLOG_IF_INDEX) ) {
		dl_dbg("ifnum error ifnum = %d,idvendor=%x, idProduct=%x ",ifnum,usbdev->descriptor.idVendor,usbdev->descriptor.idProduct);
 		return NULL;
	}

        // Continue detect some other description.
	if (usbdev->descriptor.bNumConfigurations != 1)  {
		dl_dbg("usb_dl_probe: configuration number detected.");
		return NULL;
	}

	if (usbdev->config[0].bNumInterfaces != 3)  	{
		dl_dbg("usb_dl_probe: interface number detected error");
		return NULL;
	}
	
	dl_interface = usbdev->config[0].interface[ifnum].altsetting;
	dl_endpoint =  dl_interface[0].endpoint;

	dl_dbg("usb_dl_probe: Number of Endpoints:%d", (int) dl_interface->bNumEndpoints);	

	if (dl_interface->bNumEndpoints != 1)  {
		dl_dbg("usb_dl_probe: Only one endpoints is supported.");
		return NULL;
	}

	dl_dbg("usb_dl_probe: endpoint[0] is:%x", (&dl_endpoint[0])->bEndpointAddress);

	if (!IS_EP_BULK_IN(dl_endpoint[0])) 	{			
		info("usb_dl_probe: Undetected IN endpoint");
		return NULL;	/* Shouldn't ever get here unless we have something weird */
	}

	readsize = (&dl_endpoint[0])->wMaxPacketSize;	
	dl_dbg("usb_dl_probe: readsize = %d", readsize);			

       // Ok, now initialize all the relevant values
	if (!(dl_usb.ibuf = (char *)kmalloc(readsize, GFP_KERNEL)))  	{
		err("usb_dl_probe: Not enough memory for the input buffer.");
		return NULL;
	}
	dl_usb.dl_ep      = (&dl_endpoint[0])->bEndpointAddress;
	dl_dbg("dl_usb.dl_ep = %d",dl_usb.dl_ep);
	dl_usb.probe_flag = 1;	
	dl_usb.dl_dev     = usbdev;	
	dl_usb.bufSize    = readsize;
	dl_usb.dataNum    = 0;
	dl_usb.isopen     = 0;
        dl_usb.gNum       = 0;
        dl_usb.gRead      = 0;
        dl_usb.gWrite     = 0;
	/*Build a read urb and send a IN token first time*/
	FILL_BULK_URB(&(dl_usb.dl_urb), usbdev, usb_rcvbulkpipe(usbdev, dl_usb.dl_ep),
		dl_usb.ibuf, readsize, usb_dl_callback, 0);

	// register device
#ifdef CONFIG_DEVFS_FS
	dl_usb.devfs = devfs_register(usb_devfs_handle, DL_DRV_NAME,
				    DEVFS_FL_DEFAULT, USB_MAJOR, DSPLOG_MINOR,
				    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP |
				    S_IWGRP | S_IROTH | S_IWOTH, &dsplog_fops, NULL);
				    
	if (dl_usb.devfs == NULL)  {
		dl_dbg("dsplog : device node registration failed");
		return NULL;
	}
#else
	misc_register (&usbdl_misc_device);
#endif
	
	return &dl_usb;
}


/******************************************************************************
 * Function Name:  usb_dl_disconnect
 *
 * Input:           usbdev      :  the pointer for USB device structure
 *                  ptr         :  DSPLOG usb structure
 *                  id          :
 * Value Returned:  void *      :  NULL = error.
 *
 * Description: This function is used as the disconnect for DSPLOG interface.
 *
 * Modification History:
 *  
 *****************************************************************************/
static void usb_dl_disconnect(struct usb_device *usbdev, void *ptr)
{
	int retval;
	struct dl_usb_data *dl = (struct dl_usb_data *) ptr;

	if( !dl_usb.probe_flag ) {
		dl_dbg("Call disconnect, but dsplog is not probed");
		return;
	}	

	retval = usb_unlink_urb (&dl->dl_urb);
	if (retval != -EINPROGRESS && retval != 0) 	{
		dl_dbg("disconnect: unlink urb error, %d", retval);
	}

        usb_driver_release_interface(&usb_dl_driver,
        &dl->dl_dev->actconfig->interface[DSPLOG_IF_INDEX]);
		
	dl_dbg("usb_dl_disconnect");
	dl_usb.probe_flag = 0;
	dl_usb.dataNum = 0;

        // wake up the blocked read
        wake_up_interruptible(&dl_wait);   
        if(dl_fasync)  {
                 kill_fasync(&dl_fasync, SIGIO, POLL_IN);
        }

        // freee input buffer
	kfree(dl_usb.ibuf);
#ifdef CONFIG_DEVFS_FS
	devfs_unregister(dl_usb.devfs);
#else
	misc_deregister (&usbdl_misc_device);
#endif
}

/******************************************************************************
 * Function Name: resubmit_timeout
 * 
 *****************************************************************************/
static void resubmit_timeout(unsigned long data)
{
	wake_up_interruptible(&resub_wait);
}

/******************************************************************************
 * Function Name:  usb_ldl_init
 *
 * Input:           void       :
 * Value Returned:  int        :  -1 = False;   0 = True
 *
 * Description: This function is used as the drive initialize function.
 *
 * Modification History:
 *  
 *****************************************************************************/
static int __init usb_ldl_init(void)
{
	int result;

	dl_dbg ("init usb_dl_init");
	/* register driver at the USB subsystem */
        init_waitqueue_head(&dl_wait);
        dl_fasync = NULL;
        
        // re-submit URB timer
        init_waitqueue_head(&resub_wait);
	init_timer(&resub_wait_timer);
	resub_wait_timer.function = resubmit_timeout;
	
	memset((void *)&dl_usb, 0, sizeof(struct dl_usb_data));
	result = usb_register(&usb_dl_driver);
	if (result < 0) {
		dl_dbg("usb dl driver could not be registered");
		return -1;
	}

	dl_dbg ("exit init usb_dl_init");
	return 0;
}

/******************************************************************************
 * Function Name:  usb_ldl_exit
 *
 * Input:           void       :
 * Value Returned:  void        :  -1 = False;   0 = True
 *
 * Description: This function is used as the drive exit function.
 *
 * Modification History:
 *  
 *****************************************************************************/
static void __exit usb_ldl_exit(void)
{
	dl_dbg ("usb_dl_exit");
	usb_deregister(&usb_dl_driver);
}

/* the module entry declaration of this driver */
module_init(usb_ldl_init);
module_exit(usb_ldl_exit);

