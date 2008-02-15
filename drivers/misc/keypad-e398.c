/*
 * linux/drivers/misc/keypad-e398.c
 *
 * Support for the Motorola Ezx A780 Development Platform.
 * 
 * Copyright (C) 2004 Motorola
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * Aug 19,2004 - (Motorola) Created
 * 
 * This driver is for three devices: /dev/keypad, /dev/keypadB and
 * /dev/keypadI.
 * /dev/keypad would be used for reading the key event buffer. 
 * It can be opened by one process at a time.
 * /dev/keypadB would be used for ioctl(KEYPAD_IOC_GETBITMAP).
 * It can be opened by one process at a time.
 * /dev/keypadI would be used for ioctl(KEYPAD_IOC_INSERT_EVENT).
 * It can be opened any number of times simultaneously.
 *
 * The bulverde specification is ambiguous about when interrupts happen
 * for released keys.  We were told by Intel that we won't
 * get interrupts for released keys except in the case when multiple
 * keys were down and they've all been released.  So we implemented
 * a timer to poll for changes in key states.
 *
 * The E680 P2 hardware gives us interrupts when any key is released,
 * whether it was the only key down or not, and whether it's the last
 * of multiple keys to be released or not.  We don't know whether this
 * behavior will continue in future hardware releases, so the release
 * timer is still in the code, #ifdef'd with USE_RELEASE_TIMER.  On the
 * P2 hardware, the code works with or without USE_RELEASE_TIMER defined.
 * If the release interrupts are always going to happen, we can remove
 * the #ifdef'd code.
 *
 * With the P2 hardware, the power key bit in the KPDK register is always
 * on.  We don't know if this is correct behavior or not, but in any case
 * it certainly causes trouble to have that key autorepeating indefinitely,
 * or to be checking indefinitely for the key to be released (if we're doing
 * release polling).
 *
 * For now, any_keys_down() returns 0 if the power key is the only one down.
 * . At interrupt time, if the power key is the only one down we don't
 *   set the autorepeat timer or release timer.
 * . When the release timer goes off, if the power key is the only one
 *   still down we don't set the timer again.
 * . When the autorepeat timer goes off, if the power key is the only
 *   one down we don't generate any events.
 * In autorepeat_timer_went_off, if there are non-power keys down, while we're
 * looking through the whole bitmap of keys down we ignore the power key.
 */

#include <linux/tty.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/bitops.h>
#include <linux/param.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/pm.h>
#include <linux/apm_bios.h>
#include "keypad.h"
#include <asm/arch/irqs.h>
#include "asm/arch/ezx.h"

#ifdef CONFIG_PM
static struct pm_dev *pm_dev;
static struct pm_dev *keypadI_pm_dev;
static int kpc_res,kpkdi_res;
static int GPIO_TC_MM_STATE = 1;
extern unsigned char pxafb_ezx_getLCD_status(void);
#endif
/*
 * This is the number of microseconds we wait before checking to see if any
 * keys have been released.
 */
#define RDELAY 200

#define KEYBUF_EMPTY() (keybuf_start == keybuf_end)
#define KEYBUF_FULL()  ( ((keybuf_end+1)%KEYBUF_SIZE) == keybuf_start)
#define KEYBUF_INC(x)  ((x) = (((x)+1) % KEYBUF_SIZE))

static int keypad_open (struct inode *, struct file *);
static int keypad_close (struct inode *, struct file *);
static int keypad_ioctl (struct inode *, struct file *,
                         unsigned int, unsigned long);
static unsigned int keypad_poll (struct file *, poll_table *);
static ssize_t keypad_read (struct file *, char *, size_t, loff_t *);

static int keypadB_open (struct inode *, struct file *);
static int keypadB_close (struct inode *, struct file *);
static int keypadB_ioctl (struct inode *, struct file *,
                          unsigned int, unsigned long);
static unsigned int keypadB_poll (struct file *, poll_table *);

static int keypadI_open(struct inode *, struct file *);
static int keypadI_close(struct inode *, struct file *);
static int keypadI_ioctl (struct inode *, struct file *,
                          unsigned int, unsigned long);

static void kp_interrupt (int, void *, struct pt_regs *);
#ifdef USE_RELEASE_TIMER
static void release_timer_went_off (unsigned long);
static void do_scan(void);
#endif
static void autorepeat_timer_went_off (unsigned long);

static int add_to_keybuf (unsigned short);
static int add_events (unsigned long *, unsigned long *);
static int insert_event (unsigned short);
static unsigned short get_from_keybuf (void);
static void scan_to_bitmap
           (unsigned long, unsigned long, unsigned long, unsigned long *);
static inline void set_bitmap_to_zero(unsigned long b[]);
static inline void copy_bitmap(unsigned long dest[], unsigned long source[]);
static inline void turn_on_bit(unsigned long map[], int);
static inline void turn_off_bit(unsigned long map[], int);
static inline int  any_keys_down (unsigned long *);

/*
 * keybuf is a circular buffer to hold keypad events.
 * keybuf_start is the index of the first event.
 * keybuf_end is one more than the index of the last event.
 * The buffer is empty when keybuf_start == keybuf_end.
 * We only use KEYBUF_SIZE-1 entries in the buffer so we can tell
 * when it's full without needing another variable to tell us.
 * The buffer is full when (keybuf_end+1)%KEYBUF_SIZE == keybuf_start.
 */
static unsigned short keybuf[KEYBUF_SIZE];
static int keybuf_start = 0;
static int keybuf_end = 0;

static DECLARE_WAIT_QUEUE_HEAD(keypad_wait);
static spinlock_t keypad_lock = SPIN_LOCK_UNLOCKED;

static int reading_opens = 0;
static int bitmap_opens = 0;

/* previous bitmap of keys down */
static unsigned long oldkeybitmap[NUM_WORDS_IN_BITMAP]; 
/* current bitmap of keys down */
static unsigned long keybitmap[NUM_WORDS_IN_BITMAP];
/* last bitmap read by ioctl */
static unsigned long lastioctlbitmap[NUM_WORDS_IN_BITMAP]; 

static unsigned long kpas = 0;     /* last value of kpas */
static unsigned long kpasmkp0 = 0; /* last value of kpasmkp0 */
static unsigned long kpasmkp1 = 0; /* last value of kpasmkp1 */
static unsigned long kpasmkp2 = 0; /* last value of kpasamkp2*/
static unsigned long kpdk = 0;     /* last value of kpdk */

