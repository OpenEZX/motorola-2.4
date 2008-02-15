/* 
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

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/apm_bios.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

#define   GPIO_23_SPI_CLK     0x00800000
#define   GPIO_24_SPI_CE      0x01000000
#define   GPIO_25_SPI_MOSI    0x02000000
#define   GPIO_26_SPI_MISO    0x04000000 

#define   SSP_SEND_PM_ALART_INTERVAL   100                 /* 1 second */
//#include "../../include/asm/arch/time.h"
//#include "asm/arch/irqs.h"

#include "ssp_pcap.h"
//#define  SSP_PCAP_OPERATE_WITH_SPI                                  

static u32 ssp_pcap_operate_pin_with_inverter;                
//#define  SSP_PCAP_OPERATE_DEBUG_INFORNMATION                        

#ifdef _DEBUG_BOARD
#define  SSP_PCAP_DEBUG_BOARD                                       
#endif

static struct timer_list ssp_start_adc_timer;

static struct timer_list ssp_tsi_timer;

//static struct pm_dev *pcap_pm_dev;
static U32 ssp_pcap_status = 0;

static U32 ssp_pcap_registerValue[SSP_PCAP_REGISTER_NUMBER] = {0};

SSP_PCAP_BIT_STATUS SSP_PCAP_get_audio_in_status(void);
SSP_PCAP_STATUS SSP_PCAP_write_data_to_PCAP(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER ssp_pcap_register,U32 ssp_pcap_register_value);
SSP_PCAP_STATUS SSP_PCAP_read_data_from_PCAP(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER ssp_pcap_register,P_U32 p_ssp_pcap_register_value);

SSP_PCAP_STATUS SSP_PCAP_bit_set(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE ssp_pcap_bit ) ;
SSP_PCAP_STATUS SSP_PCAP_bit_clean(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE ssp_pcap_bit ) ;
SSP_PCAP_BIT_STATUS SSP_PCAP_get_bit_from_buffer(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE ssp_pcap_bit ) ;
SSP_PCAP_BIT_STATUS SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE ssp_pcap_bit ) ;
U32 SSP_PCAP_get_register_value_from_buffer(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER ssp_pcap_register ) ;

#ifdef SSP_PCAP_OPERATE_WITH_SPI
static wait_queue_head_t ssp_pcap_send_wait;
static U32 ssp_pcap_readValue;
static void ssp_interrupt_routine(int irq, void *dev_id, struct pt_regs *regs );
void ssp_put_intoData(U32 pcapValue);
#endif

static spinlock_t pcapoperation_lock = SPIN_LOCK_UNLOCKED;
static void ssp_pcap_interrupt_routine(int irq, void *dev_id, struct pt_regs *regs );
//static U32 ssp_pcapRegisterReadOut = 0;

//static void usb_hardware_switch(void);

//static struct timer_list usb_switch_timer;

void ssp_pcap_init(void);
void ssp_pcap_release(void);
void ssp_pcap_clear_int(void);
void ssp_pcap_intoSleepCallBack(void);
void ssp_pcap_wakeUpCallBack(void);

extern int usb_gpio_init(void);
extern int stop_usb(void);
#if 0
extern int stop_ffuart(void);
extern void ffuart_gpio_init(void);
#endif

extern void ezx_ts_dataReadok_interrupt(int irq,void* dev_id, struct pt_regs *regs);        
extern void ezx_ts_touch_interrupt(int irq,void* dev_id, struct pt_regs *regs);    
extern void headjack_change_interrupt_routine(int ch,void *dev_id,struct pt_regs *regs);
extern void mic_change_interrupt_routine(int ch,void *dev_id,struct pt_regs *regs);
extern u8 pxafb_ezx_getLCD_status(void);
extern void cebus_int_hndler(int irq, void *dev_id, struct pt_regs *regs);
/*---------------------------------------------------------------------------
  DESCRIPTION: log system stored in buffer. it can output via FFUART. also you can change a little to other output mode.

   INPUTS: none.
      
       
   OUTPUTS: none.
      
       
   IMPORTANT NOTES: the macro PCAP_LOG_FFUART_OUT is a switch.
                    it is auto-output to FFUART every PCAP_LOG_OUTPUT_TIMER seconds
       
     
---------------------------------------------------------------------------*/
/* record log system by linwq */
//#define PCAP_LOG_FFUART_OUT

#ifdef  PCAP_LOG_FFUART_OUT
static struct timer_list pcap_outlog_timer;

#define PCAP_LOG_OUTPUT_TIMER   60
#define PCAP_LOG_RECORD_LENGTH  50000

unsigned char pcap_log_data[PCAP_LOG_RECORD_LENGTH];

u8* pcap_log_now_address = pcap_log_data;
#define PCAP_LOG_START_ADDRESS     pcap_log_data
#define PCAP_LOG_END_ADDRESS       pcap_log_data + PCAP_LOG_RECORD_LENGTH
u32 pcap_log_len = 0;

void pcap_log_add_pure_data(u8* pData,u32 len)
{
    u32 i;
    if ((pData ==NULL)|| len == 0)
        return;
    for(i = 0;i<len;i++)
    {
        *pcap_log_now_address ++ = pData[i];
        if( PCAP_LOG_END_ADDRESS == pcap_log_now_address )
            pcap_log_now_address = PCAP_LOG_START_ADDRESS; 
        pcap_log_len++;
        if(pcap_log_len >= PCAP_LOG_RECORD_LENGTH) 
            pcap_log_len = PCAP_LOG_RECORD_LENGTH;      
    }
    return;
}

void pcap_log_add_data(u8* pData,u32 len)
{
    u32 i;
    u8 temp[20];
    if ((pData ==NULL)|| len == 0)
        return;
    sprintf(temp,"\n <%d MS>",(int)(OSCR/3686));
    for(i = 0;i<(u32)strlen(temp);i++)
    {
        *pcap_log_now_address ++ = temp[i];
        if( PCAP_LOG_END_ADDRESS == pcap_log_now_address )
            pcap_log_now_address = PCAP_LOG_START_ADDRESS; 
        pcap_log_len++;
        if(pcap_log_len >= PCAP_LOG_RECORD_LENGTH) 
            pcap_log_len = PCAP_LOG_RECORD_LENGTH;      
    }

    for(i = 0;i<len;i++)
    {
        *pcap_log_now_address ++ = pData[i];
        if( PCAP_LOG_END_ADDRESS == pcap_log_now_address )
            pcap_log_now_address = PCAP_LOG_START_ADDRESS; 
        pcap_log_len++;
        if(pcap_log_len >= PCAP_LOG_RECORD_LENGTH) 
            pcap_log_len = PCAP_LOG_RECORD_LENGTH;      
    }
    return;
}

void pcap_log_output_from_ffuart(void)
{
    u32 i; 
    u32 ssp_ffuart_dll,ssp_ffuart_dlh;
    u8 ssp_ffuart_cken = 1,ssp_ffuart_en = 1;
    if(0 == pcap_log_len)
        return;

    /* if cable in NOT UART cable, should return */
    printk("\n *********log out ************* \n <%d jiffies>log:",(int)jiffies);
    __cli();
    if(!(CKEN&0x00000040))
    {
        ssp_ffuart_cken = 0;
        CKEN |= 0x00000040;
        pcap_log_add_data("FFUART CLK not ENABLE!",strlen("FFUART CLK not ENABLE!"));
    }
    
    FFLCR = (FFLCR&0xff)|0x80;
    ssp_ffuart_dll = FFDLL&0xff;
    ssp_ffuart_dlh = FFDLH&0xff;
    FFLCR = FFLCR&0x7f;

    if((0x08 !=ssp_ffuart_dll)||(0x00 != ssp_ffuart_dlh))        
    {
        FFLCR = 0x83;
        FFDLL = 0x08;
        FFDLH = 0;
        FFLCR = 0x03;
    }

    if(!(FFIER&0x00000040))
    {
        ssp_ffuart_en = 0;
        FFIER |= 0x00000040;
        pcap_log_add_data("FFUART model not ENABLE!",strlen("FFUART model not ENABLE!"));
    }

    for(i=0;i<pcap_log_len;i++)
    {
        while((FFLSR &0x40) == 0);
        FFTHR = pcap_log_data[i];
    }
    if(!ssp_ffuart_cken)
    {
        CKEN &= 0xffffffbf;
    }


    if((0x08 !=ssp_ffuart_dll)||(0x00 != ssp_ffuart_dlh))        
    {
        FFLCR = 0x83;
        FFDLL = ssp_ffuart_dll;
        FFDLH = ssp_ffuart_dlh;
        FFLCR = 0x03;
    }
    
    if(!ssp_ffuart_en)
    {
        CKEN &= 0xffffffbf;
    }
    __sti();

    printk("\n ********* log end ************ \n"); 
} 

