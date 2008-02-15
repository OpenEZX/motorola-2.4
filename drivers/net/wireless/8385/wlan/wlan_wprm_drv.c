/* wlan_wprm_drv.c --- This file includes Power management APIs for WLAN driver
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
 * Motorola         05/09/2005      Fix error generated from klocwork findings 
 * Motorola         13/09/2005      Add "iwpriv eth0 mototm <x>" to en/disable TM feature
 * Motorola         22/09/2005      Change the algarithm to only wake up when consecutively 
 *                                  get two tx or rx packets within 500ms.
 * Motorola         13/10/2005      Added U-APSD related Feature
 * Motorola         13/10/2005      Enable Non-unicast packet receiving in IEEE power save mode
 * Motorola         20/10/2005      Add data session in Power mode switch.
 * Motorola         01/11/2005      Fix the bug of multiple WPRM_TM_CMD_EXIT_PS commands in Full Power mode.
 * Motorola         01/11/2005      Disable broadcast packets receiving in IEEE power save mode
 * Motorola         08/11/2005      Fix the bug of BG_scan failure when link loss detected.
 * Motorola         13/02/2006      Change exit_UAPSD commands sequence
 * Motorola         18/04/2006      Change the timestamp of traffic meter from jiffies to xtime
 * Motorola         25/05/2006      Bug fix in the linkloss related proccedure.
 */


/*---------------------------------- Includes --------------------------------*/
#include <linux/spinlock.h>
#include <asm/hardware.h>
#include <asm/arch/pxa-regs.h>
#include "include.h"

/*---------------------------------- Globals ---------------------------------*/
/* Traffic meter */
static BOOLEAN    wprm_tm_enabled = TRUE;       /* Traffic meter feature enabled or not */  
unsigned int      wprm_tx_packet_cnt = 0;       /* TX packet counter */
unsigned int      wprm_rx_packet_cnt = 0;       /* RX packet counter */
WLAN_DRV_TIMER    wprm_traffic_meter_timer;            /* Traffic meter timer */
BOOLEAN           is_wprm_traffic_meter_timer_set = FALSE;      /* is WPrM traffic meter timer set or not? */
spinlock_t        wprm_traffic_meter_timer_lock = SPIN_LOCK_UNLOCKED;       /* spin lock of access traffic_meter_timer */

unsigned int      wprm_tm_ps_cmd_no;            /* Traffic meter command number */
BOOLEAN           wprm_tm_ps_cmd_in_list;       /* is Traffic meter command in list */
spinlock_t        wprm_tm_ps_cmd_lock = SPIN_LOCK_UNLOCKED;         /* spin lock of ps_cmd_no */

wlan_thread       tmThread;                     /* Thread to traffic meter */

/* tx/rx traffic timestamp */
unsigned int wprm_tx_timestamp[TIMESTAMP_SIZE] = {0};
unsigned int wprm_tx_index = 0;
unsigned int wprm_rx_timestamp[TIMESTAMP_SIZE] = {0};
unsigned int wprm_rx_index = 0;
#ifdef WMM_UAPSD
unsigned int voicesession_status = 0;
unsigned int datasession_status = 0;
#endif
/*---------------------------------- APIs ------------------------------------*/

/* wprm_wlan_host_wakeb_is_triggered --- Check WLAN_HOST_WAKEB is triggered or not
 * Description:
 *    Check WLAN_HOST_WAKEB is triggered or not
 *       - GPIO_WLAN_HOST_WAKEB       INPUT     0: not triggered, Positive pulse (0-1-0): wakeup bulverde
 *                                              (This is applicable to Bartinique P1B GPIO-94 solution)
 * Parameters:
 *    None
 * Return Value:
 *    1 : triggered
 *    0 : not triggered
 */
int wprm_wlan_host_wakeb_is_triggered(void)
{
    WPRM_DRV_TRACING_PRINT();

    /* triggered */
    if(GPIO_is_high(GPIO_WLAN_HOST_WAKEB))
        return 1;
    /* not triggered */
    return 0;
}


/* wprm_host_wlan_wakeb_is_triggered --- Check HOST_WLAN_WAKEB is triggered or not
 * Description:
 *    Check HOST_WLAN_WAKEB is triggered or not
 *       - GPIO_HOST_WLAN_WAKEB       OUTPUT    1: not triggered, Negative pulse (1-0-1): wakeup WLAN
 * Parameters:
 *    None
 * Return Value:
 *    1 : triggered
 *    0 : not triggered
 */