#ifdef USE_RELEASE_TIMER
static struct timer_list key_release_timer;
static int key_release_interval = RDELAY * HZ/1000;
#endif

static int  do_autorepeat = 1;
static long jiffies_to_first_repeat = 30;
static long jiffies_to_next_repeat = 30;
static struct timer_list autorepeat_timer;

extern int lockscreen_flag;

static int
keypad_open(struct inode *inode, struct file *file) 
{
#ifdef KDEBUG
    printk(KERN_DEBUG "keypad_open\n");
#endif
    spin_lock(keypad_lock);
    if (reading_opens > 0)
    {
       spin_unlock(keypad_lock);
       return -EBUSY;
    }
    reading_opens++;
    spin_unlock(keypad_lock);

    return 0;
}

static int
keypadB_open(struct inode *inode, struct file *file)
{
#ifdef KDEBUG
    printk(KERN_DEBUG "keypadB_open\n");
#endif
    spin_lock(keypad_lock);
    if (bitmap_opens > 0)
    {
       spin_unlock(keypad_lock);
       return -EBUSY;
    }
    bitmap_opens++;
    /*
     * poll returns when lastioctlbitmap is different from keybitmap.
     * we set lastioctlbitmap to keybitmap here so that a poll right
     * after an open returns when the bitmap is different from when
     * the open was done, not when the bitmap is different from the
     * last time some other process read it
     */
    copy_bitmap(lastioctlbitmap, keybitmap);
    spin_unlock(keypad_lock);

    return 0;
}

#if defined(CONFIG_KEYPAD_E680)
#ifdef CONFIG_PM
static int keypadI_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch(req)
       	{
       	case PM_SUSPEND:
          break;
	case PM_RESUME:
        GPDR(GPIO_TC_MM_EN) |= GPIO_bit(GPIO_TC_MM_EN);
        if(GPIO_TC_MM_STATE == 1)
          GPSR(GPIO_TC_MM_EN) = GPIO_bit(GPIO_TC_MM_EN);
		else
          GPCR(GPIO_TC_MM_EN) = GPIO_bit(GPIO_TC_MM_EN);
		break;
	default:
		break;
	}		
    return 0;
}
#endif
#endif

static int
keypadI_open(struct inode *inode, struct file *file)
{
#ifdef KDEBUG
    printk(KERN_DEBUG "keypadI_open\n");
#endif
#if defined(CONFIG_KEYPAD_E680)
    	set_GPIO_mode(GPIO_TC_MM_EN);
    	GPDR(GPIO_TC_MM_EN)   |= GPIO_bit(GPIO_TC_MM_EN);
    	GPSR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);   
#endif

    return 0;
}

static int
keypad_close(struct inode * inode, struct file *file)
{
#ifdef KDEBUG
    printk(KERN_DEBUG " keypad_close\n");
#endif
    /* 
     * this decrement of reading_opens doesn't have to be protected because
     * it can only be executed to close the single open of /dev/keypad
     */
    reading_opens--;
    return 0;
}

static int
keypadB_close(struct inode * inode, struct file *file)
{
#ifdef KDEBUG
    printk(KERN_DEBUG " keypadB_close\n");
#endif
    /* 
     * this decrement of bitmap_opens doesn't have to be protected because
     * it can only be executed to close the single open of /dev/keypadB
     */
    bitmap_opens--;
    return 0;
}

static int
keypadI_close(struct inode * inode, struct file *file)
{
#ifdef KDEBUG
    printk(KERN_DEBUG " keypadI_close\n");
#endif
#if defined(CONFIG_KEYPAD_E680)    
    set_GPIO_mode(GPIO_TC_MM_EN);
        GPDR(GPIO_TC_MM_EN)   |= GPIO_bit(GPIO_TC_MM_EN);
        GPSR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);    
#endif    
    
    return 0;
}

/**
 * Add the specified event to the key buffer.
 * This should be called with keypad_lock locked.
 */
static int
add_to_keybuf (unsigned short event)
{
    if (KEYBUF_FULL())
    {
         return -ENOMEM;
    }
	    
   	keybuf[keybuf_end] = event;
   	KEYBUF_INC (keybuf_end);
    
#if CONFIG_APM
    queue_apm_event(KRNL_KEYPAD, NULL);
#endif 
    printk("add keypad event = %x\n", event);
    return 0;
}

/*
 * code[c*8+r] is the key code for the key that's down when there's a 1
 * in column c, row r of the scan registers.
 */

#if defined(CONFIG_KEYPAD_V700)
int
scanbit_to_keycode[] = {
 /* col 0 */
 KEYPAD_UP, KEYPAD_DOWN, KEYPAD_LEFT, KEYPAD_RIGHT, 
 KEYPAD_POUND, KEYPAD_0, KEYPAD_9, KEYPAD_NONE,
 /* col 1 */
 KEYPAD_2, KEYPAD_4, KEYPAD_6, KEYPAD_8, 
 KEYPAD_7, KEYPAD_SLEFT, KEYPAD_SRIGHT, KEYPAD_NONE,
 /* col 2 */
 KEYPAD_MENU, KEYPAD_1, KEYPAD_3, KEYPAD_5, 
 KEYPAD_STAR, KEYPAD_VOLUP, KEYPAD_VOLDOWN, KEYPAD_NONE,
 /* col 3 */
 KEYPAD_CAMERA, KEYPAD_CLEAR, KEYPAD_CARRIER, KEYPAD_ACTIVATE,
 KEYPAD_SEND, KEYPAD_SMART, KEYPAD_VAVR, KEYPAD_NONE
};

/* end CONFIG_KEYPAD_V700 */
#elif defined(CONFIG_E680_P4A)
int
scanbit_to_keycode[] = {
 /* col 0 */
 KEYPAD_UP, KEYPAD_DOWN, KEYPAD_LEFT, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 1 */
 KEYPAD_RIGHT, KEYPAD_CENTER, KEYPAD_HOME, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 2 */
 KEYPAD_GAME_R, KEYPAD_NONE, KEYPAD_GAME_L, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 3 */
 KEYPAD_A, KEYPAD_B, KEYPAD_NONE, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
};

