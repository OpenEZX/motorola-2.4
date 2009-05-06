/**
 * Extension of the MMC driver from the Intel/Montavista patch to the
 * ARM/Linux kernel for Linux SDIO bus controller.
 *
 * Motorola Confidential Proprietary
 * Copyright (C) Motorola 2005. All rights reserved.
 *
 * Copyright (C) 2000-2003, Marvell Semiconductor, Inc. All rights reserved.
 */
/*
 *  linux/drivers/mmc/mmc_pxa.h 
 *
 *  Author: Vladimir Shebordaev, Igor Oblakov   
 *  Copyright:  MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef __SDIO_PXA_H__
#define __SDIO_PXA_H__

#include <linux/completion.h>

/* PXA-250 MMC controller registers */

/* Undefine the "MMC_..." macros which are defined later in the current 
 * file and also defined in 'include/asm/-arm/arch/pxa-regs.h' file. 
 * This is done to remove the compilation warnings, when the driver is 
 * compiled with MontaVista Kernel-2.4.20 + WE-16 patch, and also to keep 
 * our definations for these macros */

/* MMC_STRPCL */
#ifdef	MMC_STRPCL_STOP_CLK
#undef	MMC_STRPCL_STOP_CLK
#endif

#ifdef	MMC_STRPCL_START_CLK
#undef	MMC_STRPCL_START_CLK
#endif

/* MMC_STAT */
#ifdef	MMC_STAT_SDIO_SUSPEND_ACK
#undef	MMC_STAT_SDIO_SUSPEND_ACK
#endif

#ifdef	MMC_STAT_SDIO_INT
#undef	MMC_STAT_SDIO_INT
#endif

#ifdef	MMC_STAT_RD_STALLED
#undef	MMC_STAT_RD_STALLED
#endif

#ifdef	MMC_STAT_END_CMD_RES
#undef	MMC_STAT_END_CMD_RES
#endif

#ifdef	MMC_STAT_PRG_DONE
#undef	MMC_STAT_PRG_DONE
#endif

#ifdef	MMC_STAT_DATA_TRAN_DONE
#undef	MMC_STAT_DATA_TRAN_DONE
#endif

#ifdef	MMC_STAT_SPI_WR_ERR
#undef	MMC_STAT_SPI_WR_ERR
#endif

#ifdef	MMC_STAT_FLASH_ERR
#undef	MMC_STAT_FLASH_ERR
#endif

#ifdef	MMC_STAT_CLK_EN
#undef	MMC_STAT_CLK_EN
#endif

/* MMC_SPI */
#ifdef	MMC_SPI_CS_EN
#undef	MMC_SPI_CS_EN
#endif

#ifdef	MMC_SPI_CS_ADDRESS
#undef	MMC_SPI_CS_ADDRESS
#endif

/* MMC_CMDAT */
#ifdef	MMC_CMDAT_SDIO_RESUME
#undef	MMC_CMDAT_SDIO_RESUME
#endif

#ifdef	MMC_CMDAT_SDIO_SUSPEND
#undef	MMC_CMDAT_SDIO_SUSPEND
#endif

#ifdef	MMC_CMDAT_SDIO_INT_EN
#undef	MMC_CMDAT_SDIO_INT_EN
#endif

#ifdef	MMC_CMDAT_STOP_TRAN
#undef	MMC_CMDAT_STOP_TRAN
#endif

#ifdef	MMC_CMDAT_INIT
#undef	MMC_CMDAT_INIT
#endif

#ifdef	MMC_CMDAT_BUSY
#undef	MMC_CMDAT_BUSY
#endif

#ifdef	MMC_CMDAT_DATA_EN
#undef	MMC_CMDAT_DATA_EN
#endif

/* ######################  END OF UNDEFINING THE MACROS ####################### */


/* MMC_STRPCL */
#define MMC_STRPCL_STOP_CLK     (0x0001UL)
#define MMC_STRPCL_START_CLK        (0x0002UL)

