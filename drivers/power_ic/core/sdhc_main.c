/*
 * drivers/power_ic/core/sdhc_main.c
 *
 * Copyright (C) 2005-2006 Motorola, Inc.
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
 * Motorola 2006-May-31 - Temporarily remove SDHC2 code for WIFI driver.
 * Motorola 2006-Apr-04 - Move all GPIO functionality to gpio.c
 * Motorola 2005-Oct-26 - Initial Creation
 */

/*!
 * @defgroup sdhc Interface to the SDHC hardware
 */

/*!
 * @file sdhc_main.c
 *
 * @ingroup sdhc
 *
 * @brief This is the main file for SDHC data flow management.
 *
 * <B>Overview:</B><BR>
 *   Provides the external interface by which data can be sent out on one of the
 *   SDIO buses.
 *
 *   The system works by first populating the SDHC registers with the necessary
 *   information as well as setting the SDMA parameters.  Then, the transmission is
 *   started if another transmission is not already in progress.  The command
 *   response is copied from the FIFO (if requested) when the transmission is
 *   complete.
 *
 *   All buffers must be DMA accessible for systems which support DMA at the low level.
 */

#include <linux/config.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/limits.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sdhc_user.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/usr_blk_dev.h>
#include <linux/wait.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/semaphore.h>

#include <asm/arch/clock.h>
#include <asm/arch/spba.h>

#include <stdbool.h>
#include "gpio.h"
#include "os_independent.h"
#include "sdhc_main.h"

/******************************************************************************
* Local constants and macros
******************************************************************************/

/*!
 * @name Bit positions in the CMD_DAT_CONT register.
 */
/*! @{ */
#define SDHC_BUS_WIDTH_SHFT     9
#define SDHC_CLK_PREFIX_SHFT    7
#define SDHC_READ_WRITE_SHFT    4
#define SDHC_DATA_ENABLE_SHFT   3
#define SDHC_FORMAT_SHFT        0
/*! @} */

/* #define SDHC_MAIN_DEBUG_1 */
#ifdef SDHC_MAIN_DEBUG_1
# define DEBUG_MSG_LEVEL1 printk
#else
# define DEBUG_MSG_LEVEL1(fmt, args...)
#endif

/* #define SDHC_MAIN_DEBUG_2 */
#ifdef SDHC_MAIN_DEBUG_2
# define DEBUG_MSG_LEVEL2 printk
#else
# define DEBUG_MSG_LEVEL2(fmt, args...)
#endif

/*
 * Temporary define created to resolve a conflict over SDHC2 with the WIFI
 * driver.  Instead of removing the offending code entirely, the code was
 * placed inside conditionals to allow for easy re-enabling of the SDHC2
 * code and to make it easier to undo these changes.  Once a permanent
 * solution to the conflict is implemented, this define and its conditionals
 * can be removed.
 */
/* #define USE_BOTH_SDHC_MODULES */

/******************************************************************************
* Local variables
******************************************************************************/
/*!
 * @brief The ioremapped base address of SDHC.
 *
 * This is needed because physical addresses != virtual addresses.
 */
static void *reg_base_p;

/*!
 * @brief Structure to hold SDHC module information.
 */
typedef struct
{
    /*!
     * @brief Mutual exclusion semaphore for the module.
     */
    struct semaphore mutex;

    /*!
     * @brief Used to flag when the end command response interrupt has occured.
     */
    bool end_cmd_resp;

    /*!
     * @brief Wait queue used to wait for an end command response.
     */
    wait_queue_head_t end_cmd_resp_wait_queue;

    /*!
     * @brief The channel number to use for DMA transfers.
     */
    int dma_channel;

    /*!
     * @brief Flag to indicate if DMA transfer is in progress.
     */
    bool data_transferring;
} SDHC_INFO_T;

static SDHC_INFO_T sdhc_info[SDHC_MODULE__END];

/******************************************************************************
* Local functions
******************************************************************************/
/*!
 * @brief Reads the value from a SDHC register
 *
 * @param    module The SDHC module which has the register to be read.
 * @param    reg    The specific register to read.
 *
 * @return   The value of the specified register
 *
 * @note No range checking is done on module or reg. It is assumed this
 *       will be done by the caller.
 */
