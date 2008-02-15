#if EMU_PIHF_FEATURE
/*
 *     linux/drivers/misc/ezx-emu.c --- EMU driver on ezx
 *
 *  Support for the Motorola Ezx A780 Development Platform.
 *  
 *  Author:     Cheng Xuefeng
 *  Created:    April 30, 2004
 *  Copyright:  Motorola Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation. 
 *
 */

#include <linux/init.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/apm_bios.h>

#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/semaphore.h>
#include <asm/mach-types.h>
#include "ssp_pcap.h"
#include "ezx-emu.h"

#define DEBUG 
#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

#ifdef EMU_INTERFACE_DEBUG
#define EMU_DBG(fmt,args...) printk("EMU:"fmt,##args);
#else
#define EMU_DBG(fmt,args...)
#endif

EXPORT_SYMBOL(emu_switch_to);

extern void emu_switch_to(T_EMU_SWITCH_TO to);

#define EMU_YES  (1)
#define EMU_NO   (0)

/* 1ms */
#define EMU_PIHF_DPLUS_INIT_INTERVAL (1)

static int emu_is_int  = EMU_NO; 
static int emu_is_wait = EMU_NO; 

typedef enum{
    EMU_USB = 0,
    EMU_SPD,
    EMU_PIHF,             /*EMU_PIHF is a subset of EMU_SPD*/
    EMU_EIHF
} T_EMU_CURRENT_DEVICE;

typedef enum{
    EMU_MODE_UART =0,
    EMU_MODE_USB,
    EMU_MODE_AUDIO_MONO,
    EMU_MODE_AUDIO_STEREO
} T_EMU_MODE;

static T_EMU_CURRENT_DEVICE emu_current_device = EMU_USB;
static T_EMU_MODE emu_current_mode = EMU_MODE_USB;

static DECLARE_WAIT_QUEUE_HEAD(emu_wait_queue); /* Used for blocking read */

static int emucount=0;


static void emu_handle_int(int irq,void *dev,struct pt_regs *regs);

/* This will be export to other modules. For other feature, developer should
 * add related T_EMU_SWITCH_TO type as requriement.
 */
void emu_switch_to(T_EMU_SWITCH_TO to)
{
    switch(to)
    {
        case EMU_SWITCH_TO_NO_DEV:
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_NO_DEV\n");
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);

            /* Disable the PCAP transceiver both for USB and RS232 */
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);

            /* Set the Bulverde GPIO */
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);
            break;
        case EMU_SWITCH_TO_UART:

            /* set the PCAP as UART mode */
            /*SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_SERIAL_PORT);*/
            if(SSP_PCAP_SUCCESS == SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN))
            {
                EMU_DBG("Before SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB clean\n");
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);
            }
            else
            {
                return;
            }

            /* This must be set. */
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232_DIR);
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_UART:set txd high\n");
            set_GPIO(GPIO39_FFTXD);

            /* set other USB related GPIO direcction to IN first */
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);

            /* Init UART related GPIO */
            set_GPIO_mode(GPIO39_FFTXD_MD);
            set_GPIO_mode(GPIO53_FFRXD_MD);
            CKEN |= CKEN6_FFUART;

            /* set the MUX1/2 to data mode */
            clr_GPIO(GPIO_EMU_MUX1);
            clr_GPIO(GPIO_EMU_MUX2);

            emu_current_mode = EMU_MODE_UART;
            break;
        case EMU_SWITCH_TO_USB:
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_USB\n");
            /* set the 6 USB related GPIOs as USB function*/
            set_GPIO_mode(GPIO34_TXENB_MD);
            set_GPIO_mode(GPIO35_XRXD_MD);
            set_GPIO_mode(GPIO36_VMOUT_MD);
            set_GPIO_mode(GPIO39_VPOUT_MD);
            set_GPIO_mode(GPIO40_VPIN_MD);
            set_GPIO_mode(GPIO53_VMIN_MD);
            UP2OCR = 0x02000000;

            /* set the PCAP as UART mode */
            /*SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_HIGH_USB_PORT);*/
            if(SSP_PCAP_SUCCESS == SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB))
            {
                if(SSP_PCAP_SUCCESS == SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_FSENB))
                {
                    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
                }
            }

            /* set the MUX1/2 to data mode */
            clr_GPIO(GPIO_EMU_MUX1);
            clr_GPIO(GPIO_EMU_MUX2);

            emu_current_mode = EMU_MODE_USB;
            break;
        case EMU_SWITCH_TO_AUDIO_MONO:
            /* This will not change the PCAP audio related 
             * register. Just change the MUX1/2. Switching
             * the audio path is controlled by other driver.
             */
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_AUDIO_MONO\n");

            clr_GPIO(GPIO39_VPOUT);
            mdelay(1);
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);

            /* set the MUX1/2 to mono mode */
            set_GPIO(GPIO_EMU_MUX1);
            clr_GPIO(GPIO_EMU_MUX2);

            emu_current_mode = EMU_MODE_AUDIO_MONO;
            break;
        case EMU_SWITCH_TO_AUDIO_STEREO:
            /* This will not change the PCAP audio related 
             * register. Just change the MUX1/2. Switching
             * the audio path is controlled by other driver.
             */
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_AUDIO_STEREO\n");
            clr_GPIO(GPIO39_VPOUT);
            set_GPIO_mode(GPIO34_TXENB | GPIO_IN);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);

            /* set the MUX1/2 to stereo mode */
            set_GPIO(GPIO_EMU_MUX1);
            set_GPIO(GPIO_EMU_MUX2);
            emu_current_mode = EMU_MODE_AUDIO_STEREO;
            break;
        default:
            break;
    }
    return;
}


