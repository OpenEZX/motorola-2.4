/*
 * Copyright (C) 2006 Motorola, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 * Motorola 2006-Apr-04 - Confine all GPIO functions to this file
 * Motorola 2006-Apr-24 - Add 3.5mm headset support
 * Motorola 2006-May-12 - Add Zeus support for SD
 */
 
 /*!
 * @file gpio.c
 *
 * @ingroup poweric_core
 *
 * @brief The main interface to GPIOs.
 *
 * This file contains all the configurations, setting, and receiving of all GPIOs
 * for PCAP2 and ATLAS.
 */
#include <stdbool.h>

#include <linux/delay.h>
#include <linux/errno.h>
/* Must include before interrupt.h since interrupt.h depends on sched.h but is 
 * not included in interrupt.h */
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include <asm/arch/pxa-regs.h>
#else
#include <asm/boardrev.h>
#include <asm/arch/gpio.h>
#endif

#include "event.h"
#include "gpio.h"
#include "os_independent.h"

#if defined(CONFIG_ARCH_SCMA11) || defined(CONFIG_ARCH_ZEUS)
#include "sdhc_main.h"
#endif

#ifdef CONFIG_MOT_POWER_IC_PCAP2
#include "spi_main.h"
#endif
/******************************************************************************
* Local type definitions
******************************************************************************/

/******************************************************************************
* Local constants
******************************************************************************/
/* Main Display Lighting */
#define LIGHTS_FL_MAIN_BKL_PIN               30
#define LIGHTS_FL_PWM_BKL_PIN                17

/* EL Keypad Lighting */
#define LIGHTS_FL_EL_BASE                    27
#define LIGHTS_FL_EL_SLIDER                  29

/* Camera Flash */
#define LIGHTS_FL_CAMERA_EN_P1               10
#define LIGHTS_FL_CAMERA_EN_P2               13
#define LIGHTS_FL_TORCH_FLASH                30

#ifdef CONFIG_ARCH_ZEUS

/*
 * Defines to match scma11 in order to minimize the differences in this file.
 */
#define SP_SD1_CMD              SD1_CMD_PIN
#define SP_SD1_CLK              SD1_CLK_PIN
#define SP_SD1_DAT0             SD1_DAT0_PIN
#define SP_SD1_DAT1             SD1_DAT1_PIN
#define SP_SD1_DAT2             SD1_DAT2_PIN
#define SP_SD1_DAT3             SD1_DAT3_PIN
#define SP_SD2_CMD              SD2_CMD_PIN
#define SP_SD2_CLK              SD2_CLK_PIN
#define SP_SD2_DAT0             SD2_DAT0_PIN
#define SP_SD2_DAT1             SD2_DAT1_PIN
#define SP_SD2_DAT2             SD2_DAT2_PIN
#define SP_SD2_DAT3             SD2_DAT3_PIN
#define OUTPUTCONFIG_DEFAULT    MUX0_OUT
#define OUTPUTCONFIG_FUNC1      MUX0_OUT
#define INPUTCONFIG_FUNC1       MUX0_IN
#define INPUTCONFIG_NONE        NONE_IN

#endif /* CONFIG_ARCH_ZEUS */

/*! @brief Port to which the media card is attached. */
#define SDHC_POLL_GPIO_PORT 3

/*! @brief Bit which SDHC1 DAT[3] is connected on SDHC_POLL_GPIO_PORT. */
#define SDHC_1_GPIO_BIT 19

/*! @brief Bit which SDHC2 DAT[3] is connected on SDHC_POLL_GPIO_PORT. */
#define SDHC_2_GPIO_BIT 25

/******************************************************************************
* Local variables
******************************************************************************/
#ifdef CONFIG_MOT_POWER_IC_PCAP2
/*!
 * @brief Table which contains the SPI chip select data.  This table is indexed by
 * SPI_CS_T.
 */
static const struct
{
    int16_t gpio;
    bool active_high;
} pxa_ssp_cs_info[SPI_CS__END] =
    {
        { GPIO_SPI_CE,  true  },  /* SPI_CS_PCAP   */
        { GPIO20_DREQ0, false },  /* SPI_CS_TFLASH */
        { -1,           false }   /* SPI_CS_DUMMY  */
    };
#endif /* PCAP2 */

/******************************************************************************
* Local function prototypes
******************************************************************************/

/******************************************************************************
* Global functions
******************************************************************************/
/*!
 * @brief Initializes the power IC event GPIO lines
 *
 * This function initializes the power IC event GPIOs.  This includes:
 *     - Configuring the GPIO interrupt lines
 *     - Registering the GPIO interrupt handlers
 *
 */