static uint32_t sdhc_read_reg(SDHC_MODULE_T module, uint8_t reg)
{
    uint32_t value;

    value = readl(reg_base_p + (module << SDHC_MOD_SEL_SHFT) + reg);
    DEBUG_MSG_LEVEL2("sdhc_read_reg  - SDHC: %d Register: %02x Value: %08x\n", module+1, reg, value);

    return value;
}

/*!
 * @brief Writes a value to a SDHC register
 *
 * @param    module The SDHC module which has the register to be written.
 * @param    reg    The specific register to be written.
 * @param    value  The value to set the register to.
 *
 * @note No range checking is done on module or reg. It is assumed this
 *       will be done by the caller.
 */
static void sdhc_write_reg(SDHC_MODULE_T module, uint8_t reg, uint32_t value)
{
    DEBUG_MSG_LEVEL2("sdhc_write_reg - SDHC: %d Register: %02x Value: %08x\n", module+1, reg, value);
    writel(value, reg_base_p + (module << SDHC_MOD_SEL_SHFT) + reg);
}

/*!
 * @brief Enables a SDHC interrupt
 *
 * @param    module The current SDHC module
 * @param    value  A bit mask for SDHC register INT_CNTR
 *
 * @note No range checking is done on module. It is assumed this
 *       will be done by the caller.
 */
static void sdhc_enable_int(SDHC_MODULE_T module, uint32_t value)
{
    uint32_t int_cntr;

    int_cntr = sdhc_read_reg(module, SDHC_INT_CNTR);
    sdhc_write_reg(module, SDHC_INT_CNTR, int_cntr | value);
}

/*!
 * @brief Disables a SDHC interrupt
 *
 * @param    module The current SDHC module
 * @param    value  A bit mask for SDHC register INT_CNTR
 *
 * @note No range checking is done on module. It is assumed this
 *       will be done by the caller.
 */
static void sdhc_disable_int(SDHC_MODULE_T module, uint32_t value)
{
    uint32_t int_cntr;

    int_cntr = sdhc_read_reg(module, SDHC_INT_CNTR);
    sdhc_write_reg(module, SDHC_INT_CNTR, int_cntr & (~value));
}

/*!
 * Interrupt service routine for SDHC interrupts.
 *
 * @param   irq        The interrupt number
 * @param   private_p  Private data (not used)
 * @param   regs_p     Snapshot of the processor's context
 *
 * @return  Always return IRQ_RETVAL(1).
 */
static irqreturn_t sdhc_irq(int irq, void *private_p, struct pt_regs *regs_p)
{
    SDHC_MODULE_T module = SDHC_MODULE_1;
    uint32_t status;

    if (irq == INT_MMC_SDHC2)
    {
        module = SDHC_MODULE_2;
    }

    status = sdhc_read_reg(module, SDHC_STATUS);
    DEBUG_MSG_LEVEL1("sdhc_irq: status = %08x\n", status);

#ifdef CONFIG_MACH_SCMA11BB
    /* Check if any SDIO asynchronous detection interrupts are enabled. */
    if (sdhc_read_reg(module, SDHC_INT_CNTR) & SDHC_VAL_DETECT_WKP_INT)
    {
        /*
         * Disable the asynchronous detection interrupts.  If we don't disable them
         * here, we will continuously interrupt causing the system to freeze.
         */
        sdhc_disable_int(module, SDHC_VAL_DETECT_WKP_INT);

        /*
         * Asynchronous interrupts do not set the bits in the status register.
         * Because of this, do what is normally done if insertion/removal.
         */
        usr_blk_dev_attach_irq(irq, (void *)(module+1), regs_p);
    }
#endif

    if (status & SDHC_VAL_CMD_DONE)
    {
        /* Clear End Command Response bit */
        sdhc_write_reg(module, SDHC_STATUS, SDHC_VAL_CMD_DONE);

        sdhc_info[module].end_cmd_resp = true;
        wake_up(&sdhc_info[module].end_cmd_resp_wait_queue);
    }

    if (status & (SDHC_VAL_CARD_INSERTION | SDHC_VAL_CARD_REMOVAL))
    {
#ifdef CONFIG_MACH_SCMA11BB
        /* Inform usr_blk_dev of the change */
        /* NOTE: This will need to be changed if SDHC2 does not need usr_blk_dev. */
        usr_blk_dev_attach_irq(irq, (void *)(module+1), regs_p);
#endif

        /* Clear insertion/removal bit in status register */
        sdhc_write_reg(module, SDHC_STATUS, SDHC_VAL_CARD_INSERTION | SDHC_VAL_CARD_REMOVAL);
    }

    return IRQ_RETVAL(1);
}

