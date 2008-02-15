/*
 * linux/fs/mnt_record.c
 *
 * Copyright (c) 2005, 2006 Motorola Inc.
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
 * According the records of different partitions' mounting successful times 
 * it makes the decision that the flash partition should be checked or not.
 *
 * Design for nor-flash memory
 *
 *  2005/10/11	
 *		Initial the mount record interface 
 *  2006/02/07
 *		changed for panic log and more flexible
 *  2006/06/13
 *		modified panic log bankwidth dependent 
 *  2006/09/07
 *              remove kernel panic dump macro
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mnt_record.h>
#include <asm/hardware.h>

static struct mnt_record_sb ezx_mnts = {
	name:		"EZXBASE",
	parts:		0,
	partlen:	MNT_MAX_LENGTH,
	total_size:	MNT_RECORD_LEN,
	used_size:	0,
	info_list:	NULL,
	erase:		mnt_record_erase,
	read:		mnt_record_read,
	write:		mnt_record_write,	
}; 

struct mtd_hidden_info *mnt_record_mtd;

static int mnt_record_erase (void)
{
	return mnt_record_mtd->erase(mnt_record_mtd);	
}

static int mnt_record_read (loff_t ofs, size_t len, size_t *retlen, u_char *buf)
{
	return mnt_record_mtd->read(mnt_record_mtd, ofs, len, retlen, buf);
}

static int mnt_record_write (loff_t ofs, size_t len, size_t *retlen, u_char *buf)
{
	return mnt_record_mtd->write(mnt_record_mtd, ofs, len, retlen, buf);	
}

static struct mnt_record_sb *mnts = NULL;
//#define MNT_RECORD_DEBUG

#ifdef MNT_RECORD_DEBUG
#define RC_WARNING(fmt, ...) \
	printk(KERN_WARNING "MNT_RECORD %s: "  fmt, __FUNCTION__, ##__VA_ARGS__)
#define RC_INFO(fmt, ...) \
	printk(KERN_INFO "MNT_RECORD %s: " fmt, __FUNCTION__, ##__VA_ARGS__)
#else
#define RC_WARNING(fmt, ...)
#define RC_INFO(fmt, ...)
#endif

#ifdef MNT_RECORD_DEBUG
void mnt_record_dump_partinfo(int i, struct mnt_record_info *mnt_record_i)
{
	RC_INFO("Dump partinfo!\n");
	printk("mnt_record_info [%d]\n", i);
	printk("  ->mnt_dev %x\n", mnt_record_i->mnt_dev);
	printk("  ->mounted %d\n", mnt_record_i->mounted);
	printk("  ->mnt_count %d\n", mnt_record_i->mnt_count);
	printk("  ->max_mnt_count %d\n", mnt_record_i->max_mnt_count);
}

void mnt_record_dump_sb(void)
{
	struct mnt_record_info *mnt_record_i = mnts->info_list;
	int i = 0;
	
	RC_INFO("Dump mnts!\n");
	printk("mnt_record_sb: parts %d\n", mnts->parts);
	printk("mnt_record_sb: partlen %d\n", mnts->partlen);
	printk("mnt_record_sb: total_size %d\n", mnts->total_size);
	printk("mnt_record_sb: used_size %d\n", mnts->used_size);
	while(mnt_record_i) {
		mnt_record_dump_partinfo(i, mnt_record_i);
		mnt_record_i = mnt_record_i->next;
		++i;
	}	
}

void mnt_record_dump_part(int len, struct mnt_record_part *partbuf)
{
	uint8_t *x = partbuf->mnt_record;
	int i = 0;
	RC_INFO("Dump partbuf!\n");
	printk("partbuf->dev_major 0x%x\n", partbuf->dev_major);
	printk("partbuf->dev_minor 0x%x\n", partbuf->dev_minor);
	printk("partbuf->mnt_record:\n");	
	while(*x < 0xFF && (x - partbuf->mnt_record) < len) {
		if(i % 16 == 0)
			RC_INFO("\n");
		printk(" 0x%2x", *x);
		x++; i++;
	}
	printk("\n");	
}

void mnt_record_dump_head(struct mnt_record_head *buf)
{
	RC_INFO("Dump head buf!\n");
	printk("buf->magic: %c %c %c %c\n", buf->magic[0], buf->magic[1],
		buf->magic[2], buf->magic[3]);
	printk("buf->parts: 0x%x\n", buf->parts);
	printk("buf->part_len: %d * 256 + %d = %d\n", buf->part_len[0], 
		buf->part_len[1], (buf->part_len[0] << 8) + buf->part_len[1]);
	printk("buf->reserved: 0x%x\n", buf->reserved);
}
#endif

/*
 * Strict sanity checking according the initial process
 */
