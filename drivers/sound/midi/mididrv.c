/*
 * Header files *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <asm/arch/irqs.h>
#include <asm/io.h>

#include <linux/config.h>
#include <linux/pm.h>
#include <asm/hardware.h>

 /* for mixer*/
#ifdef HWMIDI_MIXER
//#include <asm/dma.h>
#include "../ezx-common.h"
#endif

#include "mididrv.h"
#include "madefs.h"

/*
 * declaration of functions *
 */


/* for open/close mixer */
//extern void poweron_mixer(mixer_type_enum );
//extern void shutdown_mixer(void);

extern int mixer_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
extern SINT32 midicomm_module_init(void);
extern void midicomm_module_exit(void);
extern int midicomm_clear_eventbuf(void);

extern SINT32 kmachdep_SendDelayedPacket(const UINT8 * ,UINT32 );
extern SINT32 kmachdep_SendDirectPacket(const UINT8 * ,UINT32 );
extern SINT32 kmachdep_SendDirectRamData(UINT32, UINT8, const UINT8 *, UINT32);
extern SINT32 kmachdep_SendDirectRamVal(UINT32,UINT32,UINT8);
extern void kmachdep_WriteStatusFlagReg( UINT8  );
extern void kmachdep_WriteDataReg( UINT8);
extern void kmachdep_Wait( UINT32);
extern UINT8 kmachdep_ReadDataReg(void);
extern SINT32 kmachdep_ReceiveData(UINT32, UINT8);
extern SINT32 kmachdep_CheckDelayedFifoEmpty(void);
extern UINT8 kmachdep_ReadStatusFlagReg(void);
extern void set_GPIO_mode(int);
extern int irq_request(int irq, void (*handler)(int,void *,struct pt_regs *),
                       unsigned long flags, const char *name, void *id);



extern INT32 midicomm_intflag_write(UINT8 data);

//ssize_t midi_read(struct file *filp, INT8 *buf, size_t count, loff_t *f_pos);
ssize_t midi_write(struct file *filp, const INT8 *buf, size_t count,
                loff_t *f_pos);

ssize_t midi_write(struct file *filp, const INT8 *buf, size_t count,
                loff_t *f_pos);

SINT32 midi_ioctl(struct inode *inode, struct file *filp,
                 UINT32 cmd, ULONG arg);

INT32 midi_release(struct inode *inode, struct file *filp);
INT32 midi_open(struct inode *inode, struct file *filp);

SINT32 midi_senddelayedpackage(const UINT8 *, UINT32);
SINT32 midi_senddirectpackage(const UINT8 *, UINT32);
SINT32 midi_sendramdata(const UINT8 *, UINT32);
SINT32 midi_sendramval(const UINT8 *, UINT32);
static void midi_init_gpio(void);
static void midi_intr_handler(int, void *, struct pt_regs *);
static INT32 ma3_DevicePwrMng(UINT8 ,UINT8 );

#ifdef CONFIG_PM
static int midi_PowerManagement_Cb(struct pm_dev *, pm_request_t , void *);
static int midi_PowerManagement_Suspend(void);
static int midi_PowerManagement_Resume(void);
#endif


static void midi_debug(INT32 l, const INT8 *);

static INT32 ma3_DevCtl(ULONG );
static INT32 ma3_PwrMng(ULONG );
static INT32 ma3_RcvData(ULONG);
static INT32 ma3_WaitDelayedFifoEpt(ULONG);
static INT32 ma3_CheckDelayedFifoEpt(ULONG);
static INT32 ma3_IntrMng(ULONG);
static INT32 ma3_InitRegs(ULONG);
static INT32 ma3_IoDebug(ULONG);
static INT32 ma3_HpVolCtl(ULONG);
static INT32 ma3_SetEqVol(ULONG);
static INT32 ma3_GetEqVol(ULONG);
static INT32 ma3_SetUseCount(ULONG);
static INT32 ma3_IoTest(ULONG);
static INT32 ma3_ClrIntrStatue(ULONG);
static INT32 ma3_BSetRegWr(ULONG);
static INT32 ma3_ChipReset(ULONG);
static INT32 ma3_IntrMngInIntr(ULONG);
static INT32 ma3_CheckPwrState(ULONG);

SINT32 ma3_ReceiveData(UINT32, UINT8, UINT8);
SINT32 ma3_VerifyRegisters(UINT8);






/*
 * global variables *
 */
#ifdef CONFIG_DEVFS_FS
    devfs_handle_t de;
#else
    INT32 midi_major = 0xef;
#endif

static UINT8 mididrv_buf[MIDI_DRV_BUF_SIZE];

unsigned long global_ioaddr = 0;

static DECLARE_MUTEX(mididrv_mutex);

SINT32 (*midi_writer_func[5])(const UINT8 *,UINT32);
static INT32 (*ma3_devctl_cmd[20])(ULONG);




struct file_operations midi_fops = {
//    read:       midi_read,
    write:      midi_write,
    ioctl:      midi_ioctl,
    open:       midi_open,
    release:    midi_release,
};

static INT32 midi_debug_flag = 0;//MIDI_DEBUG_FLAG;

#ifdef CONFIG_PM
struct pm_dev *midi_pm_dev;
#endif
int CanEnableIntrFlag = 1; // used to control the writing of IRQ bit of status 
                           // flag register
                           // 1: can write it to 1
                           // 0: can't write it to 1   
char PwrMngSTATE = POWER_NORMAL; 
int  Ma3ChipPowerOn = 0;   // 0: ma3 chipset is in powerdown mode
                           // 1: ma3 chipset is in poweron mode

int MixerStatueFlag = 0;   // 0: mixer hasn't been opened or has been closed
                           //    by midi drv
                           // 1: mixer has been opened by midi drv

int SmwInitFlag     = 0;   // 1: Middleware has initialize ma3 chipset
                           // 0: Middleware hasn't initialize chipset or reseted 
                           //    chipset.
/*******************************************************************************
  * Function name:  midi_debug
  * Arguments:      int level
  *                 char *message
  * Return:     int
  * Comments:   it's for debug only,when level is -1, it uses the default level of
  *     printk
  *******************************************************************************/

static inline void midi_debug(INT32 level, const INT8 *message)
{
    if(midi_debug_flag)
    {
        if(level == MIDI_DEBUG_DEFUALT_LEVEL)
        {
            printk("mididrv:%s",message);
            return ;
        }
        if((level > 7)||(level < -1))
            printk("Midi debug level is error: %s\n",message);
        printk("<1>mididrv:%s",message);
    }

}


/*******************************************************************************
  * Function name:  midi_write
  * Arguments:      struct file *filp
  *         const char *buf
  *         size_t count
  *         loff_t *f_pos
  * Return:     ssize_t : >0 indicating the number of written data
  *                               or else signaling an error
  * Comments:   writing data into MA-3
  *******************************************************************************/
ssize_t midi_write(struct file *filp, const INT8 *buf, size_t count,
                loff_t *f_pos)
{
    UINT8 cmd, flag = 0;
    UINT8 *tmp;
    SINT32 ret;
    

    midi_debug(1,"entering midi_write\n");
    if (down_interruptible(&mididrv_mutex) != 0)
    {
        printk( "<1>midi_write can get mididrv_mutex\n");
        return -EFAULT;
    }
    tmp = mididrv_buf;
    if((count <= 0) )
    {
        printk( "<1>midi_write: count is error:%d\n",count);
        up(&mididrv_mutex);
        return -EFAULT;
    }
    
    if(count > MIDI_DRV_BUF_SIZE)
    {
        if(!(tmp = kmalloc(count,GFP_KERNEL)))
        {
            printk( "<1>midi_write: kmalloc fails\n");
            up(&mididrv_mutex);
            return -EFAULT;
        }
        flag = 1;
    }



    if(copy_from_user(tmp,buf,count))
    {
        printk( "<1>midi_write: copy_from_user fails\n");
        if(1 == flag) kfree(tmp);
        up(&mididrv_mutex);
        return -EFAULT;
    }
    cmd = tmp[0];
    tmp ++ ;
    if((cmd > WRCMD_RAMVAL)||(cmd == 0))
    {
        printk( "<1>midi_write:cmd is error\n");
        if(1 == flag) 
        {
            tmp--;
            kfree(tmp);
        }
        up(&mididrv_mutex);
        return -EFAULT;
    }

    /* the below case will never happen, but for safety, we do it */
    if(NULL == midi_writer_func[cmd])
    {
        if(1 == flag)
        {
            tmp--;
            kfree(tmp);
        }
        up(&mididrv_mutex);
        printk( "<1>exit from midi_write with error\n");
        return -EFAULT;
    }

    ret = midi_writer_func[cmd](tmp, count -1);


    if( -1 == ret )
    {

        printk("<1> midi_write_func[%d] fails\n",cmd);
        if(1 == flag) kfree(tmp);
        up(&mididrv_mutex);
        return -EFAULT;
    }
    if(1 == flag) kfree(tmp);
    up(&mididrv_mutex);
    midi_debug(1,"exit from midi_write with success\n");
    return count;

}


