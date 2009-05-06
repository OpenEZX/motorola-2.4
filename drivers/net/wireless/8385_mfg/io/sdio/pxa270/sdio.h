/**
 * sdio.h:
 *
 * Copyright (C) 2000-2003, Marvell Semiconductor, Inc. All rights reserved.
 */
#ifndef __SDIO_H__
#define __SDIO_H__

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/slab.h>

#include <linux/spinlock.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <pcmcia/cs_types.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cs.h>

#include <asm/semaphore.h>

#include <sdiotypes.h>
#include <sdioerror.h>
#include <sdiospec.h>

/* RPACE adder */
//#include <asm/proc/cache.h>


#if 0
#define CONFIG_MMC_DEBUG 1
#define CONFIG_MMC_DEBUG_VERBOSE 2
#define CONFIG_MMC_DEBUG_IRQ 1
#else
#undef CONFIG_MMC_DEBUG
#undef CONFIG_MMC_DEBUG_VERBOSE
#undef CONFIG_MMC_DEBUG_IRQ
#endif

/* changed by RPACE */
#define CONFIG_MMC_DEBUG 1
#define CONFIG_MMC_DEBUG_IRQ 1
// #define CONFIG_MMC_DEBUG_VERBOSE (4)
#define CONFIG_MMC_DEBUG_VERBOSE (0)

/* added by RPACE */
#define MARVEL_DEBUG_HITS 150
#define MARVEL_DEBUG_HITS_LAST 		(MARVEL_DEBUG_HITS-1)
#define INIT_ENTER			1
#define INIT_EXIT			2
#define DRV_MODULE_INIT_ENTER		3
#define DRV_MODULE_INIT_EXIT		4
#define STOP_BUS_CLOCK_ENTER		5
#define STOP_BUS_CLOCK_EXIT		6
#define START_BUS_CLOCK_ENTER		7
#define START_BUS_CLOCK_EXIT		8
#define __UDATE_ACQ_ENTER		9
#define __UDATE_ACQ_EXIT		10
#define INIT_CARD_STACK_ENTER		11
#define INIT_CARD_STACK_EXIT		12
#define UPDATE_ACQ_ENTER		13
#define UPDATE_ACQ_EXIT			14
#define DRV_INIT_COMPLETION_ENTER	15
#define DRV_INIT_COMPLETION_EXIT	16
#define IRQ_ENTER			17
#define IRQ_EXIT			18
#define DMA_IRQ_ENTER			19
#define DMA_IRQ_EXIT			20
#define DRV_WAIT_FOR_COMPLETION_ENTER	21
#define DRV_WAIT_FOR_COMPLETION_EXIT	22
#define COMPLETE_CMD_ENTER		23
#define COMPLETE_CMD_EXIT		24
#define COMPLETE_IO_ENTER		25
#define COMPLETE_IO_EXIT		26
#define __UPDATE_CARD_STACK_ENTER	27
#define __UPDATE_CARD_STACK_EXIT	28
#define NOTIFY_ADD_ENTER		29
#define NOTIFY_ADD_EXIT			30
#define NOTIFY_REMOVE_ENTER		31
#define NOTIFY_REMOVE_EXIT		32
#define UPDATE_CARD_STACK_ENTER		33
#define UPDATE_CARD_STACK_EXIT		34
#define GET_CARD_ENTER			35
#define GET_CARD_EXIT			36
#define PUT_CARD_ENTER			37
#define PUT_CARD_EXIT			38
#define REGISTER_CONTROLLER_ENTER	39
#define REGISTER_CONTROLLER_EXIT	40
#define SDIO_REGISTER_ENTER		41
#define SDIO_REGISTER_EXIT		42
#define UNREGISTER_USER_ENTER		43
#define UNREGISTER_USER_EXIT		44
#define UNREGISTER_CONTROLLER_ENTER	45
#define UNREGISTER_CONTROLLER_EXIT	46
#define SDIO_UNREGISTER_ENTER		47
#define SDIO_UNREGISTER_EXIT		48
#define SDCTL_OP_ENTERED		49
#define SDCTL_OP_EXIT			50
#define DIRECT_IOCTL_ENTER		51
#define DIRECT_IOCTL_EXIT		52
#define SDIO_REQUEST_IRQ_ENTER		53
#define SDIO_REQUEST_IRQ_EXIT		54
#define SDIO_FREE_IRQ_ENTER		55
#define SDIO_FREE_IRQ_EXIT		56
#define WAIT_TIMEO_ENTER		57
#define WAIT_TIMEO_EXIT			58
#define CHECK_CARD_STACK_ENTER		59
#define CHECK_CARD_STACK_EXIT		60
#define SETUP_CARD_ENTER		61
#define SETUP_CARD_EXIT			62
#define READ_BUFFER_ENTER		63
#define READ_BUFFER_EXIT		64
#define WRITE_BUFFER_ENTER		65
#define WRITE_BUFFER_EXIT		66
#define INIT_STEP_1			67
#define INIT_STEP_2			68
#define INIT_STEP_3			69
#define INIT_STEP_4			70
#define ACQUIRE_IO_ENTER		71
#define ACQUIRE_IO_EXIT			72
#define RELEASE_IO_ENTER		73
#define RELEASE_IO_EXIT			74
#define READ_IMPORTANT_REGS_ENTER	75
#define READ_IMPORTANT_REGS_EXIT	76
#define __RW_IOREG_ENTER		77
#define __RW_IOREG_EXIT			78
#define SDIO_READ_IOREG_ENTER		79
#define SDIO_READ_IOREG_EXIT		80
#define SDIO_WRITE_IOREG_ENTER		81
#define SDIO_WRITE_IOREG_EXIT		82
#define WRITE_SDIO_READ_IOREG_ENTER	83
#define WRITE_SDIO_READ_IOREG_EXIT	84
#define __RW_IOMEM_ENTER		85
#define __RW_IOMEM_EXIT			86
#define SDIO_READ_IOMEM_ENTER		87
#define SDIO_READ_IOMEM_EXIT		88
#define SDIO_WRITE_IOMEM_ENTER		89
#define SDIO_WRITE_IOMEM_EXIT		90
#define REGISTER_USER_ENTER		91
#define REGISTER_USER_EXIT		92
#define REGISTER_CARD_ENTER		93
#define REGISTER_CARD_EXIT		94
#define COPY_FROM_BUFFER_ENTER		95
#define COPY_FROM_BUFFER_EXIT		96
#define COPY_TO_BUFFER_ENTER		97
#define COPY_TO_BUFFER_EXIT		98
#define RW_IOREG_ENTER			99
#define RW_IOREG_EXIT			100
#define RW_IOMEM_ENTER			101
#define RW_IOMEM_EXIT			102

