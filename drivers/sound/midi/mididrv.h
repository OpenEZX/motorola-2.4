#ifndef MIDI_DRV_H_
#define MIDI_DRV_H_





#ifndef UINT8
typedef unsigned char   UINT8;
#endif
#ifndef UINT16
typedef unsigned short  UINT16;
#endif

#ifndef UINT32
typedef unsigned int    UINT32;
#endif

#ifndef SINT8
typedef signed char     SINT8;
#endif
#ifndef SINT16
typedef signed short    SINT16;
#endif
#ifndef SINT32
typedef signed int      SINT32;
#endif
#ifndef INT8
typedef char            INT8;
#endif
#ifndef INT16
typedef short           INT16;
#endif
#ifndef INT32
typedef int             INT32;
#endif

#ifndef ULONG
typedef unsigned long   ULONG;
#endif

#define MAX_BUF_SIZE        1024

/* for debug */
#define MIDI_DEBUG_DISABLE          0
#define MIDI_DEBUG                  1
#define MIDI_DEBUG_CRITICAL_LEVEL   2
#define MIDI_DEBUG_MAJOR_LEVEL      3
#define MIDI_DEBUG_ALL              4
#ifdef MIDI_DEBUG
#define MIDI_DEBUG_DEFUALT_LEVEL    -1
#endif




#ifdef TRUE
#undef TRUE
#endif
#define TRUE        1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE       0


#define MIDI_SUCCESS        0
#define MIDI_FAILURE        -1



#define MA_STATUS_TIMEOUT     (1500)  /* 1.5msec */

#define MIDI_DRV_BUF_SIZE       278


/*  */
#define DELAY_WRHD_LEN      6
#define DIRECT_WRHD_LEN     6
#define RAM_WRHD_LEN        11
#define RAMVAL_WRHD_LEN     11

#define WRCMD_DELAY         1
#define WRCMD_DIRECT        2
#define WRCMD_RAM           3
#define WRCMD_RAMVAL        4


/* I/O command */
#define MA3_DEV_CTL                 0xddffec01
#define MA3_PWR_MNG                 0xddffec02
#define MA3_WAIT_DELAYEDFIOF_EPT    0xddffec03
#define MA3_CHECK_DELAYEDFIOF_EPT   0xddffec04
#define MA3_INT_MNG                 0xddffec05
#define MA3_INIT_REG                0xddffec06
#define MA3_RCV_DATA                0xddffec07
#define MA3_DEBUG                   0xddffec08
#define MA3_HPVOL_CTL               0xddffec09
#define MA3_SET_EQVOL               0xddffec0a
#define MA3_GET_EQVOL               0xddffec0b
#define MA3_SET_USECOUNT            0xddffec0c
#define MA3_IO_TEST                 0xddffec0d
#define MA3_BSET_REG_WR             0xddffec0e
#define MA3_CLR_INTRSTATUS          0xddffec0f
#define MA3_CHIP_RESET              0xddffec10
#define MA3_INTRMNG_IN_INTR         0xddffec11
#define MA3_CHECK_PWR_STATE         0xddffec12
#define MA3_CTL_MASK                0xddffec00

#define MA3_GET_CMD(cmd)            ((cmd)&(0xff))
#define MA3_VRF_CMD(cmd)    (((cmd)&(0xffffff00)) == MA3_CTL_MASK)

#if 0
/* GPIO used by MIDI chipset */
#define MIDI_RESET_GPIO    78
#define MIDI_CS_GPIO       33
#define MIDI_IRQ_GPIO      15
#define MIDI_NPWE_GPIO     49
#define MIDI_RDY_GPIO      18
#endif

#define MIDI_PHY_ADDR      (0x14000000)
#define MIDI_IOMEM_LEN     4  

/* power management state */
#define POWER_RESUMED      1
#define POWER_NORMAL       2

typedef struct DevCtlInfo {
    UINT8   cmd;
    UINT8   param1;
    UINT8   param2;
    UINT8   param3;
    UINT8   intr_flag;
}tDevCtlInfo;


typedef struct RcvDataInfo {
    UINT32  reg_addr; /* which register */
    UINT8   buf_addr; /* which buffer is used to put data */
    UINT8   intr_flag;/* interrupt flag */
}tRcvDataInfo;



typedef struct __midi_ram_buffer
{
    UINT32 ram_size;
    UINT32 ram_addr;


    union
    {
        UINT8 data_type;
        UINT8 ram_val;
    }a;
    UINT8 intr_flag;

}tMidiRamBuf;




#endif /* MIDI_DRV_H_ */