static int emu_ioctl(struct inode *inode, struct  file *file, uint cmd, ulong arg)
{
    int  ret;
    long val;

    switch(cmd){

        /* Switch the MUX1/2 and set the GPIOs */
        case EMU_SW_TO_UART:
            EMU_DBG("emu_ioctl:EMU_SW_TO_UART:before change\n");
            emu_switch_to(EMU_SWITCH_TO_UART);
            break;

        case EMU_SW_TO_USB:
            EMU_DBG("emu_ioctl:EMU_SW_TO_USB:before change\n");
            emu_switch_to(EMU_SWITCH_TO_USB);
            break;

        case EMU_SW_TO_AUDIO_MONO:
            EMU_DBG("emu_ioctl:EMU_SW_TO_AUDIO_MONO:before change\n");
            emu_switch_to(EMU_SWITCH_TO_AUDIO_MONO);
            break;

        case EMU_SW_TO_AUDIO_STEREO:
            EMU_DBG("emu_ioctl:EMU_SW_TO_AUDIO_STEREO: not support...\n");
            emu_switch_to(EMU_SWITCH_TO_AUDIO_STEREO);
            break;

        case EMU_SMART_SPD_INIT:

            EMU_DBG("emu_ioctl:EMU_SPD_INIT\n");
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PD);
            mdelay(EMU_PIHF_DPLUS_INIT_INTERVAL);
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PD);

            /* init to UART mode */
            emu_switch_to(EMU_SWITCH_TO_UART);

            EMU_DBG("emu_ioctl:set GPIO 86\n");
            /* For SNP_INT_CTL, the inactive state is low.
            * That means the ID line is high.
            */
            set_GPIO_mode(GPIO_SNP_INT_CTL|GPIO_OUT);
            clr_GPIO(GPIO_SNP_INT_CTL);

            EMU_DBG("emu_ioctl:before request_irq()\n");

            udelay(10);

            set_GPIO_IRQ_edge(GPIO_SNP_INT_IN,GPIO_FALLING_EDGE|GPIO_RISING_EDGE);

            if (request_irq(IRQ_GPIO(GPIO_SNP_INT_IN),emu_handle_int,
                    SA_INTERRUPT|SA_SAMPLE_RANDOM,
                    "EMU SNP interrupt",(void*)1)
               )
            {
                printk(KERN_WARNING"can't get EMU snp_int irq\n");
                return -EIO;
            }

            emu_current_device = EMU_SPD;
            break;

        /* EMU PIHF */
        case EMU_PIHF_INIT:
            EMU_DBG("%s,%s:EMU_PIHF_INIT",__FILE__,__FUNCTION__);

            /* init to UART mode */
            emu_switch_to(EMU_SWITCH_TO_UART);

            emu_current_device = EMU_PIHF;
            break;
        case EMU_PIHF_SNP_INT_CTL:
            EMU_DBG("emu_ioctl:got the SNP_INT_CTL...\n");
            /* For SNP_INT_CTL, the inactive state is low.
             * That means the ID line is high.
             */
            ret = get_user(val,(long *)arg);
            if (ret)
                return ret;
            disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            set_GPIO(GPIO_SNP_INT_CTL);
            mdelay(val);
            clr_GPIO(GPIO_SNP_INT_CTL);
            enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            break;

        case EMU_PIHF_ID_LOW:
            EMU_DBG("emu_ioctl:EMU_PIHF_ID_LOW\n");
            disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            set_GPIO(GPIO_SNP_INT_CTL);
            enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            break;

        case EMU_PIHF_ID_HIGH:
            EMU_DBG("emu_ioctl:EMU_PIHF_ID_HIGH\n");
            disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            clr_GPIO(GPIO_SNP_INT_CTL);
            enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            break;

        /* EMU EIHF */
        case EMU_EIHF_INIT:
            emu_current_device = EMU_EIHF;
            break;

        case EMU_EIHF_MUTE:
            clr_GPIO(GPIO_SNP_INT_CTL); 
            break;

        case EMU_EIHF_UNMUTE:
            set_GPIO(GPIO_SNP_INT_CTL);
            break;

        default:
            return  -EINVAL;
    }
    return 0;
}

