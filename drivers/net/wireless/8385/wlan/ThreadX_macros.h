#ifndef	_THREADX_MACROS_H
#define _THREADX_MACROS_H

//#define	PRINTK(x...)	dprintf(x)
#define	printk(x...)		dprintf(x)

#define kfree_skb(x)	//define it to nothing
#define wireless_send_event(w,x,y,z)
#define HZ	100	//check if this value is alright
#define kmalloc(x, y) agMalloc(x, SCRATCHALLOC)
#define udelay(x)	Delay(x)
#define mdelay(x);	{	\
						if((x)<10)	\
							tx_thread_sleep(x);	\
						else	\
							tx_thread_sleep((x)/10);	\
					}

#define init_etherdev(x, y) agMalloc(sizeof(struct net_device), SCRATCHALLOC)

//TODO: after the allocation of dev structure, use memset to set it to zero as below:
//memset(dev, 0, sizeof(net_device));

#define module_init(x)
#define module_exit(x)

#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define MODULE_AUTHOR(x)

#define SET_MODULE_OWNER(x)		/* define it to nothing */	
#define MOD_INC_USE_COUNT		/* define it to nothing */		
#define MOD_DEC_USE_COUNT		/* define it to nothing */		





#define init_waitqueue_head(x);		{	\
									if((x) == (&priv->adapter->scan_q)) \
										tx_event_flags_create(x, "scan_q"); \
									else if((x) == (&priv->adapter->association_q)) \
										tx_event_flags_create(x, "association_q"); \
					}


#define start_kthread(x, y);	{ 	\
									if((y) == (&priv->intThread)) \
										start_kthread(wlan_service_main_thread, &priv->intThread, "wlan_service");	\
									else if((y) == (&priv->eventThread)) \
										start_kthread(wlan_reassociation_thread, &priv->eventThread, "wlan_event"); \
								}	

#define unregister_netdev(x);	{	agFree(x); \
									x = NULL; \
								}
												
#define kfree	agFree
#define wake_up_interruptible(x)	tx_event_flags_set(x, KTHREAD_WAIT_FLAG, TX_OR)
#define interruptible_sleep_on_timeout(x, y)	tx_event_flags_get(x, KTHREAD_WAIT_FLAG, TX_OR_CLEAR, &flags, 1000)
#define interruptible_sleep_on(x)	tx_event_flags_get(x, KTHREAD_WAIT_FLAG, TX_OR_CLEAR, &flags, TX_WAIT_FOREVER)

#define schedule()	tx_event_flags_get(&kthread->events, KTHREAD_WAIT_FLAG, TX_AND_CLEAR, &flags, TX_WAIT_FOREVER)
#define spin_lock_irqsave(x, y)	tx_semaphore_get(x, TX_WAIT_FOREVER)
#define spin_unlock_irqrestore(x, y)	tx_semaphore_put(x)
#define netif_wake_queue(x)			/*define it to nothing */
#define netif_carrier_on(x)			/*define it to nothing */
#define netif_start_queue(x)		/*define it to nothing */
#define netif_stop_queue(x)			/*define it to nothing */
#define netif_carrier_off(x)

#define wlan_sched_timeout(x)	tx_thread_sleep((x)/10)
#define cli();			TX_DISABLE;
#define sti();			TX_RESTORE;

#define KERN_ERR
#define KERN_NOTICE

//#define init_waitqueue_entry(x,y);		/*define it to nothing */
/*
struct semaphore {
};


void sema_init(struct semaphore *sem, int val) {
}
*/
#define wait_queue_t	int
unsigned long current;
#define TASK_INTERRUPTIBLE	0
#define TASK_RUNNING	0

#define init_waitqueue_entry(x,y)
#define add_wait_queue(x,y)
#define set_current_state(x)
#define remove_wait_queue(x,y)
#define netif_queue_stopped(x)	0

//unsigned long jiffies = 0;

/*
void inline init_waitqueue_entry() {
}

void inline add_wait_queue() {
}

void inline set_current_state() {
}

void inline remove_wait_queue() {
}

int inline netif_queue_stopped() {
	return 0;
}
*/


#endif	//_THREADX_MACROS_H