/*!
 * SDMA interrupt service routine
 *
 * @param   info_p  Pointer to current SDHC module info structure
 */
static void sdhc_dma_irq(void *info_p)
{
    SDHC_INFO_T *sdhc_info = (SDHC_INFO_T *)info_p;

    DEBUG_MSG_LEVEL1("sdhc_dma_irq\n");

    mxc_dma_stop(sdhc_info->dma_channel);
    sdhc_info->data_transferring = false;
    wake_up(&sdhc_info->end_cmd_resp_wait_queue);
}

/*!
 * @brief Sets the block length and number of blocks in SDHC, then sets up the SDMA.
 *
 * @param params_p   Pointer to the SDHC parameters for the transmission.
 *                   This must be valid, no checking is done before it is used.
 * @param io_data_p  Pointer to the SDHC data information for the transmission.
 *                   This must be valid, no checking is done before it is used.
 */
static void sdhc_dma_setup(SDHC_DATA_PARAMETER_T *params_p, SDHC_IODATA_T *io_data_p)
{
    dma_request_t sdma_request;
    dma_channel_params sdma_params;

    /* Set the block length */
    sdhc_write_reg(params_p->module, SDHC_BLK_LEN, (uint32_t)(params_p->blk_len));

    /* Set the number of blocks */
    sdhc_write_reg(params_p->module, SDHC_NOB, (uint32_t)(io_data_p->nob));

    /* If write command */
    if (params_p->write_read)
    {
        sdma_request.sourceAddr = (__u8 *)__pa(io_data_p->data_base);
        sdma_params.transfer_type = emi_2_per;

        /* Flush the buffer in case it is in the processor cache. */
        consistent_sync(io_data_p->data_base,
                        (params_p->blk_len)*(io_data_p->nob),
                        DMA_TO_DEVICE);
    }
    /* If not write, must be read */
    else
    {
        sdma_request.destAddr = (__u8 *)__pa(io_data_p->data_base);
        sdma_params.transfer_type = per_2_emi;

        /* Invalidate the buffer in case it is in the processor cache.
         * 
         * NOTE: This is done here as a convenience and really should be done
         * after transfer completes in the DMA IRQ function.  However, this will
         * work if io_data_p->data_base is not accessed again until after the
         * transfer, as it is currently.
         */
        consistent_sync(io_data_p->data_base,
                        (params_p->blk_len)*(io_data_p->nob),
                        DMA_FROM_DEVICE);
    }

    /*
     * Setup DMA Channel parameters
     */
    /* 16 bytes in 1-bit mode and 64 bytes in 4-bit mode */
    sdma_params.watermark_level = (params_p->four_bit_bus ? SDHC_DMA_BURST_4BIT : SDHC_DMA_BURST_1BIT);
    sdma_params.peripheral_type = SDHC;
    sdma_params.per_address = MMC_SDHC1_BASE_ADDR + (params_p->module << SDHC_MOD_SEL_SHFT) + SDHC_BUFFER_ACCESS;
    sdma_params.event_id = (params_p->module == SDHC_MODULE_1 ? DMA_REQ_SDHC1 : DMA_REQ_SDHC2);
    sdma_params.bd_number = 1;
    sdma_params.word_size = TRANSFER_32BIT;
    sdma_params.callback = sdhc_dma_irq;
    sdma_params.arg = &sdhc_info[params_p->module];
    mxc_dma_setup_channel(sdhc_info[params_p->module].dma_channel, &sdma_params);

    sdma_request.count = (params_p->blk_len)*(io_data_p->nob);
    mxc_dma_set_config(sdhc_info[params_p->module].dma_channel, &sdma_request, 0);

    DEBUG_MSG_LEVEL1("sdhc_dma_setup: blk_len: %d nob: %d WML: %d Per Address: %08x Event id: %d\n",
                     params_p->blk_len, io_data_p->nob, sdma_params.watermark_level,
                     sdma_params.per_address, sdma_params.event_id);
}

