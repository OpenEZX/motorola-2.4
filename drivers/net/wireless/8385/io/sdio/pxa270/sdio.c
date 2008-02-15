/*
 * File: sdio.c 
 * Low level SDIO Driver
 *  
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * (c) Copyright 2006, Motorola, Inc.
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the license.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
 *
 * 2006-Jul-07 Motorola - Add power management related changes.
 */

#include <linux/version.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>

#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <sdiotypes.h>
#define __SDIO_CORE_IMPLEMENTATION__
#include <sdio.h>
#include <sdio_bulverde.h>

#ifdef OLD_CONFIG_PM
#include <linux/pm.h>
#endif

#include <sdioioctl.h>

#include <asm/proc/cache.h>

#ifdef WPRM_DRV
#include "sdio_wprm_drv.h"
#endif
//#define SDIO_CLKRT_RUNNING (MMC_CLKRT_20MHZ)

#ifdef _MAINSTONE
#define _MAINSTONE_GPIO_IRQ
#endif

#ifdef _MAINSTONE_GPIO_IRQ

static inline int start_bus_clock(mmc_controller_t ctrlr);

/* GPIO number used for WAKEUP */
#define WAKEUP_GPIO		22

/* Map it to the Bulverde IRQ Table */
#define	WAKEUP_GPIO_IRQ		IRQ_GPIO(WAKEUP_GPIO)

/* GPIO Interrupt Service Routine */
static void gpio_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	mmc_controller_t ctrlr = (mmc_controller_t) dev_id;

	if (start_bus_clock(ctrlr))
		printk("start_bus_clock() failed in gpio ISR\n");
	else
		printk("SDIO clock started inside the gpio ISR\n");
}

static int init_gpio_irq(mmc_controller_t ctrlr)
{
	set_GPIO_IRQ_edge(WAKEUP_GPIO, GPIO_FALLING_EDGE);

	if (request_irq(WAKEUP_GPIO_IRQ, gpio_irq, 0, "WAKEUP_GPIO_IRQ", ctrlr)) {
		printk("failed to request GPIO%d as WAKEUP_GPIO_IRQ(%d)\n", WAKEUP_GPIO, WAKEUP_GPIO_IRQ);
		return -1;
	}

	return 0;
}

static int remove_gpio_irq(mmc_controller_t ctrlr)
{
	free_irq(WAKEUP_GPIO_IRQ, ctrlr);

	return 0;
}

#endif

/* External functions */
extern int  direct_init(void);
extern void direct_cleanup(void);

unsigned char mode = 4;		//1 for 1-bit mode, 4 for 4-bit mode
unsigned char clkdiv = 0;	//0 for 20MHz, 1 for 10MHz, ...

spinlock_t sdio_lock;

#ifdef DMA_NO_WAIT
static int dma_int_flag = 0;
#endif

static unsigned long int irqnum = 0;
static int ps_sleep_confirmed = 0;
EXPORT_SYMBOL(ps_sleep_confirmed);
EXPORT_SYMBOL(irqnum);

/* MMC controllers registered in the system */
mmc_controller_t mmc_controller[MMC_CONTROLLERS_MAX];
int mmc_ncontrollers = 0;
rwsemaphore_t mmc_controller_sem;        /* controller table lock */
#ifdef OLD_CONFIG_PM
struct pm_dev *mmc_pm_dev = NULL;
#endif

/* users' notification list (defined as global since used by direct) */
mmc_notifier_t mmc_notifier = NULL;
rwsemaphore_t mmc_notifier_sem;  /* notifiers' list lock */

/* RPACE adder: saved stuff for the WLAN driver; 2 devices makes this
   difficult, one device/driver would be better */
static void *mmccard_devid;
static void (*mmccard_handler)(int, void *, struct pt_regs *);
static unsigned int sdio_interrupt_enabled = 0;

#ifdef CONFIG_PROC_FS
proc_dir_entry_t mmc_proc_dir = NULL;
#endif

#ifndef YES_NO
#define YES_NO(bit) ((bit) ? "yes" : "no")
#endif
static inline int __set_state(mmc_controller_t ctrlr, pxa_mmc_state_t state);
static inline int __check_state(mmc_controller_t ctrlr, pxa_mmc_state_t state);

/* RPACE adder */
unsigned long *marvel_debug_hits;
//static int deep_sleep_count; //Counter to keep track of deep sleep calls.

void sdio_print_CMDAT_and_IMASK(void)
{
 printk("MMC_CMDAT =0x%x, MMC_I_MASK=0x%x at %lu\n", MMC_CMDAT, MMC_I_MASK, jiffies);
}
EXPORT_SYMBOL(sdio_print_CMDAT_and_IMASK);

void sdio_enable_SDIO_INT(void)
{
   sdio_interrupt_enabled = 1;
   
}
EXPORT_SYMBOL(sdio_enable_SDIO_INT);


void sdio_clear_imask(mmc_controller_t ctrlr)
{
/*
   // MMC_I_MASK &= ~MMC_I_MASK_CLK_IS_OFF;
   // wmb();
   MMC_STRPCL = MMC_STRPCL_STOP_CLK;
   wmb();
   while (MMC_STAT & MMC_STAT_CLK_EN);
*/
  u32 mask = MMC_I_MASK;
  u32 mask1 = mask & ~MMC_I_MASK_SDIO_INT;
  if(mask != mask1) {
	MMC_I_MASK = mask1;
	wmb();
//	printk("MASK=%x ==> %x, got %x\n",mask,mask1,MMC_I_MASK);
	if(mask1 == MMC_I_MASK) printk("+");
	else printk("-");
	printk("%d",((pxa_mmc_hostdata_t)ctrlr->host_data)->state);
  }
}
EXPORT_SYMBOL(sdio_clear_imask);

void sdio_print_imask(mmc_controller_t ctrlr)
{
/*
 // MMC_I_MASK &= ~MMC_I_MASK_CLK_IS_OFF;
 // wmb();
 MMC_STRPCL = MMC_STRPCL_START_CLK;
 wmb();
 while (!(MMC_STAT & MMC_STAT_CLK_EN));
*/
  printk("MASK=%x ps_sleep_confirmed=%d state=%d\n",MMC_I_MASK,ps_sleep_confirmed,((pxa_mmc_hostdata_t)ctrlr->host_data)->state);
}
EXPORT_SYMBOL(sdio_print_imask);

int sdio_check_idle_state(mmc_controller_t ctrlr)
{
    if(!__check_state(ctrlr, PXA_MMC_FSM_IDLE))
	return 0;
    else
	return ((pxa_mmc_hostdata_t)ctrlr->host_data)->state;
}
EXPORT_SYMBOL(sdio_check_idle_state);
/************************************************
 * service function prototypes and declarations *
 ************************************************/
static inline int acquire_io(mmc_controller_t ctrlr, mmc_card_t card)
{
  int ret = -EIO;

  __ENTER0();
  

  if(!card || !ctrlr) {
    ret = -EINVAL;
    goto error;
  }
#ifdef CONFIG_HOTPLUG
  /* TODO: account for controller removal */
#endif
  down(&ctrlr->io_sem);
#if DMESG
  printk("<7>Acquired Semaphore@ %lu\n",jiffies);
#endif
#if 0
  down_read(&ctrlr->update_sem);        /* FIXME */
  if(card->state != MMC_CARD_STATE_UNPLUGGED)
    ret = 0;
  up_read(&ctrlr->update_sem);

  if(ret)
    up(&ctrlr->io_sem);
#else
  ret = 0;
#endif

error:
  if(ret) printk("acquire_io(): ret=%d\n",ret);
  
  __LEAVE("ret=%d", ret);
  return ret;
}

static inline void release_io(mmc_controller_t ctrlr, mmc_card_t card)
{
  __ENTER0();
  
#ifdef CONFIG_HOTPLUG
  /* TODO: account for controller removal */
#endif
  if(!card && !ctrlr) {         /* FIXME */
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "bad card reference\n");
    goto error;
  }
   up(&ctrlr->io_sem);
#if DMESG
   printk("<7>Released Semaphore@ %lu\n",jiffies);
#endif
error:
  
  __LEAVE0();
}

/* TODO: there should be a separate context to be awaken 
 * by the card intertion interrupt; called under ctrlr->update_sem
 * held down by now */
static int __update_card_stack(mmc_controller_t ctrlr)
{
  int ret = -1;
  mmc_card_t card, prev;
//	printk("<1>Enter __update_card_stack\n");
  __ENTER0();
	printk("<5>Enter __update_card_stack\n");
  

  if(!ctrlr || !ctrlr->tmpl){
//	  printk("<1>ctrlr->tmpl failed\n");
	  printk("<5>ctrlr->tmpl failed\n");
    goto error;
  }
  /* check unplugged cards first... */
  if((ret = ctrlr->tmpl->check_card_stack(ctrlr))){
	  printk("<1>check_card_stack failed\n");
    goto error;
  }

  /* unregister unplugged cards and free 'em immediately */
  if(ctrlr->stack.ncards > 0) {
    prev = ctrlr->stack.first;
    /* process the stack tail first */
    if(prev->next) {
      card = prev->next;
      while(card) {
        if(card->state == MMC_CARD_STATE_UNPLUGGED) {
          if(ctrlr->stack.selected == card)
            ctrlr->stack.selected = NULL;
#ifdef CONFIG_PROC_FS
          if(card->proc) {
            remove_proc_entry(card->proc_name, ctrlr->proc);
            card->proc = NULL;
          }
#endif
          ctrlr->slot_next = card->slot;        /* FIXME */
          prev->next = card->next;
          if(ctrlr->stack.last == card)
            ctrlr->stack.last = prev;
          /* FIXME: controller use count */
          notify_remove(card);
          --ctrlr->stack.ncards;
          if((ctrlr->usage > 0) && ctrlr->tmpl->owner) {
            --ctrlr->usage;
            MMC_DEBUG(MMC_DEBUG_LEVEL2, "'%s' use count decreased (%d)\n",
		      ctrlr->tmpl->name, ctrlr->usage);
//            __MOD_DEC_USE_COUNT(ctrlr->tmpl->owner);
          }
          __card_free(card);
	  
          card = prev->next;
        }
      }
    }
    /* then the head */
    card = ctrlr->stack.first;
    if(card && (card->state == MMC_CARD_STATE_UNPLUGGED)) {
      if(ctrlr->stack.selected == card)
        ctrlr->stack.selected = NULL;
#ifdef CONFIG_PROC_FS
      if(card->proc) {
        remove_proc_entry(card->proc_name, ctrlr->proc);
        card->proc = NULL;
      }
#endif
      ctrlr->slot_next = card->slot;    /* FIXME */
      notify_remove(card);  /* FIXME: should unregister here */
      ctrlr->stack.first = card->next;
      if(ctrlr->stack.last == card)
        ctrlr->stack.last = NULL;
      /* FIXME: controller use count */
      --ctrlr->stack.ncards;
      if((ctrlr->usage > 0) && ctrlr->tmpl->owner) {
        --ctrlr->usage;
        MMC_DEBUG(MMC_DEBUG_LEVEL2, "'%s' use count decreased (%d)\n",
		  ctrlr->tmpl->name, ctrlr->usage);
//        __MOD_DEC_USE_COUNT(ctrlr->tmpl->owner);
      }
      __card_free(card);
    }
  }
  MMC_DEBUG(MMC_DEBUG_LEVEL2, "after stack check: ncards=%d"
            " first=0x%p last=0x%p\n",
	    ctrlr->stack.ncards, ctrlr->stack.first, ctrlr->stack.last);
  /* ...then add newly inserted ones */
  if((ret = ctrlr->tmpl->update_acq(ctrlr)))
    goto error;
  /* ret = 0; */
error:
  printk("<1>Leave __update_card_stack\n");
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}

/*
 * 	1) check error code returned by controller; it's up to
 * 	   controller to detect error conditions reported by the card
 * 	   and to abort data transfer requests properly (e.g. send
 * 	   CMD12(STOP_TRANSMISSION) to abort ADDRESS_ERROR multiple
 * 	   block transfers)
 * 	2) arrange for card stack update when necessary
 *	   (all pending i/o requests must be held pending,
 *	   update procedure must start immediately after 
 *	   error has been detected)
 */
static inline int __check_error(mmc_card_t card, int err)
{
  int ret = -EIO;
  mmc_controller_t ctrlr;

  __ENTER0();
  if(!card || !card->ctrlr)
    goto error;

  ctrlr = card->ctrlr;

  if(err < 0) {
    switch (err) {
      /* bus error occurred */
    case MMC_ERROR_CRC_WRITE_ERROR:
    case MMC_ERROR_CRC_READ_ERROR:
    case MMC_ERROR_RES_CRC_ERROR:
    case MMC_ERROR_READ_TIME_OUT:
    case MMC_ERROR_TIME_OUT_RESPONSE:
      down_write(&ctrlr->update_sem);   /* FIXME */
      if(!__update_card_stack(ctrlr))
        ret = -ENXIO;
      up_write(&ctrlr->update_sem);
      break;
    default:
      /* generic I/O error */
      break;
    }
  } else
    ret = err;

error:
  if(ret) printk("check_error(): ret=%d\n",ret);
  __LEAVE("ret=%d", ret);
  
  return ret;
}

static inline void __free_controller(mmc_controller_t ctrlr)
{
  if(ctrlr) {
    if(ctrlr->stack.ncards > 0)
      __card_stack_free(&ctrlr->stack);
    kfree(ctrlr);
  }
}

#ifdef CONFIG_PROC_FS
static int proc_read_card_info(char *page, char **start, off_t off,
				    int count, int *eof, void *data)
{
  int ret = -EINVAL;
  mmc_card_t card = (mmc_card_t) data;
  char *cp = page;
  int i;

  unsigned char cccrsdio, sdspec, bic, cap;

  if(!card)
    goto error;

  down_read(&card->ctrlr->update_sem);
  cp += sprintf(cp, "OCR: 0x%x\n", card->info.ocr);
  cp += sprintf(cp, "Number of I/O Functions: %d\n", card->info.nio);
  cp += sprintf(cp, "Memory Present: %s\n", YES_NO(card->info.mem));
  cp += sprintf(cp, "Relative Card Address: 0x%.4x\n", card->info.rca);
  for(i = 0; i < 8 && i <= card->info.nio; i++) {
    cp += sprintf(cp, "  FN%d  BS: %d\n", i, card->info.fnblksz[i]);
    cp += sprintf(cp, "  FN%d CIS: 0x%x\n", i, card->info.cisptr[i]);
  }
  up_read(&card->ctrlr->update_sem);

  ret = sdio_read_ioreg(card, 0, CCCR_SDIO_REV_REG, &cccrsdio);
  if(ret == 0) {
    cp += sprintf(cp, "CCCR Format Version: ");
    switch(CCCR_FORMAT_VERSION(cccrsdio)) {
    case 0x00: cp += sprintf(cp, "1.00"); break;
    default:   cp += sprintf(cp, "Unknown (0x%x)",
			     CCCR_FORMAT_VERSION(cccrsdio));
      break;
    }
    cp += sprintf(cp, "\n");
    cp += sprintf(cp, "SDIO Spec Revision: ");
    switch(SDIO_SPEC_REVISION(cccrsdio)) {
    case 0x00: cp += sprintf(cp, "1.00"); break;
    default:   cp += sprintf(cp, "Unknown (0x%x)",
			     SDIO_SPEC_REVISION(cccrsdio));
      break;
    }
    cp += sprintf(cp, "\n");
  } else {
    cp += sprintf(cp, "CCCR_SDIO_REV_REG read error!\n");
  }
  ret = sdio_read_ioreg(card, 0, SD_SPEC_REV_REG, &sdspec);
  if(ret == 0) {
    cp += sprintf(cp, "SD Format Version: ");
    switch(SD_PHYS_SPEC_VERSION(sdspec)) {
    case 0x00: cp += sprintf(cp, "1.01 (March 2000)");  break;
    default:   cp += sprintf(cp, "Unknown (0x%x)",
			     SD_PHYS_SPEC_VERSION(sdspec));
      break;
    }
    cp += sprintf(cp, "\n");
  } else {
    cp += sprintf(cp, "SD_SPEC_REV_REG read error!\n");
  }
  ret = sdio_read_ioreg(card, 0, BUS_INTERFACE_CONTROL_REG, &bic);
  if(ret == 0) {
    cp += sprintf(cp, "Bus width: %d bits\n", BUS_WIDTH(bic) ? 4 : 1);
  } else {
    cp += sprintf(cp, "BUS_INTERFACE_CONTROL_REG read error!\n");
  }
  ret = sdio_read_ioreg(card, 0, CARD_CAPABILITY_REG, &cap);
  if(ret == 0) {
    cp += sprintf(cp, "SDC (Direct Command): %s\n", YES_NO(cap & SDC));
    cp += sprintf(cp, "SMB (Multi-Block): %s\n", YES_NO(cap & SMB));
    cp += sprintf(cp, "SRW (Read Wait): %s\n", YES_NO(cap & SRW));
    cp += sprintf(cp, "S4MI (4 bit Mode Interrupt): %s\n", YES_NO(cap & S4MI));
    cp += sprintf(cp, "Speed: %s\n", ((cap & LSC) ? "Low" : "Full"));
    cp += sprintf(cp, "4BLS (4 bit Low Speed): %s\n", YES_NO(cap & S4BLS));
  } else {
    cp += sprintf(cp, "CARD_CAPABILITY_REG read error!\n");
  }
#undef  YES_NO

  ret = cp - page;
error:
  return ret;
}
#endif

/**
 * After we initialize the card, we need to read some important registers
 * out and save those values in the card info data structure.
 */
static int read_important_regs(mmc_card_t card) {
  int ret = 0;
  mmc_controller_t ctrlr;
  int fn;
  u8 dat;
  ctrlr = card->ctrlr;

  

  /* Read function block sizes */
  for(fn = 0; fn < 8 && fn <= card->info.nio; fn++) {
    card->info.fnblksz[fn] = 0x0;

    /* Read low byte */
    ret = sdio_read_ioreg(card, FN0, FN_BLOCK_SIZE_0_REG(fn), &dat);
    if(ret < 0)
      goto error;
    card->info.fnblksz[fn] |= (((u16) dat) << 0);

    /* Read high byte */
    ret = sdio_read_ioreg(card, FN0, FN_BLOCK_SIZE_1_REG(fn), &dat);
    if(ret < 0)
      goto error;
    card->info.fnblksz[fn] |= (((u16) dat) << 8);
  }

  /* Read the CIS base address of each function */
  for(fn = 0; fn < 8 && fn <= card->info.nio; fn++) {
    card->info.cisptr[fn] = 0x0;
    
    /* Read [0:7] byte */
    ret = sdio_read_ioreg(card, FN0, FN_CIS_POINTER_0_REG(fn), &dat);
    if(ret < 0)
      goto error;
    card->info.cisptr[fn] |= (((u32) dat) << 0);

    /* Read [8:15] byte */
    ret = sdio_read_ioreg(card, FN0, FN_CIS_POINTER_1_REG(fn), &dat);
    if(ret < 0)
      goto error;
    card->info.cisptr[fn] |= (((u32) dat) << 8);
    
    /* Read [16:23] byte */
    ret = sdio_read_ioreg(card, FN0, FN_CIS_POINTER_2_REG(fn), &dat);
    if(ret < 0)
      goto error;
    card->info.cisptr[fn] |= (((u32) dat) << 16);
  }

 error:

  
  return ret;
}

