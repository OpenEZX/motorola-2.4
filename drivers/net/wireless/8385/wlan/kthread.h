/*	
 *	File: 	kthread.h
 *	
 *	frey@somewhere.ch
*/

#ifndef _KTHREAD_H_
#define _KTHREAD_H_

#ifndef THREADX
#include	<linux/config.h>
#include	<linux/version.h>
#ifdef	MODVERSIONS
#include	<linux/modversions.h>
#endif

#include	<linux/kernel.h>
#include	<linux/sched.h>
#include	<linux/tqueue.h>
#include	<linux/wait.h>
#include	<linux/signal.h>

#include	<asm/unistd.h>
#include	<asm/semaphore.h>
#include	<asm/smplock.h>
#else
//-jrs Re-implemented to use threadx threads.
#include "list.h"   // needed for CONTAINING_RECORD macro
#include "tx_api.h"
#include "MfpSystem.h"

#define KTHREAD_TIMESLICE   20
//#define KTHREAD_PRIORITY    THR_PRI_LOWEST
#define KTHREAD_PRIORITY    THR_PRI_NORMAL
#define KTHREAD_STACKSIZE   4096    // REVISIT: tune this value
#define KTHREAD_WAIT_FLAG   0x00000001
#endif

/** 
 * A structure to store all information we needfor our thread.
 */
typedef struct kthread_struct
{
	/* private data */
  
#ifndef THREADX
	/* Linux task structure of thread */
	struct task_struct	*thread;
	/* Task queue need to launch thread */
	struct tq_struct	tq;
	/* function to be started as thread */
	void (*function)(struct kthread_struct *kthread);
	/* semaphore needed on start and creation of thread. */
	struct semaphore startstop_sem;
  
	/* public data */
  
	/**
	* queue thread is waiting on. Gets initialized by
	* init_kthread, can be used by thread itself.
	*/
	wait_queue_head_t	queue;
	/**
	* flag to tell thread whether to die or not.
	* When the thread receives a signal, it must check
	* the value of terminate and call exit_kthread and terminate
	* if set.
	*/
	int	terminate;
	/* additional data to pass to kernel thread */
	void	*arg;
#else
    TX_THREAD txThread;

    TX_SEMAPHORE startstop_sem;

    TX_EVENT_FLAGS_GROUP events;

    ULONG terminate;
    ULONG arg;
    BYTE threadStack[KTHREAD_STACKSIZE] ALIGN4;

    unsigned char *queue;
#endif
}
#ifndef THREADX
kthread_t;
#else
kthread_t ALIGN4;
#endif

#ifndef THREADX
/**
 * Launches a new thread.
 */
static inline void kthread_launcher(void *data)
{
	kthread_t *kthread = data;

	kernel_thread((int (*)(void *))kthread->function, (void *) kthread, 0);
}
#endif

/**
 * Create a new kernel thread. Called by the creator.
 */
#ifndef THREADX
static inline void start_kthread(void (*func)(kthread_t *), kthread_t *kthread)
#else
static inline void start_kthread(void (*func)(ULONG), kthread_t *kthread, char* name)
#endif
{
#ifndef THREADX
	/**
	* initialize the semaphore:
	* we start with the semaphore locked. The new kernel
	* thread will setup its stuff and unlock it. This
	* control flow (the one that creates the thread) blocks
	* in the down operation below until the thread has reached
	* the up() operation.
	*/

	init_MUTEX_LOCKED(&kthread->startstop_sem);
  
	/*
	store the function to be executed in the data passed to the
	launcher
	*/
	kthread->function = func;
  
	/* create the new thread by running a task through keventd */
  
	/* initialize the task queue structure */
	kthread->tq.sync = 0;
	INIT_LIST_HEAD(&kthread->tq.list);
	kthread->tq.routine = kthread_launcher;
	kthread->tq.data = kthread;
  
	/* and schedule it for execution */
	schedule_task(&kthread->tq);
  
	/* wait till it has reached the setup_thread routine */
	down(&kthread->startstop_sem);             
#else
    // create binary semaphore with inital count of 1
    tx_semaphore_create(&kthread->startstop_sem, name, 1);

    // immediately decrement the semaphore; when the thread starts it
    // will increment the semaphore after it initializes itself.  This
    // is done to allow the new thread to run before the creating thread continues
    tx_semaphore_get(&kthread->startstop_sem, TX_WAIT_FOREVER);

    // initialize the thread's event flags
    tx_event_flags_create(&kthread->events, name);

    // create and start the thread
    tx_thread_create(&kthread->txThread, name, func, (ULONG)kthread, kthread->threadStack, KTHREAD_STACKSIZE, KTHREAD_PRIORITY, KTHREAD_PRIORITY, KTHREAD_TIMESLICE, TX_AUTO_START);

    // wait for it to start and init
    tx_semaphore_get(&kthread->startstop_sem, TX_WAIT_FOREVER);
#endif
}

