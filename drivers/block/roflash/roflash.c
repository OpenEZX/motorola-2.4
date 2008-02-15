/*
 * linux/drivers/block/roflash.c
 *
 * Copyright (C) 2002 Motorola Inc.
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
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#include <linux/ezx_roflash.h>

#ifdef	CONFIG_PROC_FS
#include <linux/proc_fs.h>	/* For /proc/roflash_info */
#endif	/* CONFIG_PROC_FS */

#define MAJOR_NR 62
#define DEVICE_NAME "roflash"
#define DEVICE_REQUEST roflash_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM

#include <linux/blk.h>

/* K3 flash lowlevel */
#define FLASH_WORD_ALIGN_MASK 0xFFFFFFFE
#define WIDTH_ADJUST	0x0001L		/* I.E. Converts 0x90 to 0x90 */
#define FlashCommandRead        (unsigned short)(0xFF * WIDTH_ADJUST)
#define FlashCommandStatus       (unsigned short)(0x70 * WIDTH_ADJUST)
#define FlashStatusReady          (unsigned short)(0x80 * WIDTH_ADJUST)

/* Variables used to track operational state of ROFLASH */
#define ROFLASH_STATUS_READY			0
#define ROFLASH_STATUS_NOT_READY    -1
#define ROFLASH_BLOCK_SIZE (4096)
static unsigned long flash_blocksizes[MAX_CRAMFS];
static unsigned long flash_sizes[MAX_CRAMFS];

/* Temp buffer for holding Cramfs block data */
//static unsigned char roflashTempBuffer[ROFLASH_BLOCK_SIZE];

#ifdef	CONFIG_PROC_FS
static struct proc_dir_entry *roflash_proc_entry;
static int roflash_read_procmem(char *, char **, off_t, int, int *, void *);
#endif	/* CONFIG_PROC_FS */

extern unsigned long DALHART_K3_BASE_ADDR;
extern unsigned short roflash_partitions;

roflash_area **roflash_table_pptr = NULL;
int roflash_status = ROFLASH_STATUS_NOT_READY;
static int roflash_err = 0;

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <asm/unaligned.h>


void fmemoryCopy (void * dest, void * src, unsigned long bytes)
{
	unsigned char *new_dest;
	unsigned char *new_src;
	new_dest = (unsigned char *)dest;
	new_src = (unsigned char *)src;
	/* while bytes to modify is not complete */
	while (bytes-- != 0)
	{
		/* update destination byte to equal src byte */
		*new_dest = *new_src;
		new_dest++;
		new_src++;
	}
}
void memorySet (void * buff, unsigned long bytes, unsigned char value)
{
	unsigned char * new_buff;
	new_buff = (unsigned char *)buff;
	/* while not at end of modification */
	while (bytes-- != 0)
	{
		/* set contents of buffer to value specified */
		*new_buff = value;

		/* increment to next byte */
		new_buff++;
	}
}

unsigned short IntelSCSRead(unsigned long CardAddress, unsigned long Length, unsigned char * Buffer)
{
	unsigned long volatile pointer;
	unsigned short * volatile pointer0ed;

#ifdef VFM_DEBUG2
	if ( roflash_status == ROFLASH_STATUS_READY )
	{
		printk("IntelSCSRead:  PositionPtr(pointer, CardAddress);\n");
		printk("IntelSCSRead: pointer=%d, CardAddress=%d\n", (DWORD)pointer, (DWORD)CardAddress);
	}
#endif
	pointer = (unsigned long)(DALHART_K3_BASE_ADDR + CardAddress);

	pointer0ed = (unsigned short *)((unsigned long)pointer & FLASH_WORD_ALIGN_MASK);
#ifdef CONFIG_K3_DEBUG
	printk("IntelSCSRead(): Reading %d bytes at 0x%lx\n",Length, pointer);
#endif

	*pointer0ed = FlashCommandRead;  /* Ensure it is in read mode */

	/* This is assumimg that farmemory copy performs at a byte at a time */
	fmemoryCopy(Buffer, pointer, Length);

	return(0);
}