static unsigned int emu_poll(struct file *file, poll_table *wait)
{
    if (emu_is_int == EMU_NO)
    {
        EMU_DBG("%s,%s:emu_is_int is NO\n",__FILE__,__FUNCTION__);
        emu_is_wait = EMU_YES;
        poll_wait(file, &emu_wait_queue, wait);
    }
    else
    {
        EMU_DBG("%s,%s:emu_is_int is YES\n",__FILE__,__FUNCTION__);
        emu_is_int = EMU_NO;
    }
    return POLLIN | POLLRDNORM;
}
/*
 * This function is called when a user space program attempts to read
 * /dev/emu. It puts the device to sleep on the wait queue until
 * irq handler writes some data to the buffer and flushes
 * the queue, at which point it writes the data out to the device and
 * returns the number of characters it has written. This function is
 * reentrant, so that many processes can be attempting to read from the
 * device at any one time.
 */

static int emu_read (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
    DECLARE_WAITQUEUE(wait, current);
    int retval = 0;

    EMU_DBG("%s,%s:enter ...\n",__FILE__,__FUNCTION__);
    if (EMU_NO == emu_is_int) 
    {
        EMU_DBG("%s,%s:emu_is_int is NO\n",__FILE__,__FUNCTION__);
        add_wait_queue(&emu_wait_queue, &wait);
        current->state = TASK_INTERRUPTIBLE;

        while (EMU_NO == emu_is_int) 
        {
            if (file->f_flags & O_NONBLOCK) 
            {
                retval = -EAGAIN;
                break;
            }
            if (signal_pending(current)) 
            {
                   retval = -ERESTARTSYS;
                break;
            }
            emu_is_wait = EMU_YES;
            EMU_DBG("%s,%s:before schedule()\n",__FILE__,__FUNCTION__);
            schedule();
        }
        current->state = TASK_RUNNING;
        EMU_DBG("%s,%s:after wake up\n",__FILE__,__FUNCTION__);
        remove_wait_queue(&emu_wait_queue, &wait);
    }

    return retval;    
}