/**
 * Stop a kernel thread. Called by the removing instance.
 */
static inline void stop_kthread(kthread_t *kthread)
{
#ifndef THREADX
	if (kthread->thread == NULL) {
		printk("stop_kthread: killing non existing thread!\n");
		return;
	}
  
	/**
	* this function needs to be protected with the big
	* kernel lock (lock_kernel()). The lock must be
	* grabbed before changing the terminate
	* flag and released after the down() call.
	*/
	lock_kernel();
        
	/**
	* initialize the semaphore. We lock it here, the
	* leave_thread call of the thread to be terminated
	* will unlock it. As soon as we see the semaphore
	* unlocked, we know that the thread has exited.
	*/
	init_MUTEX_LOCKED(&kthread->startstop_sem);

	/**
	* We need to do a memory barrier here to be sure that
	* the flags are visible on all CPUs. 
	*/
	mb();

	/* set flag to request thread termination */
	kthread->terminate = 1;
  
	/**
	* We need to do a memory barrier here to be sure that
	* the flags are visible on all CPUs. 
	*/
	mb();

	kill_proc(kthread->thread->pid, SIGKILL, 1);
  
	/* block till thread terminated */
	down(&kthread->startstop_sem);
  
	/* release the big kernel lock */
	unlock_kernel();
  
	/**
	* now we are sure the thread is in zombie state. We
	* notify keventd to clean the process up.
	*/
	kill_proc(2, SIGCHLD, 1);
#else
    // REVISIT: should we lock here???  Disable ints??? probably not...
    kthread->terminate = 1;

    // wake the thread in case it's sleeping
    tx_event_flags_set(&kthread->events, KTHREAD_WAIT_FLAG, TX_OR);

    // wait for thread to die; semaphore will be 0 if thread has not exited because
    // start_kthread routines leaves it at 0.
    tx_semaphore_get(&kthread->startstop_sem, TX_WAIT_FOREVER);

    tx_thread_delete(&kthread->txThread);
    tx_event_flags_delete(&kthread->events);
    tx_semaphore_delete(&kthread->startstop_sem);

#endif
}

/**
 * Initialize new created thread. Called by the new thread.
 */
static inline void init_kthread(kthread_t *kthread, char *name)
{
#ifndef THREADX
	/**
	* lock the kernel. A new kernel thread starts without
	* the big kernel lock, regardless of the lock state
	* of the creator (the lock level is *not* inheritated)
	*/
	lock_kernel();

	/* fill in thread structure */
	kthread->thread = current;

	/* set signal mask to what we want to respond */
	siginitsetinv(&current->blocked, sigmask(SIGKILL) |
			sigmask(SIGINT)  | sigmask(SIGTERM));

	/* initialise wait queue */
	init_waitqueue_head(&kthread->queue);
  
	/* initialise termination flag */
	kthread->terminate = 0;
  
	/* set name of this process (max 15 chars + 0 !) */
	sprintf(current->comm, name);
  
	/* let others run */
	unlock_kernel();
  
	/* tell the creator that we are ready and let him continue */
	up(&kthread->startstop_sem);
#else
    kthread->terminate = 0;
    tx_semaphore_put(&kthread->startstop_sem);
#endif
}

/**
 * Cleanup of thread. Called by the exiting thread.
 */
static inline void exit_kthread(kthread_t *kthread)
{
#ifndef THREADX
	/* we are terminating */
  
	/* lock the kernel, the exit will unlock it */
	lock_kernel();
	kthread->thread = NULL;
	mb();
  
	/* notify the stop_kthread() routine that we are terminating. */
	up(&kthread->startstop_sem);
	/* the kernel_thread that called clone() does a do_exit here. */
  
	/**
	* There is no race here between execution of the "killer" and real
	* termination of the thread (race window between up and do_exit), since
	* both the thread and the "killer" function are running with the kernel
	* lock held.
	* The kernel lock will be freed after the thread exited, so the code
	* is really not executed anymore as soon as the unload functions gets
	* the kernel lock back.
	* The init process may not have made the cleanup of the process here,
	* but the cleanup can be done safely with the module unloaded.
	*/
#else
    tx_semaphore_put(&kthread->startstop_sem);
#endif
}

#ifdef THREADX
static inline kthread_t* get_current_kthread()
{
    // TODO -jrs
}
#endif

#endif /* _KTHREAD_H_ */