/* end CONFIG_E680_P4A */
#elif defined(CONFIG_KEYPAD_E680)
int
scanbit_to_keycode[] = {
 /* col 0 */
 KEYPAD_UP, KEYPAD_DOWN,  KEYPAD_NONE, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 1 */
 KEYPAD_RIGHT, KEYPAD_LEFT, KEYPAD_NONE, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 2 */
 KEYPAD_NONE, KEYPAD_GAME_R, KEYPAD_NONE, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 3 */
 KEYPAD_HOME, KEYPAD_GAME_L, KEYPAD_CENTER, KEYPAD_NONE,
 KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
     
};
/*end CONFIG_KEYPAD_E680 */
#elif defined(CONFIG_KEYPAD_A780)
int
scanbit_to_keycode[] = {
 /* col 0 */
 KEYPAD_OK, KEYPAD_1, KEYPAD_4, KEYPAD_7,
 KEYPAD_STAR, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 1 */
 KEYPAD_MENU, KEYPAD_2, KEYPAD_5, KEYPAD_8,
 KEYPAD_0, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 2 */
 KEYPAD_CANCEL, KEYPAD_3, KEYPAD_6, KEYPAD_9,
 KEYPAD_POUND, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 3 */
 KEYPAD_JOG_UP, KEYPAD_JOG_MIDDLE, KEYPAD_PTT, KEYPAD_HOME,
 KEYPAD_JOG_DOWN, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
 /* col 4 */
 KEYPAD_UP, KEYPAD_CENTER, KEYPAD_LEFT, KEYPAD_RIGHT,
 KEYPAD_DOWN, KEYPAD_NONE, KEYPAD_NONE, KEYPAD_NONE,
};
#endif

/*
 * Decode the specified scan register values into the bitmap pointed
 * to by the last argument.  The bitmap will contain a 1
 * for each key that's down.
 *
 * Only the 1st two of the four scan registers are used.
 */
static void
scan_to_bitmap(unsigned long kpas, unsigned long kpasmkp0,
               unsigned long kpasmkp1, unsigned long bitmap[])
{
   int row, col;
   int bitnum;
   unsigned long scanmap;
#if defined(CONFIG_KEYPAD_A780)
   int keep;
#endif   
   
   set_bitmap_to_zero (bitmap);

   if ((kpas & KPAS_MUKP) == MUKP_NO_KEYS)
   {
      return;
   }

   if ((kpas & KPAS_MUKP) == MUKP_ONE_KEY)
   {
      row = (kpas & KPAS_RP) >> 4;
      col = kpas & KPAS_CP;
      turn_on_bit (bitmap, scanbit_to_keycode[col*8 + row]);
      return;
   }

   /* reach here if multiple keys */
   scanmap = (kpasmkp0 & 0x7f) | ((kpasmkp0 & 0x7f0000) >> 8) |
             ((kpasmkp1 & 0x7f) << 16) | ((kpasmkp1 & 0x7f0000) << 8);
   while ((bitnum = ffs(scanmap)) != 0)
   {
     /*
      * ffs returns bit numbers starting with 1, so subtract 1 to index
      * into scanbit_to_keycode[]
      */
      turn_on_bit (bitmap, scanbit_to_keycode[bitnum-1]);
      scanmap &= ~(1<<(bitnum-1));
   }
#if defined(CONFIG_KEYPAD_A780)
   kpasmkp2 = KPASMKP2;
   printk("kpasmkp0 = 0x\n", kpasmkp0);
   printk("kpasmkp1 = 0x\n", kpasmkp1);
   printk("kpasmkp2 = 0x\n", kpasmkp2);   
   if( (kpasmkp2 != 1) || (kpasmkp2 != 2) | (kpasmkp2 != 4) | (kpasmkp2 != 8) | (kpasmkp2 != 16) )
	   return;
   while ((bitnum = ffs(kpasmkp2)) !=0)
   {
      turn_on_bit (bitmap, scanbit_to_keycode[32+bitnum-1]);
      kpasmkp2 &= ~(1<<(bitnum-1));
   }   
#endif   
}

/* 
 * Add events indicated by the difference between the last scan (oldbitmap)
 * and this one (newbitmap) to the input buffer.
 * Return nonzero right away if any of the events can't be added.
 * A zero return means all the events were added.
 * This should be called with keypad_lock locked.
 */
static int
add_events (unsigned long *oldbitmap, unsigned long *newbitmap)
{
   unsigned long tmpmap, onebitmap;
   int bitnum, i, ret;

   /* generate events for keys that were down before and are up now */
   for (i=NUM_WORDS_IN_BITMAP-1;i>=0;i--)
   {
     tmpmap = oldbitmap[i];
     while ((bitnum = ffs(tmpmap)) != 0)
     {
       onebitmap = 1<<(bitnum-1);
       if ((newbitmap[i] & onebitmap) == 0) 
       {
         ret = add_to_keybuf(bitnum + (32*(NUM_WORDS_IN_BITMAP-1-i) | KEYUP));
         if (ret < 0)
         {
            return ret;
         }
       }
       tmpmap &= ~onebitmap;
     }
   }
   /* generate events for down keys that were up before */
   for (i=NUM_WORDS_IN_BITMAP-1;i>=0;i--)
   {
     tmpmap = newbitmap[i];
     while ((bitnum = ffs(tmpmap)) != 0)
     {
        onebitmap = 1<<(bitnum-1);
        if ((oldbitmap[i] & onebitmap) == 0) 
        {
           ret = add_to_keybuf
                        (bitnum + (32*(NUM_WORDS_IN_BITMAP-1-i) | KEYDOWN));
           if (ret < 0)
           {
               return ret;
           }
        }
        tmpmap &= ~onebitmap;
     }
   }
   return 0;
}

/*
 * do the INSERT_EVENT ioctl
 */
static int 
insert_event (unsigned short event)
{
    unsigned long flags;
    int ret;

    if (KEYCODE(event) > KEYPAD_MAXCODE)
    {
        printk(KERN_WARNING " inserted key code %d too big\n",
                KEYCODE(event));
        return -EINVAL;
    }
    spin_lock_irqsave(&keypad_lock, flags);
    if (KEY_IS_DOWN(event))
    {
        turn_on_bit(keybitmap, KEYCODE(event));
    }
    else
    {
        turn_off_bit (keybitmap, event);
    }
    if ((ret = add_to_keybuf(event)) < 0)
    {
        spin_unlock_irqrestore(&keypad_lock,flags);
        return ret;
    }
    spin_unlock_irqrestore(&keypad_lock, flags);
    wake_up_interruptible (&keypad_wait);
    return 0;
}

