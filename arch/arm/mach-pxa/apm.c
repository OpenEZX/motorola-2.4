/*
 * bios-less APM driver for ARM Linux 
 *  Jamey Hicks <jamey@crl.dec.com>
 *  adapted from the APM BIOS driver for Linux by Stephen Rothwell (sfr@linuxcare.com)
 *
 * APM 1.2 Reference:
 *   Intel Corporation, Microsoft Corporation. Advanced Power Management
 *   (APM) BIOS Interface Specification, Revision 1.2, February 1996.
 *
 * [This document is available from Microsoft at:
 *    http://www.microsoft.com/hwdev/busbios/amp_12.htm]
 *
 *  11/11/2002, zxf
 *	- correct memory free error
 *	- porting to ezx platform
 *
 *  05/12/2003, zxf
 *	- make apm device can only be accessed by one user
 *	- correct timer operation sequence
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/apm_bios.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/hardware.h>

#ifdef CONFIG_SA1100_H3XXX
#include <asm/arch/h3600_hal.h>
#endif

#include <asm/arch/irqs.h>
extern void disable_irq(unsigned int irq);
extern void enable_irq(unsigned int irq);

#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
extern int (*console_blank_hook)(int);
#endif

struct apm_bios_info apm_bios_info = {
        /* this driver simulates APM version 1.2 */
        version: 0x102,
        flags: APM_32_BIT_SUPPORT
};

/*
 * The apm_bios device is one of the misc char devices.
 * This is its minor number.
 */
#define	APM_MINOR_DEV	134

/*
 * See Documentation/Config.help for the configuration options.
 *
 * Various options can be changed at boot time as follows:
 * (We allow underscores for compatibility with the modules code)
 *	apm=on/off			enable/disable APM
 *	    [no-]debug			log some debugging messages
 *	    [no-]power[-_]off		power off on shutdown
 */

/*
 * Need to poll the APM BIOS every second
 */
#define APM_CHECK_TIMEOUT	(HZ)

/*
 * Ignore suspend events for this amount of time after a resume
 */
#define DEFAULT_BOUNCE_INTERVAL		(3 * HZ)

/*
 * Timeout to wait for BP signal when power off
 */
#define POWER_OFF_TIMEOUT	(2*60*HZ)

/*
 * The magic number in apm_user
 */
#define APM_BIOS_MAGIC		0x4101

/*
 * Local variables
 */
//static int			suspends_pending;
//static int			standbys_pending;
//static int			ignore_normal_resume;

#ifdef CONFIG_APM_RTC_IS_GMT
#	define	clock_cmos_diff	0
#	define	got_clock_diff	1
#else
//static long			clock_cmos_diff;
//static int			got_clock_diff;
#endif
static int			debug;
static int			apm_disabled;
#ifdef CONFIG_SMP
static int			power_off;
#else
static int			power_off = 1;
#endif
static int			exit_kapmd;
static int			kapmd_running;

/* 
 * by zxf, 11/11/2002
 * for timeout to idle and sleep
 */
unsigned long idle_limit = 0;
unsigned long sleep_limit = 0;
int can_idle = 0;
int can_sleep = 0;
static int user_off_available = 0;

extern int pm_do_suspend(void);
extern int pm_do_useroff(void);
extern void pcap_switch_off_usb(void);
extern void pcap_switch_on_usb(void);
extern void ssp_pcap_clear_int(void);