/*!
 * @brief Sets up SDHC registers then starts the transfer.
 *
 * @param params_p   Pointer to the SDHC parameters for the transmission.
 *                   This must be valid, no checking is done before it is used.
 * @param io_data_p  Pointer to the SDHC data information for the transmission.
 *                   This must be valid, no checking is done before it is used.
 *
 * @return 0 upon success.<BR>
 *         -EIO if there was a time out or CRC error.<BR>
 */
static int sdhc_initiate_tx(SDHC_DATA_PARAMETER_T *params_p, SDHC_IODATA_T *io_data_p)
{
    uint32_t status;
    uint32_t cmd_dat_cont;
    uint8_t *resp_p;
    uint32_t fifo_val;
    unsigned int fifo_reads = 3;
    int retval = 0;
    unsigned int i = 5000000;
    SDHC_INFO_T *info_p = &sdhc_info[params_p->module];

    DEBUG_MSG_LEVEL1("sdhc_initiate_tx: 4bit: %d prefix: %d write_read: %d data: %d format %d cmd %d\n",
                     params_p->four_bit_bus, params_p->clk_prefix, params_p->write_read,
                     params_p->data_enable, params_p->format_of_response, io_data_p->cmd);

    info_p->end_cmd_resp = false;

    /* Wait for clock to stop from previous command if not stopped already. */
    do
    {
        status = sdhc_read_reg(params_p->module, SDHC_STATUS);
    } while (status & SDHC_VAL_CLK_RUN);

    /* Prepare to send data if this is a data command */
    if (params_p->data_enable)
    {
        sdhc_dma_setup(params_p, io_data_p);
    }

    /* Set the command */
    sdhc_write_reg(params_p->module, SDHC_CMD, io_data_p->cmd);

    /* Set the command argument */
    sdhc_write_reg(params_p->module, SDHC_ARG, io_data_p->arg);

    /* Set clock rate */
    sdhc_write_reg(params_p->module, SDHC_CLK_RATE, (uint32_t)params_p->speed);

    /* Configure the command data control register */
    cmd_dat_cont = (((uint32_t)params_p->four_bit_bus) << SDHC_BUS_WIDTH_SHFT) |
                   (((uint32_t)params_p->clk_prefix) << SDHC_CLK_PREFIX_SHFT) |
                   (((uint32_t)params_p->write_read) << SDHC_READ_WRITE_SHFT) |
                   (((uint32_t)params_p->data_enable) << SDHC_DATA_ENABLE_SHFT) |
                   ((uint32_t)params_p->format_of_response << SDHC_FORMAT_SHFT);
    sdhc_write_reg(params_p->module, SDHC_CMD_DAT_CONT, cmd_dat_cont);

    /* Start MMC_SD_CLK */
    sdhc_write_reg(params_p->module, SDHC_STR_STP_CLK, SDHC_VAL_START_CLK);

    /* Wait for end command response. */
    wait_event_interruptible(info_p->end_cmd_resp_wait_queue, info_p->end_cmd_resp);

    /* Read current status */
    status = sdhc_read_reg(params_p->module, SDHC_STATUS);

    /* Check to see if there was a time out or CRC error. */
    if (status & SDHC_VAL_CMD_ERROR)
    {
        DEBUG_MSG_LEVEL1("sdhc_initiate_tx: IO error - status %08x\n", status);
        retval = -EIO;
    }

    /* Start data transfer if this is a data command */
    if ((retval == 0) && (params_p->data_enable))
    {
#ifdef CONFIG_MACH_SCMA11BB
        /*
         * Detection interrupt must be disabled before data transfer because
         * DAT[3] will be used for data and not for card detection. However,
         * now that interrupts are disabled, we could miss a removal and therefore
         * the card needs to be polled after the transmission to confirm it is
         * still present.  We do not poll here - it is the caller's responsibility
         * to call sdhc_poll (which will also enable the detection interrupt).
         */
        sdhc_disable_int(params_p->module, SDHC_VAL_DETECT_INT);
#endif

        /* Start DMA transfer */
        info_p->data_transferring = true;
        mxc_dma_start(info_p->dma_channel);
    }

    /* Read FIFO if there is a response to this command */
    if ((retval == 0) && (params_p->format_of_response > MOTO_SDHC_NO_RESP))
    {
        resp_p = io_data_p->resp_base;

        if (params_p->format_of_response == MOTO_SDHC_R2_RESP)
        {
            fifo_reads = 8;  /* 8x16 bits = 128 bits */
        }

        do
        {
            fifo_val = sdhc_read_reg(params_p->module, SDHC_RES_FIFO);
            *resp_p++ = (uint8_t)((fifo_val & 0x0000FF00) >> 8);
            *resp_p++ = (uint8_t)(fifo_val & 0x000000FF);
            fifo_reads--;
        } while (fifo_reads > 0);
    }

    /* Finish data transfer if necessary. */
    if ((retval == 0) && (params_p->data_enable))
    {
        /* Wait for DMA transfer to finish (if it is not done already). */
        wait_event_interruptible(info_p->end_cmd_resp_wait_queue,
                                 !info_p->data_transferring);

        /* Check current status */
        status = sdhc_read_reg(params_p->module, SDHC_STATUS);

        /* Check to see if there was a time out or CRC error. */
        if (status & SDHC_VAL_CMD_ERROR)
        {
            DEBUG_MSG_LEVEL1("sdhc_initiate_tx: DMA IO error - status %08x\n", status);
            retval = -EIO;
        }

        /* Wait for READ_OP_DONE or WRITE_OP_DONE. */
        while (!(status & SDHC_VAL_READ_WRITE_DONE) && (--i))
        {
            status = sdhc_read_reg(params_p->module, SDHC_STATUS);
        }
    }

    /* Clear the status bits */
    sdhc_write_reg(params_p->module, SDHC_STATUS, SDHC_VAL_CLR_STATUS);

    /* Stop the clock */
    sdhc_write_reg(params_p->module, SDHC_STR_STP_CLK, SDHC_VAL_STOP_CLK);

    return retval;
}

