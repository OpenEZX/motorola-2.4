/*
 * linux/drivers/char/sram.c
 *
 * Bulverde Internal Memory character device driver
 *
 * Created:	Sep 05, 2003
 * Copyright:	MontaVista Software Inc.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/config.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/rwsem.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/pgalloc.h>

#include <linux/devfs_fs_kernel.h>

static unsigned long sram_base = 0;
static unsigned long sram_size = 0;

extern int sram_access_obtain(unsigned long *pmem, unsigned long *psize);
extern int sram_access_release(unsigned long *pmem, unsigned long *psize);

/*
 * This funcion reads the SRAM memory.
 * The f_pos points to the SRAM memory location. 
 */
static ssize_t sram_read(struct file * file, char * buf,
			 size_t count, loff_t *ppos) {
	unsigned long p = *ppos;
	ssize_t read;

	if (p >= sram_size)
		return 0;

	if (count > sram_size - p)
		count = sram_size - p;

	read = 0;

	if (copy_to_user(buf, (char *)(sram_base + p), count))
		return -EFAULT;

	read += count;
	*ppos += read;

	return read;
}

static ssize_t sram_write(struct file * file, const char * buf, 
			  size_t count, loff_t *ppos) {
	unsigned long p = *ppos;
	ssize_t written;

	if (p >= sram_size)
		return 0;

	if (count > sram_size - p)
		count = sram_size - p;

	written = 0;

	if (copy_from_user((char *)(sram_base + p), buf, count))
		return -EFAULT;

	written += count;
	*ppos += written;

	return written;
}

static int sram_mmap(struct file * file, struct vm_area_struct * vma) {

	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	if (((file->f_flags & O_WRONLY) != 0) ||
	    ((file->f_flags & O_RDWR) != 0)) 	{
		vma->vm_page_prot = (pgprot_t)PAGE_SHARED;
	} else {
		vma->vm_page_prot = (pgprot_t)PAGE_READONLY;
	}
	
	/* Do not cache SRAM memory if O_SYNC flag is set */
	if ((file->f_flags & O_SYNC) != 0) {
		pgprot_val(vma->vm_page_prot) &= ~L_PTE_CACHEABLE;
	}

	/* Don't try to swap out physical pages.. */
	vma->vm_flags |= VM_RESERVED | VM_LOCKED;

	if (remap_page_range(vma->vm_start, SRAM_MEM_PHYS + offset,
			     vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}


static loff_t sram_lseek(struct file * file, loff_t offset, int orig) {

	switch (orig) {
	case 0:
		file->f_pos = offset;
		break;
	case 1:
		file->f_pos += offset;
		break;
	case 2:
		file->f_pos = SRAM_SIZE + offset;
		break;
	default:
		return -EINVAL;
	}
	
	if (file->f_pos < 0) {
		file->f_pos = 0;
	} else if (file->f_pos > SRAM_SIZE) {
		file->f_pos = SRAM_SIZE;
	}
	
	return file->f_pos;
}

static int sram_open(struct inode * inode, struct file * filp) {
	return sram_access_obtain(&sram_base, &sram_size);
}

static int sram_release(struct inode * inode, struct file * filp) {
	return sram_access_release(&sram_base, &sram_size);
}


static struct file_operations sram_fops = {
	llseek:		sram_lseek,
	read:		sram_read,
	write:		sram_write,
	mmap:		sram_mmap,
	open:		sram_open,
	release:	sram_release,
};

static struct miscdevice sram_misc = {
	minor : MISC_DYNAMIC_MINOR,
	name  : "misc/sram",
	fops  : &sram_fops,
};

static int __init sram_chr_init(void) {
	if (misc_register(&sram_misc) != 0)
	{
		printk(KERN_ERR "Cannot register device /dev/%s\n",
		       sram_misc.name);
		return -EFAULT;
	}
	
	return 0;
}

static void __exit sram_chr_exit(void) {
	misc_deregister(&sram_misc);
}

module_init(sram_chr_init)
module_exit(sram_chr_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MontaVista Software Inc.");