static int
keypad_ioctl (struct inode *inode, struct file *file,
              unsigned int cmd, unsigned long arg) 
{
    int interval; /* debounce interval */
    int imkp;     /* Ignore Multiple Key Press bit */
    struct autorepeatinfo ar; /* autorepeat information */

#ifdef KDEBUG
    printk (KERN_DEBUG "keypad_ioctl: 0x%x\n", cmd);
#endif
    switch (cmd)
    {
       case KEYPAD_IOC_INSERT_EVENT:
         return insert_event ((unsigned short)arg);
         break;
       case KEYPAD_IOC_GET_DEBOUNCE_INTERVAL:
         interval = KPKDI & KPKDI_BITS;
         return put_user(interval, (unsigned long *)arg);
         break;
       case KEYPAD_IOC_SET_DEBOUNCE_INTERVAL:
         interval = (unsigned short)arg;
         if (interval > KPKDI_BITS)
         {
            return -EINVAL;
         }
         KPKDI &= ~KPKDI_BITS;
         KPKDI |= interval;
         break;
       case KEYPAD_IOC_GET_IMKP_SETTING:
         imkp = ((KPC & KPC_IMKP) == KPC_IMKP);
         return put_user(imkp, (unsigned char *)arg);
         break;
       case KEYPAD_IOC_SET_IMKP_SETTING:
         imkp = (unsigned char)arg;
         if (imkp)
         {
            KPC |= KPC_IMKP;
         }
         else
         {
            KPC &= ~KPC_IMKP;
         }
         break;
       case KEYPAD_IOC_SET_AUTOREPEAT:
         if (copy_from_user (&ar, (void *)arg, 
                     sizeof(struct autorepeatinfo)) != 0)
         {
            return -EFAULT;
         }
         do_autorepeat = ar.r_repeat;
	 /* times are specified in milliseconds; convert to jiffies */
         jiffies_to_first_repeat = ar.r_time_to_first_repeat * HZ/1000;
         jiffies_to_next_repeat = ar.r_time_between_repeats * HZ/1000;
         break;
       default:
        return -ENOTTY;
    }
    return 0;
}

static int
keypadB_ioctl (struct inode *inode, struct file *file,
               unsigned int cmd, unsigned long arg) 
{
    unsigned long flags;
    int ret;

#ifdef KDEBUG
    printk (KERN_DEBUG "keypadB_ioctl: 0x%x\n", cmd);
#endif
    if (cmd == KEYPAD_IOC_INSERT_EVENT)
    {
        return insert_event ((unsigned short)arg);
    }
    else if (cmd == KEYPAD_IOC_GETBITMAP)
    {
        spin_lock_irqsave(&keypad_lock, flags);
        ret = copy_to_user ((void *)arg, keybitmap,
                             NUM_WORDS_IN_BITMAP * sizeof(unsigned long));
        copy_bitmap (lastioctlbitmap, keybitmap);
        spin_unlock_irqrestore(&keypad_lock, flags);
        return (ret != 0) ? -EFAULT : 0;
    }
    else 
    {
        return -ENOTTY;
    }
}

static int
keypadI_ioctl (struct inode *inode, struct file *file,
               unsigned int cmd, unsigned long arg) 
{
    unsigned long flags;
    int ret;

#ifdef KDEBUG
    printk (KERN_DEBUG "keypadI_ioctl: 0x%x\n", cmd);
#endif
#if defined(CONFIG_KEYPAD_E680)    
    if( cmd == KEYPADI_TURN_ON_LED )
    {
        GPCR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);   
        GPIO_TC_MM_STATE = 0;
        PGSR3 &= ~GPIO_bit(GPIO_TC_MM_EN);
	return 0;
    }
    if( cmd == KEYPADI_TURN_OFF_LED )	    
    {       
       GPSR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);
       GPIO_TC_MM_STATE = 1;
       PGSR3 |= GPIO_bit(GPIO_TC_MM_EN);
       return 0;
    }
#endif

    if (cmd == KEYPAD_IOC_INSERT_EVENT)
    {
        return insert_event ((unsigned short)arg);
    }
    else if (cmd == KEYPAD_IOC_GETBITMAP)
    {
        spin_lock_irqsave(&keypad_lock, flags);
        ret = copy_to_user ((void *)arg, keybitmap,
                             NUM_WORDS_IN_BITMAP * sizeof(unsigned long));
        copy_bitmap (lastioctlbitmap, keybitmap);
        spin_unlock_irqrestore(&keypad_lock, flags);
        return (ret != 0) ? -EFAULT : 0;
    }
    else 
    {
        return -ENOTTY;
    }
}

static unsigned int
keypad_poll( struct file *file, poll_table * wait )
{
#ifdef KDEBUG
    printk(KERN_DEBUG " keypad_poll\n");
#endif
    poll_wait(file, &keypad_wait, wait);

    if (!KEYBUF_EMPTY())
    {
        return (POLLIN | POLLRDNORM);
    }
    return 0;
}

static unsigned int
keypadB_poll( struct file *file, poll_table * wait )
{
    int i;

#ifdef KDEBUG
    printk(KERN_DEBUG " keypadB_poll\n");
#endif
    poll_wait(file, &keypad_wait, wait);

    for (i=0; i < NUM_WORDS_IN_BITMAP; i++)
    {
       if (lastioctlbitmap[i] != keybitmap[i])
       {
           return (POLLIN | POLLRDNORM);
       }
    }
    return 0;
}

static unsigned short
get_from_keybuf(void)
{
    unsigned short event;
    unsigned long flags;

    spin_lock_irqsave(&keypad_lock, flags);
    event = keybuf[keybuf_start];
    KEYBUF_INC (keybuf_start);
    spin_unlock_irqrestore(&keypad_lock, flags);
    return event;
}

static ssize_t
keypad_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
    int i, ret;
    unsigned short event;

#ifdef KDEBUG
    printk(KERN_DEBUG " keypad_read\n");