/*************************************
 * SDIO core interface implementation *
 *************************************/
int notify_add(mmc_card_t card)
{
  int ret = 0;
  mmc_notifier_t notifier;

  __ENTER0();
  /* RPACE adder */
  

  if(card) {
    for(notifier = mmc_notifier; notifier; notifier = notifier->next)
      if(notifier->add)
        if((ret = notifier->add(card)))
          break;
  }
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}
EXPORT_SYMBOL(notify_add);

int notify_remove(mmc_card_t card)
{
  int ret = 0;
  mmc_notifier_t notifier;

  __ENTER0();
  /* RPACE adder */
  
  if(card) {
    for(notifier = mmc_notifier; notifier; notifier = notifier->next)
      if(notifier->remove)
        if((ret = notifier->remove(card)))
          break;
  }
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}
EXPORT_SYMBOL(notify_remove);

int update_card_stack(int host)
{
  int ret = -EINVAL;
  mmc_controller_t ctrlr;

  __ENTER0();
  /* RPACE adder */
  

  if((host < 0) || (host >= MMC_CONTROLLERS_MAX))
    goto error;

  down_read(&mmc_controller_sem);
  if((ctrlr = mmc_controller[host])) {
    down_write(&ctrlr->update_sem);
    (void) __update_card_stack(ctrlr);
    up_write(&ctrlr->update_sem);
  }
  up_read(&mmc_controller_sem);
  ret = 0;
error:
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}
EXPORT_SYMBOL(update_card_stack);

static inline int __check_ioreg_args(mmc_card_t card, u8 func, u32 reg) {

  /* Check function number range */
  if(func > 7 || func > card->info.nio) {
    return -EINVAL;
  }
  /* Check register range */
  if((reg & 0xfffe0000) != 0x0) {
    return -EINVAL;
  }
  return 0;
}

static inline int __rw_ioreg(mmc_card_t card, ioreg_req_t ioreg) {
  ssize_t ret = -EIO;
  mmc_controller_t ctrlr;
  unsigned long flags;

  spin_lock_irqsave(&sdio_lock, flags);

  if(ps_sleep_confirmed
	&& !(ioreg->rw == 1 && ioreg->func == 1 && ioreg->reg == 3 && ioreg->dat == 2)
//	&& !(ioreg->rw == 0 && ioreg->func == 1 && ioreg->reg == 5)
	) {
    printk("__rw_ioreg(%p %d %s) rw=%d func=%d reg=%x dat=%d  SLEEP_CONFIRMED\n",current,current->pid,current->comm,ioreg->rw,ioreg->func,ioreg->reg,ioreg->dat);
    ret = -ENODEV;
    goto error;
  }

  if(!card) {
    ret = -ENODEV;
    goto error;
  }

  if(__check_ioreg_args(card, ioreg->func, ioreg->reg) < 0) {
	  printk("<1>__check_ioreg_args failed - __rw_ioreg\n");
    ret = -EINVAL;
    goto error;
  }

  ctrlr = card->ctrlr;
  if((ret = acquire_io(ctrlr, card))){
	  printk("<1>acquire io failed - __rw_ioreg\n");
    	goto error;
  }
  if(ctrlr->stack.selected != card) {
    if((ret = ctrlr->tmpl->setup_card(ctrlr, card))) {
	  printk("<1>setup_card failed - __rw_ioreg\n");
#ifdef SDIO_DEBUG1
	  PRINTK2("setup_card failed - __rw_ioreg\n");
#endif
      goto err_mmc;
      }
    ctrlr->stack.selected = card;
  }

  if(ctrlr->tmpl->rw_ioreg(ctrlr, ioreg) < 0) {
#ifdef SDIO_DEBUG1
	  PRINTK2("rw_ioreg failed -__rw_ioreg\n");
#endif
	  printk("rw_ioreg failed -__rw_ioreg\n");
    ret = -ENXIO;
    goto err_down;
  }

  ret = 0;

err_mmc:
  ret = __check_error(card, ret);
  if(ret >= 0) {
    /* do nothing */
  }
err_down:
  release_io(ctrlr, card);
error:
  
  spin_unlock_irqrestore(&sdio_lock, flags);

  return ret;
}

int sdio_read_ioreg(mmc_card_t card, u8 func, u32 reg, u8 *dat)
{
  int ret;
  ioreg_req_rec_t ioreg;

  __ENTER("card=%p usage=%d func=%d reg=%d",
          card, card->usage, func, reg);

  ioreg.rw = IOREG_READ;
  ioreg.func = func;
  ioreg.reg = reg;
  ioreg.dat = 0x0;

  ret = __rw_ioreg(card, &ioreg);
  *dat = ioreg.dat;

  __LEAVE("ret=%d\n", ret);
  return ret;
}
EXPORT_SYMBOL(sdio_read_ioreg);

int sdio_write_ioreg(mmc_card_t card, u8 func, u32 reg, u8 dat)
{
  int ret;
  ioreg_req_rec_t ioreg;

  __ENTER("card=%p usage=%d func=%d reg=%d", card, card->usage, func, reg);

  ioreg.rw = IOREG_WRITE;
  ioreg.func = func;
  ioreg.reg = reg;
  ioreg.dat = dat;

  ret = __rw_ioreg(card, &ioreg);
  
  __LEAVE("ret=%d", ret);
  return ret;
}
EXPORT_SYMBOL(sdio_write_ioreg);

int write_sdio_read_ioreg(mmc_card_t card, u8 func, u32 reg, u8 *dat) {
  int ret;
  ioreg_req_rec_t ioreg;

  __ENTER("card=%p usage=%d func=%d reg=%d", card, card->usage, func, reg);

  ioreg.rw = IOREG_WRITE_READ;
  ioreg.func = func;
  ioreg.reg = reg;
  ioreg.dat = *dat;

  ret = __rw_ioreg(card, &ioreg);
  *dat = ioreg.dat;

  __LEAVE("ret=%d\n", ret);
  return ret;
}
EXPORT_SYMBOL(write_sdio_read_ioreg);

static inline int __check_iomem_args(mmc_card_t card,
					  u8 func, u32 reg,
					  u8 blockmode, u8 opcode,
					  ssize_t cnt, ssize_t blksz, u8 *dat) {
  /* Check function number range */
  if(func > 7 || func > card->info.nio) {
    return -ERANGE;
  }
  /* Check register range */
  if((reg & 0xfffe0000) != 0x0) {
    return -ERANGE;
  }
  /* Check cnt range */
  if(cnt == 0 || (cnt & 0xfffffe00) != 0x0) {
    return -ERANGE;
  }
  /* Check blksz range */
  if(blockmode && (blksz == 0 || blksz > 0x0800)) {
    return -ERANGE;
  }
  /* Check null-pointer */
  if(dat == 0x0) {
    return -EINVAL;
  }
  return 0;
}

static inline int __rw_iomem(mmc_card_t card, data_transfer_req_t iomem) {
  ssize_t ret = -EIO;
  mmc_controller_t ctrlr;
  ioreg_req_rec_t ioreg;
  unsigned long flags;

  spin_lock_irqsave(&sdio_lock, flags);
  
 //printk("enter __rw_iomem\n");
  if(ps_sleep_confirmed) {
    printk("__rw_iomem(%p %d %s) rw=%d func=%d reg=%x cnt=%d  SLEEP_CONFIRMED\n",current,current->pid,current->comm,iomem->rw,iomem->func,iomem->reg,iomem->cnt);
    ret = -ENODEV;
    goto error;
  }

  if(!card) {
    ret = -ENODEV;
    goto error;
  }
  ret = __check_iomem_args(card, iomem->func, iomem->reg, 
				iomem->blockmode, iomem->opcode,
				iomem->cnt, iomem->blksz, iomem->buf);
  if(ret < 0)
    goto error;
  
  ctrlr = card->ctrlr;
  if((ret = acquire_io(ctrlr, card)))
    goto error;

  if(ctrlr->stack.selected != card) {
    if((ret = ctrlr->tmpl->setup_card(ctrlr, card)))
      goto err_mmc;
    ctrlr->stack.selected = card;
  }

  /* Set the correct block size in the register space if neccessary (CMD52) */
  if(iomem->blockmode &&
     card->info.fnblksz[iomem->func] != iomem->blksz) {

    MMC_DEBUG(MMC_DEBUG_LEVEL1, "__rw_iomem: blkmode=0x%x blksz=%d fnblksz=%d\n", iomem->blockmode, iomem->blksz, card->info.fnblksz[iomem->func]);

    //printk("__rw_iomem: blkmode=0x%x blksz=%d fnblksz=%d\n", iomem->blockmode, iomem->blksz, card->info.fnblksz[iomem->func]);

    /* Write low byte */
    ioreg.rw = 1;
    ioreg.func = FN0;
    ioreg.reg = FN_BLOCK_SIZE_0_REG(iomem->func);
    ioreg.dat = (u8) (iomem->blksz & 0x00ff);
    if(ctrlr->tmpl->rw_ioreg(ctrlr, &ioreg) < 0) {
	printk("rw_ioreg failed rw_iomem\n");
      ret = -ENXIO;
      goto err_down;
    }

    /* Write high byte */
    ioreg.rw = 1;
    ioreg.func = FN0;
    ioreg.reg = FN_BLOCK_SIZE_1_REG(iomem->func);
    ioreg.dat = (u8) (iomem->blksz >> 8);
    if(ctrlr->tmpl->rw_ioreg(ctrlr, &ioreg) < 0) {
	printk("rw_ioreg failed rw_iomem 1\n");
      ret = -ENXIO;
      goto err_down;
    }

    card->info.fnblksz[iomem->func] = iomem->blksz;
  }

  /* Perform the actual data transfer (CMD53) */
  if((ret = ctrlr->tmpl->rw_iomem(ctrlr, iomem)) < 0) {
	  printk("<1>rw_iomem failed ret=%d\n",ret);
    	printk("__rw_iomem: blkmode=0x%x blksz=%d fnblksz=%d\n", iomem->blockmode, iomem->blksz, card->info.fnblksz[iomem->func]);
	   __set_state(ctrlr,PXA_MMC_FSM_END_IO);
    ret = -ENXIO;
    goto err_down;
    //goto err_mmc;
  }

  ret = 0;

err_mmc:
  ret = __check_error(card, ret);
  if(ret >= 0) {
    /* do nothing */
  }
err_down:
  release_io(ctrlr, card);
error:
  if(ret) printk("__rw_iomem(): ret=%d\n",ret);
  
  spin_unlock_irqrestore(&sdio_lock, flags);

 //printk("Leave: __rw_iomem\n");
  return ret;
}

int sdio_read_iomem(mmc_card_t card, u8 func, u32 reg,
		    u8 blockmode, u8 opcode,
		    ssize_t cnt, ssize_t blksz, u8 *dat)
{
  int ret;
  data_transfer_req_rec_t iomem;

  __ENTER("card=%p usage=%d func=%d reg=0x%x, mode=%s opcode=%s cnt=%d",
          card, card->usage, func, reg,
	  blockmode ? "block" : "byte", opcode ? "fixed" : "incremental", cnt);

  
  
  iomem.rw = IOMEM_READ;
  iomem.func = func;
  iomem.blockmode = (blockmode ? BLOCK_MODE : BYTE_MODE);
  iomem.opcode = (opcode ? INCREMENTAL_ADDRESS : FIXED_ADDRESS);
  iomem.reg = reg;
  iomem.cnt = cnt;
  iomem.blksz = blksz;
  iomem.type = MMC_KERNEL;
  iomem.buf = dat;

  ret = __rw_iomem(card, &iomem);
  
  __LEAVE("ret=%d\n", ret);
  return ret;
}
EXPORT_SYMBOL(sdio_read_iomem);

int sdio_write_iomem(mmc_card_t card, u8 func, u32 reg,
		     u8 blockmode, u8 opcode,
		     ssize_t cnt, ssize_t blksz, u8 *dat)
{
  int ret;
  data_transfer_req_rec_t iomem;

  __ENTER("card=%p usage=%d func=%d reg=0x%x, mode=%s opcode=%s cnt=%d",
          card, card->usage, func, reg,
	  blockmode ? "block" : "byte", opcode ? "fixed" : "incremental", cnt);
	//printk("enter sdio_write_iomem\n");
/*  printk("card=%p usage=%d func=%d reg=0x%x, mode=%s opcode=%s cnt=%d",
          card, card->usage, func, reg,
	  blockmode ? "block" : "byte", opcode ? "fixed" : "incremental", cnt);*/

  

  iomem.rw = IOMEM_WRITE;
  iomem.func = func;
  iomem.blockmode = (blockmode ? BLOCK_MODE : BYTE_MODE);
  iomem.opcode = (opcode ? INCREMENTAL_ADDRESS : FIXED_ADDRESS);
  iomem.reg = reg;
  iomem.cnt = cnt;
  iomem.blksz = blksz;
  iomem.type = MMC_KERNEL;
  iomem.buf = dat;
  
  ret = __rw_iomem(card, &iomem);
  
  __LEAVE("ret=%d", ret);
	//printk("Leave sdio_write_iomem\n");
  return ret;
}
EXPORT_SYMBOL(sdio_write_iomem);

/* 
 * registry stuff 
 */
mmc_card_t get_card(int host, int slot)
{
  mmc_card_t ret = NULL;
  mmc_controller_t ctrlr = NULL;
  int found;

  __ENTER("host=%d, card=%d", host, slot);
  /* RPACE adder */
  

  if(((host < 0) || (host >= MMC_CONTROLLERS_MAX)) &&
     ((slot < 0) || (slot >= MMC_CARDS_MAX)))
    goto error;

  down_read(&mmc_controller_sem);

  if((ctrlr = mmc_controller[host])) {
    down_write(&ctrlr->update_sem);
    if(ctrlr->stack.ncards > 0) {
      ret = ctrlr->stack.first;
      found = FALSE;
      while(ret) {
        if((ret->slot == slot) && (ret->state != MMC_CARD_STATE_UNPLUGGED)) {
          found = TRUE;
          break;
        }
        ret = ret->next;
      }
      
      if(found) {
        if(ctrlr->tmpl->owner) {
          ++ctrlr->usage;
          MMC_DEBUG(MMC_DEBUG_LEVEL2, "'%s' use count increased (%d)\n",
                    ctrlr->tmpl->name, ctrlr->usage);
//          __MOD_INC_USE_COUNT(ctrlr->tmpl->owner);
        }
        ++ret->usage;
      } else
        ret = NULL;
    }
    up_write(&ctrlr->update_sem);
  }
  up_read(&mmc_controller_sem);
error:
  /* RPACE adder */
  
  __LEAVE("ret=0x%p usage=%d", ret, ret ? ret->usage : -1);
  return ret;
}
EXPORT_SYMBOL(get_card);

void put_card(mmc_card_t card)
{
  mmc_card_t tmp = NULL;
  mmc_controller_t ctrlr;
  /* RPACE, initial definition added for debug compile */
  int found=FALSE;

  __ENTER("card=0x%p", card);
  /* RPACE adder */
  

  if(!card)
    goto error;

  ctrlr = card->ctrlr;

  down_read(&mmc_controller_sem);
  if(!ctrlr || (ctrlr != mmc_controller[ctrlr->slot])) {
    MMC_ERROR("bad controller reference: ctrlr=0x%p\n", ctrlr);
    goto err_down;
  }

  down_write(&ctrlr->update_sem);
  if(ctrlr->stack.ncards > 0) {
    tmp = ctrlr->stack.first;
    found = FALSE;
    while(tmp) {
      if(tmp == card) {
        found = TRUE;
        break;
      }
      tmp = tmp->next;
    }

    if(found) {
      if(tmp->usage > 0) {
        --tmp->usage;
        MMC_DEBUG(MMC_DEBUG_LEVEL2, "usage=%d"
                  "owner=0x%p\n", tmp->usage, ctrlr->tmpl->owner);
        if(!tmp->usage && (ctrlr->usage > 0)
           && ctrlr->tmpl->owner) {
          --ctrlr->usage;
          MMC_DEBUG(MMC_DEBUG_LEVEL2,
                    "'%s' use count "
                    "decreased (%d)\n", ctrlr->tmpl->name, ctrlr->usage);
//          __MOD_DEC_USE_COUNT(ctrlr->tmpl->owner);
        }
      }
    } else
      MMC_DEBUG(MMC_DEBUG_LEVEL0, "bad card reference\n");

  }
  up_write(&ctrlr->update_sem);
err_down:
  up_read(&mmc_controller_sem);
error:
  /* RPACE adder */
  
  __LEAVE("found=%d", found);
  return;
}
EXPORT_SYMBOL(put_card);

static inline void *register_user(mmc_notifier_t notifier)
{
  mmc_notifier_t ret = NULL, last = mmc_notifier;

//  MOD_INC_USE_COUNT;

  if(notifier) {
    down_write(&mmc_notifier_sem);

    notifier->next = NULL;
    if(!last) {
      mmc_notifier = notifier;
      ret = notifier;
    } else {
      while(last->next) {
        if(last == notifier) {
//          MOD_DEC_USE_COUNT;
          break;
        }
        last = last->next;
      }
      if(last != notifier) {
        last->next = notifier;
        ret = notifier;
      }
    }
    up_write(&mmc_notifier_sem);
  }
  /* notify new user about the cards present in the system */
  if(ret && ret->add) {
    int i;

    down_read(&mmc_controller_sem);
    for(i = 0; i < mmc_ncontrollers && ret != NULL; i++) {
      mmc_controller_t ctrlr = mmc_controller[i];

      down_read(&ctrlr->update_sem);    /* FIXME */
      if(__card_stack_foreach(&ctrlr->stack, ret->add, FALSE) < 0) {
	      ret = NULL;
	      printk("<1> __card_stack_foreach Fail\n");
      }
      up_read(&ctrlr->update_sem);      /* FIXME */
    }
    up_read(&mmc_controller_sem);
  }
  
  /* error: */
  __LEAVE("mmc_notifier=0x%p, mmc_notifier->next=0x%p",
          mmc_notifier, mmc_notifier ? mmc_notifier->next : NULL);
  return ret;
}

static inline mmc_controller_t register_controller(mmc_controller_tmpl_t
							tmpl, size_t extra)
{
  mmc_controller_t ret = NULL;
  int found;
  int i;

//  MOD_INC_USE_COUNT;

  /* RPACE adder */
  

  down_write(&mmc_controller_sem);

  if(mmc_ncontrollers >= MMC_CONTROLLERS_MAX) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "there're too many controllers\n");
    goto error;
  }

  found = FALSE;
  for(i = 0; i < MMC_CONTROLLERS_MAX; i++)
    if(!mmc_controller[i]) {
      found = TRUE;
      break;
    }

  if(!found) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "there're no empty slots\n");
    goto error;
  }

  if(!tmpl->init) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "host template lacks 'init()'\n");
    goto error;
  }

  if(!tmpl->probe) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "host template lacks 'probe()'\n");
    goto error;
  }

  if(!tmpl->init_card_stack) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "host template lacks 'init_card_stack()'\n");
    goto error;
  }

  if(!tmpl->update_acq) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "host template lacks 'update_acq()'\n");
    goto error;
  }

  if(!tmpl->check_card_stack) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "host template lacks 'check_card_stack()'\n");
    goto error;
  }

  if(!tmpl->setup_card) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "host template lacks 'setup_card()'\n");
    goto error;
  }

  ret = kmalloc(sizeof(mmc_controller_rec_t) + extra, GFP_ATOMIC);      /* FIXME: ISA + GFP_DMA */
  if(!ret) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "out of memory\n");
    goto error;
  }

  memset(ret, 0, sizeof(mmc_controller_rec_t) + extra);

  if((tmpl->probe(ret) != 1)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "controller probe failed\n");
    goto err_free;
  }

  if(tmpl->init(ret)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "controller initialization failure\n");
    goto err_free;
  }

  ret->state = MMC_CONTROLLER_FOUND;
  ret->slot = i;
  ret->tmpl = tmpl;
  init_MUTEX(&ret->io_sem);
  init_rwsem(&ret->update_sem);