#ifdef CONFIG_PM
/*!
 * @brief Indicate to the power management system if sleep can be entered
 *
 * This function will be called when the power management system tries to
 * put the phone to sleep. It must return 0 if sleep is acceptable or -EBUSY
 * if sleep cannot be entered. The phone will only be allowed to go to sleep
 * between transflash commands.
 *
 * @param   dev_p  Device pointer (not used)
 * @param   state  Power state to enter. (not used)
 * @param   level  The stage in device suspension process that we want the
 *                 device to be put in
 *
 * @return  0 if sleep is acceptable
 *          -EBUSY if sleep cannot be entered
 */
static int sdhc_suspend(struct device *dev_p, u32 state, u32 level)
{
    unsigned int i = 0;

    switch(level)
    {
        case SUSPEND_DISABLE:
            /* Do not allow sleep if transfer is in progress. */
#ifdef USE_BOTH_SDHC_MODULES
            for (i = 0; i < SDHC_MODULE__END; i++)
#endif
            {
                /*
                 * Check to see if SDHC is in use.  We use down_trylock here so
                 * we don't wait.
                 */
                if (!down_trylock(&sdhc_info[i].mutex))
                {
                    /* Release the semaphore since SDHC is not really needed. */
                    up(&sdhc_info[i].mutex);
                }
                else
                {
                    /* Semaphore could not be claimed, so SDHC is busy. */
                    return -EBUSY;
                }
            }
            break;

        case SUSPEND_SAVE_STATE:
            /* do nothing */
            break;

        case SUSPEND_POWER_DOWN:
            /* Disable SDHC clock */
            mxc_clks_disable(SDHC1_CLK);
#ifdef USE_BOTH_SDHC_MODULES
            mxc_clks_disable(SDHC2_CLK);
#endif

#ifdef CONFIG_MACH_SCMA11BB
            /* Enable SDIO asynchronous detection interrupts */
#ifdef USE_BOTH_SDHC_MODULES
            for (i = 0; i < SDHC_MODULE__END; i++)
#endif
            {
                if(sdhc_poll(i) == 0)
                {
                    /* Card is not present so look for insertion. */
                    sdhc_enable_int(i, SDHC_VAL_INSERT_WKP_EN);
                }
                else
                {
                    /* Card is present so look for removal. */
                    sdhc_enable_int(i, SDHC_VAL_REMOVAL_WKP_EN);
                }
            }
#endif
            break;
    }

    return 0;
}