#endif
    /* Can't seek (pread) on this device */
    if (ptr != &file->f_pos) 
    {
        return -ESPIPE;
    }

    if (count == 0)
    {
        return 0;
    }

    if (KEYBUF_EMPTY())
    { 
        /* buffer is empty */
        /* if not blocking return */
        if (file->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }
        /* blocking, so wait for input */
        ret = wait_event_interruptible(keypad_wait, !KEYBUF_EMPTY());
        if (ret)
        {
            return ret;
        }
    }

    i = 0;
    /* copy events until we have what the user asked for or we run out */
    while ((i+1 < count) && !KEYBUF_EMPTY())
    {
        event = get_from_keybuf();
        if ((ret = put_user(event, (unsigned short *)buf)) != 0)
        {
            return ret;
        }
        buf += EVENTSIZE;
        i += EVENTSIZE;
    }
    return i;
}

#ifdef CONFIG_PM
static int button_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	int pksr;
	
	pksr = PKSR;
	PKSR = 0xfffff;
	switch(req)
	{
		case PM_SUSPEND:
			kpc_res = KPC;
			kpkdi_res = KPKDI;			
            set_bitmap_to_zero (oldkeybitmap);
            set_bitmap_to_zero (keybitmap);
            set_bitmap_to_zero (lastioctlbitmap);
            kpas = 0;
            kpasmkp0 = 0;
            kpasmkp1 = 0;
            kpasmkp2 = 0;
            kpdk = 0;
			break;
                case PM_RESUME:
			KPC = kpc_res;
			KPKDI = kpkdi_res;
#if CONFIG_APM
#if defined(CONFIG_E680_P4A)
		        if (pksr & 0xe4400) /*93 97 100 101 102 key is pressed (switch on) */
		        {
				queue_apm_event(KRNL_KEYPAD, NULL);
				if (pksr & 0x400)
					turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
		    	}
#endif			
#if defined(CONFIG_KEYPAD_E680)  
      			if (pksr & 0xee400) /*93 96 97 98 190 101 102 key is pressed */
      			{
				queue_apm_event(KRNL_KEYPAD, NULL);
				if (pksr & 0x400)
					turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
				if (pksr & 0x2000)
					turn_on_bit (keybitmap, KEYPAD_A);
				if (pksr & 0x8000)
					turn_on_bit (keybitmap, KEYPAD_B);
      			}
#endif
#if defined(CONFIG_KEYPAD_A780)
			printk("pksr = %x\n",pksr);
      			if (pksr & 0xec400) /* 93 97 98 100 101 102 key is pressed */
	      		{
				queue_apm_event(KRNL_KEYPAD, NULL);			
				if (pksr & 0x400)
					turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
		        }
#endif
#endif      
                
			break;
       }
       return 0;
}
#endif

/* for /dev/keypad */
static struct file_operations 
keypad_fops = {
    read:           keypad_read, 
    llseek:         no_llseek,
    poll:           keypad_poll, 
    ioctl:          keypad_ioctl, 
    open:           keypad_open, 
    release:        keypad_close,
};

static struct miscdevice 
keypad_misc_device = {
    KEYPAD_MINOR,
    KEYPAD_NAME,
    &keypad_fops,
};

/*
 *  for /dev/keypadB.
 *  read() isn't defined here. calls to read() will return -1 with EINVAL.
 */
static struct file_operations 
keypadB_fops = {
    llseek:         no_llseek,
    poll:           keypadB_poll, 
    ioctl:          keypadB_ioctl, 
    open:           keypadB_open, 
    release:        keypadB_close,
};

static struct miscdevice 
keypadB_misc_device = {
    KEYPAD_BITMAP_MINOR,
    KEYPAD_BITMAP_NAME,
    &keypadB_fops,
};

/*
 *  for /dev/keypadI.
 *  read() isn't defined here. calls to read() will return -1 with EINVAL.
 */
static struct file_operations 
keypadI_fops = {
    llseek:         no_llseek,
    ioctl:          keypadI_ioctl, 
    open:           keypadI_open, 
    release:        keypadI_close,
};

static struct miscdevice 
keypadI_misc_device = {
    KEYPAD_INSERT_MINOR,
    KEYPAD_INSERT_NAME,
    &keypadI_fops,
};

#ifdef CONFIG_PANIC_LOG
static u32 panic_bug_used =0;
#endif

/* Interrupt Handler for KEYPAD */
static void 
kp_interrupt(int irq, void *ptr, struct pt_regs *regs) 
{
        unsigned long flags;
        unsigned long kpc_val;

#ifdef USE_RELEASE_TIMER
        del_timer (&key_release_timer);
#endif
        del_timer (&autorepeat_timer);
        spin_lock_irqsave(&keypad_lock,flags);

        /* ack interrupt */
        kpc_val = KPC;

        /* matrix interrupt */
        if (kpc_val & KPC_MI) 
        {
/*
 * The Intel driver turned on KPC_AS here.  It doesn't seem that we
 * would need to do that, because getting an interrupt means that
 * a scan was just done.  For now, I've commented out the setting
 * and clearing of this bit.
           KPC |= KPC_AS;
 */
           while (KPAS & KPAS_SO)
           { 
              /* Wait for the Scan On bit to go off before */
              /* reading the scan registers. */
              NULL;
           };

           kpas = KPAS;
           kpasmkp0 = KPASMKP0;
           kpasmkp1 = KPASMKP1;
           kpasmkp2 = KPASMKP2;
          
        }

        /* direct interrupt */
        if (kpc_val & KPC_DI) 
        {
            kpdk = KPDK;
	    /* 
	     * reading the register turns off the "key pressed since last read"
             * bit if it was on, so we turn it off 
	     */
            kpdk &= ~KPDK_DKP;
        }

        copy_bitmap (oldkeybitmap, keybitmap);
        scan_to_bitmap (kpas, kpasmkp0, kpasmkp1, keybitmap);
#if defined(CONFIG_E680_P4A)
        if (kpdk & KPDK_DK0) /* VR key is pressed */
        {
           turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
        }
	if (kpdk & KPDK_DK4)
	{
	   turn_on_bit (keybitmap, KEYPAD_POWER);
	}
	/* power key is connected to GPIO97 as GPIO input */
#elif defined(CONFIG_KEYPAD_E680)
        if (kpdk & KPDK_DK0) /* VR key is pressed */
        {
           turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
        }
        if (kpdk & KPDK_DK4)
        {
           turn_on_bit (keybitmap, KEYPAD_POWER);
        }
        /* power key is connected to GPIO97 as GPIO input */
        if (kpdk & KPDK_DK3)
        {
           turn_on_bit (keybitmap, KEYPAD_A);
        }
        if (kpdk & KPDK_DK5)
        {
           turn_on_bit (keybitmap, KEYPAD_B);
        }
	
#elif defined(CONFIG_KEYPAD_A780)
        if (kpdk & KPDK_DK0) /* voice/camera key is pressed */
        {
           turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
        }
#endif

#ifdef CONFIG_PANIC_LOG
        if ((kpdk & KPDK_DK0)&&(kpasmkp1& 0x00020000))
        {
            if(0 == panic_bug_used)
            {
                panic_bug_used = 1;
                BUG();
            }
        }
#endif

        (void) add_events (oldkeybitmap, keybitmap);

        /* If any keys are down, set a timer to check for key release  */
	/* and one for autorepeat if that's on.                        */
        if (any_keys_down (keybitmap))
        {
#ifdef USE_RELEASE_TIMER
            key_release_timer.expires = (jiffies + key_release_interval);
            add_timer (&key_release_timer);
#endif
            if (do_autorepeat)
            {
              autorepeat_timer.expires = (jiffies + jiffies_to_first_repeat);
              add_timer (&autorepeat_timer);
            }
        }

        spin_unlock_irqrestore(&keypad_lock, flags);
        wake_up_interruptible (&keypad_wait);
	printk("keypad interrupt occurred\n");
}