#ifdef CONFIG_PM
static int emu_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
    EMU_DBG("emu_pm_callback\n");
    switch(req)
    {
        case PM_SUSPEND:    
            EMU_DBG("emu_pm_callback:PM_SUSPEND\n");
            /* Disable the PCAP transceiver both for USB and RS232 */
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);

            /* Set the Bulverde GPIO */
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            PGSR(GPIO34_TXENB)|=GPIO_bit(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);
            break;

        case PM_RESUME:

            if ( emucount != 0)
            {
                switch ( emu_current_device)
                {
                    case EMU_PIHF:

                        EMU_DBG("emu_pm_callback:EMU_PIHF\n");
                        disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
                        clr_GPIO(GPIO_SNP_INT_CTL);
                        enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));

                        switch(emu_current_mode)
                        {
                            case EMU_MODE_UART:
                                emu_switch_to(EMU_SWITCH_TO_UART);
                                break;
                            case EMU_MODE_USB:
                                emu_switch_to(EMU_SWITCH_TO_USB);
                                break;
                            case EMU_MODE_AUDIO_MONO:
                                emu_switch_to(EMU_SWITCH_TO_AUDIO_MONO);
                                break;
                            case EMU_MODE_AUDIO_STEREO:
                                emu_switch_to(EMU_SWITCH_TO_AUDIO_STEREO);
                                break;
                            default:
                                break;
                        }
                        break;
                    case EMU_EIHF:
                        /* Should not come here since the EIHF must have the charger. */
                        break;
                    case EMU_USB:
                        emu_switch_to(EMU_SWITCH_TO_USB);
                        break;
                    default:
                        break;
                 }
            }
            break;
    }
    return 0;
}
#endif

static int emu_release(struct inode * inode, struct file * file)
{
    int i;
    
    if ( emucount > 0)
        emucount --;

    if(emucount)
    {
        return -EBUSY;
    }

    if ( EMU_PIHF == emu_current_device)
        free_irq(IRQ_GPIO(GPIO_SNP_INT_IN),NULL);

    EMU_DBG("emu_release:freed irq...%d\n",IRQ_GPIO(87));

#ifdef CONFIG_PM
    pm_unregister(pm_dev);
#endif
#ifdef DEBUG
    EMU_DBG("emu_release:emu device released\n");
#endif
        
    return 0;
}

static void emu_handle_int(int irq,void *dev,struct pt_regs *regs)
{
    EMU_DBG("%s,%s:got the interrupt...\n",__FILE__,__FUNCTION__);
    if ( EMU_YES == emu_is_wait )
    {
        EMU_DBG("%s,%s:emu_is_wait is YES...\n",__FILE__,__FUNCTION__);
        wake_up_interruptible(&emu_wait_queue);
        emu_is_wait = EMU_NO;
    }
    else
    {
        EMU_DBG("%s,%s:emu_is_wait is NO...\n",__FILE__,__FUNCTION__);
        emu_is_int = EMU_YES;
    }
    EMU_DBG("emu_handle_int:after wake up...\n");
}

static int emu_open(struct inode * inode, struct file * file)
{
    int i, data;
    int ret;

    /* emucount could only be 1 or 0 */
    if(emucount)
    {    
        return -EBUSY;     
    }
    emucount ++;

#ifdef CONFIG_PM
    pm_dev = pm_register(PM_SYS_DEV, 0, emu_pm_callback);
#endif
    EMU_DBG("emu_open:emu device opened\n");
    return 0;
}

/* add emu_ioctl to check flip status */
static struct file_operations emu_fops = {
    owner:       THIS_MODULE,
    open:        emu_open,
    release:     emu_release,
    read:        emu_read,
    poll:        emu_poll,
    ioctl:       emu_ioctl,
};

/* 
 * This structure is the misc device structure, which specifies the minor
 * device number (165 in this case), the name of the device (for /proc/misc),
 * and the address of the above file operations structure.
 */

static struct miscdevice emu_misc_device = {
    EMU_MINOR,
    "emu",
    &emu_fops,
};

/*
 * This function is called to initialise the driver, either from misc.c at
 * bootup if the driver is compiled into the kernel, or from init_module
 * below at module insert time. It attempts to register the device node
 * and the IRQ and fails with a warning message if either fails, though
 * neither ever should because the device number and IRQ are unique to
 * this driver.
 */

static int __init emu_init(void)
{
    set_GPIO_mode(GPIO_SNP_INT_IN | GPIO_IN);
    set_GPIO_mode(GPIO_EMU_MUX1 | GPIO_OUT);
    set_GPIO_mode(GPIO_EMU_MUX2 | GPIO_OUT);

    if (misc_register (&emu_misc_device)) {
        EMU_DBG (KERN_WARNING "emu: Couldn't register device 10, "
                "%d.\n", EMU_MINOR);
        return -EBUSY;
    }
}