void pcap_output_log_handler( u32 data)
{ 
     pcap_log_output_from_ffuart();

     pcap_outlog_timer.function = pcap_output_log_handler;
     pcap_outlog_timer.expires = (jiffies + PCAP_LOG_OUTPUT_TIMER * HZ);
     add_timer(&pcap_outlog_timer);
}

void pcap_output_log_init(void)
{
     init_timer(&pcap_outlog_timer);	    	
     pcap_outlog_timer.function = pcap_output_log_handler;
     pcap_outlog_timer.expires = (jiffies + 2*PCAP_LOG_OUTPUT_TIMER * HZ);
     add_timer(&pcap_outlog_timer);
}
/* record log end  */
#else
void pcap_log_add_pure_data(u8* pData,u32 len)
{
    return;
}

void pcap_log_add_data(u8* pData,u32 len)
{
    return;
}
#endif

/*---------------------------------------------------------------------------
  DESCRIPTION: GPIO init for operating PCAP via GIP mode.

   INPUTS: none.
      
       
   OUTPUTS: none.
      
       
   IMPORTANT NOTES: auto-recognized pcap's CE signal. init PCAP's register.
       
     
---------------------------------------------------------------------------*/
#ifndef SSP_PCAP_OPERATE_WITH_SPI 
void ssp_pcap_gpioInit(void)
{
    u32 i;
    U32 tempValue;
    SSCR0 = 0x00000000;
    tempValue = GPDR0;
    /* GPIO27 works as BB_RESET  out PIN */ 
    tempValue = tempValue &0xf87fffff;
    tempValue = tempValue | GPIO_24_SPI_CE | GPIO_23_SPI_CLK | GPIO_25_SPI_MOSI;
    GPDR0 = tempValue;
	
    /* GPIO27 works as BB_RESET  out PIN */ 
    GAFR0_U &= 0xffc03fff;
    GPCR0 = 0x01000000;
    /* test pcap's CE with inverter??? */ 
    ssp_pcap_operate_pin_with_inverter = 0;
    /* deassert SEC_CE  */
    GPCR0 = GPIO_24_SPI_CE;
    for(i=0;i<500;i++);
    
    /* write a data 0x0003ffff to IMR */
    SSP_PCAP_write_data_to_PCAP(1, 0x0003ffff);
    SSP_PCAP_read_data_from_PCAP(1, &tempValue);
    if( 0x0003ffff != tempValue ) 
    {
        ssp_pcap_operate_pin_with_inverter = 1;
        /* deassert SEC_CE  */
        GPSR0 = GPIO_24_SPI_CE;
        for(i=0;i<500;i++);
        SSP_PCAP_write_data_to_PCAP(1, 0x0003ffff);
        SSP_PCAP_read_data_from_PCAP(1, &tempValue);
        if( 0x0003ffff != tempValue ) 
        {
            printk("\n Dalhart can not communicate with PCAP!!!!");
            return;
        }
    }
    SSP_PCAP_write_data_to_PCAP(1, 0x0013ffff);

    for(i=0; i<32 ;i++)
    {
        if(SSP_PCAP_SUCCESS == SSP_PCAP_read_data_from_PCAP(i,&tempValue))
        {
            ssp_pcap_registerValue[i] = tempValue;  
        }
        else
        {
            ssp_pcap_registerValue[i] = 0; 
        }
    }
    return;    
}


void ssp_pcap_gpio23ClkControl(U32 bitValue)
{
    if(bitValue)
    {
        GPSR0 = GPIO_23_SPI_CLK;
    }
    else
    {
        GPCR0 = GPIO_23_SPI_CLK;
    }
}

void ssp_pcap_gpio24FrmControl(U32 bitValue)
{
    if( ssp_pcap_operate_pin_with_inverter )
    {
        if(bitValue)
        {
            GPCR0 = GPIO_24_SPI_CE;
        }
        else
        {
            GPSR0 = GPIO_24_SPI_CE;
        }
    }
    else
    {
         if(bitValue)
         {
             GPSR0 = GPIO_24_SPI_CE;
         }
         else
         {
             GPCR0 = GPIO_24_SPI_CE;
         }
    }
}

void ssp_pcap_gpio25TxdControl(U32 bitValue)
{
    if(bitValue)
    {
        GPSR0 = GPIO_25_SPI_MOSI;
    }
    else
    {
        GPCR0 = GPIO_25_SPI_MOSI;
    }
}

U32 ssp_pcap_gpio26RxdGetStatus(void)
{
    if(GPLR0&GPIO_26_SPI_MISO)
    {
        return SSP_PCAP_BIT_ONE;
    }
    else
    {
        return SSP_PCAP_BIT_ZERO;
    }
}

void ssp_pcap_gpioWrite(U32 pcapValue)
{
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    char string[50];
#endif
    U32 tempValue;
    U32 loopValue = 0x80000000;
    int i;
    /*  prepare for starting the frame  */
    ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);
    ssp_pcap_gpio24FrmControl(SSP_PCAP_BIT_ZERO);
    ssp_pcap_gpio25TxdControl(SSP_PCAP_BIT_ZERO);

    /*   start the data transfering frame  */
    ssp_pcap_gpio24FrmControl(SSP_PCAP_BIT_ONE);
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    sprintf( string," Write Value =%8x  \n ",pcapValue);
    printk( string );
#endif
    for(i=0;i<32;i++)
    {
        tempValue = pcapValue&loopValue;
        /* setup data bit to TXD line    */
        ssp_pcap_gpio25TxdControl(tempValue);
        /*  create clock   */
        ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ONE);
        loopValue = loopValue/2;
        ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);

#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
        sprintf( string,"%d ",tempValue>>(31-i));
        printk( string );
#endif
    }
    /*   end the frame */
    ssp_pcap_gpio24FrmControl(SSP_PCAP_BIT_ZERO);

#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk( " \n Write END  \n " );
#endif
    return;
}


/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:
   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/

void ssp_pcap_gpioRead( U32 registerNumber,P_U32 pPcapValue)
{
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    char string[50];
#endif
    U32 tempValue;
    int i;
    U32 loopValue = 0x80000000;
    
    /*  prepare for starting the frame  */
    ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);
    ssp_pcap_gpio24FrmControl(SSP_PCAP_BIT_ZERO);
    ssp_pcap_gpio25TxdControl(SSP_PCAP_BIT_ZERO);
    
    /* start the data transfering frame   */
    ssp_pcap_gpio24FrmControl(SSP_PCAP_BIT_ONE);
    /* indictaor it's a read data command */
    ssp_pcap_gpio25TxdControl(SSP_PCAP_BIT_ZERO);
    ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ONE);
    loopValue = loopValue/2;
    ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);

#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk(" Read Begin \n ");
#endif
    for(i=0; i<5;i++)
    {
        /*  here maybe can be optimized */
        tempValue = registerNumber&(loopValue/0x04000000);
        ssp_pcap_gpio25TxdControl(tempValue);
        ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ONE);
        loopValue = loopValue/2;
        ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
        sprintf( string,"%d ",tempValue);
        printk(string);
#endif
    }
    
    /*  the dead bit  */
    ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ONE);
    loopValue = loopValue/2;
    ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);