/*!
 * @brief Called by power management system when system is waking up
 *
 * Called when sleep is exited to get things going after sleeping.  For
 * now, the only thing needed to be done is disabling the asynchronous
 * interrupts which were enabled when going to sleep.
 *
 * @param   dev_p Device pointer (not used)
 * @param   level The stage in device suspension process that we want the
 *                device to be put in
 *
 * @return  The function always returns 0.
 */
static int sdhc_resume(struct device *dev_p, u32 level)
{
#ifdef CONFIG_MACH_SCMA11BB
    unsigned int i = 0;
#endif

    switch(level)
    {
        case RESUME_POWER_ON:
#ifdef CONFIG_MACH_SCMA11BB
            /* Disable SDIO asynchronous detection interrupts */
#ifdef USE_BOTH_SDHC_MODULES
            for (i = 0; i < SDHC_MODULE__END; i++)
#endif
            {
                sdhc_disable_int(i, SDHC_VAL_DETECT_WKP_INT);
            }
#endif
            /* Enable SDHC clock */
            mxc_clks_enable(SDHC1_CLK);
#ifdef USE_BOTH_SDHC_MODULES
            mxc_clks_enable(SDHC2_CLK);
#endif
            break;
        case RESUME_RESTORE_STATE:
        case RESUME_ENABLE:
            /* do nothing */
            break;
    }
    return 0;
}
#else
#define sdhc_suspend  NULL
#define sdhc_resume   NULL
#endif /* CONFIG_PM */

/*!
 * @brief Contains pointers to the power management callback functions.
 */
static struct device_driver sdhc_driver =
{
    .name           = MOTO_SDHC_DRIVER_DEV_NAME,
    .bus            = &platform_bus_type,
    .suspend        = sdhc_suspend,
    .resume         = sdhc_resume
};

/*!
 * @brief This is platform device structure for the SDHC
 */
static struct platform_device sdhc_device =
{
    .name           = MOTO_SDHC_DRIVER_DEV_NAME,
    .id             = 0
};

/******************************************************************************
* Global functions
******************************************************************************/

/*!
 * @brief Polls to see if card is present
 *
 * @param    module The current SDHC module
 *
 * @note     No range checking is done on module. It is assumed this
 *           will be done by the caller.
 *
 * @return   0 when no card present.<BR>
 *           1 if card inserted.<BR>
 */
int sdhc_poll(SDHC_MODULE_T module)
{
#ifdef CONFIG_MACH_SCMA11BB
    int ret;
    static int last_poll = 0;

    /*
     * Only poll if SDHC is not in use.  We have to use down_trylock here because
     * this function could be called from an interrupt and waiting for the semaphore
     * to be freed would cause a deadlock.
     */
    if (!down_trylock(&sdhc_info[module].mutex))
    {
        /* Disable detection interrupt */
        sdhc_disable_int(module, SDHC_VAL_DETECT_INT);

        ret = last_poll = power_ic_gpio_sdhc_poll(module);

        /* 
         * Need to clear insert/remove bits in SDHC status register.  Changing the iomux config
         * causes them to get set and of course, that means we will be interrupted again
         * as soon as interrupts are enabled.
         */
        sdhc_write_reg(module, SDHC_STATUS, SDHC_VAL_CARD_INSERTION | SDHC_VAL_CARD_REMOVAL);

        /* Re-enable removal detection interrupt. */
        sdhc_enable_int(module, SDHC_VAL_REMOVAL_EN);

        /*
         * If card is present (ret = 1), wait until voltage settles (due to the iomux
         * switching) before enabling insertion detection interrupt.
         */
        if (ret)
        {
            udelay(125);
        }

        /* Clear insert bit in SDHC status register. */
        sdhc_write_reg(module, SDHC_STATUS, SDHC_VAL_CARD_INSERTION);

        /* Re-enable insertion detection interrupt. */
        sdhc_enable_int(module, SDHC_VAL_INSERTION_EN);

        /* Release the semaphore */
        up(&sdhc_info[module].mutex);
    }
    else
    {
        /*
         * SDHC busy, and therefore we can't poll right now.  So, return the result
         * of the last poll.
         */
        ret = last_poll;
    }

    DEBUG_MSG_LEVEL1("sdhc_poll: %d\n", ret);
    return ret;

#else
    return power_ic_gpio_sdhc_poll(module);
#endif
}