#define CORE_MODULE_INIT_ENTER		(MARVEL_DEBUG_HITS_LAST-3)
#define CORE_MODULE_INIT_EXIT		(MARVEL_DEBUG_HITS_LAST-2)


#define MMC_CONTROLLERS_MAX (4)
#define MMC_CARDS_MAX (16)

/* Device minor number encoding:
 *   [7:6] - host
 *   [5:3] - card slot number
 *   [2:0] - partition number 
 */
#define MMC_MINOR_HOST_SHIFT (6)
#define MMC_MINOR_CARD_MASK (0x07)

/*
 *  MMC controller abstraction
 */
#ifdef  SDIO_DEBUG
#define PRINTK3  printk
#else
#define PRINTK3(args...)
#endif

enum _mmc_controller_state
{
  MMC_CONTROLLER_ABSENT = 0,
  MMC_CONTROLLER_FOUND,
  MMC_CONTROLLER_INITIALIZED,
  MMC_CONTROLLER_UNPLUGGED
};

enum _mmc_dir {
  MMC_READ = 1,
  MMC_WRITE
};

enum _mmc_buftype
{
  MMC_USER = 1,
  MMC_KERNEL
};

enum _cmd53_rw {
  IOMEM_READ  = 0,
  IOMEM_WRITE = 1
};

enum _cmd53_mode {
  BLOCK_MODE = 1,
  BYTE_MODE  = 0
};

enum _cmd53_opcode {
  FIXED_ADDRESS = 0,
  INCREMENTAL_ADDRESS = 1
};

typedef struct data_transfer_req_rec_t
{
  u8 rw;               /* 0 = read, 1 = write */
  u8 func;             /* function number */
  u8 blockmode;        /* block mode */
  u8 opcode;           /* 0 = fixed address, 1 = incrementing address */
  u32 reg;             /* register address */
  ssize_t cnt;         /* number of bytes to transfer */
  ssize_t blksz;       /* block size */
  mmc_buftype_t type;  /* whether 'buf' user or kernel mem */
  u8 *buf;             /* pointer to caller's buffer */
} data_transfer_req_rec_t;

