/*
 * /vobs/ezx_linux/code/linux/linux-2.4.17/drivers/power_ic/core/sdhc_user.c
 *
 * Copyright (C) 2005-2006 - Motorola
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Motorola 2005-Dec-06 - File create
 *
 */

/*!
 * @file sdhc_user.c
 *
 * @ingroup sdhc
 *
 * @brief This file contains the ioctls necessary to use the SDHC from user space.
 *
 *    Provides an interface to control the SDHC from user space.  The following
 * ioctls are provided:
 *
 *    -# #MOTO_SDHC_IOCTL_GET_SDHC_PARAMS Get the SDHC parameters for the file handle.
 *    -# #MOTO_SDHC_IOCTL_SET_SDHC_PARAMS Set the SDHC parameters for the file handle.
 *    -# #MOTO_SDHC_IOCTL_CMD Send a command for the file handle.
 *    -# #MOTO_SDHC_IOCTL_DATA Send or receive data for the file handle.
 *    -# #MOTO_SDHC_IOCTL_SEQ Indicate the beginning or ending of a command sequence.
 */

#include <linux/devfs_fs_kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sdhc_user.h>
#include <linux/usr_blk_dev.h>
#include <asm/page.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <stdbool.h>
#include "sdhc_main.h"
#include "os_independent.h"

/******************************************************************************
* Constants
******************************************************************************/

/*!
 * @brief The size of the response buffer used to hold the response moving
 *        between kernel and user space
 */
#define RESPONSE_BUFFER_SIZE 16

/*!
 * @brief Initial size of the scratch buffer used to hold data moving between
 *        kernel and user space
 *
 * The initial size of the kernel scratch buffer which user space data is
 * copied into.  This buffer will dynamically grow to bigger sizes if
 * necessary.  The buffer is allocated when the file is opened, so a balance
 * must be acheived between the memory used and the number of times the buffer
 * will be re-allocated.
 */
#define SCRATCH_BUFFER_SIZE 512

/*! @brief Maximum number of times that /dev/sdhc_user can be opened concurrently. */
#define MAX_OPENS 1

/******************************************************************************
* Local Variables
******************************************************************************/

typedef struct
{
    /*! @brief flag indicating if slot is in use. */
    int open;

    /*! @brief Transfer parameters passed to SDHC driver. */
    SDHC_DATA_PARAMETER_T params;
    
    /*! @brief The number of bytes allocated for the data scratch buffer. */
    unsigned int data_sz;

    /*!
     * @brief Scratch buffer used when data is in user-space.
     */ 
    void *data_buf;

    /*!
     * @brief Scratch buffer used when response buffer is in user-space.
     */ 
    void *resp_buf;
} SDHC_FDS_T;

/*! @brief Data for the open file descriptors. */
static SDHC_FDS_T open_fds[MAX_OPENS];

#ifndef DOXYGEN_SHOULD_SKIP_THIS 
/*! @brief Mutex to control changes to the open flags in the open_fds array. */
DECLARE_MUTEX(global_mutex);
#endif

/******************************************************************************
* Local Functions
******************************************************************************/

/*!
 * @brief the open() handler for the SDHC device node
 *
 * @param inode inode pointer
 * @param file  file pointer
 *
 * @return 0 if successfully opened.<BR>
 *         -EBUSY if all slots are in use.
 */

static int sdhc_open (struct inode *inode, struct file *file)
{
    unsigned int i;
    SDHC_DATA_PARAMETER_T *params_p;

    /* Hold the semaphore to ensure our exclusive access to the open_fds array */
    if (down_interruptible(&global_mutex))
    {
        return -ERESTARTSYS;
    }

    /* Find the first open slot in the array */
    for (i = 0; i < MAX_OPENS; i++)
    {
        if (open_fds[i].open == 0)
        {
            /* Claim the slot for this file descriptor */
            open_fds[i].open = 1;

            break;
        }
    }

    /* We are done, so release the semaphore */
    up(&global_mutex);

    /* If we could not find a free slot, return busy */
    if (i == MAX_OPENS)
    {
        return -EBUSY;
    }

    /* Save "i" in the file to remember which slot we are using */
    file->private_data = (void *)i;

    /* Allocate memory for response buffer */
    open_fds[i].resp_buf = kmalloc(RESPONSE_BUFFER_SIZE, GFP_KERNEL);

    /* Allocate memory for scratch buffer */
    open_fds[i].data_sz = SCRATCH_BUFFER_SIZE;
    open_fds[i].data_buf = kmalloc(SCRATCH_BUFFER_SIZE, GFP_KERNEL|__GFP_DMA);

    /* Start out with known scratch data. */
    memset(open_fds[i].resp_buf, 0x00, RESPONSE_BUFFER_SIZE);
    memset(open_fds[i].data_buf, 0x00, SCRATCH_BUFFER_SIZE);
    
    /* Start out with an invalid SDHC module for error checking. */
    params_p = &open_fds[i].params;
    params_p->module = SDHC_MODULE__END;
    
    /* Set the default block length. */
    params_p->blk_len = 512;

    return 0;
}