void power_ic_gpio_config_event_int(void)
{
#ifdef CONFIG_CPU_BULVERDE
    /* Configure and register the PCAP interrupt */
    set_GPIO_mode(GPIO1_RST);
    set_GPIO_IRQ_edge(GPIO1_RST, GPIO_RISING_EDGE);
    request_irq(IRQ_GPIO(1), power_ic_irq_handler, 0, "PCAP irq", NULL);

    /* Configure and register the EMU One Chip interrupt */
    set_GPIO_mode(GPIO10_RTCCLK);
    set_GPIO_IRQ_edge(GPIO10_RTCCLK, GPIO_RISING_EDGE);
    request_irq(IRQ_GPIO(10), power_ic_irq_handler, 0, "EMU One Chip irq", NULL);

#ifdef CONFIG_MOT_POWER_IC_BARREL_HEADSET_STEREO_3MM5
    /* For Sumatra P4 to recognize standard 3.5mm headset */
    set_GPIO_mode(GPIO_HDST_VGS_BOOST|GPIO_OUT);
    set_GPIO(GPIO_HDST_VGS_BOOST);
#endif
#elif defined(CONFIG_ARCH_SCMA11)
    /* Configure mux */
    iomux_config_mux(AP_ED_INT5, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1); 
    /* Configure and register the ATLAS interrupt */
    edio_config(PM_INT, false, EDIO_INT_RISE_EDGE);
    request_irq(INT_EXT_INT5, power_ic_irq_handler, SA_INTERRUPT, "Atlas irq: SCM-A11", 0);
#elif defined (CONFIG_MACH_ARGONPLUS_BUTE) || defined(CONFIG_MACH_ARGONLV_BUTE)
    /* Configure mux */
    iomux_config_mux(PIN_PM_INT, OUTPUTCONFIG_ALT2, INPUTCONFIG_ALT2); 
    /* Configure and register the ATLAS interrupt */
    edio_config(PM_INT, false, EDIO_INT_RISE_EDGE);
    request_irq(INT_EXT_INT1, power_ic_irq_handler, SA_INTERRUPT, "Atlas irq: BUTE", 0);
#endif
}

/*
 * @brief Read the power IC event GPIO lines
 *
 * This function will read the primary interrupt GPIO line
 *
 * @return UINT32 value of the GPIO data
 *
 */
int power_ic_gpio_event_read_priint(unsigned int signo)
{
#ifdef CONFIG_MOT_POWER_IC_PCAP2
    return(GPLR(signo) & GPIO_bit(signo));
#elif defined(CONFIG_MOT_POWER_IC_ATLAS)
    return(edio_get_data(signo));
#else
    return(0);
#endif
}

/*
 * @brief Reset the power IC event GPIO lines
 *
 * This function will clear the primary interrupt GPIO line and then re-read it
 *
 * @note  This currently does not do anything for Bulverde
 */
void power_ic_gpio_event_clear_priint(void)
{
#if defined(CONFIG_ARCH_SCMA11) || defined(CONFIG_MACH_ARGONPLUS_BUTE) || defined(CONFIG_MACH_ARGONLV_BUTE)
    /* Clear interrupt */
    edio_clear_int(PM_INT);
    edio_get_int(PM_INT);
#elif !defined(CONFIG_MOT_POWER_IC_PCAP2) && !defined(CONFIG_ARCH_ZEUS)
    /* Build error out because Atlas is currently not implemented for Bulverde, so we do not
       know which GPIO will need to be constantly checked for the while loop.  Until Atlas is
       implemented, this conditional and error must stay */
    #error "Bulverde with ATLAS: GPIO not defined"
#endif
}

/*
 * @brief Configure the power IC EMU GPIO lines
 *
 * This function will configure the EMU GPIO lines for D+ and D-
 *
 * @note  This currently does not do anything for Bulverde
 */
