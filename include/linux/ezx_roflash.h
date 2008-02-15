/*
 *  linux/include/linux/ezx_roflash.h 
 *
 *  Created by Susan Gu    0ct, 22 2002
 *  
 *  For multiple cramfs partitions in ezx project
 *  At repsent, there are three cramfs partitions in ezx project, they are: 
 *  1.  root file system
 *  2.  Language package
 *  3.  setup package 
*/

#ifndef EZX_ROFLASH_FS_H
#define EZX_ROFLASH_FS_H

#include <asm/ioctl.h>

#define ROFLASH_MAJOR  62

#define ROFLASH_LINEAR  0x0010
#define ROFLASH_LINEAR_XIP     0x0011
#define ROFLASH_BLOCK   0x1000
#define ROFLASH_CHAR    0x1100
#define MAX_ROFLASH  4

typedef struct
{
	char name[16];  /* This length should be enough for DC stuff */
	unsigned long offset;
	unsigned long size;
	int (*roflash_read)(void *, unsigned long, size_t, size_t *, u_char *);
	/* Added by Susan for multiple Linear or block cramfs */
	void *priv_map;
	unsigned long phys_addr;
	unsigned short l_x_b;
}roflash_area;

extern roflash_area *roflash_get_dev(unsigned char minor);
extern roflash_area **roflash_table_pptr;

#endif