void roflash_request(request_queue_t *q)
{
    unsigned short minor;
    unsigned long offset, len, retlen;
	unsigned short err = 0;
	unsigned char *blockTempBufPtr;
	unsigned char *tmpBufPtr;
	unsigned short nrSectors;
	
#ifdef ROFLASH_DEBUG_ERR
	    printk(KERN_ERR "Roflash_request(): Entered\n");
#endif

	/* Loop here forever */
	/* Not really, Linux decides when to return inside *_REQUEST macros */
    for(;;) 
    {
		INIT_REQUEST;

		/* Verify minor number is in range */
		minor = MINOR(CURRENT->rq_dev);
		if (minor >= roflash_partitions) 
		{
#ifdef ROFLASH_DEBUG_ERR
		    printk(KERN_ERR "roflash_request(): ERROR! Out of partition range (minor = %d)\n", minor );
#endif
			end_request(0);
		    continue;
		}

		/* Verify block driver is ready to accept requests */
		if (roflash_status != ROFLASH_STATUS_READY)
		{
#ifdef ROFLASH_DEBUG_ERR
		    printk(KERN_ERR "roflash_request(): ERROR! roflash device minor %d not ready (VSB device driver failure?)!\n", minor );
			end_request(0);
		    continue;
#endif
		}

		/* Check if writting-request is demanded */
		if ( CURRENT->cmd == WRITE )
		{
			printk(KERN_ERR "roflash_request: write-accessing is not allowed!\n");
			end_request(0);
			break;
		}

		/* Verify sector is in range */
		offset = CURRENT->sector << 9;			// 512 bytes per sector
		len = CURRENT->current_nr_sectors << 9;	// 512 bytes per sector
		if ((offset + len) > (flash_sizes[minor] << BLOCK_SIZE_BITS)) 
		{
#ifdef ROFLASH_DEBUG_ERR
		    printk(KERN_ERR "roflash_request(): ERROR! Access beyond end of partition\n" );
#endif
		    end_request(0);
		    continue;
		}

#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "roflash_request(): minor=%d, %d sectors at sector %ld\n", minor, CURRENT->current_nr_sectors, CURRENT->sector);
#endif

		/* Prepare to move data to/from the Linux buffer */	
		nrSectors = CURRENT->current_nr_sectors;
		tmpBufPtr = CURRENT->buffer;
//		blockTempBufPtr = roflashTempBuffer; -- this may cause race-condition in multiple cramfs

		/* Execute the request */
		switch( CURRENT->cmd ) 
		{
		    case READ:
		    {
#ifdef ROFLASH_DEBUG_ERR
		    	printk(KERN_ERR "roflash_request: READ\n");
#endif
//				memorySet(roflashTempBuffer, len, 0x00); -- don't use the internal buffer, may exist race-conditions.

				err = (((roflash_area*)(roflash_table_pptr[minor]))->roflash_read)((offset + ((roflash_area*)(roflash_table_pptr[minor]))->offset), len, &retlen, tmpBufPtr);
				if (err == 0)
				{
#ifdef ROFLASH_DEBUG_ERR
					printk(KERN_ERR " < %x %x %x %x ...>\n",CURRENT->buffer[0],CURRENT->buffer[1],CURRENT->buffer[2],CURRENT->buffer[3]);
#endif
					end_request(1);
				}
				else
				{
#ifdef ROFLASH_DEBUG_ERR
					printk(KERN_ERR "roflash_request(): ERROR READ %d sectors at sector %ld\n", CURRENT->current_nr_sectors, CURRENT->sector);
#endif
					end_request(0);
				}
			break;
		    }

		    default:
		    {
#ifdef ROFLASH_DEBUG_ERR
				printk(KERN_ERR "roflash_request(): WARNING! Unknown Request 0x%x\n",CURRENT->cmd);
#endif
				end_request(0);
			break;
		    }
		}
    }
} 

static int roflash_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
// What is the requirements? -- todo list
    return 0;
}


static int roflash_open(struct inode * inode, struct file * filp)
{
#ifdef ROFLASH_DEBUG_ERR
    printk(KERN_ERR "roflash_open() called, minor = %d\n", DEVICE_NR(inode->i_rdev));
#endif
    if (DEVICE_NR(inode->i_rdev) >= roflash_partitions)
	{
#ifdef ROFLASH_DEBUG_ERR
    printk(KERN_ERR "vfm_open() Failed!\n");
#endif
		return -ENXIO;
	}
    return 0;
}


static int roflash_release(struct inode * inode, struct file * filp)
{
#ifdef ROFLASH_DEBUG_ERR
    printk(KERN_ERR "roflash_release() called\n");
#endif
    return 0;
}


/*Susan 2.4.6 */
static struct block_device_operations roflash_fops = {
    open:	roflash_open,
    release:	roflash_release,
    ioctl:	roflash_ioctl,
};
	