/*******************************************************************************
  * Function name:  midi_ioctl
  * Arguments:      struct inode *inode
  *         struct file *filp
  *         unsigned int cmd
  *         unsigned long arg
  * Return:     int
  * Comments:   I/O control operation of MA-3
  *******************************************************************************/
INT32 midi_ioctl(struct inode *inode, struct file *filp,
                 UINT32 cmd, ULONG arg)
{
    INT32 ret = -EINVAL;
    midi_debug(-1,"entering midi_ioctl\n");
#ifdef MIDI_DEBUG_FLAG
    printk("<1> ioctl command = 0x%x\n",cmd);
#endif
    if((cmd&0xffffff00) == MA3_CTL_MASK)
    {
        if(ma3_devctl_cmd[MA3_GET_CMD(cmd)])
            ret = ma3_devctl_cmd[MA3_GET_CMD(cmd)](arg);
		
    }
	else 
        ret = mixer_ioctl(inode,filp,cmd,arg);
	

    return ret;
}


/*******************************************************************************
  * Function name:  midi_open
  * Arguments:      struct inode *inode
  *                 struct file  *file
  * Return:     0 for success, non-zero for error
  * Comments:   open operation
  *******************************************************************************/
INT32 midi_open(struct inode *inode, struct file *filp)
{
    INT32 ret;
	UINT8 read_data;
    midi_debug(1, "entering midi_open\n");
    
	// init all ma3-related gpios
    midi_init_gpio();
  
    // because only hardware can clear 	MA_INTERRUPT_FLAG_REG register,but the hardware 
	// reset of ma3 chipset is the app reset, so we clear the register in software way
    kmachdep_WriteStatusFlagReg(MA_INTERRUPT_FLAG_REG);
    read_data = kmachdep_ReadDataReg();
    kmachdep_WriteStatusFlagReg(MA_INTERRUPT_FLAG_REG);
    kmachdep_WriteDataReg(read_data);	
   	
	// power on ma3 chipset 
    ret = ma3_DevicePwrMng(1, 0x00);
    if (ret != 0) 
	{
        printk("<1>w20691:::ma3_DevicePwrMng(1, 0x00) fails in midi_open\n");
		return ret;
    }
    
    Ma3ChipPowerOn = 1;	//indicating midi chipset has been powered on
    PwrMngSTATE = POWER_NORMAL;
#ifdef HWMIDI_MIXER
	/* open mixer */
    poweron_mixer(MIDI_DEVICE);
    MixerStatueFlag = 1; // indicating mixer has been opened by app
    
#endif
    MOD_INC_USE_COUNT;

    return 0;
}

/*******************************************************************************
  * Function name:  midi_release
  * Arguments:      struct inode *inode
  *                 struct file  *file
  * Return:     0 for success
  * Comments:   close operation
  *******************************************************************************/
INT32 midi_release(struct inode *inode, struct file *filp)
{
    midi_debug(-1,"entering midi_release\n");
    

	
#ifdef HWMIDI_MIXER
    /* for close mixer */
    shutdown_mixer(MIDI_DEVICE);
    MixerStatueFlag = 0; //indicating mixer has been closed by app
#endif
   	/* reset of midi chipset, then power down midi */
    ma3_ChipReset(0);
    ma3_DevicePwrMng(2,0x00);
    Ma3ChipPowerOn = 0;
    kernel_clr_oscc_13m_clock();	
    MOD_DEC_USE_COUNT;
    return 0;
}

/*******************************************************************************
  * Function name:  midi_module_init
  * Arguments:      none
  * Return:     0 for success, non-zero for error
  * Comments:   module initialization function
  *******************************************************************************/
INT32 __init midi_module_init(void)
{
    INT32 result;
    void *ioaddr;
    unsigned long physaddr;


    midi_debug(-1,"hello,midi module\n");
    midicomm_module_init();
    
        
    /* register  functions for write operation */
    midi_writer_func[WRCMD_DELAY]   = midi_senddelayedpackage;
    midi_writer_func[WRCMD_DIRECT]  = midi_senddirectpackage;
    midi_writer_func[WRCMD_RAM]     = midi_sendramdata;
    midi_writer_func[WRCMD_RAMVAL]  = midi_sendramval;

    /* register functions for I/O control */
    ma3_devctl_cmd[MA3_GET_CMD(MA3_DEV_CTL)]    = ma3_DevCtl;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_PWR_MNG)]    = ma3_PwrMng;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_RCV_DATA)]   = ma3_RcvData;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_WAIT_DELAYEDFIOF_EPT)]  = ma3_WaitDelayedFifoEpt;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_CHECK_DELAYEDFIOF_EPT)] = ma3_CheckDelayedFifoEpt;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_INT_MNG)]    = ma3_IntrMng;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_INIT_REG)]   = ma3_InitRegs;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_DEBUG)]      = ma3_IoDebug;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_HPVOL_CTL)]  = ma3_HpVolCtl;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_SET_EQVOL)]  = ma3_SetEqVol;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_GET_EQVOL)]  = ma3_GetEqVol;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_SET_USECOUNT)]   = ma3_SetUseCount;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_IO_TEST)]        = ma3_IoTest;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_BSET_REG_WR)]    = ma3_BSetRegWr;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_CLR_INTRSTATUS)] = ma3_ClrIntrStatue;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_CHIP_RESET)]     = ma3_ChipReset;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_INTRMNG_IN_INTR)]     = ma3_IntrMngInIntr;
    ma3_devctl_cmd[MA3_GET_CMD(MA3_CHECK_PWR_STATE)]     = ma3_CheckPwrState;
    /* init GPIO33/78/15/09 */
    midi_init_gpio();
	
#ifdef CONFIG_PM
	pm_register(PM_UNKNOWN_DEV, PM_SYS_UNKNOWN, midi_PowerManagement_Cb);
#endif

    /* nCS<5> selects 0x14000000 as its starting addr */
    physaddr = MIDI_PHY_ADDR;
    ioaddr = ioremap(physaddr,MIDI_IOMEM_LEN);
    if(!request_mem_region((unsigned long)ioaddr,MIDI_IOMEM_LEN,"midi"))
    {
        //printk("<1>request_region fails\n");
        midicomm_module_exit();
        iounmap(ioaddr);
        return -EBUSY;
    }

    global_ioaddr = (unsigned long)ioaddr; 
#ifdef MIDI_DEBUG_FLAG
    printk("<1> ioaddr = 0x%x\n",(unsigned int)ioaddr);
#endif

 
    /* register ma-3 device as a char dev in linux */
#ifdef CONFIG_DEVFS_FS
    printk("<1>devfs is support\n");
    de = devfs_register(NULL,"midi",DEVFS_FL_AUTO_DEVNUM,
            0, 0, S_IFCHR | S_IRUGO | S_IWUGO,
            &midi_fops,NULL);

    if( NULL == de)
    {
        printk("<1>devfs_register failed\n");
        midicomm_module_exit();
        release_mem_region(global_ioaddr,MIDI_IOMEM_LEN);
        iounmap(ioaddr);
        return -1;
    }
#else
    result = register_chrdev(midi_major, "midi", &midi_fops);
    if (result < 0)
    {
        printk("<1> can't get major %d\n",midi_major);
        midicomm_module_exit();
        release_mem_region(global_ioaddr,MIDI_IOMEM_LEN);
        iounmap(ioaddr);

        return result;
    }
    if( midi_major == 0)
    {
        midi_major = result;
#ifdef MIDI_DEBUG_FLAG
        printk("<1>midi_major = %d\n",midi_major);
#endif
    }