void power_ic_gpio_emu_config(void)
{
#ifdef CONFIG_ARCH_SCMA11
    iomux_config_mux(AP_GPIO_AP_C16, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_FUNC3); 
    iomux_config_mux(AP_GPIO_AP_C17, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_FUNC3); 
    gpio_config(GPIO_AP_C_PORT, 16, false, GPIO_INT_NONE);
    gpio_config(GPIO_AP_C_PORT, 17, false, GPIO_INT_NONE);
#elif defined (CONFIG_MACH_ARGONPLUS_BUTE) || defined(CONFIG_MACH_ARGONLV_BUTE)
    iomux_config_mux(PIN_USB_VPIN, OUTPUTCONFIG_FUNC, INPUTCONFIG_GPIO);
    iomux_config_mux(PIN_USB_VMIN, OUTPUTCONFIG_FUNC, INPUTCONFIG_GPIO); 
    iomux_config_mux(PIN_GPIO4, OUTPUTCONFIG_FUNC, INPUTCONFIG_GPIO);
    iomux_config_mux(PIN_GPIO6, OUTPUTCONFIG_FUNC, INPUTCONFIG_GPIO);
    gpio_config(0, 4, false, GPIO_INT_NONE);  /* D+ for GPIO 4 (0 Base) */
    gpio_config(0, 6, false, GPIO_INT_NONE);  /* D- for GPIO 6 (0 Base) */
#endif
}
/*!
 * @brief Initializes the lighting
 *
 * The function performs any initialization required to get the lighting
 * for SCM-A11 running using GPIOs.  
 *
 * @note  This currently does not do anything for Bulverde or ArgonPlus
 */
void power_ic_gpio_lights_config(void)
{
#if defined(CONFIG_ARCH_SCMA11) && !(defined(CONFIG_MACH_ASCENSION))
#ifdef CONFIG_MOT_FEAT_BRDREV
    if(boardrev() < BOARDREV_P1A) 
    {
        iomux_config_mux(AP_GPIO_AP_B17, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
        gpio_config(GPIO_AP_B_PORT, LIGHTS_FL_PWM_BKL_PIN, true, GPIO_INT_NONE);
        gpio_set_data(GPIO_AP_B_PORT, LIGHTS_FL_PWM_BKL_PIN, 1);    
    }
#endif /* CONFIG_MOT_FEAT_BRDREV */
    iomux_config_mux(AP_IPU_D3_PS, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
    gpio_config(GPIO_AP_A_PORT, LIGHTS_FL_MAIN_BKL_PIN, true, GPIO_INT_NONE);
    gpio_set_data(GPIO_AP_A_PORT, LIGHTS_FL_MAIN_BKL_PIN, 1);  
#elif defined(CONFIG_MACH_ASCENSION)
    /* EL Keypad GPIOs */
    iomux_config_mux(SP_SPI1_CLK, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
    gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_EL_BASE, 0);
    gpio_config(GPIO_SP_A_PORT, LIGHTS_FL_EL_BASE, true, GPIO_INT_NONE);

    iomux_config_mux(SP_SPI1_MISO, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
    gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_EL_SLIDER, 0);
    gpio_config(GPIO_SP_A_PORT, LIGHTS_FL_EL_SLIDER, true, GPIO_INT_NONE);
    
    /* Camera Flash GPIOs */
    iomux_config_mux(SP_SPI1_SS0, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
    gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_TORCH_FLASH, 0);
    gpio_config(GPIO_SP_A_PORT, LIGHTS_FL_TORCH_FLASH, true, GPIO_INT_NONE);

    iomux_config_mux(SP_UH2_RXDM, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
    gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P2, 0);
    gpio_config(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P2, true, GPIO_INT_NONE);
    
    /* P1 Ascension is still hooked up to GPIO 10 so it needs to be configured */
    if(boardrev() < BOARDREV_P2A)
    {
        iomux_config_mux(SP_UH2_SUSPEND, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE);
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P1, 0);
        gpio_config(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P1, true, GPIO_INT_NONE);
    }
#endif /* SCM-A11 & ASCENSION */
}

/*!
 * @brief Set main display
 *
 *  This function will handle turning on and off the main display thru GPIO
 *
 * @param   nColor     The color to set the region to.
 *
 * @note  This currently does not do anything for Bulverde or ArgonPlus
 */
void power_ic_gpio_lights_set_main_display(LIGHTS_FL_COLOR_T nColor)
{
#if !(defined(CONFIG_MACH_ASCENSION)) && defined(CONFIG_ARCH_SCMA11)
#ifdef CONFIG_MOT_FEAT_BRDREV
    if(boardrev() < BOARDREV_P1A) 
    {
        gpio_set_data(GPIO_AP_B_PORT, LIGHTS_FL_PWM_BKL_PIN, (nColor != 0));    
    }
#endif /* CONFIG_MOT_FEAT_BRDREV */
    gpio_set_data(GPIO_AP_A_PORT, LIGHTS_FL_MAIN_BKL_PIN, (nColor != 0)); 
#endif
}


/*!
 * @brief Update keypad base light
 *
 * This function will handle keypad base light update. 
 *
 * @param  nColor       The color to set the region to. 
 *
 * @note  This currently does not do anything for Bulverde or ArgonPlus
 */
void power_ic_gpio_lights_set_keypad_base(LIGHTS_FL_COLOR_T nColor)
{
#ifdef CONFIG_MACH_ASCENSION
    gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_EL_BASE, (nColor != 0));
#endif
}

