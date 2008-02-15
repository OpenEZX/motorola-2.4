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
 * Motorola 2006-May-12 - Add Zeus support for SD
 */

 /*!
 * @file gpio.h
 *
 * @ingroup poweric_core
 *
 * @brief The main interface to GPIOs.
 *
 * This file contains all the function declarations for GPIOs
 */

#include <linux/lights_funlights.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include <asm/arch/pxa-regs.h>
#else
#include <asm/arch/gpio.h>
#endif

#if defined(CONFIG_ARCH_SCMA11) || defined(CONFIG_ARCH_ZEUS)
#include "sdhc_main.h"
#endif

#ifdef CONFIG_MOT_POWER_IC_PCAP2
#include "spi_main.h"
#endif
/******************************************************************************
* Local constants
******************************************************************************/
#define POWER_IC_UNUSED __attribute((unused))

/* define GPIO for stereo emu headset */
#define GPIO99_ST_HST  99

#ifdef CONFIG_MOT_POWER_IC_PCAP2
#define PM_INT   1
#elif defined(CONFIG_ARCH_SCMA11)
#define PM_INT   ED_INT5
#elif defined(CONFIG_MACH_ARGONPLUS_BUTE) || defined(CONFIG_MACH_ARGONLV_BUTE)
#define PM_INT   ED_INT1
#endif /* ArgonPlus & ArgonLV */

#ifdef CONFIG_ARCH_ZEUS

/*! @brief Pin to which the media card detect switch is attached. */
#define USR_BLK_DEV_DEV1_INT_GPIO 16

/*! @brief Returns non zero if the media card is attached.
 *
 * The gpio pin is low when the card is attached and high when it is removed.
 */
#define GPIO_DEV1_ATTACHED \
    ((gpio_get_data(1, USR_BLK_DEV_DEV1_INT_GPIO)) == 0)

#else

/*! @brief Port to which the media card detect switch is attached. */
#define USR_BLK_DEV_DEV1_INT_GPIO 11 

#ifndef CONFIG_ARCH_SCMA11
/*! @brief Returns non zero if the media card is attached. */
#define GPIO_DEV1_ATTACHED ((GPLR(USR_BLK_DEV_DEV1_INT_GPIO) & GPIO_bit(USR_BLK_DEV_DEV1_INT_GPIO)) != 0)
#endif

#endif

/******************************************************************************
* Macros
******************************************************************************/
/* Define GPIOs specific to PCAP2 */
#ifdef CONFIG_CPU_BULVERDE
#define EMU_CONFIG_HEADSET_MODE() \
    (set_GPIO_mode(GPIO99_ST_HST | GPIO_OUT))
    
#define EMU_SET_HEADSET_PULL_UP(pullup) \
({ \
    if(pullup == ENABLE) \
    { \
        set_GPIO(GPIO99_ST_HST); \
    } \
    else \
    { \
        clr_GPIO(GPIO99_ST_HST); \
    }  \
})

#define EMU_GET_D_PLUS() \
    ((GPLR(GPIO40_FFDTR) & GPIO_bit(GPIO40_FFDTR)) ? \
      EMU_BUS_SIGNAL_STATE_ENABLED : EMU_BUS_SIGNAL_STATE_DISABLED)

#define EMU_GET_D_MINUS() \
    ((GPLR(GPIO53_MMCCLK) & GPIO_bit(GPIO53_MMCCLK)) ? \
      EMU_BUS_SIGNAL_STATE_ENABLED : EMU_BUS_SIGNAL_STATE_DISABLED)

/* Define GPIOs specific to SCMA11 */
#elif defined(CONFIG_ARCH_SCMA11)
#define EMU_GET_D_PLUS() \
({\
    ((gpio_get_data(GPIO_AP_C_PORT, 16)) ? \
        EMU_BUS_SIGNAL_STATE_ENABLED : EMU_BUS_SIGNAL_STATE_DISABLED); \
})