typedef data_transfer_req_rec_t *data_transfer_req_t;

enum _cmd52_rw {
  IOREG_READ       = 0,
  IOREG_WRITE      = 1,
  IOREG_WRITE_READ = 2
};

typedef struct _ioreg_req_rec_t
{
  u8  rw;              /* 0 = read, 1 = write, 2 = RAW (read after write) */
  u8  func;            /* function number */
  u32 reg;             /* register */
  u8  dat;             /* data (variable) */
} ioreg_req_rec_t;

typedef ioreg_req_rec_t *ioreg_req_t;

struct _mmc_controller_tmpl_rec
{
  struct module *owner;         /* driver module */
  char name[16];

  const ssize_t block_size_max; /* max acceptable block size */
  const ssize_t nob_max;        /* max blocks per one data transfer */

  int (*probe) (mmc_controller_t);      /* hardware probe */
  int (*init) (mmc_controller_t);       /* initialize, e.g. request irq, DMA and allocate buffers */
  void (*remove) (mmc_controller_t);    /* free resources */
#if 0 /* CONFIG_HOTPLUG */
  void (*attach) (void);
   /|*controller hotplug callbacks * |/void (*detach) (void);
#endif
#ifdef OLD_CONFIG_PM
  int (*suspend) (mmc_controller_t);    /* power management callbacks */
  void (*resume) (mmc_controller_t);
#endif

  int (*init_card_stack) (mmc_controller_t);
  int (*update_acq) (mmc_controller_t); /* update card stack management data */
  int (*check_card_stack) (mmc_controller_t);
  int (*setup_card) (mmc_controller_t, mmc_card_t);
  int (*rw_iomem) (mmc_controller_t, data_transfer_req_t);
  int (*rw_ioreg) (mmc_controller_t, ioreg_req_t);
  int irq_line;
  int irq_line_ready;
  void (*setup_irq_line)(mmc_controller_t);

};

#ifndef MMC_CTRLR_BLKSZ_DEFAULT
#define MMC_CTRLR_BLKSZ_DEFAULT (512)
#endif

#ifndef MMC_CTRLR_NOB_DEFAULT
#define MMC_CTRLR_NOB_DEFAULT (1)
#endif

struct _card_info_rec {

  mmc_type_t type;
  __u32 ocr;          /* Operating Condition Register */
  __u8  nio;          /* Number of functions */
  __u8  mem;          /* Memory Present? */
  __u16 rca;          /* Relative Card Address */
  __u16 fnblksz[8];   /* Function block sizes */
  __u32 cisptr[8];    /* CIS pointers */
  
};

struct _mmc_card_rec
{
  /* public card interface */
  struct _card_info_rec info;
  
  /* private kernel specific data */
  mmc_state_t state;            /* card's state as per last operation */
  mmc_card_t next;              /* link to the stack */
  mmc_controller_t ctrlr;       /* back reference to the controller */
  int usage;                    /* reference count */
  int slot;                     /* card's number for device reference */
  u8 chiprev;			/* chip revision number */
  u8 hw_tx_done;		/* hw tx done enabled */
  u8 block_size_512;		/* hw supports SDIO block size up to 512 bytes */
  u8 async_int_mode;		/* async. interrupt mode needed */
  /* TODO: async I/O queue */
#ifdef CONFIG_PROC_FS
  proc_dir_entry_t proc;
  char proc_name[16];
#endif

  /* card specific data */
  unsigned long card_data[0] __attribute__ ((aligned(sizeof(unsigned long))));
};

struct _mmc_card_stack_rec
{
  mmc_card_t first;             /* first card on the stack */
  mmc_card_t last;              /* last card on the stack */
  mmc_card_t selected;          /* currently selected card */
  int ncards;
};

struct _mmc_controller_rec
{
  mmc_controller_state_t state; /* found, initialized, unplugged... */
  int usage;                    /* reference count */
  int slot;                     /* host's number for device reference */
  semaphore_t io_sem;           /* I/O serialization */
  rwsemaphore_t update_sem;     /* card stack check/update serialization */