/*!
 * @brief Update keypad slider light
 *
 * This function will handle keypad slider light update. 
 *
 * @param  nColor       The color to set the region to. 
 *
 * @note  This currently does not do anything for Bulverde or ArgonPlus
 */
void power_ic_gpio_lights_set_keypad_slider(LIGHTS_FL_COLOR_T nColor)
{
#ifdef CONFIG_MACH_ASCENSION
    gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_EL_SLIDER, (nColor != 0));
#endif
}

/*!
 * @brief Sets the Camera Flash and Torch Mode
 * 
 * This function sets the camera flash and torch mode to the passed setting
 *
 * @param  nColor       The color to set the region to.
 *
 * @note  This currently does not do anything for Bulverde or ArgonPlus
 */
void power_ic_gpio_lights_set_camera_flash(LIGHTS_FL_COLOR_T nColor)
{
#ifdef CONFIG_MACH_ASCENSION
    if(nColor == LIGHTS_FL_CAMERA_FLASH )
    {
        /*Turn on the camera flash*/
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P2, 1);
        
        /* P1 Ascension is still hooked up to GPIO 10 so it needs to be shut off as well */
        if(boardrev() < BOARDREV_P2A)
        {
           /*Turn off the camera flash*/
            gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P1, 1);
        }
        
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_TORCH_FLASH, 1);
    }
    else if (nColor == LIGHTS_FL_CAMERA_TORCH)
    {
        /*Turn on the camera torch mode*/
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P2, 1);
        
        /* P1 Ascension is still hooked up to GPIO 10 so it needs to be shut off as well */
        if(boardrev() < BOARDREV_P2A)
        {
           /*Turn off the camera flash*/
            gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P1, 1);
        }
        
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_TORCH_FLASH, 0);
    }
    else 
    {
        /*Turn off the camera flash*/
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P2, 0);

        /* P1 Ascension is still hooked up to GPIO 10 so it needs to be shut off as well */
        if(boardrev() < BOARDREV_P2A)
        {
           /*Turn off the camera flash*/
            gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_CAMERA_EN_P1, 0);
        }
        
        gpio_set_data(GPIO_SP_A_PORT, LIGHTS_FL_TORCH_FLASH, 0);
    }
#endif
}

/*!
 * @brief Configures the MMC interrupts for detection.
 *
 * Configures interrupts for detecting the MMC card
 *
 * @note  This currently does not do anything for ArgonPlus
 */
void power_ic_gpio_sdhc_mmc_config(void * call_back)
{
#ifdef CONFIG_ARCH_SCMA11
#ifdef CONFIG_MACH_ASCENSION
    edio_clear_int(ED_INT4);
    edio_config(ED_INT4, false, EDIO_INT_BOTH_EDGE);
    if (request_irq(INT_EXT_INT4, call_back,
                    SA_INTERRUPT | SA_SAMPLE_RANDOM, "usr_blk_dev card detect", (void *)1))
    {
        printk(KERN_ERR "usr_blk_dev: Unable to initialize card detect interrupt.\n");
    }
#endif
#elif defined(CONFIG_CPU_BULVERDE) /* Bulverde */
    /* Set up the interrupt for detecting the media card. */
    set_GPIO_mode(USR_BLK_DEV_DEV1_INT_GPIO | GPIO_IN);
    set_GPIO_IRQ_edge(USR_BLK_DEV_DEV1_INT_GPIO, GPIO_FALLING_EDGE | GPIO_RISING_EDGE);
    if (request_irq(IRQ_GPIO(USR_BLK_DEV_DEV1_INT_GPIO), call_back,
                    SA_INTERRUPT | SA_SAMPLE_RANDOM, "usr_blk_dev card detect", (void *)1))
    {
        printk(KERN_ERR "usr_blk_dev: Unable to initialize card detect interrupt.\n");
    }
#elif defined(CONFIG_ARCH_ZEUS)
    /* Configure the card detect pin for GPIO */
    iomux_config_mux(SD1_MMC_PU_CTRL_PIN, MUX0_OUT, GPIO_MUX1_IN);

    /* Initialize the interrupt condition */
    gpio_config(1, USR_BLK_DEV_DEV1_INT_GPIO, false,
                (gpio_get_data(1, USR_BLK_DEV_DEV1_INT_GPIO)
                 ? GPIO_INT_FALL_EDGE : GPIO_INT_RISE_EDGE));

    /* Setup the interrupt handler */
    if (gpio_request_irq(1, USR_BLK_DEV_DEV1_INT_GPIO, GPIO_HIGH_PRIO,
                         call_back, 0, "MMC", (void *)1))
    {
        printk(KERN_ERR "usr_blk_dev: Unable to initialize card detect "
               "interrupt.\n");
    }
#endif
}