#ifdef USE_RELEASE_TIMER
/*
 * This is called when the key release timer goes off.  That's the
 * only time it should be called.  Check for changes in what keys
 * are down.
 */
static void
release_timer_went_off(unsigned long unused)
{
   unsigned long flags;

   spin_lock_irqsave(&keypad_lock,flags);
   do_scan();
   /* If any keys are still down, set the timer going again. */
   if (any_keys_down (keybitmap))
   {
      mod_timer (&key_release_timer, jiffies + key_release_interval);
   }
   else
   {
      del_timer (&autorepeat_timer);
   }
   spin_unlock_irqrestore(&keypad_lock, flags);
}
#endif

/*
 * This is called when the autorepeat timer goes off.
 */
static void
autorepeat_timer_went_off(unsigned long unused)
{
   int i, bitnum;
   unsigned long tmp;
   unsigned long flags;

   spin_lock_irqsave(&keypad_lock,flags);
   if (!any_keys_down (keybitmap))
   {
      spin_unlock_irqrestore(&keypad_lock, flags);
      return;
   }
   for (i=0;i<NUM_WORDS_IN_BITMAP;i++)
   {
     tmp = keybitmap[NUM_WORDS_IN_BITMAP-i-1];
     while ((bitnum = ffs(tmp)) != 0)
     {
       tmp &= ~(1<<(bitnum-1));
       /* see explanation at top of file */
       if ( (bitnum + (32*i)) == KEYPAD_POWER )
       {
          continue;
       }
       (void) add_to_keybuf( (bitnum + (32*i)) | KEYDOWN );
     }
   }
   spin_unlock_irqrestore(&keypad_lock, flags);
   wake_up_interruptible (&keypad_wait);

   mod_timer (&autorepeat_timer, jiffies + jiffies_to_next_repeat);
}

#ifdef USE_RELEASE_TIMER
/*
 * Do a scan and add key events for anything that's different from the
 * last scan.  We assume keypad_lock is locked when this is called
 * (by release_timer_went_off).
 */
static void
do_scan(void)
{
   unsigned long kpas_new, kpasmkp0_new, kpasmkp1_new;
   unsigned long kpdk_new;
   int change;

   /* Initiate an automatic scan */
   KPC |= KPC_AS;
   /* Wait for the Scan On bit to go off before */
   /* reading the scan registers. */
   while (KPAS & KPAS_SO)
   { 
      NULL;
   };
   kpas_new = KPAS;
   kpasmkp0_new = KPASMKP0;
   kpasmkp1_new = KPASMKP1;
   kpdk_new = KPDK;
   /*
    * reading the register turns off the "key pressed since last read" bit
    * if it was on, so we turn it off 
    */
   kpdk_new &= ~KPDK_DKP;
   KPC &= ~KPC_AS;

   change = ((kpas_new != kpas) || 
             (kpasmkp0_new != kpasmkp0) || 
             (kpasmkp1_new != kpasmkp1) ||
             (kpdk_new != kpdk));

   if (change)
   {
      kpas = kpas_new;
      kpasmkp0 = kpasmkp0_new;
      kpasmkp1 = kpasmkp1_new;
      kpdk = kpdk_new;

      copy_bitmap (oldkeybitmap, keybitmap);
      scan_to_bitmap (kpas, kpasmkp0, kpasmkp1, keybitmap);
#if defined(CONFIG_E680_P4A)    
      if (kpdk & KPDK_DK0) /* screen lock key is pressed */
      {
         turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
      }
      if (kpdk & KPDK_DK4)
      {
         turn_on_bit (keybitmap, KEYPAD_POWER);
      }	   
     /* power key is connected to GPIO97 as GPIO input */
#elif defined(CONFIG_KEYPAD_E680)
      if (kpdk & KPDK_DK0) /* screen lock key is pressed */
      {
         turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
      }
      if (kpdk & KPDK_DK4)
      {
         turn_on_bit (keybitmap, KEYPAD_POWER);
      }
      /* power key is connected to GPIO97 as GPIO input */
      if (kpdk & KPDK_DK3)
      {
         turn_on_bit (keybitmap, KEYPAD_A);
      }
      if (kpdk & KPDK_DK5)
      {
         turn_on_bit (keybitmap, KEYPAD_B);
      }
      
#elif defined(CONFIG_KEYPAD_A780)
      if (kpdk & KPDK_DK0) /* voice/camera key is pressed */
      {
         turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
      }
#endif
      add_events (oldkeybitmap, keybitmap);

      wake_up_interruptible (&keypad_wait);
   }
}
#endif

/*
 * set all words in the specified bitmap to 0
 */
