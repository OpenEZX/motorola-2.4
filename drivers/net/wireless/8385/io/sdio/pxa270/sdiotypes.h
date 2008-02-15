/*
 * File: sdiotypes.h
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
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
 */

#ifndef __SDIOTYPES_H__
#define __SDIOTYPES_H__

#ifdef __KERNEL__
#include <linux/kdev_t.h>

typedef enum _mmc_reg_type mmc_reg_type_t;
typedef enum _mmc_response mmc_response_fmt_t;

/* MMC card private description */
typedef struct _mmc_card_rec mmc_card_rec_t;
typedef struct _mmc_card_rec *mmc_card_t;
typedef enum _mmc_dir mmc_dir_t;
typedef enum _mmc_buftype mmc_buftype_t;

/* notifier declarations */
typedef struct _mmc_notifier_rec mmc_notifier_rec_t;
typedef struct _mmc_notifier_rec *mmc_notifier_t;

typedef int (*mmc_notifier_fn_t) ( mmc_card_t );

/* MMC card stack */
typedef struct _mmc_card_stack_rec mmc_card_stack_rec_t;
typedef struct _mmc_card_stack_rec *mmc_card_stack_t;

typedef struct _mmc_data_transfer_req_rec mmc_data_transfer_req_rec_t;
typedef struct _mmc_data_transfer_req_rec *mmc_data_transfer_req_t;

/* MMC controller */
typedef struct _mmc_controller_tmpl_rec mmc_controller_tmpl_rec_t;
typedef struct _mmc_controller_tmpl_rec *mmc_controller_tmpl_t;

typedef enum _mmc_controller_state mmc_controller_state_t;
typedef struct _mmc_controller_rec mmc_controller_rec_t;
typedef struct _mmc_controller_rec *mmc_controller_t;

/* various kernel types */
typedef struct semaphore semaphore_t;
typedef struct rw_semaphore rwsemaphore_t;
typedef struct proc_dir_entry proc_dir_entry_rec_t;
typedef struct proc_dir_entry *proc_dir_entry_t;
typedef struct gendisk gendisk_rec_t;
typedef struct gendisk *gendisk_t;
#endif /* __KERNEL__ */

/* The following are originally from include/mmc/types.h */
typedef enum _mmc_type mmc_type_t;
typedef enum _mmc_state mmc_state_t;
typedef enum _mmc_transfer_mode mmc_transfer_mode_t;
typedef struct _mmc_card_csd_rec mmc_card_csd_rec_t;
typedef struct _mmc_card_cid_rec mmc_card_cid_rec_t;
typedef struct _mmc_card_info_rec mmc_card_info_rec_t;
typedef struct _mmc_card_info_rec *mmc_card_info_t;
typedef enum _mmc_error mmc_error_t;

/* The following are originally from include/mmc/mmc.h */

/*
 * MMC card type
 */
enum _mmc_type {
  MMC_CARD_TYPE_RO = 1,
  MMC_CARD_TYPE_RW,
  MMC_CARD_TYPE_IO
};

/*
 * MMC card state
 */
enum _mmc_state {
  MMC_CARD_STATE_IDLE = 1,
  MMC_CARD_STATE_READY,
  MMC_CARD_STATE_IDENT,
  MMC_CARD_STATE_STNBY,
  MMC_CARD_STATE_TRAN,
  MMC_CARD_STATE_DATA,
  MMC_CARD_STATE_RCV,
  MMC_CARD_STATE_DIS,
  MMC_CARD_STATE_UNPLUGGED=0xff
};

/*
 * Data transfer mode
 */
enum _mmc_transfer_mode {
  MMC_TRANSFER_MODE_STREAM = 1,
  MMC_TRANSFER_MODE_BLOCK_SINGLE,
  MMC_TRANSFER_MODE_BLOCK_MULTIPLE,
  MMC_TRANSFER_MODE_UNDEFINED = -1
};

struct _mmc_card_csd_rec { /* CSD register contents */
  /* FIXME: BYTE_ORDER */
  u8	ecc:2,
	    file_format:2,
			tmp_write_protect:1,
					  perm_write_protect:1,
							     copy:1,
								  file_format_grp:1;
  u64	content_prot_app:1,
			 rsvd3:4,
			       write_bl_partial:1,
						write_bl_len:4,
							     r2w_factor:3,
									default_ecc:2,
										    wp_grp_enable:1,
												  wp_grp_size:5,
													      erase_grp_mult:5,
															     erase_grp_size:5,
																	    c_size_mult:3,
																			vdd_w_curr_max:3,
																				       vdd_w_curr_min:3,
																						      vdd_r_curr_max:3,
																								     vdd_r_curr_min:3,
																										    c_size:12,
																											   rsvd2:2,
																												 dsr_imp:1,
																													 read_blk_misalign:1,
																															   write_blk_misalign:1,
																																	      read_bl_partial:1;

  u16	read_bl_len:4,
		    ccc:12;
  u8	tran_speed;
  u8	nsac;
  u8	taac;
  u8	rsvd1:2,
	      spec_vers:4,
			csd_structure:2;
};

struct _mmc_card_cid_rec { /* CID register contents */
  /* FIXME: BYTE_ORDER */
  u8	mdt_year:4,
		 mdt_mon:4;
  u32	psn;
  u8	prv_minor:4,
		  prv_major:4;
  u8	pnm[6];
  u16	oid;
  u8	mid;
};

/* 
 * Public card description
 */
struct _mmc_card_info_rec {
  mmc_type_t type;
  mmc_transfer_mode_t transfer_mode; /* current data transfer mode */
  __u16 rca;        /* card's RCA assigned during initialization */
  struct _mmc_card_csd_rec csd;
  struct _mmc_card_cid_rec cid;
  __u32 tran_speed; /* kbits */
  __u16 read_bl_len;
  __u16 write_bl_len;
  size_t capacity;    /* card's capacity in bytes */
};

/* 
 * Micsellaneous defines
 */
#ifndef SEEK_SET
#define SEEK_SET (0)
#endif

#ifndef SEEK_CUR
#define SEEK_CUR (1)
#endif

#ifndef SEEK_END
#define SEEK_END (2)
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#endif