#if defined(CONFIG_ARCH_SCMA11) || defined(CONFIG_ARCH_ZEUS)
/*!
 * @brief Setup GPIO for SDHC
 *
 * @param module SDHC module number
 * @param enable Flag indicating whether to enable or disable the SDHC
 */
void power_ic_gpio_sdhc_gpio_config(SDHC_MODULE_T module, bool enable)
{
    unsigned int i;
    static const struct mux_struct
    {
        int pin;
        int out_config;
        int in_config;
    } mux_tb[4][6] =
    {
        {
            /* enable = 0, module = SDHC_MODULE_1 */
            { SP_SD1_CMD,  OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD1_CLK,  OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD1_DAT0, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD1_DAT1, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD1_DAT2, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD1_DAT3, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE }
        },
        {
            /* enable = 1, module = SDHC_MODULE_1 */
            { SP_SD1_CMD,  OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD1_CLK,  OUTPUTCONFIG_FUNC1, INPUTCONFIG_NONE  },
            { SP_SD1_DAT0, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD1_DAT1, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD1_DAT2, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD1_DAT3, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 }
        },
        {
            /* enable = 0, module = SDHC_MODULE_2 */
            { SP_SD2_CMD,  OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD2_CLK,  OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD2_DAT0, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD2_DAT1, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD2_DAT2, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE },
            { SP_SD2_DAT3, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_NONE }
        },
        {
            /* enable = 1, module = SDHC_MODULE_2 */
            { SP_SD2_CMD,  OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD2_CLK,  OUTPUTCONFIG_FUNC1, INPUTCONFIG_NONE  },
            { SP_SD2_DAT0, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD2_DAT1, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD2_DAT2, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 },
            { SP_SD2_DAT3, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1 }
        }
    };
    struct mux_struct *mux_p = (struct mux_struct *)&mux_tb[module*2 + enable];

    for (i = 0; i < 6; i++)
    {
        iomux_config_mux(mux_p->pin, mux_p->out_config, mux_p->in_config);
        mux_p++;
    }

#ifdef CONFIG_MACH_ZEUSEVB
    /*
     * The PBC is only present on the ZAS EVB.
     */
    {
        unsigned int pbc_bctrl1_set = readw(PBC_BASE_ADDRESS + PBC_BCTRL1_SET);
        unsigned int port_mask = ((module == 0) ? PBC_BCTRL1_MCP1
                                                : PBC_BCTRL1_MCP2);

        /* Set voltage for the MMC interface. */
        if (enable)
        {
            pbc_bctrl1_set |= port_mask;
        }
        else
        {
            pbc_bctrl1_set &= ~port_mask;
        }

        writew(pbc_bctrl1_set, PBC_BASE_ADDRESS + PBC_BCTRL1_SET);
    }
#endif
}

/*!
 * @brief Clear GPIO for SDHC
 *
 * This function clears the irq GPIO
 */
void power_ic_gpio_sdhc_clear_irq(void)
{
#ifdef CONFIG_MACH_ASCENSION
    edio_clear_int(ED_INT4);
#endif
}

/*!
 * @brief Polls to see if card is present
 *
 * @param    module SDHC module to poll.
 *
 * @note     No range checking is done on module. It is assumed this
 *           will be done by the caller.
 *
 * @return   0 when no card present.
 *           1 if card inserted.
 */
