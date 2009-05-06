
#ifdef MODULE
//#define  EXPORT_SYMTAB
#include <linux/module.h>
#endif

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/poll.h>

/*
 * declaration of functions *
 */
static ssize_t midicomm_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);




static int midicomm_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg);

static int midicomm_release(struct inode *inode, struct file *filp);
static int midicomm_open(struct inode *inode, struct file *filp);
static unsigned int midicomm_poll(struct file *filp, poll_table *wait);

static int midicomm_debug(int level, const char *message);

int midicomm_intflag_write(unsigned char data);

//EXPORT_SYMBOL_NOVERS(midicomm_intflag_write);

/*
 * MACRO *
 */

#ifndef FALSE
#define     FALSE       0
#endif

#ifndef TRUE
#define     TRUE        1
#endif

    
#define     MIDICOMM_EVENT_NUM  12
#define     MIDICOMM_NAME       "midicomm"
//#define   MIDICOMM_DEBUG      1

#ifdef MIDICOMM_DEBUG
#define     MIDICOMM_DEBUG_DEFUALT_LEVEL    -1
#endif


/********************************************************************************
 * global variables                                 *
 ********************************************************************************/
DECLARE_WAIT_QUEUE_HEAD(midicomm_wait_queue);

#ifdef CONFIG_DEVFS_FS
    devfs_handle_t midicomm_de;
#else
    int midicomm_major = 0xee;
#endif

static int usage_count = 0;

static DECLARE_MUTEX(midicomm_mutex);

struct __midicomm_event
{
    char    event[MIDICOMM_EVENT_NUM];  /* event buffer */
    short   write_pos;                  /* the position for next write */
    short   read_pos;                   /* the position for next read  */
    int     num;                        /* the number of events in buffer */
    spinlock_t lock;                    /* spin lock for writing/reading event buffer */
}midicomm_event;




struct file_operations midicomm_fops = {
    read:       midicomm_read,
    poll:       midicomm_poll,
    ioctl:      midicomm_ioctl,
    open:       midicomm_open,
    release:    midicomm_release,

};

/*******************************************************************************
  * Function name:  midicomm_eventbuf_empty
  * Arguments:      None
  * Return:         bool            -   TRUE indicating buffer is empty
  *                                     FALSE for non-empty
  * Comments:     
  *******************************************************************************/
static inline int midicomm_eventbuf_empty(void)
{
    unsigned long flags;

    
    spin_lock_irqsave(&(midicomm_event.lock),flags);

    if(0 == midicomm_event.num)
    {
        spin_unlock_irqrestore(&(midicomm_event.lock),flags);
        midicomm_debug(-1,"midicomm: event buf is empty\n");
        return TRUE;
    }
    spin_unlock_irqrestore(&(midicomm_event.lock),flags);
    midicomm_debug(-1,"midicomm: event buf is non-empty\n");
    return FALSE;
}
/*******************************************************************************
  * Function name:  midicomm_eventbuf_full
  * Arguments:      None
  * Return:         bool            -   TRUE indicating buffer is full
  *                                     FALSE for non-full
  * Comments:     
  *******************************************************************************/
static inline int midicomm_eventbuf_full(void)
{
    midicomm_debug(-1,"midicomm: entering midicomm_eventbuf_full\n");
    if(MIDICOMM_EVENT_NUM == midicomm_event.num)
    {
        printk("<1> midicomm_eventbuf_full: buf is full,heihei\n");
        return TRUE;
    }
    return FALSE;
}
/*******************************************************************************
  * Function name:  midicomm_eventbuf_write
  * Arguments:      char buf     -  buffer to which data is copied
  *         ssize_t size -  size of the buf
  * Return:     int -   -1 indicating error, or for sucess
  * Comments:   read the event data  
  *******************************************************************************/