int __init roflash_init(void)
{
        int		i, size;
	unsigned short err = 0;
	unsigned long volatile flash_addr = 0;
	unsigned short flash_status;

#ifdef ROFLASH_DEBUG_ERR
	printk(KERN_ERR "roflash_init(): Enter into this API.\n");
#endif
	
/* We should do initialization for flash chips, however, we don't need to do it twice after __init_lubbock has initialized K3 flash device */
/* Just check flash status */
	if ( roflash_status == ROFLASH_STATUS_NOT_READY )
	{
		flash_addr = (DALHART_K3_BASE_ADDR);
#ifdef ROFLASH_DEBUG_ERR
	printk(KERN_ERR "roflash_init(): DALHART_K3_BASE_ADDR is %x\n", flash_addr);
#endif
		*((unsigned short *)(flash_addr)) = FlashCommandStatus;
		flash_status = *((unsigned short *)(flash_addr));
#ifdef ROFLASH_DEBUG_ERR
	printk(KERN_ERR "roflash_init(): flash_status is %x\n", flash_status);
#endif

		*((unsigned short *)(flash_addr)) = FlashCommandRead;  // Return to read-array mode //
		
		if ( (flash_status & FlashStatusReady) == FlashStatusReady )
			roflash_status = ROFLASH_STATUS_READY;
		else
		{
			printk(KERN_ERR "FLASH ROFLASH: Driver failed initialization. Error \n");
			return(1);
		}
	}
	else err = -1;
	
	if (err == 0)
	{
#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "roflash_init: Registering device major %d [%s]\n",MAJOR_NR,DEVICE_NAME);
#endif

		if (register_blkdev(MAJOR_NR, DEVICE_NAME, &roflash_fops)) 
		{
#ifdef ROFLASH_DEBUG_ERR
			printk(KERN_ERR "roflash_init(): Could not get major %d", MAJOR_NR);
#endif
			roflash_status = ROFLASH_STATUS_NOT_READY;
			return -EIO;
		}
#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "roflash_init(): register_blkdev success\n");
#endif

	/* We know this is invoked only once during kernel booting up, so there is not race-conditions */
		for (i = 0; i < roflash_partitions; i++) 
		{
			flash_blocksizes[i] = ROFLASH_BLOCK_SIZE;
			size = ((roflash_area *)(roflash_table_pptr[i]))->size;
#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "roflash_init(): roflash_table_pptr[%d]->size = %d\n",i,size);
#endif
			flash_sizes[i] = size >> BLOCK_SIZE_BITS ;
		}

		blksize_size[MAJOR_NR] = flash_blocksizes;
		blk_size[MAJOR_NR] = flash_sizes;
		
#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "roflash_init(): fill in blksize_size[] and blk_size[] success\n");
#endif
		blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR),&roflash_request);

#ifdef ROFLASH_DEBUG_ERR
		printk(KERN_ERR "roflash_init(): ROFLASH Block device: blksize_size = %d, blk_size = %d\n",flash_blocksizes[0],flash_sizes[0]);
#endif

#ifdef	CONFIG_PROC_FS
		/* register procfs device */
		if (roflash_proc_entry = create_proc_entry("roflash_info", 0, 0))
		{
			roflash_proc_entry->read_proc = roflash_read_procmem;
		}

#endif	/* CONFIG_PROC_FS */
	}
	else
	{
		printk(KERN_ERR "FLASH ROFLASH: Driver failed initialization.\n");

		roflash_status = ROFLASH_STATUS_NOT_READY;
		return(1);
	}
	
	for ( i = 0; i < roflash_partitions; i ++ )
	{
	printk(KERN_ERR "ROFLASH Driver initialized and ready for use. Size: %d Offset: %d\n", ((roflash_area*)(roflash_table_pptr[i]))->size, ((roflash_area*)(roflash_table_pptr[i]))->offset);
	}
	
	return (0);	
}


#ifdef CONFIG_PROC_FS
static int roflash_read_procmem(char *buf, char **start, off_t offset, int len,
				int *eof, void *data)
{
	#define LIMIT (PAGE_SIZE-80)	/* don't print anymore */
					/* after this size */

	char roflash_status_str[16];
	int i;
	
	len=0;
	
	switch (roflash_status)
	{
		case ROFLASH_STATUS_NOT_READY:
			strcpy(roflash_status_str,"Not-ready");
			break;
		case ROFLASH_STATUS_READY:
			strcpy(roflash_status_str,"Ready");
			break;
		default:
			strcpy(roflash_status_str,"Unknown!");
			break;
	}

	len += sprintf(buf+len, "ROFLASH Driver status: %s\n\n",roflash_status_str);
	for ( i = 0; i < roflash_partitions; i ++ )
	{
		len += sprintf(buf+len, "ROFLASH area name is %s\n",((roflash_area*)(roflash_table_pptr[i]))->name);
		len += sprintf(buf+len, "ROFLASH area size = %d bytes\n",((roflash_area*)(roflash_table_pptr[i]))->size);
		len += sprintf(buf+len, "ROFLASH area offset = %ld bytes\n",((roflash_area*)(roflash_table_pptr[i]))->offset);
	}
	return len;
}
#endif	/* CONFIG_PROC_FS */

#ifdef MODULE
static void __exit roflash_cleanup(void)  //Susan//
{
    int i;
    int err_info = 0;

    /* For read-only flash device, we don't need to invoke fsync_dev */
 
    unregister_blkdev(MAJOR_NR, DEVICE_NAME);
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
    blk_size[MAJOR_NR] = NULL;
    blksize_size[MAJOR_NR] = NULL;
    remove_proc_entry("roflash_info",NULL);

    printk(KERN_ERR "remove roflash_info\n");

}

module_init(roflash_init);
module_exit(roflash_cleanup);

#endif