#ifdef CONFIG_PROC_FS
  if(mmc_proc_dir) {
    snprintf(ret->proc_name, sizeof(ret->proc_name), "host%d", ret->slot);
    ret->proc = proc_mkdir(ret->proc_name, mmc_proc_dir);
  }
#endif

  /* initialize card stack */
  if(ret->tmpl->init_card_stack(ret)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "card stack initialization failure\n");
    /*RPACE adder */
    //printk("<5>card stack initialization failure =0x%x\n",ret);
    if(ret->tmpl->remove)
      ret->tmpl->remove(ret);   /* FIXME */
    goto err_free;
  }

  mmc_controller[ret->slot] = ret;
  ++mmc_ncontrollers;

  /* notify users */
  if(ret->stack.ncards > 0) {
    down_read(&mmc_notifier_sem);
    if((i = __card_stack_foreach(&ret->stack, notify_add, FALSE)) < 0)
      MMC_ERROR("device added notification %d\n", -i);
    up_read(&mmc_notifier_sem);
  }
  goto out;

err_free:
#ifdef CONFIG_PROC_FS
  if(ret->proc)
    remove_proc_entry(ret->proc_name, mmc_proc_dir);
#endif
  kfree(ret);
error:
  ret = NULL;
  MOD_DEC_USE_COUNT;
out:
  up_write(&mmc_controller_sem);
  /* RPACE adder */
  
  return ret;
}

static inline mmc_card_t register_card(mmc_card_t card)
{
  mmc_card_t ret = NULL;
  mmc_controller_t ctrlr;

  

  if(!card || !card->ctrlr)
    goto error;

  ctrlr = card->ctrlr;
#ifdef CONFIG_PROC_FS
  if(ctrlr->proc) {
    snprintf(card->proc_name, sizeof(card->proc_name), "card%d", card->slot);
    card->proc = create_proc_read_entry(card->proc_name,
                                        0444, ctrlr->proc,
                                        proc_read_card_info, card);
  }
#endif

  read_important_regs(card);
  notify_add(card);

error:
  return ret;
}

void *sdio_register(mmc_reg_type_t reg_type, void *tmpl, size_t extra)
{
  void *ret = NULL;

  /* RPACE adder */
  

  switch (reg_type) {
  case MMC_REG_TYPE_CARD:
    ret = register_card((mmc_card_t) tmpl);
    break;

  case MMC_REG_TYPE_USER:
    ret = register_user((mmc_notifier_t) tmpl);
    break;

  case MMC_REG_TYPE_HOST:
    ret = register_controller((mmc_controller_tmpl_t) tmpl, extra);
    break;

  default:
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "register request for unknown type\n");
  }

  /* RPACE adder */
  

  return ret;
}
EXPORT_SYMBOL(sdio_register);

static inline void unregister_user(mmc_notifier_t notifier)
{
  mmc_notifier_t prev = mmc_notifier;
  int found = FALSE;

  /* RPACE adder */
  if(notifier) {
    down_write(&mmc_notifier_sem);

    if(mmc_notifier == notifier) {
      mmc_notifier = prev->next;
      found = TRUE;

    } else if(mmc_notifier) {
      while(prev) {
        if(prev->next == notifier) {
          found = TRUE;
          prev->next = prev->next->next;
          break;
        }
        prev = prev->next;
      }
    }

    if(found) {
      if(notifier->remove) {
        int i;

        down_read(&mmc_controller_sem);
        for(i = 0; i < mmc_ncontrollers; i++) {
          mmc_controller_t ctrlr = mmc_controller[i];

          down_read(&ctrlr->update_sem);
          __card_stack_foreach(&ctrlr->stack, notifier->remove, FALSE);
          up_read(&ctrlr->update_sem);
        }
        up_read(&mmc_controller_sem);
      }
    }

    up_write(&mmc_notifier_sem);
  }

//  MOD_DEC_USE_COUNT;
  /* RPACE adder */
  
}

static inline void unregister_controller(mmc_controller_t ctrlr)
{
  /* RPACE adder */

  if(!ctrlr || (mmc_controller[ctrlr->slot] != ctrlr)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "bad unregister request\n");
    goto error;
  }

  down_write(&mmc_controller_sem);

  /* notify users */
  if(ctrlr->stack.ncards > 0) {
    int slot;
#ifdef CONFIG_PROC_FS
	mmc_card_t card;
	card = ctrlr->stack.first;
	if(card->proc) {
        remove_proc_entry(card->proc_name, ctrlr->proc);
        card->proc = NULL;
	
     }
#endif 	
    down_read(&mmc_notifier_sem);
    if((slot = __card_stack_foreach(&ctrlr->stack, notify_remove, FALSE)))
      MMC_ERROR("device remove notification failed at slot %d\n", -slot);
    up_read(&mmc_notifier_sem);
  }
#ifdef CONFIG_PROC_FS
  if(ctrlr->proc)
    remove_proc_entry(ctrlr->proc_name, mmc_proc_dir);
#endif

  if(ctrlr->tmpl && ctrlr->tmpl->remove)
    ctrlr->tmpl->remove(ctrlr);

  mmc_controller[ctrlr->slot] = NULL;
  --mmc_ncontrollers;

  __free_controller(ctrlr);

  up_write(&mmc_controller_sem);
//  MOD_DEC_USE_COUNT;
error:
  /* RPACE adder */
//    sdio_write_ioreg(ctrlr->stack.selected, FN0, IO_ABORT_REG, 0x08);

  return;
}

void sdio_unregister(mmc_reg_type_t reg_type, void *tmpl)
{
  /* RPACE adder */
  
//  printk(__FUNCTION__);

  switch (reg_type) {
  case MMC_REG_TYPE_USER:
    unregister_user((mmc_notifier_t) tmpl);
    break;

  case MMC_REG_TYPE_HOST:
    unregister_controller((mmc_controller_t) tmpl);
    break;

  default:
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "unregister request for unknown type\n");
  }
  /* RPACE adder */
  
}
EXPORT_SYMBOL(sdio_unregister);

#ifdef OLD_CONFIG_PM
/* power management support */
static int sdpm_callback(struct pm_dev *pmdev, pm_request_t pmreq,
			    void *pmdata)
{
  int ret = -EINVAL;
  mmc_controller_t ctrlr;
  int i;

  __ENTER("pmreq=%d", pmreq);

  down_read(&mmc_controller_sem);

  switch (pmreq) {
  case PM_SUSPEND:
    for(ret = 0, i = 0; !ret && (i < mmc_ncontrollers); i++) {
      ctrlr = mmc_controller[i];
      if(ctrlr->tmpl->suspend)
        ret = ctrlr->tmpl->suspend(ctrlr);
    }
    if(!ret)
      break;

  case PM_RESUME:
    for(i = mmc_ncontrollers - 1; i >= 0; i--) {
      ctrlr = mmc_controller[i];
      if(ctrlr->tmpl->resume)
        ctrlr->tmpl->resume(ctrlr);
    }
    ret = 0;
    break;

  default:
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "unsupported PM request %d\n", pmreq);
  }

  up_read(&mmc_controller_sem);
  /* error: */
  __LEAVE("ret=%d", ret);
  return ret;
}
#endif

/* kernel module stuff */
static int __devinit drv_module_init(void);
static void __devexit drv_module_cleanup(void);

static int __init core_module_init(void)
{
  int ret = -ENODEV;

#ifdef WPRM_DRV
  /* WLAN module power supply turn on */
  wprm_wlan_power_supply(POWER_IC_PERIPH_ON);

  /* WLAN module GPIO clean up */
  wprm_wlan_gpio_init();
#endif

  memset(&mmc_controller, 0, sizeof(mmc_controller));
  /* RPACE adder */
  marvel_debug_hits = (unsigned long *)kmalloc(MARVEL_DEBUG_HITS*sizeof(unsigned long), GFP_KERNEL);
  if (!marvel_debug_hits) {
	printk("marvel_debug_hits kmalloc failed\n");
	return -1;
  }
  memset(marvel_debug_hits, 0, MARVEL_DEBUG_HITS*sizeof(unsigned long));
  marvel_debug_hits[0]= 0x7777badd;
  marvel_debug_hits[MARVEL_DEBUG_HITS_LAST]= 0xbadd7777;
  

  init_rwsem(&mmc_controller_sem);
  init_rwsem(&mmc_notifier_sem);
#ifdef OLD_CONFIG_PM
  if(!(mmc_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, sdpm_callback)))
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "failed to register PM callback\n");
#endif
#ifdef CONFIG_PROC_FS
  mmc_proc_dir = proc_mkdir("sdio", NULL);
#endif
  ret = 0;
  if(direct_init() < 0)
	  return -1; 
  printk(KERN_INFO "SDIO subsystem (Marvell International Ltd.)\n");
  if(drv_module_init() < 0)
	  return -1; 
  /* RPACE adder */
  
  return ret;
}

static void __exit core_module_cleanup(void)
{
  direct_cleanup();
#ifdef OLD_CONFIG_PM
  pm_unregister(mmc_pm_dev);
#endif
  drv_module_cleanup();

#ifdef WPRM_DRV
  /* WLAN module GPIO clean up when exit */
  wprm_wlan_gpio_exit();

  /* WLAN module power supply turn off */
  wprm_wlan_power_supply(POWER_IC_PERIPH_OFF);
#endif

#ifdef CONFIG_PROC_FS
  if(mmc_proc_dir)
    remove_proc_entry("sdio", NULL);
#endif
}

module_init(core_module_init);
module_exit(core_module_cleanup);

static int parse_checksum(tuple_t *tuple, cistpl_checksum_t *csum)
{
  u_char *p;
  if (tuple->TupleDataLen < 5)
    return CS_BAD_TUPLE;
  p = (u_char *)tuple->TupleData;
  csum->addr = tuple->CISOffset+(short)le16_to_cpu(*(u_short *)p)-2;
  csum->len = le16_to_cpu(*(u_short *)(p + 2));
  csum->sum = *(p+4);
  return CS_SUCCESS;
}

static int parse_strings(u_char *p, u_char *q, int max,
			 char *s, u_char *ofs, u_char *found)
{
  int i, j, ns;
  
  if (p == q) return CS_BAD_TUPLE;
  ns = 0; j = 0;
  for (i = 0; i < max; i++) {
    if (*p == 0xff) break;
    ofs[i] = j;
    ns++;
    for (;;) {
      s[j++] = (*p == 0xff) ? '\0' : *p;
      if ((*p == '\0') || (*p == 0xff)) break;
	    if (++p == q) return CS_BAD_TUPLE;
    }
    if ((*p == 0xff) || (++p == q)) break;
  }
  if (found) {
    *found = ns;
	return CS_SUCCESS;
  } else {
    return (ns == max) ? CS_SUCCESS : CS_BAD_TUPLE;
  }
}

static int parse_vers_1(tuple_t *tuple, cistpl_vers_1_t *vers_1)
{
  u_char *p, *q;
  
  p = (u_char *)tuple->TupleData;
  q = p + tuple->TupleDataLen;
  
  vers_1->major = *p; p++;
  vers_1->minor = *p; p++;
  if (p >= q) return CS_BAD_TUPLE;
  
  return parse_strings(p, q, CISTPL_VERS_1_MAX_PROD_STRINGS,
		       vers_1->str, vers_1->ofs, &vers_1->ns);
}

static int parse_altstr(tuple_t *tuple, cistpl_altstr_t *altstr)
{
  u_char *p, *q;
  
  p = (u_char *)tuple->TupleData;
  q = p + tuple->TupleDataLen;
  
  return parse_strings(p, q, CISTPL_MAX_ALTSTR_STRINGS,
		       altstr->str, altstr->ofs, &altstr->ns);
}


static int parse_manfid(tuple_t *tuple, cistpl_manfid_t *m)
{
  u_short *p;
  if (tuple->TupleDataLen < 4)
    return CS_BAD_TUPLE;
  p = (u_short *)tuple->TupleData;
  m->manf = le16_to_cpu(p[0]);
  m->card = le16_to_cpu(p[1]);
  return CS_SUCCESS;
}


static int parse_funcid(tuple_t *tuple, cistpl_funcid_t *f)
{
  u_char *p;
  if (tuple->TupleDataLen < 2)
    return CS_BAD_TUPLE;
  p = (u_char *)tuple->TupleData;
  f->func = p[0];
  f->sysinit = p[1];
  return CS_SUCCESS;
}

static int parse_funce(tuple_t *tuple, cistpl_funce_t *f)
{
  u_char *p;
  int i;
  if (tuple->TupleDataLen < 1)
    return CS_BAD_TUPLE;
  p = (u_char *)tuple->TupleData;
  f->type = p[0];
  for (i = 1; i < tuple->TupleDataLen; i++)
    f->data[i-1] = p[i];
  return CS_SUCCESS;
}

/**********************************************************************/
/* The section below are module functions dealing with SDIO/CIS       */
/**********************************************************************/

int sdio_get_first_tuple(mmc_card_t card, u8 func, tuple_t *tuple) {
  tuple->TupleOffset = 0;
  return get_next_tuple(card, func, tuple);
}
EXPORT_SYMBOL(sdio_get_first_tuple);

int get_next_tuple(mmc_card_t card, u8 func, tuple_t *tuple) {
  int ret;
  int matched;
  cisdata_t TupleCode;
  cisdata_t TupleLink;
  cisdata_t TupleOffset;

  if(!card)
    return CS_BAD_HANDLE;

  if(func > card->info.nio)
    return CS_NO_MORE_ITEMS;

  /* Traverse until we match the tuple code */
  for(matched = 0, TupleOffset = tuple->TupleOffset; !matched;
      TupleOffset += (TupleLink + 2)) {

    /* Read TPL_CODE */
    ret = sdio_read_ioreg(card, FN0,
			  card->info.cisptr[func] + TupleOffset,
			  &TupleCode);
    if(ret < 0)
      return CS_OUT_OF_RESOURCE;
    
    if(TupleCode == CISTPL_END)
      return CS_OUT_OF_RESOURCE;
    
    if(TupleCode == tuple->DesiredTuple ||
       tuple->DesiredTuple == RETURN_FIRST_TUPLE)
      matched = 1;

    /* Read TPL_LINK */
    ret = sdio_read_ioreg(card, FN0,
			  card->info.cisptr[func] + TupleOffset + 1,
			  &TupleLink);
    if(ret < 0)
      return CS_OUT_OF_RESOURCE;

    if(matched) {
      /* Here we matched, so assign values, and the loop will be broken */
      tuple->TupleCode   = TupleCode;
      tuple->TupleLink   = TupleLink;
      tuple->TupleOffset = TupleOffset;
    } else {
      /* Otherwise, let's makesure that the TPL_CODE looks decent. */
      /* if not, then there is a possibility that the IO register  */
      /* space we are accessing is bad!                            */
      switch(TupleCode) {
      case CISTPL_NULL:
      case CISTPL_END:
      case CISTPL_CHECKSUM:
      case CISTPL_VERS_1:
      case CISTPL_ALTSTR:
      case CISTPL_MANFID:
      case CISTPL_FUNCID:
      case CISTPL_FUNCE:
	/* The above are valid tuples supported by SDIO spec. */
	break;
      default:
	/* Any other TPL_CODE is unacceptable! */
	return CS_OUT_OF_RESOURCE;
      }
    }
  }

  return matched ? CS_SUCCESS : CS_NO_MORE_ITEMS;
}
EXPORT_SYMBOL(get_next_tuple);

int sdio_get_tuple_data(mmc_card_t card, u8 func, tuple_t *tuple) {
  int ret;

  if(!card)
    return CS_BAD_HANDLE;

  if(func > card->info.nio)
    return CS_OUT_OF_RESOURCE;

  /* Make sure that the body length is sane */
  if(tuple->TupleLink > sizeof(MAX_SDIO_TUPLE_BODY_LEN)) {
    return CS_OUT_OF_RESOURCE;
  }

  /* Read the tuple body */
  for(tuple->TupleDataLen = 0;
      ( tuple->TupleDataLen < tuple->TupleLink && 
	tuple->TupleDataLen < tuple->TupleDataMax );
      tuple->TupleDataLen++) {
    ret = sdio_read_ioreg(card, FN0,
			  card->info.cisptr[func] + tuple->TupleOffset + 2 +
			  tuple->TupleDataLen,
			  &tuple->TupleData[tuple->TupleDataLen]);
    if(ret < 0)
      return CS_OUT_OF_RESOURCE;
  }

  return CS_SUCCESS;
}
EXPORT_SYMBOL(sdio_get_tuple_data);

int sdio_parse_tuple(mmc_card_t card, u8 func, tuple_t *tuple, cisparse_t *parse){
  int ret = CS_SUCCESS;
  
  if(tuple->TupleDataLen > tuple->TupleDataMax)
    return CS_BAD_TUPLE;
  switch (tuple->TupleCode) {
  case CISTPL_CHECKSUM:
    ret = parse_checksum(tuple, &parse->checksum);
    break;
  case CISTPL_VERS_1:
    ret = parse_vers_1(tuple, &parse->version_1);
    break;
  case CISTPL_ALTSTR:
    ret = parse_altstr(tuple, &parse->altstr);
    break;
  case CISTPL_MANFID:
    ret = parse_manfid(tuple, &parse->manfid);
    break;
  case CISTPL_FUNCID:
    ret = parse_funcid(tuple, &parse->funcid);
    break;
  case CISTPL_FUNCE:
    ret = parse_funce(tuple, &parse->funce);
    break;
  default:
    ret = CS_UNSUPPORTED_FUNCTION;
    break;
  }
  return ret;
}
EXPORT_SYMBOL(sdio_parse_tuple);

int validate_cis(mmc_card_t card, u8 func, tuple_t *tuple, cisinfo_t *cisinfo) {
  return -EINVAL;  /* Not yet supported */
}
EXPORT_SYMBOL(validate_cis);

int replace_cis(mmc_card_t card, u8 func, cisdump_t *cisinfo) {
  return -EINVAL;  /* Not supported */
}
EXPORT_SYMBOL(replace_cis);

