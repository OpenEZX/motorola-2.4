/*
 * Kernel panic log interface for Linux.
 *
 * Copyright (C) 2003 Motorola Inc.
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


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/rtc.h>
#include <linux/notifier.h>
#include <asm/bitops.h>
#include <asm/hardware.h>
#include <asm/irq.h>

#include "log.h"

#define	LOGDRV_MINOR 188

static struct log_area *log = NULL;
struct log_cmdline log_cmdline;

static int __init log_setup(char *str)
{
	struct log_cmdline *c;

	c = &log_cmdline;
	memcpy(c->name, str, sizeof(c->name));

	return 1;
}

__setup("paniclog=", log_setup);

void panic_trig(const char *str, struct pt_regs *regs, int err)
{
	if (log)
		die(str, regs, err);
}
EXPORT_SYMBOL(panic_trig);

int log_register(struct log_area *log_f)
{
	if (strcmp(log_cmdline.name, log_f->name) == 0)
		log = log_f;
	return 0;
}

int log_unregister(struct log_area *log_f)
{
	log = NULL;
	return 0;
}

static loff_t logdrv_lseek(struct file *file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:
		/* SEEK_SET */
		file->f_pos = offset;
		break;
	case 1:
		/* SEEK_CUR */
		file->f_pos += offset;
		break;
	case 2:
		/* SEEK_END */
		file->f_pos = log->size + offset;
		break;
	default:
		return -EINVAL;
	}

	if (file->f_pos < 0)
		file->f_pos = 0;
	else if (file->f_pos >= log->size)
		file->f_pos = log->size - 1;

	return file->f_pos;
}

static int logdrv_open(struct inode *inode, struct file *file)
{
	if (!log) {
		printk("Not register mechine specific log structure!\n");
		return -ENODEV;
	}

	return 0;
}

static int logdrv_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t logdrv_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;

	if (count < 0)
		return -EINVAL;

	if (count > log->size - p)
		count = log->size - p;
	if (log->read)
		return log->read(buf, count, ppos);

	if (copy_to_user((void *)buf, (void *)(log->start + *ppos), count))
		return -EFAULT;

	*ppos += count;
	return count;
}

static struct file_operations logdrv_fops = {
	owner:		THIS_MODULE,
	llseek:		logdrv_lseek,
	read:		logdrv_read,
	open:		logdrv_open,
	release:	logdrv_close,
};

static struct miscdevice logdrv_miscdev = {
	LOGDRV_MINOR,
	"log drv",
	&logdrv_fops
};

extern void logbuf_info(char **st, unsigned long *cnt);
int panic_log_notify(struct notifier_block *self, unsigned long code, void *unused)
{
	unsigned long count;
	char *start;

	printk(KERN_EMERG "****** panic time: 0x%x ******\n", RCNR);

	/* Below 2 lines will overwrite useful information if log_buf is not large enough */
//	printk(KERN_EMERG "****** show task state when panic *******\n");
//	show_state();

	/* get syslog buf info: start position, number of chars */
	logbuf_info(&start, &count);

	/* write information to log area */
	if (log && log->write)
		log->write(start, count);

	printk(KERN_EMERG "OK, Now we exit panic log function!\n");

	return NOTIFY_DONE;
}

static struct notifier_block panic_log_nb = {
	panic_log_notify,
	NULL,
	0
};

extern struct notifier_block *panic_notifier_list;
static int __init log_init(void)
{
	misc_register(&logdrv_miscdev);
	return notifier_chain_register(&panic_notifier_list, &panic_log_nb);
}

static void __exit log_exit(void)
{
	notifier_chain_unregister(&panic_notifier_list, &panic_log_nb);
	misc_deregister(&logdrv_miscdev);
}

module_init(log_init);
module_exit(log_exit);

EXPORT_SYMBOL(log_register);
EXPORT_SYMBOL(log_unregister);

MODULE_AUTHOR("zxf <w19962@motorola.com>");
MODULE_DESCRIPTION("Kernel panic log interface");
MODULE_LICENSE("GPL");