  mmc_controller_tmpl_t tmpl;   /* methods provided by the driver */
  mmc_card_stack_rec_t stack;   /* card stack management data */

  u32 rca_next;                 /* next RCA to assign */
  int slot_next;                /* next slot number to assign */
#ifdef CONFIG_PROC_FS
  char proc_name[16];
  proc_dir_entry_t proc;
#endif
    /* driver can request some extra space */
  unsigned long host_data[0] __attribute__ ((aligned(sizeof(unsigned long))));
};

/*
 * MMC core interface
 */
enum _mmc_reg_type
{
  MMC_REG_TYPE_USER = 1,
  MMC_REG_TYPE_HOST,
  MMC_REG_TYPE_CARD
};

struct _mmc_notifier_rec
{
  struct _mmc_notifier_rec *next;
  mmc_notifier_fn_t add;
  mmc_notifier_fn_t remove;
};

enum _mmc_response
{
  MMC_NORESPONSE = 1,
  MMC_R1,
  MMC_R2,
  MMC_R3,
  MMC_R4,
  MMC_R5,
  MMC_R6
};

#undef EXTERN
#ifndef __SDIO_CORE_IMPLEMENTATION__
#define EXTERN extern
#else
#define EXTERN                  /* empty */
#endif

EXTERN void *drv_register(mmc_reg_type_t, void *, size_t);
EXTERN void drv_unregister(mmc_reg_type_t, void *);
EXTERN int update_card_stack(int);

EXTERN void *sdio_register(mmc_reg_type_t reg_type, void *tmpl, size_t extra);
EXTERN void sdio_unregister(mmc_reg_type_t reg_type, void *tmpl);


EXTERN mmc_card_t get_card(int, int);   /* get reference to the card */
EXTERN void put_card(mmc_card_t);       /* release card reference */

EXTERN int notify_add(mmc_card_t);      /* user notification */
EXTERN int notify_remove(mmc_card_t);

EXTERN int sdio_read_ioreg(mmc_card_t, u8, u32, u8 *);
EXTERN int sdio_write_ioreg(mmc_card_t, u8, u32, u8);
EXTERN int sdio_read_iomem(mmc_card_t, u8, u32, u8, u8, ssize_t, ssize_t, u8 *);
EXTERN int sdio_write_iomem(mmc_card_t, u8, u32, u8, u8, ssize_t, ssize_t, u8 *);
EXTERN int sdio_get_first_tuple(mmc_card_t, u8, tuple_t *);
EXTERN int get_next_tuple(mmc_card_t, u8, tuple_t *);
EXTERN int sdio_get_tuple_data(mmc_card_t, u8, tuple_t *);
EXTERN int sdio_parse_tuple(mmc_card_t, u8, tuple_t *, cisparse_t *);
EXTERN int validate_cis(mmc_card_t, u8, tuple_t *, cisinfo_t *);
EXTERN int replace_cis(mmc_card_t, u8, cisdump_t *);
EXTERN int sdio_request_irq(mmc_card_t,
			    void (*)(int, void *, struct pt_regs *),
			    unsigned long, const char *, void *);
EXTERN int sdio_free_irq(mmc_card_t card, void *);
EXTERN int sdio_suspend(mmc_card_t card);
EXTERN int sdio_resume(mmc_card_t card);
void sdio_enable_SDIO_INT(void);

#undef EXTERN

static inline mmc_card_t __card_alloc(size_t extra)
{
  mmc_card_t ret = kmalloc(sizeof(mmc_card_rec_t) + extra, GFP_KERNEL);

  if(ret) {
    memset(ret, 0, sizeof(mmc_card_rec_t) + extra);
  }

  return ret;
}

static inline void __card_free(mmc_card_t card)
{
  if(card) {
    kfree(card);
  }
}

static inline mmc_card_stack_t __card_stack_init(mmc_card_stack_t stack)
{
  mmc_card_stack_t ret = NULL;
  if(stack) {
    memset(stack, 0, sizeof(mmc_card_stack_rec_t));
    ret = stack;
  }
  return ret;
}

static inline mmc_card_stack_t __card_stack_add(mmc_card_stack_t stack,
						     mmc_card_t card)
{
  mmc_card_stack_t ret = NULL;

  if(stack && card) {
    card->next = NULL;

    if(stack->first) {
      stack->last->next = card;
      stack->last = card;
    } else
      stack->first = stack->last = card;

    ++stack->ncards;
    ret = stack;
  }
  return ret;
}

