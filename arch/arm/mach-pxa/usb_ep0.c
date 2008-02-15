/*
 *  Copyright (C) Extenex Corporation 2001
 *  Copyright (C) Compaq Computer Corporation, 1998, 1999
 *  Copyright (C) Intrinsyc, Inc., 2002
 *  Copyright (C) Motorola, Inc., 2002
 *
 *  PXA USB controller driver - Endpoint zero management
 *
 *  Please see:
 *    linux/Documentation/arm/SA1100/SA1100_USB 
 *  for more info.
 *
 *  02-May-2002
 *   Frank Becker (Intrinsyc) - derived from sa1100 usb_ctl.c
 *  14-Oct-2002
 *   Levis Xu (Motorola) - add Motorola Vendor Specfic USB Protocol.
 */
#define EXPORT_SYMTAB

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/tqueue.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

#include "pxa_usb.h"  /* public interface */
#include "usb_ctl.h"  /* private stuff */
#include "usb_ep0.h"

// 1 == lots of trace noise,  0 = only "important' stuff
#define VERBOSITY 0 

enum { true = 1, false = 0 };
typedef int bool;
#ifndef MIN
#define MIN( a, b ) ((a)<(b)?(a):(b))
#endif

#if 1 && !defined( ASSERT )
#  define ASSERT(expr) \
if(!(expr)) { \
	printk( "Assertion failed! %s,%s,%s,line=%d\n",\
#expr,__FILE__,__FUNCTION__,__LINE__); \
}
#else
#  define ASSERT(expr)
#endif

#if VERBOSITY
#define PRINTKD(fmt, args...) printk( fmt , ## args)
#else
#define PRINTKD(fmt, args...)
#endif

static EP0_state ep0_state = EP0_IDLE;

interface_t intf6,intf8;
DECLARE_WAIT_QUEUE_HEAD(wq_read6);
DECLARE_WAIT_QUEUE_HEAD(wq_read8);
int intf_index;
int polling_times6=0,polling_times8=0;
int polling_len;
int specific_times6=0,specific_times8=0;
int read_ep0;

//lw
extern int polling_ready6, polling_ready8;
/***************************************************************************
  Prototypes
 ***************************************************************************/
/* "setup handlers" -- the main functions dispatched to by the
   .. isr. These represent the major "modes" of endpoint 0 operation */
static void sh_setup_begin(void);				/* setup begin (idle) */
static void sh_write( void );      				/* writing data */
static int  read_fifo( void* data, int length );
static int  read_fifo_data( void* data, int length );
static void write_fifo( void );
static void get_descriptor( usb_dev_request_t * pReq );
static void queue_and_start_write( void * p, int req, int act );

static void set_interface( usb_dev_request_t * pReq );
static void set_descriptor( usb_dev_request_t * pReq );

static void send_status_packet( int status );
static void wait_status_packet( void );

int active_interface, active_alt_setting;
int data_len;
unsigned char *ep0_data;
/***************************************************************************
  Inline Helpers
 ***************************************************************************/

/*
inline int type_code_from_request( __u8 by ) { return (( by >> 4 ) & 3); }
*/
inline int type_code_from_request( __u8 by ) { return (( by >> 5 ) & 3); }

/* print string descriptor */
static inline void psdesc( string_desc_t * p )
{
  int i;
  int nchars = ( p->bLength - 2 ) / sizeof( __u16 );
  printk( "'" );
  for( i = 0 ; i < nchars ; i++ ) {
    printk( "%c", (char) p->bString[i] );
  }
  printk( "'\n" );
}

#if VERBOSITY
/* "pcs" == "print control status" */
static inline void pcs( void )
{
  __u32 foo = UDCCS0;
  printk( "%08x: %s %s %s %s %s %s\n",
	  foo,
	  foo & UDCCS0_SA   ? "SA" : "",
	  foo & UDCCS0_OPR  ? "OPR" : "",
	  foo & UDCCS0_RNE  ? "RNE" : "",
	  foo & UDCCS0_SST  ? "SST" : "",
	  foo & UDCCS0_FST  ? "FST" : "",
	  foo & UDCCS0_DRWF ? "DRWF" : ""
	  );
}
static inline void preq( usb_dev_request_t * pReq )
{
  static char * tnames[] = { "dev", "intf", "ep", "oth" };
  static char * rnames[] = { "std", "class", "vendor", "???" };
  char * psz;
  switch( pReq->bRequest ) {
  case GET_STATUS:        psz = "get stat"; break;
  case CLEAR_FEATURE:     psz = "clr feat"; break;
  case SET_FEATURE:       psz = "set feat"; break;
  case SET_ADDRESS:       psz = "set addr"; break;
  case GET_DESCRIPTOR:    psz = "get desc"; break;
  case SET_DESCRIPTOR:    psz = "set desc"; break;
  case GET_CONFIGURATION: psz = "get cfg"; break;
  case SET_CONFIGURATION: psz = "set cfg"; break;
  case GET_INTERFACE:     psz = "get intf"; break;
  case SET_INTERFACE:     psz = "set intf"; break;
  case SYNCH_FRAME:       psz = "synch frame"; break;
  default:                psz = "unknown"; break;
  }
  printk( "- [%s: %s req to %s. dir=%s]\n", psz,
	  rnames[ (pReq->bmRequestType >> 5) & 3 ],
	  tnames[ pReq->bmRequestType & 3 ],
	  ( pReq->bmRequestType & 0x80 ) ? "in" : "out" );
}

#else
static inline void pcs( void ){}
static inline void preq( usb_dev_request_t *x){}
#endif

/***************************************************************************
  Globals
 ***************************************************************************/
static const char pszMe[] = "usbep0: ";

/* pointer to current setup handler */
static void (*current_handler)(void) = sh_setup_begin;

/* global write struct to keep write
   ..state around across interrupts */
static struct {
  unsigned char *p;
  int bytes_left;
} wr;

/***************************************************************************
  Public Interface
 ***************************************************************************/

/* reset received from HUB (or controller just went nuts and reset by itself!)
   so udc core has been reset, track this state here  */
void ep0_reset(void)
{
  PRINTKD( "%sep0_reset\n", pszMe);
  /* reset state machine */
  current_handler = sh_setup_begin;
  wr.p = NULL;
  wr.bytes_left = 0;
  usbd_info.address=0;
}

/* handle interrupt for endpoint zero */
void ep0_int_hndlr( void )
{
	int read_ep0_tmp;
	__u32 cs_reg_in = UDCCS0;
  	PRINTKD( "%sep0_int_hndlr\n", pszMe);
  	pcs();
  	if ( (cs_reg_in & UDCCS0_OPR) == 0 ) {
	    /* we can get here early...if so, we'll int again in a moment  */
	    PRINTKD( "ep0_int_hndlr: no OUT packet available. Exiting\n" );
	    goto end;
	  }

	if( (cs_reg_in & UDCCS0_RNE) == 0 )
	{
		PRINTKD("\nzero_packet_flag=%d\n", zero_packet_flag);
		UDCCS0 = UDCCS0_OPR;
		return;
	}
	else if( cs_reg_in & UDCCS0_SA )
	{
		read_ep0=0;
	 	(*current_handler)();
	}
	else
	{
		if(read_ep0 == 0)
		{
			read_ep0=read_fifo_data(ep0_data, data_len);
		}else{
			read_ep0_tmp=read_ep0;
			read_ep0=read_fifo_data(ep0_data, data_len);
			read_ep0=read_ep0+read_ep0_tmp;
		}
	//	printk("\nread_ep0=%d\n",read_ep0);
	//	printk("\ndata_len=%d\n",data_len);
		if(read_ep0 == data_len)
		{
                        if (intf_index == 8)
			    specific_message(&intf8,ep0_data,data_len);
                        else if (intf_index == 6)
                            specific_message(&intf6,ep0_data,data_len);
                        else
                            printk("\nerror interface index");
		}
	}
	
  	end:
  	return;
}

void ep0_process_standard_request( usb_dev_request_t req )
{
#if VERBOSITY
  {
    unsigned char * pdb = (unsigned char *) &req;
    PRINTKD( "STANDARD REQUEST:%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X ",
	     pdb[0], pdb[1], pdb[2], pdb[3], pdb[4], pdb[5], pdb[6], pdb[7]
	     );
    preq( &req );
  }
#endif

  switch( req.bRequest ) {

  case SET_ADDRESS:
    PRINTKD( "%sSET_ADDRESS handled by UDC\n", pszMe);
    break;

  case SET_FEATURE:
    PRINTKD( "%sSET_FEATURE handled by UDC\n", pszMe);
    break;

  case CLEAR_FEATURE:
    PRINTKD( "%sCLEAR_FEATURE handled by UDC\n", pszMe);
    break;

  case GET_CONFIGURATION:
    PRINTKD( "%sGET_CONFIGURATION handled by UDC\n", pszMe );
    break;

  case GET_STATUS:
    PRINTKD( "%sGET_STATUS handled by UDC\n", pszMe );
    break;

  case GET_INTERFACE:
    PRINTKD( "%sGET_INTERFACE handled by UDC\n", pszMe);
    break;

  case SYNCH_FRAME:
    PRINTKD( "%sSYNCH_FRAME handled by UDC\n", pszMe );
    break;

  case GET_DESCRIPTOR:
    PRINTKD( "%sGET_DESCRIPTOR\n", pszMe );
    get_descriptor( &req );
    break;

  case SET_INTERFACE:
    PRINTKD( "%sSET_INTERFACE\n", pszMe);
    set_interface( &req );
    break;

  case SET_DESCRIPTOR:
    PRINTKD( "%sSET_DESCRIPTOR\n", pszMe );
    set_descriptor( &req );
    break;

  case SET_CONFIGURATION:
    PRINTKD( "%sSET_CONFIGURATION %d\n", pszMe, req.wValue);
//lw
    switch( req.wValue)
      {
      case 0:
	usbctl_next_state_on_event( kEvConfig );
	break;
      case 1:
	usbctl_next_state_on_event( kEvDeConfig );
	break;
      default:
	PRINTKD( "%sSET_CONFIGURATION: unknown configuration value (%d)\n", pszMe, req.wValue);
	break;
      }
//lw
//lw    pxa_usb_send("lw", 2, NULL);
    break;
  default :
    printk("%sunknown request 0x%x\n", pszMe, req.bRequest);
    break;

  }

}

/* We don't have any class request to this device now. */
void ep0_process_class_request( usb_dev_request_t req )
{
#if VERBOSITY
  {
    unsigned char * pdb = (unsigned char *) &req;
    PRINTKD( " CLASS REQUEST:%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X ",
	     pdb[0], pdb[1], pdb[2], pdb[3], pdb[4], pdb[5], pdb[6], pdb[7]
	     );
    preq( &req );
  }
#endif
  PRINTKD(" Do nothing now!\n");
}

/* 
 * invoked by process_interface8_request and process_interface6_request
 *
 * process the polling request for interface
 *
 */
#if 0
void polling_message( interface_t *interface, usb_dev_request_t *req )
{
  polling_data_t pd;
  struct list_head *ptr;
  msg_node_t* entry;
  int i;
  int count = 50000;

  memset( (void*)&pd, 0, sizeof(polling_data_t) );
  
  do{
	if( interface->list_len ) break;
	count--;
	udelay(1);
	}while(count);

  if ( interface->list_len == 0 ){
	PRINTKD("interface->list_len is 0\n");
	return;
	}

  if ( interface->list_len>3 ) {
    pd.status &= 0x02;//???
    pd.numMessages = 3;
  } else {
	pd.status &= 0x00;
	pd.numMessages = interface->list_len;
  }

  ptr = interface->in_msg_list.next;
  for ( i=0; i<pd.numMessages; i++ ) {
    entry = list_entry( ptr, msg_node_t, list );
    pd.msgsiz[i] = make_word_b(entry->size);
    ptr = ptr->next;
  }

  /* write back to HOST */
  queue_and_start_write( &pd, req->wLength, sizeof(__u8)*2+sizeof(__u16)*pd.numMessages );

  PRINTKD("polling messages: send out response now!\n");
  return;
}
#endif

void polling_return( usb_dev_request_t *req )
{
	polling_len = req->wLength;
/*	if(polling_times == 0)
          polling_message( &intf8, polling_len );*/
/*	
	if(polling_times6 == 0)
          polling_message( &intf6, polling_len );
		else if(polling_times8 == 0)
			polling_message( &intf8, polling_len);*/
//lw
        PRINTKD("\npolling_return..., polling_times6=%d specific_times6=%d\n", polling_times6,specific_times6);
	if(req->wIndex == 6)
        {
            if( (specific_times6 == 0) || (polling_ready6 > 0) )
            {
                polling_message( &intf6, polling_len );
                if(polling_ready6 > 0) polling_ready6--;
            }
            else
                polling_times6++;
        }
	else if(req->wIndex == 8)
        {
            if( (specific_times8 == 0) || (polling_ready8 > 0) )
            {
                polling_message( &intf8, polling_len );
                if(polling_ready8 > 0) polling_ready8--;
            }
            else
                polling_times8++;
        }
	return;
}

void polling_message( interface_t *interface, int polling_data_len )
{
	polling_data_t pd;
	struct list_head *ptr;
	msg_node_t* entry;
	int i;

	memset( (void*)&pd, 0, sizeof(polling_data_t) );
        PRINTKD("\nThis is polling1\n");

	if ( interface->list_len>3 ) {
	pd.status &= 0x02;//???
	pd.numMessages = 3;
	} else {
	pd.status &= 0x00;
	pd.numMessages = interface->list_len;
	}
	
    ptr = interface->in_msg_list.next;
	for ( i=0; i<pd.numMessages; i++ ) {
	entry = list_entry( ptr, msg_node_t, list );
	pd.msgsiz[i] = make_word_b(entry->size);
	ptr = ptr->next;
	}

  	PRINTKD("\nThis is polling2\n");
	/* write back to HOST */
	queue_and_start_write( &pd, polling_data_len, sizeof(__u8)*2+sizeof(__u16)*pd.numMessages );
	
  	PRINTKD("\nThis is polling3\n");
	PRINTKD("polling messages: send out response now!\n");
//lw 
//        ep_bulk_in1_reset();
//        usbctl_next_state_on_event(kEvConfig);
        PRINTKD("\npolling_message... polling_times6=%d specific_times6=%d\n",polling_times6,specific_times6);
	return;
}


typedef struct {
  __u16 size;
} msg_t;

/* 
 * invoked by process_interface1_request and process_interface0_request
 *
 * process the queue request for interface
 *
 */
void queue_message( interface_t *interface, usb_dev_request_t *req )
{
  struct list_head* ptr=NULL,*tmp;
  msg_node_t* entry=NULL;
  queued_data_t *data=NULL;
  int numReq,i,numMsgBytes=0,numQueuedBytes=0,data_size=0;
  msg_t* msg_array;

  numReq = MIN( interface->list_len, req->wValue );
  PRINTKD("The request wanted to be returned to host is %d.\n", numReq );
  
  ptr = interface->in_msg_list.next;
  for (i=0; i<numReq; i++ ) {
    entry = list_entry( ptr, msg_node_t, list );
    numMsgBytes += entry->size;
    ptr = ptr->next;
  }

  for (i=0; i<(interface->list_len-numReq); i++ ) {
    entry = list_entry( ptr, msg_node_t, list );
    numQueuedBytes += entry->size;
    ptr = ptr->next;
  }

//  printk("\nThis is queue1\n");
  data_size = sizeof(queued_data_t)+sizeof(__u16)*numReq+numMsgBytes;
//  printk("\nsize=%d\n",data_size);
  data = (queued_data_t*) kmalloc( data_size, GFP_ATOMIC );

  if ( !data ) {
    printk( "\nqueue_message: (%d) out of memory!\n", (sizeof(queued_data_t)+sizeof(__u16)*numReq+numMsgBytes) );
    return;
  }
  data->numMsgs = numReq;
  data->numQueuedMsgs = interface->list_len - numReq;
  data->numQueuedBytes = make_word_b(numQueuedBytes);
  msg_array = (msg_t*) (data+1);

  ptr = interface->in_msg_list.next;
  for ( i=0; i<numReq; i++ ) {
    entry = list_entry(ptr,  msg_node_t, list );
    msg_array->size = make_word_b(entry->size);
    memcpy(((char*)msg_array)+2, entry->body, entry->size );
    msg_array = (msg_t*)(((char*)msg_array)+2+entry->size);

    tmp = ptr;
    ptr = ptr->next;
    
    interface->list_len--;
    list_del( tmp );
    kfree( entry->body );
    kfree( entry );
   }
// printk("\nThis is queue_message1\n"); 
  queue_and_start_write( data, req->wLength, sizeof(queued_data_t)+sizeof(__u16)*numReq+numMsgBytes );

// printk("\nThis is queue_message2\n"); 
  kfree( data );
//  polling_times = 0;
//lw
  PRINTKD("\nqueue_messages: send out response now.\n");
  return;
}

/* 
 * invoked by process_interface8_request and process_interface0_request
 *
 * process the specific request for interface
 *
 */

void ep0_data_buff( usb_dev_request_t* req )
{
   /* send back zero */
  //queue_and_start_write( NULL, 0, 0 );
  //PRINTKD("specific_messages: send out zero response now.\n");

  data_len = req->wLength;
  ep0_data = (unsigned char*)kmalloc( sizeof(unsigned char)*data_len, GFP_ATOMIC );
  intf_index = req->wIndex;
  if ( !ep0_data ) {
    printk( "ep0_data_buff: (%d) out of memory!\n", sizeof(unsigned char)*data_len );
    return;
  }
  return;
}


void specific_message( interface_t *interface, unsigned char* data, int length )
{
  msg_node_t *node;
  unsigned char *value;

  /* send back zero */
//  queue_and_start_write( NULL, 0, 0 );
//  PRINTKD("specific_messages: send out zero response now.\n");

  node = (msg_node_t*)kmalloc( sizeof(msg_node_t), GFP_KERNEL );
  if ( !node ) {
    printk( "specific_messages: (%d) out of memory!\n", sizeof(msg_node_t) );
    return;
  }

  node->size = length;
  node->body = (unsigned char*) data;
  list_add_tail( &node->list, &interface->out_msg_list);

  if(intf_index == 6)
 		{
 			wake_up_interruptible(&wq_read6);
 		}else if(intf_index == 8)
 			{
 				wake_up_interruptible(&wq_read8);
 			}
  
  PRINTKD( "specific_messages: return successfully!\n" );

//  polling_times++;
  if(intf_index == 6)
      specific_times6++;
  else if (intf_index ==8)
      specific_times8++;
//lw
  PRINTKD("specific_message... polling_times6=%d specific_times6=%d\n", polling_times6,specific_times6);
  return;
}


/* 
 * process_interface0_request 
 * process the request for interface1
 */
/* NOTE: process_interface0_request() is called in interrupt context, so we can't use wait queue here. */
void process_interface6_request( usb_dev_request_t req )
{
  switch ( req.bRequest ) {
  case 0: /* Polling */
    if ( DATA_DIRECTION(req.bmRequestType) == DATA_IN ) {
      polling_return( &req );
    } else {
      PRINTKD( "process_interface6_request() Invalid polling direction.\n" );
    }
    break;
  case 1: /* Queued Messages */
    if ( DATA_DIRECTION(req.bmRequestType) == DATA_IN ) {
      queue_message( &intf6, &req );
    } else {
      PRINTKD( "process_interface6_request() Invalid queue direction.\n" );
    }
    break;
  case 2: /* Interface specific request */
    if ( DATA_DIRECTION(req.bmRequestType) == DATA_OUT ) {
      //specific_message( &intf6, &req );
      ep0_data_buff(&req);
    } 
    break;
  case 3: /* Security - TBD */
    PRINTKD( "TBD\n" );
    break;
  default:
    break;
  }
}

/* 
 * NOTE: process_interface8_request() is called in interrupt context, so we can't use wait queue here.
 *                             
 */
void process_interface8_request( usb_dev_request_t req )
{
	int count = 10000;
	
  switch ( req.bRequest ) {
  case 0: /* Polling */
	  printk("This is Polling case.\n");
    if ( DATA_DIRECTION(req.bmRequestType) == DATA_IN ) {
    	polling_return(&req);
//        polling_message( &intf8, req.wLength );
    } else {
      PRINTKD( "process_interface8_request() Invalid polling direction.\n" );
    }
    break;

  case 1: /* Queued Messages */
	  PRINTKD("This is Queued Messages case.\n");
    if ( DATA_DIRECTION(req.bmRequestType) == DATA_IN ) {
      queue_message( &intf8, &req );
    } else {
      PRINTKD( "process_interface8_request() Invalid queued direction.\n" );
    }
    break;

  case 2: /* Interface specific request */
	PRINTKD("This is Test Command case.\n");
    if ( DATA_DIRECTION(req.bmRequestType) == DATA_OUT ) {
       ep0_data_buff(&req);
    } else {
      PRINTKD( "process_interface8_request() Invalid interface command direction.\n" );
    }
    break;

  case 3: /* Security - TBD */
    PRINTKD( "Security command - TBD.\n" );
    break;

  default:
    break;
  }
  return;
}

/* We will process some interface specific request here. */
void ep0_process_vendor_request( usb_dev_request_t req )
{
	int enable,count;
	__u8 num_cfg = USB_NUM_OF_CFG_SETS;
	
#if VERBOSITY
  {
    unsigned char * pdb = (unsigned char *) &req;
    PRINTKD( "VENDOR REQUEST:%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X ",
	     pdb[0], pdb[1], pdb[2], pdb[3], pdb[4], pdb[5], pdb[6], pdb[7]
	     );
    preq( &req );
  }
#endif
  /* 
   * Do we need to do any thing else? We only throw it to upper driver and let 
   * them to decide what to do 
   */ 
  /*if ( (req.bmRequestType&0x0F)!=0x01 ) { 
    printk("%sunknown vendor request.", pszMe );
    return;
  }*/
enable = 1;
count = 100;
if ( (req.bmRequestType&0x0F) == 0x00 ) 
{
	switch ( req.bRequest ) {
	  case 0:
	  	//This branch will Report Number of configurations.
	    PRINTKD( "DEVICE 0\n");
	    if (((req.bmRequestType & 0x80) != 0)
             && (req.wValue == 0)
             && (req.wIndex == 0)
             && (req.wLength == 2))
         {
             queue_and_start_write( &num_cfg, req.wLength, 1 );
         }
	    break;
		
	  case 1:
	  	//This branch will Change to alternate configuration set.
	    PRINTKD( "DEVICE 1\n");
	    if (((req.bmRequestType & 0x80) == 0)
             && (req.wIndex <= USB_NUM_OF_CFG_SETS)
             && (req.wValue == 0)
             && (req.wLength == 0) )
         {
		queue_and_start_write( NULL, 0, 0 );
	/*	do{
			count--;
			udelay(1);
			}while(count);*/
	//	usb_reset_for_cfg8(enable);
		
	   }
	    break;
		
	  case 2:
	  	//This branch will List descriptors for alternate configuration set
	    PRINTKD( "DEVICE 2\n");
	    break;
		
	  case 3:
	  	//This branch will Quick-change to alternate configuration set
	    PRINTKD( "DEVICE 3\n");
	    if (((req.bmRequestType & 0x80) == 0)
             && (req.wIndex <= USB_NUM_OF_CFG_SETS)
             && (req.wValue == 0)
             && (req.wLength == 0) )
         {
         	queue_and_start_write( NULL, 0, 0 );;
	   }
	    break;
		
	  default:
	    printk( "%sunknown device.\n", pszMe );
	    break;
	  }
}
else if ( (req.bmRequestType&0x0F) == 0x01 ) 
{
	  switch ( req.wIndex ) {
	  case 6:
	    PRINTKD( "INTERFACE 6\n");
	    if ( intf6.enable ) 
	      process_interface6_request( req );
	    else /* This branch is not only for test. It is necessary. */
	      PRINTKD("interface6 is not enabled.");

	    break;
	  case 8:
	    PRINTKD( "INTERFACE 8\n");
	    if ( intf8.enable ) 
	      process_interface8_request( req );
	    else /* This branch is not only for test. It is necessary. */
	      PRINTKD("interface8 is not enabled.");

	    break;
	  default:
	    printk( "%sunknown interface.\n", pszMe );
	    break;
	  }

	  PRINTKD("process vendor command OK!\n");
	}
	return;
}

/*
 * sh_setup_begin()
 * This setup handler is the "idle" state of endpoint zero. It looks for OPR
 * (OUT packet ready) to see if a setup request has been been received from the
 * host. 
 */
static void sh_setup_begin( void )
{
  usb_dev_request_t req;
  int request_type;
  int n;
  __u32 cs_reg_in = UDCCS0;

  PRINTKD( "%ssh_setup_begin\n", pszMe);

  /* Be sure out packet ready, otherwise something is wrong */
  if ( (cs_reg_in & UDCCS0_OPR) == 0 ) {
    /* we can get here early...if so, we'll int again in a moment  */
    PRINTKD( "%ssetup begin: no OUT packet available. Exiting\n", pszMe );
    goto sh_sb_end;
  }

  if( ((cs_reg_in & UDCCS0_SA) == 0) && (ep0_state == EP0_IN_DATA_PHASE))
    {
      PRINTKD( "%ssetup begin: premature status\n", pszMe );

      /* premature status, reset tx fifo and go back to idle state*/
      UDCCS0 = UDCCS0_OPR | UDCCS0_FTF;

      ep0_state = EP0_IDLE;
      return;
    }

  if( (UDCCS0 & UDCCS0_RNE) == 0)
    {
      /* zero-length OUT? */
      printk( "%ssetup begin: zero-length OUT?\n", pszMe );
      goto sh_sb_end;
    }

  /* read the setup request */
  n = read_fifo( (void*)&req, sizeof(req) );
  if ( n != sizeof( req ) ) {
    printk( "%ssetup begin: fifo READ ERROR wanted %d bytes got %d. Stalling out...\n",
	    pszMe, sizeof( req ), n );
    /* force stall, serviced out */
    UDCCS0 = UDCCS0_FST;
    goto sh_sb_end;
  }

  request_type = type_code_from_request( req.bmRequestType );

  switch( request_type ) {
  case 0: /* standard request */
    ep0_process_standard_request( req );
    break;
    /* 
     * Note: Here, we should be attention that now we are in interrupt context. 
     * So, we can't use wait queue here. 
     */
  case 1: /* class request */
    ep0_process_class_request( req );
    break;
  case 2: /* vendor request */
    ep0_process_vendor_request( req );
    break;
  default: /* reserved request */
    printk( "%ssetup begin: unsupported bmRequestType: %d ignored\n", pszMe, request_type );
    break;
  }
 sh_sb_end:
  return;
}

/*
 * sh_write()
 * 
 * Due to UDC bugs we push everything into the fifo in one go.
 * Using interrupts just didn't work right...
 * This should be ok, since control request are small.
 */
static void sh_write()
{
  PRINTKD( "sh_write\n" );
  do
    {
      write_fifo();
    } while( ep0_state != EP0_END_XFER);
}

/***************************************************************************
  Other Private Subroutines
 ***************************************************************************/
/*
 * queue_and_start_write()
 * data == data to send
 * req == bytes host requested
 * act == bytes we actually have
 *
 * Sets up the global "wr"-ite structure and load the outbound FIFO 
 * with data.
 *
 */
static void queue_and_start_write( void * data, int req, int act )
{
  PRINTKD( "write start: bytes requested=%d actual=%d\n", req, act);

  wr.p = (unsigned char*) data;
  wr.bytes_left = MIN( act, req );

  ep0_state = EP0_IN_DATA_PHASE;
  sh_write();

  return;
}

/*
 * write_fifo()
 * Stick bytes in the endpoint zero FIFO.
 *
 */
static void write_fifo( void )
{
  int bytes_this_time = MIN( wr.bytes_left, EP0_FIFO_SIZE );
  int bytes_written = 0;

  while( bytes_this_time-- ) {
    PRINTKD( "%2.2X ", *wr.p );
    UDDR0 = *wr.p++;
    bytes_written++;
  }
  wr.bytes_left -= bytes_written;

  usbd_info.stats.ep0_bytes_written += bytes_written;

  if( (wr.bytes_left==0))
    {
      wr.p = NULL;  				/* be anal */

      if(bytes_written < EP0_FIFO_SIZE)
	{
	  int count;
	  int udccs0;

	  /* We always end the transfer with a short or zero length packet */
	  ep0_state = EP0_END_XFER;
	  current_handler = sh_setup_begin;

	  /* Let the packet go... */
	  UDCCS0 = UDCCS0_IPR;

	  /* Wait until we get to status-stage, then ack.
	   *
	   * When the UDC sets the UDCCS0[OPR] bit, an interrupt
	   * is supposed to be generated (see 12.5.1 step 14ff, PXA Dev Manual).   
	   * That approach didn't work out. Usually a new SETUP command was
	   * already in the fifo. I tried many approaches but was always losing 
	   * at least some OPR interrupts. Thus the polling below...
	   */
	  count = 10000;	//by Levis.
	  udccs0 = UDCCS0;
	  do
	    {
	      if( (UDCCS0 & UDCCS0_OPR)) 
		{
		  /* clear OPR, generate ack */
		  UDCCS0 = UDCCS0_OPR;
		  break;
		}
	      count--;	
	      udelay(1);
	    } while( count);
//lw PRINTKD
	  PRINTKD( "\nwrite fifo: count=%d UDCCS0=%x UDCCS0=%x\n", count, udccs0, UDCCS0);
	}
    }
  /* something goes poopy if I dont wait here ... */
  /* wait the tranfer to be finished. */
  udelay(1500);
//lw
  PRINTKD( "\nwrite fifo: bytes sent=%d, bytes left=%d\n", bytes_written, wr.bytes_left);
}

/*
 * read_fifo()
 * Read bytes out of FIFO and put in request.
 * Called to do the initial read of setup requests
 * from the host. Return number of bytes read.
 *
 */
static int read_fifo( void* data, int length )
{
  int bytes_read = 0;
  unsigned char * pOut = (unsigned char*) data;

  int udccs0 = UDCCS0;

 // printk("read_fifo: length=%d\n",length);
  if( (udccs0 & SETUP_READY) == SETUP_READY)
    {
      /* ok it's a setup command */
      while( UDCCS0 & UDCCS0_RNE)
	{
 	  //printk("read_fifo: bytes_read=%d\n",bytes_read);
	  if( bytes_read >= length )
	    {
				/* We've already read enought o fill usb_dev_request_t.
				 * Our tummy is full. Go barf... 
				 */
	      printk( "%sread_fifo(): read failure\n", pszMe );
	      usbd_info.stats.ep0_fifo_read_failures++;
	      break;
	    }

	  *pOut++ = UDDR0;
	  bytes_read++;
	}
    }
  PRINTKD( "read_fifo %d bytes\n", bytes_read );

  /* clear SA & OPR */
  UDCCS0 = SETUP_READY;

  usbd_info.stats.ep0_bytes_read += bytes_read;
  return bytes_read;
}

/***by Levis. for reading ep0 data***/
static int read_fifo_data( void* data, int length )
{
  int bytes_read = 0;
  unsigned char * pOut;
  int udccs0 = UDCCS0;
  
  if(read_ep0 == 0)
  {
 	pOut=(unsigned char*) data;
  }else{
		pOut=(unsigned char*)((char*) data+read_ep0);
	}

  if( (udccs0 & UDCCS0_OPR) == UDCCS0_OPR)
    {
      /* ok it's a ep0 data*/
      PRINTKD("ep0 data received");
      while( UDCCS0 & UDCCS0_RNE)
	{
 	  if( bytes_read >= length )
	    {
				/* We've already read enought o fill usb_dev_request_t.
				 * Our tummy is full. Go barf... 
				 */
	      printk( "read_fifo_data(): read failure\n" );
	      usbd_info.stats.ep0_fifo_read_failures++;
	      break;
	    }

	  *pOut++ = UDDR0;
	  bytes_read++;
	}
    }
  PRINTKD( "read_fifo_data %d bytes\n", bytes_read );

  /* clear SA & OPR */
  UDCCS0 = UDCCS0_OPR;

  usbd_info.stats.ep0_bytes_read += bytes_read;
/*	printk("\nread_ep0=%d\n",read_ep0);
	printk("\ndata_len=%d\n",data_len);
	printk("\nlength=%d\n",length);
	if(read_ep0==length)
	{
		specific_message(&intf8,data,data_len);
	}*/
	send_zero_packet(NULL);
  return bytes_read;
}

void send_zero_packet( void *data )
{
	UDDR0 = data;
	current_handler = sh_setup_begin;
	UDCCS0 = UDCCS0_IPR;
	if( ( UDCCS0 & UDCCS0_OPR ) )
	{ 
		UDCCS0 = UDCCS0_OPR;
	}
	udelay(10);
	
	PRINTKD("\n Send zero packet successfully!\n");
}

/*
 * set_descriptor()
 * Called from ep0_process_standard_request to handle 
 * data return for a SET_DESCRIPTOR setup request.
 */
static void set_descriptor( usb_dev_request_t * pReq )
{
  /* TODO */
}

/*
 * set_interface()
 * Called from ep0_process_standard_request to handle 
 * data return for a SET_INTERFACE setup request.
 *
 * This command still are no use in new devices for they use ep0 communication.
 *
 */
static void set_interface( usb_dev_request_t * pReq )
{
  int alt_setting = pReq->wValue;
  int interface = pReq->wIndex;

  /* only two interfaces */
  if ( interface<2 ) {
    if ( alt_setting==0 ) {
      active_interface = interface;
      active_alt_setting = alt_setting;
    } else {
      printk("%sunknown interface %d alt_setting %d stall\n", pszMe, interface, alt_setting );
    }
  } else {
    printk("%sunknown interface %d stall\n", pszMe, interface );
  }
}

/*
 * get_descriptor()
 * Called from sh_setup_begin to handle data return
 * for a GET_DESCRIPTOR setup request.
 */
static void get_descriptor( usb_dev_request_t * pReq )
{
  string_desc_t * pString = NULL;
  intf_desc_t* pInterface = NULL;
  ep_desc_t * pEndpoint = NULL;

  desc_t * pDesc = pxa_usb_get_descriptor_ptr();
  int type = pReq->wValue >> 8;
  int idx  = pReq->wValue & 0xFF;

  switch( type ) {
  case USB_DESC_DEVICE:
    queue_and_start_write( &pDesc->dev, pReq->wLength, pDesc->dev.bLength );
    break;

  case USB_DESC_CONFIG:
    queue_and_start_write( &pDesc->b, pReq->wLength, sizeof( struct cdb ) );
    break;

  case USB_DESC_STRING:
    pString = pxa_usb_get_string_descriptor( idx );
    if ( pString ) {
      if ( idx != 0 ) {  // if not language index
//	printk( "%sReturn string %d: ", pszMe, idx );
//	psdesc( pString );
      }

      queue_and_start_write( pString, pReq->wLength, pString->bLength );
    }
    else {
      printk("%sunkown string index %d Stall.\n", pszMe, idx );
      /* may be a bug */
      queue_and_start_write( NULL, 0, 0 );
    }
    break;
	
  case USB_DESC_INTERFACE:  /* only two interface 0 and 1 */
    switch (idx) {
    case 0:
      pInterface = &pDesc->b.interface6;
      break;
    case 1:
      pInterface = &pDesc->b.interface8;
      break;
    default:
      pInterface = NULL;
      break;
    }
		  
    if ( pInterface ) 
      queue_and_start_write( pInterface, pReq->wLength, pInterface->bLength );
    else
      printk("%sunknown interface index %d stall.\n", pszMe, idx );
  
    break;

  case USB_DESC_ENDPOINT:
    if ( idx == 1 )
      pEndpoint = &pDesc->b.ep1; //[BULK_IN1];
    else
      pEndpoint = NULL;

    if ( pEndpoint ) {
      queue_and_start_write( pEndpoint, pReq->wLength, pEndpoint->bLength );
    } else {
      printk("%sunknown endpoint index %d Stall.\n", pszMe, idx );
    }
    break;

  default :
    printk("%sunknown descriptor type %d. Stall.\n", pszMe, type );
    break;
  }
}

static void send_status_packet( int status )
{
  /* TODO */
}

static void wait_status_packet( void )
{
  /* TODO */
}

EXPORT_SYMBOL( intf6 );
EXPORT_SYMBOL( intf8 );
EXPORT_SYMBOL( wq_read6 );
EXPORT_SYMBOL( wq_read8 );
EXPORT_SYMBOL( polling_times6 );
EXPORT_SYMBOL( polling_times8 );
EXPORT_SYMBOL( polling_len );
EXPORT_SYMBOL( specific_times6 );
EXPORT_SYMBOL( specific_times8 );
/* end usb_ep0.c - who needs this comment? */