/* MMC_STAT */
#define MMC_STAT_SDIO_SUSPEND_ACK   (0x0001UL << 16)
#define MMC_STAT_SDIO_INT	    (0x0001UL << 15)
#define MMC_STAT_RD_STALLED	    (0x0001UL << 14)
#define MMC_STAT_END_CMD_RES        (0x0001UL << 13) 
#define MMC_STAT_PRG_DONE           (0x0001UL << 12)
#define MMC_STAT_DATA_TRAN_DONE     (0x0001UL << 11)
#define MMC_STAT_SPI_WR_ERR	    (0x0001UL << 10)
#define MMC_STAT_FLASH_ERR	    (0x0001UL << 9)
#define MMC_STAT_CLK_EN             (0x0001UL << 8)
/* RPACE, in yellow book bits 7 and 6 are labelled as reserved */
#define MMC_STAT_RECV_FIFO_FULL     (0x0001UL << 7)
#define MMC_STAT_XMIT_FIFO_EMPTY    (0x0001UL << 6)
#define MMC_STAT_RES_CRC_ERROR      (0x0001UL << 5)
#define MMC_STAT_SPI_READ_ERROR_TOKEN   (0x0001UL << 4)
#define MMC_STAT_CRC_READ_ERROR     (0x0001UL << 3)
#define MMC_STAT_CRC_WRITE_ERROR    (0x0001UL << 2)
#define MMC_STAT_TIME_OUT_RESPONSE  (0x0001UL << 1)
#define MMC_STAT_READ_TIME_OUT      (0x0001UL)

#define MMC_STAT_ERRORS (MMC_STAT_RES_CRC_ERROR|MMC_STAT_SPI_READ_ERROR_TOKEN\
        |MMC_STAT_CRC_READ_ERROR|MMC_STAT_TIME_OUT_RESPONSE\
        |MMC_STAT_READ_TIME_OUT)

/* MMC_CLKRT */
#define MMC_CLKRT_20MHZ         (0x0000UL)
#define MMC_CLKRT_10MHZ         (0x0001UL)
#define MMC_CLKRT_5MHZ          (0x0002UL)
#define MMC_CLKRT_2_5MHZ        (0x0003UL)
#define MMC_CLKRT_1_25MHZ       (0x0004UL)
#define MMC_CLKRT_0_625MHZ      (0x0005UL)
#define MMC_CLKRT_0_3125MHZ     (0x0006UL)

/* MMC_SPI */
#define MMC_SPI_DISABLE         (0x00UL)
#define MMC_SPI_EN          (0x01UL)
#define MMC_SPI_CS_EN           (0x01UL << 2)
#define MMC_SPI_CS_ADDRESS      (0x01UL << 3)
#define MMC_SPI_CRC_ON          (0x01UL << 1)

/* MMC_CMDAT */
/* RPACE adder */

/* RPACE new yellow book items after bit 9 */
#define MMC_CMDAT_SDIO_RESUME   (0x0001UL << 13)
#define MMC_CMDAT_SDIO_SUSPEND  (0x0001UL << 12)
#define MMC_CMDAT_SDIO_INT_EN   (0x0001UL << 11)
#define MMC_CMDAT_STOP_TRAN	(0x0001UL << 10)
/* RPACE yellow boook says bit 9 is now reserved */
#define MMC_CMDAT_SD_PROT_EN	(0x0001UL << 9)
#define MMC_CMDAT_MMC_PROT_EN	(0x0000UL << 9)
#define MMC_CMDAT_SD_4DAT_EN	(0x0001UL << 8)
#define MMC_CMDAT_SD_1DAT_EN	(0x0000UL << 8)


#define MMC_CMDAT_MMC_DMA_EN    (0x0001UL << 7)
#define MMC_CMDAT_INIT          (0x0001UL << 6)
#define MMC_CMDAT_BUSY          (0x0001UL << 5)
#define MMC_CMDAT_STREAM        (0x0001UL << 4)
#define MMC_CMDAT_BLOCK         (0x0000UL << 4)
#define MMC_CMDAT_WRITE         (0x0001UL << 3)
#define MMC_CMDAT_READ          (0x0000UL << 3)
#define MMC_CMDAT_DATA_EN       (0x0001UL << 2)
/* RPACE yellow book says R1 is also R4,R5,R6 depending on MMC & SD/SDIO mode */
#define MMC_CMDAT_R1            (0x0001UL | MMC_CMDAT_SDIO_INT_EN)
#define MMC_CMDAT_R2            (0x0002UL | MMC_CMDAT_SDIO_INT_EN)
#define MMC_CMDAT_R3            (0x0003UL | MMC_CMDAT_SDIO_INT_EN)

/* MMC_RESTO */
#define MMC_RES_TO_MAX          (0x007fUL) /* [6:0] */