int wprm_host_wlan_wakeb_is_triggered(void)
{
    WPRM_DRV_TRACING_PRINT();

    /* triggered */
    if(GPIO_is_high(GPIO_HOST_WLAN_WAKEB))
        return 0;
    /* not triggered */
    return 1;
}


/* wprm_trigger_host_wlan_wakeb --- Trigger HOST_WLAN_WAKEB
 * Description:
 *    Trigger HOST_WLAN_WAKEB to notify wlan chips that host is now wakeup.
 *      - GPIO_HOST_WLAN_WAKEB       OUTPUT    1: not triggered, Negative pulse (1-0-1): wakeup WLAN
 * Parameters:
 *    int tr: 
 *       1 - trigger
 *       0 - not trigger
 * Return Value:
 *    0 : success
 *    -1: fail
 */
int wprm_trigger_host_wlan_wakeb(int tr)
{
    WPRM_DRV_TRACING_PRINT();

    if(tr) {
        /* trigger HOST_WLAN_WAKEB */
        clr_GPIO(GPIO_HOST_WLAN_WAKEB);
    }
    else {
        /* do not trigger HOST_WLAN_WAKEB */
        set_GPIO(GPIO_HOST_WLAN_WAKEB);
    }

    return 0;
}

/* wprm_wlan_host_wakeb_handler -- WLAN_HOST_WAKEB interrupt handler 
 * Description:
 *     This callback is registered in wlan_add_card(), and works as WLAN_HOST_WAKEB 
 *     interrupt handler. When WLAN firmware wakes up Host(Bulverde) via WLAN_HOST_WAKEB
 *     falling edge, this callback is called to turn on SDIO_CLK. So, firmware can send SDIO
 *     event to Host through SDIO bus.
 *     Please refer to wlan_pm_callback() also.
 * Return:
 *     None.
 */
void wprm_wlan_host_wakeb_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int ret;
	wlan_private   *priv;

	printk("wprm_wlan_host_wakeb_handler: Enter.\n");
	priv = (wlan_private *)dev_id;

	ret = wlan_sdio_clock(priv, TRUE);
	if(ret != 0) {
		printk("wprm_wlan_host_wakeb_handler: turn on sdio clock error. %d. \n", ret);
	}
	printk("wprm_wlan_host_wakeb_handler: Exit.\n");

	return;
}

/* wprm_traffic_meter_init --- initialize traffic meter
 * Description:
 *    . init tx/rx packet counters, and
 *    . register traffic meter timer
 * Parameters:
 *    priv : wlan driver's priv data
 * Return Value:
 *    WPRM_RETURN_SUCCESS: succeed
 *    WPRM_RETURN_FAILURE: fail
 */
int wprm_traffic_meter_init(wlan_private * priv)
{

    WPRM_DRV_TRACING_PRINT();

    /* to avoid init again */
    if(is_wprm_traffic_meter_timer_set)
        return WPRM_RETURN_SUCCESS;

    /* init tx/rx packet counters */
    wprm_tx_packet_cnt = 0;       /* TX packet counter */
    wprm_rx_packet_cnt = 0;       /* RX packet counter */

    /* register traffic meter timer */
    InitializeTimer( &wprm_traffic_meter_timer, wprm_traffic_meter_timer_handler, priv);
    ModTimer( &wprm_traffic_meter_timer, TRAFFIC_METER_MEASUREMENT_INTERVAL);

    is_wprm_traffic_meter_timer_set = TRUE;

    return WPRM_RETURN_SUCCESS;
}

/* wprm_traffic_meter_exit --- de-init traffic meter 
 * Description:
 *    . delete traffic meter timer
 * Parameters:
 *    priv : wlan driver's priv data
 * Return Value:
 *    WPRM_RETURN_SUCCESS: succeed
 *    WPRM_RETURN_FAILURE: fail
 */
int wprm_traffic_meter_exit(wlan_private * priv)
{
    WPRM_DRV_TRACING_PRINT();

    /* to avoid exit twice */
    if(!is_wprm_traffic_meter_timer_set)
        return WPRM_RETURN_SUCCESS;

    /* delete traffic meter timer */
    CancelTimer(&wprm_traffic_meter_timer);
    FreeTimer(&wprm_traffic_meter_timer);

    is_wprm_traffic_meter_timer_set = FALSE;

    return WPRM_RETURN_SUCCESS;
}

/* wprm_traffic_meter_timer_handler --- traffic meter timer's handler
 * Description:
 *    Traffic meter timer handler. 
 *    In this handler, tx/rx packets in this time out period
 *     is accounted, and then make a decision about next PM state should be, then,
 *     issue a command in the queue, and wake up TM thread's waitQ. 
 * Parameters:
 *    FunctionContext : timer handler's private data 
 * Return Value:
 *    None
 */