#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk("  \n  Data begin \n");
#endif
    tempValue = 0;
    for(i=0;i<25;i++)
    {
        tempValue |= loopValue*ssp_pcap_gpio26RxdGetStatus();
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
        sprintf( string,"%d ",ssp_pcap_gpio26RxdGetStatus());
        printk(string);
#endif
        ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ONE);
        loopValue = loopValue/2;
        ssp_pcap_gpio23ClkControl(SSP_PCAP_BIT_ZERO);
    }
    /*   end the data frame   */
    ssp_pcap_gpio24FrmControl(SSP_PCAP_BIT_ZERO);
    if(pPcapValue)
    {
        *pPcapValue = tempValue;
    }

#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk("  \n  Read Data end \n");
#endif
    return ;
}

#endif
/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:

       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION

void SSPTest(void)
{
    char string[80];
    U32 i,j,k;
    U16 x,y;
    U32 tempValue;
   // ssp_pcap_init();
//    ssp_pcap_open();
    printk(" \n ****************************** \n SSP TEST \n *****************\n ");
    SSP_vibrate_start_command();
    for(i=0; i<100000000; i++);
    SSP_vibrate_stop_command();
    printk( "\n ************* \n  test TS position start \n ");
    
    SSP_PCAP_write_data_to_PCAP(0,0x00100007);
    for(i=0;i<10000;i++)
    {
        tempValue = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_ISR_TSI); 
        if(tempValue)
        {        

            sprintf(string, "GEDR0 = %x  ICPR = %x ICMR= %x \n ",GEDR0,ICPR,ICMR );
            printk(string);
            GEDR0 = GEDR0;
            ICPR = ICPR;


            SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_MSR_TSM);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_ADCDONE2M);

            SSP_PCAP_TSI_mode_set(PCAP_TS_POSITION_XY_MEASUREMENT);
            SSP_PCAP_TSI_start_XY_read();
            tempValue = 0 ;
            while(!tempValue)
            {
                tempValue = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I);
            }
            SSP_PCAP_TSI_get_XY_value(&x,&y);
 
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_TSI);
            SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I);

//            sprintf(string,"\n  TS xy possition X=%d y=%d  GEDR0= %x ICPR= %x ICMR = %x GPIO1 interrupt times = %d \n",x,y,GEDR0,ICPR,ICMR,ssp_pcap_times);
            printk(string);
            GEDR0 = GEDR0;
            ICPR = ICPR;
            i = 0;
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM);
            SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_MSR_ADCDONE2M);
            
       }
       for(j=0;j<60000;j++);
    }
    printk(" TS pressure test start  \n ");
    SSP_PCAP_read_data_from_PCAP(0,&tempValue);
     
    sprintf(string," before clean ISR = %x \n ", tempValue );
    printk(string);
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_TSI);
    SSP_PCAP_read_data_from_PCAP(0,&tempValue);
    sprintf(string," after clean ISR = %x \n ", tempValue );
    printk(string);
    GEDR0 = GEDR0;
    ICPR = ICPR; 

    tempValue = 0;
    while(!tempValue)
    {
         tempValue = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_ISR_TSI);
         tempValue &= 0x00000001;
    }
    SSP_PCAP_read_data_from_PCAP(0,&tempValue);
    SSP_PCAP_read_data_from_PCAP(1,&k);
    sprintf(string, " MSR = %8x ISR= %8x,  GEDR0 = %8x  ICPR = %8x \n",k,tempValue,GEDR0,ICPR);
    printk(string);
    GEDR0 = GEDR0;
    ICPR = ICPR; 
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I);
    SSP_PCAP_TSI_mode_set(PCAP_TS_PRESSURE_MEASUREMENT);
    SSP_PCAP_TSI_start_XY_read();
    tempValue = 0;
    while(!tempValue)
    {
         tempValue = SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I);
         tempValue &= 0x00000001;
    }
    for(i=0;i<60000;i++);
    SSP_PCAP_read_data_from_PCAP(0,&tempValue);
    sprintf(string," ISR =%8x  GEDR0 =%8x ICPR = %8x \n",tempValue,GEDR0,ICPR);
    printk(string);
    SSP_PCAP_TSI_get_XY_value(&x,&y);
    sprintf(string," TS pressure x = %d y= %d \n" ,x,y );
    printk(string);
    return;
}
#endif
/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
static int pcap_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
    switch(req)
    {
        case PM_SUSPEND:
             ssp_pcap_intoSleepCallBack();
             break;
        case PM_RESUME:
             ssp_pcap_wakeUpCallBack();
             break;
    }
    return 0; 
}
static int ssp_pcap_init_is_ok = 0;

void ssp_pcap_init(void)
{
#ifdef SSP_PCAP_DEBUG_BOARD	
    U32 tempValue;
#endif
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    char debugStr[100];   
    sprintf(debugStr,"\n ***************** \n SSP init start \n");
    printk(debugStr);
#endif

#ifdef SSP_PCAP_OPERATE_WITH_SPI
    CKEN    |= 0x00000008;
    GRER0   &= 0xf07fffff;
    GFER0   &= 0xf07fffff;
    GPSR0   =  0x01000000;
    
    GPCR0   =  0x02800000;
    GPDR0   |= 0x03800000;
    /* GPIO27 works as BB_RESET  out PIN */ 
    GPDR0   &= 0xfbffffff;
    GAFR0_U = (GAFR0_U&0xffc03fff)|0x001a8000;
    SSCR1   = 0x00000401;
    SSCR0   = 0x00000000;
    SSCR0   = 0x0000008f;
    init_waitqueue_head(&ssp_pcap_send_wait);
    request_irq(IRQ_SSP,ssp_interrupt_routine,0,"SSP received the data irq ",NULL);
#else
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk(" \n ********************* \n SSP GPIO mode init \n");
#endif
    if(!ssp_pcap_init_is_ok)
    {
        ssp_pcap_gpioInit();
	ssp_pcap_init_is_ok = 1;
    }
#endif

#ifdef SSP_PCAP_DEBUG_BOARD	
    printk("\n ********************* \n SSP DEBUG BOARD init \n ");
    tempValue = GPDR2;
    tempValue = tempValue&0x0001ffff;
    tempValue |= 0x00010000;
    GPDR2 = tempValue;
    GAFR2_U = 0x00000002;    
#endif
    set_GPIO_mode(GPIO1_RST);
    set_GPIO_IRQ_edge(GPIO1_RST,GPIO_RISING_EDGE);

    request_irq(IRQ_GPIO1,ssp_pcap_interrupt_routine,0,"PCAP request irq ",NULL);
#ifdef  PCAP_LOG_FFUART_OUT
    pcap_output_log_init();
#endif
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk(" \n *********************** \n SSP init END \n ");
#endif
    /*  only for PST tool recognize the USB cable  */
/*    if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V ))
    {
        SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);  

        del_timer(&usb_switch_timer);	    
        init_timer(&usb_switch_timer);	    	
	usb_switch_timer.function = usb_hardware_switch;
	usb_switch_timer.expires = (jiffies + 130);
	add_timer(&usb_switch_timer);
            
    }
*/
    init_timer(&ssp_tsi_timer);	
    /* since in GPIO init, we have gotten the register's value.  */  
//    SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_MSR_USB4VM );
    SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_ISR_USB4VI);
    SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_USB4VM);
    SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_ISR_USB1VI);
    SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_USB1VM);
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_AUD_RX_AMPS_A1CTRL);
    SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL3);
    SSP_PCAP_V_VIB_level_set(PCAP_VIBRATOR_VOLTAGE_LEVEL3);
    /* register pm callback function */
    /* the PM callback will move to apm.c */
//    pcap_pm_dev = pm_register(PM_SYS_DEV,0,pcap_pm_callback);
    return;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void pcap_switch_off_usb(void)
{
    if(!ssp_pcap_init_is_ok)
    {
        ssp_pcap_gpioInit();
	ssp_pcap_init_is_ok = 1;
    }
    /*  only for PST tool recognize the USB cable  */
    if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V ))
    {
        SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
        SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);  
    }
    return;
}