int power_ic_gpio_sdhc_poll(SDHC_MODULE_T module)
{
    int ret = 0;

#ifdef CONFIG_MACH_SCMA11BB
    enum iomux_pins pin = SP_SD1_DAT3;
    int bit = SDHC_1_GPIO_BIT;

    if(module == SDHC_MODULE_2)
    {
        pin = SP_SD2_DAT3;
        bit = SDHC_2_GPIO_BIT;
    }

    /* Set iomux so card can be polled. */
    iomux_config_mux(pin, OUTPUTCONFIG_DEFAULT, INPUTCONFIG_DEFAULT);
    udelay(125);

    /* Poll for card */
    ret = gpio_get_data(SDHC_POLL_GPIO_PORT, bit);

    /* Change iomux back */
    iomux_config_mux(pin, OUTPUTCONFIG_FUNC1, INPUTCONFIG_FUNC1);

#elif defined(CONFIG_MACH_ASCENSION)
    /* Driver only supports polling SDHC1 at this time. */
    if(module == SDHC_MODULE_1)
    {
        /*
         * If edio_get_data returns 0, card is present.  If 1, card not present.
         * Because of this, the return of edio_get_data is inverted to match what
         * this function is expected to return.
         */
        ret = !edio_get_data(ED_INT4);
    }
#endif

    return ret;
}
#endif /* CONFIG_ARCH_SCMA11 || CONFIG_ARCH_ZEUS */

#ifdef CONFIG_MOT_POWER_IC_PCAP2
/*!
 * @brief Sets the chip select to the requested level.
 *
 * Enables or disables the chip select which will be used for the
 * transfer. The polarity of the chip select is based on values in
 * the table pxa_ssp_cs_info.
 *
 * @param cs      The chip select which must be activated or deactivated.
 * @param enable  true when the chip select must be activated.
 *                false when the chip select must be deactivated.
 *
 * @return 0 upon success<BR>
 *         -EINVAL when the chip select is out of range of the table<BR>
 */
int power_ic_gpio_spi_set_cs(SPI_CS_T cs, bool enable)
{
    int16_t gpio;
    
    tracemsg(_k_d("power_ic_gpio_pxa_ssp_set_cs %d %d"), cs, enable);
    if (cs >= SPI_CS__END)
    {
        tracemsg(_k_d("power_ic_gpio_pxa_ssp_set_cs - Invalid CS number"));
        return -EINVAL;
    }
    
    gpio = pxa_ssp_cs_info[cs].gpio;
    /*
     * Only change the state of the GPIO if it is valid.  This is done to
     * support the dummy chip select.
     */
    if (gpio >= 0)
    {
        if (pxa_ssp_cs_info[cs].active_high == enable)
        {
            GPSR(gpio) = GPIO_bit(gpio);
        }
        else
        {
            GPCR(gpio) = GPIO_bit(gpio);
        }
    }
    return 0;
}

/*!
 * @brief Initialize the ssp ports used in SPI mode.
 *
 * This routine initializes the SSP hardware as well as the GPIO needed for SPI
 * transmits over the SSP.  The chip select ports will all be disabled by this
 * init routine.
 */
void power_ic_gpio_spi_initialize(void)
{
    SPI_CS_T cs;
    int16_t gpio;
    
    /* Set the GPIO muxes for CLK, MOSI and MISO. */
    set_GPIO_mode(GPIO_SPI_CLK|GPIO_ALT_FN_3_OUT);
    set_GPIO_mode(GPIO_SPI_MOSI|GPIO_ALT_FN_2_OUT);
    set_GPIO_mode(GPIO_SPI_MISO|GPIO_ALT_FN_1_IN);

    /* Set up all but the dummy chip select. */
    for (cs=0; cs < SPI_CS_DUMMY; cs++)
    {
        gpio = pxa_ssp_cs_info[cs].gpio;
        set_GPIO_mode(gpio|GPIO_OUT);
        power_ic_gpio_spi_set_cs(cs, false);
        /* The PGSR will be loaded with the inactive states for the GPIO to be 
           used since these settings are used during sleep and deep-sleep mode.
           The active states can found in the pxa_ssp_cs_info Table.  */  
        if (pxa_ssp_cs_info[cs].active_high)
        {
            PGSR(gpio) &= ~GPIO_bit(gpio);
        }
        else
        {
            PGSR(gpio) |= GPIO_bit(gpio);
        }
    }
    
    /*
     * The SPI_MOSI line has to be high during sleep and deep-sleep mode
     * to avoid current consumption through a 47k pull-up.
     */
    PGSR(GPIO_SPI_MOSI) |= GPIO_bit(GPIO_SPI_MOSI);
    
    /*
     * Disable the SSP and it's clock until a transmit is ready to go out.
     */
    SSCR0 = 0;
    CKEN &= ~CKEN23_SSP1;
}
#endif /* PCAP2 */
