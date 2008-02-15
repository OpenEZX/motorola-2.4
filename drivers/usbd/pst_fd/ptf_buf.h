/*
 * ptf_fd/ptf_buf.h -header file for buffer management
 *
 * Copyright (c) 2003 Motorola
 *
 * By: a17400 Motorola
 *
 *Description:
 *header file for ptf buffer managerment
 *
 *---------File History:---------------
 *07 MAR 2003 		created by a17400
 *19 MAR 2003		add current data size for queue, to delete oldest data when buffer is full.l
 */
#ifndef PTF_BUF_H
#define PTF_BUF_H 1

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>

//buffer structure for each ptf device

struct ptf_buf_queue{
	struct list_head queue_head;	//queue head, we don't use it, and keep it undeleted, the first valid data element is indexed as queue_head->next.
	struct list_head queue_end;	//queue end, we don't use it, and keep it undeleted, the last valid data element is indexed as queue_end->prev.
	size_t cur_len;		//count current data size in buffer in byte.
	size_t max_size;		//store the max size of a queue in byte. it is summed by all elements' DATA length.
};


//element type in queue list. returned by list_entry()

struct ptf_buf_element {
	struct list_head list_node;
	unsigned char * data;
	size_t length;
	unsigned char * cur_pos;
};

//interface functions:

extern void ptf_buf_init(struct ptf_buf_queue*,size_t);
extern int ptf_buf_is_empty(struct ptf_buf_queue * );

extern int ptf_buf_get_data(unsigned char * , size_t , struct ptf_buf_queue *, int );
extern int ptf_buf_put_data(unsigned char * , size_t , struct ptf_buf_queue * );
extern void ptf_buf_queue_release(struct ptf_buf_queue * );

#endif
