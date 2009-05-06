/*
 * Copyright 2005-2006 - Motorola
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
 *
 *  Author     Date                Comment
 * ======== ===========   =====================================================
 * Motorola 2005-Dec-06 - File create.
 * Motorola 2006-Feb-12 - bug fix in sdhc_poll.
 * Motorola 2006-Mar-30 - MMC low power mode support on SCMA11
 * Motorola 2006-Jul-03 - Merge OSS cleanup to macau in kernel.
 * Motorola 2006-Aug-10 - Resolve all the OSS related issues.
 */

#ifndef __SDHC_MAIN_H__
#define __SDHC_MAIN_H__

#include <linux/types.h>
#include <asm/arch/platform.h>

/*!
 * @ingroup sdhc
 *
 * @file sdhc_main.h
 *
 * @brief Defines the external interface to the sdhc drivers.
 *
 * The SDHC drivers are used to send data to the SDHC modules.  This must be the only
 * interface to the SDHC hardware.
 *
 * @note This code was written to the SCMA11 DTS, Rev 2.0.  References to the
 *       SCMA11 spec in the code refer to this version.
 */

/*! @brief The size of the SDHC memory map (both SDHC1 and SDHC2) */
#define SDHC_MEM_SIZE        (16384*2)

/*!
 * @brief The module select shift.
 *
 * Use this to get the register base offset for the currently selected module.
 * SDHC base + (module << SDHC_MOD_SEL_SHFT) = Selected module base
 */
#define SDHC_MOD_SEL_SHFT    14

/*!
 * @name DMA burst lengths
 *
 * The length of a DMA burst length in bytes.
 */
/*! @{ */
#define SDHC_DMA_BURST_1BIT  16
#define SDHC_DMA_BURST_4BIT  64
/*! @} */

/*!
 * @name SDHC register offsets
 *
 * The register offsets from the SDHC register base.  Please see the SCMA11
 * spec section 78.3.1 for more information about the registers.
 */
/*! @{ */
#define SDHC_STR_STP_CLK     0x00   /* Clock Control register */
#define SDHC_STATUS          0x04   /* Status register */
#define SDHC_CLK_RATE        0x08   /* Card Clock Rate register */
#define SDHC_CMD_DAT_CONT    0x0C   /* Command Data Control register */
#define SDHC_RES_TO          0x10   /* Response Time-out register */
#define SDHC_READ_TO         0x14   /* Read Time-out register */
#define SDHC_BLK_LEN         0x18   /* Block Length register */
#define SDHC_NOB             0x1C   /* Number of Block register */
#define SDHC_REV_NO          0x20   /* Revision Number register */
#define SDHC_INT_CNTR        0x24   /* Interrupt Control register */
#define SDHC_CMD             0x28   /* Command Number register */
#define SDHC_ARG             0x2C   /* Command Argument register */
#define SDHC_RES_FIFO        0x34   /* Command Response FIFO Access register */
#define SDHC_BUFFER_ACCESS   0x38   /* Data Buffer Access register */
/*! @} */

/*!
 * @name SDHC register values
 */
/*! @{ */
#define SDHC_VAL_RESET           0x00000008   /* STR_STP_CLK register */
#define SDHC_VAL_START_CLK       0x00000002   /* STR_STP_CLK register */
#define SDHC_VAL_STOP_CLK        0x00000001   /* STR_STP_CLK register */
#define SDHC_VAL_CLK_RUN         0x00000100   /* STATUS register */
#define SDHC_VAL_CMD_DONE        0x00002000   /* STATUS register */
#define SDHC_VAL_CLR_STATUS      0x00007E2F   /* STATUS register */
#define SDHC_VAL_CMD_ERROR       0x0000002F   /* STATUS register */
#define SDHC_VAL_CARD_INSERTION  0x80000000   /* STATUS register */
#define SDHC_VAL_CARD_REMOVAL    0x40000000   /* STATUS register */
#define SDHC_VAL_READ_WRITE_DONE 0x00001800   /* STATUS register */
#define SDHC_VAL_RESP_TO         0x000000FF   /* RES_TO register */
#define SDHC_VAL_END_CMD_RES_EN  0x00000004   /* INT_CNTR register */
#define SDHC_VAL_INSERTION_EN    0x00008000   /* INT_CNTR register */
#define SDHC_VAL_REMOVAL_EN      0x00004000   /* INT_CNTR register */
#define SDHC_VAL_DETECT_INT      0x0000C000   /* INT_CNTR register */
/*! @} */

/*!
 * @brief All of the possible SDHC modules which data can be sent out on.
 */
typedef enum
{
    SDHC_MODULE_1,
    SDHC_MODULE_2,
    SDHC_MODULE__END  /*!< Must be the last entry in the enumeration.  */
} SDHC_MODULE_T;

/*!
 * @brief Holds data which is allowed to change between SDHC transactions.
 *
 * Includes the following:
 *   -# Module to use for the transmission.
 *   -# Flag to control the width of the data bus.
 *   -# Flag to indicate if a clock prefix is necessary before transmission.
 *   -# Flag to indicate if data command is a read or write
 *   -# Flag to indicate if this is a data command.
 *   -# The format of the command response.
 *   -# The length of the data blocks.
 *   -# The speed for the transmission
 */
typedef struct
{
    /*! The SDHC module which the target device is attached. */
    SDHC_MODULE_T module;

    /*! Specifies the width of the data bus.  False = 1-bit, True = 4-bit. */
    unsigned int four_bit_bus:1;

    /*!
     * Controls if an additional 80 clock cycle prefix will happen before sending a
     * command.  False = disable, True = enable.
     */
    unsigned int clk_prefix:1;

    /*!
     * Specifies whether the data transfer of the current command is a write or read
     * operation.  0=read, 1=write
     */
    unsigned int write_read:1;

    /*!
     * Specifies whether the current command includes a data transfer.
     * 0=no, 1=yes
     */
    unsigned int data_enable:1;

    /*!
     * Sets the response format for the command.
     * 00 = No response for command
     * 01 = R1/R5/R6 (48-bit response with CRC7)
     * 10 = R2 (136-bit, CSD/CID read response)
     * 11 = R3/R4 (48-bit response without CRC check)
     */
    unsigned int format_of_response:2;

    /*! Specifies the length of the data blocks. */
    uint16_t blk_len;

    /*!
     * The speed at which the transfer must be completed.  This value will be set
     * in the CLK_RATE register before the transmission.  See 78.3.3.3 in the
     * SCMA11 spec for details.
     */
    uint16_t speed;
      
} SDHC_DATA_PARAMETER_T;

/*!
 * @brief Structure used to specify start addresses for reads and writes to the SDHC.
 * 
 * This is used when a user wants to send multiple blocks in the same transmission.
 * This structure is also used for all internal transmits.  The resp_base must have a
 * valid address (if command has a response) when calling sdhc_transceive().
 */
typedef struct
{
    uint8_t cmd;       /*!< Command number to be sent to the card. */
    uint32_t arg;      /*!< Argument to be sent to the card. */
    void *resp_base;   /*!< Base address of the response buffer. */
    void *data_base;   /*!< Base address of the data buffer. */
    uint16_t nob;      /*!< Number of block in the data transfer. */
} SDHC_IODATA_T;

extern int sdhc_poll(SDHC_MODULE_T module);
extern int sdhc_transceive(SDHC_DATA_PARAMETER_T *params_p, SDHC_IODATA_T *io_data_p);
extern int sdhc_reset(SDHC_MODULE_T module, bool enable_int);
extern int sdhc_initialize(void);
extern void sdhc_cleanup(void);

#endif /* __SDHC_MAIN_H__ */