int sdio_get_vendor_id(mmc_card_t card)
{
	int		ret = -ENODEV;
	tuple_t		tuple;
	cisparse_t	parse;
	cisdata_t	buf[MAX_SDIO_TUPLE_BODY_LEN];

	tuple.Attributes = 0;
	tuple.DesiredTuple = CISTPL_MANFID;
	if ((ret = sdio_get_first_tuple(card, FN0, &tuple)) != CS_SUCCESS)
		return -ENODEV;

	tuple.TupleData = buf;
	tuple.TupleDataMax = sizeof(buf);
	ret = sdio_get_tuple_data(card, FN0, &tuple);
	if (ret != CS_SUCCESS)
		return -ENODEV;

	ret = sdio_parse_tuple(card, FN0, &tuple, &parse);
	if (ret != CS_SUCCESS)
		return -ENODEV;
 
	return parse.manfid.manf;
}
EXPORT_SYMBOL(sdio_get_vendor_id);

/**
 * Support open() system call.
 */
static int direct_open(struct inode *inode, struct file *file)
{
  MOD_INC_USE_COUNT;  
  file->private_data = NULL;
  return 0;
}

/**
 * Support close() system call.
 */
static int direct_release(struct inode *inode, struct file *file)
{
  MOD_DEC_USE_COUNT;
  file->private_data = NULL;
  return 0;
}

/**
 * No read() supported on SDIO. Use ioctl() instead!
 */
static ssize_t direct_read(struct file *file, char *buf,
				size_t size, loff_t *ppos)
{
  return 0;
}

/**
 * No write() supported on SDIO. Use ioctl() instead!
 */
static ssize_t direct_write(struct file *file, const char *buf,
				 size_t size, loff_t *ppos)
{
  return 0;
}

/**
 * Handles:
 *   IOCSDCTL_INSERT.
 *   IOCSDCTL_EJECT.
 *   IOCSDCTL_RECYCLE.
 */
static int sdctl_op(unsigned int cmd) {
  mmc_controller_t ctrlr;
  int host;

  /* RPACE adder */
  

  down_read(&mmc_controller_sem);
  for(host = 0; host < MMC_CONTROLLERS_MAX; host++) {
    ctrlr = mmc_controller[host];
    
    if(ctrlr != NULL) {
      switch(cmd) {
      case IOCSDCTL_INSERT:
	update_card_stack(0);
	break;
      case IOCSDCTL_EJECT:
	update_card_stack(0);
	break;
      case IOCSDCTL_RECYCLE:
	down_read(&mmc_notifier_sem);
	__card_stack_foreach(&ctrlr->stack, notify_remove, FALSE);
	__card_stack_foreach(&ctrlr->stack, notify_add,    FALSE);
	up_read(&mmc_notifier_sem);
	break;
      }
    }
  }

  up_read(&mmc_controller_sem);

  /* RPACE adder */
  
  return 0;
}

/**
 * The ioctl() implementation of all SDIO direct access commands are
 * listed here.
 */
static int direct_ioctl(struct inode *inode, struct file *file,
			     unsigned int cmd, unsigned long arg) {
  int ret = -ENODEV;

  /* RPACE adder */
  

  switch(cmd) {
  case IOCSDCTL_INSERT:
  case IOCSDCTL_EJECT:
  case IOCSDCTL_RECYCLE:
    ret = sdctl_op(cmd);
    break;
  case IOCSDIO_RREG:
  case IOCSDIO_WREG:
  case IOCSDIO_RMEM:
  case IOCSDIO_WMEM:
  case IOCSSDIO_GET_FIRST_TUPLE:
  case IOCSSDIO_GET_NEXT_TUPLE:
  case IOCSSDIO_GET_TUPLE_DATA:
  case IOCSSDIO_PARSE_TUPLE:
  case IOCSSDIO_VALIDATE_CIS:
  case IOCSSDIO_REPLACE_CIS:
  default:
    ret = -ENOIOCTLCMD;
    break;
  }

  /* RPACE adder */
  
  return ret;
}

struct file_operations direct_fops = {
  owner:	THIS_MODULE,
  open:		direct_open,
  release:	direct_release,
  read:		direct_read,
  write:	direct_write,
  ioctl:	direct_ioctl,
  llseek:	NULL
};

int direct_init(void)
{
  if(register_chrdev( SDIO_DIRECT_MAJOR, "direct", &direct_fops)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "failed to request device major number\n");
    return -ENODEV;
  }
  return 0;
}

void direct_cleanup(void)
{
  unregister_chrdev(SDIO_DIRECT_MAJOR, "direct");
}

int sdio_request_irq(mmc_card_t card,
		     void (*handler)(int, void *, struct pt_regs *),
		     unsigned long irq_flags,
		     const char *devname,
		     void *dev_id)
{
  int ret;
  int func;
  u8 io_enable_reg;
  u8 int_enable_reg;
  mmc_controller_t ctrlr;

  /* RPACE adder */
  

  if(!card) {
    ret = -ENODEV;
    goto done;
  }

  /* RPACE, save dev_id for SDIO Interrupt time */
  mmccard_devid = dev_id;
  mmccard_handler = handler;
  
  ctrlr = card->ctrlr;

  /* Ask the controller to setup the IRQ line */
  if(!ctrlr->tmpl->irq_line_ready){
    ctrlr->tmpl->setup_irq_line(ctrlr);
  }

  /* Try to request the IRQ from the OS */
  /* RPACE for new yellow book design comment this out */
  /* ret = request_irq(ctrlr->tmpl->irq_line, handler,
		    SA_SHIRQ,  
		    devname, dev_id);
  if(ret < 0)
    goto done; */

  io_enable_reg  = 0x0;
  int_enable_reg = IENM;
  for(func = 1; func <= 7 && func <= card->info.nio; func++) {
    io_enable_reg  |= IOE(func);
    int_enable_reg |= IEN(func);
  }
  /* Enable function IOs on card */
  sdio_write_ioreg(card, FN0, IO_ENABLE_REG, io_enable_reg);
  /* Enable function interrupts on card */
  sdio_write_ioreg(card, FN0, INT_ENABLE_REG, int_enable_reg);

  /* RPACE, unmask the SDIO card interrupt */
  MMC_I_MASK &= ~MMC_I_MASK_SDIO_INT;
  wmb();
  printk("sdio_request_irq: MMC_I_MASK = 0x%x\n", MMC_I_MASK);


  ret = 0;

 done:
  
  /* RPACE adder */
  
  return ret;
}
EXPORT_SYMBOL(sdio_request_irq);

int sdio_free_irq(mmc_card_t card, void *dev_id)
{
  int ret;
  mmc_controller_t ctrlr;
  dev_id=mmccard_devid;
//  printk(__FUNCTION__);

  /* RPACE adder */
  

  if(!card) {
    ret = -ENODEV;
    goto done;
  }

  ctrlr = card->ctrlr;

  /* Disable function IOs on the card */
//  sdio_write_ioreg(card, FN0, IO_ENABLE_REG, 0x0);

  /* Disable function interrupts on the function */
//  sdio_write_ioreg(card, FN0, INT_ENABLE_REG, 0x0);

  /* Release IRQ from OS */
//	printk("Before free_irq line==>%d\n",ctrlr->tmpl->irq_line);
//  free_irq(ctrlr->tmpl->irq_line, ctrlr);
	MMC_I_MASK = MMC_I_MASK_ALL;
//	MOD_DEC_USE_COUNT;
	mmccard_handler = NULL;
	mdelay(10);

  ret = 0;

 done:
  /* RPACE adder */
  

  return ret;
}
EXPORT_SYMBOL(sdio_free_irq);

/* Prototypes */

    /* RPACE clock experiment */
static int stop_bus_clock_2(mmc_controller_t);
static int stop_bus_clock(mmc_controller_t);
static int rw_iomem(mmc_controller_t, data_transfer_req_t);
static int rw_ioreg(mmc_controller_t, ioreg_req_t);

static mmc_controller_t host = NULL;

/* service routines */
static inline int __check_state(mmc_controller_t ctrlr,
                                         pxa_mmc_state_t state)
{
  int ret = -1;
#ifndef EXTRA  
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  if(hostdata->state != state) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3,
              "state (%s vs %s)\n",
              PXA_MMC_STATE_LABEL(hostdata->state),
              PXA_MMC_STATE_LABEL(state));
    goto error;
  }
#else
  if(((pxa_mmc_hostdata_t)ctrlr->host_data)->state != state) {
     MMC_DEBUG(MMC_DEBUG_LEVEL3,
               "state (%s vs %s)\n",
	       PXA_MMC_STATE_LABEL(((pxa_mmc_hostdata_t)ctrlr->host_data)->state),
	       PXA_MMC_STATE_LABEL(state));
     goto error;
  }
#endif
  ret = 0;
error:
  return ret;
}

static inline int __set_state(mmc_controller_t ctrlr,
                                       pxa_mmc_state_t state)
{
  int ret = -1;
#ifndef EXTRA  
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  MMC_DEBUG(MMC_DEBUG_LEVEL3,
            "state changed from %s to %s\n",
            PXA_MMC_STATE_LABEL(hostdata->state), PXA_MMC_STATE_LABEL(state));

  if(state == PXA_MMC_FSM_IDLE) {
  	//MMC_I_MASK &= ~MMC_I_MASK_SDIO_INT;
	//In IDLE state only SDIO_INT is expected
  	MMC_I_MASK = MMC_I_MASK_ALL & (~MMC_I_MASK_SDIO_INT);
  	wmb();
	if(MMC_I_MASK != (MMC_I_MASK_ALL & (~MMC_I_MASK_SDIO_INT)))
//	if(MMC_I_MASK & MMC_I_MASK_SDIO_INT)
		printk("?");
  }
  else {
//  	MMC_I_MASK |= MMC_I_MASK_SDIO_INT;
  }
  hostdata->state = state;
#else
  MMC_DEBUG(MMC_DEBUG_LEVEL3,
            "state changed from %s to %s\n",
	    PXA_MMC_STATE_LABEL(((pxa_mmc_hostdata_t)ctrlr->host_data)->state),
			  	PXA_MMC_STATE_LABEL(state));
  ((pxa_mmc_hostdata_t)ctrlr->host_data)->state = state;
#endif
  ret = 0;
  return ret;
}

static inline int drv_init_completion(mmc_controller_t ctrlr, u32 mask)
{
  u32 tmask, tjiffies;
  int ret = -1;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();
  /* RPACE adder */
  

  if(xchg(&hostdata->busy, 1)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "another interrupt "
              "is already been expected\n");
    printk("Error: another int is already been expected\n");
    goto error;
  }
//#if CONFIG_MMC_DEBUG_IRQ
#if IRQ_DEBUG
//  hostdata->irqcnt = 1000;
  hostdata->irqcnt = 1;
#endif
  init_completion(&hostdata->completion);

  if ((mmccard_handler != (void *)0) && (sdio_interrupt_enabled != 0))
//      tmask = MMC_I_MASK_ALL & (~MMC_I_MASK_SDIO_INT) & (~mask);
      tmask = MMC_I_MASK_ALL & (~mask);
  else tmask = MMC_I_MASK_ALL & ~mask;
  tjiffies=jiffies;
  MMC_I_MASK = tmask;
  wmb();
   //printk("drv_init_completion:set MMC_I_MASK = 0x%x at %lu\n", tmask, tjiffies);
  ret = 0;
error:
  if(ret) printk("drv_init_complrtion(): ret=%d\n",ret);
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}

//#if CONFIG_MMC_DEBUG_IRQ
#if IRQ_DEBUG
static struct timer_list timer;
static void wait_timeo(unsigned long arg)
{
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) arg;

  /* RPACE adder */
  

  hostdata->timeo = 1;
  complete(&hostdata->completion);
  /* RPACE adder */
  
  return;
}
#endif

static inline int drv_wait_for_completion(mmc_controller_t ctrlr,
                                               u32 mask)
{
  int ret = -1;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();

  /* RPACE adder */
  

  if(!xchg(&hostdata->busy, 1)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "there were no " "interrupt awaited for\n");
    printk("there were no interrupt awaited for\n");
    goto error;
  }

  MMC_DEBUG(MMC_DEBUG_LEVEL3, "mask=%x stat=%x\n", mask, MMC_STAT);
//#if CONFIG_MMC_DEBUG_IRQ
#if IRQ_DEBUG
  hostdata->timeo = 0;
  del_timer(&timer);
  timer.function = wait_timeo;
  timer.expires = jiffies + 5UL * HZ;
  timer.data = (unsigned long) hostdata;
  add_timer(&timer);
#endif
  wait_for_completion(&hostdata->completion);
//#if CONFIG_MMC_DEBUG_IRQ
#if IRQ_DEBUG
  del_timer(&timer);
  if(hostdata->timeo) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "irq timed out: mask=%x stat=%x\n",
	      mask, MMC_STAT);
    printk("irq timed out: mask=%x stat=%x MMC_I_MASK=%x MMC_I_REG=%x mask=%x reg=%x\n",
	      mask, MMC_STAT,MMC_I_MASK,MMC_I_REG,hostdata->mmc_i_mask,hostdata->mmc_i_reg);
  printk("state %s\n",
            PXA_MMC_STATE_LABEL(hostdata->state));
  	//stop_bus_clock_2(ctrlr);
	  //__set_state(ctrlr, PXA_MMC_FSM_IDLE);
	  //ret = -10001; //Need to change hard coding if this works! read timeo
    goto error;
  }
#endif
  /*  verify interrupt */
//  if((mask == ~0UL) || !(hostdata->mmc_i_reg & ~mask))
//  if((mask == ~0UL) || (hostdata->mmc_i_reg & mask))
  if((mask == MMC_I_MASK_ALL) || (hostdata->mmc_i_reg & mask))
    ret = 0;
//  else
//    printk("mask=%x mmc_i_reg=%x  & = %x\n",mask,hostdata->mmc_i_reg,hostdata->mmc_i_reg&mask);

error:
  //if(ret) printk("drv_wait4c(): ret=%d\n",ret);
  xchg(&hostdata->busy, 0);
  MMC_DEBUG(MMC_DEBUG_LEVEL3, "mask=0x%x, stat=%x\n", mask, MMC_STAT);
  //printk("mask=0x%x, stat=%x\n", mask, MMC_STAT);
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}


    /* RPACE clock experiment */
static int stop_bus_clock_2(mmc_controller_t ctrlr)
{
   int ret = -1;

    /* RPACE adder */
    __ENTER0();
    /* RPACE adder */
    

    if(!__check_state(ctrlr, PXA_MMC_FSM_CLK_OFF))
      goto out;

    if(!__check_state(ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT)) {
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "BUFFER_IN_TRANSIT\n");
      goto error;
    }

    /* RPACE clock experiment */
    if(drv_init_completion(ctrlr, MMC_I_MASK_CLK_IS_OFF))
      goto error;

    if ((sdio_interrupt_enabled !=0) || (mmccard_handler!=(void *)0)) {
            MMC_DEBUG(MMC_DEBUG_LEVEL1, "stopping bus clock at %lu\n",jiffies);
    }

    MMC_STRPCL = MMC_STRPCL_STOP_CLK;

    if(drv_wait_for_completion(ctrlr, MMC_I_MASK_CLK_IS_OFF))
      goto error;

    MMC_DEBUG(MMC_DEBUG_LEVEL3, "clock is off\n");
    if(__set_state(ctrlr, PXA_MMC_FSM_CLK_OFF))
      goto error;
out:
    ret = 0;
error:
  //if(ret) printk("stop_bus_clock_2(): ret=%d\n",ret);
    /* RPACE adder */
    
    __LEAVE("ret=%d", ret);

    return ret;
}



static int stop_bus_clock(mmc_controller_t ctrlr)
{
  int ret = -1;

  /* RPACE adder */
  __ENTER0();
  /* RPACE adder */
  

  if(!__check_state(ctrlr, PXA_MMC_FSM_CLK_OFF))
    goto out;

  if(!__check_state(ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "BUFFER_IN_TRANSIT\n");
    goto error;
  }

  /* RPACE clock experiment */
  // if(drv_init_completion(ctrlr, MMC_I_MASK_CLK_IS_OFF))
  //   goto error;

  // MMC_STRPCL = MMC_STRPCL_STOP_CLK;

  // if(drv_wait_for_completion(ctrlr, MMC_I_REG_CLK_IS_OFF))
  //   goto error;

  MMC_DEBUG(MMC_DEBUG_LEVEL3, "clock is off\n");
  if(__set_state(ctrlr, PXA_MMC_FSM_CLK_OFF))
    goto error;
out:
  ret = 0;
error:
  if(ret) printk("stop_bus_clock(): ret=%d\n",ret);
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}

static inline int start_bus_clock(mmc_controller_t ctrlr)
{
  int ret = -1;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  /* RPACE adder */
  

  // printk("<1>start_bus_clock enter i_reg=0x%x, stat=0x%x\n",MMC_I_REG, MMC_STAT);

  if((hostdata->state != PXA_MMC_FSM_CLK_OFF)
     && (hostdata->state != PXA_MMC_FSM_END_IO)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "illegal state %s\n",
              PXA_MMC_STATE_LABEL(hostdata->state));

    /* RPACE clock experiment */
    /* if clock is already enabled then don't bother turning on,
       and don't give an error */
    if ( (MMC_STAT & MMC_STAT_CLK_EN) ) 
        goto no_error;


    /* RPACE adder */
    printk("<5>start_bus_clock: illegal state %s\n", PXA_MMC_STATE_LABEL(hostdata->state));
    //goto error;
    goto do_start_clock;
  }


  /* RPACE clock experiment */
  /* if clock is already enabled then don't bother turning on */
  if ( (MMC_STAT & MMC_STAT_CLK_EN) ) 
       goto no_error;

do_start_clock:
  MMC_STRPCL = MMC_STRPCL_START_CLK;
  wmb();
no_error:
  /* RPACE adder */
  // printk("<5>clock on\n");
  MMC_DEBUG(MMC_DEBUG_LEVEL3, "clock on\n");
  ret = 0;
//error:
  if(ret) printk("start_bus_clock(): ret=%d\n",ret);
  /* RPACE adder */
  
  // printk("<1>start_bus_clock exit i_reg=0x%x, stat=0x%x\n",MMC_I_REG, MMC_STAT);

  return ret;
}

/* 
   int pxa_mmc_complete_cmd( mmc_controller_t ctrlr, mmc_response_fmt_t response )

   Effects: initializes completion to wait for END_CMD_RES intr,
   waits for intr to occur, checks controller and card status 
   Requiers: controller is in CLK_OFF state
   Modifies: moves controller to the END_CMD state
   Returns: 
*/
static mmc_error_t complete_cmd(mmc_controller_t ctrlr,
                                         mmc_response_fmt_t format,
                                         int send_abort)
{
  mmc_error_t ret = MMC_ERROR_GENERIC;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
  int nwords;
  u32 status, mask;

  __ENTER0();
  /* RPACE adder */
  

  /* FIXME: check arguments */

  if((hostdata->state != PXA_MMC_FSM_CLK_OFF) &&
     (hostdata->state != PXA_MMC_FSM_END_IO)) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "illegal state %s\n",
              PXA_MMC_STATE_LABEL(hostdata->state));
    printk("<1>illegal state %s\n",
              PXA_MMC_STATE_LABEL(hostdata->state));
    goto error;
  }

  mask = MMC_I_MASK_END_CMD_RES;
  if(drv_init_completion(ctrlr, mask)){
	  printk("<1>drv_init_completion failed complete_cmd\n");
    goto error;
  }
  MMC_PRTBUF = MMC_PRTBUF_BUF_FULL;
  /* start the clock */
  if(start_bus_clock(ctrlr)){
	  printk("<1>start_bus_clock failed complete_cmd\n");
    goto error;
  }

  /* wait for END_CMD_RES intr */
  if(drv_wait_for_completion(ctrlr, MMC_I_REG_END_CMD_RES)){
	printk("<1>drv_wait_for_completion failed  complete_cmd\n");	  
    goto error;
  }
  //printk("MMC STAT =%x\n",hostdata->mmc_stat);
  /* check status */
  if(hostdata->mmc_stat & MMC_STAT_TIME_OUT_RESPONSE) {
    ret = MMC_ERROR_TIME_OUT_RESPONSE;
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "response timeout!\n");
    printk("<1>response timeout! irqnum=%ld\n",irqnum);