#endif/* CONFIG_DEVFS_FS */

    /* irq register */
    if(request_irq(IRQ_GPIO(15), midi_intr_handler, SA_INTERRUPT, "midi", NULL))
    {
        printk("<1>midi_module_init: request_irq fails\n");
        midicomm_module_exit();

#ifdef CONFIG_DEVFS_FS
        devfs_unregister(de);
#else
        unregister_chrdev(midi_major,"midi");
#endif
        release_mem_region(global_ioaddr,MIDI_IOMEM_LEN);
        iounmap((void *)global_ioaddr);
        return -EIO;
    }
    kernel_clr_oscc_13m_clock();
 
    return 0;
}


/*******************************************************************************
  * Function name:  midi_module_exit
  * Arguments:      none
  * Return:         none
  * Comments:   module exit function
  *******************************************************************************/
void __exit midi_module_exit(void)
{
    midicomm_module_exit();
	
#ifdef CONFIG_PM
    pm_unregister(midi_pm_dev);
#endif

    free_irq(IRQ_GPIO(15),NULL);
        
#ifdef CONFIG_DEVFS_FS
    devfs_unregister(de);
#else
    unregister_chrdev(midi_major,"midi");
#endif

    release_mem_region(global_ioaddr,MIDI_IOMEM_LEN);

    iounmap((void *)global_ioaddr);
#ifdef MIDI_DEBUG_FLAG
    printk("<1>bye,midi module\n");
#endif
}

module_init(midi_module_init);
module_exit(midi_module_exit);







/*******************************************************************************
  * Function name:  midi_senddelayedpackage
  * Arguments:      const UINT8 *data : poiter to written data
  *         UINT32 size : the size of data
  * Return:     int
  * Comments:   it writes data into delayed fifo of MA-3
  *******************************************************************************/
SINT32 midi_senddelayedpackage(const UINT8 *data, UINT32 size)
{
    SINT32 ret = 0;
    SINT32 len;
    
    if((NULL == data)||(0 == size))
    {
        printk("<1>midi_senddelayedpackage:data and size error\n");
        return MIDI_FAILURE;
    }

#ifdef MIDI_DEBUG_FLAG
    printk("<1>midi_senddelayedpackage: size = %d,  %x,%x,%x,%x,%x\n",size,data[0],data[1],data[2],data[3],data[4]);
#endif

    len = (data[1] | data[2]<<8 | data[3]<<16 | data[4]<<24);
    if(len != size - 5)
    {
        printk("<1>midi_senddelayedpackage:size error:len = %d,size = %d\n",len,size);
        return MIDI_FAILURE;
    }
    ret = kmachdep_SendDelayedPacket(data+5,size-5);
    if (CanEnableIntrFlag)
        kmachdep_WriteStatusFlagReg(data[0]);
    return ret;


}

/*******************************************************************************
  * Function name:  midi_senddirectpackage
  * Arguments:      const UINT8 *data : poiter to written data
  *         UINT32 size : the size of data
  * Return:     int
  * Comments:   it writes data into direct fifo of MA-3
  *******************************************************************************/
SINT32 midi_senddirectpackage(const UINT8 *data, UINT32 size)
{
    SINT32 ret = 0;
    SINT32 len;

    
    if((NULL == data)||(0 == size))
    {
        printk("<1>midi_senddirectpackage:data and size error\n");
        return MIDI_FAILURE;
    }
#ifdef MIDI_DEBUG_FLAG
    printk("<1>midi_senddirectpackage: size = %d, %x,%x,%x,%x,%x\n",size,data[0],data[1],data[2],data[3],data[4]);
#endif

    len = (data[1] | data[2]<<8 | data[3]<<16 | data[4]<<24);
    if(len != size - 5)
    {
        printk("<1>midi_senddirectpackage:size error:len = %d,size = %d\n",len,size);
        return MIDI_FAILURE;
    }
    ret = kmachdep_SendDirectPacket(data+5,size-5);
    if (CanEnableIntrFlag)
        kmachdep_WriteStatusFlagReg(data[0]);
    return ret;

}

/*******************************************************************************
  * Function name:  midi_sendramdata
  * Arguments:      const UINT8 *data : poiter to information and written data
  *         UINT32 size : the size of data
  * Return:     int
  * Comments:   it writes data into the internal ram of MA-3
  *******************************************************************************/
SINT32 midi_sendramdata(const UINT8 *data, UINT32 size)
{
    tMidiRamBuf *pmidiram;
    SINT32 ret = 0;

    pmidiram = (tMidiRamBuf *)data;

    if((NULL == data)|| (10 > size) || ( size !=  pmidiram->ram_size + 10))
    {

        printk("<1>midi_sendramdata fail: size = %d,ramaddr = 0x%x, datatype = %d,ramsize = %d, int_flag=0x%x\n",
            size,pmidiram->ram_addr,pmidiram->a.data_type,pmidiram->ram_size,data[9]);

        return -1;
    }
#ifdef MIDI_DEBUG_FLAG
    printk("<1>midi_sendramdata: size = %d,ramaddr = 0x%x, datatype = %d,ramsize = %d\n",
            size,pmidiram->ram_addr,pmidiram->a.data_type,pmidiram->ram_size);
#endif
    ret = kmachdep_SendDirectRamData(pmidiram->ram_addr,pmidiram->a.data_type,
                (data+10) ,pmidiram->ram_size);

    if (CanEnableIntrFlag)
        kmachdep_WriteStatusFlagReg(data[9]);
    return ret;   

}

/*******************************************************************************
  * Function name:  midi_sendramval
  * Arguments:      const UINT8 *data : poiter to the information and
  *                                         written data
  *         UINT32 size : the size of data
  * Return:     int
  * Comments:   it writes data into the internal ram of MA-3
  *******************************************************************************/
SINT32 midi_sendramval(const UINT8 *data, UINT32 size)
{
    tMidiRamBuf *pmidiram;
    SINT32 ret = 0;

    if((NULL == data)||(10 != size))
    {
        printk("<1> midi_sendramval: size = %d\n",size);
        return -1;
    }
    pmidiram = (tMidiRamBuf *)data;

#ifdef MIDI_DEBUG_FLAG
    printk("<1>midi_sendramval: size = %d,ramaddr = 0x%x, ram_val = %d,ramsize = %d\n",
            size,pmidiram->ram_addr,pmidiram->a.ram_val,pmidiram->ram_size);
#endif
    ret = kmachdep_SendDirectRamVal(pmidiram->ram_addr, pmidiram->ram_size, pmidiram->a.ram_val);
    if (CanEnableIntrFlag)
        kmachdep_WriteStatusFlagReg(data[9]);
    return ret;

}

/* MA-3 device control */

/*******************************************************************************
  * Function name:  ma3_DevCtl
  * Arguments:  ULONG arg: in fact, it's a pointer to tDevCtlInfo structure
  * Return:     0 for success, all others signal an error
  * Comments:   MA-3 device control function
  *******************************************************************************/
