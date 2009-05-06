/*
 * Copyright 2006 Motorola
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Motorola 2006-Oct-05 - Initial Creation
 * Motorola 2006-Oct-25 - Update comments
 */

/*!
 * @defgroup poweric_accy Power IC accessory driver
 *
 * This module provides the interface for accessory operations through /dev/accy.
 */

/*!
 * @defgroup poweric_debounce Power IC debounce
 *
 * This module debounces signals from the power IC, primarily keypresses.
 */

/*!
 * @defgroup poweric_debug Power IC debug
 *
 * This is a group of source code that is not compiled by default, but is
 * included with the driver for developer builds to aid debugging.
 */

/*!
 * @defgroup poweric_emu Power IC EMU driver
 *
 * This module handles the detection of all devices attached to the EMU bus.
 */

#ifndef __POWER_IC_KERNEL_H__
#define __POWER_IC_KERNEL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL__

 /*!
 * @file power_ic_kernel.h
 *
 * @brief This file contains defines, types, enums, macros, functions, etc. for the kernel.
 *
 * @ingroup poweric_core
 *
 * This file contains the following information:
 * - Kernel-Space Interface:
 *   - @ref kernel_reg_macros        "Register Macros"
 *   - @ref kernel_enums             "Enumerations"
 *   - @ref kernel_types             "Types"
 *   - @ref kernel_funcs_accy        "Accessory functions"
 *   - @ref kernel_funcs_atod        "AtoD functions"
 *   - @ref kernel_funcs_audio       "Audio functions"
 *   - @ref kernel_funcs_charger     "Charger functions"
 *   - @ref kernel_funcs_event       "Event functions"
 *   - @ref kernel_funcs_funlights   "Funlight lighting functions"
 *   - @ref kernel_funcs_periph      "Peripheral functions"
 *   - @ref kernel_funcs_pwr_mgmt    "Power Management functions"
 *   - @ref kernel_funcs_reg         "Register access functions"
 *   - @ref kernel_funcs_rtc         "RTC functions"
 *   - @ref kernel_funcs_touchscreen "Touchscreen functions"
 *   - @ref kernel_funcs_usr_blk_dev "User Block Device Control functions"
 */

/*==================================================================================================
                                         INCLUDE FILES
==================================================================================================*/

#include <linux/lights_funlights.h>
#include <linux/moto_accy.h>
#include <linux/power_ic.h>

#include <asm/ptrace.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/time.h>

/*!
 * @anchor kernel
 * @section kernel Kernel-space Interface
 *
 * The following macros, types and functions are only available from within the kernel. These
 * are inaccessible from user space.
 */
/*!
 * @anchor kernel_reg_macros
 * @name Register Macros
 *
 * The following macros are used to identify to which device a register belongs.
 */

/* @{ */

#ifdef CONFIG_MOT_POWER_IC_PCAP2
/*! @brief Returns 1 if given register is a PCAP register */
#define POWER_IC_REGISTER_IS_PCAP(reg) \
     (((reg) >= POWER_IC_REG_PCAP_FIRST_REG) && ((reg) <= POWER_IC_REG_PCAP_LAST_REG))

/*! @brief Returns 1 if given register is an EMU One Chip register */
#define POWER_IC_REGISTER_IS_EOC(reg) \
     (((reg) >= POWER_IC_REG_EOC_FIRST_REG) && ((reg) <= POWER_IC_REG_EOC_LAST_REG))

/*! @brief Returns 1 if given register is a Funlight register */
#define POWER_IC_REGISTER_IS_FL(reg) \
     (((reg) >= POWER_IC_REG_FL_FIRST_REG) && ((reg) <= POWER_IC_REG_FL_LAST_REG))
#elif defined(CONFIG_MOT_POWER_IC_ATLAS)
/*! @brief Returns 1 if given register is an Atlas register */
#define POWER_IC_REGISTER_IS_ATLAS(reg) \
     (((reg) >= POWER_IC_REG_ATLAS_FIRST_REG) && ((reg) <= POWER_IC_REG_ATLAS_LAST_REG))
#endif /* ATLAS */

/* @} End of register macros.  --------------------------------------------------------------------*/

/*!
 * @anchor kernel_types
 * @name Kernel-space types
 *
 * These types are only for use by the kernel-space interfaces.
 */

/* @{ */

/*! States to determine if Touchscreen should be Disabled, in Standby, Position,
 *  or Pressure Mode. 
 */