void pcap_switch_on_usb(void)
{
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU); 
    if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V ))
    {
        stop_ffuart();
        usb_gpio_init();
    }
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void ssp_pcap_release(void)
{
/* the PM callback will move to apm.c */
//    pm_unregister(pcap_pm_dev);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void ssp_pcap_open(SSP_PCAP_INIT_DRIVER_TYPE portType)
{
#if 0
    U32 i;
    U32 tempValue;
#endif
#ifdef SSP_PCAP_DEBUG_BOARD	
    /*  select the primary SPI of PCAP  */
    *SSP_SELECT_BUFFER = 0x00000094;
    SSP_PCAP_write_data_to_PCAP(3, 0x00100002);
    SSP_PCAP_write_data_to_PCAP(20,0x010000a4);
    SSP_PCAP_bit_set(0x58080000 );
    SSP_PCAP_TSI_mode_set(PCAP_TS_STANDBY_MODE);
    *SSP_SELECT_BUFFER = 0x00000070;
#endif
#if 0
    if(0 == ssp_pcapRegisterReadOut)
    {
        ssp_pcapRegisterReadOut = 1;
        for(i=0; i<32 ;i++)
        {
            if(SSP_PCAP_SUCCESS == SSP_PCAP_read_data_from_PCAP(i,&tempValue))
            {
                ssp_pcap_registerValue[i] = tempValue;  
            }
            else
            {
                ssp_pcap_registerValue[i] = 0; 
            }
        }

/*      if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V ))
        {
            stop_ffuart();
            usb_gpio_init();
        }
        del_timer(&usb_switch_timer);	    
*/
//       SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU); 
    }
#endif

    switch(portType)
    {
        case SSP_PCAP_TS_OPEN:
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_TS_REFENB );
            SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I);
            SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_ISR_TSI);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_ADCDONE2M);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC2_ADINC1);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC2_ADINC2);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_ATO0);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_ATO1);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_ATO2);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_ATO3);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_ATOX);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_MTR1);
            SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ADC1_MTR2);
            break;
        case SSP_PCAP_AUDIO_OPEN:
            break;
        default:
            break;
    }
    return;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void ssp_pcap_close(void)
{
    SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_MSR_REGISTER,PCAP_MASK_ALL_INTERRUPT);
    return;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void ssp_pcap_intoSleepCallBack(void)
{
    /* set SPI_CLK as input PIN */;
    GPDR0 &= ~GPIO_23_SPI_CLK ;
    /* set SPI_MOSI as input PIN */
    GPDR0 &= ~GPIO_25_SPI_MOSI;
    /* set PCAP's CE pin to LOW level */
    if(ssp_pcap_operate_pin_with_inverter)
    {
        PGSR0  |= GPIO_24_SPI_CE;
    }
    else
    {
        PGSR0  &= ~GPIO_24_SPI_CE;
    }
}
/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void ssp_pcap_wakeUpCallBack(void)
{
    /* set SPI_CLK to output PIN */;
    GPDR0 |= GPIO_23_SPI_CLK ;
    /* set SPI_MOSI to output PIN */
    GPDR0 |= GPIO_25_SPI_MOSI;
}   

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_configure_USB_UART_transeiver(SSP_PCAP_PORT_TYPE portType)
{
    SSP_PCAP_STATUS ret = SSP_PCAP_SUCCESS;
    switch(portType)
    {
        case SSP_PCAP_SERIAL_PORT:
            ret = SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            if(SSP_PCAP_SUCCESS == ret)
            {
                ret = SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);
            }
            break;
        case SSP_PCAP_LOW_USB_PORT:
            ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);
            if(SSP_PCAP_SUCCESS == ret)
            {
                ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_FSENB);
            }
            if(SSP_PCAP_SUCCESS == ret)
            {
                ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            }
            break;
        case SSP_PCAP_HIGH_USB_PORT:
            ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_RS232ENB);
            if(SSP_PCAP_SUCCESS == ret)
            {
                ret = SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_FSENB);
            }
            if(SSP_PCAP_SUCCESS == ret)
            {
                ret = SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_VUSB_EN);
            }
            break;
        default:
            ret = SSP_PCAP_NOT_RUN ;
            break;
    }
    return ret;
}
/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/

SSP_PCAP_STATUS SSP_PCAP_TSI_mode_set(TOUCH_SCREEN_DETECT_TYPE mode_Type )
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_ADC1_REGISTER]&(~SSP_PCAP_TOUCH_PANEL_POSITION_DETECT_MODE_MASK);
    tempValue = tempValue|mode_Type;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ADC1_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
static void ssp_pcap_start_adc(u32 data)
{
    U32 tempValue;
    if(GPLR2&0x02000000)
    {
        GPCR2 = 0x02000000;
    }
    else
    {
        GPSR2 = 0x02000000;
    }
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION        
    printk("\n start a adc. ");	    	
#endif
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_ADC1_REGISTER]&SSP_PCAP_ADC_START_VALUE_SET_MASK;
    tempValue |= SSP_PCAP_ADC_START_VALUE;
    SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ADC1_REGISTER,tempValue);
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ADC2_ASC);
}


SSP_PCAP_STATUS SSP_PCAP_TSI_start_XY_read(void)
{
    SSP_PCAP_STATUS ret = SSP_PCAP_SUCCESS;
    /* invert the TC_MM_EN pin to alert BP */
    if(GPLR2&0x02000000)
    {
        GPCR2 = 0x02000000;
    }
    else
    {
        GPSR2 = 0x02000000;
    }
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION        
    printk("\n invert TC_MM_EN and start a adc timer. ");	    	
#endif
    kernel_OSCR2_delete(&ssp_start_adc_timer);
    ssp_start_adc_timer.function = ssp_pcap_start_adc;
    ssp_start_adc_timer.expires = START_ADC_DELAY_TIMER;
    kernel_OSCR2_setup(&ssp_start_adc_timer);
    return ret;    
}    

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES: y reads TSX1's ADC value in ADD2 register, x reads TSY1's ADC value in ADD1 register.  
          
---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_TSI_get_XY_value(P_U16 p_x,P_U16 p_y)
{
    U32 tempValue;
    SSP_PCAP_STATUS ret = SSP_PCAP_read_data_from_PCAP(SSP_PCAP_ADJ_ADC2_REGISTER,&tempValue);
    if( SSP_PCAP_SUCCESS != ret)
        return ret;
    if(tempValue&0x00400000)
        return SSP_PCAP_ERROR_VALUE;
    if(p_x)
    { 
        *p_x = (U16)(tempValue&SSP_PCAP_ADD1_VALUE_MASK);
    }
    if(p_y)
    {
        *p_y = (U16)((tempValue&SSP_PCAP_ADD2_VALUE_MASK)>>SSP_PCAP_ADD2_VALUE_SHIFT);
    }
    return ret;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/

SSP_PCAP_STATUS SSP_PCAP_CDC_CLK_set(PHONE_CDC_CLOCK_TYPE clkType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_AUD_CODEC_REGISTER]&(~SSP_PCAP_PHONE_CDC_CLOCK_MASK);
    tempValue = tempValue|clkType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_CODEC_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_V_VIB_level_set(VibratorVoltageLevel_TYPE VIBLevelType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_AUX_VREG_REGISTER]&(~SSP_PCAP_VIBRATOR_VOLTAGE_LEVEL_MASK);
    tempValue = tempValue|VIBLevelType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUX_VREG_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_CDC_SR_set(ST_SAMPLE_RATE_TYPE srType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_ST_DAC_REGISTER]&(~SSP_PCAP_STEREO_SAMPLE_RATE_MASK);
    tempValue = tempValue|srType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_BCLK_set(ST_BCLK_TIME_SLOT_TYPE bclkType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_ST_DAC_REGISTER]&(~SSP_PCAP_STEREO_BCLK_TIME_SLOT_MASK);
    tempValue = tempValue|bclkType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_STCLK_set(ST_CLK_TYPE stClkType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_ST_DAC_REGISTER]&(~SSP_PCAP_STEREO_CLOCK_MASK);
    tempValue = tempValue|stClkType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_DIG_AUD_FS_set(DIG_AUD_MODE_TYPE fsType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_ST_DAC_REGISTER]&(~SSP_PCAP_DIGITAL_AUDIO_MODE_MASK);
    tempValue = tempValue|fsType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ST_DAC_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_AUDIG_set(U32 audioInGain)
{
    U32 tempValue;
    if (audioInGain > PCAP_AUDIO_IN_GAIN_MAX_VALUE)
        return SSP_PCAP_ERROR_VALUE;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER]&(~SSP_PCAP_AUDIO_IN_GAIN_MASK);
    
    /* shoulb be better like this 	tempValue |= audioInGain <<SSP_PCAP_AUDIO_IN_GAIN_SHIFT; */
    tempValue = tempValue|audioInGain;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_MONO_set(MONO_TYPE monoType)
{
    U32 tempValue;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER]&(~SSP_PCAP_MONO_PGA_MASK);
    tempValue = tempValue|monoType;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER,tempValue);
}