/* MMC_RDTO */
#define MMC_READ_TO_MAX         (0x0ffffUL) /* [15:0] */

/* MMC_BLKLEN */
/* RPACE yellow book now has this as 11:0, but will leave alone for now */
#define MMC_BLK_LEN_MAX         (0x03ffUL) /* [9:0] */

/* MMC_PRTBUF */
#define MMC_PRTBUF_BUF_PART_FULL       (0x01UL) 
#define MMC_PRTBUF_BUF_FULL		(0x00UL    )

/* MMC_I_MASK */
/* RPACE yellow book latest additions */
#define MMC_I_MASK_SDIO_SUSPEND_ACK	(0x01UL << 12)
#define MMC_I_MASK_SDIO_INT		(0x01UL << 11)
#define MMC_I_MASK_RD_STALLED		(0x01UL << 10)
#define MMC_I_MASK_RES_ERR		(0x01UL << 9)
#define MMC_I_MASK_DAT_ERR		(0x01UL << 8)

#define MMC_I_MASK_TINT			(0x01UL << 7)
#define MMC_I_MASK_TXFIFO_WR_REQ        (0x01UL << 6)
#define MMC_I_MASK_RXFIFO_RD_REQ        (0x01UL << 5)
#define MMC_I_MASK_CLK_IS_OFF           (0x01UL << 4)
#define MMC_I_MASK_STOP_CMD         	(0x01UL << 3)
#define MMC_I_MASK_END_CMD_RES          (0x01UL << 2)
#define MMC_I_MASK_PRG_DONE         	(0x01UL << 1)
#define MMC_I_MASK_DATA_TRAN_DONE       (0x01UL)
//#define MMC_I_MASK_DATA_TRAN_DONE       (0x00UL)
/* changed for bulverde */
// #define MMC_I_MASK_ALL              (0x07fUL)
// #define MMC_I_MASK_ALL              (0x0ffUL)
// one below was latest and greatest on 3/12 before change
#define MMC_I_MASK_ALL              (0x1fffUL)
//#define MMC_I_MASK_ALL              (0x08ffUL)


/* MMC_I_REG */
/* RPACE latest yellow book additions */
#define MMC_I_REG_SDIO_SUSPEND_ACK  (0x01UL << 12)
#define MMC_I_REG_SDIO_INT	    (0x01UL << 11)
#define MMC_I_REG_RD_STALLED	    (0x01UL << 10)
#define MMC_I_REG_RES_ERR	    (0x01UL << 9)
#define MMC_I_REG_DAT_ERR	    (0x01UL << 8)
#define MMC_I_REG_TINT		    (0x01UL << 7)

#define MMC_I_REG_TXFIFO_WR_REQ     (0x01UL << 6)
#define MMC_I_REG_RXFIFO_RD_REQ     (0x01UL << 5)
#define MMC_I_REG_CLK_IS_OFF        (0x01UL << 4)
#define MMC_I_REG_STOP_CMD          (0x01UL << 3)
#define MMC_I_REG_END_CMD_RES       (0x01UL << 2)
#define MMC_I_REG_PRG_DONE          (0x01UL << 1)
#define MMC_I_REG_DATA_TRAN_DONE    (0x01UL)
// #define MMC_I_REG_ALL               (0x007fUL)
#define MMC_I_REG_ALL               (0x01fffUL)

/* MMC_CMD */
#define MMC_CMD_INDEX_MAX       (0x006fUL)  /* [5:0] */
#define CMD(x)  (x)

/* MMC_ARGH */
/* MMC_ARGL */
/* MMC_RES */
/* MMC_RXFIFO */
#define MMC_RXFIFO_PHYS_ADDR 0x41100040 //MMC_RXFIFO physical address
/* MMC_TXFIFO */ 
#define MMC_TXFIFO_PHYS_ADDR 0x41100044 //MMC_TXFIFO physical address

/* implementation specific declarations */
/* RPACE will need changes for latest yellow book but leave as is for now */
#define PXA_MMC_BLKSZ_MAX (1<<9) /* actually 1023 */
#define PXA_MMC_NOB_MAX ((1<<16)-2)
#ifdef _USE_INTERNAL_SRAM
#define PXA_MMC_BLOCKS_PER_BUFFER (4)
#else
#define PXA_MMC_BLOCKS_PER_BUFFER (2)
#endif //_USE_INTERNAL_SRAM