static  int midicomm_eventbuf_wirte(char data)
{
    short pos;
    int ret = FALSE;
    unsigned long flags;

    midicomm_debug(-1,"midicomm: write data into event buf\n");
    spin_lock_irqsave(&(midicomm_event.lock),flags);
    if(!midicomm_eventbuf_full())
    {
        pos = midicomm_event.write_pos;
        midicomm_event.event[pos] = data;
        midicomm_event.num ++;
        midicomm_event.write_pos = (pos == MIDICOMM_EVENT_NUM-1)?0:(pos+1);

//      printk("<1>midicomm_write: num = %d,write_pos = %d,data = 0x%x\n",midicomm_event.num,midicomm_event.write_pos,data);
        ret = TRUE;
    }
    spin_unlock_irqrestore(&(midicomm_event.lock),flags);
    return ret;
}

/*******************************************************************************
  * Function name:  midicomm_eventbuf_read
  * Arguments:      char buf    -   buffer to which data is copied  
  *                 ssize_t size-   size of the buf
  * Return:         int         -   -1 indicating error, or the number of copied 
  *                                 data
  * Comments:   read the event data  
  *******************************************************************************/
static int midicomm_eventbuf_read(char *buf, ssize_t size)
{
    short pos;
    int ret = -1,num;
    unsigned long flags;
    int i = 0;
    
    midicomm_debug(-1,"midicomm: read data from event buf\n");
    spin_lock_irqsave(&(midicomm_event.lock),flags);
#if 0
    if(!midicomm_eventbuf_empty())
    {
        pos = midicomm_event.read_pos;
        num = (midicomm_event.num > size) ? size : midicomm_event.num;
        ret = 0;
        while( num)
        {
//          printk("<1>read_pos = %d, data = %d\n",pos,midicomm_event.event[pos]);
            buf[i++] = midicomm_event.event[pos];
            pos = (pos == MIDICOMM_EVENT_NUM-1)?0:(pos+1);
            ret ++;
            midicomm_event.num --;
            num--;
        }
        midicomm_event.read_pos = pos;
    

    }
#endif
    pos = midicomm_event.read_pos;
    num = (midicomm_event.num > size) ? size : midicomm_event.num;
    ret = 0;
    while( num)
    {       
//        printk("<1>read_pos = %d, data = %d\n",pos,midicomm_event.event[pos]);
          buf[i++] = midicomm_event.event[pos];
          pos = (pos == MIDICOMM_EVENT_NUM-1)?0:(pos+1);
          ret ++; 
          midicomm_event.num --;
          num--;  
    }                                   
    midicomm_event.read_pos = pos;
    spin_unlock_irqrestore(&(midicomm_event.lock),flags);
    return ret;
}
    
 
 /*******************************************************************************
  * Function name:  midicomm_debug
  * Arguments:      int level
  *         char *message
  * Return:     int
  * Comments:   it's for debug only,when level is -1, it uses the default level of 
  *     printk
  *******************************************************************************/
#ifdef MIDICOMM_DEBUG
static int midicomm_debug(int level, const char *message)
{
    if(level == MIDICOMM_DEBUG_DEFUALT_LEVEL)
    {
        printk(message);
        return 1;
    }

    if((level > 7)||(level < -1))
    {
        printk("Midi debug level is error: %s\n",message);
        return 1;
    }
    printk("<1>%s\n",message);
    return 1;
}
#else
static int midicomm_debug(int level, const char *message)
{
    return 1;
}
#endif
/*******************************************************************************
  * Function name:  midicomm_clear_eventbuf
  * Arguments:      void 
  * Return:         int         - 0  for successful, 
  *                               negative number signaling a error
  * Comments:       clear the event buf
  *******************************************************************************/
int midicomm_clear_eventbuf(void)
{
    int i;
    unsigned long flags;
    spin_lock_irqsave(&(midicomm_event.lock),flags);
    for(i=0; i<MIDICOMM_EVENT_NUM;i++)
        midicomm_event.event[i] = 0;
    midicomm_event.write_pos = midicomm_event.read_pos = 0;
    midicomm_event.num = 0;
    spin_unlock_irqrestore(&(midicomm_event.lock),flags);


    return 0;
}


