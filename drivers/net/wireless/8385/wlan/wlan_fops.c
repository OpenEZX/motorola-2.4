/** @file wlan_fops.c
  * @brief This file contains the file read functions
  * 
  *  Copyright (c) Marvell Semiconductor, Inc., 2003-2005
  */
/********************************************************
Change log:
	01/06/06: Add Doxygen format comments

********************************************************/

#include	"include.h"

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/fcntl.h>

/********************************************************
		Local Variables
********************************************************/



/********************************************************
		Global Variables
********************************************************/



/********************************************************
		Local Functions
********************************************************/

/** 
 *  @brief This function opens/create a file in kernel mode.
 *  
 *  @param filename	Name of the file to be opened
 *  @param flags		File flags 
 *  @param mode		File permissions
 *  @return 		file pointer if successful or NULL if failed.
 */
static struct file * wlan_fopen(const char * filename, unsigned int flags, int mode)
{
	int				orgfsuid, orgfsgid;
	struct file *	file_ret;

	/* Save uid and gid used for filesystem access.  */

	orgfsuid = current->fsuid;
	orgfsgid = current->fsgid;

	/* Set user and group to 0 (root) */
	current->fsuid = 0;
	current->fsgid = 0;
  
	/* Open the file in kernel mode */
	file_ret = filp_open(filename, flags, mode);
	
	/* Restore the uid and gid */
	current->fsuid = orgfsuid;
	current->fsgid = orgfsgid;

	/* Check if the file was opened successfully
	  and return the file pointer of it was.  */
	return ((IS_ERR(file_ret)) ? NULL : file_ret);
}



/** 
 *  @brief This function closes a file in kernel mode.
 *  
 *  @param file_ptr     File pointer 
 *  @return 		WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int wlan_fclose(struct file * file_ptr)
{
	int	orgfsuid, orgfsgid;
	int	file_ret;

	if((NULL == file_ptr) || (IS_ERR(file_ptr)))
		return -ENOENT;
	
	/* Save uid and gid used for filesystem access.  */
	orgfsuid = current->fsuid;
	orgfsgid = current->fsgid;

	/* Set user and group to 0 (root) */
	current->fsuid = 0;
	current->fsgid = 0;
  
	/* Close the file in kernel mode (user_id = 0) */
	file_ret = filp_close(file_ptr, 0);
	
	/* Restore the uid and gid */
	current->fsuid = orgfsuid;
	current->fsgid = orgfsgid;

    return (file_ret);
}



/** 
 *  @brief This function reads data from files in kernel mode.
 *  
 *  @param file_ptr     File pointer
 *  @param buf		Buffers to read data into
 *  @param len		Length of buffer
 *  @return 		number of characters read	
 */
static int wlan_fread(struct file * file_ptr, char * buf, int len)
{
	int				orgfsuid, orgfsgid;
	int				file_ret;
	mm_segment_t	orgfs;

	/* Check if the file pointer is valid */
	if((NULL == file_ptr) || (IS_ERR(file_ptr)))
		return -ENOENT;

	/* Check for a valid file read function */
	if(file_ptr->f_op->read == NULL)
		return  -ENOSYS;

	/* Check for access permissions */
	if(((file_ptr->f_flags & O_ACCMODE) & (O_RDONLY | O_RDWR)) == 0)
		return -EACCES;

	/* Check if there is a valid length */
	if(0 >= len)
		return -EINVAL;

	/* Save uid and gid used for filesystem access.  */
	orgfsuid = current->fsuid;
	orgfsgid = current->fsgid;

	/* Set user and group to 0 (root) */
	current->fsuid = 0;
	current->fsgid = 0;

	/* Save FS register and set FS register to kernel
	  space, needed for read and write to accept
	  buffer in kernel space.  */
	orgfs = get_fs();

	/* Set the FS register to KERNEL mode.  */
	set_fs(KERNEL_DS);

	/* Read the actual data from the file */
	file_ret = file_ptr->f_op->read(file_ptr, buf, len, &file_ptr->f_pos);

	/* Restore the FS register */
	set_fs(orgfs);

	/* Restore the uid and gid */
	current->fsuid = orgfsuid;
	current->fsgid = orgfsgid;

    return (file_ret);
}



/** 
 *  @brief This function reads a character from files.
 *  
 *  @param file_ptr     File pointer
 *  @return 		character read from file
 */
static int wlan_fgetc(struct file * file_ptr)
{
	int		read_ret;
	char	buf;

	/* Read one byte from the file */
	read_ret = wlan_fread(file_ptr, &buf, 1);

	/* Check return value */
	if (read_ret > 0)
		return buf;	
	else if (0 == read_ret)
		return -1;
	else
		return read_ret;
}


/** 
 *  @brief This function reads a string from file
 *  
 *  @param file_ptr     File pointer
 *  @param str		Buffers to read data into
 *  @param len          Length of buffer
 *  @return 		number of bytes read
 */
static int wlan_fgets(struct file * file_ptr, char * str, int len)
{
	int				file_ret, readlen;
	char *			cp;

	/* Check if there is a valid length */
	if(0 >= len)
		return -EINVAL;

	/* Read the actual data from the file and parse it */
	for(cp = str, file_ret = -1, readlen = 0; readlen < len - 1; ++cp, ++readlen)
	{
		/* Read one character from the file */
		file_ret = wlan_fgetc(file_ptr);

		/*  Check for error in reading */
		if(0 >= file_ret)
			break;			
		else
			*cp = file_ret;

		/* Check for a new line character */
		
		if(*cp == '\n')
		{
			++cp;
			++readlen;
			break;
		}
	}

	*cp = (char) NULL;

	return ((0 >= file_ret) ? file_ret : readlen);
}



/********************************************************
		Global Functions
********************************************************/
#define MAX_LINE_LEN 	256	
#define DATA_LEN	6

/** 
 *  @brief This function reads FW/Helper.
 *  
 *  @param name		File name
 *  @param addr		Pointer to buffer storing FW/Helper
 *  @param len     	Pointer to length of FW/Helper
 *  @return 		WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int fw_read( char *name, u8 **addr, u32 *len )
{
	struct 	file *fp;
	u8	buf[MAX_LINE_LEN] = { 0 };
	int 	ret, data, i=0;
	u8	*ptr;

	fp = wlan_fopen(name, O_RDWR, 0 );

	if ( fp == NULL ) {
		printk("<1>Could not open file:%s\n", name );
		return WLAN_STATUS_FAILURE;
	}

	/*calculate file length*/
	*len = fp->f_dentry->d_inode->i_size - fp->f_pos;
	*len /= DATA_LEN;

	PRINTK2("len=%x\n", *len );

	ptr= (u8 *)kmalloc( *len, GFP_KERNEL );
	if ( ptr == NULL ) {
		printk("<1>vmalloc failure\n");
		return WLAN_STATUS_FAILURE;
	}

	while ( (ret = wlan_fgets(fp, buf, MAX_LINE_LEN)) > 0 ) 
	{
		if ( ret != DATA_LEN ) {
			continue;
		}

		sscanf(buf+2, "%x", &data );
		*(ptr+i) = (u8)data;		
		memset( buf, 0, MAX_LINE_LEN );
		i++;
	}
	wlan_fclose( fp );
	*addr = ptr;
	*len = i;
	PRINTK2("*addr=%p ptr=%p len=%x\n", *addr, ptr, *len );
	return WLAN_STATUS_SUCCESS;
}