/*---------------------------------------------------------------------------
  DESCRIPTION: get audio_in pin 's status

   INPUTS: None.


   OUTPUTS:


   IMPORTANT NOTES: stop interrup.


---------------------------------------------------------------------------*/
SSP_PCAP_BIT_STATUS SSP_PCAP_get_audio_in_status(void)
{
    u32 adcRegister = 0,i;
    u16 x,y;
    u8 adcStart;
#ifdef  PCAP_LOG_FFUART_OUT
    u8  temp[100];
#endif
    __cli();
    SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_MSR_ADCDONE2M);
    if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_ADC2_ASC))
    {
        adcStart = 1;
        adcRegister = SSP_PCAP_get_register_value_from_buffer(SSP_PCAP_ADJ_ADC1_REGISTER);
        while(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_ADC2_ASC));
    }
    else
    {
        adcStart = 0;
    }
    if(GPLR2&0x02000000)
    {
        GPCR2 = 0x02000000;
    }
    else
    {
        GPSR2 = 0x02000000;
    }
    for(i=0;i<0x5000;i++);
    SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ADC1_REGISTER,0x9f);
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ADC2_ASC);
    if(GPLR2&0x02000000)
    {
        GPCR2 = 0x02000000;
    }
    else
    {
        GPSR2 = 0x02000000;
    }
    while(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_ADC2_ASC));
    SSP_PCAP_TSI_get_XY_value(&x,&y);
    SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I);
    SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_ADCDONE2M);
#ifdef  PCAP_LOG_FFUART_OUT
    sprintf(temp,"x=%d y=%d",x,y);
    pcap_log_add_data(temp,strlen(temp));    
#endif
    if(1 == adcStart)
    {
#ifdef  PCAP_LOG_FFUART_OUT
        pcap_log_add_data("re-start adc!",strlen("re-start adc!"));    
#endif
        if(GPLR2&0x02000000)
        {
            GPCR2 = 0x02000000;
        }
        else
        {
            GPSR2 = 0x02000000;
        }
        for(i=0;i<0x5000;i++);
        SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ADC1_REGISTER,adcRegister);
        SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_ADC2_ASC);
        if(GPLR2&0x02000000)
        {
            GPCR2 = 0x02000000;
        }
        else
        {
            GPSR2 = 0x02000000;
        }
    }
#ifdef  PCAP_LOG_FFUART_OUT
    pcap_log_add_data("test audio end!",strlen("test audio end!"));    
#endif
    __sti();  
    if((x>500)&&(y>500))
    {
        return SSP_PCAP_BIT_ONE;
    }
    else
    {
        return SSP_PCAP_BIT_ZERO;
    }  
}
/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_AUDOG_set(U32 audioOutGain)
{
    U32 tempValue;
    if (audioOutGain > PCAP_AUDIO_OUT_GAIN_MAX_VALUE)
        return SSP_PCAP_ERROR_VALUE;
    tempValue = ssp_pcap_registerValue[SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER]&(~SSP_PCAP_AUDIO_OUT_GAIN_MASK);
    tempValue |= audioOutGain <<SSP_PCAP_AUDIO_OUT_GAIN_SHIFT;
    return SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER,tempValue);
}


/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:


---------------------------------------------------------------------------*/
#ifdef SSP_PCAP_OPERATE_WITH_SPI
/*  ssp has received 2 bytes, the function will be invoked.  */
static void ssp_interrupt_routine(int irq, void *dev_id, struct pt_regs *regs )
{
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk("\n SSP INTERRUPT WAS CALLED \n ");  
#endif 
    if(SSSR&SSP_SR_RNE)
    {
        ssp_pcap_readValue = (SSDR&SSP_PCAP_REGISTER_VALUE_DOWN_WORD_MASK)<<16;
        if(SSSR&SSP_SR_RNE)
        {
            ssp_pcap_readValue += (SSDR&SSP_PCAP_REGISTER_VALUE_DOWN_WORD_MASK);
            if(SSSR&SSP_SR_RNE)
            {
                while(SSSR&SSP_SR_RNE)
                {
                    ssp_pcap_readValue = (SSDR&SSP_PCAP_REGISTER_VALUE_DOWN_WORD_MASK);
                }
                ssp_pcap_readValue = 0;
            }
	}
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
        printk("\n SSP INTERRUPT send wakeup info \n ");  
#endif 
	wake_up(&ssp_pcap_send_wait);
    }
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk("\n SSP INTERRUPT EXIT \n ");   
#endif
    return;
}

#endif

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/

static void ssp_tsi_keeper(u32 data)
{
#ifdef  PCAP_LOG_FFUART_OUT
    pcap_log_add_data("TS timer arrived!",strlen("TS timer arrived!")); 
#endif
    if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_MSR_TSM))
    {
        SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_ISR_TSI); 
        SSP_PCAP_bit_clean( SSP_PCAP_ADJ_BIT_MSR_TSM); 
    }
    if(GPLR0&0x00000002)
    {
#ifdef  PCAP_LOG_FFUART_OUT
        pcap_log_add_data("Handle the missed int!",strlen("Handle the missed int!")); 
#endif
        ssp_pcap_interrupt_routine(1,NULL,NULL);
    }
    return;     
}
/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
void ssp_pcap_clear_int(void)
{
    U32 tempValue;
    U32 tempClearInterrupt;
    
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk( "\n ************************ \n SSP interrupt by GPIO from sleep. ");
#endif
    /* should care the return value */
    SSP_PCAP_read_data_from_PCAP( SSP_PCAP_ADJ_ISR_REGISTER,&tempValue);
    tempClearInterrupt = tempValue&(SSP_PCAP_ADJ_BIT_ISR_MB2I|SSP_PCAP_ADJ_BIT_ISR_STI|SSP_PCAP_ADJ_BIT_ISR_A1I|SSP_PCAP_ADJ_BIT_ISR_USB1VI|SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I|SSP_PCAP_ADJ_BIT_ISR_TSI|SSP_PCAP_ADJ_BIT_ISR_USB4VI);

    /* send the data to ISR register to clear the interrupt flag   */
    SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ISR_REGISTER,tempClearInterrupt);

    if(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I&tempValue)
    {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
        printk( "\n SSP ADC interrupt should be discarded." );
#endif
   }
    
    if(SSP_PCAP_ADJ_BIT_ISR_TSI&tempValue)
    {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
        printk( "\n SSP TSI should be discarded. ");
#endif
    }

    
    if(SSP_PCAP_ADJ_BIT_ISR_USB4VI&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_USB4VM))
        {
            if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V))
            {
	        /*  maybe we can support low speed USB  how to do??? */
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
                printk( "\n SSP USB4VI need handle. ");
#endif
                cebus_int_hndler(0,NULL,NULL);
            }
       }
   }
    
    if(SSP_PCAP_ADJ_BIT_ISR_USB1VI&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_USB1VM))
        {
            if(SSP_PCAP_BIT_ONE != SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_1V))
            {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
                printk( "\n SSP USB_VBUS less than 1V! need handle.");
#endif
                cebus_int_hndler(0,NULL,NULL);
            }
        }
    }

    
    if(SSP_PCAP_ADJ_BIT_ISR_A1I&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_A1M))
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n A1_INT insert/remove case, need to handle. ISR =%x ",tempValue );
#endif
            /*  earpiece insert/remove need handler */
            headjack_change_interrupt_routine(0,NULL,NULL);
        }
    }

    if(SSP_PCAP_ADJ_BIT_ISR_MB2I&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_MB2M))
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n MIC insert/remove case, need to handle! ISR =%x ",tempValue );
#endif
            /*  mic insert/remove or answer/end call handler   */
            mic_change_interrupt_routine(0,NULL,NULL);
        }
    }
    return;
}
/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
static u32 ssp_pcap_last_jiffies = 0;