static inline void set_bitmap_to_zero(unsigned long b[]) {
   int i;
   for (i=0;i<NUM_WORDS_IN_BITMAP;i++)
   {
      b[i]=0;
   }
}

/*
 * copy source bitmap to destination bitmap
 */
static inline void copy_bitmap(unsigned long dest[], unsigned long source[]) {
   int i;
   for (i=0;i<NUM_WORDS_IN_BITMAP;i++)
   {
      dest[i]=source[i];
   }
}

/*
 * Turn on the bit position in map for the specified key code.
 * Bit numbers start with 1 (not 0) and increase moving left within
 * words and within the array.
 * Bit 1 is the rightmost bit in map[NUM_WORDS_IN_BITMAP-1].
 * Bit 33 is the rightmost bit in map[NUM_WORDS_IN_BITMAP-2].
 */
static inline void 
turn_on_bit(unsigned long map[], int code)
{
   int word, pos, p;

   /* if we have 2 words, bits 1-32 are in map[1], 33-64 in map[0] */
   word = (NUM_WORDS_IN_BITMAP - 1) - ((code-1) / 32);
   /* 
    * bit 1 is 1<<0, bit 2 is 1<<1, ... bit 32 is 1 << 31, all in map[1]
    * bit 33 is 1<<0, bit 34 is 1<<1, ... in map[0]
    * we want this:
    *     code       pos
    *      1          0
    *      2          1
    *          ...
    *     31         30
    *     32         31
    *     33          0
    *     34          1
    *          ...
    */
   pos = (p = (code % 32)) == 0 ? 31 : p-1;
   map[word] |= (1<<pos);
}

/*
 * Turn off the bit position in map for the specified key code.
 * Bit positions start with 1 (not 0).
 */
static inline void 
turn_off_bit(unsigned long map[], int code)
{
   int word, pos, p;

   word = (NUM_WORDS_IN_BITMAP - 1) - ((code-1) / 32);
   pos = (p = (code % 32)) == 0 ? 31 : p-1;
   map[word] &= ~(1<<pos);
}

/*
 * Return 1 if any bits are down in map[]
 */
