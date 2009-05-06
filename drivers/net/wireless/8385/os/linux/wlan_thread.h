#ifndef	__WLAN_THREAD_H_
#define	__WLAN_THREAD_H_

#include	<linux/smp_lock.h>

#define WLAN_THREAD_STOPPED	0
#define WLAN_THREAD_RUNNING	1

typedef struct {
	struct completion	sync;
	struct tq_struct	taskQ;
	wait_queue_head_t	waitQ;
	int			(*wlanfunc)(void *arg);
	void			*priv;
	int			state;
	pid_t			pid;
	char			name[16];
} wlan_thread;

static inline void wlan_activate_thread(wlan_thread *thr)
{
	/** Acquire kernel lock */
	lock_kernel();

	/** Record the thread pid */
	thr->pid = current->pid;
	
	/** Tell Signalling mechanism about the signals to be caught */
	siginitsetinv(&current->blocked, sigmask(SIGKILL) |
			sigmask(SIGINT)  | sigmask(SIGTERM));

	/** Initialize the wait queue */
	init_waitqueue_head(&thr->waitQ);
  
	/* Set the state to running */
	thr->state = WLAN_THREAD_RUNNING;
  
	/** Setup the name of the thread */
	memcpy(current->comm, thr->name, sizeof(current->comm));
  
	/** Release the kernel lock */
	unlock_kernel();
  
	/** Unlock the process waiting for this activation */
	complete(&thr->sync);
}

static inline void wlan_deactivate_thread(wlan_thread *thr)
{
	ENTER();

	/** Acquire kernel lock */
	lock_kernel();

	/** Reset the pid */
	thr->pid = 0;
	
	/** Wake the process waiting for de-activation */
	complete(&thr->sync);

	LEAVE();
}

static void wlan_register_thread(void *arg)
{
	wlan_thread	*thr = arg;
	
	/** Request the kernel to create a thread */
	kernel_thread(thr->wlanfunc, arg, 0);
}

static inline void wlan_create_thread(int (*wlanfunc)(void *), 
					wlan_thread *thr, char *name)
{
	/** Initialise wait mechanism */
	init_completion(&thr->sync);

	/** Setup the function to be run as the thread */
	thr->wlanfunc = wlanfunc;

	/** Record the thread name */
	memset(thr->name, 0, sizeof(thr->name));
	strncpy(thr->name, name, sizeof(thr->name)-1);
	
	/** Initialize the task queue */
	thr->taskQ.sync = 0;
	INIT_LIST_HEAD(&thr->taskQ.list);
	thr->taskQ.routine = wlan_register_thread;
	thr->taskQ.data = thr;
  
	/** Start the thread running */
	schedule_task(&thr->taskQ);

	/** Wait for thread to complete activation */
	wait_for_completion(&thr->sync);
}

static inline int wlan_terminate_thread(wlan_thread *thr)
{
	ENTER();

	/** Check if the thread is active or not */
	if (!thr->pid) {
		printk(KERN_DEBUG "Thread does not exist\n");
		return -1;
	}

	/** Acquire kernel lock */
	lock_kernel();

	/** Initialise wait mechanism */
	init_completion(&thr->sync);

	/** Reset the state to Stopped */
	thr->state = WLAN_THREAD_STOPPED;

	/** Remove the thread from the process table */
	kill_proc(thr->pid, SIGKILL, 1);

	/** Wait for thread to complete de-activation */
	wait_for_completion(&thr->sync);

	/** Release the kernel lock */
	unlock_kernel();

	/** Tell kernel to cleanup the thread which is in zombie state */
	kill_proc(2, SIGCHLD, 1);

	LEAVE();
	return 0;
}

#endif