static INT32 ma3_DevCtl(ULONG arg)
{
    tDevCtlInfo structDevCtl;
    UINT8 param1, param2, param3, cmd, intr_flag;

    midi_debug(1,"entering ma3_DevCtl\n");

    if( 0 == arg)
    {
        printk("<1>ma3_DevCtl fails: arg = 0\n");
        return -EFAULT;
    }

    if(copy_from_user(&structDevCtl,(void *)arg,sizeof(tDevCtlInfo)))
    {
        midi_debug(-1,"ma3_DevCtl: copy_from_user fails\n");
        return -EFAULT;
    }
    cmd    = structDevCtl.cmd;
    param1 = structDevCtl.param1;
    param2 = structDevCtl.param2;
    param3 = structDevCtl.param3;
    intr_flag = structDevCtl.intr_flag;
#ifdef MIDI_DEBUG_FLAG
    printk("<1> ma3_DevCtl: cmd=%d p1=%d p2=%d p3=%d\n", cmd, param1, param2, param3);
#endif


    if ( cmd <= 6 )
    {
        kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
        kmachdep_WriteDataReg( 0x00 );

        switch ( cmd )
        {
        case 0:
            kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
            kmachdep_WriteDataReg( param1 );
            break;
        case 1:
            kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
            kmachdep_WriteDataReg( (UINT8)(param1 & 0xBF) );
            break;
        case 2:
            kmachdep_WriteStatusFlagReg( MA_ANALOG_EQVOL_REG );
            kmachdep_WriteDataReg( (UINT8)(param1 & 0x1F) );
            break;
        case 3:
            kmachdep_WriteStatusFlagReg( MA_ANALOG_HPVOL_L_REG );
            kmachdep_WriteDataReg( (UINT8)((param1<<7) | (param2&0x1F)) );
            kmachdep_WriteStatusFlagReg( MA_ANALOG_HPVOL_R_REG );
            kmachdep_WriteDataReg( param3 );
            break;
        case 4:
            kmachdep_WriteStatusFlagReg( MA_ANALOG_SPVOL_REG );
            kmachdep_WriteDataReg( (UINT8)((MA_VSEL<<6) | (param1&0x1F)) );
            break;
        case 5:
            kmachdep_WriteStatusFlagReg( MA_LED_SETTING_1_REG );
            kmachdep_WriteDataReg( (UINT8)(param1 & 0x3F) );
            kmachdep_WriteStatusFlagReg( MA_LED_SETTING_2_REG );
            kmachdep_WriteDataReg( (UINT8)((param2<<4) | (param3 & 0x07)) );
            break;
        case 6:
            kmachdep_WriteStatusFlagReg( MA_MOTOR_SETTING_1_REG );
            kmachdep_WriteDataReg( (UINT8)(param1 & 0x3F) );
            kmachdep_WriteStatusFlagReg( MA_MOTOR_SETTING_2_REG );
            kmachdep_WriteDataReg( (UINT8)((param2<<4) | (param3 & 0x07)) );
            break;
        }
        if (CanEnableIntrFlag)
            kmachdep_WriteStatusFlagReg( intr_flag );
    }
    else if ( cmd <= 7 )
    {
        kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
        kmachdep_WriteDataReg( 0x01 );

        kmachdep_WriteStatusFlagReg( MA_PLL_SETTING_1_REG );
        kmachdep_WriteDataReg( (UINT8)(param1) );
        kmachdep_WriteStatusFlagReg( MA_PLL_SETTING_2_REG );
        kmachdep_WriteDataReg( (UINT8)(param2 & 0x7F) );
        if (CanEnableIntrFlag)
            kmachdep_WriteStatusFlagReg( intr_flag );
    }
    midi_debug(1,"exitting ma3_DevCtl with success\n");
    return 0;
}


/*******************************************************************************
  * Function name:  ma3_PwrMng
  * Arguments:  ULONG arg: in fact, it includes 2 sub-fields: one is mode, and
  *                        another is the interrupt state
  * Return:     0 for success, all others signal an error
  * Comments:   MA-3 device power management function
  *******************************************************************************/
static INT32 ma3_PwrMng(ULONG arg)
{
    UINT8   mode;
    UINT8   intr_flag;
    UINT32  tmp;
	INT32  ret;

    midi_debug(1,"entering ma3_PwrMng\n");
    
    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        printk("<1>ma3_PwrMng: copy_from_user fails\n");
        return -EFAULT;
    }
    
    mode = (UINT8)(tmp&0xff);
    intr_flag = (UINT8)((tmp&0xff00)>>8);
#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_PwrMng: mode=%d, intr_flag = %d\n", mode, intr_flag);
#endif
    /* if ma3 chipset has been powered on,do nothing and  return successfully */
    if( Ma3ChipPowerOn == 1) 
	{
        SmwInitFlag = 1;
        PwrMngSTATE = POWER_NORMAL;

	    return 0;
	}
   	
    ret = ma3_DevicePwrMng(mode, intr_flag);
    if(( ret == 0)&&(mode == 1)) 
    {
        Ma3ChipPowerOn = 1; // poweron of ma3
        PwrMngSTATE = POWER_NORMAL;

#ifdef HWMIDI_MIXER
        if( MixerStatueFlag == 0)
        {
            poweron_mixer(MIDI_DEVICE);
            MixerStatueFlag = 1; // poweron of mixer		
        }
#endif
    }
    if(( ret == 0)&&(mode == 3))  
    {
        Ma3ChipPowerOn = 0; // powerdown of ma3

#ifdef HWMIDI_MIXER
        if(MixerStatueFlag == 1)
        {
            shutdown_mixer(MIDI_DEVICE);
            MixerStatueFlag = 0; //powerdown of mixer
        }
#endif
    }

    return ret;
    
}

/*******************************************************************************
  * Function name: ma3_DevicePwrMng 
  * Arguments:  mode:
  *             int_flag
  * Return:     0 for success, all others signal an error
  * Comments:   MA-3 device power management function
  *******************************************************************************/
static INT32 ma3_DevicePwrMng(UINT8 mode, UINT8 intr_flag)
{
    UINT8   count;                      /* loop counter */
    SINT32  result = MIDI_SUCCESS;      /* result of function */
   
    midi_debug(1,"entering ma3_DevicePwrMng\n");
    
#ifdef MIDI_DEBUG_FLAG
    printk("<1> ma3_DevicePwrMng: mode=%d, intr_flag = %d\n", mode, intr_flag);
#endif
    switch ( mode )
    {
    case 0:

        /* sequence of hardware initialize when power downed */

        /* set BANK bits of REG_ID #4 basic setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
        kmachdep_WriteDataReg( 0x00 );

        /* set DP0 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 );

        /* set PLLPD bit of power management (A) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_AP4R | MA_AP4L | MA_AP3 | MA_AP2 | MA_AP1 | MA_AP0 );

        /* wait 10ms */
        kmachdep_Wait( 10 * 1000 * 1000 );

        /* set DP1 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 );

        /* set DP2 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteDataReg( MA_DP3 );

        for ( count = 0; count < MA_RESET_RETRYCOUNT; count++ )
        {
            /* set RST bit of REG_ID #4 basic setting register to '1' */
            kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
            kmachdep_WriteDataReg( 0x80 );

            /* set RST bit of REG_ID #4 basic setting register to '0' */
            kmachdep_WriteDataReg( 0x00 );

            /* wait 41.7us */
            kmachdep_Wait( 41700 );

            /* verify the initialized registers by software reset */
            result = ma3_VerifyRegisters(intr_flag);
            if ( result == MIDI_SUCCESS ) break;
        }

        /* set DP2 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 );

        /* set DP1 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 );

        /* set PLLPD bit of REG_ID #6 power management (A) setting register to '1' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_PLLPD | MA_AP4R | MA_AP4L | MA_AP3 | MA_AP2 | MA_AP1 | MA_AP0 );

        /* set DP0 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 | MA_DP0 );

        /* enable interrupt if needed */
	if (CanEnableIntrFlag)
            kmachdep_WriteStatusFlagReg( intr_flag );

        break;

    case 1:

        /* sequence of hardware initialize when normal */

        /* set BANK bits of REG_ID #4 basic setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
        kmachdep_WriteDataReg( 0x00 );

        /* set DP0 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 );

        /* set PLLPD and AP0 bits of REG_ID #6 power management (A) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_AP4R | MA_AP4L | MA_AP3 | MA_AP2 | MA_AP1 );

        mdelay( 30 );

        /* set DP1 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 );

        /* set DP2 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteDataReg( MA_DP3 );

        for ( count = 0; count < 10; count++ )
        {
            /* set RST bit of REG_ID #4 basic setting register to '1' */
            kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
            kmachdep_WriteDataReg( 0x80 );

            /* set RST bit of REG_ID #4 basic setting register to '0' */
            kmachdep_WriteDataReg( 0x00 );

            /* wait 41.7us */
            mdelay( 10 );

            /* verify the initialized registers by software reset */
            result = ma3_VerifyRegisters(intr_flag);
            if(result != MIDI_SUCCESS)
                printk("<1>verify register fails\n");
            if ( result == MIDI_SUCCESS ) break;
        }

        /* set DP3 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( 0x00 );

        /* set AP1, AP3 and AP4 bits of REG_ID #6 power management (A) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_AP2 );

        /* wait 10us */
        mdelay( 10 );

        /* set AP2 bit of REG_ID #6 power management (A) setting register to '0' */
        kmachdep_WriteDataReg( 0x00 );
        
        /* because SPERKER isn't used now, so we set AP2/AP1 to 1 */
        kmachdep_WriteDataReg( MA_AP2 | MA_AP1 );

        /* enable interrupt */
        if( CanEnableIntrFlag )
            kmachdep_WriteStatusFlagReg( intr_flag );

        /* normal */

        break;

    case 2:

        /* sequence of power down */

        /* set BANK bits of REG_ID #4 basic setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
        kmachdep_WriteDataReg( 0x00 );


        /* set DP3 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 );

        /* set DP2 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 );

        /* set DP1 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 );

        /* set AP1, AP2, AP3, AP4 and PLLPD bits of REG_ID #6 power management (A) setting register to '1' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_PLLPD | MA_AP4R | MA_AP4L | MA_AP3 | MA_AP2 | MA_AP1 | MA_AP0 );

        /* set DP0 bit of REG_ID #5 power management (D) setting register to '1' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 | MA_DP0 );

        /* enable interrupt */
        if( CanEnableIntrFlag )
            kmachdep_WriteStatusFlagReg( intr_flag );

        break;

    case 3:


        /* set BANK bits of REG_ID #4 basic setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
        kmachdep_WriteDataReg( 0x00 );

        /* set DP0 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 | MA_DP1 );

        /* set AP0 and PLLPD bits of REG_ID #6 power management (A) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_AP4R | MA_AP4L | MA_AP3 | MA_AP2 | MA_AP1 );

        /* wait 10ms */
        kmachdep_Wait( 10 * 1000 * 1000 );

        /* set DP1 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_DIGITAL_REG );
        kmachdep_WriteDataReg( MA_DP3 | MA_DP2 );

        /* set DP2 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteDataReg( MA_DP3 );

        /* set DP3 bit of REG_ID #5 power management (D) setting register to '0' */
        kmachdep_WriteDataReg( 0x00 );

        /* set AP1, AP3 and AP4 bits of REG_ID #6 power management (A) setting register to '0' */
        kmachdep_WriteStatusFlagReg( MA_POWER_MANAGEMENT_ANALOG_REG );
        kmachdep_WriteDataReg( MA_AP2 );

        /* wait 10us */
        kmachdep_Wait( 10 * 1000 );

        /* set AP2 bit of REG_ID #6 power management (A) setting register to '0' */
        kmachdep_WriteDataReg( 0x00 );
        
	/* because SPERKER isn't used now, so we set AP2/AP1 to 1 */
	kmachdep_WriteDataReg( MA_AP2 | MA_AP1 );
		
        /* enable interrupt */
        if( CanEnableIntrFlag )
            kmachdep_WriteStatusFlagReg( intr_flag );

        break;

    default:
        midi_debug(1,"ma3_DevicePwrMng returns with error\n");
        return -EFAULT;
    }

    return result;

}