int mnt_record_head_sanity_check(struct mnt_record_head *buf)
{
	if (buf->magic[0] == 'K' && buf->magic[1] == 'I' 
		&& buf->magic[2] == 'N' && buf->magic[3] == 'G'
		&& mnt_bits_to_count(buf->parts) <= MNT_MAX_PARTS
		&& ((buf->part_len[0]<<8)+ buf->part_len[1]) == mnts->partlen)
		return 0;
	else
		return -1; 
}
/*
 * Just consider the partitions we cared 
 */
int mnt_record_part_sanity_check(struct mnt_record_part *partbuf)
{
	if (partbuf->dev_major == MNT_RC_MAJOR 
		&& partbuf->dev_minor < MNT_MAX_MINOR)
		return 0;
	else
		return -1;
}

/*
 * Mount record increase ONE bit on flash whenever mount or umount time 
 */
int mnt_record_inc_part(struct mnt_record_info *mnt_record_i, int num_part)
{
	uint32_t ofs, mount_record;
	uint8_t record;
	int ret, retlen, i=0;
	
	RC_INFO("Mount record %d inc!\n", num_part);
	
	mnt_record_i->mnt_count++;
	if (mnt_record_i->mnt_count > mnt_record_i->max_mnt_count)
		return -EINVAL;
	
	ofs = sizeof(struct mnt_record_head) + num_part * (sizeof(struct mnt_record_part) + mnts->partlen);
	
	ofs += sizeof(struct mnt_record_part);
	mount_record = mnt_record_i->mnt_count;
	
	while (mount_record > 8) {
		ofs++;
		mount_record -= 8;
	}

	RC_INFO("ofs %x mount_record %d!\n", ofs, mount_record);
	record = mnt_count_to_bits((uint8_t)(mount_record));
	
	ret = mnts->write(ofs, sizeof(uint8_t), &retlen, &record);
	if (ret) {
		RC_WARNING("inc part error %d\n", ret);
		return ret;
	}
#ifdef MNT_RECORD_DEBUG
	mnt_record_dump_partinfo(num_part, mnt_record_i);
#endif
	return 0;	
}

/*
 * When it mount a new partition, the mount record should add the part info
 * to the mnt_record_sd->info_list. 
 */
struct mnt_record_info* mnt_record_add_partinfo(kdev_t mnt_dev)
{	
	struct mnt_record_info *mnt_record_i, *prev;
        mnt_record_i = prev = mnts->info_list;

	RC_INFO("Add new part mount info\n");
	while (mnt_record_i) {
		if (mnt_record_i->mnt_dev == mnt_dev) {
			RC_INFO("Add a exist part\n");
			return ERR_PTR(-EACCES);
		}
		prev = mnt_record_i;
		mnt_record_i = mnt_record_i->next;	
	}
	mnt_record_i = (struct mnt_record_info *)kmalloc(sizeof(struct mnt_record_info), GFP_KERNEL);
	if (!mnt_record_i)
		return ERR_PTR(-ENOMEM);
	memset(mnt_record_i, 0, sizeof(struct mnt_record_info));
	