#ifdef SDIO_DEBUG1
    PRINTK2("<1>response timeout!\n");
#endif
    goto error;

  } else if(hostdata->mmc_stat & MMC_STAT_READ_TIME_OUT) {
    ret = MMC_ERROR_READ_TIME_OUT;
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "read timeout!\n");
    printk("<1>read timeout! complete_cmd\n");
    goto error;

  } else if(hostdata->mmc_stat & MMC_STAT_RES_CRC_ERROR) {
    ret = MMC_ERROR_RES_CRC_ERROR;
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "response crc error!\n");
    printk("<1>response crc error! complete_cmd\n");
    goto error;

  } else if(hostdata->mmc_stat & MMC_STAT_CRC_READ_ERROR) {
    ret = MMC_ERROR_CRC_READ_ERROR;
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "read crc error!\n");
    printk("<1>read crc error! complete_cmd\n");
    goto error;

  } else if(hostdata->mmc_stat & MMC_STAT_CRC_WRITE_ERROR) {
    ret = MMC_ERROR_CRC_WRITE_ERROR;
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "write crc error!\n");
    printk("<1>write crc error! complete_cmd\n");
    goto error;
  }

  nwords = (format == MMC_NORESPONSE) ? 0 :
    (format == MMC_R1) ? 3 :
    (format == MMC_R2) ? 8 :
    (format == MMC_R3) ? 3 :
    (format == MMC_R4) ? 3 :
    (format == MMC_R5) ? 3 :
    (format == MMC_R6) ? 3 :
    -1;
  MMC_DEBUG(MMC_DEBUG_LEVEL3, "nwords=%d\n", nwords);
  ret = nwords;
  if(nwords > 0) {
    register int i;

    for(i = nwords - 1; i >= 0; i--) {
      u32 res = MMC_RES;
      int ibase = i << 1;

      hostdata->mmc_res[ibase] = ((u8 *) & res)[0];
      hostdata->mmc_res[ibase + 1] = ((u8 *) & res)[1];
      --ret;
    }
#ifdef CONFIG_MMC_DEBUG
    switch (format) {
    case MMC_R1:
      MMC_DUMP_R1(ctrlr);
      break;
    case MMC_R2:
      MMC_DUMP_R2(ctrlr);
      break;
    case MMC_R3:
      MMC_DUMP_R3(ctrlr);
      break;
    case MMC_R4:
      MMC_DUMP_R4(ctrlr);
      break;
    case MMC_R5:
      MMC_DUMP_R5(ctrlr);
      break;
    case MMC_R6:
      MMC_DUMP_R6(ctrlr);
      break;
    default:
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "unknown response format\n");
      ret = MMC_ERROR_GENERIC;
      goto error;
    }
#endif

    /* check card status for R1(b) commands */
    if(format == MMC_R1) {
      u8 cmd;
      ((u8 *) & status)[0] = hostdata->mmc_res[1];
      ((u8 *) & status)[1] = hostdata->mmc_res[2];
      ((u8 *) & status)[2] = hostdata->mmc_res[3];
      ((u8 *) & status)[3] = hostdata->mmc_res[4];
      cmd = PXA_MMC_RESPONSE(ctrlr, 5) & 0x3f;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "cmd=%u status: 0x%08x\n", cmd, status);
      if(status & MMC_CARD_STATUS_OUT_OF_RANGE) {
        ret = MMC_ERROR_OUT_OF_RANGE;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "out of range!\n");
    	printk("<1>error out of range! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_ADDRESS_ERROR) {
        ret = MMC_ERROR_ADDRESS_ERROR;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "address error!\n");
    	printk("<1>address error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_BLOCK_LEN_ERROR) {
        ret = MMC_ERROR_BLOCK_LEN_ERROR;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "block len error!\n");
    	printk("<1>block len error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_ERASE_SEQ_ERROR) {
        ret = MMC_ERROR_ERASE_SEQ_ERROR;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "erase seq error!\n");
    	printk("<1>erase seq error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_ERASE_PARAM) {
        ret = MMC_ERROR_ERASE_PARAM;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "erase param!\n");
    	printk("<1>erase param error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_WP_VIOLATION) {
        ret = MMC_ERROR_WP_VIOLATION;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "wp violation!\n");
    	printk("<1>wp violation error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_CARD_IS_LOCKED) {
        ret = MMC_ERROR_CARD_IS_LOCKED;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "card is locked!\n");
    	printk("<1>card is locked! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_LOCK_UNLOCK_FAILED) {
        ret = MMC_ERROR_LOCK_UNLOCK_FAILED;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "lock unlock failed!\n");
    	printk("<1>lock unlock failed! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_COM_CRC_ERROR) {
        ret = MMC_ERROR_COM_CRC_ERROR;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "com crc error!\n");
    	printk("<1>com crc error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_ILLEGAL_COMMAND) {
        ret = MMC_ERROR_ILLEGAL_COMMAND;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "illegal command!\n");
    	printk("<1>illegal command! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_CARD_ECC_FAILED) {
        ret = MMC_ERROR_CARD_ECC_FAILED;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "ecc failed!\n");
    	printk("<1>card ecc failed! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_CC_ERROR) {
        ret = MMC_ERROR_CC_ERROR;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "cc error!\n");
    	printk("<1>error cc error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_ERROR) {
        ret = MMC_ERROR_ERROR;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "general error!\n");
    	printk("<1>general error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_UNDERRUN) {
        ret = MMC_ERROR_UNDERRUN;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "underrun!\n");
    	printk("<1>underrun error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_OVERRUN) {
        ret = MMC_ERROR_OVERRUN;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "overrun!\n");
    	printk("<1>overrun error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_CID_CSD_OVERWRITE) {
        ret = MMC_ERROR_CID_CSD_OVERWRITE;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "cid/csd overwrite!\n");
    	printk("<1>cid/csd overwrite error! complete_cmd\n");
        goto mmc_error;
      } else if(status & MMC_CARD_STATUS_ERASE_RESET) {
        ret = MMC_ERROR_ERASE_RESET;
	MMC_DEBUG(MMC_DEBUG_LEVEL3, "erase reset!\n");
    	printk("<1>erase reset error! complete_cmd\n");
        goto mmc_error;
      }
    }
  }

  /* check for response flags in R5 response */
  if(format == MMC_R5) {
    u8 r5flags = PXA_MMC_RESPONSE(ctrlr, 2);
    if(r5flags & SDIO_IO_RW_COM_CRC_ERROR) {
      ret = R5_COM_CRC_ERROR;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "com crc error!\n");
      printk("<1>com crc error!\n");
      goto mmc_error;
    } else if(r5flags & SDIO_IO_RW_ILLEGAL_COMMAND) {
      ret = R5_ILLEGAL_COMMAND;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "illegal command!\n");
      printk("<1>illegal command!\n");
      goto mmc_error;
    } else if(r5flags & SDIO_IO_RW_ERROR) {
      ret = R5_ERROR;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "error!\n");
      printk("<1>error!\n");
      goto mmc_error;
    } else if(r5flags & SDIO_IO_RW_FUNCTION_NUMBER) {
      ret = R5_FUNCTION_NUMBER;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "function number!\n");
      printk("<1>function number!\n");
      goto mmc_error;
    } else if(r5flags & SDIO_IO_RW_OUT_OF_RANGE) {
      ret = R5_OUT_OF_RANGE;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "out of range!\n");
      printk("<1>out of range!\n");
      goto mmc_error;
    }
    
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "io current state: %s\n",
	      ((r5flags & 0x30) == 0x00) ? "DIS" :
	      ((r5flags & 0x30) == 0x10) ? "CMD" :
	      ((r5flags & 0x30) == 0x20) ? "TRN" : "RFU");
  }

  /* check for card status in R6 response */
  if(format == MMC_R6) {
    u16 cardstat = 0x0;
    u16 cst;
    cardstat |= (PXA_MMC_RESPONSE(ctrlr, 2) << 8);
    cardstat |= (PXA_MMC_RESPONSE(ctrlr, 1) << 0);
    if(cardstat & SD_R6_CARD_STATUS_COM_CRC_ERROR) {
      ret = R6_COM_CRC_ERROR;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "command crc error!\n");
      printk("<1>command crc error!\n");
      goto mmc_error;
    } else if(cardstat & SD_R6_CARD_STATUS_ILLEGAL_COMMAND) {
      ret = R6_ILLEGAL_COMMAND;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "illegal command!\n");
      printk("<1>illegal command!\n");
      goto mmc_error;
    } else if(cardstat & SD_R6_CARD_STATUS_ERROR) {
      ret = R6_ERROR;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "general error!\n");
      printk("<1>general error!\n");
      goto mmc_error;
    } else if(cardstat & SD_R6_CARD_STATUS_AKE_SEQ_ERROR) {
      ret = R6_AKE_SEQ_ERROR;
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "authentication sequence error!\n");
      printk("<1>authentication sequence error!\n");
      goto mmc_error;
    }

    cst = ((cardstat & 0x1e00) >> 8);
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "card state: %s\n",
	      (cst == 0) ? "idle"  :
	      (cst == 1) ? "ready" :
	      (cst == 2) ? "ident" :
	      (cst == 3) ? "stby"  :
	      (cst == 4) ? "tran"  :
	      (cst == 5) ? "data"  :
	      (cst == 6) ? "rcv"   :
	      (cst == 7) ? "prg"   :
	      (cst == 8) ? "dis"   :
	      "unkown");
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "ready for data: %s\n",
	      (cardstat & 0x0100) ? "yes" : "no");
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "expect ACMD: %s\n",
	      (cardstat & 0x0020) ? "yes" : "no");
  }
    

  if(ret >= 0) {
    if(__set_state(ctrlr, PXA_MMC_FSM_END_CMD)) {
      ret = MMC_ERROR_GENERIC;
      goto error;
    }
  }

  goto out;

mmc_error:
error:
  if(ret) printk("comp_cmd(): ret=%d\n",ret);
  /* move controller to the IDLE state */
  /* RPACE clock experiment */
  stop_bus_clock_2(ctrlr);
  __set_state(ctrlr, PXA_MMC_FSM_IDLE);

out:
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}

static mmc_error_t complete_io(mmc_controller_t ctrlr, mmc_dir_t dir)
{
  int ret = MMC_ERROR_GENERIC;

  __ENTER("dir=%d", dir);

  /* RPACE adder */
  

  if(__check_state(ctrlr, PXA_MMC_FSM_END_IO)){
	  printk("<1>__check_state failed complete_io\n");
    goto error;
  }

  /* 1. wait for DATA_TRAN_DONE intr */
  if((ret = drv_init_completion(ctrlr, MMC_I_MASK_DATA_TRAN_DONE))){
	 // printk("<1>drv_init_completion failed complete_io 1\n");
    goto error;
  }

  if((ret = drv_wait_for_completion(ctrlr, MMC_I_REG_DATA_TRAN_DONE))){
//	udelay(500);
	printk("<1>drv_wait_for_completion  data_transfer_done complete_io 1\n");
	goto error;
  }
	//printk("Got the MMC_I_REG_DATA_TRAN_DONE ->3\n");
	  
  if(dir == MMC_WRITE) {
    /* 2. wait for PRG_DONE intr */
    if((ret = drv_init_completion(ctrlr, MMC_I_MASK_PRG_DONE))){
	printk("<1>drv_init_completion failed -complete_io 2\n");
	goto error;
    }

    if((ret = drv_wait_for_completion(ctrlr, MMC_I_REG_PRG_DONE))){
	printk("<1>drv_wait_for_completion complete_io 2\n");
	goto error;
	}
	printk("Got MMC_IREG_PRG_DONE ->4\n");

  }
  /* move the controller to the IDLE state */
  if((ret = stop_bus_clock(ctrlr))){
	printk("<1>stop_bus_clock failed -complete_io\n");
	goto error;
  }
  
  if(__set_state(ctrlr, PXA_MMC_FSM_IDLE)){
	printk("<1>__set_state failed -complete_io\n");
	goto error;
  }
  ret = 0;
error:
//  if(ret) printk("comp_io(): ret=%d\n",ret);
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}

static int __update_acq(mmc_controller_t ctrlr)
{
  int ret = -EINVAL;
  pxa_mmc_hostdata_t hostdata = NULL;
  mmc_card_t card = NULL;
  mmc_card_stack_rec_t fake;
  mmc_card_stack_t stack = &fake;
  u16 argl = 0U, argh = 0U;
  int ncards = 0;
  int tries = 0;
  int passed = 0;
  struct _card_info_rec tmpinfo;
	printk("<1>Enter __update_acq\n");

  /* RPACE adder */
  

  if(!ctrlr)
    goto error;

  hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
  
  //This variable is used when clock is 
  //set back to its previous value when exiting bus power save mode.
  SAVED_MMC_HIGH_CLKRT = clkdiv;
  
  __card_stack_init(stack);

  /* max open-drain mode frequency is 400kHZ */
  MMC_CLKRT = MMC_CLKRT_0_3125MHZ;
  MMC_RESTO = MMC_RES_TO_MAX;   /* set response timeout */

  /* discover and add cards to the stack */
  /* I. bus operation condition setup */
  /*      1) send CMD5 and get the OCR */
  /* RPACE clock experiment */
  if((ret = stop_bus_clock_2(ctrlr)))
    goto err_free;
  
  argh = 0x0000;
  argl = 0x0000;

  MMC_CMD = CMD(5);
  MMC_ARGH = argh;
  MMC_ARGL = argl;

  wmb();
  MMC_CMDAT = MMC_CMDAT_INIT | MMC_CMDAT_R1;  /* R4 */

  MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD5(0x%04x%04x)\n", argh, argl);
  /* RPACE adder */
  printk("<1>__update_acq: 1  CMD5(0x%04x%04x)\n",  argh, argl);
  ret = complete_cmd(ctrlr, MMC_R4, FALSE);
  if(!ret) {
    argh = (PXA_MMC_RESPONSE(ctrlr, 4) << 8) | PXA_MMC_RESPONSE(ctrlr, 3);
    argh &= 0x00ff;
    argl = (PXA_MMC_RESPONSE(ctrlr, 2) << 8) | PXA_MMC_RESPONSE(ctrlr, 1);    
  } else if(ret != MMC_ERROR_TIME_OUT_RESPONSE)
    goto err_free;
  
  if(!argh && !argl) {
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "assuming full voltage range support\n");
    argh = 0x00ff;
    argl = 0xff00;
  }

  /*      1) send CMD5 and set the OCR */
  /* RPACE adder */
  printk("<1>__update_acq: 2 stop_bus_clock \n");
  /* RPACE clock experiment */
  if((ret = stop_bus_clock_2(ctrlr)))
    goto err_free;
  
  MMC_CMD = CMD(5);
  MMC_ARGH = argh;
  MMC_ARGL = argl;

  wmb();
  MMC_CMDAT = MMC_CMDAT_R1;

  MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD5(0x%04x%04x)\n", argh, argl);
  /* RPACE adder */
  printk("<1>__update_acq: 3  CMD5(0x%04x%04x)\n",  argh, argl);
  ret = complete_cmd(ctrlr, MMC_R4, FALSE);
  if(ret != 0 && ret != MMC_ERROR_TIME_OUT_RESPONSE)
    goto err_free;

  tmpinfo.ocr  = 0x0;
  tmpinfo.ocr |= (PXA_MMC_RESPONSE(ctrlr, 3) << 16);
  tmpinfo.ocr |= (PXA_MMC_RESPONSE(ctrlr, 2) <<  8);
  tmpinfo.ocr |= (PXA_MMC_RESPONSE(ctrlr, 1) <<  0);
  tmpinfo.nio = ((PXA_MMC_RESPONSE(ctrlr, 4) & 0x70) >> 4);
  tmpinfo.mem = ((PXA_MMC_RESPONSE(ctrlr, 4) & 0x08) ? 1 : 0);
  
  /*      2) continuously send CMD3 and CMD7 until we put the card into */
  /*         STANDBY and then COMMAND state */
#define MAX_INIT_TRIES   5  /* try at most 5 times */
  passed = 0;
  for(tries = 0; !passed && tries < MAX_INIT_TRIES; tries++) {
    /* RPACE clock experiment */
    if((ret = stop_bus_clock_2(ctrlr)))
      goto err_free;

    MMC_CMD = CMD(3);
    MMC_ARGH = 0x0;
    MMC_ARGL = 0x0;
    wmb();
    MMC_CMDAT = MMC_CMDAT_R1;  /* R6 */

    MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD3(0x%04x%04x)\n", argh, argl);
  /* RPACE adder */
  printk("<1>__update_acq: 4  CMD3(0x%04x%04x)\n",  argh, argl);
    ret = complete_cmd(ctrlr, MMC_R6, FALSE);

    if(ret == MMC_ERROR_TIME_OUT_RESPONSE) {
      argh = 0x0;
      argl = 0x0;
    } else {
      argh = (PXA_MMC_RESPONSE(ctrlr, 4) << 8) | PXA_MMC_RESPONSE(ctrlr, 3);
      argl = 0x0;
      passed = 1;
    }


    /* RPACE clock experiment */
    if((ret = stop_bus_clock_2(ctrlr)))
      goto err_free;

    MMC_CMD = CMD(7);
    MMC_ARGH = argh;
    MMC_ARGL = argl;
    wmb();
    MMC_CMDAT = MMC_CMDAT_BUSY | MMC_CMDAT_R1;  /* R1b */

    MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD7(0x%04x%04x)\n", argh, argl);
  /* RPACE adder */
  printk("<1>__update_acq: 5  CMD7(0x%04x%04x)\n",  argh, argl);
    ret = complete_cmd(ctrlr, MMC_R1, FALSE);

    tmpinfo.rca  = argh;

    if(ret == 0 && passed) {
      card = __card_alloc(sizeof(pxa_mmc_card_data_rec_t));
      if(!card) {
	MMC_ERROR("out of memory\n");
	goto err_free;
      }
      
      card->info.type = tmpinfo.type;
      card->info.ocr = tmpinfo.ocr;
      card->info.nio = tmpinfo.nio;
      card->info.mem = tmpinfo.mem;
      card->info.rca = tmpinfo.rca;
      card->slot = ctrlr->slot_next++;    /* FIXME: minor encoding */
      card->ctrlr = ctrlr;
      
      if(!__card_stack_add(stack, card))
	goto err_free;
      
      MMC_DEBUG(MMC_DEBUG_LEVEL2, "added card: slot %d\n", card->slot);
      MMC_DEBUG(MMC_DEBUG_LEVEL2, "  OCR = 0x%x\n", card->info.ocr);
      MMC_DEBUG(MMC_DEBUG_LEVEL2, "  NIO = %d\n", card->info.nio);
      MMC_DEBUG(MMC_DEBUG_LEVEL2, "  MEM = %c\n", card->info.mem ? 'y' : 'n');
      MMC_DEBUG(MMC_DEBUG_LEVEL2, "  RCA = 0x%04x\n", card->info.rca);
      ++ncards;
      break;
    }
  }

  if(!passed) {
    MMC_DEBUG(MMC_DEBUG_LEVEL2,
	      "gave up after exceeding %d attempts to initialize card\n",
	      MAX_INIT_TRIES);

   /* RPACE adder */
   printk("<1>gave up after exceeding %d attempts to initialize card\n", MAX_INIT_TRIES);
    ret = 0;
    goto err_free;
  }

  if(ncards) {
    for(card = stack->first; card; card = card->next) {
      pxa_mmc_card_data_t card_data = (pxa_mmc_card_data_t) card->card_data;
      /* RPACE adder, may not be able to operate at highest freq  */

      if((mode != 1) && (mode != 4))
	mode = 4;
      if(clkdiv > 1)
	clkdiv = 0;

      if(clkdiv == MMC_CLKRT_20MHZ)
	printk("setting clkrt driver data structure to 20MHZ\n");
      else if(clkdiv == MMC_CLKRT_10MHZ)
	printk("setting clkrt driver data structure to 10MHZ\n");
      else
	printk("setting clkrt division to %u\n",clkdiv);
      card_data->clkrt = clkdiv;

      // printk("setting clkrt driver data structure to 10MHZ\n");
      // card_data->clkrt = MMC_CLKRT_10MHZ;
      if(mode == 1) {
	printk("setting SDIO data transfer to 1-bit mode\n");
	ret = sdio_write_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 0x00);
      }
      else {
	printk("setting SDIO data transfer to 4-bit mode\n");
	ret = sdio_write_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 0x02);
      }
      sdio_register(MMC_REG_TYPE_CARD, card, 0);
    }
  }

  /* merge list of the newly inserted cards into controller card stack */
  if(!ctrlr->stack.ncards) {
    ctrlr->stack.first = stack->first;
    ctrlr->stack.last = stack->last;
  } else
    ctrlr->stack.last->next = stack->first;

  ctrlr->stack.ncards += stack->ncards;

  ret = 0;
  goto out;
