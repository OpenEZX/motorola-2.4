
/*================================================================================
                                                                               
                      Header Name: i2c-adcm2700.h

General Description: Camera module adcm2700 I2C interface head file
 
==================================================================================
                     Motorola Confidential Proprietary
                 Advanced Technology and Software Operations
               (c) Copyright Motorola 1999, All Rights Reserved
 
Revision History:
                            Modification     Tracking
Author                 Date          Number     Description of Changes
----------------   ------------    ----------   -------------------------
wangfei(w20239)      12/15/2003     LIBdd35749    Created   
wangfei(w20239)      02/26/2004     LIBdd81055    New chip id support

==================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef  __I2C_ADCM2700_H__
#define  __I2C_ADCM2700_H__

#define DEBUG

/* Calculating the Module Block Number */
#define BLOCK(a)    ((u8)(((a) >> 7) & 0x7F))     /* register's module block address. */
#define OFFSET(a)   ((u8)(((a) << 1) & 0xFF))     /* register's offset to this block. */
#define OFFSET_R(a) (OFFSET(a)|1)                 /* register's offset ot this block to read*/     

#define BLOCK_SWITCH_CMD        0xFE
#define ADCM2700_I2C_ADDR       0x53
#define I2C_DRIVERID_ADCM2700   I2C_DRIVERID_EXP1
#define ADCM2700_CHIP_ID        0x0060
#define ADCM2700_CHIP_ID_NEW    0x0061
#define REV_ID                  0x0               /* Register definitions in ADCM2700's chip. */


struct adcm2700_data {
    /*
     *  Because the i2c bus is slow, it is often useful to cache the read
     *  information of a chip for some time (for example, 1 or 2 seconds).
     *  It depends of course on the device whether this is really worthwhile
     *  or even sensible.
     */
    struct semaphore update_lock; /* When we are reading lots of information,
                                     another process should not update the
                                     below information */

    char valid;                   /* != 0 if the following fields are valid. */
    int  blockaddr;               /* current using block address.    */
    unsigned long last_updated;   /* In jiffies */
};

#ifdef DEBUG
#define dbg_print(fmt, args...) printk(KERN_INFO "I2C-ADCM2700 in fun:%s "fmt"\n", __FUNCTION__, ##args)
#else
#define dbg_print(fmt, args...) ;
#endif

#endif  /* __I2C_ADCM2700_H__ */