#define EMU_GET_D_MINUS() \
({\
    ((gpio_get_data(GPIO_AP_C_PORT, 17)) ? \
      EMU_BUS_SIGNAL_STATE_ENABLED : EMU_BUS_SIGNAL_STATE_DISABLED); \
})

/* Define GPIOs specific to BUTE */
#elif defined (CONFIG_MACH_ARGONPLUS_BUTE) || defined(CONFIG_MACH_ARGONLV_BUTE)
#define EMU_GET_D_PLUS() \
({\
    ((gpio_get_data(0, 4)) ? \
        EMU_BUS_SIGNAL_STATE_ENABLED : EMU_BUS_SIGNAL_STATE_DISABLED); \
})

#define EMU_GET_D_MINUS() \
({\
    ((gpio_get_data(0, 6)) ? \
        EMU_BUS_SIGNAL_STATE_ENABLED : EMU_BUS_SIGNAL_STATE_DISABLED); \
})
 
#endif

/* Used for SDHC Driver */
#ifdef CONFIG_ARCH_SCMA11
#ifdef CONFIG_MACH_ASCENSION

#define ENABLE_TFLASH_IRQ()   (enable_irq(INT_EXT_INT4))
#define DISABLE_TFLASH_IRQ()  (disable_irq(INT_EXT_INT4))

#else /* Not Ascension */

#define ENABLE_TFLASH_IRQ() /* NULL */
#define DISABLE_TFLASH_IRQ() /* NULL */

#endif

#elif defined(CONFIG_ARCH_ZEUS)

#define ENABLE_TFLASH_IRQ() \
  do {                                                                       \
     /*                                                                      \
      * GPIO can't trigger on both edges, so when the interrupt is enabled   \
      * the proper edge must be set based on the current reading.            \
      */                                                                     \
     gpio_config(1, USR_BLK_DEV_DEV1_INT_GPIO, false, ((GPIO_DEV1_ATTACHED)  \
                 ? GPIO_INT_RISE_EDGE : GPIO_INT_FALL_EDGE));                \
     gpio_config_int_en(1, USR_BLK_DEV_DEV1_INT_GPIO, true);                 \
  } while (0);

#define DISABLE_TFLASH_IRQ()  (gpio_config_int_en(1, USR_BLK_DEV_DEV1_INT_GPIO, \
                                                  false))

#else /* Not SCMA11 and Not ZEUS */

#define ENABLE_TFLASH_IRQ()   (enable_irq(IRQ_GPIO(USR_BLK_DEV_DEV1_INT_GPIO)))
#define DISABLE_TFLASH_IRQ()  (disable_irq(IRQ_GPIO(USR_BLK_DEV_DEV1_INT_GPIO)))

#endif

/******************************************************************************
* function prototypes
******************************************************************************/
void power_ic_gpio_config_event_int(void);
int power_ic_gpio_event_read_priint(unsigned int signo);
void power_ic_gpio_event_clear_priint(void);
void power_ic_gpio_emu_config(void);
void power_ic_gpio_lights_config(void);
void power_ic_gpio_lights_set_main_display(LIGHTS_FL_COLOR_T nColor);
void power_ic_gpio_lights_set_keypad_base(LIGHTS_FL_COLOR_T nColor);
void power_ic_gpio_lights_set_keypad_slider(LIGHTS_FL_COLOR_T nColor);
void power_ic_gpio_lights_set_camera_flash(LIGHTS_FL_COLOR_T nColor);
void power_ic_gpio_sdhc_mmc_config(void * call_back);
#if defined(CONFIG_ARCH_SCMA11) || defined(CONFIG_ARCH_ZEUS)
void power_ic_gpio_sdhc_clear_irq(void);
void power_ic_gpio_sdhc_gpio_config(SDHC_MODULE_T module, bool enable);
int power_ic_gpio_sdhc_poll(SDHC_MODULE_T module);
#endif
#ifdef CONFIG_MOT_POWER_IC_PCAP2
int power_ic_gpio_spi_set_cs(SPI_CS_T cs, bool enable);
void power_ic_gpio_spi_initialize(void);
#endif