err_free:
  /* RPACE adder */
  printk("<5>__update_acq: card stack acquisition failure =0x%x\n",ret);
  __card_stack_free(stack);
error:
out:
  /* RPACE adder */
  

  return ret;
}

static int init_card_stack(mmc_controller_t ctrlr)
{
  int ret = -EIO;

  __ENTER0();
  /* RPACE adder */
  

  if(!ctrlr || ctrlr->stack.ncards) {
    ret = -EINVAL;
    goto error;
  }

/* RPACE comment out since bulverde MMC says MMC controller
   cannot operate at highest frequency 
   MMC_CLKRT = MMC_CLKRT_20MHZ;
*/
/* TODO: Check for the appropriate value , Doc says 19.5 MHZ .. */

  MMC_CLKRT = clkdiv;

  MMC_RESTO = MMC_RES_TO_MAX;
  MMC_SPI = MMC_SPI_DISABLE;

  /* update card stack */
  if((ret = __update_acq(ctrlr))) {
    goto err_free;
  }

  /* move the controller to the IDLE state */
  /* RPACE clock experiment */
  if((ret = stop_bus_clock_2(ctrlr))) {
    goto err_free;
  }

  if(__set_state(ctrlr, PXA_MMC_FSM_IDLE))
    goto err_free;

  ret = 0;
  MMC_DEBUG(MMC_DEBUG_LEVEL2, "ncards=%d\n", ctrlr->stack.ncards);
  goto out;

err_free:
  __card_stack_free(&ctrlr->stack);
error:
out:
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);
  return ret;
}

static int update_acq(mmc_controller_t ctrlr)
{
  int ret = -EINVAL;

  __ENTER0();
printk("<1>Enter update_acq\n");
  /* RPACE adder */
  

  if(!ctrlr)
    goto error;

  ret = __update_acq(ctrlr);
error:
  printk("<1>leave update_acq\n");
  /* RPACE adder */
  
  __LEAVE("ret=%d", ret);

  return ret;
}

static int check_card_stack(mmc_controller_t ctrlr)
{
  return 0;
}

static int setup_card(mmc_controller_t ctrlr, mmc_card_t card)
{
  int ret = -ENODEV;
  pxa_mmc_hostdata_t hostdata;
  pxa_mmc_card_data_t card_data;
  u16 argh = 0U;
  u16 argl = 0U;

  __ENTER0();

  /* RPACE adder */
  


  if(!ctrlr || !card) {
    ret = -EINVAL;
    goto error;
  }

  if(card->ctrlr != ctrlr) {
    MMC_DEBUG(MMC_DEBUG_LEVEL2, "card is on another bus\n");
    goto error;
  }

  hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
  card_data = (pxa_mmc_card_data_t) card->card_data;

  argh = card->info.rca;

  /* select requested card */
  /* RPACE clock experiment */
  if((ret = stop_bus_clock_2(ctrlr)))
    goto error;

  MMC_CMD = CMD(7);
  MMC_ARGH = argh;
  MMC_ARGL = argl;
  wmb();
  MMC_CMDAT = MMC_CMDAT_R1;

  MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD7(0x%04x%04x)\n", argh, argl);
  if((ret = complete_cmd(ctrlr, MMC_R1, FALSE)))
    goto error;

  /* set controller options */
  MMC_CLKRT = card_data->clkrt;
//added for 8385PN card
//	card->ctrlr = ctrlr;
  
  /* move the controller to the IDLE state */
  /* RPACE clock experiment */
  if((ret = stop_bus_clock_2(ctrlr)))
    goto error;

  if(__set_state(ctrlr, PXA_MMC_FSM_IDLE))
    goto error;

  ret = 0;
error:
  /* RPACE adder */
  
  __LEAVE0();
  return ret;
}

static inline int __iobuf_init(mmc_controller_t ctrlr, ssize_t cnt)
{
#ifdef PIO
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
#endif
#ifndef PIO
  /* TODO */
#else
  hostdata->iobuf.buf.pos = hostdata->iobuf.iodata;
  hostdata->iobuf.buf.cnt = cnt;
#endif
  return 0;
}

/* TODO: ssize_t pxa_mmc_read_buffer( mmc_controller_t ctrlr, ssize_t cnt )
   effects: reads at most cnt bytes from the card to the controller I/O buffer;
   takes care of partial data transfers
   requieres: 
   modifies: ctrlr->iobuf
   returns: number of bytes actually transferred or negative error code if there were any errors
*/
ssize_t read_buffer(mmc_controller_t ctrlr, ssize_t cnt)
{
  ssize_t ret = -EIO;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
#ifndef PIO
  register int ndesc;
  int chan = hostdata->iobuf.buf.chan;
  pxa_dma_desc *desc;
#endif
  __ENTER("cnt=%d", cnt);
  
  /* RPACE adder */
  

  if((hostdata->state != PXA_MMC_FSM_END_CMD) &&
     (hostdata->state != PXA_MMC_FSM_END_BUFFER)) {
    printk("<1>unexpected state (%s)",
	      PXA_MMC_STATE_LABEL(hostdata->state));
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "unexpected state (%s)",
	      PXA_MMC_STATE_LABEL(hostdata->state));
    goto error;
  }
  
  if(cnt > hostdata->iobuf.bufsz)
    cnt = hostdata->iobuf.bufsz;
  
  if((ret = __iobuf_init(ctrlr, cnt))){
    printk("<1>__iobuf_init failed read_buffer\n");
    goto error;
  }
  (void) __set_state(ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT);
#ifndef PIO
  //if(drv_init_completion(ctrlr, ~MMC_I_MASK_ALL))  /* FIXME */

#ifndef DMA_NO_WAIT
    if(drv_init_completion(ctrlr, ~MMC_I_MASK_ALL))
    {
      printk("<1>drv_init_completion failed read_buffer1\n");
      goto error;
    }
#endif 
  
  if((desc = hostdata->iobuf.buf.last_read_desc)) {
    desc->ddadr &= ~DDADR_STOP;
    desc->dcmd &= ~(DCMD_ENDIRQEN | DCMD_LENGTH);
    desc->dcmd |= (1 << 5);
  }
  /* 1) setup descriptors for DMA transfer from the device */
  ndesc = (cnt >> 5) - 1;       /* FIXME: partial read */
  desc = &hostdata->iobuf.buf.read_desc[ndesc];
  hostdata->iobuf.buf.last_read_desc = desc;
  /* TODO: partial read */
  desc->ddadr |= DDADR_STOP;
  desc->dcmd |= DCMD_ENDIRQEN;
  /* 2) start DMA channel */
  DDADR(chan) = hostdata->iobuf.buf.read_desc_phys_addr;
  wmb();
  DCSR(chan) |= DCSR_RUN;
#else
  if(drv_init_completion(ctrlr, MMC_I_MASK_RXFIFO_RD_REQ))
    goto error;
#endif
  
//if((ret= drv_wait_for_completion(ctrlr, ~0UL))< 0){

#ifndef DMA_NO_WAIT
  if((ret= drv_wait_for_completion(ctrlr, MMC_I_MASK_ALL))< 0){
	  printk("<1>drv_wait_for_completion failed read_buffer2\n");
    	goto error;
  } 
#else
  /* Wait for DMA transfer to complete */
  while (!dma_int_flag) {
  	schedule(); 
  };
  
  dma_int_flag = 0;  
#endif

	//DMA Done -->Interrupt
	//printk("Completed DMA Operation ->Read\n");
//	udelay(500);
  if(__check_state(ctrlr, PXA_MMC_FSM_END_BUFFER)){
	  printk("<1>__check_state failed -read_buffer\n");
	  printk(" Err val =%lu\n",hostdata->mmc_stat&MMC_STAT_ERRORS);
    goto error;
  } 

  if(!(hostdata->mmc_stat & MMC_STAT_ERRORS))   /* FIXME */
    ret = cnt;
error:
  /* RPACE adder */
  

  __LEAVE("ret=%d", ret);
  return ret;
}

ssize_t write_buffer(mmc_controller_t ctrlr, ssize_t cnt)
{
  ssize_t ret = -EIO;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
#ifndef PIO
  register int ndesc;
  int chan = hostdata->iobuf.buf.chan;
  pxa_dma_desc *desc;
#endif
  __ENTER("cnt = %d\n", cnt);
//	printk("write_buffer count = %d\n",cnt);

  /* RPACE adder */
  

  if((hostdata->state != PXA_MMC_FSM_END_CMD) &&
     (hostdata->state != PXA_MMC_FSM_END_BUFFER)) {
    printk("<1>Unexpected state (%s)\n",
              PXA_MMC_STATE_LABEL(hostdata->state));
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "unexpected state (%s)\n",
              PXA_MMC_STATE_LABEL(hostdata->state));
    goto error;
  }

  if(cnt > hostdata->iobuf.bufsz)
    cnt = hostdata->iobuf.bufsz;

  if((ret = __iobuf_init(ctrlr, cnt))){
	printk("<1>__iobuf_init failed write_buffer\n");
	goto error;
  }

  (void) __set_state(ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT);
#ifndef PIO

#ifndef DMA_NO_WAIT
   if(drv_init_completion(ctrlr, ~MMC_I_MASK_ALL))  /* FIXME */
   {
 	printk("drv_init_completion failed write_buffer\n");
    	goto error;
   }
#endif

  if((desc = hostdata->iobuf.buf.last_write_desc)) {
    desc->ddadr &= ~DDADR_STOP;
    desc->dcmd &= ~(DCMD_ENDIRQEN | DCMD_LENGTH);
    desc->dcmd |= (1 << 5);
  }
  /* 1) setup descriptors for DMA transfer to the device */
  ndesc = (cnt >> 5) - 1;       /* FIXME: partial write */
  desc = &hostdata->iobuf.buf.write_desc[ndesc];
  /* TODO: partial write */
  hostdata->iobuf.buf.last_write_desc = desc;
  desc->ddadr |= DDADR_STOP;
  desc->dcmd |= DCMD_ENDIRQEN;
  /* 2) start DMA channel */
  DDADR(chan) = hostdata->iobuf.buf.write_desc_phys_addr;
  DCSR(chan) |= DCSR_RUN;
#else
  if(drv_init_completion(ctrlr, MMC_I_MASK_TXFIFO_WR_REQ))
    goto error;
#endif
//if(drv_wait_for_completion(ctrlr, ~0UL)){

#ifndef DMA_NO_WAIT
   if((ret= drv_wait_for_completion(ctrlr, MMC_I_MASK_ALL))< 0){
	  printk("<1>drv_wait_for_completion dma failed write_buffer\n");
      goto error;
   } 
#else
  /* Wait for DMA transfer to complete */
  while (!dma_int_flag) {
  	schedule();
  };
  
  dma_int_flag = 0;
#endif

	//Dma write -->Interrupt
	//printk("finished the DMA ->2\n");
//	udelay(500);
  if(__check_state(ctrlr, PXA_MMC_FSM_END_BUFFER)){
	 printk("<1>__check_state error write_buffer\n");
	udelay(500);
    goto error;
  } 

  if(!(hostdata->mmc_stat & MMC_STAT_ERRORS))   /* FIXME */
    ret = cnt;
 error:
  /* RPACE adder */
  

  __LEAVE("ret=%d", ret);
  return ret;
}

ssize_t copy_from_buffer(mmc_controller_t ctrlr, mmc_buftype_t to,
				  char *buf, ssize_t cnt)
{
  ssize_t ret = -EIO;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();

  
#ifndef PIO
  /* TODO: check that DMA channel is not running */
#endif
  switch (to) {
  case MMC_USER:
    if(copy_to_user(buf, hostdata->iobuf.iodata, cnt)) {
      ret = -EFAULT;
      goto error;
    }
    break;
  case MMC_KERNEL:
    memcpy(buf, hostdata->iobuf.iodata, cnt);
    break;
  default:
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "unknown buffer type\n");
    goto error;
  }
  ret = cnt;
error:
  
  __LEAVE("ret=%d", ret);
  return ret;
}

ssize_t copy_to_buffer(mmc_controller_t ctrlr, mmc_buftype_t to,
                                char *buf, ssize_t cnt)
{
  ssize_t ret = -EIO;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();

#ifndef PIO
  /* check that DMA channel is not running */
#endif
  switch (to) {
  case MMC_USER:
    if(copy_from_user(hostdata->iobuf.iodata, buf, cnt)) {
      ret = -EFAULT;
      goto error;
    }
    break;
  case MMC_KERNEL:
    memcpy(hostdata->iobuf.iodata, buf, cnt);
#ifdef _USE_INTERNAL_SRAM
    //flush Write Buffer after memcpy to SRAM
    //_DRAIN_WRITE_BUFFER();
#endif //_USE_INTERNAL_SRAM
    break;
  default:
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "unknown buffer type\n");
    goto error;
  }
  ret = cnt;
error:
  
  return ret;
}

static int rw_ioreg(mmc_controller_t ctrlr, ioreg_req_t ioreg) {
  int ret = -EIO;
  u16 argh = 0UL, argl = 0UL;
  

  if(ioreg->rw) {
    __ENTER("ioreg write func=%d reg=0x%02x dat=0x%02x",
	    ioreg->func, ioreg->reg, ioreg->dat);
  } else {
    __ENTER("ioreg read func=%d reg=0x%02x",
	    ioreg->func, ioreg->reg);
  }
  

  if((ret = stop_bus_clock(ctrlr))){
	  printk("<1>stop bus clock failed -rw_ioreg\n");
    goto error;
  }
  argh = 
    (ioreg->rw ? (1 << 15) : 0)         |
    (ioreg->func << 12)                 |
    (ioreg->rw == 2 ? (1 << 11) : 0)    |
    ((ioreg->reg & 0x0001ff80) >> 7);
  argl =
    ((ioreg->reg & 0x0000007f) << 9)    |
    (ioreg->rw ? ioreg->dat : 0);
  
  MMC_CMD = CMD(52);
  MMC_ARGH = argh;
  MMC_ARGL = argl;
  wmb();
#define CARD_RESET 8
  if((ioreg->reg == IO_ABORT_REG) && ioreg->rw && (ioreg->func == FN0) && ((ioreg->dat & CARD_RESET) == CARD_RESET))
    MMC_CMDAT = MMC_CMDAT_SDIO_INT_EN;
  else
    MMC_CMDAT = MMC_CMDAT_R1;  /* R5 */

  MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD52(0x%04x%04x)\n", argh, argl);
  if((ret = complete_cmd(ctrlr, MMC_R5, FALSE))){
	  printk("<1>complete_cmd failed\n");
#ifdef SDIO_DEBUG1
	  PRINTK2("<1>complete_cmd failed\n");
#endif
    goto error;
  }
  if(ioreg->rw == IOREG_READ || ioreg->rw == IOREG_WRITE_READ)
    ioreg->dat = PXA_MMC_RESPONSE(ctrlr, 1);

 error:
  if(ret) printk("rw_ioreq(): ret=%d\n",ret);
  

  __LEAVE("ret=%d", ret);

  return ret;
}