typedef enum
{
    POWER_IC_TS_MODE_DISABLED,
    POWER_IC_TS_MODE_LOW_PWR_MODE,
    POWER_IC_TS_MODE_FULL_PWR_MODE
} POWER_IC_TS_MODES_T;

/*! States to determine if the touchscreen has been pressed, the position, or if 
 *  it was released. 
 */
typedef enum
{
    POWER_IC_TS_PRESSED,
    POWER_IC_TS_POSITION,
    POWER_IC_TS_RELEASE,
    
    POWER_IC_TS_NUM
} POWER_IC_TS_CALLBACK_T;

/*!
 * The type of the callback functions used for events.  Function takes the
 * event number as its parameter and returns 0 if the event is NOT handled
 * by the function (and the next callback in the chain should be called) or
 * non-zero if the event is handled.
 */
typedef int (*POWER_IC_EVENT_CALLBACK_T)(POWER_IC_EVENT_T);

/*!
 * The type of callback functions that can be used for Touchscreen can either
 * take one or two parameters.  When the touchscreen callback function for 
 * position or pressure is called, both the x and y values will need to be 
 * passed.  When calling the release callback function, it is not necessary
 * to pass any parameters to the function.  These typedefs will be used
 * to cast the call to the callback in order to pass the correct number of 
 * parameters.
 */
typedef void (*POWER_IC_TS_PRESS_POSITION_T)(int x, int y);
typedef void (*POWER_IC_TS_RELEASE_T)(void);

/* @} End of kernel types  ------------------------------------------------------------------------*/

/*!
 * @anchor kernel_funcs_reg
 * @name Kernel-space register access functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * have register-level access to the power IC.  For more information, see the
 * documentation for the external.c file.
 */

/* @{ */

int power_ic_read_reg(POWER_IC_REGISTER_T reg, unsigned int *reg_value);
int power_ic_write_reg(POWER_IC_REGISTER_T reg, unsigned int *reg_value);
int power_ic_write_reg_value(POWER_IC_REGISTER_T reg, unsigned int reg_value);
int power_ic_set_reg_value(POWER_IC_REGISTER_T reg, int index, int value, int nb_bits);
int power_ic_get_reg_value(POWER_IC_REGISTER_T reg, int index, int *value, int nb_bits);
int power_ic_set_reg_mask(POWER_IC_REGISTER_T reg, int mask, int value);
int power_ic_set_reg_bit(POWER_IC_REGISTER_T reg, int index, int value);

#ifdef CONFIG_MOT_POWER_IC_PCAP2_SPI_BUS_INTERRUPT
int spi_int_reg_read (int reg, u32 *value_ptr);
int spi_int_reg_write (int reg, u32 value);
int spi_int_write_reg_value(POWER_IC_REGISTER_T reg, unsigned int reg_value);
int spi_int_set_reg_value(POWER_IC_REGISTER_T reg, int index, int value, int nb_bits);
int spi_int_get_reg_value(POWER_IC_REGISTER_T reg, int index, int *value, int nb_bits);
int spi_int_set_reg_bit(POWER_IC_REGISTER_T reg, int index, int value);
int spi_int_set_reg_mask(POWER_IC_REGISTER_T reg, int mask, int value);
#endif

/* @} End of kernel register access functions -----------------------------------------------------*/

/*!
 * @anchor kernel_funcs_backup_memory
 * @name Kernel-space Backup Memory API
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * have access to the Backup Memory registers.  For more information, see the
 * documentation for the backup_mem.c file.
 */

/* @{ */

int power_ic_backup_memory_read(POWER_IC_BACKUP_MEMORY_ID_T id, int* value);
int power_ic_backup_memory_write(POWER_IC_BACKUP_MEMORY_ID_T id, int value);

/* @} End of kernel Backup Memory API -------------------------------------------------------------*/

/*!
 * @anchor kernel_funcs_event
 * @name Kernel-space event handling functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * subscribe to and manage power IC events.  For more information, see the
 * documentation for the event.c file.
 */

/* @{ */

int power_ic_event_subscribe (POWER_IC_EVENT_T event, POWER_IC_EVENT_CALLBACK_T callback);
int power_ic_event_unsubscribe (POWER_IC_EVENT_T event, POWER_IC_EVENT_CALLBACK_T callback);
int power_ic_event_unmask (POWER_IC_EVENT_T event);
int power_ic_event_mask (POWER_IC_EVENT_T event);
int power_ic_event_clear (POWER_IC_EVENT_T event);
int power_ic_event_sense_read (POWER_IC_EVENT_T event);