/*******************************************************************************
  * Function name:  midicomm_open
  * Arguments:      struct inode * 
  *                 struct file *
  * Return:         int         - 0  for successful, 
  *                               negative number signaling a error
  * Comments:   open operation
  *******************************************************************************/
static int midicomm_open(struct inode * inode, struct file * file)
{
    int i;
    unsigned long flags;

    MOD_INC_USE_COUNT;
    midicomm_debug(-1,"midicomm: entering midicomm_open\n");
    

    if (down_interruptible(&midicomm_mutex) != 0)
    {
        midicomm_debug(-1,"midicomm: midicomm_open: no midicomm_mutex\n");
        MOD_DEC_USE_COUNT;
        return -EINTR;
    }

/*
    if(usage_count)
    {
        midicomm_debug(-1,"midicomm: midicomm_open: device has been opened by other program\n");
        up(&midicomm_mutex);
        MOD_DEC_USE_COUNT;
        return -EBUSY;
    }

    usage_count ++;
*/
    up(&midicomm_mutex);

    spin_lock_irqsave(&(midicomm_event.lock),flags);
    for(i=0; i<MIDICOMM_EVENT_NUM;i++)
        midicomm_event.event[i] = 0;
    midicomm_event.write_pos = midicomm_event.read_pos = 0;
    midicomm_event.num = 0;
    spin_unlock_irqrestore(&(midicomm_event.lock),flags);


    return 0;
}
/*******************************************************************************
  * Function name:  midicomm_poll
  * Arguments:      struct inode *
  *                 struct file *
  * Return:         int         - 0  for successful,
  *                               negative number signaling a error
  * Comments:   release operation
  *******************************************************************************/

static unsigned int midicomm_poll(struct file *file, poll_table *wait)
{
    unsigned int mask = 0;
    if(!midicomm_eventbuf_empty())
    {
        mask = POLLIN | POLLRDNORM;
        return mask;
    }
    poll_wait(file,&midicomm_wait_queue,wait);
    if ( !midicomm_eventbuf_empty()) mask = POLLIN | POLLRDNORM;
    return mask;
}
/*******************************************************************************
  * Function name:  midicomm_release
  * Arguments:      struct inode *
  *                 struct file *
  * Return:         int         - 0  for successful,
  *                               negative number signaling a error
  * Comments:   release operation
  *******************************************************************************/
static int midicomm_release(struct inode * inode, struct file * file)
{
    midicomm_debug(-1,"midicomm: entering midicomm_release\n");

    if (down_interruptible(&midicomm_mutex) != 0)
    {
        midicomm_debug(-1,"midicomm: midicomm_release: no midicomm_mutex\n");
        return -EINTR;
    }
/*
    usage_count -- ;
    if(usage_count)
    {
        midicomm_debug(-1,"midicomm: usage_count: device has been opened by other program\n");
        up(&midicomm_mutex);
        return -EBUSY;
    }
*/
    up(&midicomm_mutex);
    MOD_DEC_USE_COUNT;

    return 0;
}
/*******************************************************************************
  * Function name:  midicomm_read
  * Arguments:      struct file *
  *                 char *buffer    - buffer in user space to which read operation
  *                                   copies device data  
  *                 size_t count    - size of the buffer
  *                 loff_t          - not used currently
  * Return:         ssize_t         - if successful, the size of copied data, or 
  *                                   error codes sigaling the failure
  * Comments:   read operation
  *******************************************************************************/