/*!
 * @brief the close() handler for the SDHC device node
 *
 * This function implements the close() system call on the SDHC device node.
 * This function releases the allocated slot in the open_fds array.
 *
 * @param inode inode pointer
 * @param file  file pointer
 *
 * @return 0
 */
 
static int sdhc_close (struct inode *inode, struct file *file)
{
    int i = (int)file->private_data;

    /* Free the response scratch memory buffer. */
    kfree(open_fds[i].resp_buf);
    open_fds[i].resp_buf = NULL;

    /* Free the data scratch memory buffer. */
    open_fds[i].data_sz = 0;
    kfree(open_fds[i].data_buf);
    open_fds[i].data_buf = NULL;
    
    /* Hold the semaphore to ensure our exclusive access to the open_fds array */
    if (down_interruptible(&global_mutex))
    {
        return -EINTR;
    }

    /* Release our slot in the array */
    open_fds[i].open = 0;

    /* We are done, so release the semaphore */
    up(&global_mutex);

    return 0;
}

/*!
 * @brief the ioctl() handler for the SDHC device node
 *
 * This function implements the ioctl() system call for the SDHC device node.
 *
 * @param inode inode pointer
 * @param file  file pointer
 * @param cmd   the ioctl() command
 * @param arg   the ioctl() argument
 *
 * @return 0 if successful
 */