/* @} End of kernel event functions ---------------------------------------------------------------*/

/*!
 * @anchor kernel_funcs_periph
 * @name Kernel-space peripheral functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * control power IC-related peripherals.  For more information, see the documentation
 * for the peripheral.c file.
 */

/* @{ */

int power_ic_periph_set_bluetooth_on(int on);
int power_ic_periph_set_flash_card_on(int on);
int power_ic_periph_set_vibrator_level(int level);
int power_ic_periph_set_vibrator_on(int on);
int power_ic_periph_is_usb_cable_connected (void);
int power_ic_periph_is_usb_pull_up_enabled (void);
void power_ic_periph_set_usb_pull_up (int on);
int power_ic_periph_set_wlan_on(int on);
int power_ic_periph_set_wlan_low_power_state_on(void);
int power_ic_periph_set_sim_voltage(unsigned char sim_card_num, POWER_IC_SIM_VOLTAGE_T volt);
int power_ic_periph_set_camera_on(int on);

/* @} End of kernel peripheral functions --------------------------------------------------------- */

/*!
 * @anchor kernel_funcs_rtc
 * @name Kernel-space RTC functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * access power IC real-time clock information.  For more information, see the
 * documentation for the rtc.c file.
 */

/* @{ */

int power_ic_get_num_power_cuts(int * data);
int power_ic_rtc_set_time(struct timeval * power_ic_time);
int power_ic_rtc_get_time(struct timeval * power_ic_time);
int power_ic_rtc_set_time_alarm(struct timeval * power_ic_time);
int power_ic_rtc_get_time_alarm(struct timeval * power_ic_time);

/* @} End of kernel rtc functions ---------------------------------------------------------------- */


/*!
 * @anchor kernel_funcs_atod
 * @name Kernel-space AtoD functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * perform AtoD conversions.  For more information, see the documentation for the 
 * atod.c file.
 */

/* @{ */
int power_ic_atod_single_channel(POWER_IC_ATOD_CHANNEL_T channel, int * result);
int power_ic_atod_general_conversion(POWER_IC_ATOD_RESULT_GENERAL_CONVERSION_T * result);
int power_ic_atod_current_and_batt_conversion(POWER_IC_ATOD_TIMING_T timing,
                                              int timeout_secs,
                                              POWER_IC_ATOD_CURR_POLARITY_T polarity,
                                              int * batt_result, int * curr_result);
int power_ic_atod_touchscreen_position_conversion(int * x, int * y);
int power_ic_atod_touchscreen_pressure_conversion(int * p);
int power_ic_atod_raw_conversion(POWER_IC_ATOD_CHANNEL_T channel, int * samples, int * length);
int power_ic_atod_set_touchscreen_mode(POWER_IC_TS_MODES_T mode);
/* @} End of kernel atod functions --------------------------------------------------------------- */

/*!
 * @anchor kernel_funcs_pwr_mgmt
 * @name Kernel-space power management functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * perform power management functions. For more information, see the documentation
 * for the power_management.c file.
 */
 
/* @{ */
#ifdef CONFIG_MOT_POWER_IC_PCAP2
int power_ic_set_core_voltage(int millivolts);
#endif
/* @} End of power management functions.  ---------------------------------------------------------*/

/*!
 * @anchor kernel_funcs_audio
 * @name Kernel-space audio functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * perform audio registers read/write.  For more information, see the documentation for the 
 * audio.c file.
 */

/* @{ */
int power_ic_audio_power_off(void);
int power_ic_audio_write_output_path(unsigned int out_type);
int power_ic_audio_read_output_path(void);
int power_ic_audio_write_input_path(POWER_IC_AUD_IN_PATH_T in_type);
POWER_IC_AUD_IN_PATH_T power_ic_audio_read_input_path(void);
int power_ic_audio_write_igain(int igain);
int power_ic_audio_read_igain(int *igain);
int power_ic_audio_write_ogain(int ogain);
int power_ic_audio_read_ogain(int* ogain);
int power_ic_audio_codec_en(bool val);
int power_ic_audio_st_dac_en(bool val);
int power_ic_audio_set_codec_sm(bool val);
int power_ic_audio_set_st_dac_sm(bool val);
int power_ic_audio_set_st_dac_sr(POWER_IC_ST_DAC_SR_T val);
int power_ic_audio_set_codec_sr(POWER_IC_CODEC_SR_T val);
int power_ic_audio_config_loopback(bool val);
int power_ic_audio_set_output_hp(bool val);
int power_ic_audio_set_input_hp(bool val);
int power_ic_audio_dither_en(bool val);
int power_ic_audio_set_st_dac_net_mode(int val);
int power_ic_audio_set_st_dac_num_ts(int val);
int power_ic_audio_st_dac_reset(void);
int power_ic_audio_codec_reset(void);
void power_ic_audio_conn_mode_set(unsigned int out_type, POWER_IC_AUD_IN_PATH_T in_type);