	mnt_record_i->next = NULL;
	mnt_record_i->mnt_dev = mnt_dev;
	mnt_record_i->mounted = RC_NOMOUNT;
	mnt_record_i->mnt_count = 0;
	mnt_record_i->max_mnt_count = mnts->partlen * 8;

	if (prev)	
		prev->next = mnt_record_i;
	else
		mnts->info_list = mnt_record_i;

#ifdef MNT_RECORD_DEBUG
	mnt_record_dump_partinfo(mnts->parts, mnt_record_i);
#endif
	mnts->parts++;
	mnts->used_size += sizeof(struct mnt_record_part) + mnts->partlen;
	return mnt_record_i;

}

/*
 * Write a new part record on the flash when it could not research any info
 * from the info_list on mounting time. 
 */
int mnt_record_add_new_part(kdev_t mnt_dev)
{
	struct mnt_record_part *partbuf;
	int ret, retlen;
	
	RC_INFO("Write new part record!\n");
	partbuf = (struct mnt_record_part *)kmalloc(sizeof(struct mnt_record_part), GFP_KERNEL);
	if (!partbuf)
		return -ENOMEM;

	partbuf->dev_major = major(mnt_dev);
	partbuf->dev_minor = minor(mnt_dev);

	if ((mnts->used_size + sizeof(struct mnt_record_part) + mnts->partlen)
		> mnts->total_size) {
		RC_WARNING("No space to write on part info block\n");
		kfree(partbuf);
		return -ENOSPC;
	}

	ret = mnts->write(mnts->used_size, sizeof(struct mnt_record_part), &retlen, (u_char *)partbuf);
	if (ret) {
		RC_WARNING("write part error %d\n", ret);
		kfree(partbuf);
		return ret;
	}
	if (retlen < sizeof(struct mnt_record_part)) {
		RC_WARNING("write part len=%d\n", retlen);
		kfree(partbuf);
		return -EIO;
	}
	
#ifdef MNT_RECORD_DEBUG
	mnt_record_dump_part(0, partbuf);
#endif
	kfree(partbuf);
	return 0;
}

/*
 * Increase the sb->parts feild for indicate the part num
 */