static int sdhc_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    static const SDHC_MODULE_T usr_ms_to_kernel_ms[MOTO_SDHC_MS__END] =
    {
        SDHC_MODULE_1          /* MOTO_SDHC_MS_TFLASH */
    };
    static const MOTO_SDHC_MS_T kernel_ms_to_usr_ms[SDHC_MODULE__END+1] =
    {
        MOTO_SDHC_MS_TFLASH,   /* SDHC_MODULE_1 */
        MOTO_SDHC_MS__END,     /* SDHC_MODULE_2 */
        MOTO_SDHC_MS__END      /* SDHC_MODULE__END */
    }; 
    int retval = 0;
    size_t resp_size;
    MOTO_SDHC_COMMAND_T cmddata;
    MOTO_SDHC_TRANSCEIVE_T xcvdata;
    MOTO_SDHC_PARAMETER_T sdhc_params;
    SDHC_IODATA_T io_data;
    SDHC_FDS_T *fds_p = &open_fds[(int)file->private_data];
    SDHC_DATA_PARAMETER_T *params_p = &fds_p->params;

    switch (cmd)
    {
        case MOTO_SDHC_IOCTL_GET_SDHC_PARAMS:
            /* Copy the needed parameters from the kernel SDHC parameters to the
               user space parameters. */
            sdhc_params.ms = kernel_ms_to_usr_ms[params_p->module];
            sdhc_params.four_bit_bus = params_p->four_bit_bus;
            sdhc_params.clk_prefix = params_p->clk_prefix;
            sdhc_params.blk_len = params_p->blk_len;
            sdhc_params.speed = params_p->speed;
                
            /* Copy the settings to user space */
            retval = copy_to_user (
                (void *)arg,
                (void *)&sdhc_params,
                sizeof(sdhc_params));

            /* If the copy failed, return an error */
            if (retval != 0)
            {
                return -EFAULT; 
            }
            break;

        case MOTO_SDHC_IOCTL_SET_SDHC_PARAMS:
            /* Copy the new settings from user space */
            retval = copy_from_user (
                (void *)&sdhc_params,
                (void *)arg,
                sizeof(sdhc_params));

            /* If the copy failed, return an error */
            if (retval != 0)
            {
                return -EFAULT;
            }

            if (sdhc_params.ms >= MOTO_SDHC_MS__END)
            {
                return -EINVAL;
            }
            
            /* Copy the user parameters to the kernel SDHC parameters. */
            params_p->module = usr_ms_to_kernel_ms[sdhc_params.ms];
            params_p->four_bit_bus = sdhc_params.four_bit_bus;
            params_p->clk_prefix = sdhc_params.clk_prefix;
            params_p->blk_len = sdhc_params.blk_len;
            params_p->speed = sdhc_params.speed;
            break;

        case MOTO_SDHC_IOCTL_CMD:
            /* If the parameters have not yet been initialized return an error. */
            if (params_p->module == SDHC_MODULE__END)
            {
                return -ENOTCONN;
            }
            /* Copy the command structure from user space */
            if (copy_from_user ((void *)&cmddata, (void *)arg, sizeof(cmddata)) != 0)
            {
                return -EFAULT;
            }

            /* Set up params for this command */
            params_p->format_of_response = cmddata.format_of_response;
            params_p->data_enable = 0;
            params_p->write_read = 0;

            /* Copy the data from the command structure into SDHC_IODATA_T */

            /* Copy the command and argument */
            io_data.cmd = cmddata.command;
            io_data.arg = cmddata.argument;

            /* If the response pointer is already a kernel pointer and not NULL, nothing to do */
            if ((cmddata.resp_is_kernel_space) && (cmddata.resp != NULL))
            {
                io_data.resp_base = cmddata.resp;
            }

            /* Else, Response must be a user-space pointer (or NULL) */
            else
            {
                /* Set the response base address to the scratch buffer */
                io_data.resp_base = fds_p->resp_buf;
            }

            /* Set the data base address to null and NOB to 0 */
            io_data.data_base = NULL;
            io_data.nob = 0;

            /* Perform the transfer.  This will block until the transfers are complete. */
            retval = sdhc_transceive(params_p, &io_data);

            /* If the transfer was successful, see if we need to copy any data */
            if (retval < 0)
            {
                return retval;
            }

            /* Copy any response data back into the user space buffers */
            if ((!cmddata.resp_is_kernel_space) && (cmddata.resp != NULL) &&
                (cmddata.format_of_response != MOTO_SDHC_NO_RESP))
            {
                /* Check for R2 response */
                if (cmddata.format_of_response == MOTO_SDHC_R2_RESP)
                {
                    resp_size = 16;  /* 128 bits */
                }
                else
                {
                    resp_size = 6;  /* 48 bits */
                }

                if (copy_to_user (cmddata.resp, fds_p->resp_buf, resp_size) != 0)
                {
                    return -EFAULT;
                }
            }
            break;

        case MOTO_SDHC_IOCTL_DATA:
            /* If the parameters have not yet been initialized return an error. */
            if (params_p->module == SDHC_MODULE__END)
            {
                return -ENOTCONN;
            }
            /* Copy the transceive structure from user space */
            if (copy_from_user ((void *)&xcvdata, (void *)arg, sizeof(xcvdata)) != 0)
            {
                return -EFAULT;
            }

            /* Set up params for this command */
            params_p->format_of_response = MOTO_SDHC_R1_RESP;  /* Always the case */
            params_p->data_enable = 1;
            params_p->write_read = xcvdata.is_write;

            /* Copy the data from the transceive structure into SDHC_IODATA_T */

            /* Copy the command and argument */
            io_data.cmd = xcvdata.command;
            io_data.arg = xcvdata.argument;

            /* If the response pointer is already a kernel pointer and not NULL, nothing to do */
            if ((xcvdata.resp_is_kernel_space) && (xcvdata.resp != NULL))
            {
                io_data.resp_base = xcvdata.resp;
            }

            /* Else, Response must be a user-space pointer (or NULL) */
            else
            {
                /* Set the response base address to the scratch buffer */
                io_data.resp_base = fds_p->resp_buf;
            }

            /* If the data is already a kernel pointer, nothing to do */
            if ((xcvdata.data_is_kernel_space) && (xcvdata.data != NULL))
            {
                io_data.data_base = xcvdata.data;
            }

            /* Else, data must be a user-space pointer (or NULL) */
            else
            {
                /* If the data is larger than the scratch buffer, increase the size
                 * of the scratch buffer. */
                if ((params_p->blk_len * xcvdata.nob) > fds_p->data_sz)
                {
                    /* Set the size to 0 in case of an allocation error. */
                    fds_p->data_sz = 0;
                    kfree(fds_p->data_buf);
                    fds_p->data_buf = kmalloc((params_p->blk_len * xcvdata.nob), GFP_KERNEL|__GFP_DMA);
                    if (fds_p->data_buf == NULL)
                    {
                        return -ENOMEM;
                    }
                    fds_p->data_sz = params_p->blk_len * xcvdata.nob;
                }

                /* Must copy data from user space if write */
                if (xcvdata.is_write)
                {
                    /* Writes make no sense if the data is null */
                    if (xcvdata.data == NULL)
                    {
                        return -EFAULT;
                    }

                    /* Copy the data from user space into the scratch buffer */
                    if (copy_from_user (fds_p->data_buf, xcvdata.data,
                                        (params_p->blk_len * xcvdata.nob)) != 0)
                    {
                        return -EFAULT;
                    }
                }

                /* Set the data base address to the scratch buffer */
                io_data.data_base = fds_p->data_buf;
            }
            io_data.nob = xcvdata.nob;

            /* Perform the transfer.  This will block until the transfers are complete. */
            retval = sdhc_transceive(params_p, &io_data);

            /* If the transfer was successful, see if we need to copy any data */
            if (retval < 0)
            {
                return retval;
            }

            /* Copy any Rx data back into the user space buffers */
            if ((!xcvdata.is_write) && (!xcvdata.data_is_kernel_space) && (xcvdata.data != NULL))
            {
                if (copy_to_user (xcvdata.data, fds_p->data_buf, (params_p->blk_len * xcvdata.nob)) != 0)
                {
                    return -EFAULT;
                }
            }

            /* Copy any response data back into the user space buffers */
            if ((!xcvdata.resp_is_kernel_space) && (xcvdata.resp != NULL))
            {
                resp_size = 6;  /* 48 bits - always */
                if (copy_to_user (xcvdata.resp, fds_p->resp_buf, resp_size) != 0)
                {
                    return -EFAULT;
                }
            }
            break;

        case MOTO_SDHC_IOCTL_SEQ:
            /*
             * MOTO_SDHC_SEQ_BEGIN:
             *
             * (Currently, nothing needs to be done for the start of a sequence.)
             */

            /*
             * MOTO_SDHC_SEQ_END:
             *
             * Need to poll if card still present since interrupts have been disabled
             * at some point during the command sequence.
             */
            if (arg == MOTO_SDHC_SEQ_END)
            {
                if (!sdhc_poll(params_p->module))
                {
                    /* Card missing in action!  Inform usr_blk_dev. */
                    usr_blk_dev_attach_irq(INT_MMC_SDHC1, (void *)(params_p->module+1), NULL);
                }
            }
            break;

        default:
            return -ENOTTY;
            break;
    }
    return 0;
}

