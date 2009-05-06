/****************************************************************************
 *                                                                          *
 *      Copyright (C) 2001-2003 YAMAHA CORPORATION. All rights reserved.    *
 *                                                                          *
 *      Module      : mamachdepk.c                                          *
 *                                                                          *
 *      Description : machine dependent part on kernel for MA SMW           *
 *                                                                          *
 *      Version     : 1.3.15.8  2003.09.29                                  *
 *                                                                          *
 ****************************************************************************/

#include <asm/io.h>
#include <linux/delay.h>
#include "mamachdepk.h"
#include "madefs.h"


extern void machdep_IntHandler( void );
extern unsigned long global_ioaddr ;

/****************************************************************************
 *  kmachdep_Wait
 *
 *  Function:
 *          Waiting time.
 *  Arguments:
 *          wait_time   waiting time [us]
 *  Return:
 *          None
 *
 ****************************************************************************/
void kmachdep_Wait( UINT32 wait_time )
{
#if 1
    if(wait_time >=1000000)
    {
        mdelay(wait_time/1000000+1);
        return;
    }
    if((wait_time%1000) == 0)
        udelay(wait_time/1000);
    else udelay((wait_time/1000) + 1);
#else
    int delaytime = (int)(uSec*3.25);
    
    do
    {
        OSCR = 0;
    }while(OSCR <100);
    
    while(OSCR  <  delaytime);

#endif
}
/****************************************************************************
 *  kmachdep_WriteStatusFlagReg
 *
 *  Function:
 *          Write a byte data to the status flag register.
 *  Arguments:
 *          data    byte data for write
 *  Return:
 *          None
 *
 ****************************************************************************/
void kmachdep_WriteStatusFlagReg( UINT8 data )
{

    MA3_WRB_STATUSREG( global_ioaddr ,data);

    udelay( 1 );
}
/****************************************************************************
 *  kmachdep_ReadStausFlagReg
 *
 *  Function:
 *          Read a byte data from the status flag register.
 *  Arguments:
 *          data    byte data for write
 *  Return:
 *          status register value
 *
 ****************************************************************************/
UINT8 kmachdep_ReadStatusFlagReg( void )
{
    UINT8   data;
    
    data = MA3_RDB_STATUSREG(global_ioaddr);

    udelay( 1 );

    return data;
}
/****************************************************************************
 *  kmachdep_WriteDataReg
 *
 *  Function:
 *          Write a byte data to the MA-3 write data register.
 *  Arguments:
            data    byte data for write
 *  Return:
 *          none
 *
 ****************************************************************************/
void kmachdep_WriteDataReg( UINT8 data )
{

    MA3_WRB_DATAREG( global_ioaddr,data);

    udelay( 1 );
}
/****************************************************************************
 * kmachdep_ReadDataReg
 *
 *  Function:
 *          Read a byte data from the MA-3 read data register.
 *  Arguments:
            None
 *  Return:
 *          read data
 *
 ****************************************************************************/
UINT8 kmachdep_ReadDataReg( void )
{
    UINT8   data;
    data = MA3_RDB_DATAREG(global_ioaddr);
    udelay( 1 );

    return data;
}
/****************************************************************************
 *  kmachdep_CheckStauts
 *
 *  Function:
 *          Check status of the MA-3.
 *  Arguments:
            flag    check flag
 *  Return:
 *          0       success
 *          -1      time out
 *
 ****************************************************************************/