int mnt_record_inc_partnum(void)
{
	uint8_t parts;
	int ret, retlen;

	RC_INFO("increase part num on flash head!\n");
	parts = mnt_count_to_bits(mnts->parts + 1);
	ret = mnts->write(MNT_RC_MAGIC, sizeof(uint8_t), &retlen, (u_char *)&parts);
	if (ret) {
		RC_WARNING("inc part num error %d\n", ret);
		return ret;
	}
#ifdef MNT_RECORD_DEBUG
	struct mnt_record_head *buf;
	buf = (struct mnt_record_head *)kmalloc(sizeof(struct mnt_record_head), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = mnts->read(0, sizeof(struct mnt_record_head),&retlen, (u_char *)buf);
	if (ret) {
		kfree(buf);
		return ret;
	}
	mnt_record_dump_head(buf);
	kfree(buf);
#endif
	return 0;
}

/*
 * Write mount record head on the begin of mount record partition
 */
int mnt_record_establish(void)
{
	struct mnt_record_head *buf;
	uint8_t *ptr;
	int ret, retlen, i;

	RC_INFO("mount record establish\n");	
	buf = (struct mnt_record_head *)kmalloc(sizeof(struct mnt_record_head), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* check for erased, ugly for panic log */
#ifdef CONFIG_PANIC_LOG
	ret = mnts->read(0, sizeof(struct mnt_record_head), &retlen, (u_char *)buf);
	if (ret) {
		kfree(buf);
		return -EIO;
	}
	
	ptr = (uint8_t *)buf;
#ifdef MNT_RECORD_DEBUG
//	mnt_record_dump_head(buf);
#endif
	for (i = 0; i < sizeof(struct mnt_record_head); i++) {
		if( *ptr != 0xFF) {
			RC_WARNING("Flash not clear (0x%x)!\n", *ptr);
			ptr = (uint8_t *)kmalloc(8, GFP_KERNEL);
			if (!ptr) {
				kfree(buf);
				return -ENOMEM;
			}
			ret = mnts->read(MNT_RECORD_LEN, 8, &retlen, (u_char *)ptr);
			if (ret) {
				kfree(buf);
				kfree(ptr);
				return -EIO;
			}
			//if (strncmp(ptr, "&&KERNEL", 8) == 0) {
			if (*ptr == '&' && *(ptr+1) == '&' && *(ptr+2) == 'K' 
					&& *(ptr+3) == 'E' && *(ptr+4) == 'R' 
					&& *(ptr+5) == 'N' && *(ptr+6) == 'E'
					&& *(ptr+7) == 'L') {
				kfree(buf);
				kfree(ptr);
				return -1; /*
					    * means the mount record data 
					    * structrue was destroied and 
					    * kernel panic occured, we wanna 
					    * reserve the panic log and do 
					    * not record mount times until 
					    * the panic occured next time 
					    * when the whole block would 
					    * be erased at the time. 
					    */
			}
			else {
				kfree(ptr);
				ret = mnts->erase();
				if (ret) {
					RC_WARNING("erase error %d\n", ret);
					kfree(buf);
					return ret;
				}	
				goto establish;
			}
		}
		ptr++;
	}
establish:	
#else
	ret = mnts->erase();
	if (ret) {
		RC_WARNING("erase error %d\n", ret);
		kfree(buf);
		return ret;
	}
#endif
	
	buf->magic[0] = 'K';
	buf->magic[1] = 'I';
	buf->magic[2] = 'N';
	buf->magic[3] = 'G';
	buf->parts = 0xFF; 
	buf->part_len[0] = mnts->partlen >> 8 & 0xFF;
	buf->part_len[1] = mnts->partlen & 0xFF;
	buf->reserved = 0xFF; 

	ret = mnts->write(0, sizeof(struct mnt_record_head), &retlen, (u_char *)buf);
	if (ret) {
		RC_WARNING("write head error %d\n", ret);
		kfree(buf);
		return ret;
	}
	if (retlen < sizeof(struct mnt_record_head)) {
		RC_WARNING("write head len=%d\n",retlen);
		kfree(buf);
		return -EIO;
	}
#ifdef MNT_RECORD_DEBUG
	memset(buf, 0, sizeof(struct mnt_record_head));
	ret = mnts->read(0, sizeof(struct mnt_record_head),&retlen, (u_char *)buf);
	if (ret) {
		kfree(buf);
		return ret;
	}
	mnt_record_dump_head(buf);
#endif
	kfree(buf);
	return 0;
}

/*
 * Convert the mount record to count  
 */
int mnt_record_get_count(struct mnt_record_part *partbuf)
{
	uint32_t count, t;
	uint8_t *p;

	RC_INFO("get record count\n");
	count = 0;
	for (p = partbuf->mnt_record; *p == 0; ++p) {
		if (p - partbuf->mnt_record == mnts->partlen)	
			return count;	
		count += 8;
	}
	/* ideal record list is :
		[ 0x0 0x0 0x0 ... 0xfc 0xff 0xff ... ]
	   for avoiding the condition like that:
		[ 0x0 0x0 0x40 0xfc 0xff 0xff ... ]
	   we just simply check the forward record whether is zero. 
	 */
	t = mnt_bits_to_count(*p);
	if (t == 8) {
		RC_WARNING("count record error\n");
		return -1;
	}
	count += t;
	RC_INFO("return count = %d\n", count);
	return count;
}

/*
 * Fill the part info and structure the info_list
 */
int mnt_record_fill_info(struct mnt_record_part *partbuf)
{
	struct mnt_record_info *mnt_record_i, *prev;
        mnt_record_i = prev = mnts->info_list;

	RC_INFO("Fill mount info\n");
	if (!mnts)
		return -EINVAL;

	while (mnt_record_i) {
		prev = mnt_record_i;
		mnt_record_i = mnt_record_i->next;		
	}
	mnt_record_i = (struct mnt_record_info *)kmalloc(sizeof(struct mnt_record_info), GFP_KERNEL);
	if (!mnt_record_i)
		return -ENOMEM;
	memset(mnt_record_i, 0, sizeof(struct mnt_record_info));
	
	mnt_record_i->next = NULL;
	mnt_record_i->mnt_dev = MKDEV(partbuf->dev_major, partbuf->dev_minor);
	mnt_record_i->mounted = RC_NOMOUNT;
	mnt_record_i->mnt_count = mnt_record_get_count(partbuf);
	mnt_record_i->max_mnt_count = mnts->partlen * 8;

	if (prev)	
		prev->next = mnt_record_i;
	else
		mnts->info_list = mnt_record_i;
	
	return 0;
}

/*
 * Get the every mount record part one by one and establish the info_list
 */
int mnt_record_scan(void)
{
	int num_part, ret;
	struct mnt_record_part *partbuf;
	size_t retlen;
	uint32_t ofs = sizeof(struct mnt_record_head);
	
	RC_INFO("Mount record scan!\n");
	if (!mnts)
		return -EINVAL;
	
	partbuf = (struct mnt_record_part *)kmalloc(sizeof(struct mnt_record_part) + mnts->partlen, GFP_KERNEL);
	if (!partbuf)
		return -ENOMEM;
	
	for(num_part = 0; num_part < mnts->parts; ++num_part) {
		ofs += num_part*(sizeof(struct mnt_record_part)+ mnts->partlen);
		ret = mnts->read(ofs, sizeof(struct mnt_record_part)
			+ mnts->partlen, &retlen, (u_char *)partbuf);
		if (ret) {
			RC_WARNING("read error %d\n", ret);
			kfree(partbuf);
			return ret;
		}
		if (retlen < sizeof(struct mnt_record_part) + mnts->partlen) {
			RC_WARNING("read len=%d\n", retlen);
			kfree(partbuf);
			return -EIO;
		}
		ret = mnt_record_fill_info(partbuf);	
		if (ret) {
			RC_WARNING("fill info=%d\n", ret);
			kfree(partbuf);
			return ret;
		}
#ifdef MNT_RECORD_DEBUG
		mnt_record_dump_part(mnts->partlen, partbuf);
#endif
		ret = mnt_record_part_sanity_check(partbuf);
		if (ret) {
			RC_WARNING("part sanity check failed!\n");
			kfree(partbuf);
			return ret;
		}
	}
	RC_INFO("Read mount record OK\n");
	kfree(partbuf);
	return 0;
}

int mnt_record_build(void)
{
	struct mnt_record_head *buf;
	size_t retlen;
	int ret;

	RC_INFO("Build mount record!\n");	
	
	buf = (struct mnt_record_head *)kmalloc(sizeof(struct mnt_record_head), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	
	ret = mnts->read(0, sizeof(struct mnt_record_head), &retlen, (u_char *)buf);
	if (ret) {
		RC_WARNING("read head error %d\n", ret);
		kfree(buf);
		return ret;
	}
	if (retlen < sizeof(struct mnt_record_head)) {
		RC_WARNING("read head len=%d\n", retlen);
		kfree(buf);
		return -EIO;
	}
#ifdef MNT_RECORD_DEBUG
	mnt_record_dump_head(buf);
#endif
	spin_lock_init(&mnts->mnt_record_lock);
	if (!mnt_record_head_sanity_check(buf)) {
		mnts->parts = mnt_bits_to_count(buf->parts);
		mnts->partlen = (buf->part_len[0] << 8) + buf->part_len[1];
		mnts->total_size = MNT_RECORD_LEN;
		mnts->used_size = sizeof(struct mnt_record_head) + ( mnts->parts * (sizeof(struct mnt_record_part) + mnts->partlen));
		mnts->info_list = NULL;
		ret = mnt_record_scan();
		if (ret) 
			goto init_record;
		kfree(buf);
		return 0;
	}

init_record:	
	RC_WARNING("magic mismatch or sanity check failed!\n");
	kfree(buf);
	
	mnts->parts = 0;	
	mnts->partlen = MNT_MAX_LENGTH;	
	mnts->total_size = MNT_RECORD_LEN;
	mnts->used_size = sizeof(struct mnt_record_head); 
	mnts->info_list = NULL;
	
	ret =  mnt_record_establish();
	if (ret) {
		RC_WARNING("Establish record error!\n");
		return ret;
	}
	return 0;	
}

/*
 * When the part was erased and partinfo still in mem, it need to restruct
 * the part on flash according the memory info.
 */
int mnt_record_restruct(void)
{	
	struct mnt_record_info *mnt_record_i, *prev;
	int ret, num_part;
	
	RC_INFO("restruct part!\n");	
	
	mnts->parts = 0;	
	mnts->partlen = MNT_MAX_LENGTH;	
	mnts->total_size = MNT_RECORD_LEN;
	mnts->used_size = sizeof(struct mnt_record_head); 
	
	ret = mnt_record_establish();
	if (ret) {
		RC_WARNING("Establish record error!\n");
		return ret;
	}
	
	num_part = 0;
	mnt_record_i = prev = mnts->info_list; 
	while(mnt_record_i) {
		if (mnt_record_i->mounted == RC_NOMOUNT) {
			if (prev == mnt_record_i) {
				mnts->info_list = mnt_record_i->next;
				kfree(mnt_record_i);
				mnt_record_i = prev = mnts->info_list;
			}
			else {
				prev->next = mnt_record_i->next; 
				kfree(mnt_record_i);
				mnt_record_i = prev->next;
			}
			continue;
		}
		
		mnt_record_i->mnt_count = 0;
		mnt_record_add_new_part(mnt_record_i->mnt_dev);
		if (mnt_record_i->mounted == RC_MOUNTED) {
			ret = mnt_record_inc_part(mnt_record_i, num_part);
			if (ret) 
				return ret;
		}
		ret = mnt_record_inc_partnum();
		if (ret) 
			return ret;
		prev = mnt_record_i;
		mnt_record_i = mnt_record_i->next;
		mnts->parts = ++num_part;	
		mnts->used_size +=sizeof(struct mnt_record_part)+ mnts->partlen;
	}
	
	return 0;
}

/*
 * Support two sockets. when it needs to be recoreded mount times we can 
 * use this function 
 */
int mnt_record_enter(kdev_t mnt_dev)
{
	int ret, num_part;
	struct mnt_record_info *mnt_record_i;

	RC_INFO("Enter mount %x counting!\n", mnt_dev);	
	
	ret = 0; /* default for not checking */	
	if (!mnts)
		return -EFAULT;	
		
		
	spin_lock(&mnts->mnt_record_lock);
	if(!mnts->info_list) {	
		ret = mnt_record_build();
		if (ret) 
			return ret;
	}
#ifdef MNT_RECORD_DEBUG
	mnt_record_dump_sb();
#endif 
	mnt_record_i = mnts->info_list;
	num_part = 0;
	while(mnt_record_i) {
		if (mnt_record_i->mnt_dev == mnt_dev)
			break;
		mnt_record_i = mnt_record_i->next;
		num_part++;
	}

	if (!mnt_record_i) {
		if (mnts->parts >= MNT_MAX_PARTS) {
			RC_WARNING("too many parts!\n");
			spin_unlock(&mnts->mnt_record_lock);
			return 1;
		}
		ret = mnt_record_add_new_part(mnt_dev);
		if (ret) {
			spin_unlock(&mnts->mnt_record_lock);
			return ret;
		}
		ret = mnt_record_inc_partnum();
		if (ret) {
			spin_unlock(&mnts->mnt_record_lock);
			return ret;
		}
		mnt_record_i = mnt_record_add_partinfo(mnt_dev);
		if (!mnt_record_i) {
			spin_unlock(&mnts->mnt_record_lock);
			return -EINVAL;
		}
		ret = 1; /* we want check as first mount */
	}

	if (mnt_record_i->mounted != RC_NOMOUNT) {
		RC_WARNING("Mount too many times!\n");
		spin_unlock(&mnts->mnt_record_lock);
		return -EINVAL;
	}
			
	mnt_record_i->mounted = RC_MOUNTING;
	if (mnt_record_i->mnt_count % 2) {
		if(mnt_record_inc_part(mnt_record_i, num_part)) {
			RC_WARNING("inc part failed!\n");
			spin_unlock(&mnts->mnt_record_lock);
			return -EINVAL;
		}
		ret = 1;		
	}
	
	if (mnt_record_i->mnt_count == mnt_record_i->max_mnt_count) {
		ret = mnt_record_restruct();
		if (ret) {
			spin_unlock(&mnts->mnt_record_lock);
			return ret;
		}
		ret = 1; //indicate this partition had not been proper umounted
	}
	
	if(mnt_record_inc_part(mnt_record_i, num_part)) {
		RC_WARNING("inc part failed!\n");
		spin_unlock(&mnts->mnt_record_lock);
		return -EINVAL;
	}
	mnt_record_i->mounted = RC_MOUNTED;
	spin_unlock(&mnts->mnt_record_lock);
	return ret;
}

int mnt_record_leave(kdev_t mnt_dev)
{
	struct mnt_record_info *mnt_record_i;
	int ret, num_part;
	
	RC_INFO("Leave mount %x counting!\n", mnt_dev);	
	
	if (!mnts)
		return -EINVAL;

	spin_lock(&mnts->mnt_record_lock);
#ifdef MNT_RECORD_DEBUG
	mnt_record_dump_sb();
#endif
	mnt_record_i = mnts->info_list;
	num_part = 0;	
	while(mnt_record_i) {
		if (mnt_record_i->mnt_dev == mnt_dev) {
			ret = mnt_record_inc_part(mnt_record_i, num_part);
			if (ret) {
				RC_WARNING("umount recording error!\n");
				spin_unlock(&mnts->mnt_record_lock);
				return ret;
			}
			mnt_record_i->mounted = RC_NOMOUNT;
			spin_unlock(&mnts->mnt_record_lock);
			return 0;
		}
		mnt_record_i = mnt_record_i->next;
		num_part++;
	}
	spin_unlock(&mnts->mnt_record_lock);
	return -EINVAL;
}

int mnt_record_register(struct mnt_record_sb *mnts_t)
{
	mnts = mnts_t;
	return 0;
}

int mnt_record_unregister(void)
{
	mnts = NULL;
	return 0;
}

static int __init init_mnt_record(void)
{
	int ret;

	RC_INFO("Moto nor-flash mount record\n");
	mnt_record_mtd = (struct mtd_hidden_info *)kmalloc(sizeof(struct mtd_hidden_info), GFP_KERNEL);
	if(!mnt_record_mtd)
		return -ENOMEM;

	ret = setup_mtd_hidden_info(mnt_record_mtd, MNT_RECORD_OFS, MNT_RECORD_LEN, F_NORMAL);
	if (ret)
		return ret;
	
	ret = mnt_record_register(&ezx_mnts);
	return ret;	
}

static void __exit exit_mnt_record(void)
{
	kfree(mnt_record_mtd);
	mnt_record_unregister();
	return;
}

module_init(init_mnt_record);
module_exit(exit_mnt_record);

MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("EzX file system mount record interface");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;