static void __exit emu_exit (void) 
{
    misc_deregister(&emu_misc_device);
}


module_init(emu_init);
module_exit(emu_exit);
#else /* EMU_PIHF_FEATURE */

/*
 *     linux/drivers/misc/ezx-emu.c --- EMU driver on ezx
 *
 *  Support for the Motorola Ezx A780 Development Platform.
 *  
 *  Author:     Cheng Xuefeng
 *  Created:    April 30, 2004
 *  Copyright:  Motorola Inc.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation. 
 *
 */

#include <linux/init.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/apm_bios.h>

#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/semaphore.h>
#include <asm/mach-types.h>
#include "ssp_pcap.h"
#include "ezx-emu.h"

#define DEBUG 
#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
#endif

#ifdef EMU_INTERFACE_DEBUG
#define EMU_DBG(fmt,args...) printk("EMU:"fmt,##args);
#else
#define EMU_DBG(fmt,args...)
#endif

EXPORT_SYMBOL(emu_switch_to);

extern void emu_switch_to(T_EMU_SWITCH_TO to);

#define EMU_YES  (0)
#define EMU_NO   (1)

/* 1ms */
#define EMU_PIHF_DPLUS_INIT_INTERVAL (1)

static int emu_is_int  = EMU_NO; 
static int emu_is_wait = EMU_NO; 

typedef enum{
    EMU_USB = 0,
    EMU_PIHF,
    EMU_EIHF
} T_EMU_CURRENT_DEVICE;

typedef enum{
    EMU_MODE_UART =0,
    EMU_MODE_USB,
    EMU_MODE_AUDIO_MONO,
    EMU_MODE_AUDIO_STEREO
} T_EMU_MODE;

static T_EMU_CURRENT_DEVICE emu_current_device = EMU_USB;
static T_EMU_MODE emu_current_mode = EMU_MODE_USB;

static DECLARE_WAIT_QUEUE_HEAD(emu_wait_queue); /* Used for blocking read */

static int emucount=0;


static void emu_handle_int(int irq,void *dev,struct pt_regs *regs);

/* This will be export to other modules. For other feature, developer should
 * add related T_EMU_SWITCH_TO type as requriement.
 */
void emu_switch_to(T_EMU_SWITCH_TO to)
{
    switch(to)
    {
        case EMU_SWITCH_TO_NO_DEV:
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_NO_DEV\n");
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);

            /* Disable the PCAP transceiver both for USB and RS232 */
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);

            /* Set the Bulverde GPIO */
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);
            break;
        case EMU_SWITCH_TO_UART:
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_UART\n");
            /* set other USB related GPIO direcction to IN first */
            set_GPIO(GPIO39_VPOUT);

            set_GPIO_mode(GPIO34_TXENB | GPIO_IN);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);

            /* Init UART related GPIO */
            set_GPIO_mode(GPIO39_FFTXD_MD);
            set_GPIO_mode(GPIO53_FFRXD_MD);
            CKEN |= CKEN6_FFUART;
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_UART:set txd high\n");

            /* set the PCAP as UART mode */
            /*SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_SERIAL_PORT);*/
            if(SSP_PCAP_SUCCESS == SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN))
            {
                SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);
            }
            else
            {
                return;
            }

            /* set the MUX1/2 to data mode */
            clr_GPIO(GPIO_EMU_MUX1);
            clr_GPIO(GPIO_EMU_MUX2);

            emu_current_mode = EMU_MODE_UART;
            break;
        case EMU_SWITCH_TO_USB:
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_USB\n");
            /* set the 6 USB related GPIOs as USB function*/
            set_GPIO_mode(GPIO34_TXENB_MD);
            set_GPIO_mode(GPIO35_XRXD_MD);
            set_GPIO_mode(GPIO36_VMOUT_MD);
            set_GPIO_mode(GPIO39_VPOUT_MD);
            set_GPIO_mode(GPIO40_VPIN_MD);
            set_GPIO_mode(GPIO53_VMIN_MD);
            UP2OCR = 0x02000000;

            /* set the PCAP as UART mode */
            /*SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_HIGH_USB_PORT);*/
            if(SSP_PCAP_SUCCESS == SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB))
            {
                if(SSP_PCAP_SUCCESS == SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_FSENB))
                {
                    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
                }
            }

            /* set the MUX1/2 to data mode */
            clr_GPIO(GPIO_EMU_MUX1);
            clr_GPIO(GPIO_EMU_MUX2);

            emu_current_mode = EMU_MODE_USB;
            break;
        case EMU_SWITCH_TO_AUDIO_MONO:
            /* This will not change the PCAP audio related 
             * register. Just change the MUX1/2. Switching
             * the audio path is controlled by other driver.
             */
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_AUDIO_MONO\n");

            clr_GPIO(GPIO39_VPOUT);
            mdelay(1);
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);

            /* set the MUX1/2 to mono mode */
            set_GPIO(GPIO_EMU_MUX1);
            clr_GPIO(GPIO_EMU_MUX2);

            emu_current_mode = EMU_MODE_AUDIO_MONO;
            break;
        case EMU_SWITCH_TO_AUDIO_STEREO:
            /* This will not change the PCAP audio related 
             * register. Just change the MUX1/2. Switching
             * the audio path is controlled by other driver.
             */
            EMU_DBG("emu_switch_to:EMU_SWITCH_TO_AUDIO_STEREO\n");
            clr_GPIO(GPIO39_VPOUT);
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);

            /* set the MUX1/2 to stereo mode */
            set_GPIO(GPIO_EMU_MUX1);
            set_GPIO(GPIO_EMU_MUX2);
            emu_current_mode = EMU_MODE_AUDIO_STEREO;
            break;
        default:
            break;
    }
    return;
}