static int rw_iomem(mmc_controller_t ctrlr,
			     data_transfer_req_t transfer)
{
  u32 cmdat_temp;
  int ret = -ENODEV,ret1=0;
  u16 argh = 0UL, argl = 0UL;
  ssize_t bytecnt;
 // pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();
	  
//printk("enter rw_iomem\n");

  /* RPACE changed 7f to 1fff per yellow book */
  //printk("<1>MMC_STAT enter Val = %x\n",(MMC_I_MASK & 0x1fff));
  /* CMD53 */
  argh = 
    (transfer->rw << 15)                   |
    (transfer->func << 12)                 |
    (transfer->blockmode << 11)            |
    (transfer->opcode << 10)               |
    ((transfer->reg & 0x0001ff80) >> 7);
  argl =
    ((transfer->reg & 0x0000007f) << 9)    |
    (transfer->cnt & 0x1ff);
  // really stop the clock - causes DMA transfers > 64 bytes to
  // work - don't know why though
	//sanity check for the DMA .... stop bus clock ... Required to do a DMA

  //need to handle the error
  if((ret = stop_bus_clock_2(ctrlr))) {
	ret1 = -1;
	goto error; 
  }

  MMC_CMD = CMD(53);
  MMC_ARGH = argh;
  MMC_ARGL = argl;
  cmdat_temp = 
    MMC_CMDAT_R1 | MMC_CMDAT_BLOCK | MMC_CMDAT_DATA_EN |
    (transfer->rw ? MMC_CMDAT_WRITE : MMC_CMDAT_READ);
  if(mode == 1)
    cmdat_temp |= MMC_CMDAT_SD_1DAT_EN;
  else
    cmdat_temp |= MMC_CMDAT_SD_4DAT_EN;
  if(transfer->blockmode) {
    MMC_BLKLEN = transfer->blksz;
    MMC_NOB = transfer->cnt;
  } else {
    MMC_BLKLEN = transfer->cnt;
    MMC_NOB = 1;
  }
#ifndef PIO
  cmdat_temp |= MMC_CMDAT_MMC_DMA_EN;
#endif
  wmb();
  MMC_CMDAT = cmdat_temp;

  MMC_DEBUG(MMC_DEBUG_LEVEL2, "CMD53(0x%04x%04x)\n", argh, argl);
  if((ret = complete_cmd(ctrlr, MMC_R5, FALSE)))
    goto error;

	//printk("Got the END_CMD_RES for the command 53 ->1\n");

  if(transfer->blockmode)
    bytecnt = transfer->blksz * transfer->cnt;
  else
    bytecnt = transfer->cnt * 1;
 
  /***************************************/
  /* Start transferring data on DAT line */
  /***************************************/
  while(bytecnt > 0) {

    if(transfer->rw == 0) {
      /* READ */
      MMC_DEBUG(MMC_DEBUG_LEVEL1, "read_buffer of %d\n", bytecnt);
      if((ret = read_buffer(ctrlr, bytecnt)) <= 0){
	      printk("<1>HWAC Read buffer error\n");
	      ret1 = ret;
	      goto error;
	      //break;
	      //continue;
	//goto abort;
      }
      MMC_DEBUG(MMC_DEBUG_LEVEL1, "read %d\n",ret);
      if((ret = copy_from_buffer(ctrlr, transfer->type, transfer->buf, ret)) < 0)
	goto error;
    } else {
      /* WRITE */    
      if((ret = copy_to_buffer(ctrlr, transfer->type, transfer->buf, bytecnt)) < 0)
	goto error;
      if((ret = write_buffer(ctrlr, ret)) <= 0){
	      printk("HWAC Write buffer error\n");
	      ret1 = ret;
	goto error;
      }
    }

    transfer->buf += ret;
    bytecnt -= ret;
  }

  if(__set_state(ctrlr, PXA_MMC_FSM_END_IO)){
	  printk("<1>__set_state failed rw_iomem\n");
    goto error;
  }
  
  if((ret = complete_io(ctrlr, transfer->rw))){
	printk("<1>complete_io failed rw_iomem\n");
	goto error;
  } 

/*  printk("<1>MMC_STAT= %x\n",MMC_STAT);
  printk("<1>state  %s\n",
            PXA_MMC_STATE_LABEL(hostdata->state));*/
  return 0;
error:
  if(__set_state(ctrlr, PXA_MMC_FSM_END_IO)){
	  printk("<1>__set_state failed rw_iomem\n");
  }

  if((ret = complete_io(ctrlr, transfer->rw))){
	  //printk("<1>complete_io failed rw_iomem\n");
  } 
//  printk("<1>MMC_I_MASK ret Val = %x\n",(MMC_I_MASK & 0x1fff));
//  printk("MMC_I_REG=%x MMC_STAT =%x\n",MMC_I_REG,MMC_STAT);
//  printk("<1>state  %s\n",
//            PXA_MMC_STATE_LABEL(hostdata->state));

  

  __LEAVE("ret=%d", ret);
	//printk("Leave rw_iomem\n");
  return ret1;
}

static void irq(int irq, void *dev_id, struct pt_regs *regs)
{

  u32 save_ireg;
  mmc_controller_t ctrlr = (mmc_controller_t) dev_id;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
  int sdio_scheduled = 0;
#ifdef PIO
  register int i, cnt;
  register char *buf;
#endif

  
  /* RPACE adder */
  

	irqnum++;
  /* get and save the current interrupt register, but
     don't change what the driver's state machine thinks
     the interrupt register contains if this interrupt is
     *only* the SDIO card interrupt - not having this code
     will cause problems with the driver state machine and
     "timing issue" effects */
  save_ireg = MMC_I_REG;
  if ( save_ireg != MMC_I_REG_SDIO_INT )
       hostdata->mmc_i_reg = save_ireg;

  hostdata->mmc_stat = MMC_STAT;
  hostdata->mmc_cmdat = MMC_CMDAT;
  hostdata->mmc_i_mask = MMC_I_MASK;
#ifdef PIO
  MMC_DEBUG(MMC_DEBUG_LEVEL3,
            "MMC interrupt: state=%s, status=%x, i_reg=%x,"
            " pos=%p, cnt=%d\n", PXA_MMC_STATE_LABEL(hostdata->state),
            hostdata->mmc_stat, save_ireg,
            hostdata->iobuf.buf.pos, hostdata->iobuf.buf.cnt);
#else /* DMA */
/*  MMC_DEBUG(MMC_DEBUG_LEVEL3,
            "MMC interrupt: state=%s status=%x i_reg=%x i_mask=%x cmdat=%x at %lu\n",
            PXA_MMC_STATE_LABEL(hostdata->state),
	    hostdata->mmc_stat, save_ireg, hostdata->mmc_i_mask,
	                hostdata->mmc_cmdat, jiffies); */
 /* 
  printk("MMC interrupt: state=%s status=%x i_reg=%x i_mask=%x cmdat=%x at %lu\n",
            PXA_MMC_STATE_LABEL(hostdata->state),
	    hostdata->mmc_stat, save_ireg, hostdata->mmc_i_mask, 
	    hostdata->mmc_cmdat, jiffies);
 */
#endif
//#if CONFIG_MMC_DEBUG_IRQ
#if IRQ_DEBUG
/*
  if(ps_sleep_confirmed) {
     if(save_ireg!=0x807 || (MMC_I_MASK!=0x17ff))
        printk("<1>sdio.c irq(%ld) SLEEP Confirmed mmc_i_reg=%x(%x), int=%d, STAT=%x, MASK=%x cnt=%d\n", irqnum,hostdata->mmc_i_reg,save_ireg,sdio_interrupt_enabled, MMC_STAT, MMC_I_MASK, hostdata->irqcnt);
  }
*/
  if(save_ireg & MMC_I_REG_RES_ERR) {
	u32 sreg = MMC_STAT;
	u32 old_ireg = MMC_I_MASK;
	u32 new_ireg = old_ireg | MMC_I_MASK_RES_ERR;
	u32 icmr = ICMR;
	u32 iclr = ICLR;
	u32 icip = ICIP;
	u32 icpr = ICPR;
	u32 iccr = ICCR;
	MMC_I_MASK = new_ireg;
	wmb();
        printk("<1>irq(%ld) mmc_i_reg=%x(%x), int=%d, MMC_STAT=%x, MASK=%x->%x(%x), ICMR=%x ICLR=%x ICIP=%x ICPR=%x ICCR=%x %d ps_sleep_confirmed=%d\n", irqnum,hostdata->mmc_i_reg,save_ireg,sdio_interrupt_enabled, sreg, old_ireg,new_ireg,MMC_I_MASK, icmr,iclr,icip,icpr,iccr,hostdata->irqcnt,ps_sleep_confirmed);
  }
  if(save_ireg & MMC_I_REG_DAT_ERR) {
	u32 sreg = MMC_STAT;
	u32 old_ireg = MMC_I_MASK;
	u32 new_ireg = old_ireg | MMC_I_MASK_DAT_ERR;
	u32 icmr = ICMR;
	u32 iclr = ICLR;
	u32 icip = ICIP;
	u32 icpr = ICPR;
	u32 iccr = ICCR;
	MMC_I_MASK = new_ireg;
	wmb();
        printk("<1>irq(%ld) mmc_i_reg=%x(%x), int=%d, MMC_STAT=%x, MASK=%x->%x(%x), ICMR=%x ICLR=%x ICIP=%x ICPR=%x ICCR=%x %d ps_sleep_confirmed=%d\n", irqnum,hostdata->mmc_i_reg,save_ireg,sdio_interrupt_enabled, sreg, old_ireg,new_ireg,MMC_I_MASK, icmr,iclr,icip,icpr,iccr,hostdata->irqcnt,ps_sleep_confirmed);
  }
  if(!(hostdata->irqcnt++ & 0x00f)) {
       /*RPACE change */
       /* printk(__FUNCTION__); */
       printk("<1>irq(%ld) mmc_i_reg, 0x%x, 0x%x, %d\n", irqnum,save_ireg, MMC_I_MASK, hostdata->irqcnt);
       printk("<1> irq(): irqcnt exceeded\n");
       panic("stoping kernel");
    goto complete;
  }
#endif

 
  /* RPACE, did a card interrupt occur ? */
  if (  save_ireg & MMC_I_REG_SDIO_INT ) {
      /* check to make sure we've actually had a
         registration */
      if ( mmccard_handler != (void *)0) {
	  if((save_ireg & MMC_I_REG_RES_ERR) || (save_ireg & MMC_I_REG_DAT_ERR))
            printk("<1> SDIO Card interrupt w/ %s %s at %lu,irqnum=%ld\n", (save_ireg&MMC_I_REG_RES_ERR)?"RES_ERR":"",(save_ireg&MMC_I_REG_DAT_ERR)?"DAT_ERR":"",jiffies,irqnum);
          mmccard_handler(irq, mmccard_devid, regs);
	  sdio_scheduled = 1;
          goto complete;
      }
      else {
      	  printk("<1> Card int not regstred yet\n");
      }
  } 

  switch (hostdata->state) {
  case PXA_MMC_FSM_IDLE:
  case PXA_MMC_FSM_CLK_OFF:
  case PXA_MMC_FSM_END_IO:
  case PXA_MMC_FSM_END_BUFFER:
  case PXA_MMC_FSM_END_CMD:
    goto complete;
#ifdef PIO
  case PXA_MMC_FSM_BUFFER_IN_TRANSIT:
    if(hostdata->mmc_stat & MMC_STAT_ERRORS)
      goto complete;
    
    buf = hostdata->iobuf.buf.pos;
    cnt = (hostdata->iobuf.buf.cnt < 32) ? hostdata->iobuf.buf.cnt : 32;
    if(hostdata->mmc_cmdat & MMC_CMDAT_WRITE) {
      if(!(hostdata->mmc_stat & MMC_STAT_XMIT_FIFO_EMPTY))
        break;
      for(i = 0; i < cnt; i++)
        MMC_TXFIFO = *buf++;
      if(cnt < 32)
        MMC_PRTBUF = MMC_PRTBUF_BUF_PART_FULL;
    } else {                    /* i.e. MMC_CMDAT_READ */
      if(!(hostdata->mmc_stat & MMC_STAT_RECV_FIFO_FULL))
        break;
      for(i = 0; i < cnt; i++)
        *buf++ = MMC_RXFIFO;
    }
    
    hostdata->iobuf.buf.pos = buf;
    hostdata->iobuf.buf.cnt -= i;
    if(hostdata->iobuf.buf.cnt <= 0) {
      (void) __set_state(ctrlr, PXA_MMC_FSM_END_BUFFER);
      MMC_DEBUG(MMC_DEBUG_LEVEL3, "buffer transferred\n");
      goto complete;
    }
    break;
#endif /* PIO */
  default:
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "unexpected state %d\n", hostdata->state);
    goto complete;
  }
  return;
complete:
  /* if a handler has registered then let SDIO Interrupts thrugh */
  if ((mmccard_handler != (void *)0) && (sdio_interrupt_enabled != 0)) {
     if (sdio_scheduled == 0) {
           MMC_I_MASK = MMC_I_MASK_ALL & (~MMC_I_MASK_SDIO_INT);
     }
     else {
	MMC_I_MASK = MMC_I_MASK_ALL;
     }
  }
  else {
      MMC_I_MASK = MMC_I_MASK_ALL; 
  }
  wmb();
//This printk has the timing effect !!

/*  printk("lvMMC interrupt status=%x i_reg=%x i_mask=%x sdio_schedul=%d at %lu\n", hostdata->mmc_stat,save_ireg, MMC_I_MASK, sdio_scheduled, jiffies);*/

#ifndef PIO
  if (hostdata->state != PXA_MMC_FSM_BUFFER_IN_TRANSIT) 
#endif
	  complete(&hostdata->completion);
  /* RPACE adder */
  

  return;
}

#ifndef PIO
static void dma_irq(int irq, void *dev_id, struct pt_regs *regs)
{
  mmc_controller_t ctrlr = (mmc_controller_t) dev_id;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
  u32 dcsr;
  u32 ddadr;
  int chan = hostdata->iobuf.buf.chan;

  /* RPACE adder */
  

  ddadr = DDADR(chan);
  dcsr = DCSR(chan);
  DCSR(chan) = dcsr & ~DCSR_STOPIRQEN;

  MMC_DEBUG(MMC_DEBUG_LEVEL3, "MMC DMA interrupt: chan=%d ddadr=0x%08x "
            "dcmd=0x%08x dcsr=0x%08x\n", chan, ddadr, DCMD(chan), dcsr);
 	//printk("."); 
  /*printk("<1>MMC DMA interrupt: chan=%d ddadr=0x%08x "
            "dcmd=0x%08x dcsr=0x%08x\n", chan, ddadr, DCMD(chan), dcsr);*/
  
  /* bus error */
  if(dcsr & DCSR_BUSERR) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "bus error on DMA channel %d\n", chan);
    printk("<1>bus error on DMA channel %d\n", chan);
    __set_state(ctrlr, PXA_MMC_FSM_ERROR);
    goto complete;
  }
  /* data transfer completed */
  if(dcsr & DCSR_ENDINTR) {
    MMC_DEBUG(MMC_DEBUG_LEVEL3, "buffer transferred\n");
    __set_state(ctrlr, PXA_MMC_FSM_END_BUFFER);
    goto complete;
  }
  return;
complete:

#ifndef DMA_NO_WAIT
  complete(&hostdata->completion);
#else   
  /* Signal end of DMA transfer */
  dma_int_flag = 1;
#endif 
  /* RPACE adder */
  

  return;
}
#endif

#if defined(CONFIG_ARCH_LUBBOCK) && defined(USE_SD_CD)

static void cd_irq(int irq, void *dev_id, struct pt_regs *regs) {
  /* needs to be implemented */
}

#endif