/*! This structure defines the file operations for the SDHC device */
static struct file_operations sdhc_fops =
{
    .owner =    THIS_MODULE, 
    .ioctl =    sdhc_ioctl,
    .open =     sdhc_open,
    .release =  sdhc_close
};

/******************************************************************************
* Global Functions
******************************************************************************/

/*!
 * @brief SDHC module initialization function
 *
 * This function performs the initialization of the SDHC user driver.
 * The low level driver is initialized first and then the character device is
 * registered with the kernel.
 *
 * @return 0 upon success
 */

int __init moto_sdhc_init (void)
{
    int ret;

    /* Initialize the low level driver. */
    ret = sdhc_initialize();
    if (ret < 0)
    {
        tracemsg(_k_d("Unable to initialize the SDHC low level driver: %d"), ret);
        return ret;
    }

    /* Register our character device */
    ret = register_chrdev(MOTO_SDHC_DRIVER_MAJOR_NUM, MOTO_SDHC_DRIVER_DEV_NAME, &sdhc_fops);

    /* Display a message if the registration fails */
    if (ret < 0)
    {
        tracemsg(_k_d("Unable to get a major (%d) for SDHC driver: %d"),
            (int)MOTO_SDHC_DRIVER_MAJOR_NUM, ret);
        return ret;
    }

    devfs_mk_cdev(MKDEV(MOTO_SDHC_DRIVER_MAJOR_NUM,0), S_IFCHR | S_IRUSR | S_IWUSR, MOTO_SDHC_DRIVER_DEV_NAME);

    return 0;
}

/*!
 * @brief SDHC module cleanup function
 *
 * This function is called when the power IC driver is being destroyed (which
 * generally never happens).  Its purpose is to unregister the character special
 * device that was registered when the SDHC module was initialized.
 */

void __exit moto_sdhc_destroy (void)
{
    unsigned int i;
    
    /* Free any memory which is allocated to scratch buffers. */
    for (i = 0; i < MAX_OPENS; i++)
    {
        if (open_fds[i].open != 0)
        {
            kfree(open_fds[i].resp_buf);
            open_fds[i].resp_buf = NULL;

            kfree(open_fds[i].data_buf);
            open_fds[i].data_buf = NULL;
            open_fds[i].data_sz = 0;

            open_fds[i].open = 0;
        }
    }

    /* Cleanup the low level driver. */
    sdhc_cleanup();

    unregister_chrdev(MOTO_SDHC_DRIVER_MAJOR_NUM, MOTO_SDHC_DRIVER_DEV_NAME);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
module_init(moto_sdhc_init);
module_exit(moto_sdhc_destroy);

MODULE_AUTHOR("Motorola Inc");
MODULE_DESCRIPTION("User space access to the SDHC");
MODULE_LICENSE("GPL");
#endif