SINT32 kmachdep_CheckStatus( UINT8 flag )
{
    UINT8   read_data;
#if 0
    do
    {
        read_data = kmachdep_ReadStatusFlagReg();

        if ( i > (int)(MA_STATUS_TIMEOUT * 3.25) )
        {
            /* timeout: stop timer */
            return MASMW_ERROR;
        }
        i++;
    }
    while ( ( read_data & flag ) != 0 );
#else
    read_data = kmachdep_ReadStatusFlagReg();
    if(( read_data & flag ) != 0)
    {
        mdelay(2);
        read_data = kmachdep_ReadStatusFlagReg();
        if(( read_data & flag ) != 0)
            return MASMW_ERROR;
    }
#endif
    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_WaitValidData
 *
 *  Function:
 *          Check status of the MA-3.
 *  Arguments:
            flag    check flag
 *  Return:
 *          0       success
 *          -1      time out
 *
 ****************************************************************************/
static SINT32 kmachdep_WaitValidData( UINT8 flag )
{
    UINT8   read_data;
#if 0
    do
    {
        read_data = kmachdep_ReadStatusFlagReg();

        if ( i > (int)(MA_STATUS_TIMEOUT * 3.25) )
        {
            /* timeout: stop timer */
            return MASMW_ERROR;
        }
        i++;
    }
    while ( ( read_data & flag ) == 0 );
#else
    read_data = kmachdep_ReadStatusFlagReg();
#ifdef MIDI_DEBUG_FLAG
    printk("<1>kmachdep_WaitValidData:read_data= 0x%x\n",read_data);
#endif
    if(( read_data & flag ) == 0)
    {
        mdelay(2);
        read_data = kmachdep_ReadStatusFlagReg();
        if(( read_data & flag ) == 0)
        {
            printk("<1>kmachdep_WaitValidData returns with failure\n");
            return MASMW_ERROR;
        }
    }
#endif

    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_CheckDelayedFifoEmpty
 *
 *  Function:
 *          Check Delayed FIFO Empty Flag
 *  Arguments:
            
 *  Return:
 *          0       not Empty
 *          1       Empty
 *
 ****************************************************************************/
SINT32  kmachdep_CheckDelayedFifoEmpty( void )
{
    UINT32 flag;

    flag = 0;
    if( kmachdep_ReadStatusFlagReg()&MA_EMP_DW ) flag |= 0x01;
    if( kmachdep_ReadStatusFlagReg()&MA_EMP_DW ) flag |= 0x02;

    if( flag==0 ) return 0;
    else if( flag==0x03 ) return 1;

    flag = 0;
    if( kmachdep_ReadStatusFlagReg()&MA_EMP_DW ) flag |= 0x01;
    if( kmachdep_ReadStatusFlagReg()&MA_EMP_DW ) flag |= 0x02;
    if( flag==0 ) return 0;
    else return 1;
}
/****************************************************************************
 *  kmachdep_WaitImmediateFifoEmpty
 *
 *  Function:
 *          Wait untill Immediate FIFO Empty
 *  Arguments:
            
 *  Return:
 *          0       success
 *          -1      time out
 *
 ****************************************************************************/
static SINT32 kmachdep_WaitImmediateFifoEmpty( void )
{
    UINT8 read_data;
    
#if 0
    do
    {
        read_data = kmachdep_ReadStatusFlagReg();

        if ( i > (int)(MA_STATUS_TIMEOUT * 3.25) )
        {
            /* timeout: stop timer */
            return MASMW_ERROR;
        }
        i++;
    } while( (read_data&MA_EMP_W)==0 );
#else
    read_data = kmachdep_ReadStatusFlagReg();
#ifdef MIDI_DEBUG_FLAG
    printk("<1>kmachdep_WaitImmediateFifoEmpty: read_data =0x%x\n",read_data);
#endif
    if(( read_data & MA_EMP_W ) == 0)
    {
        mdelay(2);
        read_data = kmachdep_ReadStatusFlagReg();
        if(( read_data & MA_EMP_W ) == 0)
        {
            printk("<1>kmachdep_WaitImmediateFifoEmpty fails: read_data =0x%x\n",read_data);
            return MASMW_ERROR;
        }
    }
#endif
    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_SendDelayedPacket
 *
 *  Description:
 *          Write delayed packets to the REG_ID #1 delayed write register.
 *  Argument:
 *          ptr         pointer to the delayed packets
 *          size        size of the delayed packets
 *  Return:
 *          MASMW_SUCCESS(0)
 *
 ****************************************************************************/
SINT32 kmachdep_SendDelayedPacket
(
    const UINT8 * ptr,          /* pointer to packets */
    UINT32        size          /* byte size of packets */
)
{
    kmachdep_WriteStatusFlagReg( MA_DELAYED_WRITE_REG );
    
#if 1
    while ( (size--) != 0 )
#else
    for ( i = 0; i < size; i++ )
#endif
    {
        kmachdep_WriteDataReg( *ptr++ );
    }

    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_SendDirectPacket
 *
 *  Description:
 *          Write direct packets to the REG_ID #2 direct write register.
 *  Argument:
 *          ptr         pointer to the direct packets
 *          size        size of the direct packets
 *  Return:
 *          MASMW_SUCCESS(0)
 *
 ****************************************************************************/
SINT32 kmachdep_SendDirectPacket
(
    const UINT8 * ptr,                  /* pointer to packets */
    UINT32 size                         /* byte size of packets */
)
{
    UINT32  i, j;
    SINT32  result;

    kmachdep_WriteStatusFlagReg( MA_IMMEDIATE_WRITE_REG );

#if 1
    i = size/64;
    while ( (i--) != 0 )
#else
    for ( i = 0; i < (size/64); i++ )
#endif
    {
        result = kmachdep_WaitImmediateFifoEmpty();
        if ( result < MASMW_SUCCESS )
        {
            return result;
        }
#if 1
        j = 64;
        while ( (j--) != 0 )
#else
        for ( j = 0; j < 64; j++ )
#endif
        {
            kmachdep_WriteDataReg( *ptr++ );
        }
    }

    if ( (size %64) != 0 )
    {
        result = kmachdep_WaitImmediateFifoEmpty();
        if ( result < MASMW_SUCCESS )
        {
            return result;
        }
        
#if 1
        j = size%64;
        while ( (j--) != 0 )
#else
        for ( j = 0; j < (size%64); j++ )
#endif
        {
            kmachdep_WriteDataReg( *ptr++ );
        }
    }

    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_ReceiveData
 *
 *  Description:
 *          Read byte data of register or memory.
 *  Argument:
 *          address         address of the register for read
 *          buffer_address  address of the return buffer
 *  Return:
 *          MASMW_SUCCESS(0)
 *
 ****************************************************************************/
SINT32 kmachdep_ReceiveData
(
    UINT32  address,            /* address of register */
    UINT8   buffer_address      /* address of read buffer */
)
{
    UINT8   i;
    UINT8   packet_buffer[4];
    UINT8   valid_rx;
    UINT8   read_data;
    UINT8   count;
    SINT32  result;

    kmachdep_WriteStatusFlagReg( MA_IMMEDIATE_READ_REG );
    
    
    if      ( address < 0x0080 )    count = 0;
    else if ( address < 0x4000 )    count = 1;
    else                            count = 2;

    for ( i = 0; i < count; i++ )
    {
        packet_buffer[i] = (UINT8)( (address >> (7*i)) & 0x7F );
    }
    packet_buffer[i] = (UINT8)( (address >> (7*i)) | 0x80 );
    
    count++;
    packet_buffer[count] = (UINT8)( 0x80 | buffer_address );

    result = kmachdep_WaitImmediateFifoEmpty();
    if ( result < MASMW_SUCCESS )
    {
        return result;
    }

    for ( i = 0; i <= count; i++ )
    {
        kmachdep_WriteDataReg( packet_buffer[i] );
    }

    valid_rx = (UINT8)(1 << (MA_VALID_RX + buffer_address));
    
    result = kmachdep_WaitValidData( valid_rx );
    if ( result < MASMW_SUCCESS )
    {
        printk("<1>receiveData fails\n");
        return result;
    }

    kmachdep_WriteStatusFlagReg( (UINT8)(valid_rx | (buffer_address + 1)) );
    
    read_data = kmachdep_ReadDataReg();

    return (SINT32)read_data;
}
/****************************************************************************
 *  kmachdep_SendDirectRamData
 *
 *  Description:
 *          write data to internal RAM.
 *  Arguments:
 *          address         internal RAM address
 *          data_type       type of data
 *          data_ptr        pointer ot the write byte data to internal RAM
 *          data_size       size of write byte data to internal RAM
 *  Return:
 *          MASMW_SUCCESS(0)
 *
 ****************************************************************************/
SINT32 kmachdep_SendDirectRamData
(
    UINT32  address,                    /* address of internal ram */
    UINT8   data_type,                  /* type of data */
    const UINT8 * data_ptr,             /* pointer to data */
    UINT32  data_size                   /* size of data */
)
{
    UINT32  i, j;
    UINT8   packet_buffer[3+2];
    UINT32  count;
    UINT32  write_size;
    UINT8   write_data;
    UINT8   temp = 0;
    SINT32  result;

    if ( data_size == 0 ) return MASMW_SUCCESS;

    kmachdep_WriteStatusFlagReg( MA_IMMEDIATE_WRITE_REG );
    
    /* address section  WUHX*/
    packet_buffer[0] = (UINT8)( (address >>  0 ) & 0x7F );
    packet_buffer[1] = (UINT8)( (address >>  7 ) & 0x7F );
    packet_buffer[2] = (UINT8)( (address >> 14 ) | 0x80 );

    /* I don't know the meaning of data_type ????? WUHX*/
    if ( data_type == 0 )
    {
        write_size = data_size;
    }
    else
    {
        write_size = (data_size / 8) * 7;
        if ( data_size % 8 != 0 )
            write_size = (UINT32)( write_size + (data_size % 8 - 1) );
    }

    if  ( write_size < 0x0080 )
    {
        packet_buffer[3] = (UINT8)( (write_size >> 0) | 0x80 );
        count = 4;
    }
    else
    {
        packet_buffer[3] = (UINT8)( (write_size >> 0) & 0x7F );
        packet_buffer[4] = (UINT8)( (write_size >> 7) | 0x80 );
        count = 5;
    }

    result = kmachdep_WaitImmediateFifoEmpty();
    if ( result < MASMW_SUCCESS )
    {
        return result;
    }
    
    for ( i = 0; i < count; i++ )
    {
        kmachdep_WriteDataReg( packet_buffer[i] );
    }

    if ( data_type == 0 )
    {
#if 1
        i = data_size/64;
        while ( (i--) != 0 )
#else
        for ( i = 0; i < data_size/64; i++ )
#endif
        {
            result = kmachdep_WaitImmediateFifoEmpty();
            if ( result < MASMW_SUCCESS )
            {
                return result;
            }
#if 1
            j = 64;
            while ( (j--) != 0 )
#else
            for ( j = 0; j < 64; j++ )
#endif
            {
                kmachdep_WriteDataReg( *data_ptr++ );
            }
        }

        if ( (data_size%64) != 0 )
        {
            result = kmachdep_WaitImmediateFifoEmpty();
            if ( result < MASMW_SUCCESS )
            {
                return result;
            }
#if 1
            j = data_size%64;
            while ( (j--) != 0 )
#else
            for ( j = 0; j < (data_size%64); j++ )
#endif
            {
                kmachdep_WriteDataReg( *data_ptr++ );
            }
        }
    }
    else
    {
        for ( i = 0; i < data_size; i++ )
        {
            if ( (i & 0xFFFFFFC0) == 0 )
            {
                result = kmachdep_WaitImmediateFifoEmpty();
                if ( result < MASMW_SUCCESS )
                {
                    return result;
                }
            }

            if ( ( i % 8 ) == 0 )
            {
                temp = *data_ptr++;
            }
            else
            {
                write_data = *data_ptr++;
                write_data |= (UINT8)( ( temp << (i % 8) ) & 0x80 );

                kmachdep_WriteDataReg( write_data );
            }
        }
    }

    return MASMW_SUCCESS;
}
/****************************************************************************
 *  machdep_SendDirectRamVal
 *
 *  Description:
 *          write data to internal RAM.
 *  Arguments:
 *          address     internal RAM address
 *          size        size of write byte data to internal RAM
 *          val         write byte data
 *  Return:
 *          0           success
 *
 ****************************************************************************/
SINT32 kmachdep_SendDirectRamVal
(
    UINT32  address,                    /* address of internal ram */
    UINT32  data_size,                  /* size of data */
    UINT8   val                         /* write value */
)
{
    UINT32  i, j;                       /* loop counter */
    UINT32  count;                      /* count of packet */
    SINT32  result;                     /* result of function */
    UINT8   packet_buffer[3+2];         /* packet buffer */

    if ( data_size == 0 ) return MASMW_SUCCESS;

    kmachdep_WriteStatusFlagReg( MA_IMMEDIATE_WRITE_REG );
    

    packet_buffer[0] = (UINT8)( (address >>  0 ) & 0x7F );
    packet_buffer[1] = (UINT8)( (address >>  7 ) & 0x7F );
    packet_buffer[2] = (UINT8)( (address >> 14 ) | 0x80 );

    if  ( data_size < 0x0080 )
    {
        packet_buffer[3] = (UINT8)( (data_size >> 0) | 0x80 );
        count = 4;
    }
    else
    {
        packet_buffer[3] = (UINT8)( (data_size >> 0) & 0x7F );
        packet_buffer[4] = (UINT8)( (data_size >> 7) | 0x80 );
        count = 5;
    }


    result = kmachdep_WaitImmediateFifoEmpty();
    if ( result < MASMW_SUCCESS )
    {
        return result;
    }

    for ( i = 0; i < count; i++ )
    {
        kmachdep_WriteDataReg( packet_buffer[i] );
    }

#if 1
    i = data_size/64;
    while ( (i--) != 0 )
#else
    for ( i = 0; i < (data_size/64); i++ )
#endif
    {
        result = kmachdep_WaitImmediateFifoEmpty();
        if ( result < MASMW_SUCCESS )
        {
            return result;
        }

#if 1
        j = 64;
        while ( (j--) != 0 )
#else
        for ( j = 0; j < 64; j++ )
#endif
        {
            kmachdep_WriteDataReg( val );
        }
    }

    if ( (data_size%64) != 0 )
    {
        result = kmachdep_WaitImmediateFifoEmpty();
        if ( result < MASMW_SUCCESS )
        {
            return result;
        }
#if 1
        j = data_size%64;
        while ( (j--) != 0 )
#else
        for ( j = 0; j < (data_size%64); j++ )
#endif
        {
            kmachdep_WriteDataReg( val );
        }
    }

    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_Open
 *
 *  Description:
 *          processing for open hardware resources.
 *  Argument:
 *          None
 *  Return:
 *          0       success
 *          -1      error
 *
 ****************************************************************************/
SINT32 kmachdep_Open( void )
{
    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_Close
 *
 *  Description:
 *          processing for close hardware resources.
 *  Argument:
 *          None
 *  Return:
 *          0       success
 *          -1      error
 *
 ****************************************************************************/
SINT32 kmachdep_Close( void )
{
    return MASMW_SUCCESS;
}
/****************************************************************************
 *  kmachdep_IntHandler
 *
 *  Description:
 *          Interrupt handler for hardware interrupts.
 *  Argument:
 *          None
 *  Return:
 *          None
 *
 ****************************************************************************/
void kmachdep_IntHandler( void )
{
//  machdep_IntHandler();
}