static void ssp_pcap_ts_send_pMmessage(void)
{
     u32 temp = jiffies - ssp_pcap_last_jiffies;
     
     if( temp >SSP_SEND_PM_ALART_INTERVAL)
     { 
         ssp_pcap_last_jiffies = (u32) jiffies;
         queue_apm_event(KRNL_TOUCHSCREEN,NULL);
     }
     return;
}

static void ssp_pcap_interrupt_routine(int irq, void *dev_id, struct pt_regs *regs )
{
    u32 pcap_repeat_num;
    U32 tempValue;
    u32 tempClearInterrupt;
#ifdef  PCAP_LOG_FFUART_OUT
    u8  temp[100];
    pcap_log_add_data("SSP interrupt by GPIO.",strlen("\n SSP interrupt by GPIO."));    
#endif

#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk( "\n ************************ \n SSP interrupt by GPIO. ");
#endif
    /* should care the return value */
    pcap_repeat_num = 0;
intRepeat:    
    SSP_PCAP_read_data_from_PCAP( SSP_PCAP_ADJ_ISR_REGISTER,&tempValue);
    tempClearInterrupt = tempValue&(SSP_PCAP_ADJ_BIT_ISR_MB2I|SSP_PCAP_ADJ_BIT_ISR_STI|SSP_PCAP_ADJ_BIT_ISR_A1I|SSP_PCAP_ADJ_BIT_ISR_USB1VI|SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I|SSP_PCAP_ADJ_BIT_ISR_TSI|SSP_PCAP_ADJ_BIT_ISR_USB4VI);
    
    /* send the data to ISR register to clear the interrupt flag   */
    SSP_PCAP_write_data_to_PCAP(SSP_PCAP_ADJ_ISR_REGISTER,tempClearInterrupt);

    if(SSP_PCAP_ADJ_BIT_ISR_ADCDONE2I&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_ADCDONE2M))
        {
            /* call read XY callback function */
#ifdef  PCAP_LOG_FFUART_OUT
            pcap_log_add_data("SSP ADC interrupt need handle.",strlen("SSP ADC interrupt need handle."));    
#endif
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n ************************** \n SSP ADC interrupt need handle." );
#endif
            ssp_pcap_ts_send_pMmessage();
            SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_ADC1_ADEN);       
            del_timer(&ssp_tsi_timer);	    
            ssp_tsi_timer.function = ssp_tsi_keeper;
            /* 2 seconds  protect timer */
            ssp_tsi_timer.expires = jiffies + SSP_PCAP_TS_KEEPER_TIMER;  
	    add_timer(&ssp_tsi_timer);
            ezx_ts_dataReadok_interrupt(0,NULL,NULL);
        }
        else
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n ************************** \n SSP ADC interrupt should be discarded." );
#endif
        }
    }
    
    if(SSP_PCAP_ADJ_BIT_ISR_TSI&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_TSM))
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n ************************* \n SSP TSI need handle.");
#endif
#ifdef  PCAP_LOG_FFUART_OUT
            pcap_log_add_data("SSP TSI need handle.",strlen("SSP TSI need handle."));    
#endif
            ssp_pcap_ts_send_pMmessage();
            if ( pxafb_ezx_getLCD_status() )
            {
                 /* call touch panel callbcak function */
                 SSP_PCAP_bit_set( SSP_PCAP_ADJ_BIT_MSR_TSM);       
                 del_timer(&ssp_tsi_timer);	    
                 ssp_tsi_timer.function = ssp_tsi_keeper;
                 /* 2 seconds  protect timer */
                 ssp_tsi_timer.expires = jiffies + SSP_PCAP_TS_KEEPER_TIMER; 
	         add_timer(&ssp_tsi_timer);
                 ezx_ts_touch_interrupt(0,NULL,NULL);
	    }    
        }
        else
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n************************* \n SSP TSI should be discarded.");
#endif
#ifdef  PCAP_LOG_FFUART_OUT
            pcap_log_add_data("SSP TSI need not handle.",strlen("SSP TSI need not handle."));    
#endif
        }
    }

   
    if(SSP_PCAP_ADJ_BIT_ISR_USB4VI&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_USB4VM))
        {
            if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_4V))
            {
	        /*  maybe we can support low speed USB  how to do??? */
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
                printk( "\n************************* \n SSP USB4VI need handle.");
#endif
#ifdef  PCAP_LOG_FFUART_OUT
                pcap_log_add_data("SSP USB4VI need handle.",strlen("SSP USB4VI need handle."));    
#endif
                cebus_int_hndler(0,NULL,NULL);
            }
            else
            {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
                printk( "\n ************************* \n SSP USB4VI low need NOT handle same as USB1VI low.");
#endif
            }
        }
        else
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n************************* \n SSP USB4VI should be discarded.");
#endif
        }
    }
    
    if(SSP_PCAP_ADJ_BIT_ISR_USB1VI&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_USB1VM))
        {
            if(SSP_PCAP_BIT_ONE == SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_PSTAT_USBDET_1V))
            {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
                printk( "\n************************* \n SSP USB_VBUS greater than 1V!");
#endif
            }
            else
            {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
                printk( "\n************************* \n SSP USB_VBUS less than 1V! need handle.");
#endif
                cebus_int_hndler(0,NULL,NULL);
            }
        }
        else
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n************************* \n SSP USB1VI should be discarded.");
#endif
        }
    }

    
    if(SSP_PCAP_ADJ_BIT_ISR_A1I&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_A1M))
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n A1_INT insert/remove case, need to handle. ISR =%x ",tempValue );
#endif
#ifdef  PCAP_LOG_FFUART_OUT
            sprintf(temp,"A1_INT insert/remove case, need to handle. ISR =%x ",tempValue );
            pcap_log_add_data(temp,strlen(temp));    
#endif
            /*  earpiece insert/remove need handler */
            headjack_change_interrupt_routine(0,NULL,NULL);
        }
        else
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n A1_INT should be discarded." );
#endif
        }     
    }

    if(SSP_PCAP_ADJ_BIT_ISR_MB2I&tempValue)
    {
        if(SSP_PCAP_BIT_ZERO == SSP_PCAP_get_bit_from_buffer(SSP_PCAP_ADJ_BIT_MSR_MB2M))
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n MIC insert/remove case, need to handle! ISR =%x ",tempValue );
#endif
#ifdef  PCAP_LOG_FFUART_OUT
            sprintf(temp,"MIC insert/remove case, need to handle! ISR =%x ",tempValue );
            pcap_log_add_data(temp,strlen(temp));    
#endif
            /*  mic insert/remove or answer/end call handler   */
            mic_change_interrupt_routine(0,NULL,NULL);
        }
        else
        {
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
            printk( "\n************************* \n SSP MB2I should be discarded.");
#endif
        }   
    }
    if(GPLR0&0x00000002)
    {
        /*  this case is a very critical case. */
#ifdef  PCAP_LOG_FFUART_OUT
        pcap_log_add_data("repeat pcap int!",strlen("repeat pcap int!"));    
#endif
        if(pcap_repeat_num<10)
        {
            pcap_repeat_num++;
            goto intRepeat;
        }
        else
        {
#ifdef  PCAP_LOG_FFUART_OUT
            pcap_log_add_data("repeat pcap exceed 10 times!",strlen("repeat pcap exceed 10 times!"));    
#endif
        }  
    }
    return;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:


   OUTPUTS:


   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/

#ifdef SSP_PCAP_OPERATE_WITH_SPI

void ssp_put_intoData(U32 pcapValue)
{
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    char string[100];
#endif
    U32 tempFirstByte;
    U32 tempSecondByte;
    tempFirstByte = (pcapValue&SSP_PCAP_REGISTER_VALUE_UP_WORD_MASK)>>SSP_PCAP_REGISTER_VALUE_LENGTH;
    tempSecondByte = (pcapValue&SSP_PCAP_REGISTER_VALUE_DOWN_WORD_MASK);
	/* disable all interrupt or disable the SSP (zero to SSE) */
    __cli();
    SSDR = tempFirstByte ;
    SSDR = tempSecondByte ;
    __sti();
#ifdef SSP_PCAP_OPERATE_DEBUG_INFORNMATION
    printk("\n ssp put dat  \n");
    sprintf( string,"\n fisrt part =%8x  second part =%8x \n",tempFirstByte,tempSecondByte);
    printk(string);
#endif
    return;
}

