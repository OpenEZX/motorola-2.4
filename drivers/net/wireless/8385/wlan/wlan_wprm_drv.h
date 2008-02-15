#ifndef _WLAN_WPRM_DRV_H_
#define _WLAN_WPRM_DRV_H_
/* wlan_wprm_drv.h --- This is header file for Power management APIs for WLAN driver
 *
 * (c) Copyright Motorola 2005-2006.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Revision History:
 * Author           Date            Description
 * Motorola         28/04/2005      Created
 * Motorola         24/06/2005      Turn on SD_CLK to receive BG-SCAN report event
 * Motorola         24/08/2005      WLAN driver traffic meter
 * Motorola         13/09/2005      Add "iwpriv eth0 mototm <x>" to en/disable TM feature
 * Motorola         22/09/2005      Change the algarithm to only wake up when consecutively 
 *                                  get two tx or rx packets within 500ms.
 * Motorola         13/10/2005      Added U-APSD related feature.
 * Motorola         25/05/2006      Change the WPRM_WLAN_HOST_WAKEUP_GAP from 80 to 20
 */

/*---------------------------------- Includes --------------------------------*/
/*----------------------------------- Macros ---------------------------------*/
/* Return valuse */
#define WPRM_RETURN_SUCCESS  (0)
#define WPRM_RETURN_FAILURE  (-1)

/* Traffic Meter */
#define TRAFFIC_METER_MEASUREMENT_INTERVAL      (500)    /* in millisecond. Traffic meter granularity */
#define TXRX_TRESHOLD_EXIT_PS       (4)    /* Threshold to exit low power mode */
#define TXRX_TRESHOLD_ENTER_PS      (1)    /* Threshold to enter low power mode */
#define TIMESTAMP_SIZE              (10)
/* Traffic meter command number */
#define WPRM_TM_CMD_NONE            (0)
#define WPRM_TM_CMD_ENTER_PS        (1)
#define WPRM_TM_CMD_EXIT_PS         (2)
#ifdef WMM_UAPSD
#define WPRM_TM_CMD_ENTER_UAPSD	    (3)
#endif

/* hostwakeupcfg params */
#define WPRM_HWUC_NON_UNICAST_MASK   (1 << 0)    /* bit0=1: non-unicast data */
#define WPRM_HWUC_UNICAST_MASK       (1 << 1)    /* bit1=1: unicast data */
#define WPRM_HWUC_MAC_EVENTS_MASK    (1 << 2)    /* bit2=1: mac events */
#define WPRM_HWUC_MAGIC_PACKET_MASK  (1 << 3)    /* bit3=1: magic packet */
#ifdef WMM_UAPSD
#define WPRM_HWUC_CANCEL_MASK   (0xffffffff)
#endif

#define WPRM_WLAN_HOST_WAKEB_GPIO_NO (1)         /* FW uses GPIO-1 to wakeup host */
#define WPRM_WLAN_HOST_WAKEUP_GAP    (20)        /* in ms. How far to trigger GPIO before sending an wakeup event */

#ifdef WPRM_DRV_TRACING
#define WPRM_DRV_TRACING_PRINT()    printk("WPRM TRACING: %s, %s:%i\n", __FUNCTION__, \
						__FILE__, __LINE__)
#else
#define WPRM_DRV_TRACING_PRINT()
#endif


/*---------------------------------- Structures & Typedef --------------------*/
typedef enum {
    ST_DATA_GENERAL = 0,
    ST_VOICE_20MS,
    ST_VOICE_40MS,
} WPRM_SESSION_TYPE_E;

typedef enum {
    SA_REGISTER = 0,
    SA_FREE,
} WPRM_SESSION_ACTION_E; 

typedef struct _WPRM_SESSION_SPEC_S {
    pid_t pid;
    int sid;
    int type;
    int act;
} WPRM_SESSION_SPEC_S; 

/*---------------------------------- Globals ---------------------------------*/
extern unsigned int wprm_tx_packet_cnt;       /* TX packet counter */
extern unsigned int wprm_rx_packet_cnt;       /* RX packet counter */
extern wlan_thread  tmThread;                 /* Thread to traffic meter */
extern unsigned int wprm_tm_ps_cmd_no;        /* Traffic meter command number */
extern BOOLEAN      wprm_tm_ps_cmd_in_list;   /* is Traffic meter command in list */
extern BOOLEAN      is_wprm_traffic_meter_timer_set;            /* traffic meter timer is set or not */
extern unsigned int voicesession_status;
extern unsigned int datasession_status;
extern BOOLEAN      wprm_snr_polling_back_to_quiet_required;    /* after SNR polling,
                                                                 * should put SDIO bus back to quiet
                                                                 */


extern int wlan_set_power(struct net_device *dev, struct iw_request_info *info, \
                struct iw_param *vwrq, char *extra);      /* change wlan power mode */
/*---------------------------------- APIs ------------------------------------*/
int wprm_wlan_host_wakeb_is_triggered(void);
int wprm_host_wlan_wakeb_is_triggered(void);
int wprm_trigger_host_wlan_wakeb(int tr);
/* WLAN_HOST_WAKEB interrupt handler */
void wprm_wlan_host_wakeb_handler(int irq, void *dev_id, struct pt_regs *regs);
int wprm_traffic_meter_init(wlan_private * priv);
int wprm_traffic_meter_exit(wlan_private * priv);
int wprm_traffic_meter_start(wlan_private * priv);
void wprm_traffic_meter_timer_handler(void *FunctionContext);
int wprm_traffic_meter_service_thread(void *data);
inline void wprm_traffic_measurement(wlan_private *priv, wlan_adapter *Adapter, BOOLEAN isTx);
inline BOOLEAN wlan_wprm_tm_is_enabled(void);
int wlan_wprm_tm_enable(BOOLEAN tmEnable);
int wprm_session_type_action(WPRM_SESSION_SPEC_S *session);
#endif