/* @} End of kernel audio functions -------------------------------------------------------------- */

/*!
 * @anchor kernel_funcs_charger
 * @name Kernel-space charger control functions
 *
 * These functions are exported by the driver to allow other kernel-space code to control
 * various charging capabilities.  For more information, see the documentation for the 
 * charger.c file.
 */

/* @{ */
int power_ic_charger_set_charge_voltage(int charge_voltage);
int power_ic_charger_set_charge_current(int charge_current);
int power_ic_charger_set_trickle_current(int charge_current);
int power_ic_charger_set_power_path(POWER_IC_CHARGER_POWER_PATH_T path);
int power_ic_charger_get_overvoltage(int * overvoltage);
int power_ic_charger_reset_overvoltage(void);
/* @} End of kernel charging functions ----------------------------------------------------------- */

/*!
 * @anchor kernel_funcs_touchscreen
 * @name Kernel-space touchscreen functions
 *
 * These functions are exported by the driver to allow other kernel-space code to
 * control various touchscreen capabilities.  For more information, see the documentation
 * for the touchscreen.c file.
 */

/* @{ */
void * power_ic_touchscreen_register_callback(POWER_IC_TS_CALLBACK_T callback, void * reg_ts_fcn);
int power_ic_touchscreen_set_repeat_interval(int period);
void power_ic_touchscreen_enable(bool on);
/* @} End of kernel touchscreen functions -------------------------------------------------------- */


/*==================================================================================================
  ==================================================================================================*/

/*
 * The structure below is used to pass information to the functions which are responsible for enabling
 * the LEDs for different regions. 
 */
typedef struct
{
    unsigned char port_id;              /* The gpio port number */
    unsigned char nLed;                 /* LED ID which is passed to the  set_tri_color_led. */
    unsigned char nSwitchedValue;       /* This value is used to enable or disabled the LED */
} LIGHTS_FL_LED_CTL_T;

/*
 * Structure used to configure the regions for different products.  See the configuration tables
 * below for more details.
 */
typedef struct
{
    bool (* const pFcn)(LIGHTS_FL_LED_CTL_T *, LIGHTS_FL_COLOR_T);
    LIGHTS_FL_LED_CTL_T xCtlData;  /* See LIGHTS_FL_LED_CTL_T for a description. */
} LIGHTS_FL_REGION_CFG_T;


extern LIGHTS_FL_REGION_MSK_T lights_fl_update   
(
    LIGHTS_FL_APP_CTL_T nApp,            
    unsigned char nRegions,
    ...
);

extern int lights_ioctl(unsigned int cmd, unsigned long arg);

/*============================================================================================
 =============================================================================================*/

/*!
 * @name Kernel-space accessory interface functions
 *
 * These functions are exported by the driver to allow other kernel-space code to control
 * various accessory operations. For more information, see the documentation for the 
 * accy.c file.
 */
/* @{ */
void moto_accy_init (void);
void moto_accy_destroy (void);
void moto_accy_notify_insert (MOTO_ACCY_TYPE_T accy);
void moto_accy_notify_remove (MOTO_ACCY_TYPE_T accy);
MOTO_ACCY_MASK_T moto_accy_get_all_devices(void);
void moto_accy_set_accessory_power(MOTO_ACCY_TYPE_T device, int on_off);
/* @} */

/*==========================================================================================
 ===========================================================================================*/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
extern void usr_blk_dev_attach_irq(int irq, void *device_msk, struct pt_regs *regs);
#else
extern irqreturn_t usr_blk_dev_attach_irq(int irq, void *device_msk, struct pt_regs *regs);
#endif

#endif /* KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __POWER_IC_KERNEL_H__ */