static ssize_t midicomm_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{

    int retval = 0;
    char buf[MIDICOMM_EVENT_NUM];
//  DECLARE_WAITQUEUE(wait, current);

    midicomm_debug(-1,"midicomm: entering midicomm_read\n");
    if((0 == count)||(NULL == buffer))
    {
        printk("<1>midicomm: midicomm_read: the size of user space buffer is zore\n");
        return -EFAULT;
    }
    
    if ( midicomm_eventbuf_empty())
    {
#if 0
        midicomm_debug(-1,"midicomm_read: event queue is empty\n");
        add_wait_queue(&midicomm_wait_queue, &wait);
        current->state = TASK_INTERRUPTIBLE;

        while (midicomm_eventbuf_empty())
        {
            if (file->f_flags & O_NONBLOCK)
            {
                midicomm_debug(-1,"midicomm_read: O_NONBLOCK\n");
                retval = -EAGAIN;
                break;
            }
            if (signal_pending(current))
            {
                midicomm_debug(-1,"midicomm_read: have signal\n");
                retval = -ERESTARTSYS;
                break;
            }

            schedule();
        }
        current->state = TASK_RUNNING;
        remove_wait_queue(&midicomm_wait_queue, &wait);
#endif
        return 0;
    }

    
    if (retval)
        return retval;

    midicomm_debug(-1,"midicomm_read: event queue is non-empty\n");
    /* copy event data to local kernel buf */
    retval = midicomm_eventbuf_read(buf,count);
    if( -1 == retval)
    {
        printk("<1>midicomm_read: read none\n");
        return 0;
    }
    
    if( copy_to_user(buffer,buf,retval))
    {
        midicomm_debug(-1,"midicomm_read: copy_to_user fails\n");
        return -EFAULT;
    }
    
    midicomm_debug(-1,"midicomm_read: exit with sucessful return \n");
    return retval;
    
}

static int midicomm_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{

    return -1;
}

/*******************************************************************************
  * Function name:  midicomm_intflag_write
  * Arguments:      unsigned char data      1-byte data which will be writen into
  *                                         event buffer
  * Return:         int     TRUE    -   sucess
  *                             FALSE   -   failure
  * Comments:   write operation called by ISR of MA-3 hardware
  *******************************************************************************/

int midicomm_intflag_write(unsigned char data)
{
    
    midicomm_debug(-1,"midicomm: entering midicomm_intflag_write \n");
    if(!midicomm_eventbuf_wirte(data))
        return FALSE;

    wake_up_interruptible(&midicomm_wait_queue);
    midicomm_debug(-1,"midicomm_intflag_write: exit with successful return \n");
    return TRUE;    
}

/*******************************************************************************
  * Function name:  midicomm_module_init
  * Arguments:      None
  * Return:     int
  * Comments:   it's initialization routine of midicomm module. In this function,
  *     midicomm device is registered to linux as a misc device
  *******************************************************************************/

int midicomm_module_init(void)
{
    int result;


    midicomm_debug(1,"hello,midicomm module\n");

    /* register "midicomm" to linux */
#ifdef CONFIG_DEVFS_FS
    midicomm_de = devfs_register(NULL,"midicomm",DEVFS_FL_AUTO_DEVNUM,
                                0, 0, S_IFCHR | S_IRUGO | S_IWUGO,
                                &midicomm_fops,NULL);

    if( NULL == midicomm_de)
        printk("<1>midicomm:devfs_register failed\n");
#else
    result = register_chrdev(midicomm_major, "midicomm", &midicomm_fops);
    if (result < 0)
    {
        printk("<1> can not get major %d\n",midicomm_major);
        return result;
    }
    if( 0 == midicomm_major)
    {
        midicomm_major = result;
        printk("<1>midicomm:midicomm_major = %d\n",midicomm_major);
    }
#endif
    spin_lock_init(&midicomm_event.lock);
    midicomm_event.num = MIDICOMM_EVENT_NUM ;
    return 0;
}


/*******************************************************************************
  * Function name:  midicomm_module_exit
  * Arguments:      None
  * Return:         None
  * Comments:   it's cleanup routine of midicomm module. In this function,
  *     all resources allocated in initialization routine are released
  *******************************************************************************/
void midicomm_module_exit(void)
{
#ifdef CONFIG_DEVFS_FS
    devfs_unregister(midicomm_de);
#else
    unregister_chrdev(midicomm_major,"midicomm");
#endif
    midicomm_debug(1,"bye,midicomm\n");
}