/*******************************************************************************
  * Function name:  ma3_RcvData
  * Arguments:  ULONG arg: in fact, it's a pointer to tRcvDataInfo structure
  * Return:     0 for success, all others signal an error
  * Comments:   This function read the content of the control register and ram
  *******************************************************************************/
static INT32 ma3_RcvData(ULONG arg)
{
    tRcvDataInfo structRcvData;
    UINT32 reg_addr;
    UINT8  buf_addr, intr_flag;
    SINT32  result;

    midi_debug(1, "entering ma3_RcvData\n");

    if(copy_from_user(&structRcvData,(void *)arg,sizeof(tRcvDataInfo)))
    {
        printk("<1>ma3_RcvData: copy_from_user fails\n");
        return -EFAULT;
    }

    reg_addr  = structRcvData.reg_addr;
    buf_addr  = structRcvData.buf_addr;
    intr_flag = structRcvData.intr_flag;
#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_RcvData:reg_addr = 0x%x,buf_addr = %d, intr_flag = %d\n",
                reg_addr, buf_addr, intr_flag);
#endif

//  return 0;

    result = ma3_ReceiveData(reg_addr, buf_addr, intr_flag);
    if(MIDI_FAILURE == result)
    {
        printk("<1>ma3_RcvData fails when calling ma3_ReceiveData\n");
        return -EFAULT;
    }

    if(copy_to_user((void *)arg, &result, sizeof(UINT8)))
    {
        printk("<1>ma3_RcvData: copy_to_user fails\n");
        return -EFAULT;
    }
    midi_debug(1,"ma3_RcvData returns with success\n");
    return MIDI_SUCCESS;
}


/*******************************************************************************
  * Function name:  ma3_ReceiveData
  * Arguments:  UINT32 address: the address of control registers or ram
  *             UINT8  buffer_addr: the address of read buffer
  *             UINT8  intr_flag: interrupt state
  * Return:      -1 signal an error, the value not less than 0 stands for success
  * Comments:   This function read the content of the control register and ram
  *******************************************************************************/
SINT32 ma3_ReceiveData(UINT32 address, UINT8 buffer_address, UINT8 intr_flag)
{
    SINT32 result;
#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_ReceiveData: adrs=%d, bufadrs=%d\n", address, buffer_address);
#endif
    if(buffer_address > 2)
        buffer_address = 0;

    result = kmachdep_ReceiveData( address, buffer_address );

    if( CanEnableIntrFlag )
        kmachdep_WriteStatusFlagReg( intr_flag );
#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_ReceiveData returns with result = %d\n", result);
#endif
    return result;
}



/*******************************************************************************
  * Function name:  ma3_VerifyRegisters
  * Arguments:  UINT8  intr_flag: interrupt state
  * Return:      -1 signal an error, 0 stands for success
  * Comments:   This function make sure that some registers are right
  *******************************************************************************/
SINT32 ma3_VerifyRegisters(UINT8 intr_flag)
{
    UINT32  reg_adrs;                   /* register address */
    UINT32  count;                      /* loop count */
    UINT8   data;                       /* read data */

    midi_debug(-1,"ma3_VerifyRegisters\n");

    //return 0;

    /* Verify the channel volume value to 0x60 */
    reg_adrs = (UINT32)MA_CHANNEL_VOLUME;
    for ( count = 0; count < 16; count++ )
    {
        data = (UINT8)ma3_ReceiveData( (UINT32)(reg_adrs + count), 2, intr_flag );
        if ( data != 0x60 )
        {
            printk("<1>ma3_VerifyRegisters:ma3_ReceiveData return with data %d != 0x60\n",data);
            return MIDI_FAILURE;
        }
    }

    /* Verify the panpot value to 0x3C */
    reg_adrs = (UINT32)MA_CHANNEL_PANPOT;
    for ( count = 0; count < 16; count++ )
    {
        data = (UINT8)ma3_ReceiveData( (UINT32)(reg_adrs + count), 2, intr_flag);
        if ( data != 0x3C )
        {
            printk("<1>ma3_VerifyRegisters:ma3_ReceiveData return with data %d != 0x3c\n",data);
            return MIDI_FAILURE;
        }
    }

    /* Verify the WT position value to 0x40 */
    reg_adrs = (UINT32)MA_WT_PG;
    for ( count = 0; count < 8; count++ )
    {
        data = (UINT8)ma3_ReceiveData( (UINT32)(reg_adrs + count), 2, intr_flag);
        if ( data != 0x40 )
        {
            printk("<1>ma3_VerifyRegisters:ma3_ReceiveData return with data %d != 0x40\n",data);
            return MIDI_FAILURE;
        }
    }
    midi_debug(1,"ma3_VerifyRegisters returns with success\n");
    return MIDI_SUCCESS;
}

/*******************************************************************************
  * Function name:  ma3_WaitDelayedFifoEpt
  * Arguments:  ULONG arg: not used
  * Return:      -1 signal an error, 0 stands for success
  * Comments:   This function will wait until the delayed fifo is empty
  *******************************************************************************/
static INT32 ma3_WaitDelayedFifoEpt(ULONG arg )
{
    UINT32 read_data;
    (void)arg;
#if 0
    do
    {
        read_data = kmachdep_CheckDelayedFifoEmpty();

        /* the next block will be modified, wuhongxing */
        if ( /* time */10 > MA_STATUS_TIMEOUT )
        {
            /* timeout: stop timer */
            return MIDI_FAILURE;
        }
    }
    while ( read_data==0 );
#endif
    midi_debug(1,"entering ma3_WaitDelayedFifoEpt\n");

//  return 0;


    read_data = kmachdep_CheckDelayedFifoEmpty();

    if(0 == read_data)
    {
        mdelay(2);
        read_data = kmachdep_CheckDelayedFifoEmpty();
        if(0 == read_data)
        {
//            printk("<1>ma3_WaitDelayedFifoEpt returns with error\n");
            return MIDI_FAILURE;
        }
    }
    midi_debug(1,"ma3_WaitDelayedFifoEpt returns with success\n");
    return MIDI_SUCCESS;
}


/*******************************************************************************
  * Function name:  ma3_CheckDelayedFifoEpt
  * Arguments:  ULONG arg: not used
  * Return:      -1 for nonempty, 0 stands for emptyness
  * Comments:   This function will check whether the delayed fifo is empty
  *******************************************************************************/