static int emu_ioctl(struct inode *inode, struct  file *file, uint cmd, ulong arg)
{
    int  ret;
    long val;

    switch(cmd){

        /* Switch the MUX1/2 and set the GPIOs */
        case EMU_SW_TO_UART:
            EMU_DBG("emu_ioctl:EMU_SW_TO_UART:before change\n");
            emu_switch_to(EMU_SWITCH_TO_UART);
            break;

        case EMU_SW_TO_USB:
            EMU_DBG("emu_ioctl:EMU_SW_TO_USB:before change\n");
            emu_switch_to(EMU_SWITCH_TO_USB);
            break;

        case EMU_SW_TO_AUDIO_MONO:
            EMU_DBG("emu_ioctl:EMU_SW_TO_AUDIO_MONO:before change\n");
            emu_switch_to(EMU_SWITCH_TO_AUDIO_MONO);
            break;

        case EMU_SW_TO_AUDIO_STEREO:
            EMU_DBG("emu_ioctl:EMU_SW_TO_AUDIO_STEREO: not support...\n");
            emu_switch_to(EMU_SWITCH_TO_AUDIO_STEREO);
            break;

        /* EMU PIHF */
        case EMU_PIHF_INIT:
            EMU_DBG("%s,%s:EMU_PIHF_INIT",__FILE__,__FUNCTION__);

            /* init to UART mode */
            emu_switch_to(EMU_SWITCH_TO_UART);

            EMU_DBG("emu_ioctl:set GPIO 86\n");
            /* For SNP_INT_CTL, the inactive state is low.
            * That means the ID line is high.
            */
            set_GPIO_mode(GPIO_SNP_INT_CTL|GPIO_OUT);
            clr_GPIO(GPIO_SNP_INT_CTL);

            EMU_DBG("emu_ioctl:before request_irq()\n");

            udelay(10);

            set_GPIO_IRQ_edge(GPIO_SNP_INT_IN,GPIO_FALLING_EDGE|GPIO_RISING_EDGE);

#if 0
            if(GPLR(GPIO_SNP_INT_IN) & GPIO_bit(GPIO_SNP_INT_IN))
            {
                EMU_DBG("emu_ioctl:87 is high\n");
            }
            else
            {
                EMU_DBG("emu_ioctl:87 is low\n");
            }
#endif

            if (request_irq(IRQ_GPIO(GPIO_SNP_INT_IN),emu_handle_int,
                    SA_INTERRUPT|SA_SAMPLE_RANDOM,
                    "EMU SNP interrupt",(void*)1)
               )
            {
                printk(KERN_WARNING"can't get EMU snp_int irq\n");
                return -EIO;
            }

            emu_current_device = EMU_PIHF;
            break;
        case EMU_PIHF_SNP_INT_CTL:
            EMU_DBG("emu_ioctl:got the SNP_INT_CTL...\n");
            /* For SNP_INT_CTL, the inactive state is low.
             * That means the ID line is high.
             */
            ret = get_user(val,(long *)arg);
            if (ret)
                return ret;
            disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            set_GPIO(GPIO_SNP_INT_CTL);
            mdelay(val);
            clr_GPIO(GPIO_SNP_INT_CTL);
            enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            break;

        case EMU_PIHF_ID_LOW:
            EMU_DBG("emu_ioctl:EMU_PIHF_ID_LOW\n");
            disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            set_GPIO(GPIO_SNP_INT_CTL);
            enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            break;

        case EMU_PIHF_ID_HIGH:
            EMU_DBG("emu_ioctl:EMU_PIHF_ID_HIGH\n");
            disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            clr_GPIO(GPIO_SNP_INT_CTL);
            enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
            break;

        case EMU_PIHF_INIT_DPLUS_CTL:
            EMU_DBG("emu_ioctl:EMU_PIHF_INIT_DPLUS_CTL\n");
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PD);
            mdelay(EMU_PIHF_DPLUS_INIT_INTERVAL);
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PD);
            break;

        /* EMU EIHF */
        case EMU_EIHF_INIT:
            emu_current_device = EMU_EIHF;
            break;

        case EMU_EIHF_MUTE:
            clr_GPIO(GPIO_SNP_INT_CTL); 
            break;

        case EMU_EIHF_UNMUTE:
            set_GPIO(GPIO_SNP_INT_CTL);
            break;

        default:
            return  -EINVAL;
    }
    return 0;
}