void wprm_traffic_meter_timer_handler(void *FunctionContext)
{

    /* get the adapter context */
    wlan_private *priv = (wlan_private *)FunctionContext;
    wlan_adapter *Adapter = priv->adapter;

    WPRM_DRV_TRACING_PRINT();
    if (wprm_tm_ps_cmd_in_list!=FALSE)
		goto wprm_tm_exit_reinstall;
    printk("WPRM_TM: tx_cnt = %d. rx_cnt = %d.\n", wprm_tx_packet_cnt, wprm_rx_packet_cnt);
    if(datasession_status != 0 )
    {   /*Should Enter Active Mode*/
	if(Adapter->PSMode == Wlan802_11PowerModeCAM)
		goto wprm_tm_exit_reinstall;
        wprm_tm_ps_cmd_no = WPRM_TM_CMD_EXIT_PS;
        wprm_tm_ps_cmd_in_list = TRUE;
        wake_up_interruptible(&tmThread.waitQ);
        printk("WPRM_TM: after wake_up_interruptible() to ENTER FULL POWER with data session set.\n");
        WPRM_DRV_TRACING_PRINT();
        goto wprm_tm_exit_reinstall;
    }

#ifdef WMM_UAPSD
    else
    {
    /*Whether in UAPSD Mode*/
        if ((Adapter->CurBssParams.wmm_uapsd_enabled == TRUE)&& (Adapter->wmm.qosinfo != 0)) /**/
        {
            if((Adapter->PSMode != Wlan802_11PowerModeCAM)&&(Adapter->sleep_period.period!=0))
            {
    		if((voicesession_status != 0 ) /*&& (datasession_status == 0)*/)	
				goto wprm_tm_exit_reinstall;	/*Keep in UAPSD Mode*/
		if((wprm_tx_packet_cnt + wprm_rx_packet_cnt) >= TXRX_TRESHOLD_EXIT_PS) 
		{	/*Should Enter Active Mode*/
			wprm_tm_ps_cmd_no = WPRM_TM_CMD_EXIT_PS;
		       wprm_tm_ps_cmd_in_list = TRUE;
	              wake_up_interruptible(&tmThread.waitQ);
	              printk("WPRM_TM: after wake_up_interruptible() to ENTER FULL POWER.\n");

	              WPRM_DRV_TRACING_PRINT();
	              goto wprm_tm_exit_reinstall;
		}
		else		/*Should Enter PS Mode*/
		{
		      wprm_tm_ps_cmd_no = WPRM_TM_CMD_ENTER_PS;
		      wprm_tm_ps_cmd_in_list = TRUE;
	              wake_up_interruptible(&tmThread.waitQ);
	              printk("WPRM_TM: after wake_up_interruptible() to ENTER ps.\n");

	              WPRM_DRV_TRACING_PRINT();
	              goto wprm_tm_exit_no_install;
		}
            }
        }
    }
#endif

    /* whether in power save mode */
    if((Adapter->PSMode != Wlan802_11PowerModeCAM)&&(Adapter->sleep_period.period==0)) 
    { 

	/*If voice session is set, then goto UAPSD mode*/
        if(datasession_status != 0 )
        {       /*Should Enter Active Mode*/
              printk ("WPRM_TM: datasession_status = %d\n",datasession_status);
              wprm_tm_ps_cmd_no = WPRM_TM_CMD_EXIT_PS;
              wprm_tm_ps_cmd_in_list = TRUE;
              wake_up_interruptible(&tmThread.waitQ);
              printk("WPRM_TM: after wake_up_interruptible() to ENTER FULL POWER.\n");
              WPRM_DRV_TRACING_PRINT();
              goto wprm_tm_exit_reinstall;
        }


	#ifdef WMM_UAPSD
	else
	{
        	if ((Adapter->CurBssParams.wmm_uapsd_enabled == TRUE)	&& (Adapter->wmm.qosinfo != 0)) /**/
        	{
			if((voicesession_status != 0 ) /*&& (datasession_status == 0)*/)/*There is VOICE application running.*/
															/*go to UAPSD mode then*/			
			{
				wprm_tm_ps_cmd_no = WPRM_TM_CMD_ENTER_UAPSD;
				wprm_tm_ps_cmd_in_list = TRUE;
				wake_up_interruptible(&tmThread.waitQ);
				printk("WPRM_TM: after wake_up_interruptible() to enter UAPSD.\n");
	
				 /* need to reinstall traffic meter timer */
				WPRM_DRV_TRACING_PRINT();
				 goto wprm_tm_exit_reinstall;

			}
		}
	}						
	#endif
        if((wprm_tx_packet_cnt + wprm_rx_packet_cnt) >= TXRX_TRESHOLD_EXIT_PS) 
	{
            /* ENTER FULL POWER  mode */
            /* send command to traffic meter thread */
            wprm_tm_ps_cmd_no = WPRM_TM_CMD_EXIT_PS;
            wprm_tm_ps_cmd_in_list = TRUE;
            wake_up_interruptible(&tmThread.waitQ);
            printk("WPRM_TM: after wake_up_interruptible() to exit ps.\n");

            WPRM_DRV_TRACING_PRINT();
            goto wprm_tm_exit_reinstall;
        }
        else {	
            /* should keep in power save mode */
            goto wprm_tm_exit_no_install;
        }
    }

    /* whether in active (full power) mode */
    if(Adapter->PSMode == Wlan802_11PowerModeCAM)
   {

	/*If voice session is set, then goto UAPSD mode*/
        if(datasession_status != 0 )
        {       /*Should keep in  Active Mode*/
              printk("WPRM_TM: wprm_traffic_meter_handler(), datasession_status = %d.\n",datasession_status);
              printk("WPRM_TM: wprm_traffic_meter_handler(), keep in ACTIVE mode.\n");
              goto wprm_tm_exit_reinstall;
        }

	#ifdef WMM_UAPSD
	else
	{
		if ((Adapter->CurBssParams.wmm_uapsd_enabled == TRUE)	&& (Adapter->wmm.qosinfo != 0)) /**/		/*There is VOICE application running.*/
		{
			
			if(voicesession_status != 0 )
			{
			       printk("WPRM_TM: wprm_traffic_meter_timer_handler(), decide to enter UAPSD.\n");
				wprm_tm_ps_cmd_no = WPRM_TM_CMD_ENTER_UAPSD;
				wprm_tm_ps_cmd_in_list = TRUE;
				wake_up_interruptible(&tmThread.waitQ);
				printk("WPRM_TM: after wake_up_interruptible() to enter UAPSD.\n");
	
				 /*need to reinstall traffic meter timer */
				WPRM_DRV_TRACING_PRINT();
				 goto wprm_tm_exit_reinstall;

			}
		}
	}
	#endif

        if((wprm_tx_packet_cnt + wprm_rx_packet_cnt) <= TXRX_TRESHOLD_ENTER_PS) 
	{
	    printk("WPRM_TM: wprm_traffic_meter_timer_handler(), decide to enter PS.\n");
            /* enter power save mode */
            /* send command to traffic meter thread */
            wprm_tm_ps_cmd_no = WPRM_TM_CMD_ENTER_PS;
            wprm_tm_ps_cmd_in_list = TRUE;
            wake_up_interruptible(&tmThread.waitQ);
            printk("WPRM_TM: after wake_up_interruptible() to enter ps.\n");

            /* do not need to reinstall traffic meter timer */
            WPRM_DRV_TRACING_PRINT();
            goto wprm_tm_exit_no_install;
        }
        else {
            /* shoudl keep in active mode */
            goto wprm_tm_exit_reinstall;
        }
    }

    /* install traffic meter timer again */
wprm_tm_exit_reinstall:

    /* reset tx/rx packet counters */
    wprm_tx_packet_cnt = 0;       /* TX packet counter */
    wprm_rx_packet_cnt = 0;       /* RX packet counter */

    
    printk("WPRM_TM: before ModTimer()\n");
    ModTimer( &wprm_traffic_meter_timer, TRAFFIC_METER_MEASUREMENT_INTERVAL);
    printk("WPRM_TM: after ModTimer()\n");

    return;

    /* do not reinstall traffic meter timer */
wprm_tm_exit_no_install:
    /* reset tx/rx packet counters */
    wprm_tx_packet_cnt = 0;       /* TX packet counter */
    wprm_rx_packet_cnt = 0;       /* RX packet counter */

    /* delete timer */
    wprm_traffic_meter_exit(priv);

    return;
}