static inline mmc_card_stack_t __card_stack_remove(mmc_card_stack_t stack,
							mmc_card_t card)
{
  mmc_card_stack_t ret = NULL;
  register mmc_card_t prev;
  int found = FALSE;

  if(!stack || !card)
    goto error;

  if(stack->ncards > 0) {
    if(stack->first == card) {
      stack->first = stack->first->next;
      if(stack->last == card)
        stack->last = stack->last->next;
      found = TRUE;
    } else {
      for(prev = stack->first; prev; prev = prev->next)
        if(prev->next == card) {
          found = TRUE;
          break;
        }
      if(found) {
        if(prev->next == stack->last)
          stack->last = prev->next;
        prev->next = prev->next->next;
      }
    }
    if(found) {
      --stack->ncards;
      ret = stack;
    }
  }
error:
  return ret;
}

static inline void __card_stack_free(mmc_card_stack_t stack)
{
  mmc_card_t card, next;

  if(stack && (stack->ncards > 0)) {
    card = stack->first;
    while(card) {
      next = card->next;
      kfree(card);
      card = next;
    }
    __card_stack_init(stack);
  }
}

static inline int __card_stack_foreach(mmc_card_stack_t stack,
					    mmc_notifier_fn_t fn,
					    int unplugged_also)
{
  int ret = 0;
  register mmc_card_t card = NULL;

  if(stack && fn) {
    for(card = stack->first; card; card = card->next)
      if((card->state != MMC_CARD_STATE_UNPLUGGED) || unplugged_also)
        if(fn(card)) {
          ret = -card->slot;
          break;
        }
  }

  return ret;
}

/*
 * Debugging macros
 */

 /* RPACE adder */
/* #define MV_TRAIL(array_entry) { marvel_debug_hits[array_entry]++; \
			clean_dcache_entry(&marvel_debug_hits[array_entry]); }
*/

#ifdef CONFIG_MMC_DEBUG

#define MMC_DEBUG_LEVEL0 (0)    /* major */
#define MMC_DEBUG_LEVEL1 (1)
#define MMC_DEBUG_LEVEL2 (2)    /* device */
#define MMC_DEBUG_LEVEL3 (3)    /* protocol */
#define MMC_DEBUG_LEVEL4 (4)    /* everything */

   /* RPACE changes */
#define MMC_DEBUG(n, args...)                 \
if(n <=  CONFIG_MMC_DEBUG_VERBOSE) {          \
  printk(KERN_NOTICE args);  \
}
/*  printk(__FUNCTION__);  */

#define __ENTER0( )             /* empty */
#define __LEAVE0( )             /* empty */
#define __ENTER(args...)        /* empty */
#define __LEAVE(args...)        /* empty */
// #define __ENTER0( ) printk(__FUNCTION__); MMC_DEBUG( MMC_DEBUG_LEVEL2, "entry\n" );
// #define __LEAVE0( ) printk(__FUNCTION__); MMC_DEBUG( MMC_DEBUG_LEVEL2, "exit\n" );
// #define __ENTER( format, args... ) printk(__FUNCTION__); MMC_DEBUG( MMC_DEBUG_LEVEL2, "entry: " format "\n", args );
// #define __LEAVE( format, args... ) printk(__FUNCTION__); MMC_DEBUG( MMC_DEBUG_LEVEL2, "exit: " format "\n", args );

#else /* CONFIG_MMC_DEBUG */
#define MMC_DEBUG(n, args...)   /* empty */
#define __ENTER0( )             /* empty */
#define __LEAVE0( )             /* empty */
#define __ENTER(args...)        /* empty */
#define __LEAVE(args...)        /* empty */
#define MMC_DUMP_CSD( card )    /* empty */
#endif /* CONFIG_MMC_DEBUG */

/* 
 * Miscellaneous defines
 */
#ifndef MMC_DUMP_R1
#define MMC_DUMP_R1( ctrlr )    /* empty */
#endif
#ifndef MMC_DUMP_R2
#define MMC_DUMP_R2( ctrlr )    /* empty */
#endif
#ifndef MMC_DUMP_R3
#define MMC_DUMP_R3( ctrlr )    /* empty */
#endif

#endif /* __KERNEL__ */

#endif