static INT32 ma3_CheckDelayedFifoEpt(ULONG arg)
{
    (void)arg;

    midi_debug(1, "entering ma3_CheckDelayedFifoEpt\n");

//  return 0;


    if(0 == kmachdep_CheckDelayedFifoEmpty())
    {
//        printk("<1>ma3_CheckDelayedFifoEpt returns with error\n");
        return MIDI_FAILURE;
    }
    midi_debug(1, "ma3_CheckDelayedFifoEpt returns with success\n");
    return MIDI_SUCCESS;
}

/*******************************************************************************
  * Function name:  ma3_IntrMng
  * Arguments:  ULONG arg: the lowest 1 byte stands for interrupt state
  * Return:      -1 for error, 0 stands for successs
  * Comments:   diable or enable interrupt
  *******************************************************************************/
static INT32 ma3_IntrMng(ULONG arg)
{
    UINT8 intr_flag ;
    UINT32 tmp;

    midi_debug(1, "entering ma3_IntrMng\n");
    
    if(copy_from_user(&tmp,(void *)arg, sizeof(ULONG)))
    {
        printk("<1>ma3_IntrMng: copy_from_user fails\n");
        return -EFAULT;
    }
    
    intr_flag = (UINT8)tmp;

#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_IntrMng:arg = 0x%lx\n",intr_flag);
#endif

//    printk("<1>ma3_IntrMng:arg = 0x%lx\n",intr_flag);
    if((intr_flag == 0x80) || (intr_flag == 0x00))
    {
        if( CanEnableIntrFlag )
            kmachdep_WriteStatusFlagReg(intr_flag);
        midi_debug(1, "ma3_IntrMng returns with success\n");
        return MIDI_SUCCESS;
    }
    midi_debug(1, "ma3_IntrMng returns with error\n");
    return MIDI_FAILURE;
}

/*******************************************************************************
  * Function name:  ma3_InitRegs
  * Arguments:  ULONG arg: not used
  * Return:      0 stands for successs, this function always return success
  * Comments:   initializing some registers
  *******************************************************************************/
static INT32 ma3_InitRegs(ULONG arg)
{
    UINT8   count;


    static UINT8 index[15] = { 0x00, 0x04, 0x07, 0x08, 0x09, 0x0A, 0x0F, 0x04,
                               0x0B, 0x0C, 0x04, 0x05, 0x06, 0x0A, 0x0D };

    static UINT8 data[15]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
                               0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00 };
    (void)arg;

    midi_debug(1, "entering ma3_InitRegs\n");


    for ( count = 0; count < 15; count++ )
    {
        /* set index of interrupt flag register to REG_ID */
        kmachdep_WriteStatusFlagReg( index[count] );

        /* write data to the target register */
        kmachdep_WriteDataReg( data[count] );
    }
    CanEnableIntrFlag = 1;
    midi_debug(1, "ma3_InitRegs returns with success\n");
    return MIDI_SUCCESS;
}

/*******************************************************************************
  * Function name:  ma3_IoDebug
  * Arguments:  ULONG arg: the debug enable/disable flag or debug level
  * Return:      -1 for error, 0 stands for successs
  * Comments:   for debug only
  *******************************************************************************/
static INT32 ma3_IoDebug(ULONG arg)
{
    UINT32 tmp;
    midi_debug(1,"entering ma3_IoDebug\n");
    
    
    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        printk("<1>ma3_IoDebug: copy_from_user fails\n");
        return -EFAULT;
    }

#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_IoDebug: arg = %d\n",tmp);
#endif
    switch(tmp)
    {
        case MIDI_DEBUG_DISABLE:
            midi_debug_flag = MIDI_DEBUG_DISABLE;
            break;

        case MIDI_DEBUG:
            midi_debug_flag = MIDI_DEBUG;
            break;

        case MIDI_DEBUG_CRITICAL_LEVEL:
            break;

        case MIDI_DEBUG_MAJOR_LEVEL:
            break;

        case MIDI_DEBUG_ALL:
            break;

        default:
            return MIDI_FAILURE;
    }

    return MIDI_SUCCESS;
}


/*******************************************************************************
  * Function name:  ma3_HpVolCtl
  * Arguments:  ULONG arg: it's a pointer to a integer
  * Return:       0 stands for success, or signals an error
  * Comments:   be used to control the output of Headphone
  *******************************************************************************/
static INT32 ma3_HpVolCtl(ULONG arg)
{
    UINT8 intr_flag, cmd, hpvol;
    UINT8 getvol;
    ULONG param;

    midi_debug(1, "entering ma3_HpVolCtl \n");
    if(copy_from_user(&param,(void *)arg, sizeof(ULONG)))
    {
        printk("<1>ma3_HpVolCtl: copy_from_user fails\n");
        return -EFAULT;
    }

    hpvol     = (UINT8)(param&0xff);
    cmd       = (UINT8)((param&0xff00)>>8);
    intr_flag = (UINT8)((param&0xff0000)>>16);

#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_HpVolCtl: hpvol = %d, cmd = %d, intr_flag = %d\n",hpvol,cmd,intr_flag);
#endif


    switch(cmd)
    {
        case 0x01: /* get the volume of the left out of Headphone */
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_L_REG);
            getvol = (kmachdep_ReadDataReg()&0x1F);
            if(copy_to_user((void *)arg,&getvol,sizeof(UINT8)))
            {
                printk("<1>ma3_HpVolCtl: copy_to_user fails when getting left out volume of headphone\n");
                if( CanEnableIntrFlag )
                    kmachdep_WriteStatusFlagReg(intr_flag);
                return -EFAULT;
            }

            break;

        case 0x02:/* get the volume of the right out of Headphone */
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_R_REG);
            getvol = (kmachdep_ReadDataReg()&0x1F);
            if(copy_to_user((void *)arg,&getvol,sizeof(UINT8)))
            {
                printk("<1>ma3_HpVolCtl: copy_to_user fails when getting right out volume of headphone\n");
                if( CanEnableIntrFlag )
                    kmachdep_WriteStatusFlagReg(intr_flag);
                return -EFAULT;
            }
            break;

        case 0x03:/* set the left out of Headphone */
            if(hpvol > 31)
            {
                printk("<1>ma3_HpVolCtl: setting left out volume to %d DB\n",hpvol);
                return -EINVAL;
            }
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_L_REG);
            getvol = (kmachdep_ReadDataReg()&0x80);
            kmachdep_WriteDataReg(getvol|(hpvol&0x1F));
            break;

        case 0x04:/* set the right out of Headphone */
            if(hpvol > 31)
            {
                printk("<1>ma3_HpVolCtl: setting right out volume to %d DB\n",hpvol);
                return -EINVAL;
            }
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_R_REG);
            kmachdep_WriteDataReg(hpvol&0x1F);
            break;

        case 0x05:/* set both out of Headphone */
            if(hpvol > 31)
            {
                printk("<1>ma3_HpVolCtl: setting both out volume to %d DB\n",hpvol);
                return -EINVAL;
            }
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_R_REG);
            kmachdep_WriteDataReg(hpvol&0x1F);

            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_L_REG);
            getvol = (kmachdep_ReadDataReg()&0x80);
            kmachdep_WriteDataReg(getvol|(hpvol&0x1F));
            break;

        case 0x06:/* enable mono */
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_L_REG);
            getvol = (kmachdep_ReadDataReg()&0x1f);
            kmachdep_WriteDataReg(getvol|0x80);
            break;

        case 0x07:/* disable mono */
            kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
            kmachdep_WriteDataReg(0x00);
            kmachdep_WriteStatusFlagReg(MA_ANALOG_HPVOL_L_REG);
            getvol = (kmachdep_ReadDataReg()&0x1f);
            kmachdep_WriteDataReg(getvol|0x00);
            break;

        default:
            return -EINVAL;
    }
    if( CanEnableIntrFlag )
        kmachdep_WriteStatusFlagReg(intr_flag);
    midi_debug(1, "ma3_HpVolCtl returns with success\n");
    return MIDI_SUCCESS;
}


/*******************************************************************************
  * Function name:  ma3_GetEqVol
  * Arguments:  ULONG arg: it's a pointer to a integer
  * Return:       0 stands for success, or signals an error
  * Comments:   be used to get the current setting of EQ volume
  *******************************************************************************/
