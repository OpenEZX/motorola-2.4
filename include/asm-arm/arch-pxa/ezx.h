/*
 *  linux/include/asm-arm/arch-pxa/ezx.h
 *
 *  Specific macro defines for Motorola Ezx Development Platform
 *
 *  Author:     Zhuang Xiaofan
 *  Created:    Nov 25, 2003
 *  Copyright:  Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* support E680 p3 and ealier PCB */
//#define   E680_P3_AND_EARLY

/*
 * Flags in memory for sleep use
 */
#define FLAG_ADDR       PHYS_OFFSET
#define RESUME_ADDR     (PHYS_OFFSET + 4)
#define BPSIG_ADDR      (PHYS_OFFSET + 8)

#define USER_OFF_FLAG   0x5a5a5a5a
#define SLEEP_FLAG      0x6b6b6b6b
#define OFF_FLAG        0x7c7c7c7c
#define REFLASH_FLAG    0x0C1D2E3F
#define PASS_THRU_FLAG	0x12345678

#define WDI_FLAG        0xbb00dead
#define NO_FLAG         0xaa00dead

/*
 * GPIO control pin, have to change when hardware lock down
 */

#ifdef E680_P3_AND_EARLY

/* shakehand  with BP's PIN  */
#define GPIO_BP_RDY            0       /* BP_RDY     */
#define GPIO_BB_WDI            13      /* BB_WDI     */
#define GPIO_BB_WDI2           3       /* BB_WDI2    */
#define GPIO_BB_RESET          57      /* BB_RESET   */
#define GPIO_MCU_INT_SW        115     /* MCU_INT_SW */
#define GPIO_TC_MM_EN          89      /* TC_MM_EN   */

/* control PCAP direct PIN */
#define GPIO_WDI_AP            4       /* WDI_AP                       */
#define GPIO_SYS_RESTART       55      /* restart PCAP power           */
#define GPIO_AP_STANDBY        28      /* make pcap enter standby mode */ 

/* communicate with PCAP's PIN  */
#define GPIO_PCAP_SEC_INT      1       /* PCAP interrupt PIN to AP     */ 
#define GPIO_SPI_CLK           23      /* PCAP SPI port clock          */ 
#define GPIO_SPI_CE            24      /* PCAP SPI port SSPFRM         */  
#define GPIO_SPI_MOSI          25      /* PCAP SPI port SSPTXD         */ 
#define GPIO_SPI_MISO          26      /* PCAP SPI port SSPRXD         */ 

/*  blue tooth control PIN   */
#define GPIO_BT_WAKEUP         2       /* AP wake up bluetooth module        */
#define GPIO_BT_HOSTWAKE       14      /* bluetooth module wake up Ap module */
#define GPIO_BT_RESET          56      /* AP reset bluetooth module          */

/* control LCD high - OFF low -- ON  */
#define GPIO_LCD_OFF           116     /* control LCD                */

/*  FFUART PIN              */
#define GPIO_ICL_FFRXD_MD      (34 | GPIO_ALT_FN_1_IN)
#define GPIO_ICL_FFCTS_MD      (35 | GPIO_ALT_FN_1_IN)
#define GPIO_ICL_FFTXD_MD      (39 | GPIO_ALT_FN_2_OUT)
#define GPIO_ICL_FFRTS_MD      (41 | GPIO_ALT_FN_2_OUT)

#elif defined(A780_P1_AND_EARLY)

/* shakehand  with BP's PIN  */
#define GPIO_BP_RDY            0       /* BP_RDY     */
#define GPIO_BB_WDI            13      /* BB_WDI     */
#define GPIO_BB_WDI2           3       /* BB_WDI2    */
#define GPIO_BB_RESET          82      /* BB_RESET   */
#define GPIO_MCU_INT_SW        57      /* MCU_INT_SW */
#define GPIO_TC_MM_EN          89      /* TC_MM_EN   */

/* control PCAP direct PIN */
#define GPIO_WDI_AP            4       /* WDI_AP                       */
#define GPIO_SYS_RESTART       55      /* restart PCAP power           */
#define GPIO_AP_STANDBY        28      /* make pcap enter standby mode */ 

/* communicate with PCAP's PIN  */
#define GPIO_PCAP_SEC_INT      1       /* PCAP interrupt PIN to AP     */ 
#define GPIO_SPI_CLK           29      /* PCAP SPI port clock          */ 
#define GPIO_SPI_CE            24      /* PCAP SPI port SSPFRM         */  
#define GPIO_SPI_MOSI          25      /* PCAP SPI port SSPTXD         */ 
#define GPIO_SPI_MISO          26      /* PCAP SPI port SSPRXD         */ 

/*  blue tooth control PIN   */
#define GPIO_BT_WAKEUP         2       /* AP wake up bluetooth module        */
#define GPIO_BT_HOSTWAKE       14      /* bluetooth module wake up Ap module */
#define GPIO_BT_RESET          56      /* AP reset bluetooth module          */

/* control LCD high - OFF low -- ON  */
#define GPIO_LCD_OFF           116     /* control LCD                */

/*  FFUART PIN              */
#define GPIO_ICL_FFRXD_MD      (53 | GPIO_ALT_FN_1_IN)
#define GPIO_ICL_FFCTS_MD      (35 | GPIO_ALT_FN_1_IN)
#define GPIO_ICL_FFTXD_MD      (39 | GPIO_ALT_FN_2_OUT)
#define GPIO_ICL_FFRTS_MD      (41 | GPIO_ALT_FN_2_OUT)

#else