/* wprm_traffic_measurement --- WLAN traffic measurement
 * Description:
 *    Measure wlan traffic, 
 *       . to decide to exit low power mode, and 
 *       . to start traffic meter timer
 *    If traffic meter timer has already been started, this routine does nothing.
 * Parameters:
 *    priv   : wlan driver priv pointer
 *    Adapter: wlan driver Adapter pointer
 * Return Value:
 *    None
 */
inline void wprm_traffic_measurement(wlan_private *priv, wlan_adapter *Adapter, BOOLEAN isTx)
{
    WPRM_DRV_TRACING_PRINT();

    /* check automatic traffic meter is enabled or not */
    if(wlan_wprm_tm_is_enabled() == FALSE) {
       return;
    }
    
    if((is_wprm_traffic_meter_timer_set == FALSE) /* ||
        (Adapter->PSMode != Wlan802_11PowerModeCAM) ||
        Adapter->bHostWakeupDevRequired */ ) {

        WPRM_DRV_TRACING_PRINT();

        if(isTx == TRUE) {

            /* update tx packets timestamp */
            wprm_tx_timestamp[wprm_tx_index] = xtime.tv_usec/10000+(xtime.tv_sec&0xffff)*100;
            printk("TX: %d, %d.\n", wprm_tx_index, wprm_tx_timestamp[wprm_tx_index]);
            wprm_tx_index = (wprm_tx_index + 1) % TIMESTAMP_SIZE;
        } 
        else {
            /* update rx packets timestamp */
            wprm_rx_timestamp[wprm_rx_index] = xtime.tv_usec/10000+(xtime.tv_sec&0xffff)*100;
            printk("RX: %d, %d.\n", wprm_rx_index, wprm_rx_timestamp[wprm_rx_index]);
            wprm_rx_index = (wprm_rx_index + 1) % TIMESTAMP_SIZE;
        }
        if(datasession_status != 0 )
        {       /*Should Enter Active Mode*/
            wprm_tm_ps_cmd_no = WPRM_TM_CMD_EXIT_PS;
            wprm_tm_ps_cmd_in_list = TRUE;
            wake_up_interruptible(&tmThread.waitQ);
            printk("WPRM_TM: after wake_up_interruptible() to ENTER FULL POWER.\n");
        }
 
	/*If voice session is set, then goto UAPSD mode*/
	#ifdef WMM_UAPSD
	else
	{
		if ((Adapter->CurBssParams.wmm_uapsd_enabled == TRUE)&& (Adapter->wmm.qosinfo != 0)) /**/		/*There is VOICE application running.*/
		{
			
			if(voicesession_status != 0 )
			{
			       printk("WPRM_TM: wprm_traffic_meter_timer_handler(), decide to enter UAPSD.\n");
				wprm_tm_ps_cmd_no = WPRM_TM_CMD_ENTER_UAPSD;
				wprm_tm_ps_cmd_in_list = TRUE;
				wake_up_interruptible(&tmThread.waitQ);
				printk("WPRM_TM: after wake_up_interruptible() to enter UAPSD.\n");

			}
		}
	}
	#endif


        /* need to exit ps or not */
        if( /* tx packets measurement */
            ((wprm_tx_timestamp[(wprm_tx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE] - \
            wprm_tx_timestamp[(wprm_tx_index + TIMESTAMP_SIZE - 2) % TIMESTAMP_SIZE]) < \
            (TRAFFIC_METER_MEASUREMENT_INTERVAL /10)) && \
            /* rx packets measurement */ \
            ( (wprm_rx_timestamp[(wprm_rx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE] - \
            wprm_rx_timestamp[(wprm_rx_index + TIMESTAMP_SIZE - 2) % TIMESTAMP_SIZE]) < \
            (TRAFFIC_METER_MEASUREMENT_INTERVAL /10)) && \
            /* tx/rx correlation measurement */ \
            ( ((wprm_rx_timestamp[(wprm_rx_index + TIMESTAMP_SIZE - 1)  % TIMESTAMP_SIZE] > \
            wprm_tx_timestamp[(wprm_tx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE] ) ? \
            (wprm_rx_timestamp[(wprm_rx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE] - \
            wprm_tx_timestamp[(wprm_tx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE]) : \
            (wprm_tx_timestamp[(wprm_tx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE] - \
            wprm_rx_timestamp[(wprm_rx_index + TIMESTAMP_SIZE - 1) % TIMESTAMP_SIZE])) < \
            (TRAFFIC_METER_MEASUREMENT_INTERVAL /10)) ) {

            /* exit power save mode */
            /* send command to traffic meter thread */
            wprm_tm_ps_cmd_no = WPRM_TM_CMD_EXIT_PS;
            wprm_tm_ps_cmd_in_list = TRUE;
 
            wake_up_interruptible(&tmThread.waitQ);
            printk("WPRM_TM_MEASUREMENT: wprm_traffic_measurement(), to exit ps mode on TX/RX traffic high.\n");
        }
    }

    return;
}


/* wprm_traffic_meter_service_thread --- service thread for meter feature
 * Description:
 *    In this thread, PM commands in queue are sent to driver's main service thread and got executed.
 * Parameters:
 *    (void *)data: this thread
 * Return Value:
 *    0     : succeed
 *    other : fail
 */
int wprm_traffic_meter_service_thread(void *data)
{
    wlan_thread  *thread = data;
    wlan_private *priv = thread->priv;
    wait_queue_t wait;
    struct iw_param iwrq;
    HostCmd_DS_802_11_SLEEP_PERIOD	sleeppd;
    HostCmd_DS_802_11_HOST_WAKE_UP_CFG hwuc;

    WPRM_DRV_TRACING_PRINT();

    wlan_activate_thread(thread);
    init_waitqueue_entry(&wait, current);

    for(;;) {
        add_wait_queue(&thread->waitQ, &wait);
        OS_SET_THREAD_STATE(TASK_INTERRUPTIBLE);

        /* check any command need to be sent to wlan_main_service */
        if( !wprm_tm_ps_cmd_in_list ) { /* no command in list */
            schedule();
        }

        OS_SET_THREAD_STATE(TASK_RUNNING);
        remove_wait_queue(&thread->waitQ, &wait);

        /* To terminate thread */
        if(thread->state == WLAN_THREAD_STOPPED)
            break;

        if(wlan_wprm_tm_is_enabled() == FALSE) {
            /* 
             * automatic traffic meter feature is disabled.
             * stop TM timer.
             */
            wprm_traffic_meter_exit(priv);
            printk("WPRM_TM_THREAD: command DISCARDED, while TM disabled.\n");
        } 
        else
        if( wprm_tm_ps_cmd_no == WPRM_TM_CMD_EXIT_PS ) {
            WPRM_DRV_TRACING_PRINT();

#ifdef WMM_UAPSD
		if ((priv->adapter->CurBssParams.wmm_uapsd_enabled == TRUE)&& (priv->adapter->wmm.qosinfo != 0)) /**/		/*There is VOICE application running.*/
		{
			
			if(priv->adapter->sleep_period.period!=0 )
			{
	
			    /*assembly sleeppd command*/
			     /* To Exit periodic sleep*/
			     sleeppd.Action =  wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			     sleeppd.Period = 0;
			#ifdef DEEP_SLEEP
			     if ((priv->adapter->IsDeepSleep == TRUE))
			     {
				printk("<1>():%s IOCTLS called when station is"
					" in DeepSleep\n",__FUNCTION__);
			     }
			     else
			     {
			#endif
			     PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SLEEP_PERIOD,
					0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
					0, HostCmd_PENDING_ON_NONE, (void *) &sleeppd);
		            printk("WPRM_TM_THREAD: after send sleeppd cmd.\n");
			#ifdef DEEP_SLEEP
			     }
			#endif
			}
		}
#endif

            /* assemble "iwconfig eth0 power off" command request */
            iwrq.value = 0;
            iwrq.fixed = 0;
            iwrq.disabled = 1;  /* exit from low power mode */
            iwrq.flags = 0;
            /* send exit IEEE power save command */
            wlan_set_power(priv->wlan_dev.netdev, NULL, &iwrq, NULL);

            printk("WPRM_TM_THREAD: after send exit ps cmd.\n");

            /* once enter active mode, start tm timer */
           if(priv->adapter->MediaConnectStatus != WlanMediaStateConnected)
            {
                wprm_tm_ps_cmd_no = WPRM_TM_CMD_NONE;
                wprm_tm_ps_cmd_in_list = FALSE;
		wprm_traffic_meter_exit(priv);
                continue;
            }
            wprm_traffic_meter_init(priv);
            printk("WPRM_TM_THREAD: %s(), exit PS, and start TM timer.\n", __FUNCTION__);
        }
        else if( wprm_tm_ps_cmd_no == WPRM_TM_CMD_ENTER_PS ) {
            WPRM_DRV_TRACING_PRINT();
            if(priv->adapter->MediaConnectStatus != WlanMediaStateConnected)
            {
                wprm_tm_ps_cmd_no = WPRM_TM_CMD_NONE;
                wprm_tm_ps_cmd_in_list = FALSE;
		wprm_traffic_meter_exit(priv);
                continue;
            }
#ifdef WMM_UAPSD
		if ((priv->adapter->CurBssParams.wmm_uapsd_enabled == TRUE)&& (priv->adapter->wmm.qosinfo != 0)) /**/		/*There is VOICE application running.*/
		{
			
			if(priv->adapter->sleep_period.period!=0 )
			{
	
			    /*assembly sleeppd command*/
			     /*To Exit periodic sleep*/
			     sleeppd.Action =  wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
			     sleeppd.Period = 0;
                        #ifdef DEEP_SLEEP
                             if ((priv->adapter->IsDeepSleep == TRUE))
                             {
                                printk("<1>():%s IOCTLS called when station is"
                                        " in DeepSleep\n",__FUNCTION__);
                             }
                             else
                             {
                        #endif
			     PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SLEEP_PERIOD,
					0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
					0, HostCmd_PENDING_ON_NONE, (void *) &sleeppd);
		            printk("WPRM_TM_THREAD: after send sleeppd cmd.\n");
			#ifdef DEEP_SLEEP
			    }
			#endif
			}
		}
#endif
#if 1
        #ifdef DEEP_SLEEP
            if ((priv->adapter->IsDeepSleep == TRUE))
            {
                 printk("<1>():%s IOCTLS called when station is"
                                 " in DeepSleep\n",__FUNCTION__);
            }
            else
            {
        #endif
            /* assembly hostwakeupcfg command params */
            priv->adapter->HostWakeupCfgParam.conditions = /* WPRM_HWUC_NON_UNICAST_MASK | */ WPRM_HWUC_UNICAST_MASK;
            priv->adapter->HostWakeupCfgParam.gpio = WPRM_WLAN_HOST_WAKEB_GPIO_NO;
            priv->adapter->HostWakeupCfgParam.gap = WPRM_WLAN_HOST_WAKEUP_GAP;
            /* send hostwakeupcfg command to quiet SDIO bus */
            PrepareAndSendCommand(priv,
                HostCmd_CMD_802_11_HOST_WAKE_UP_CFG,
                0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
                0, HostCmd_PENDING_ON_NONE, (void *)&(priv->adapter->HostWakeupCfgParam));

            printk("WPRM_TM_THREAD: after send hostwakeupcfg.\n");
	#ifdef DEEP_SLEEP
	    }
	#endif
#endif
            /* assembly enter IEEE power save command request */
            iwrq.value = 0;
            iwrq.fixed = 0;
            iwrq.disabled = 0;  /* enter low power mode */
            iwrq.flags = 0;
            /* send enter IEEE power save command */
            wlan_set_power(priv->wlan_dev.netdev, NULL, &iwrq, NULL);
            printk("WPRM_TM_THREAD: after send enter ps cmd.\n");

            if(priv->adapter->MediaConnectStatus != WlanMediaStateConnected)
            {
                iwrq.disabled = 1;
                wlan_set_power(priv->wlan_dev.netdev, NULL, &iwrq, NULL);
                printk("WPRM_TM_THREAD: Linkloss detected, enter Full power mode.\n");
            }
            /* once enter PS mode, stop tm timer. After that, tm timer will only be triggered by 
             * traffic measurement or association 
             */
            wprm_traffic_meter_exit(priv);
            printk("WPRM_TM_THREAD: %s(), enter PS, and stop TM timer.\n", __FUNCTION__);
        }

	/*For the command to enter UAPSD Mode*/
#ifdef WMM_UAPSD
	else if( wprm_tm_ps_cmd_no == WPRM_TM_CMD_ENTER_UAPSD)
	{
            WPRM_DRV_TRACING_PRINT();
            if(priv->adapter->MediaConnectStatus != WlanMediaStateConnected)
            {
                wprm_tm_ps_cmd_no = WPRM_TM_CMD_NONE;
                wprm_tm_ps_cmd_in_list = FALSE;
		wprm_traffic_meter_exit(priv);
                continue;
            }
#if 1
        #ifdef DEEP_SLEEP
            if ((priv->adapter->IsDeepSleep == TRUE))
            {
                 printk("<1>():%s IOCTLS called when station is"
                                 " in DeepSleep\n",__FUNCTION__);
            }
            else
            {
        #endif

            /* assembly hostwakeupcfg command params */
            priv->adapter->HostWakeupCfgParam.conditions = WPRM_HWUC_CANCEL_MASK;
            priv->adapter->HostWakeupCfgParam.gpio = 0;
            priv->adapter->HostWakeupCfgParam.gap = 0;
            /* send hostwakeupcfg command to Cancel hostwakecfg */
            PrepareAndSendCommand(priv,
                HostCmd_CMD_802_11_HOST_WAKE_UP_CFG,
                0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
                0, HostCmd_PENDING_ON_NONE, (void *)&(priv->adapter->HostWakeupCfgParam));

            printk("WPRM_TM_THREAD: after send hostwakeupcfg.\n");
	#ifdef DEEP_SLEEP
	    }
	#endif
#endif
	    /*assembly sleeppd command*/
	     sleeppd.Action =  wlan_cpu_to_le16(HostCmd_ACT_GEN_SET);
	     sleeppd.Period = 20;
        #ifdef DEEP_SLEEP
            if ((priv->adapter->IsDeepSleep == TRUE))
            {
                 printk("<1>():%s IOCTLS called when station is"
                                 " in DeepSleep\n",__FUNCTION__);
            }
            else
            {
        #endif

	     PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SLEEP_PERIOD,
			0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
			0, HostCmd_PENDING_ON_NONE, (void *) &sleeppd);
            printk("WPRM_TM_THREAD: after send sleeppd cmd.\n");
        #ifdef DEEP_SLEEP
	    }
        #endif			
            /* assembly enter IEEE power save command request */
            iwrq.value = 0;
            iwrq.fixed = 0;
            iwrq.disabled = 0;  /* enter low power mode */
            iwrq.flags = 0;
            /* send enter IEEE power save command */
            wlan_set_power(priv->wlan_dev.netdev, NULL, &iwrq, NULL);
            printk("WPRM_TM_THREAD: after send enter ps cmd.\n");

            /* once enter APSD mode, start tm timer.  */
            wprm_traffic_meter_init(priv);
            printk("WPRM_TM_THREAD: %s(), enter PS, and stop TM timer.\n", __FUNCTION__);
            if(priv->adapter->MediaConnectStatus != WlanMediaStateConnected)
            {
                sleeppd.Period = 0;
	    #ifdef DEEP_SLEEP
                if ((priv->adapter->IsDeepSleep == TRUE))
	        {
        	     printk("<1>():%s IOCTLS called when station is"
                             " in DeepSleep\n",__FUNCTION__);
	        }
        	else
	        {
	    #endif

                PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SLEEP_PERIOD,
                        0, HostCmd_OPTION_USE_INT | HostCmd_OPTION_WAITFORRSP,
                        0, HostCmd_PENDING_ON_NONE, (void *) &sleeppd);
            #ifdef DEEP_SLEEP
		}
            #endif

                iwrq.disabled = 1;
                wlan_set_power(priv->wlan_dev.netdev, NULL, &iwrq, NULL);
                printk("WPRM_TM_THREAD: Linkloss detected, Enter full power mode.\n");
		wprm_traffic_meter_exit(priv);
            }	
	}
#endif
        /* restore */
        wprm_tm_ps_cmd_no = WPRM_TM_CMD_NONE;
        wprm_tm_ps_cmd_in_list = FALSE;
    }
    /* exit */
    wlan_deactivate_thread(thread);

    WPRM_DRV_TRACING_PRINT();
    return 0;
}

/* wlan_wprm_tm_is_enabled --- check wlan traffic meter feature is enabled or not
 * Description:
 *    check wlan traffic meter feature is enabled or not
 * Parameters:
 *    None
 * Return Value:
 *    TRUE  : enabled
 *    FALSE : disabled
 */
inline BOOLEAN wlan_wprm_tm_is_enabled()
{
    return wprm_tm_enabled;
}

/* wlan_wprm_tm_enable --- enable/disable wlan traffic meter feature 
 * Description:
 *    enable/disable wlan traffic meter feature
 * Parameters:
 *    tmEnable:
 *        TRUE : enable TM
 *        FALSE: disable TM
 * Return Value:
 *    1 : succeed
 *    0 : fail
 */
int wlan_wprm_tm_enable(BOOLEAN tmEnable)
{
    wprm_tm_enabled = tmEnable;
    return 1;
}

/* wprm_session_type_action --- Register session type
 * Description:
 *    register/free a session type. 
 *    . According to different session types currnetly registered, WPrM chooses different PM policy
 * Parameters:
 *    session:
 *        session pid, type, and action
 * Return Value:
 *    0      : succeed
 *    others : fail
 *    
 */
int wprm_session_type_action(WPRM_SESSION_SPEC_S *session)
{
    int ret = 0;

    /* debug */
    printk("WPRM: Add new session.\n");
    printk("session->pid = 0x%x, sid = %d, type = %d, action = %d.\n", \
           session->pid, session->sid, session->type, session->act);
	switch (session->type){
		
	case ST_DATA_GENERAL:

		if (SA_REGISTER == session->act)
			datasession_status = 1;
		else
			datasession_status = 0;
		break;
		
	case ST_VOICE_20MS:

		if (SA_REGISTER == session->act)
			voicesession_status = 1;
		else
			voicesession_status = 0;
		break;
	case ST_VOICE_40MS:

		if (SA_REGISTER == session->act)
			voicesession_status = 1;
		else
			voicesession_status = 0;
	
		break;
	}
			
    return ret;
}