/*!
 * @brief Send and receive data over the SDHC
 *
 * Sends and receives data over the SDHC.  The data will be transmitted as soon as possible.
 *
 * @param     params_p   Contains the SDHC bus specific parameters for the transaction. These
 *                       include items such as the SDHC module and the transfer speed.
 * @param     io_data_p  Pointer to the data to send over the SDHC.
 *
 * @return    0 upon success.<BR>
 *            -EINVAL  - Null pointer passed in or bad data contained in params_p.<BR>
 *            -EIO if there was a time out or CRC error during transfer.<BR>
 */
int sdhc_transceive(SDHC_DATA_PARAMETER_T *params_p, SDHC_IODATA_T *io_data_p)
{
    int retval;

    DEBUG_MSG_LEVEL1("sdhc_transceive\n");
    if ((params_p == NULL) || (io_data_p == NULL))
    {
        printk("sdhc_transceive: Parameter and/or io_data pointers are null.\n");
        return -EINVAL;
    }

    /* Return an error if user specified resp pointer is null and command has response. */
    if ((params_p->format_of_response != MOTO_SDHC_NO_RESP) && (io_data_p->resp_base == NULL))
    {
        printk("sdhc_transceive: Vector base address for resp null.\n");
        return -EINVAL;
    }

    /* Return an error if user specified data pointer is null and command has data transfer. */
    if ((params_p->data_enable == 1) && (io_data_p->data_base == NULL))
    {
        printk("sdhc_transceive: Vector base address for data null.\n");
        return -EINVAL;
    }

    /* Check the SDHC module for the transaction and do not continue if it is invalid. */
    if (params_p->module >= SDHC_MODULE__END)
    {
        printk("sdhc_transceive: Invalid module %d.\n", params_p->module);
        return -EINVAL;
    }

    /* Claim SDHC module and start transmission. */
    retval = down_interruptible(&sdhc_info[params_p->module].mutex);
    if (!retval)
    {
        retval = sdhc_initiate_tx(params_p, io_data_p);
        up(&sdhc_info[params_p->module].mutex);
    }
    
    return retval;
}

/*!
 * @brief Resets the specified SDHC module
 *
 * @param    module      The SDHC module to reset
 * @param    enable_int  true = enable SDHC interrupts, false = no SDHC interrupts
 *
 * @note     No range checking is done on module. It is assumed this
 *           will be done by the caller.
 *
 * @return    0 upon success
 */
int sdhc_reset(SDHC_MODULE_T module, bool enable_int)
{
    int retval;
    unsigned int i;

    DEBUG_MSG_LEVEL1("sdhc_reset: module %d enable_int %d\n", module+1, enable_int);

    retval = down_interruptible(&sdhc_info[module].mutex);
    if (!retval)
    {
        /* Reset the SDHC module - see section 78.5.3.2 of SCMA11 spec for details */
        sdhc_write_reg(module, SDHC_STR_STP_CLK, SDHC_VAL_RESET);
        sdhc_write_reg(module, SDHC_STR_STP_CLK, SDHC_VAL_RESET | SDHC_VAL_STOP_CLK);
        for (i = 0; i < 8; i++)
        {
            sdhc_write_reg(module, SDHC_STR_STP_CLK, SDHC_VAL_STOP_CLK);
        }
        sdhc_write_reg(module, SDHC_RES_TO, SDHC_VAL_RESP_TO);

        if (enable_int)
        {
            /* Enable SDHC interrupts */
#ifdef CONFIG_MACH_SCMA11BB
            sdhc_enable_int(module, SDHC_VAL_END_CMD_RES_EN | SDHC_VAL_DETECT_INT);
#else
            sdhc_enable_int(module, SDHC_VAL_END_CMD_RES_EN);
#endif
        }

        up(&sdhc_info[module].mutex);
    }

    return retval;
}

/*!
 * @brief Initialize the SDHC memory region and setup GPIOs at power up.
 *
 * @return  0 upon success.
 */