static unsigned int emu_poll(struct file *file, poll_table *wait)
{
    if (emu_is_int == EMU_NO)
    {
        emu_is_wait = EMU_YES;
        poll_wait(file, &emu_wait_queue, wait);
    }
    else
    {
        emu_is_int = EMU_NO;
    }
    return POLLIN | POLLRDNORM;
}
/*
 * This function is called when a user space program attempts to read
 * /dev/emu. It puts the device to sleep on the wait queue until
 * irq handler writes some data to the buffer and flushes
 * the queue, at which point it writes the data out to the device and
 * returns the number of characters it has written. This function is
 * reentrant, so that many processes can be attempting to read from the
 * device at any one time.
 */

static int emu_read (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
    DECLARE_WAITQUEUE(wait, current);
    int retval = 0;

    if (EMU_NO == emu_is_int) 
    {
        add_wait_queue(&emu_wait_queue, &wait);
        current->state = TASK_INTERRUPTIBLE;

        while (EMU_NO == emu_is_int) 
        {
            if (file->f_flags & O_NONBLOCK) 
            {
                retval = -EAGAIN;
                break;
            }
            if (signal_pending(current)) 
            {
                   retval = -ERESTARTSYS;
                break;
            }
            emu_is_wait = EMU_YES;
            schedule();
        }
        current->state = TASK_RUNNING;
        remove_wait_queue(&emu_wait_queue, &wait);
    }

    return retval;    
}