static inline int
any_keys_down (unsigned long *map)
{
  int i;
  for (i=0;i<NUM_WORDS_IN_BITMAP;i++)
  {
    /* don't count the power key */
    if ((i == 0) && (*map == 0x10))
    {
       map++;
       continue;
    }
    if (*map++)
    {
       return 1;
    }
  }
  return 0;
}
static int
__init keypad_init(void)
{
    int err;

#ifdef KDEBUG
    printk(KERN_DEBUG " keypad_init\n");
#endif

    set_bitmap_to_zero (oldkeybitmap);
    set_bitmap_to_zero (keybitmap);
    set_bitmap_to_zero (lastioctlbitmap);

    /* set up gpio */ 
#if defined (CONFIG_KEYPAD_V700)
    set_GPIO_mode(95 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<6> */
    set_GPIO_mode(97 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<3> */
    set_GPIO_mode(98 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<4> */
    set_GPIO_mode(99 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<5> */
    set_GPIO_mode(100 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<0> */
    set_GPIO_mode(101 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<1> */
    set_GPIO_mode(102 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<2> */
    set_GPIO_mode(103 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<0> */
    set_GPIO_mode(104 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<1> */
    set_GPIO_mode(105 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<2> */
    set_GPIO_mode(106 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<3> */
#elif defined(CONFIG_E680_P4A)
    set_GPIO_mode(93 | GPIO_ALT_FN_1_IN);  /* KP_DKIN<0>, VR Key */
    set_GPIO_mode(97 | GPIO_ALT_FN_1_IN);  /* KP_DKIN<4>, power key */
    set_GPIO_mode(100 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<0> */
    set_GPIO_mode(101 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<1> */
    set_GPIO_mode(102 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<2> */
    set_GPIO_mode(103 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<0> */
    set_GPIO_mode(104 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<1> */
    set_GPIO_mode(105 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<2> */
    set_GPIO_mode(106 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<3> */
#elif defined(CONFIG_KEYPAD_E680)
    set_GPIO_mode(93 | GPIO_ALT_FN_1_IN);  /* KP_DKIN<0>, VR Key */
    set_GPIO_mode(96 | GPIO_ALT_FN_1_IN);  /* KP_DKIN<3>, GAME_A */
    set_GPIO_mode(97 | GPIO_ALT_FN_1_IN);  /* KP_DKIN<4>, power key */
    set_GPIO_mode(98 | GPIO_ALT_FN_1_IN);  /* KP_DKIN<5>, GAME_B */
    set_GPIO_mode(100 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<0> */
    set_GPIO_mode(101 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<1> */
    set_GPIO_mode(102 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<2> */
    set_GPIO_mode(103 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<0> */
    set_GPIO_mode(104 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<1> */
    set_GPIO_mode(105 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<2> */
    set_GPIO_mode(106 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<3> */
    set_GPIO_mode(GPIO_TC_MM_EN);
    GPDR(GPIO_TC_MM_EN)   |= GPIO_bit(GPIO_TC_MM_EN);
    GPSR(GPIO_TC_MM_EN)   = GPIO_bit(GPIO_TC_MM_EN);
    PGSR3 |= GPIO_bit(GPIO_TC_MM_EN);
#elif defined (CONFIG_KEYPAD_A780)
    set_GPIO_mode(93 | GPIO_ALT_FN_1_IN);   /* KP_DKIN<0>, voice_rec */
    set_GPIO_mode(97 | GPIO_ALT_FN_3_IN);   /* KP_MKIN<3> */
    set_GPIO_mode(98 | GPIO_ALT_FN_3_IN);   /* KP_MKIN<4> */
    set_GPIO_mode(100 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<0> */
    set_GPIO_mode(101 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<1> */
    set_GPIO_mode(102 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<2> */
    set_GPIO_mode(103 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<0> */
    set_GPIO_mode(104 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<1> */
    set_GPIO_mode(105 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<2> */
    set_GPIO_mode(106 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<3> */
    set_GPIO_mode(107 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<4> */
#elif defined(CONFIG_KEYPAD_MAINSTONE)
    /* from Intel driver */
    set_GPIO_mode(93 | GPIO_ALT_FN_1_IN);
    set_GPIO_mode(94 | GPIO_ALT_FN_1_IN); 
    set_GPIO_mode(95 | GPIO_ALT_FN_1_IN); 
    set_GPIO_mode(98 | GPIO_ALT_FN_1_IN); 
    set_GPIO_mode(99 | GPIO_ALT_FN_1_IN); 
    set_GPIO_mode(100 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<0> */
    set_GPIO_mode(101 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<1> */
    set_GPIO_mode(102 | GPIO_ALT_FN_1_IN);  /* KP_MKIN<2> */
    set_GPIO_mode(103 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<0> */
    set_GPIO_mode(104 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<1> */
    set_GPIO_mode(105 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<2> */
    set_GPIO_mode(106 | GPIO_ALT_FN_2_OUT); /* KP_MKOUT<3> */
#endif

    /* set keypad control register */ 
    KPC = (KPC_ASACT |         /* automatic scan on activity */
           KPC_ME | KPC_DE |   /* matrix and direct keypad enabled */
#if defined (CONFIG_KEYPAD_V700)
           (3<<23) | /* 4 columns */
           (6<<26) | /* 7 rows */
#elif defined(CONFIG_E680_P4A)
           (3<<23) | /* 4 columns */
           (2<<26) | /* 3 rows */
           (4<<6) |  /* 4# direct key */
#elif defined(CONFIG_KEYPAD_E680)
           (5<<23) | /* 4 columns */
           (4<<26) | /* 3 rows */
           (5<<6) |  /* 5# direct keys */
#elif defined(CONFIG_KEYPAD_A780)
           (4<<23) | /* 5 columns */
           (4<<26) | /* 5 rows */
           (0<<6) |  /* 1# direct keys */
#endif
           KPC_MS7_0); /* scan all columns */

    CKEN |= CKEN19_KEYPAD;

    err = request_irq (IRQ_KEYPAD, kp_interrupt, 0, "Keypad", NULL);
    if (err)
    {
      printk (KERN_CRIT "can't register IRQ%d for keypad, error %d\n",
	      IRQ_KEYPAD, err);
      CKEN &= ~CKEN19_KEYPAD;
      return -ENODEV;
    }

    if (misc_register (&keypad_misc_device))
    {
       printk(KERN_ERR "Couldn't register keypad driver\n");
       return -EIO;
    }
    if (misc_register (&keypadB_misc_device))
    {
       printk(KERN_ERR "Couldn't register keypadB driver\n");
       misc_deregister(&keypad_misc_device);
       return -EIO;
    }
    if (misc_register (&keypadI_misc_device))
    {
       printk(KERN_ERR "Couldn't register keypadI driver\n");
       misc_deregister(&keypad_misc_device);
       misc_deregister(&keypadB_misc_device);
       return -EIO;
    }

#ifdef USE_RELEASE_TIMER
    init_timer (&key_release_timer);
    key_release_timer.function = release_timer_went_off;
#endif
    init_timer (&autorepeat_timer);
    autorepeat_timer.function = autorepeat_timer_went_off;

    /* 
     * we want the phone to be able to tell the status of the screen
     * lock switch at power-up time
     */
    kpdk = KPDK;         /* read direct key register */
    /*
     * reading the register turns off the "key pressed since last read" bit
     * if it was on, so we turn it off 
     */
    kpdk &= ~KPDK_DKP;
#if defined(CONFIG_E680_P4A)
    if (kpdk & KPDK_DK0) /* VR key is pressed (switch on) */
    {
        turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
    }
    if (kpdk & KPDK_DK4) /* Power key is pressed (switch on) */
    {
        turn_on_bit (keybitmap, KEYPAD_POWER);
    }
#elif defined(CONFIG_KEYPAD_E680)
    if (kpdk & KPDK_DK0) /* VR key is pressed (switch on) */
    {
        turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
    }
    if (kpdk & KPDK_DK4) /* Power key is pressed (switch on) */
    {
        turn_on_bit (keybitmap, KEYPAD_POWER);
    }
    if (kpdk & KPDK_DK3) /* GAME_A key is pressed (switch on) */
    {
        turn_on_bit (keybitmap, KEYPAD_A);
    }
    if (kpdk & KPDK_DK5) /* GAME_B key is pressed (switch on) */
    {
        turn_on_bit (keybitmap, KEYPAD_B);
    }
#elif defined(CONFIG_KEYPAD_A780)
    if (kpdk & KPDK_DK0) /* voice/camera key is pressed */
    {
        turn_on_bit (keybitmap, KEYPAD_CAMERA_VOICE);
    }
#endif

#ifdef CONFIG_PM
    pm_dev = pm_register(PM_SYS_DEV, 0, button_pm_callback);
#if defined(CONFIG_E680_P4A)
/*93,97,100,101,102*/
    PKWR = 0xe4400;			
/*103 104 105 106*/
    PGSR3 |= 0x780;
#endif
#if defined(CONFIG_KEYPAD_E680)
/*93,96,97,98,100,101,102*/    
    PKWR = 0xee400;
/*103 104 105 106*/
    PGSR3 |= 0x780;
#endif
#if defined(CONFIG_KEYPAD_A780)
/*93,97,98,100,101,102*/    
    PKWR = 0xec400;
/*103 104 105 106 107*/
    PGSR3 |= 0xf80;
#endif    
#endif

#if defined(CONFIG_KEYPAD_E680)    
#ifdef CONFIG_PM
	keypadI_pm_dev = pm_register(PM_SYS_DEV, 0, keypadI_pm_callback);
        PGSR3 |= 0x8;
#endif
#endif
	
    KPC |= (KPC_DIE | KPC_MIE);	
    KPKDI = 0x40;
    return 0;
}

static void
__exit keypad_exit(void)
{
#ifdef KDEBUG
    printk(KERN_DEBUG " keypad_exit\n");
#endif
    misc_deregister(&keypad_misc_device);
    misc_deregister(&keypadB_misc_device);
    misc_deregister(&keypadI_misc_device);

#ifdef CONFIG_PM
    pm_unregister(pm_dev);
#endif
    
#if defined(CONFIG_KEYPAD_E680)
#ifdef CONFIG_PM
        pm_unregister(keypadI_pm_dev);
#endif
#endif
	
    CKEN &= ~CKEN19_KEYPAD;
}

module_init (keypad_init);
module_exit (keypad_exit);