static INT32 ma3_GetEqVol(ULONG arg)
{
    UINT8 getvol;
    UINT8 intr_flag;
    UINT32 tmp;

    midi_debug(1, "entering ma3_GetEqVol\n");

    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        printk("<1>ma3_GetEqVol: copy_from_user fails\n");
        return -EFAULT;
    }

#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_GetEqVol: arg = 0x%x\n",tmp);
#endif

//  return 0;

    /* get the old statue of interrupt enable/disable */
    intr_flag = ((tmp&0xf0)>>8);

    kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
    kmachdep_WriteDataReg(0x00);
    kmachdep_WriteStatusFlagReg(MA_ANALOG_EQVOL_REG);
    getvol = (kmachdep_ReadDataReg()&0x1f);

    if(copy_to_user((void *)arg,&getvol,sizeof(UINT8)))
    {
        printk("<1>ma3_GetEqVol: copy_to_user fails when getting the EQ volume\n");
        if( CanEnableIntrFlag )
            kmachdep_WriteStatusFlagReg(intr_flag);
        return -EFAULT;
    }

    /* restore the interrupt status */
    if( CanEnableIntrFlag )
        kmachdep_WriteStatusFlagReg(intr_flag);
    midi_debug(1, "ma3_GetEqVol returns with success\n");
    return MIDI_SUCCESS;
}

/*******************************************************************************
  * Function name:  ma3_SetEqVol
  * Arguments:  ULONG arg:
  * Return:       0 stands for success, or signals an error
  * Comments:   be used to set EQ volume
  *******************************************************************************/
static INT32 ma3_SetEqVol(ULONG arg)
{

    UINT8 intr_flag;
    UINT8 eqvol ;
    UINT32 tmp;
    
    midi_debug(1, "entering ma3_SetEqVol\n");
    
    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        printk("<1>ma3_SetEqVol: copy_from_user fails\n");
        return -EFAULT;
    }
    eqvol = (UINT8)tmp;
   
#ifdef MIDI_DEBUG_FLAG
    printk("<1>ma3_SetEqVol: eqvol = 0x%x\n",eqvol);
#endif


    if( eqvol > 31)
    {
        printk("<1>ma3_SetEqVol: setting EQ volume to %d DB\n",eqvol);
        return -EINVAL;
    }

    /* get the old statue of interrupt enable/disable */
    intr_flag = ((arg&0xf0)>>8);
    
    kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
    kmachdep_WriteDataReg(0x00);
    kmachdep_WriteStatusFlagReg(MA_ANALOG_EQVOL_REG);
    kmachdep_WriteDataReg(eqvol&0x1F);

    /* restore the interrupt status */
    if( CanEnableIntrFlag )
        kmachdep_WriteStatusFlagReg(intr_flag);
    midi_debug(1, "ma3_SetEqVol returns with success\n");
    return MIDI_SUCCESS;
}

/*******************************************************************************
  * Function name:  ma3_ClrIntrStatue
  * Arguments:  ULONG arg:
  * Return:       0 stands for success, or signals an error
  * Comments:   be used to clear the intrrupt statue register
  *******************************************************************************/
static INT32 ma3_ClrIntrStatue(ULONG arg)
{

    UINT8 intr_num ;
    UINT32 tmp;
    
    midi_debug(1, "entering ma3_ClrIntrStatue\n");
    
    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        printk("<1>ma3_ClrIntrStatue: copy_from_user fails\n");
        return -EFAULT;
    }
    intr_num = (UINT8)tmp;
    
    kmachdep_WriteStatusFlagReg(MA_INTERRUPT_FLAG_REG);
    kmachdep_WriteDataReg(intr_num);
    return MIDI_SUCCESS;
}

/*******************************************************************************
  * Function name:  ma3_BSetRegWr
  * Arguments:  ULONG arg:
  * Return:       0 stands for success, or signals an error
  * Comments:   be used to configure basic setting register
  *******************************************************************************/
static INT32 ma3_BSetRegWr(ULONG arg)
{

    UINT8 data,intr_flag ;
    UINT32 tmp;
    
    midi_debug(1, "entering ma3_BSetRegWr\n");
    
    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        printk("<1>ma3_BSetRegWr: copy_from_user fails\n");
        return -EFAULT;
    }
    data = (UINT8)tmp;
    intr_flag = (UINT8  )(tmp>>8);

    kmachdep_WriteStatusFlagReg(MA_BASIC_SETTING_REG);
    kmachdep_WriteDataReg(data);
    kmachdep_Wait(300);
    kmachdep_WriteDataReg(0x00);
				 
   if(intr_flag == 0x80)
        if( CanEnableIntrFlag )
            kmachdep_WriteStatusFlagReg(intr_flag);
    
    return MIDI_SUCCESS;
}

/**********************************************************************************
 * Function Name: ma3_ChipReset
 * Arguments: ULONG arg:
 * Return:    0 stands for success, or signals an error
 
 * Comments:  be used to reset ma3 chipset
 * *******************************************************************************/
static INT32 ma3_ChipReset(ULONG arg)
{
	(void)arg;

    //printk("<1>entering ma3_ChipReset.\n");
    /* set RST bit of REG_ID #4 basic setting register to '1' */
    kmachdep_WriteStatusFlagReg( MA_BASIC_SETTING_REG );
    kmachdep_WriteDataReg( 0x80 );
    
    /* wait at leaset 300ns */
    udelay( 1 );
    /* set RST bit of REG_ID #4 basic setting register to '0' */
    kmachdep_WriteDataReg( 0x00 );
    
    SmwInitFlag = 0;

    kmachdep_WriteStatusFlagReg(0x80);
	midicomm_clear_eventbuf();

	
/*
    if( MixerStatueFlag == 1 )
    {
        shutdown_mixer(MIDI_DEVICE);
        MixerStatueFlag = 0; // powerdown  of mixer
    }
*/  
   	return MIDI_SUCCESS;
}    

/*******************************************************************************
 * Function name:  ma3_IntrMngInIntr
 * Arguments:  ULONG arg
 * Return   :  0 stands for success, or signals an error
 * Comments:  be used to control IRQ bit of status flag register. This I/O command
 *            is used by MaDevDrv_IntHandler()
 * *******************************************************************************/
static INT32 ma3_IntrMngInIntr(ULONG arg)
{
    (void)arg;
//    printk("<1>entering ma3_IntrMngInIntr.\n");
    CanEnableIntrFlag = 1;
    
    kmachdep_WriteStatusFlagReg(0x80);
    return MIDI_SUCCESS;
}
/*******************************************************************************
 *  Function name: ma3_CheckPwrState
 *  Arguments:  ULONG arg
 *  Return   :  0 stands for success, or signals an error Return:
 *  Comments :  used to check powermanagement state
 *  *******************************************************************************/	    
static INT32 ma3_CheckPwrState(ULONG arg)
{
    //printk("<1>entering ma3_CheckPwrState: power state = %d.\n",PwrMngSTATE);
    if(copy_to_user((void *)arg,(void *)&PwrMngSTATE,1))
    {
        printk("<1>ma3_CheckPwrState: copy_to_user fails.\n");
        return -EFAULT;
    }

    if(PwrMngSTATE == POWER_RESUMED)
        PwrMngSTATE = POWER_NORMAL;

    return 0;

}	
/*******************************************************************************
  * Function name:  midi_init_gpio
  * Arguments:  void
  * Return:     void
  * Comments:   be used to init gpio33, gpio78, gpio15 
  *******************************************************************************/