#define PXA_MMC_IODATA_SIZE (PXA_MMC_BLOCKS_PER_BUFFER*PXA_MMC_BLKSZ_MAX) /* 1K */

#ifdef _USE_INTERNAL_SRAM
#define _DRAIN_WRITE_BUFFER() \
	{ \
		asm volatile ("mcr p15, 0, r0, c7, c10, 4"); \
	}
#endif //_USE_INTERNAL_SRAM

/* connect GPIO12 to DAT1 of the MMC pin for SDIO IRQ */
#define SDIO_IRQ_GPIO     10
#define IRQ_SDIO          IRQ_GPIO(SDIO_IRQ_GPIO)

typedef enum _pxa_mmc_fsm {         /* command processing FSM */
  PXA_MMC_FSM_IDLE = 1,
  PXA_MMC_FSM_CLK_OFF,
  PXA_MMC_FSM_END_CMD,
  PXA_MMC_FSM_BUFFER_IN_TRANSIT,
  PXA_MMC_FSM_END_BUFFER, 
  PXA_MMC_FSM_END_IO, 
  PXA_MMC_FSM_END_PRG,
  PXA_MMC_FSM_ERROR
} pxa_mmc_state_t;

#define PXA_MMC_STATE_LABEL( state ) (\
    (state == PXA_MMC_FSM_IDLE) ? "IDLE" :\
    (state == PXA_MMC_FSM_CLK_OFF) ? "CLK_OFF" :\
    (state == PXA_MMC_FSM_END_CMD) ? "END_CMD" :\
    (state == PXA_MMC_FSM_BUFFER_IN_TRANSIT) ? "IN_TRANSIT" :\
    (state == PXA_MMC_FSM_END_BUFFER) ? "END_BUFFER" :\
    (state == PXA_MMC_FSM_END_IO) ? "END_IO" :\
    (state == PXA_MMC_FSM_END_PRG) ? "END_PRG" : "UNKNOWN" )

typedef enum _pxa_mmc_result {
  PXA_MMC_NORMAL = 0,
  PXA_MMC_INVALID_STATE = -1,
  PXA_MMC_TIMEOUT = -2,
  PXA_MMC_ERROR = -3
} pxa_mmc_result_t;

typedef u32 pxa_mmc_clkrt_t;

typedef char *pxa_mmc_iodata_t;
#ifdef PIO
typedef struct _pxa_mmc_piobuf_rec {
  char *pos; /* current buffer position */
  int   cnt; /* byte counter */
} pxa_mmc_piobuf_rec_t, *pxa_mmc_piobuf_t;
#else /* i.e. DMA */
typedef struct _pxa_mmc_dmabuf_rec { /* TODO: buffer ring, DMA irq completion */
  int chan;                        /* dma channel no */
  dma_addr_t phys_addr;            /* iodata physical address */
  pxa_dma_desc *read_desc;         /* input descriptor array virtual address */
  pxa_dma_desc *write_desc;        /* output descriptor array virtual address*/
  dma_addr_t read_desc_phys_addr;  /* descriptor array physical address */
  dma_addr_t write_desc_phys_addr; /* descriptor array physical address */
  pxa_dma_desc *last_read_desc;    /* last input descriptor 
				    * used by the previous transfer 
				    */
  pxa_dma_desc *last_write_desc;   /* last output descriptor
				    * used by the previous transfer 
				    */
} pxa_mmc_dmabuf_rec_t, *pxa_mmc_dmabuf_t;
#endif

typedef struct _pxa_mmc_iobuf_rec {
  ssize_t blksz; /* current block size in bytes */
  ssize_t bufsz; /* buffer size for each transfer */
  ssize_t nob;   /* number of blocks pers buffer */ 
#ifndef PIO 
  pxa_mmc_dmabuf_rec_t buf; /* i.e. DMA buffer ring on the iodata */
#else /* i.e. DMA */
  pxa_mmc_piobuf_rec_t buf; /* PIO buffer accounting */
#endif
  pxa_mmc_iodata_t iodata;  /* I/O data buffer */
} pxa_mmc_iobuf_rec_t, *pxa_mmc_iobuf_t;