int __init sdhc_initialize(void)
{
    unsigned int i = 0;
    int ret;

    DEBUG_MSG_LEVEL1("sdhc_initialize\n");

    reg_base_p = ioremap(MMC_SDHC1_BASE_ADDR, SDHC_MEM_SIZE);
    if (!reg_base_p)
    {
        printk("sdhc_initialize: Unable to map SDHC region.\n");
        return -ENOMEM;
    }

#ifdef USE_BOTH_SDHC_MODULES
    if (spba_take_ownership(SPBA_SDHC1, SPBA_MASTER_A | SPBA_MASTER_C) ||
        spba_take_ownership(SPBA_SDHC2, SPBA_MASTER_A | SPBA_MASTER_C))
#else
    if (spba_take_ownership(SPBA_SDHC1, SPBA_MASTER_A | SPBA_MASTER_C))
#endif
    {
        printk("sdhc_initialize: Unable to take SPBA ownership.\n");
        iounmap(reg_base_p);
        return -EBUSY;
    }

    ret = driver_register(&sdhc_driver);
    if (ret != 0)
    {
        printk("sdhc_initialize: Unable to register driver.\n");
        sdhc_cleanup();
        return ret;
    }

    ret = platform_device_register(&sdhc_device);
    if (ret != 0)
    {
        printk("sdhc_initialize: Unable to register platform device.\n");
        sdhc_cleanup();
        return ret;
    }

#ifdef USE_BOTH_SDHC_MODULES
    for (i = 0; i < SDHC_MODULE__END; i++)
#endif
    {
        /* Initialize module info */
        init_MUTEX(&sdhc_info[i].mutex);
        init_waitqueue_head(&sdhc_info[i].end_cmd_resp_wait_queue);

        /* Setup GPIO for the module */
        power_ic_gpio_sdhc_gpio_config(i, true);

        /* Reset the SDHC module */
        sdhc_reset(i, false);

        /* Enable end command response interrupt */
        sdhc_enable_int(i, SDHC_VAL_END_CMD_RES_EN);

        /* Request DMA channel */
        sdhc_info[i].dma_channel = 0;
        if (mxc_request_dma(&sdhc_info[i].dma_channel, MOTO_SDHC_DRIVER_DEV_NAME))
        {
            printk("sdhc_initialize: Unable to get DMA channel.\n");
            sdhc_cleanup();
            return -EBUSY;
        }
    }

    /* Set up interrupts */
#ifdef USE_BOTH_SDHC_MODULES
    if (request_irq(INT_MMC_SDHC1, sdhc_irq, 0, MOTO_SDHC_DRIVER_DEV_NAME, NULL) ||
        request_irq(INT_MMC_SDHC2, sdhc_irq, 0, MOTO_SDHC_DRIVER_DEV_NAME, NULL))
#else
    if (request_irq(INT_MMC_SDHC1, sdhc_irq, 0, MOTO_SDHC_DRIVER_DEV_NAME, NULL))
#endif
    {
        printk("sdhc_initialize: Unable to get IRQ.\n");
        sdhc_cleanup();
        return -EBUSY;
    }

    return 0;
}

/*!
 * @brief SDHC cleanup function
 *
 * This function unmaps and releases the memory region that was reserved
 * when the SDHC module was initialized.
 */

void sdhc_cleanup (void)
{
    unsigned int i = 0;

#ifdef USE_BOTH_SDHC_MODULES
    for (i = 0; i < SDHC_MODULE__END; i++)
#endif
    {
        /* Disable end command response interrupt */
        sdhc_disable_int(i, SDHC_VAL_END_CMD_RES_EN);

        power_ic_gpio_sdhc_gpio_config(i, false);
        mxc_free_dma(sdhc_info[i].dma_channel);
    }

    platform_device_unregister(&sdhc_device);
    driver_unregister(&sdhc_driver);

    free_irq(INT_MMC_SDHC1, NULL);
#ifdef USE_BOTH_SDHC_MODULES
    free_irq(INT_MMC_SDHC2, NULL);
#endif
    iounmap(reg_base_p);

    spba_rel_ownership(SPBA_SDHC1, SPBA_MASTER_A | SPBA_MASTER_C);
#ifdef USE_BOTH_SDHC_MODULES
    spba_rel_ownership(SPBA_SDHC2, SPBA_MASTER_A | SPBA_MASTER_C);
#endif
}