static int init(mmc_controller_t ctrlr)
{
  int ret = -ENODEV;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;
  unsigned long p_addr;
#ifndef PIO
  register int i;
  register pxa_dma_desc *desc;
#endif
  __ENTER0();

   /* RPACE adder */
   p_addr = virt_to_phys(marvel_debug_hits);
   //printk("marvel_debug_hits virtual= 0x%x, physical=0x%x\n", marvel_debug_hits, p_addr);

  /* RPACE adder */
  

  /* hardware initialization */
  /* I. prepare to transfer data */
  /* 1. allocate buffer */
#ifndef PIO
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: DMA transfers\n");
  
#ifdef	CONFIG_ARM_2_4_21
  hostdata->iobuf.buf.read_desc = consistent_alloc(GFP_KERNEL,
                                                   (PXA_MMC_IODATA_SIZE >> 5)
                                                   * sizeof(pxa_dma_desc),
                                                   &hostdata->iobuf.buf.
                                                   read_desc_phys_addr, 0);
#else
  hostdata->iobuf.buf.read_desc = consistent_alloc(GFP_KERNEL,
                                                   (PXA_MMC_IODATA_SIZE >> 5)
                                                   * sizeof(pxa_dma_desc),
                                                   &hostdata->iobuf.buf.
                                                   read_desc_phys_addr);
#endif
  if(!hostdata->iobuf.buf.read_desc) {
    ret = -ENOMEM;
    goto error;
  }
#ifdef	CONFIG_ARM_2_4_21
  hostdata->iobuf.buf.write_desc = consistent_alloc(GFP_KERNEL,
                                                    (PXA_MMC_IODATA_SIZE >> 5)
                                                    * sizeof(pxa_dma_desc),
                                                    &hostdata->iobuf.buf.
                                                    write_desc_phys_addr, 0);
#else
  hostdata->iobuf.buf.write_desc = consistent_alloc(GFP_KERNEL,
                                                    (PXA_MMC_IODATA_SIZE >> 5)
                                                    * sizeof(pxa_dma_desc),
                                                    &hostdata->iobuf.buf.
                                                    write_desc_phys_addr);
#endif
  if(!hostdata->iobuf.buf.write_desc) {
    ret = -ENOMEM;
    goto error;
  }
  
#ifdef _USE_INTERNAL_SRAM
  hostdata->iobuf.iodata = (void *)SRAM_MEM_VIRT;
  hostdata->iobuf.buf.phys_addr = SRAM_MEM_PHYS;
#else
#ifdef	CONFIG_ARM_2_4_21
  hostdata->iobuf.iodata = consistent_alloc(GFP_ATOMIC,
                                            PXA_MMC_IODATA_SIZE,
                                            &hostdata->iobuf.buf.phys_addr, 0);
#else
  hostdata->iobuf.iodata = consistent_alloc(GFP_ATOMIC,
                                            PXA_MMC_IODATA_SIZE,
                                            &hostdata->iobuf.buf.phys_addr);
#endif /* CONFIG_ARM_2_4_21 */
#endif //_USE_INTERNAL_SRAM
#else
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: PIO transfers\n");
  hostdata->iobuf.iodata = kmalloc(PXA_MMC_IODATA_SIZE, GFP_ATOMIC);
#endif
  if(!hostdata->iobuf.iodata) {
    ret = -ENOMEM;
    goto error;
  }
  /* 2. initialize iobuf */
  hostdata->iobuf.blksz = PXA_MMC_BLKSZ_MAX;
  hostdata->iobuf.bufsz = PXA_MMC_IODATA_SIZE;
  hostdata->iobuf.nob = PXA_MMC_BLOCKS_PER_BUFFER;
#ifndef PIO
    p_addr = virt_to_phys(&dma_irq);
//    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: request DMA channel, v_dma_irq=0x%x, p_dma_irq=0x%x\n",(void *)&dma_irq,p_addr);
  /* request DMA channel */
  if((hostdata->iobuf.buf.chan = pxa_request_dma("MMC", DMA_PRIO_LOW,
                                                 dma_irq,
                                                 ctrlr)) < 0) {
    MMC_ERROR("failed to request DMA channel\n");
    goto error;
  }

  DRCMRRXMMC = hostdata->iobuf.buf.chan | DRCMR_MAPVLD;
  DRCMRTXMMC = hostdata->iobuf.buf.chan | DRCMR_MAPVLD;

  for(i = 0; i < ((PXA_MMC_IODATA_SIZE >> 5) - 1); i++) {
    desc = &hostdata->iobuf.buf.read_desc[i];
    desc->ddadr = hostdata->iobuf.buf.read_desc_phys_addr
      + ((i + 1) * sizeof(pxa_dma_desc));
    desc->dsadr = MMC_RXFIFO_PHYS_ADDR;
    desc->dtadr = hostdata->iobuf.buf.phys_addr + (i << 5);
    desc->dcmd = DCMD_FLOWSRC | DCMD_INCTRGADDR
      | DCMD_WIDTH1 | DCMD_BURST32 | (1 << 5);

    desc = &hostdata->iobuf.buf.write_desc[i];
    desc->ddadr = hostdata->iobuf.buf.write_desc_phys_addr
      + ((i + 1) * sizeof(pxa_dma_desc));
    desc->dsadr = hostdata->iobuf.buf.phys_addr + (i << 5);
    desc->dtadr = MMC_TXFIFO_PHYS_ADDR;
    desc->dcmd = DCMD_FLOWTRG | DCMD_INCSRCADDR
      | DCMD_WIDTH1 | DCMD_BURST32 | (1 << 5);
  }
  desc = &hostdata->iobuf.buf.read_desc[i];
  desc->ddadr = (hostdata->iobuf.buf.read_desc_phys_addr +
                 (i + 1) * sizeof(pxa_dma_desc)) | DDADR_STOP;
  desc->dsadr = MMC_RXFIFO_PHYS_ADDR;
  desc->dtadr = hostdata->iobuf.buf.phys_addr + (i << 5);
  desc->dcmd = DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_WIDTH1 | DCMD_BURST32 | (1 << 5);

  desc = &hostdata->iobuf.buf.write_desc[i];
  desc->ddadr = (hostdata->iobuf.buf.write_desc_phys_addr +
                 (i + 1) * sizeof(pxa_dma_desc)) | DDADR_STOP;
  desc->dsadr = hostdata->iobuf.buf.phys_addr + (i << 5);
  desc->dtadr = MMC_TXFIFO_PHYS_ADDR;
  desc->dcmd = DCMD_FLOWTRG | DCMD_INCSRCADDR | DCMD_WIDTH1 | DCMD_BURST32 | (1 << 5);
#endif
  /* II. MMC */
  /*  1) request irq */
    p_addr = virt_to_phys(&irq);
//    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: address of v_irq=0x%x, p_irq=0x%x\n",(void *)&irq,p_addr);
  
  if(request_irq(IRQ_MMC, irq, 0, "SDIO controller", ctrlr)) {
    MMC_ERROR("failed to request IRQ_MMC\n");
    goto error;
  }

#if defined(CONFIG_ARCH_LUBBOCK) && defined(USE_SD_CD)
  if(request_irq(LUBBOCK_SD_IRQ, cd_irq, 0, "SDIO controller (CD)", ctrlr)) {
    free_irq(IRQ_MMC, ctlrl);
    free_irq(IRQ_SDIO, ctrlr);
    MMC_ERROR("failed to request LUBBOCK_SDIO\n");
    goto error;
  }
#endif

  /*  2) initialize h/w and ctrlr */
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: starting initialize h/w and ctrlr\n");
    p_addr = virt_to_phys(&init);
//    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: address of v_init=0x%x, p_init=0x%x\n",(void *)&init,p_addr);
    p_addr = virt_to_phys(&set_GPIO_mode);
//    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: address of v_set_GPIO_mode=0x%x, p_set_GPIO_mode=0x%x\n",(void *)&set_GPIO_mode,p_addr);
  
//  set_GPIO_mode(GPIO6_MMCCLK_MD);
//  set_GPIO_mode(GPIO8_MMCCS0_MD);
	
	/* Configure MMCLK bus clock output signal
	   ([2], 15.3, 24.4.2) */
	set_GPIO_mode(32 | GPIO_ALT_FN_2_OUT);

	/* Configure MMCMD command/response bidirectional signal
	   ([2], 15.3, 24.4.2) */
	set_GPIO_mode(112 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);

	/* Configure MMDAT[0123] data bidirectional signals
	   ([2], 15.3, 24.4.2) */
	set_GPIO_mode(92  | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
	set_GPIO_mode(109 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
	set_GPIO_mode(110 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
	set_GPIO_mode(111 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);

  CKEN |= CKEN12_MMC;           /* enable MMC unit clock */
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "RPACE: completed h/w and ctrlr initialize\n");

  ret = 0;
  goto out;
error:
#ifndef PIO
  /* free DMA resources */
  if(hostdata->iobuf.buf.chan >= 0) {
    DRCMRRXMMC = 0;
    DRCMRTXMMC = 0;
    pxa_free_dma(hostdata->iobuf.buf.chan);
  }
  if(hostdata->iobuf.iodata)
#ifdef _USE_INTERNAL_SRAM
    hostdata->iobuf.iodata = 0;
    hostdata->iobuf.buf.phys_addr = 0;
#else
    consistent_free(hostdata->iobuf.iodata,
                    PXA_MMC_IODATA_SIZE, hostdata->iobuf.buf.phys_addr);
#endif //_USE_INTERNAL_SRAM
  if(hostdata->iobuf.buf.read_desc)
    consistent_free(hostdata->iobuf.buf.read_desc, (PXA_MMC_IODATA_SIZE >> 5)
                    * sizeof(pxa_dma_desc),
                    hostdata->iobuf.buf.read_desc_phys_addr);
  if(hostdata->iobuf.buf.write_desc)
    consistent_free(hostdata->iobuf.buf.write_desc, (PXA_MMC_IODATA_SIZE >> 5)
                    * sizeof(pxa_dma_desc),
                    hostdata->iobuf.buf.write_desc_phys_addr);
#else
  kfree(hostdata->iobuf.iodata);
#endif
out:
  /* RPACE adder */
  

  __LEAVE("ret=%d", ret);
  return ret;
}

static void remove(mmc_controller_t ctrlr)
{
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();

  sdio_write_ioreg(ctrlr->stack.selected, FN0, IO_ABORT_REG, 0x08);

  /*  1) free buffer(s) */
#ifndef PIO
#ifdef _USE_INTERNAL_SRAM
    hostdata->iobuf.iodata = 0;
    hostdata->iobuf.buf.phys_addr = 0;
#else
  consistent_free(hostdata->iobuf.iodata, PXA_MMC_IODATA_SIZE,
                  hostdata->iobuf.buf.phys_addr);
#endif //_USE_INTERNAL_SRAM
  consistent_free(hostdata->iobuf.buf.read_desc, (PXA_MMC_IODATA_SIZE >> 5)
                  * sizeof(pxa_dma_desc),
                  hostdata->iobuf.buf.read_desc_phys_addr);
  consistent_free(hostdata->iobuf.buf.write_desc, (PXA_MMC_IODATA_SIZE >> 5)
                  * sizeof(pxa_dma_desc),
                  hostdata->iobuf.buf.write_desc_phys_addr);
  /*  2) release DMA channel */
  if(hostdata->iobuf.buf.chan >= 0) {
    DRCMRRXMMC = 0;
    DRCMRTXMMC = 0;
    pxa_free_dma(hostdata->iobuf.buf.chan);
  }
#else
  kfree(hostdata->iobuf.iodata);
#endif
  /* II. MMC */
  /*  1) release irq */
  free_irq(IRQ_MMC, ctrlr);
#if defined(CONFIG_ARCH_LUBBOCK) && defined(USE_SD_CD)
  free_irq(LUBBOCK_SD_IRQ, ctrlr);
#endif
  CKEN &= ~CKEN12_MMC;          /* disable MMC unit clock */
  kfree(marvel_debug_hits);

  __LEAVE0();
}

static int probe(mmc_controller_t ctrlr)
{
  int ret = -ENODEV;

  __ENTER0();
  /* TODO: hardware probe: i.g. read registers */
  ret = 1;

  __LEAVE("ret=%d", ret);
  return ret;
}

#ifdef OLD_CONFIG_PM

static int suspend(mmc_controller_t ctrlr)
{
	int ret;
	__ENTER0();	
	 ret = sdio_enter_deep_sleep(ctrlr->stack.selected);
#if 0
	printk("<1> Suspend called by OS\n");
        sdio_write_ioreg(ctrlr->stack.selected, FN1, 0x03, 0);
	mdelay(2);
        sdio_write_ioreg(ctrlr->stack.selected, FN1, 0x03, 1);

	__LEAVE0();
#endif
        return ret;
}
                                                                                

static void resume(mmc_controller_t ctrlr)
{
	__ENTER0();
	int ret;
	ret = sdio_exit_deep_sleep(ctrlr->stack.selected);
#if 0	
	printk("<1> Resume called by OS\n");

        sdio_write_ioreg(ctrlr->stack.selected, FN0, IO_ABORT_REG, 0);
	mdelay(2);
        sdio_write_ioreg(ctrlr->stack.selected, FN0, IO_ABORT_REG, 1);

	__LEAVE0();
#endif 
        return ;
}

#endif //OLD_CONFIG_PM

int sdio_suspend(mmc_card_t card)
{
	mmc_controller_t ctrlr = card->ctrlr;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;

	printk("Calling sdio_suspend\n");

	/* Check Are we in sleep already by Application ? */

	//Save the regesters and the mode GPIO mode
	SAVED_MMC_CLKRT = MMC_CLKRT;
	SAVED_MMC_I_MASK = MMC_I_MASK;
	SAVED_MMC_RESTO = MMC_RESTO;
	SAVED_MMC_SPI = MMC_SPI;
	SAVED_DRCMRRXMMC = DRCMRRXMMC;
	SAVED_DRCMRTXMMC = DRCMRTXMMC;
//	wmb();

	CKEN &= ~CKEN12_MMC; /* disable MMC unit clock */
	wmb();

	set_GPIO_mode(32);

	hostdata->suspended = TRUE;

	return 0;
}
EXPORT_SYMBOL(sdio_suspend);

int sdio_resume(mmc_card_t card)
{
	mmc_controller_t ctrlr = card->ctrlr;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;

	printk("Kernel Call Back Calling SD-Resume\n");

	/* Check if the application has called deep sleep off , then only 
	 * get out! */
	if ( hostdata->suspended == TRUE ) {
		//Resume all the reg back! 
		MMC_I_MASK = MMC_I_MASK_ALL;
		wmb();

		/* restore registers */
		set_GPIO_mode(32 | GPIO_ALT_FN_2_OUT);
		/* Configure MMCMD command/response bidirectional signal
		   ([2], 15.3, 24.4.2) */
		set_GPIO_mode(112 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);

		/* Configure MMDAT[0123] data bidirectional signals
		   ([2], 15.3, 24.4.2) */
		set_GPIO_mode(92  | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
		set_GPIO_mode(109 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
		set_GPIO_mode(110 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
		set_GPIO_mode(111 | GPIO_ALT_FN_1_IN | GPIO_ALT_FN_1_OUT);
		wmb();
		CKEN |= CKEN12_MMC; /* enable MMC unit clock */
		wmb();
		MMC_I_MASK = MMC_I_MASK_ALL;
		wmb();

		MMC_CLKRT = SAVED_MMC_CLKRT;
		wmb();
		MMC_RESTO = SAVED_MMC_RESTO;
		wmb();
		MMC_SPI = SAVED_MMC_SPI;
		wmb();
		DRCMRRXMMC = SAVED_DRCMRRXMMC;
		DRCMRTXMMC = SAVED_DRCMRTXMMC;
		wmb();

		MMC_I_MASK = SAVED_MMC_I_MASK & (~MMC_I_MASK_SDIO_INT);
		MMC_I_MASK  |= MMC_I_MASK_RES_ERR | MMC_I_MASK_END_CMD_RES;
                wmb();

		MMC_CMDAT = MMC_CMDAT_SDIO_INT_EN;
		wmb();

		MMC_CMD = CMD(0);
		wmb();

                mdelay(1);

		MMC_I_MASK = SAVED_MMC_I_MASK & (~MMC_I_MASK_SDIO_INT);
                wmb();

		hostdata->suspended = FALSE;
	}
	printk("Got out of SD-Resume\n");
	printk("in sdio_resume(), MMC_I_MASK = 0x%x.\n", MMC_I_MASK);
	return 0;
}
EXPORT_SYMBOL(sdio_resume);

int sdio_enter_bus_powersave(mmc_card_t card)
{
	int ret = -1;
	mmc_controller_t ctrlr = card->ctrlr;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;

	__ENTER0();
	
	/* check that low clock rate is not set */
	if(MMC_CLKRT==MMC_CLKRT_0_3125MHZ) {
		return 0;
	}
	
	/* save high clock rate */
	SAVED_MMC_HIGH_CLKRT = MMC_CLKRT;
	
	/* stop the clock */
	if(stop_bus_clock_2(ctrlr)){
		printk("<1>stop_bus_clock failed\n");
		goto error;
	}
	
	/* set low clock rate */
	MMC_CLKRT = MMC_CLKRT_0_3125MHZ;
	wmb();
	
	/* start the clock */ 
	if(start_bus_clock(ctrlr)){
		printk("<1>start_bus_clock failed\n");
		goto error;
	}
	
	ret = 0;
	
error:
	__LEAVE("ret=%d", ret);
	return ret;
}
EXPORT_SYMBOL(sdio_enter_bus_powersave);

int sdio_exit_bus_powersave(mmc_card_t card)
{
	int ret = -1;
	mmc_controller_t ctrlr = card->ctrlr;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	
	__ENTER0();
	
	/* check that high clock rate is not set */
	if(MMC_CLKRT==SAVED_MMC_HIGH_CLKRT) {
		return 0;
	}
	
	/* stop the clock */
	if(stop_bus_clock_2(ctrlr)){
		printk("<1>stop_bus_clock failed\n");
		goto error;
	}
	
	/* restore high clock rate */
	MMC_CLKRT = SAVED_MMC_HIGH_CLKRT;
	wmb();
	
	/* start the clock */ 
	if(start_bus_clock(ctrlr)){
		printk("<1>start_bus_clock failed\n");
		goto error;
	}
	
	ret = 0;
	
error:
	__LEAVE("ret=%d", ret);
	return ret;
}
EXPORT_SYMBOL(sdio_exit_bus_powersave);

 /* New API's can be called from the private ioctls or 
  * from kernel PM API's */

int sdio_enter_deep_sleep(mmc_card_t card)
{
//	if(!deep_sleep_count){
//		deep_sleep_count++;
		//printk("<1> Enter sleep called by OS\n");
		sdio_write_ioreg(card, FN1, 0x03, 0);
		mdelay(2);
	        sdio_write_ioreg(card, FN1, 0x03, 1);
		return 0;
//	}
//	else {
//		//printk("Already in deep sleep\n");
//		deep_sleep_count++;
//		return 0;
//	}
}
EXPORT_SYMBOL(sdio_enter_deep_sleep);

int sdio_exit_deep_sleep(mmc_card_t card)
{
//	--deep_sleep_count;
//	if(!deep_sleep_count){
	//	printk("<1> Resume called by OS\n");
	        sdio_write_ioreg(card, FN0, IO_ABORT_REG, 0);
		mdelay(2);
	        sdio_write_ioreg(card, FN0, IO_ABORT_REG, 1);
		return 0;
//	}
//	else 
//		return 0;
}
EXPORT_SYMBOL(sdio_exit_deep_sleep);

#if 0
static int suspend(mmc_controller_t ctrlr)
{
  int ret = -EBUSY;
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();

  MMC_DEBUG(MMC_DEBUG_LEVEL2, "state=%s\n",
	    PXA_MMC_STATE_LABEL(hostdata->state));

  if(hostdata->state == PXA_MMC_FSM_IDLE) {
    /* save registers */
    SAVED_MMC_CLKRT = MMC_CLKRT;
    SAVED_MMC_RESTO = MMC_RESTO;
    SAVED_MMC_SPI = MMC_SPI;
    SAVED_DRCMRRXMMC = DRCMRRXMMC;
    SAVED_DRCMRTXMMC = DRCMRTXMMC;

    set_GPIO_mode(GPIO6_MMCCLK);
    set_GPIO_mode(GPIO8_MMCCS0);
    CKEN &= ~CKEN12_MMC;        /* disable MMC unit clock */
    
    hostdata->suspended = TRUE;
    ret = 0;
  }
  
  goto error;
  
 error:
  __LEAVE("ret=%d", ret);
  return ret;
}

static void resume(mmc_controller_t ctrlr)
{
  pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t) ctrlr->host_data;

  __ENTER0();

  if(hostdata->suspended == TRUE) {
    set_GPIO_mode(GPIO6_MMCCLK_MD);
    set_GPIO_mode(GPIO8_MMCCS0_MD);
    CKEN |= CKEN12_MMC;         /* enable MMC unit clock */

    /* restore registers */
    MMC_CLKRT = SAVED_MMC_CLKRT;
    MMC_RESTO = SAVED_MMC_RESTO;
    MMC_SPI = SAVED_MMC_SPI;
    DRCMRRXMMC = SAVED_DRCMRRXMMC;
    DRCMRTXMMC = SAVED_DRCMRTXMMC;

    hostdata->suspended = FALSE;

    update_card_stack(ctrlr->slot); /* FIXME */
  }

  __LEAVE0();
  return;
}
#endif

static void setup_irq_line(mmc_controller_t ctrlr) {
  /* RPACE commented out for new yellow book design */
  // set_GPIO_IRQ_edge(SDIO_IRQ_GPIO, GPIO_FALLING_EDGE);
  ctrlr->tmpl->irq_line_ready = 1;
}

static mmc_controller_tmpl_rec_t controller_tmpl_rec = {
  owner:            THIS_MODULE,
  name:             "PXA270-SDIO",
  block_size_max:   PXA_MMC_BLKSZ_MAX,
  nob_max:          PXA_MMC_NOB_MAX,
  probe:            probe,
  init:             init,
  remove:           __devexit_p(remove),
#ifdef OLD_CONFIG_PM
  suspend:          suspend,
  resume:           resume,
#endif /* OLD_CONFIG_PM */
  update_acq:       update_acq,
  init_card_stack:  init_card_stack,
  check_card_stack: check_card_stack,
  setup_card:       setup_card,
  rw_iomem:         rw_iomem,
  rw_ioreg:         rw_ioreg,
  irq_line:         IRQ_SDIO,
  irq_line_ready:   0,
  setup_irq_line:   setup_irq_line
};

static int __devinit drv_module_init(void)
{
  int ret = -ENODEV;

  __ENTER0();

  switch(mode) {
  case 4:
    break;
  case 1:
    break;
  default:
    mode = 4;
    break;
  }

#ifdef _MAINSTONE
  printk("MST_MSCWR1=%lx",MST_MSCWR1);
  //turn on power for MMC controller
  MST_MSCWR1 |= MST_MSCWR1_MMC_ON;
  //signals are routed to MMC controller
  MST_MSCWR1 &= ~MST_MSCWR1_MS_SEL;
  printk("  ==>  %lx\n",MST_MSCWR1);
#endif

  host = sdio_register(MMC_REG_TYPE_HOST, &controller_tmpl_rec,
		       sizeof(pxa_mmc_hostdata_rec_t));
  if(!host) {
    MMC_DEBUG(MMC_DEBUG_LEVEL0, "failed to register with MMC core\n");
    goto error;
  }

  printk(KERN_INFO "%sMMC controller -SDIO protocol(Marvell International Ltd.)\n",
         controller_tmpl_rec.name);

#ifdef _MAINSTONE_GPIO_IRQ
  if(host)
	init_gpio_irq(host);
#endif

  ret = 0;
 error:
  /* RPACE adder */
  

  __LEAVE("ret=%d", ret);
  return ret;
}

static void __devexit drv_module_cleanup(void)
{
  sdio_unregister(MMC_REG_TYPE_HOST, host);

#ifdef _MAINSTONE
  //turn off power for MMC controller
  MST_MSCWR1 &= ~MST_MSCWR1_MMC_ON;
#endif

#ifdef _MAINSTONE_GPIO_IRQ
  if(host)
	remove_gpio_irq(host);
#endif
}

EXPORT_SYMBOL(start_bus_clock);
EXPORT_SYMBOL(stop_bus_clock);
EXPORT_SYMBOL(stop_bus_clock_2);

MODULE_PARM(mode,"i");
MODULE_PARM(clkdiv,"i");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_DESCRIPTION("Marvell SD8385 SDIO Driver");