static DECLARE_WAIT_QUEUE_HEAD(apm_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(apm_suspend_waitqueue);
static struct apm_user *	user_list = NULL;

static char			driver_version[] = "1.13";	/* no spaces */

typedef struct lookup_t {
	int	key;
	char *	msg;
} lookup_t;

static const lookup_t error_table[] = {
/* N/A	{ APM_SUCCESS,		"Operation succeeded" }, */
	{ APM_DISABLED,		"Power management disabled" },
	{ APM_CONNECTED,	"Real mode interface already connected" },
	{ APM_NOT_CONNECTED,	"Interface not connected" },
	{ APM_16_CONNECTED,	"16 bit interface already connected" },
/* N/A	{ APM_16_UNSUPPORTED,	"16 bit interface not supported" }, */
	{ APM_32_CONNECTED,	"32 bit interface already connected" },
	{ APM_32_UNSUPPORTED,	"32 bit interface not supported" },
	{ APM_BAD_DEVICE,	"Unrecognized device ID" },
	{ APM_BAD_PARAM,	"Parameter out of range" },
	{ APM_NOT_ENGAGED,	"Interface not engaged" },
	{ APM_BAD_FUNCTION,     "Function not supported" },
	{ APM_RESUME_DISABLED,	"Resume timer disabled" },
	{ APM_BAD_STATE,	"Unable to enter requested state" },
/* N/A	{ APM_NO_EVENTS,	"No events pending" }, */
	{ APM_NO_ERROR,		"BIOS did not set a return code" },
	{ APM_NOT_PRESENT,	"No APM present" }
};
#define ERROR_COUNT	(sizeof(error_table)/sizeof(lookup_t))

static int apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{
        //platform_apm_get_power_status(ac_line_status, battery_status, battery_flag, battery_percentage, battery_life);
	return APM_SUCCESS;
}

static int queue_empty(struct apm_user *as)
{
	return as->event_head == as->event_tail;
}

static apm_event_t get_queued_event(struct apm_user *as)
{
	as->event_tail = (as->event_tail + 1) % APM_MAX_EVENTS;
	return as->events[as->event_tail];
}

static int check_apm_user(struct apm_user *as, const char *func)
{
	if ((as == NULL) || (as->magic != APM_BIOS_MAGIC)) {
		printk(KERN_ERR "apm: %s passed bad filp\n", func);
		return 1;
	}
	return 0;
}

/*
 * by zxf, 11/11/2002 
 * It can not be static now in order to be referred outside of apm.c
 */
void queue_apm_event(apm_event_t event, struct apm_user *sender)
{
	struct apm_user *	as;

	if (user_list == NULL)
		return;
	for (as = user_list; as != NULL; as = as->next) {
		if (as == sender)
			continue;
		as->event_head = (as->event_head + 1) % APM_MAX_EVENTS;
		if (as->event_head == as->event_tail) {
			static int notified;

			if (notified++ == 0)
			    printk(KERN_ERR "apm: an event queue overflowed\n");
			as->event_tail = (as->event_tail + 1) % APM_MAX_EVENTS;
		}
		as->events[as->event_head] = event;
		if (!as->suser)
			continue;
		switch (event) {
		case APM_SYS_SUSPEND:
		case APM_USER_SUSPEND:
			as->suspends_pending++;
			//suspends_pending++;
			break;

		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			as->standbys_pending++;
			//standbys_pending++;
			break;
		}
	}
	wake_up_interruptible(&apm_waitqueue);
}

#include <linux/interrupt.h>
#include <asm/mach/irq.h>
/* this function is used after exiting sleep */
static void check_wakeup(void)
{
	int i;
	struct irqaction *action;
	struct irqdesc *desc;
	unsigned long source = PEDR & 0xffff;

	cli();
	APM_DPRINTK("check event: PEDR = 0x%x\n", PEDR);

	if (source == 0) {
		desc = irq_desc + IRQ_RTCAlrm;
		action = desc->action;

		if (action)
			action->handler(IRQ_RTCAlrm, action->dev_id, NULL);
	}

	for (i = 0; i < 15; i++) {
		if (source & (1 << i)) {
			if (i == PEDR_INT_SEC) {
				/* here should clear pcap int */
				ssp_pcap_clear_int();
				queue_apm_event(KRNL_TOUCHSCREEN, NULL);

			} else {
				desc = irq_desc + IRQ_GPIO(i);
				action = desc->action;

				if (action)
					action->handler(IRQ_GPIO(i), action->dev_id, NULL);
			}
		}
	}

	sti();
}

void pm_do_poweroff()
{
	unsigned long start = jiffies;

	*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = NO_FLAG;
	do {
		if (!(GPLR(GPIO_BB_WDI) & GPIO_bit(GPIO_BB_WDI))) {
			APM_DPRINTK("got BB_WDI signal when power down\n");
			cli();
			*(unsigned long *)(phys_to_virt(BPSIG_ADDR)) = WDI_FLAG;

			pcap_switch_off_usb();
			mdelay(1000);
			pcap_switch_on_usb();

			GPCR(GPIO_BB_RESET) = GPIO_bit(GPIO_BB_RESET);
			mdelay(1);
			GPSR(GPIO_BB_RESET) = GPIO_bit(GPIO_BB_RESET);

			/* Initialize the watchdog and let it fire */
			OWER = OWER_WME;
			OSSR = OSSR_M3;
			OSMR3 = OSCR + 36864;	/* ... in 10 ms */

			MDREFR |= MDREFR_SLFRSH;
			while(1);
		}

		if (!(GPLR(GPIO_BB_WDI2) & GPIO_bit(GPIO_BB_WDI2))) {
			APM_DPRINTK("got BB_WDI2 signal when power down\n");
			cli();
			if (user_off_available) {
				APM_DPRINTK("enter useroff\n");
				break;
			} else {
				GPCR(GPIO_WDI_AP) = GPIO_bit(GPIO_WDI_AP);
				while(1);
			}
		}

		if ((jiffies - start) >= POWER_OFF_TIMEOUT) {
			APM_DPRINTK("timeout when power down\n");
			GPCR(GPIO_WDI_AP) = GPIO_bit(GPIO_WDI_AP);
			while(1);
		}
	} while(1);
					
	pm_do_useroff();
	APM_DPRINTK("resume from useroff\n");
}

static ssize_t do_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
	struct apm_user *	as;
	int			i;
	apm_event_t		event;
	DECLARE_WAITQUEUE(wait, current);

	as = fp->private_data;
	if (check_apm_user(as, "read"))
		return -EIO;
	if (count < sizeof(apm_event_t))
		return -EINVAL;
	if (queue_empty(as)) {
		if (fp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		add_wait_queue(&apm_waitqueue, &wait);

repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (queue_empty(as) && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&apm_waitqueue, &wait);
	}
	i = count;
	while ((i >= sizeof(event)) && !queue_empty(as)) {
		event = get_queued_event(as);

		if (copy_to_user(buf, &event, sizeof(event))) {
			if (i < count)
				break;
			return -EFAULT;
		}
		switch (event) {
		case APM_SYS_SUSPEND:
		case APM_USER_SUSPEND:
			as->suspends_read++;
			break;

		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			as->standbys_read++;
			break;
		}
		buf += sizeof(event);
		i -= sizeof(event);
	}
	if (i < count)
		return count - i;
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

static unsigned int do_poll(struct file *fp, poll_table * wait)
{
	struct apm_user * as;

	as = fp->private_data;
	if (check_apm_user(as, "poll"))
		return 0;
	poll_wait(fp, &apm_waitqueue, wait);
	if (!queue_empty(as))
		return POLLIN | POLLRDNORM;
	return 0;
}

extern void ssp_pcap_intoSleepCallBack(void);
extern void ssp_pcap_wakeUpCallBack(void);
static struct timer_list idle_sleep_timer;

static void idle_sleep_handler(unsigned long data)
{
	APM_DPRINTK("pm isr: idle timeout\n");
	/* if pm sleep timer expired, queue event and enable OS timer0 */
	queue_apm_event(KRNL_IDLE_TIMEOUT, NULL);
	can_sleep = 1;
}

static int last_MCU_INT_SW = 0;
int ret_MCU_INT_SW()
{
	return last_MCU_INT_SW;
}

static int do_ioctl(struct inode * inode, struct file *filp,
		    u_int cmd, u_long arg)
{
	struct apm_user *	as;
	struct pm_dev *		pm;

	/*
	 * by zxf, 11/11/2002
	 * for correct time after idle
	 */
	static unsigned long oscr_before_idle, oscr_after_idle;
	static unsigned long osmr0_before_idle;
	static unsigned long div, left;

	as = filp->private_data;
	if (check_apm_user(as, "ioctl"))
		return -EIO;
	if (!as->suser)
		return - EPERM;
	switch (cmd) {
        case APM_IOC_SUSPEND:
		pm_do_suspend();
		break;
        case APM_IOC_SET_WAKEUP:
		if ((pm = pm_find((pm_dev_t)arg,NULL)) == NULL)
			return -EINVAL;
		pm_send(pm,PM_SET_WAKEUP,NULL);
		break;

	/*
	 * by zxf, 11/11/2002
	 * for enter idle and sleep
	 */
	case APM_IOC_IDLE:
		/* disable OS timer0 to prevent from waking up idle */
		OIER &= ~OIER_E0;		
		disable_irq(IRQ_OST0);

		/* start sleep timer */
            
	        idle_sleep_timer.function = idle_sleep_handler;
	        idle_sleep_timer.expires = sleep_limit * LATCH;
                kernel_OSCR2_setup(&idle_sleep_timer);

		/*
		 * save OS timer0 registers before idle
		 */
		osmr0_before_idle = OSMR0;
		APM_DPRINTK("do_ioctl: before idle(xtime): %lu\n", xtime.tv_sec);

		/* the second chance whether to idle or not */
		APM_DPRINTK("do_ioctl: can idle %d\n", can_idle);
		if (can_idle || arg)
			cpu_do_idle(0);			/* idle */

                kernel_OSCR2_delete(&idle_sleep_timer);
		/*
		 * after exit from idle, update xtime structure
		 * if idle too long, register overflow may cause fault time
		 */
		oscr_before_idle = osmr0_before_idle - LATCH;
		oscr_after_idle = OSCR;
		div = (oscr_after_idle - oscr_before_idle) / LATCH;
		left = (oscr_after_idle - oscr_before_idle) % LATCH;

		APM_DPRINTK("do_ioctl: after idle(xtime): %lu\n", xtime.tv_sec);

		if (div / HZ)			
			xtime.tv_sec += div / HZ;
		else
			xtime.tv_usec += div * (1000000/HZ);

		APM_DPRINTK("do_ioctl: after correct(xtime): %lu\n", xtime.tv_sec);

		/*
		 * refill OSMR0 register, reenable OS timer0
		 */
		OSMR0 = LATCH - left + OSCR;	//oscr_after_idle;
		OIER |= OIER_E0;	

		
		enable_irq(IRQ_OST0);
		break;
	case APM_IOC_SLEEP:
		/* the second chance whether to sleep or not */
		APM_DPRINTK("do_ioctl: can sleep %d\n", can_sleep);
		if (can_idle || can_sleep || arg) {

			if (pm_send_all(PM_SUSPEND, (void *)3) == 0) {
                        ssp_pcap_intoSleepCallBack();
				pm_do_suspend();
				APM_DPRINTK("do_ioctl: resume\n");
                        ssp_pcap_wakeUpCallBack();
				pm_send_all(PM_RESUME, (void *)0);
				check_wakeup();
			}
		}
		break;
	case APM_IOC_SET_IDLETIMER:
		idle_limit = arg * HZ;
		APM_DPRINTK("do_ioctl: idle timeout %ld\n", arg);
		break;
	case APM_IOC_SET_SLEEPTIMER:
		sleep_limit = arg * HZ;
		APM_DPRINTK("do_ioctl: sleep timeout %ld\n", arg);
		break;
	case APM_IOC_WAKEUP_ENABLE:
		PWER |= arg;
		APM_DPRINTK("do_ioctl: enable wakeup source:PWER=0x%x\n", PWER);
		break;
	case APM_IOC_WAKEUP_DISABLE:
		PWER &= ~arg;
		APM_DPRINTK("do_ioctl: disable wakeup source:PWER=0x%x\n", PWER);
		break;
	case APM_IOC_POWEROFF:
		APM_DPRINTK("do_ioctl: do power off\n");
		/* here all device should response ok */
		pm_send_all(PM_SUSPEND, (void *)3);
//                ssp_pcap_intoSleepCallBack();
		pm_do_poweroff();
//                ssp_pcap_wakeUpCallBack();
		pm_send_all(PM_RESUME, (void *)0);
		break;
	case APM_IOC_RESET_BP:
		APM_DPRINTK("do_ioctl: reset bp\n");
		GPCR(GPIO_BB_RESET) = GPIO_bit(GPIO_BB_RESET);
		mdelay(1);
		GPSR(GPIO_BB_RESET) = GPIO_bit(GPIO_BB_RESET);
		break;
	case APM_IOC_USEROFF_ENABLE:
		APM_DPRINTK("do_ioctl: useroff support enable\n");
		user_off_available = (int)arg;
		break;
	case APM_IOC_NOTIFY_BP:
		switch (arg) {
		case APM_NOTIFY_BP_QUIET:
			GPSR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
			last_MCU_INT_SW = 1;
			break;
		case APM_NOTIFY_BP_UNHOLD:
			GPCR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
			last_MCU_INT_SW = 0;
			break;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
	return 0;
}

static int do_release(struct inode * inode, struct file * filp)
{
	struct apm_user *	as;

	as = filp->private_data;
	if (check_apm_user(as, "release"))
		return 0;
	filp->private_data = NULL;
	lock_kernel();

	/*
	 * by zxf, 11/11/2002
	 * free memory malloced when open correctly
	 */
	if (user_list == as)
		user_list = as->next;
	else {
		struct apm_user *	as1;

		for (as1 = user_list;
		     (as1 != NULL) && (as1->next != as);
		     as1 = as1->next)
			;
		if (as1 == NULL)
			printk(KERN_ERR "apm: filp not in user list\n");
		else
			as1->next = as->next;
	}

	unlock_kernel();
	APM_DPRINTK("do_release: free mem at 0x%x\n", (unsigned int)as);
	kfree(as);
	return 0;
}

static int do_open(struct inode * inode, struct file * filp)
{
	struct apm_user *	as;

	/* now make apm can only be accessed by one user */
	if (user_list != NULL)
		return -EBUSY;

	as = (struct apm_user *)kmalloc(sizeof(*as), GFP_KERNEL);
	APM_DPRINTK("do_open: malloc mem at 0x%x\n", (unsigned int)as);
	if (as == NULL) {
		printk(KERN_ERR "apm: cannot allocate struct of size %d bytes\n",
		       sizeof(*as));
		return -ENOMEM;
	}
	as->magic = APM_BIOS_MAGIC;
	as->event_tail = as->event_head = 0;
	as->suspends_pending = as->standbys_pending = 0;
	as->suspends_read = as->standbys_read = 0;
	/*
	 * XXX - this is a tiny bit broken, when we consider BSD
         * process accounting. If the device is opened by root, we
	 * instantly flag that we used superuser privs. Who knows,
	 * we might close the device immediately without doing a
	 * privileged operation -- cevans
	 */
	as->suser = capable(CAP_SYS_ADMIN);
	as->next = user_list;
	user_list = as;
	filp->private_data = as;

	return 0;
}

static int apm_get_info(char *buf, char **start, off_t fpos, int length)
{
	char *		p;
	unsigned short	dx;
	unsigned short	error;
	unsigned char   ac_line_status = 0xff;
	unsigned char   battery_status = 0xff;
	unsigned char   battery_flag   = 0xff;
        unsigned char   percentage     = 0xff;
	int             time_units     = -1;
	char            *units         = "?";

	p = buf;

	if ((smp_num_cpus == 1) &&
	    !(error = apm_get_power_status(&ac_line_status,
                                           &battery_status, &battery_flag, &percentage, &dx))) {
		if (apm_bios_info.version > 0x100) {
			if (dx != 0xffff) {
				units = (dx & 0x8000) ? "min" : "sec";
				time_units = dx & 0x7fff;
			}
		}
	}
	/* Arguments, with symbols from linux/apm_bios.h.  Information is
	   from the Get Power Status (0x0a) call unless otherwise noted.

	   0) Linux driver version (this will change if format changes)
	   1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
	   2) APM flags from APM Installation Check (0x00):
	      bit 0: APM_16_BIT_SUPPORT
	      bit 1: APM_32_BIT_SUPPORT
	      bit 2: APM_IDLE_SLOWS_CLOCK
	      bit 3: APM_BIOS_DISABLED
	      bit 4: APM_BIOS_DISENGAGED
	   3) AC line status
	      0x00: Off-line
	      0x01: On-line
	      0x02: On backup power (BIOS >= 1.1 only)
	      0xff: Unknown
	   4) Battery status
	      0x00: High
	      0x01: Low
	      0x02: Critical
	      0x03: Charging
	      0x04: Selected battery not present (BIOS >= 1.2 only)
	      0xff: Unknown
	   5) Battery flag
	      bit 0: High
	      bit 1: Low
	      bit 2: Critical
	      bit 3: Charging
	      bit 7: No system battery
	      0xff: Unknown
	   6) Remaining battery life (percentage of charge):
	      0-100: valid
	      -1: Unknown
	   7) Remaining battery life (time units):
	      Number of remaining minutes or seconds
	      -1: Unknown
	   8) min = minutes; sec = seconds */

	p += sprintf(p, "%s %d.%d 0x%02x 0x%02x 0x%02x 0x%02x %d%% %d %s\n",
		     driver_version,
		     (apm_bios_info.version >> 8) & 0xff,
		     apm_bios_info.version & 0xff,
		     apm_bios_info.flags,
		     ac_line_status,
		     battery_status,
		     battery_flag,
		     percentage,
		     time_units,
		     units);

	return p - buf;
}

#ifndef MODULE
static int __init apm_setup(char *str)
{
	int	invert;

	while ((str != NULL) && (*str != '\0')) {
		if (strncmp(str, "off", 3) == 0)
			apm_disabled = 1;
		if (strncmp(str, "on", 2) == 0)
			apm_disabled = 0;
		invert = (strncmp(str, "no-", 3) == 0);
		if (invert)
			str += 3;
		if (strncmp(str, "debug", 5) == 0)
			debug = !invert;
		if ((strncmp(str, "power-off", 9) == 0) ||
		    (strncmp(str, "power_off", 9) == 0))
			power_off = !invert;
		str = strchr(str, ',');
		if (str != NULL)
			str += strspn(str, ", \t");
	}
	return 1;
}

__setup("apm=", apm_setup);
#endif


static struct file_operations apm_bios_fops = {
	owner:		THIS_MODULE,
	read:		do_read,
	poll:		do_poll,
	ioctl:		do_ioctl,
	open:		do_open,
	release:	do_release,
};
static struct miscdevice apm_device = {
	APM_MINOR_DEV,
	"apm_bios",
	&apm_bios_fops
};

#define APM_INIT_ERROR_RETURN	return -1

/*
 * Just start the APM thread. We do NOT want to do APM BIOS
 * calls from anything but the APM thread, if for no other reason
 * than the fact that we don't trust the APM BIOS. This way,
 * most common APM BIOS problems that lead to protection errors
 * etc will have at least some level of being contained...
 *
 * In short, if something bad happens, at least we have a choice
 * of just killing the apm thread..
 */
static int __init apm_init(void)
{
	if (apm_bios_info.version == 0) {
		printk(KERN_INFO "apm: BIOS not found.\n");
		APM_INIT_ERROR_RETURN;
	}
	printk(KERN_INFO
		"apm: BIOS version %d.%d Flags 0x%02x (Driver version %s)\n",
		((apm_bios_info.version >> 8) & 0xff),
		(apm_bios_info.version & 0xff),
		apm_bios_info.flags,
		driver_version);

	if (apm_disabled) {
		printk(KERN_NOTICE "apm: disabled on user request.\n");
		APM_INIT_ERROR_RETURN;
	}

	if (PM_IS_ACTIVE()) {
		printk(KERN_NOTICE "apm: overridden by ACPI.\n");
		APM_INIT_ERROR_RETURN;
	}
	pm_active = 1;

	create_proc_info_entry("apm", 0, NULL, apm_get_info);

	misc_register(&apm_device);


	return 0;
}

static void __exit apm_exit(void)
{
	misc_deregister(&apm_device);
	remove_proc_entry("apm", NULL);
	if (power_off)
		pm_power_off = NULL;
	exit_kapmd = 1;
	while (kapmd_running)
		schedule();
	pm_active = 0;
}

module_init(apm_init);
module_exit(apm_exit);

MODULE_AUTHOR("Jamey Hicks, pulling bits from original by Stephen Rothwell");
MODULE_DESCRIPTION("A minimal emulation of APM");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Enable debug mode");
MODULE_PARM(power_off, "i");
MODULE_PARM_DESC(power_off, "Enable power off");

EXPORT_NO_SYMBOLS;