static void midi_init_gpio(void)
{
    midi_debug(1,"entering midi_init_gpio\n");
    
#if 1 
    /* init GPIO33 as out, alternate function nCS<5>, low active */
//  GPSR1 = 0x00000002;
    set_GPIO_mode(GPIO_MIDI_CS | GPIO_ALT_FN_2_OUT);
    	
#ifdef VLIO   
    /* Set GPIO 49 as nPWE */  
    set_GPIO_mode(GPIO_MIDI_NPWE | GPIO_ALT_FN_2_OUT);
 
    /* set GPIO 18 as RDY */
    set_GPIO_mode(GPIO_MIDI_RDY | GPIO_ALT_FN_1_IN);

    MSC2 = (MSC2|(0x000C0000));   /* for VLIO */
#else
    MSC2 = (MSC2|(0x00090000));  /* for SRAM */
#endif

    /* disable GPIO9 */
//    OSCC = ( ( OSCC&0xFFFFFFF7 ) | 0x00000008 ); 
	kernel_set_oscc_13m_clock();
	
    /* init GPIO78 as out for reset ,low active */
//    set_GPIO_mode(MIDI_RESET_GPIO | GPIO_OUT);
//    GPSR( MIDI_RESET_GPIO ) = GPIO_bit(MIDI_RESET_GPIO);
//    GPSR2|= 0x00004000;
    
//    set_GPIO_mode(119 | GPIO_OUT);
//    GPDR3|= 0x00800000;
//    GPCR3|= 0x00800000;
    
   
    /* init GPIO15 as IRQ input, low active */
    set_GPIO_mode(GPIO_MIDI_IRQ | GPIO_IN);
    set_GPIO_IRQ_edge(GPIO_MIDI_IRQ,GPIO_FALLING_EDGE);

    midi_debug(1,"midi_init_gpio returns with success\n");
#else
    printk("<1>GPDR3 = 0x%x,GPCR3=0x%x\n",GPSR3,GPCR3);
//    GPDR3|= 0x00800000;
//    GPCR3|= 0x00800000;
//  printk("<1>GPDR3 = 0x%x,GPCR3=0x%x\n",GPSR3,GPCR3);
    printk("<1>before setting: GAFR1_L = 0x%x\n",GAFR1_L);

    GAFR1_L = ((GAFR1_L &0xFFFFFFF3) | 0x00000008);
    printk("<1>GAFR1_L = 0x%x\n",GAFR1_L);
#ifdef VLIO   
    /* Set GPIO 49 as nPWE */   
    GAFR1_U = ((GAFR1_U&0xFFFFFFF3) | 0x00000008); 
    /* set GPIO 18 as RDY */
    GAFR0_U = ((GAFR0_U&0xFFFFFFCF) | 0x00000010);
    
    printk("<1>Before setting:MSC2 = 0x%x\n",MSC2);
    MSC2 = (MSC2|(0x000C0000));   /* for VLIO */
    printk("<1>MSC2 = 0x%x\n",MSC2);
#else
    printk("<1>Before setting:MSC2 = 0x%x\n",MSC2);
    MSC2 = (MSC2|(0x00090000));  /* for SRAM */
    printk("<1>MSC2 = 0x%x\n",MSC2);
#endif
    printk("<1>GAFR0_L = 0x%x\n",GAFR0_L);
    
    printk("<1>before setting:OSCC = 0x%x\n",OSCC);
    
    OSCC = ((OSCC&0xFFFFFFF7    ) |0x00000008); 
    printk("<1>OSCC = 0x%x\n",OSCC);
    

   // printk("<1>GPSR2 = 0x%x\n",GPSR2);    
    GPSR2|= 0x00004000;
    //printk("<1>GPSR2 = 0x%x\n",GPSR2);
    /* init GPIO15 as IRQ input, low active */
    set_GPIO_mode(15|GPIO_IN);
    set_GPIO_IRQ_edge(15,GPIO_FALLING_EDGE);
#endif  
    
}
/*******************************************************************************
  * Function name:  midi_intr_handler
  * Arguments:  int irq
  *             void *dev_id
  *             struct pt_regs *regs
  *             all these arguments aren't used at present
  * Return:     none
  * Comments:   interrupt handler
  *****************************************************************************/
static void midi_intr_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    UINT8   read_data,tmp;
#if 0
    SINT32  receive_data;
    UINT8   packet[3];
#endif

    midi_debug(1, "entering midi_intr_handler\n");

    /* read the interrupt statue */
    kmachdep_WriteStatusFlagReg(MA_INTERRUPT_FLAG_REG);
    read_data = kmachdep_ReadDataReg();
//    printk("<1>interrupt state flag is 0x%x\n",read_data);
    
    /* clear all enabled interrupt statues except for EFIFO */
    tmp = (read_data&0x7f);
    kmachdep_WriteStatusFlagReg(MA_INTERRUPT_FLAG_REG);
    kmachdep_WriteDataReg(tmp);
     
     // this is a workaround for occurrence interrupt before ma3 power on
	if( SmwInitFlag == 0 )
    {
		kmachdep_WriteStatusFlagReg(MA_INTERRUPT_FLAG_REG);
        kmachdep_WriteDataReg(read_data);
		//printk("w20691::::############# ma3 hasn't power on \n");	
		return;
	}

    	
    /* Write interrupt status data into the buffer of midicomm, if failure,
       enable the interrupt */
	if(FALSE == midicomm_intflag_write(read_data))
    {
        //printk("<1>midi_intr_handler: midicomm_intflag_write fails\n");
        //kmachdep_WriteStatusFlagReg(0x80);
    }
    CanEnableIntrFlag = 0;

}
/********************************************************************************
 * Function Name: ma3_SetUseCount
 * Arguments: ULONG arg:
 * Result:   0 for success
 * Comments: it's for debugging device driver,not used currently.
 * *****************************************************************************/
static INT32 ma3_SetUseCount(ULONG arg)
{
    (void)arg;
    return 0;
}
/********************************************************************************
 * Function Name: ma3_IoTest
 * Arguments: ULONG arg:
 * Result:   0 for success,or else for error
 * Comments: it's for debugging device driver
 * *****************************************************************************/
static INT32 ma3_IoTest(ULONG arg)
{
    UINT8 data;
    UINT32 tmp;
    
    midi_debug(1, "entering ma3_IoTest\n");
    
    if(copy_from_user(&tmp,(void *)arg,sizeof(tmp)))
    {
        midi_debug(-1,"ma3_IoTest: copy_from_user fails\n");
        return -EFAULT;
    }

    
    data = (UINT8)ma3_ReceiveData( tmp, 2, 0x80 );
    printk("<1> set value of register %d is 0x%x\n",tmp,data);

    if(copy_to_user((void *)arg,(void *)&data,1))
    {
        midi_debug(-1,"copy_to_user fails\n");
        return -EFAULT;
    }


    return 0;
    
}

#ifdef CONFIG_PM
/**************************************************************************************
 * function name: midi_PowerManagement_Cb
 * arguments: struct pm_dev *pm_dev, 
 *            pm_request_t req, 
 *            void *data
 * return : 0 for success
 * comments: power management callback function
 * ************************************************************************************/
static int midi_PowerManagement_Cb(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
    switch(req)
    {
        case PM_SUSPEND:
            midi_PowerManagement_Suspend();
	    break;
	case PM_RESUME:
	    midi_PowerManagement_Resume();
	    break;
	default:
	    printk("<1> mididrv: invalid PM request.\n");
	    break;
    }
	
    return 0;
}
/**************************************************************************************
 * function name: midi_PowerManagement_Suspend
 * arguments: void 
 * return : 0 for success
 * comments: power management callback function - suspend
 * ************************************************************************************/
static int midi_PowerManagement_Suspend(void)
{ 
    int ret = 0 ;
    //printk("<1>w20691::::entering midi_PowerManagement_Suspend.\n");
#ifdef HWMIDI_MIXER
    // if mixer have been opened by app through midi_open() calling,
    // shutdown_mixer must be called here
    if(MixerStatueFlag == 1)
        shutdown_mixer(MIDI_DEVICE);
#endif
    // if ma3chip has been powered on, shutdown it here,
    // or other nothing is done
    if(Ma3ChipPowerOn == 1)
    {
        ret = ma3_DevicePwrMng(2, 0); 
        kernel_clr_oscc_13m_clock();
        
    }
    return ret;
}
/**************************************************************************************
 * function name: midi_PowerManagement_Resume
 * arguments: void 
 * return : 0 for success
 * comments: power management callback function - resume
 * ************************************************************************************/
static int midi_PowerManagement_Resume(void)
{
    int ret = 0 ;
    printk("<1>w20691::::entering midi_PowerManagement_Resume.\n");
    //PwrMngSTATE = POWER_RESUMED;
    	
	// if ma3chip has been powered on before sleeping, we must resume it here
    if( Ma3ChipPowerOn == 1 )
    {
        midi_init_gpio();

        ret = ma3_DevicePwrMng(1, 0);
        ma3_ChipReset(0);
        PwrMngSTATE = POWER_RESUMED;
    }
#ifdef HWMIDI_MIXER
    // if mixer have been opened by app through midi_open() calling
    // before power suspending, poweron must be called here
    if(MixerStatueFlag == 1) 
        poweron_mixer(MIDI_DEVICE);
#endif
	return ret;
}
#endif