typedef struct _pxa_mmc_hostdata_rec {
  pxa_mmc_state_t state; /* FSM */
  int suspended;
  pxa_mmc_iobuf_rec_t iobuf; /* data transfer state */
    
  int busy;   /* atomic busy flag */
  struct completion completion; /* completion */
//#if CONFIG_MMC_DEBUG_IRQ
#if IRQ_DEBUG
  int irqcnt;
  int timeo;
#endif
    
  /* cached controller state */
  u32 mmc_i_reg;  /* interrupt last requested */
  u32 mmc_i_mask; /* mask to be set by intr handler */
  u32 mmc_stat;   /* status register at the last intr */
  u32 mmc_cmdat;  /* MMC_CMDAT at the last inr */
  u8 mmc_res[16]; /* response to the last command in host order */ 
  u32 saved_mmc_clkrt;
  u32 saved_mmc_high_clkrt; /* MMC_CLK for bus powersave mode */
  u32 saved_mmc_i_mask;
  u32 saved_mmc_resto;
  u32 saved_mmc_spi;
  u32 saved_drcmrrxmmc;
  u32 saved_drcmrtxmmc;

  /* controller options */
  pxa_mmc_clkrt_t clkrt; /* current bus clock rate */
} pxa_mmc_hostdata_rec_t, *pxa_mmc_hostdata_t;

#define PXA_MMC_STATUS( ctrlr ) (((pxa_mmc_hostdata_t)ctrlr->host_data)->mmc_stat)
#define PXA_MMC_RESPONSE( ctrlr, idx ) ((((pxa_mmc_hostdata_t)ctrlr->host_data)->mmc_res)[idx])
#define PXA_MMC_CLKRT( ctrlr ) (((pxa_mmc_hostdata_t)ctrlr->host_data)->clkrt)  

#define SAVED_MMC_CLKRT (hostdata->saved_mmc_clkrt)
#define SAVED_MMC_HIGH_CLKRT (hostdata->saved_mmc_high_clkrt)
#define SAVED_MMC_I_MASK (hostdata->saved_mmc_i_mask)
#define SAVED_MMC_RESTO (hostdata->saved_mmc_resto)
#define SAVED_MMC_SPI (hostdata->saved_mmc_spi)
#define SAVED_DRCMRRXMMC (hostdata->saved_drcmrrxmmc )
#define SAVED_DRCMRTXMMC (hostdata->saved_drcmrtxmmc )

/* PXA MMC controller specific card data */
typedef struct _pxa_mmc_card_data_rec { 
  pxa_mmc_clkrt_t clkrt;   /* clock rate to be set for the card */
} pxa_mmc_card_data_rec_t, *pxa_mmc_card_data_t;

#ifdef CONFIG_MMC_DEBUG
#undef MMC_DUMP_R1
#undef MMC_DUMP_R2
#undef MMC_DUMP_R3
#undef MMC_DUMP_R4
#undef MMC_DUMP_R5
#undef MMC_DUMP_R6
#define MMC_DUMP_R2( ctrlr ) MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R2 response: 0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 15 ), \
PXA_MMC_RESPONSE( ctrlr, 14 ), \
PXA_MMC_RESPONSE( ctrlr, 13 ), \
PXA_MMC_RESPONSE( ctrlr, 12 ), \
PXA_MMC_RESPONSE( ctrlr, 11 ), \
PXA_MMC_RESPONSE( ctrlr, 10 ), \
PXA_MMC_RESPONSE( ctrlr, 9 ), \
PXA_MMC_RESPONSE( ctrlr, 8 ), \
PXA_MMC_RESPONSE( ctrlr, 7 ), \
PXA_MMC_RESPONSE( ctrlr, 6 ), \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ), \
PXA_MMC_RESPONSE( ctrlr, 0 ) );
#define MMC_DUMP_R1( ctrlr ) MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R1(b) response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );
#define MMC_DUMP_R3( ctrlr )	MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R3 response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );
#define MMC_DUMP_R4( ctrlr )    MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R4 response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );
#define MMC_DUMP_R5( ctrlr )    MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R5 response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );
#define MMC_DUMP_R6( ctrlr )    MMC_DEBUG( MMC_DEBUG_LEVEL3, \
"R6 response: 0x%02x%02x%02x%02x%02x\n", \
PXA_MMC_RESPONSE( ctrlr, 5 ), \
PXA_MMC_RESPONSE( ctrlr, 4 ), \
PXA_MMC_RESPONSE( ctrlr, 3 ), \
PXA_MMC_RESPONSE( ctrlr, 2 ), \
PXA_MMC_RESPONSE( ctrlr, 1 ) );

#endif
#endif /* __MMC_PXA_P_H__ */
