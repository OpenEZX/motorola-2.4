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

typedef struct
{
	char name[16];  /* This length should be enough for DC stuff */
	unsigned long offset;
	unsigned long size;
	int (*roflash_read)(unsigned long, size_t, size_t *, u_char *);
}roflash_area;

#define MAX_CRAMFS  4

#endif