#endif

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_write_data_to_PCAP(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER sspPcapRegister,U32 registerValue)
{
    u32 pcapTempValue ;
    unsigned long flags;
    /* prevent the process schedule out and mask the PCAP's interrupt handler */
/*
#ifdef  PCAP_LOG_FFUART_OUT
    pcap_log_add_data("write pcap start!",strlen("write pcap start!"));    
#endif
*/
    //ICMR &= ~(1<<(GPIO1_RST+PXA_IRQ_SKIP));
    save_flags(flags);
    __cli();
    spin_lock(&pcapoperation_lock);

    if(!test_and_set_bit(0,&ssp_pcap_status)) 
    {
        switch(sspPcapRegister)
        {
#ifdef SSP_PCAP_DEBUG_BOARD
            case 3:
            case 22:
#endif
            case SSP_PCAP_ADJ_ISR_REGISTER:
            case SSP_PCAP_ADJ_MSR_REGISTER:
            case SSP_PCAP_ADJ_PSTAT_REGISTER:
            case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
            case SSP_PCAP_ADJ_ADC1_REGISTER:
            case SSP_PCAP_ADJ_ADC2_REGISTER:
            case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
            case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
            case SSP_PCAP_ADJ_ST_DAC_REGISTER:
            case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
            case SSP_PCAP_ADJ_PERIPH_REGISTER:
            case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
            case SSP_PCAP_ADJ_GP_REG_REGISTER:
                ssp_pcap_registerValue[sspPcapRegister] = registerValue&SSP_PCAP_REGISTER_VALUE_MASK;
                pcapTempValue = SSP_PCAP_REGISTER_WRITE_OP_BIT|(sspPcapRegister<<SSP_PCAP_REGISTER_ADDRESS_SHIFT )|(ssp_pcap_registerValue[sspPcapRegister]);

#ifdef SSP_PCAP_OPERATE_WITH_SPI
                ssp_put_intoData(pcapTempValue);
                interruptible_sleep_on(&ssp_pcap_send_wait);
                /* here need to judge the wakeup reason and handle it */
#else
                ssp_pcap_gpioWrite(pcapTempValue);
#endif
                clear_bit(0,&ssp_pcap_status);

                /* Now it's OK */
                spin_unlock(&pcapoperation_lock);
                restore_flags(flags);
                //ICMR |= (1<<(GPIO1_RST+PXA_IRQ_SKIP));
/*
#ifdef  PCAP_LOG_FFUART_OUT
                pcap_log_add_data("write pcap end!",strlen("write pcap end!"));    
#endif
*/
                return SSP_PCAP_SUCCESS;
                /* maybe here should NOT be a break */
                break;
            default:
                clear_bit(0,&ssp_pcap_status);

                /* Now it's OK */
                spin_unlock(&pcapoperation_lock);
                restore_flags(flags);
                //ICMR |= (1<<(GPIO1_RST+PXA_IRQ_SKIP));
/*
#ifdef  PCAP_LOG_FFUART_OUT
                pcap_log_add_data("write pcap error register!",strlen("write pcap error register!"));    
#endif
*/
                return SSP_PCAP_ERROR_REGISTER;
                /* maybe here should NOT be a break */
                break;
        }
    }
    else
    {

         /* Now it's OK */
         spin_unlock(&pcapoperation_lock);
         restore_flags(flags);
         //ICMR |= (1<<(GPIO1_RST+PXA_IRQ_SKIP));
/*
#ifdef  PCAP_LOG_FFUART_OUT
         pcap_log_add_data("write pcap not run!",strlen("write pcap not run!"));    
#endif
*/
         return SSP_PCAP_NOT_RUN;
    }    
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_read_data_from_PCAP(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER sspPcapRegister,P_U32 pSspPcapRegisterValue)
{
#ifdef SSP_PCAP_OPERATE_WITH_SPI
    U32 pcapTempValue;
#endif
    unsigned long flags;
/*
#ifdef  PCAP_LOG_FFUART_OUT
    pcap_log_add_data("read pcap start!",strlen("read pcap start!"));    
#endif
*/
    /* prevent the process schedule out and mask the PCAP's interrupt handler */
    //ICMR &= ~(1<<(GPIO1_RST+PXA_IRQ_SKIP));
    save_flags(flags);
    __cli();
    spin_lock(&pcapoperation_lock);
      
    if(!test_and_set_bit(0,&ssp_pcap_status)) 
    {
        switch(sspPcapRegister)
        {
            case SSP_PCAP_ADJ_ISR_REGISTER:
            case SSP_PCAP_ADJ_MSR_REGISTER:
            case SSP_PCAP_ADJ_PSTAT_REGISTER:
            case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
            case SSP_PCAP_ADJ_ADC1_REGISTER:
            case SSP_PCAP_ADJ_ADC2_REGISTER:
            case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
            case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
            case SSP_PCAP_ADJ_ST_DAC_REGISTER:
            case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
            case SSP_PCAP_ADJ_PERIPH_REGISTER:
            case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
            case SSP_PCAP_ADJ_GP_REG_REGISTER:
#ifdef SSP_PCAP_OPERATE_WITH_SPI
                pcapTempValue = SSP_PCAP_REGISTER_READ_OP_BIT|(sspPcapRegister<<SSP_PCAP_REGISTER_ADDRESS_SHIFT );
                ssp_put_intoData(pcapTempValue);
                /* sleep with timeout */
                interruptible_sleep_on(&ssp_pcap_send_wait);

                /* here need to judge the wakeup reason and handle it */
//				if (signal_pending(current)) {
//					ret = -ERESTARTSYS;
//					break;
//				}
                *pSspPcapRegisterValue = ssp_pcap_readValue;
#else
                ssp_pcap_gpioRead(sspPcapRegister,pSspPcapRegisterValue);
#endif
                ssp_pcap_registerValue[sspPcapRegister] = *pSspPcapRegisterValue&SSP_PCAP_REGISTER_VALUE_MASK;
                clear_bit(0,&ssp_pcap_status);

                /* Now it's OK */
                spin_unlock(&pcapoperation_lock);
                restore_flags(flags);
                //ICMR |= (1<<(GPIO1_RST+PXA_IRQ_SKIP));
/*
#ifdef  PCAP_LOG_FFUART_OUT
                pcap_log_add_data("read pcap end!",strlen("read pcap end!"));    
#endif
*/
                return SSP_PCAP_SUCCESS;
                /* maybe here should NOT be a break */
                break;
            default:
                clear_bit(0,&ssp_pcap_status);
                
                /* Now it's OK */
                spin_unlock(&pcapoperation_lock);
                restore_flags(flags);
                //ICMR |= (1<<(GPIO1_RST+PXA_IRQ_SKIP));
/*
#ifdef  PCAP_LOG_FFUART_OUT
                pcap_log_add_data("read pcap error register!",strlen("read pcap error register!"));    
#endif
*/
                return SSP_PCAP_ERROR_REGISTER;
                /* maybe here should NOT be a break */
                break;
        }
    }
    else
    {
        /* Now it's OK */
        spin_unlock(&pcapoperation_lock);
        restore_flags(flags);
        //ICMR |= (1<<(GPIO1_RST+PXA_IRQ_SKIP));
/*
#ifdef  PCAP_LOG_FFUART_OUT
        pcap_log_add_data("read pcap not run!",strlen("read pcap not run!"));    
#endif
*/
        return SSP_PCAP_NOT_RUN;
    }    
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_bit_set(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE sspPcapBit ) 
{
    U32 sspPcapRegisterBitValue;
    SSP_PCAP_STATUS ret;
    U8 sspPcapRegister = (sspPcapBit&SSP_PCAP_REGISTER_ADDRESS_MASK)>>SSP_PCAP_REGISTER_ADDRESS_SHIFT;
    
    switch(sspPcapRegister)
    {
	case SSP_PCAP_ADJ_ISR_REGISTER:
            ssp_pcap_registerValue[sspPcapRegister] = 0;
#ifdef SSP_PCAP_DEBUG_BOARD
        case 22:
#endif
        case SSP_PCAP_ADJ_MSR_REGISTER:
	case SSP_PCAP_ADJ_PSTAT_REGISTER:
	case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
	case SSP_PCAP_ADJ_ADC1_REGISTER:
	case SSP_PCAP_ADJ_ADC2_REGISTER:
	case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
	case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
	case SSP_PCAP_ADJ_ST_DAC_REGISTER:
	case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
	case SSP_PCAP_ADJ_PERIPH_REGISTER:
	case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
        case SSP_PCAP_ADJ_GP_REG_REGISTER:

            sspPcapRegisterBitValue = (sspPcapBit&SSP_PCAP_REGISTER_VALUE_MASK);
            ssp_pcap_registerValue[sspPcapRegister] |= sspPcapRegisterBitValue;
            /* should care the return value */
            ret =SSP_PCAP_write_data_to_PCAP(sspPcapRegister,ssp_pcap_registerValue[sspPcapRegister]);
            return SSP_PCAP_SUCCESS;
            break;
	default:
	    return SSP_PCAP_ERROR_REGISTER;
	    break;
    }
    return SSP_PCAP_SUCCESS;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:

   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
SSP_PCAP_STATUS SSP_PCAP_bit_clean(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE sspPcapBit )
{
    U32 sspPcapRegisterBitValue;
    SSP_PCAP_STATUS ret;
    U8 sspPcapRegister = (sspPcapBit&SSP_PCAP_REGISTER_ADDRESS_MASK)>>SSP_PCAP_REGISTER_ADDRESS_SHIFT;
    
    switch(sspPcapRegister)
    {
	case SSP_PCAP_ADJ_ISR_REGISTER:
            ssp_pcap_registerValue[sspPcapRegister] = 0;
        case SSP_PCAP_ADJ_MSR_REGISTER:
	case SSP_PCAP_ADJ_PSTAT_REGISTER:
	case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
	case SSP_PCAP_ADJ_ADC1_REGISTER:
	case SSP_PCAP_ADJ_ADC2_REGISTER:
	case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
	case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
	case SSP_PCAP_ADJ_ST_DAC_REGISTER:
	case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
	case SSP_PCAP_ADJ_PERIPH_REGISTER:
	case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
        case SSP_PCAP_ADJ_GP_REG_REGISTER:
	    sspPcapRegisterBitValue = (sspPcapBit&SSP_PCAP_REGISTER_VALUE_MASK);
	    ssp_pcap_registerValue[sspPcapRegister] &= ~sspPcapRegisterBitValue;
	    /* should care the return value */
	    ret =SSP_PCAP_write_data_to_PCAP(sspPcapRegister,ssp_pcap_registerValue[sspPcapRegister]);
	    return SSP_PCAP_SUCCESS;
	    break;
	default:
	    return SSP_PCAP_ERROR_REGISTER;
	    break;
    }
    return SSP_PCAP_SUCCESS;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:

     
---------------------------------------------------------------------------*/
SSP_PCAP_BIT_STATUS SSP_PCAP_get_bit_from_buffer(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE sspPcapBit )
{
    U32 sspPcapRegisterBitValue;
    U8 sspPcapRegister = (sspPcapBit&SSP_PCAP_REGISTER_ADDRESS_MASK)>>SSP_PCAP_REGISTER_ADDRESS_SHIFT;
    switch(sspPcapRegister)
    {
        case SSP_PCAP_ADJ_ISR_REGISTER:
	case SSP_PCAP_ADJ_MSR_REGISTER:
	case SSP_PCAP_ADJ_PSTAT_REGISTER:
	case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
	case SSP_PCAP_ADJ_ADC1_REGISTER:
	case SSP_PCAP_ADJ_ADC2_REGISTER:
	case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
	case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
	case SSP_PCAP_ADJ_ST_DAC_REGISTER:
	case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
	case SSP_PCAP_ADJ_PERIPH_REGISTER:
	case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
        case SSP_PCAP_ADJ_GP_REG_REGISTER:
            sspPcapRegisterBitValue = (sspPcapBit&SSP_PCAP_REGISTER_VALUE_MASK);
            sspPcapRegisterBitValue &= ssp_pcap_registerValue[sspPcapRegister];
            if(sspPcapRegisterBitValue)
            {
                return SSP_PCAP_BIT_ONE;
            }
            else
            {
                return SSP_PCAP_BIT_ZERO;
            }    
            break;
	default:
            return SSP_PCAP_BIT_ERROR;
            break;
    }
    return SSP_PCAP_BIT_ERROR;
}

/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
SSP_PCAP_BIT_STATUS SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER_BIT_TYPE sspPcapBit )
{
    U32 sspPcapTempRegisterValue;
    U32 sspPcapRegisterBitValue;
    SSP_PCAP_STATUS ret;
    U8 sspPcapRegister = (sspPcapBit&SSP_PCAP_REGISTER_ADDRESS_MASK)>>SSP_PCAP_REGISTER_ADDRESS_SHIFT;
    
    switch(sspPcapRegister)
    {
	case SSP_PCAP_ADJ_ISR_REGISTER:
	case SSP_PCAP_ADJ_MSR_REGISTER:
	case SSP_PCAP_ADJ_PSTAT_REGISTER:
	case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
	case SSP_PCAP_ADJ_ADC1_REGISTER:
	case SSP_PCAP_ADJ_ADC2_REGISTER:
	case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
	case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
	case SSP_PCAP_ADJ_ST_DAC_REGISTER:
	case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
	case SSP_PCAP_ADJ_PERIPH_REGISTER:
	case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
        case SSP_PCAP_ADJ_GP_REG_REGISTER:
            sspPcapRegisterBitValue = (sspPcapBit&SSP_PCAP_REGISTER_VALUE_MASK);
		/* should care the return value */
            ret = SSP_PCAP_read_data_from_PCAP(sspPcapRegister,&sspPcapTempRegisterValue);
            sspPcapRegisterBitValue &= sspPcapTempRegisterValue;
            if(sspPcapRegisterBitValue)
            {
                return SSP_PCAP_BIT_ONE;
            }
            else
            {
                return SSP_PCAP_BIT_ZERO;
            }
            break;
	default:
            return SSP_PCAP_BIT_ERROR;
            break;
    }
    return SSP_PCAP_BIT_ERROR;
}
/*---------------------------------------------------------------------------
  DESCRIPTION:
      
   INPUTS:
      
       
   OUTPUTS:
      
       
   IMPORTANT NOTES:
       
     
---------------------------------------------------------------------------*/
U32 SSP_PCAP_get_register_value_from_buffer(SSP_PCAP_SECONDARY_PROCESSOR_REGISTER sspPcapRegister )
{
    switch(sspPcapRegister)
    {
	case SSP_PCAP_ADJ_ISR_REGISTER:
	case SSP_PCAP_ADJ_MSR_REGISTER:
	case SSP_PCAP_ADJ_PSTAT_REGISTER:
	case SSP_PCAP_ADJ_AUX_VREG_REGISTER:
	case SSP_PCAP_ADJ_ADC1_REGISTER:
	case SSP_PCAP_ADJ_ADC2_REGISTER:
	case SSP_PCAP_ADJ_AUD_CODEC_REGISTER:
	case SSP_PCAP_ADJ_AUD_RX_AMPS_REGISTER:
	case SSP_PCAP_ADJ_ST_DAC_REGISTER:
	case SSP_PCAP_ADJ_BUSCTRL_REGISTER:
	case SSP_PCAP_ADJ_PERIPH_REGISTER:
	case SSP_PCAP_ADJ_TX_AUD_AMPS_REGISTER:
        case SSP_PCAP_ADJ_GP_REG_REGISTER:
            return ssp_pcap_registerValue[sspPcapRegister];
            break;
	default:
	    return SSP_PCAP_ERROR_REGISTER;
	    break;
    }
}
#ifdef __cplusplus
}
#endif