#ifdef CONFIG_PM
static int emu_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
    EMU_DBG("emu_pm_callback\n");
    switch(req)
    {
        case PM_SUSPEND:    
            EMU_DBG("emu_pm_callback:PM_SUSPEND\n");
            /* Disable the PCAP transceiver both for USB and RS232 */
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);

            /* Set the Bulverde GPIO */
            set_GPIO_mode(GPIO34_TXENB | GPIO_OUT);
            set_GPIO(GPIO34_TXENB);
            PGSR(GPIO34_TXENB)|=GPIO_bit(GPIO34_TXENB);
            set_GPIO_mode(GPIO35_XRXD  | GPIO_IN);
            set_GPIO_mode(GPIO36_VMOUT | GPIO_IN);
            set_GPIO_mode(GPIO39_VPOUT | GPIO_IN);
            set_GPIO_mode(GPIO40_VPIN  | GPIO_IN);
            set_GPIO_mode(GPIO53_VMIN  | GPIO_IN);
            break;

        case PM_RESUME:

            if ( emucount != 0)
            {
                switch ( emu_current_device)
                {
                    case EMU_PIHF:

                        EMU_DBG("emu_pm_callback:EMU_PIHF\n");
                        disable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));
                        clr_GPIO(GPIO_SNP_INT_CTL);
                        enable_irq(IRQ_GPIO(GPIO_SNP_INT_IN));

                        switch(emu_current_mode)
                        {
                            case EMU_MODE_UART:
                                emu_switch_to(EMU_SWITCH_TO_UART);
                                break;
                            case EMU_MODE_USB:
                                emu_switch_to(EMU_SWITCH_TO_USB);
                                break;
                            case EMU_MODE_AUDIO_MONO:
                                emu_switch_to(EMU_SWITCH_TO_AUDIO_MONO);
                                break;
                            case EMU_MODE_AUDIO_STEREO:
                                emu_switch_to(EMU_SWITCH_TO_AUDIO_STEREO);
                                break;
                            default:
                                break;
                        }
                        break;
                    case EMU_EIHF:
                        /* Should not come here since the EIHF must have the charger. */
                        break;
                    case EMU_USB:
                        emu_switch_to(EMU_SWITCH_TO_USB);
                        break;
                    default:
                        break;
                 }
            }
            break;
    }
    return 0;
}
#endif

static int emu_release(struct inode * inode, struct file * file)
{
    int i;
    
    if ( emucount > 0)
        emucount --;

    if(emucount)
    {
        return -EBUSY;
    }

    if ( EMU_PIHF == emu_current_device)
        free_irq(IRQ_GPIO(GPIO_SNP_INT_IN),NULL);

    EMU_DBG("emu_release:freed irq...%d\n",IRQ_GPIO(87));

#ifdef CONFIG_PM
    pm_unregister(pm_dev);
#endif
#ifdef DEBUG
    EMU_DBG("emu_release:emu device released\n");
#endif
        
    return 0;
}

static void emu_handle_int(int irq,void *dev,struct pt_regs *regs)
{
    EMU_DBG("emu_handle_int:got the interrupt...\n");
    if ( EMU_YES == emu_is_wait )
    {
        wake_up_interruptible(&emu_wait_queue);
        emu_is_wait = EMU_NO;
    }
    else
    {
        emu_is_int = EMU_YES;
    }
    EMU_DBG("emu_handle_int:after wake up...\n");
}

static int emu_open(struct inode * inode, struct file * file)
{
    int i, data;
    int ret;

    /* emucount could only be 1 or 0 */
    if(emucount)
    {    
        return -EBUSY;     
    }
    emucount ++;

#ifdef CONFIG_PM
    pm_dev = pm_register(PM_SYS_DEV, 0, emu_pm_callback);
#endif
    EMU_DBG("emu_open:emu device opened\n");
    return 0;
}

/* add emu_ioctl to check flip status */
static struct file_operations emu_fops = {
    owner:       THIS_MODULE,
    open:        emu_open,
    release:     emu_release,
    read:        emu_read,
    poll:        emu_poll,
    ioctl:       emu_ioctl,
};

/* 
 * This structure is the misc device structure, which specifies the minor
 * device number (165 in this case), the name of the device (for /proc/misc),
 * and the address of the above file operations structure.
 */

static struct miscdevice emu_misc_device = {
    EMU_MINOR,
    "emu",
    &emu_fops,
};

/*
 * This function is called to initialise the driver, either from misc.c at
 * bootup if the driver is compiled into the kernel, or from init_module
 * below at module insert time. It attempts to register the device node
 * and the IRQ and fails with a warning message if either fails, though
 * neither ever should because the device number and IRQ are unique to
 * this driver.
 */

static int __init emu_init(void)
{
    set_GPIO_mode(GPIO_SNP_INT_IN | GPIO_IN);

    if (misc_register (&emu_misc_device)) {
        EMU_DBG (KERN_WARNING "emu: Couldn't register device 10, "
                "%d.\n", EMU_MINOR);
        return -EBUSY;
    }
}

static void __exit emu_exit (void) 
{
    misc_deregister(&emu_misc_device);
}


module_init(emu_init);
module_exit(emu_exit);
#endif /* EMU_PIHF_FEATURE */