/* shakehand  with BP's PIN  */
#define GPIO_BP_RDY            0       /* BP_RDY     */
#define GPIO_BB_WDI            13      /* BB_WDI     */
#define GPIO_BB_WDI2           3       /* BB_WDI2    */
#define GPIO_BB_RESET          82      /* BB_RESET   */
#define GPIO_MCU_INT_SW        57      /* MCU_INT_SW */
#define GPIO_TC_MM_EN          99      /* TC_MM_EN   */

/* control PCAP direct PIN */
#define GPIO_WDI_AP            4       /* WDI_AP                       */
#define GPIO_SYS_RESTART       55      /* restart PCAP power           */
//#define GPIO_AP_STANDBY        28      /* make pcap enter standby mode */ 

/* communicate with PCAP's PIN  */
#define GPIO_PCAP_SEC_INT      1       /* PCAP interrupt PIN to AP     */ 
#define GPIO_SPI_CLK           29      /* PCAP SPI port clock          */ 
#define GPIO_SPI_CE            24      /* PCAP SPI port SSPFRM         */  
#define GPIO_SPI_MOSI          25      /* PCAP SPI port SSPTXD         */ 
#define GPIO_SPI_MISO          26      /* PCAP SPI port SSPRXD         */ 

/*  blue tooth control PIN   */
#define GPIO_BT_WAKEUP         28      /* AP wake up bluetooth module  */
#define GPIO_BT_HOSTWAKE       14      /* AP wake up bluetooth module  */
#define GPIO_BT_RESET          48      /* AP reset bluetooth module    */

/* control LCD high - OFF low -- ON  */
#define GPIO_LCD_OFF           116     /* control LCD                */

/*  FFUART PIN              */
#define GPIO_ICL_FFRXD_MD      (53 | GPIO_ALT_FN_1_IN)
#define GPIO_ICL_FFCTS_MD      (35 | GPIO_ALT_FN_1_IN)
#define GPIO_ICL_FFTXD_MD      (39 | GPIO_ALT_FN_2_OUT)
#define GPIO_ICL_FFRTS_MD      (41 | GPIO_ALT_FN_2_OUT)

#endif
/*
 * ezx platform, wake up source edge detect bit
 */
#define PEDR_INT_SEC            1

#define GPIO_FLIP_PIN          12
/*E680 screen lock button*/

#define GPIO_LOCK_SCREEN_PIN    GPIO_FLIP_PIN

/* MMC interface */
#define GPIO_MMC_DETECT         11
#define GPIO_MMC_CLK            32
#define GPIO_MMC_DATA0          92
#define GPIO_MMC_WP		107
#define GPIO_MMC_DATA1          109
#define GPIO_MMC_DATA2          110
#define GPIO_MMC_DATA3          111
#define GPIO_MMC_CMD            112

/* interface function */
#define GPIO_MMC_CLK_MD         (GPIO_MMC_CLK | GPIO_ALT_FN_2_OUT)
#define GPIO_MMC_DATA0_MD       (GPIO_MMC_DATA0 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT)
#define GPIO_MMC_DATA1_MD       (GPIO_MMC_DATA1 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT)
#define GPIO_MMC_DATA2_MD       (GPIO_MMC_DATA2 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT)
#define GPIO_MMC_DATA3_MD       (GPIO_MMC_DATA3 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT)

#define GPIO_MMC_CMD_MD         (GPIO_MMC_CMD | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT)

/* EMU GPIO 119 ---MUX2 120 --- MUX1   */
#define GPIO_EMU_MUX1      120
#define GPIO_EMU_MUX2      119
#define GPIO_SNP_INT_CTL   86
#define GPIO_SNP_INT_IN    87


/* audio related pins  */
#define AP_13MHZ_OUTPUT_PIN  9

#ifdef CONFIG_ARCH_EZX_E680
#define GPIO_VA_SEL_BUL     79
#define GPIO_FLT_SEL_BUL    80		/* out filter select pin */
#define GPIO_MIDI_RESET    78		/* GPIO used by MIDI chipset */
#define GPIO_MIDI_CS       33
#define GPIO_MIDI_IRQ      15
#define GPIO_MIDI_NPWE     49
#define GPIO_MIDI_RDY      18
#endif

#ifdef CONFIG_ARCH_EZX_A780
#define GPIO_HW_ATTENUATE_A780	96	/* hw noise attenuation be used or bypassed, for receiver or louderspeaker mode */
#endif


/* bp status pin */
#define GPIO_BP_STATE       41

/* define usb related pin  */
#define GPIO34_TXENB        34 
#define GPIO35_XRXD         35 
#define GPIO36_VMOUT        36 
#define GPIO39_VPOUT        39 
#define GPIO40_VPIN         40 
#define GPIO53_VMIN         53 

/* USB client 6 pin defination */
#define GPIO34_TXENB_MD     (GPIO34_TXENB | GPIO_ALT_FN_1_OUT)
#define GPIO35_XRXD_MD      (GPIO35_XRXD | GPIO_ALT_FN_2_IN )
#define GPIO36_VMOUT_MD     (GPIO36_VMOUT | GPIO_ALT_FN_1_OUT)
#define GPIO39_VPOUT_MD     (GPIO39_VPOUT | GPIO_ALT_FN_1_OUT)
#define GPIO40_VPIN_MD      (GPIO40_VPIN | GPIO_ALT_FN_3_IN )
#define GPIO53_VMIN_MD      (GPIO53_VMIN | GPIO_ALT_FN_2_IN )

#define GPIO53_FFRXD_MD     (53 | GPIO_ALT_FN_1_IN)



