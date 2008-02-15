/*
 *  Copyright (C) Compaq Computer Corporation, 1998, 1999
 *  Copyright (C) Extenex Corporation, 2001
 *  Copyright (C) Intrinsyc, Inc., 2002
 *
 *  PXA USB controller core driver.
 *
 *  This file provides interrupt routing and overall coordination
 *  of the endpoints.
 *
 *  Please see:
 *    linux/Documentation/arm/SA1100/SA1100_USB 
 *  for more info.
 *
 *  02-May-2002
 *   Frank Becker (Intrinsyc) - derived from sa1100 usb_ctl.c
 *  02-Oct-2002
 *   Levis Xu (Motorola) - updated for Motorola EZX Phone.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/tqueue.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/arch-pxa/pxa-regs.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include "pxa_usb.h"
#include "usb_ctl.h"
#include "usb_ep0.h"

//#define DEBUG 1

#if 0

/***add by Levis***/
//     GPIO   
#define GPDRx      __REG( 0x40E0000C )
#define GPDRy      __REG( 0x40E00010 )          // GPIO pin direction register GPIO<63:32>
#define GPSRx      __REG( 0x40E00018 )  	/* GPIO Pin Output Set Register GPIO<31:0> */
#define GPSRy      __REG( 0x40E0001C )          // GPIO pin output set register GPIO<63:32>
#define GPCRx      __REG( 0x40E00024 )          // GPIO pin output clear register GPIO<31:0>
#define GPCRy      __REG( 0x40E00028 )          // GPIO pin output clear register GPIO <63:32>
#define GAFR0x     __REG( 0x40E00054 )          // GPIO alternate function select register GPIO<15:0>
#define GAFR1x     __REG( 0x40E00058 )          // GPIO alternate function select register GPIO<31:16>
#define GAFR0y     __REG( 0x40E0005C )          // GPIO alternate function select register GPIO<47:32>
#define GAFR1y     __REG( 0x40E00060 )          // GPIO alternate function select register GPIO<63:48>

#endif

#if DEBUG
static unsigned int usb_debug = DEBUG;
#else
#define usb_debug 0     /* gcc will remove all the debug code for us */
#endif

//////////////////////////////////////////////////////////////////////////////
// Prototypes
//////////////////////////////////////////////////////////////////////////////

int usbctl_next_state_on_event( int event );
static void udc_int_hndlr(int, void *, struct pt_regs *);
static void initialize_descriptors( void );
static void soft_connect_hook( int enable );
static void udc_disable(void);
static void udc_enable(void);

void usb_reset_for_cfg8(int enable);

#if CONFIG_PROC_FS
#define PROC_NODE_NAME "driver/pxausb"
static int usbctl_read_proc(char *page, char **start, off_t off,
			    int count, int *eof, void *data);
#endif

//////////////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////////////
static const char pszMe[] = "usbctl: ";
struct usb_info_t usbd_info;  /* global to ep0, usb_recv, usb_send */

/* device descriptors */
static desc_t desc;

#define MAX_STRING_DESC 9
static string_desc_t * string_desc_array[ MAX_STRING_DESC ];

// called when configured
static usb_notify_t configured_callback = NULL;

enum {
    kStateZombie		= 0,
    kStateZombieSuspend		= 1,
    kStateDefault		= 2,
    kStateDefaultSuspend	= 3,
    kStateAddr			= 4,
    kStateAddrSuspend		= 5,
    kStateConfig		= 6,
    kStateConfigSuspend		= 7
};

/*
 * FIXME: The PXA UDC handles several host device requests without user 
 * notification/intervention. The table could be collapsed quite a bit...
 */
/* default --> configured ??? */
static int device_state_machine[8][6] = {
//              suspend               reset          resume         adddr       config        deconfig
/* zombie */  { kStateZombieSuspend , kStateDefault, kStateZombie , kError    , kError      , kError },
/* zom sus */ { kStateZombieSuspend , kStateDefault, kStateZombie , kError    , kError      , kError },
/* default */ { kStateDefaultSuspend, kStateDefault, kStateDefault, kStateAddr, kStateConfig, kError },
/* def sus */ { kStateDefaultSuspend, kStateDefault, kStateDefault, kError    , kError      , kError },
/* addr */    { kStateAddrSuspend   , kStateDefault, kStateAddr   , kError    , kStateConfig, kError },
/* addr sus */{ kStateAddrSuspend   , kStateDefault, kStateAddr   , kError    , kError      , kError },
/* config */  { kStateConfigSuspend , kStateDefault, kStateConfig , kError    , kError      , kStateDefault },
/* cfg sus */ { kStateConfigSuspend , kStateDefault, kStateConfig , kError    , kError      , kError }
};

/* "device state" is the usb device framework state, as opposed to the
   "state machine state" which is whatever the driver needs and is much
   more fine grained
*/
static int sm_state_to_device_state[8] = { 
//  zombie            zom suspend       
USB_STATE_POWERED, USB_STATE_SUSPENDED, 
//  default           default sus
USB_STATE_DEFAULT, USB_STATE_SUSPENDED,
//  addr              addr sus         
USB_STATE_ADDRESS, USB_STATE_SUSPENDED, 
//  config            config sus
USB_STATE_CONFIGURED, USB_STATE_SUSPENDED
};

static char * state_names[8] =
{ "zombie", "zombie suspended", 
  "default", "default suspended",
  "address", "address suspended", 
  "configured", "config suspended"
};

static char * event_names[6] =
{ "suspend", "reset", "resume",
  "address assigned", "configure", "de-configure"
};

static char * device_state_names[] =
{ "not attached", "attached", "powered", "default",
  "address", "configured", "suspended" };

static int sm_state = kStateZombie;

//////////////////////////////////////////////////////////////////////////////
// Async
//////////////////////////////////////////////////////////////////////////////

/* The UDCCR reg contains mask and interrupt status bits,
 * so using '|=' isn't safe as it may ack an interrupt.
 */

void udc_set_mask_UDCCR( int mask )
{
	UDCCR = (UDCCR & UDCCR_MASK_BITS) | (mask & UDCCR_MASK_BITS);
}

void udc_clear_mask_UDCCR( int mask)
{
	UDCCR = (UDCCR & UDCCR_MASK_BITS) & ~(mask & UDCCR_MASK_BITS);
}

void udc_ack_int_UDCCR( int mask)
{
	/* udccr contains the bits we dont want to change */
	__u32 udccr = UDCCR & UDCCR_MASK_BITS; 

	UDCCR = udccr | (mask & ~UDCCR_MASK_BITS);
}

static void
udc_int_hndlr(int irq, void *dev_id, struct pt_regs *regs)
{
  	__u32 status = UDCCR;
  	__u32 ir0_status = USIR0;
  	__u32 ir1_status = USIR1;
  	__u32 uicr0 = UICR0;
  	__u32 uicr1 = UICR1;

	//mask ints
	udc_set_mask_UDCCR( UDCCR_REM | UDCCR_SRM);
	UICR0 = 0xff;
	UICR1 = 0xff;

	if( usb_debug > 2)
	{
		printk("%s--- udc_int_hndlr\n"
		   "UDCCR=0x%08x UDCCS0=0x%08x UDCCS1=0x%08x UDCCS2=0x%08x\n"
		   "USIR0=0x%08x USIR1=0x%08x UICR0=0x%08x UICR1=0x%08x\n", 
		    pszMe, status, UDCCS0, UDCCS1, UDCCS2, ir0_status, ir1_status, uicr0, uicr1);
	}

	/* SUSpend Interrupt Request */
	if ( status & UDCCR_SUSIR )
	{
		udc_ack_int_UDCCR( UDCCR_SUSIR);
		if( usb_debug) printk("%sSuspend...\n", pszMe);
		usbctl_next_state_on_event( kEvSuspend );
	}

	/* RESume Interrupt Request */
	if ( status & UDCCR_RESIR )
	{
		udc_ack_int_UDCCR( UDCCR_RESIR);
		if( usb_debug) printk("%sResume...\n", pszMe);
		usbctl_next_state_on_event( kEvResume );
	}

	/* ReSeT Interrupt Request - UDC has been reset */
	if ( status & UDCCR_RSTIR )
	{
		/* clear the reset interrupt */
		udc_ack_int_UDCCR( UDCCR_RSTIR);

		/* check type of reset */
		if( (UDCCR & UDCCR_UDA) == 0)
		{
			/* reset assertion took place, nothing to do */
			if( usb_debug) printk("%sReset assertion...\n", pszMe);
		}
		/* ok, it's a reset negation, go on with reset */
		else if ( usbctl_next_state_on_event( kEvReset ) != kError )
		{
		        /* Here we will reset to another configuration */
			/* starting reset sequence now... */
			if( usb_debug) printk("%sResetting\n", pszMe);

			ep0_reset();
			ep_bulk_in1_reset();
			/*
			  ep_bulk_out1_reset();
			*/

			usbctl_next_state_on_event( kEvConfig );
		}
		else
		{
			printk("%sUnexpected reset\n", pszMe);
		}
	}
	else
	{
		/* ep0 int */
		if (ir0_status & USIR0_IR0)
			ep0_int_hndlr();

		/* transmit bulk */
		if (ir0_status & USIR0_IR1)
			ep_bulk_in1_int_hndlr(ir0_status);

		/* receive bulk */
		/* in new device driver, we don't need EP2(BULK OUT) */
		/*
		if ( ir0_status & USIR0_IR2)
			ep_bulk_out1_int_hndlr(ir0_status);
		*/

		while (UDCCS2 & UDCCS_BO_RNE)
		{
			if( usb_debug) printk("More Bulk-out data...\n");
			ep_bulk_out1_int_hndlr(ir0_status);
		}
	}

	UICR0 = uicr0;
	UICR1 = uicr1;
	udc_clear_mask_UDCCR( UDCCR_SRM | UDCCR_REM); /* enable suspend/resume, reset */

	/* clear all endpoint ints */
	USIR0 |= 0xff;
	USIR1 |= 0xff;

	if( usb_debug > 2)
	{
		printk("%sudc_int_hndlr\n"
		       "UDCCR=0x%08x UDCCS0=0x%08x UDCCS1=0x%08x UDCCS2=0x%08x\n"
		       "USIR0=0x%08x USIR1=0x%08x UICR0=0x%08x UICR1=0x%08x\n", 
			pszMe, UDCCR, UDCCS0, UDCCS1, UDCCS2, USIR0, USIR1, UICR0, UICR1);
	}
}

//////////////////////////////////////////////////////////////////////////////
// Public Interface
//////////////////////////////////////////////////////////////////////////////

/* Open PXA usb core on behalf of a client, but don't start running */

int
pxa_usb_open( const char * client )
{
	int i;

	printk( "%s%s start.\n", pszMe, client );

	if ( usbd_info.client_name != NULL )
	{
		printk( "%sUnable to register %s (%s already registered).\n", 
			pszMe, client, usbd_info.client_name );
		return -EBUSY;
	}

	usbd_info.client_name = (char*) client;
	memset(&usbd_info.stats, 0, sizeof(struct usb_stats_t));
	for ( i=0 ; i<MAX_STRING_DESC ; i++ )
		string_desc_array[i] = NULL;


	/* hack to start in zombie suspended state */
	sm_state = kStateZombieSuspend;
	usbd_info.state = USB_STATE_SUSPENDED;

	/* create descriptors for enumeration */
	initialize_descriptors();

	printk( "%s%s registered.\n", pszMe, client );
	return 0;
}

/* Start running. Must have called usb_open (above) first */
int
pxa_usb_start( void )
{
	if ( usbd_info.client_name == NULL ) {
		printk( "%s%s - no client registered\n",
				pszMe, __FUNCTION__ );
		return -EPERM;
	}

	/* start UDC internal machinery running */
	udc_enable();
	udelay( 100 );

	/* flush DMA and fire through some -EAGAINs */
	/*
	ep_bulk_out1_init( usbd_info.dmach_rx );
	*/
	ep_bulk_in1_init( usbd_info.dmach_tx );

	/* give endpoint notification we are starting */
	/*
	ep_bulk_out1_state_change_notify( USB_STATE_SUSPENDED );
	*/
	ep_bulk_in1_state_change_notify( USB_STATE_SUSPENDED );

	/* enable any platform specific hardware */
	soft_connect_hook( 1 );

	/* enable suspend/resume, reset */
	udc_clear_mask_UDCCR( UDCCR_SRM | UDCCR_REM); 
	/* enable ep0, ep1, ep2 */
	/*
	UICR0 &= ~(UICR0_IM0 | UICR0_IM1 | UICR0_IM2); 
	*/
	UICR0 &= ~(UICR0_IM0 | UICR0_IM1 );

	if( usb_debug) printk( "%sStarted %s\n", pszMe, usbd_info.client_name );
	return 0;
}

/* Stop USB core from running */
int
pxa_usb_stop( void )
{
	if ( usbd_info.client_name == NULL ) {
		printk( "%s%s - no client registered\n",
				pszMe, __FUNCTION__ );
		return -EPERM;
	}
	/* mask everything */
	/* disable suspend/resume, reset */
	udc_set_mask_UDCCR( UDCCR_SRM | UDCCR_REM); 
	/* disable ep0, ep1, ep2 */
	/*
	UICR0 |= (UICR0_IM0 | UICR0_IM1 | UICR0_IM2); 
	*/
	UICR0 |= (UICR0_IM0 | UICR0_IM1 );

	/*
	ep_bulk_out1_reset();
	*/
	ep_bulk_in1_reset();

	udc_disable();
	if( usb_debug) printk( "%sStopped %s\n", pszMe, usbd_info.client_name );
	return 0;
}

/* Tell PXA core client is through using it */
int
pxa_usb_close( void )
{
	 if ( usbd_info.client_name == NULL ) {
		  printk( "%s%s - no client registered\n",
				  pszMe, __FUNCTION__ );
		  return -EPERM;
	 }
	 printk( "%s%s closed.\n", pszMe, (char*)usbd_info.client_name );
	 usbd_info.client_name = NULL;
	 return 0;
}

/* set a proc to be called when device is configured */
usb_notify_t pxa_set_configured_callback( usb_notify_t func )
{
	 usb_notify_t retval = configured_callback;
	 configured_callback = func;
	 return retval;
}

/*====================================================
 * Descriptor Manipulation.
 * Use these between open() and start() above to setup
 * the descriptors for your device.
 *
 */

/* get pointer to static default descriptor */
desc_t *
pxa_usb_get_descriptor_ptr( void ) { return &desc; }

/* optional: set a string descriptor */
int
pxa_usb_set_string_descriptor( int i, string_desc_t * p )
{
	 int retval;
	 if ( i < MAX_STRING_DESC ) {
		  string_desc_array[i] = p;
		  retval = 0;
	 } else {
		  retval = -EINVAL;
	 }
	 return retval;
}

/* optional: get a previously set string descriptor */
string_desc_t *
pxa_usb_get_string_descriptor( int i )
{
	 return ( i < MAX_STRING_DESC )
		    ? string_desc_array[i]
		    : NULL;
}

/* optional: kmalloc and unicode up a string descriptor */
string_desc_t *
pxa_usb_kmalloc_string_descriptor( const char * p )
{
	 string_desc_t * pResult = NULL;

	 if ( p ) {

		  int len = strlen( p );
		  int uni_len = len * sizeof( __u16 );
		  pResult = (string_desc_t*) kmalloc( uni_len + 2, GFP_KERNEL ); /* ugh! */
		  if ( pResult != NULL ) {
			   int i;
			   pResult->bLength = uni_len + 2;
			   pResult->bDescriptorType = USB_DESC_STRING;
			   for( i = 0; i < len ; i++ ) {
					pResult->bString[i] = make_word( (__u16) p[i] );
			   }
		  }
	 }
	 return pResult;
}

//////////////////////////////////////////////////////////////////////////////
// Exports to rest of driver
//////////////////////////////////////////////////////////////////////////////

/* called by the int handler here and the two endpoint files when interesting
   .."events" happen */

int
usbctl_next_state_on_event( int event )
{
	int next_state = device_state_machine[ sm_state ][ event ];
	if ( next_state != kError )
	{
		int next_device_state = sm_state_to_device_state[ next_state ];
		if( usb_debug) printk( "%s%s --> [%s] --> %s. Device in %s state.\n",
				pszMe, state_names[ sm_state ], event_names[ event ],
				state_names[ next_state ], device_state_names[ next_device_state ] );

		sm_state = next_state;
		if ( usbd_info.state != next_device_state )
		{
			if ( configured_callback != NULL
				 &&
				 next_device_state == USB_STATE_CONFIGURED
				 &&
				 usbd_info.state != USB_STATE_SUSPENDED
			   ) {
			  configured_callback();
			}
			usbd_info.state = next_device_state;

			ep_bulk_out1_state_change_notify( next_device_state );
			ep_bulk_in1_state_change_notify( next_device_state );
		}
	}
	/* This is just for debug */
#if 1
	else
		printk( "%s%s --> [%s] --> ??? is an error.\n",
				pszMe, state_names[ sm_state ], event_names[ event ] );
#endif
	return next_state;
}

//////////////////////////////////////////////////////////////////////////////
// Private Helpers
//////////////////////////////////////////////////////////////////////////////

/* If we did all the functions for Motorola, do we need try to do so greatful? */
/*
 * string descriptor array
 * 0 -- Language ID
 * 1 -- manufacturer description
 * 2 -- product description
 * 3 -- production serial number
 * 4 -- Configuration description
 * 5 -- Interface 6 description
 * 6 -- Interface 8 description
 * 7 -- endpoint 1 description
 */
char* dstr_array[] = { 
  NULL, 
  "Motorola Inc.", 
  "Motorola Ezx Phone", 
  "test", 
  "MCU Logging+test cmd+accy", 
  "Motorola MCU Data Logger", 
  "Motorola Test Command", 
  "Motorola Accessory",
  "Motorola Full configuration",
  "Bulk In" 
};

static void 
string_desc_setup( void )
{
        int len,i,j;

	string_desc_array[0] = kmalloc( sizeof( string_desc_t ), GFP_KERNEL );
	if ( !string_desc_array[0] ) {
	  printk("string_desc_setup() run out of memory!\n");
	  return;
	}
	string_desc_array[0]->bDescriptorType = USB_DESC_STRING;
	string_desc_array[0]->bLength = sizeof( string_desc_t );
	string_desc_array[0]->bString[0] = make_word_c( 0x409 );

	for( i=1; i<sizeof(dstr_array)/sizeof(string_desc_t*); i++ ) {
	  len = sizeof(string_desc_t)+strlen(dstr_array[i])*2-2; //by Levis
	  string_desc_array[i] = kmalloc( len, GFP_KERNEL ); /* this is right */
	  if ( string_desc_array[i] ) {
	    string_desc_array[i]->bDescriptorType = USB_DESC_STRING;
	    string_desc_array[i]->bLength = len;
	    for ( j=0; j<strlen(dstr_array[i]); j++ ) {
	      string_desc_array[i]->bString[j] = make_word( (__u16)dstr_array[i][j] );
	    }
	  }
	}	  
}

/* setup default descriptors */
static void
initialize_descriptors(void)
{
	desc.dev.bLength               = sizeof( device_desc_t );
	desc.dev.bDescriptorType       = USB_DESC_DEVICE;
	desc.dev.bcdUSB                = make_word_c(0x0110);
	desc.dev.bDeviceClass          = 0xFF;	/* vendor specific */
	desc.dev.bDeviceSubClass       = 0;
	desc.dev.bDeviceProtocol       = 0;
	desc.dev.bMaxPacketSize0       = 16;	/* ep0 max fifo size */
	desc.dev.idVendor              = make_word_c(0x22B8);	/* vendor ID undefined */
	desc.dev.idProduct             = make_word_c(0x3801); /* product */
	desc.dev.bcdDevice             = make_word_c(0x0001); /* vendor assigned device release num */
	desc.dev.iManufacturer         = 1;	/* index of manufacturer string */
	desc.dev.iProduct              = 2; /* index of product description string */
	desc.dev.iSerialNumber         = 0;	/* index of string holding product s/n */
	desc.dev.bNumConfigurations    = 1;

	desc.b.cfg.bLength             = sizeof( config_desc_t );
	desc.b.cfg.bDescriptorType     = USB_DESC_CONFIG;
	desc.b.cfg.wTotalLength        = make_word_c( sizeof(struct cdb) );
	desc.b.cfg.bNumInterfaces      = 3;
	desc.b.cfg.bConfigurationValue = 1;
	desc.b.cfg.iConfiguration      = 4;
	desc.b.cfg.bmAttributes        = USB_CONFIG_SELFPOWERED;
	desc.b.cfg.MaxPower            = USB_POWER( 500 );//0;

	desc.b.interface5.bLength            = sizeof( intf_desc_t );
	desc.b.interface5.bDescriptorType    = USB_DESC_INTERFACE;
	desc.b.interface5.bInterfaceNumber   = 5; /* unique intf index*/
	desc.b.interface5.bAlternateSetting  = 0;
	desc.b.interface5.bNumEndpoints      = 0;
	desc.b.interface5.bInterfaceClass    = 0xFF; /* vendor specific */
	desc.b.interface5.bInterfaceSubClass = 0x02;
	desc.b.interface5.bInterfaceProtocol = 0xff;
	desc.b.interface5.iInterface         = 0x07;
	
	desc.b.interface6.bLength            = sizeof( intf_desc_t );
	desc.b.interface6.bDescriptorType    = USB_DESC_INTERFACE;
	desc.b.interface6.bInterfaceNumber   = 6; /* unique intf index*/
	desc.b.interface6.bAlternateSetting  = 0;
	desc.b.interface6.bNumEndpoints      = 1;
	desc.b.interface6.bInterfaceClass    = 0xFF; /* vendor specific */
	desc.b.interface6.bInterfaceSubClass = 0x02;
	desc.b.interface6.bInterfaceProtocol = 0xff;
	desc.b.interface6.iInterface         = 0x05;

	desc.b.interface8.bLength            = sizeof( intf_desc_t );
	desc.b.interface8.bDescriptorType    = USB_DESC_INTERFACE;
	desc.b.interface8.bInterfaceNumber   = 8; /* unique intf index*/
	desc.b.interface8.bAlternateSetting  = 0;
	desc.b.interface8.bNumEndpoints      = 0;
	desc.b.interface8.bInterfaceClass    = 0xFF; /* vendor specific */
	desc.b.interface8.bInterfaceSubClass = 0x02;
	desc.b.interface8.bInterfaceProtocol = 0xff;
	desc.b.interface8.iInterface         = 0x06;

/*
 * On the first phase, we only need ep1 to upload the log data.( Maybe )
 */
	desc.b.ep1.bLength             = sizeof( ep_desc_t );
	desc.b.ep1.bDescriptorType     = USB_DESC_ENDPOINT;
	desc.b.ep1.bEndpointAddress    = USB_EP_ADDRESS( 1, USB_IN );
	desc.b.ep1.bmAttributes        = USB_EP_BULK;
	desc.b.ep1.wMaxPacketSize      = make_word_c( 64 );
	desc.b.ep1.bInterval           = 0;

	string_desc_setup( );
}

/* soft_connect_hook()
 * Some devices have platform-specific circuitry to make USB
 * not seem to be plugged in, even when it is. This allows
 * software to control when a device 'appears' on the USB bus
 * (after Linux has booted and this driver has loaded, for
 * example). If you have such a circuit, control it here.
 */
static void
soft_connect_hook( int enable )
{
}

/* disable the UDC at the source */
static void
udc_disable(void)
{
	soft_connect_hook( 0 );
	/* clear UDC-enable */
	udc_clear_mask_UDCCR( UDCCR_UDE); 

        /* Disable clock for USB device */
        CKEN &= ~CKEN11_USB;
}


/*  enable the udc at the source */
static void
udc_enable(void)
{
	ulong irqs;

	local_irq_save( irqs );
        /* Enable clock for USB device */
        CKEN |= CKEN11_USB;

	/* try to clear these bits before we enable the udc */
	udc_ack_int_UDCCR( UDCCR_SUSIR);
	udc_ack_int_UDCCR( UDCCR_RSTIR);
	udc_ack_int_UDCCR( UDCCR_RESIR);

	/* set UDC-enable */
	udc_set_mask_UDCCR( UDCCR_UDE); 
	if( (UDCCR & UDCCR_UDA) == 0)
	{
		/* There's a reset on the bus,
		 * clear the interrupt bit and keep going
		 */
		udc_ack_int_UDCCR( UDCCR_RSTIR);
	}

	local_irq_restore( irqs );
	
	/* "USB test mode" to work around errata 40-42 (stepping a0, a1) 
	 * which could result in missing packets and interrupts. 
	 * Supposedly this turns off double buffering for all endpoints.
	 */
	if( usb_debug) printk( "USB RES1=%x RES2=%x RES3=%x\n", UDC_RES1, UDC_RES2, UDC_RES3);
	UDC_RES1 = 0x00;
	UDC_RES2 = 0x00;
	if( usb_debug) printk( "USB RES1=%x RES2=%x RES3=%x\n", UDC_RES1, UDC_RES2, UDC_RES3);
}

//////////////////////////////////////////////////////////////////////////////
// Proc Filesystem Support
//////////////////////////////////////////////////////////////////////////////

#if CONFIG_PROC_FS

#define SAY( fmt, args... )  p += sprintf(p, fmt, ## args )
#define SAYV(  num )         p += sprintf(p, num_fmt, "Value", num )
#define SAYC( label, yn )    p += sprintf(p, yn_fmt, label, yn )
#define SAYS( label, v )     p += sprintf(p, cnt_fmt, label, v )

static int usbctl_read_proc(char *page, char **start, off_t off,
			    int count, int *eof, void *data)
{
	 const char * num_fmt   = "%25.25s: %8.8lX\n";
	 const char * cnt_fmt   = "%25.25s: %lu\n";
	 const char * yn_fmt    = "%25.25s: %s\n";
	 const char * yes       = "YES";
	 const char * no        = "NO";
	 unsigned long v;
	 char * p = page;
	 int len;

 	 SAY( "PXA USB Controller Core\n" );
 	 SAY( "Active Client: %s\n", usbd_info.client_name ? usbd_info.client_name : "none");
	 SAY( "USB state: %s (%s) %d\n",
		  device_state_names[ sm_state_to_device_state[ sm_state ] ],
		  state_names[ sm_state ],
		  sm_state );

	 SAYS( "ep0 bytes read", usbd_info.stats.ep0_bytes_read );
	 SAYS( "ep0 bytes written", usbd_info.stats.ep0_bytes_written );
	 SAYS( "ep0 FIFO read failures", usbd_info.stats.ep0_fifo_write_failures );
	 SAYS( "ep0 FIFO write failures", usbd_info.stats.ep0_fifo_write_failures );

	 SAY( "\n" );

	 v = UDCCR;
	 SAY( "\nUDC Control Register\n" );
	 SAYV( v );
	 SAYC( "UDC Enabled",                ( v & UDCCR_UDE ) ? yes : no );
	 SAYC( "UDC Active",                ( v & UDCCR_UDA ) ? yes : no );
	 SAYC( "Suspend/Resume interrupts masked", ( v & UDCCR_SRM ) ? yes : no );
	 SAYC( "Reset interrupts masked",   ( v & UDCCR_REM ) ? yes : no );
	 SAYC( "Reset pending",      ( v & UDCCR_RSTIR ) ? yes : no );
	 SAYC( "Suspend pending",    ( v & UDCCR_SUSIR ) ? yes : no );
	 SAYC( "Resume pending",     ( v & UDCCR_RESIR ) ? yes : no );

	 len = ( p - page ) - off;
	 if ( len < 0 )
		  len = 0;
	 *eof = ( len <=count ) ? 1 : 0;
	 *start = page + off;
	 return len;
}

#endif  /* CONFIG_PROC_FS */

#if 0
static void irq_handler(int channel, void *data, struct pt_regs *regs)
{
	if( channel == usbd_info.dmach_rx)
	{
	    printk( "USB receive DMA\n");
	}
	else if( channel == usbd_info.dmach_tx)
	{
	    printk( "USB transmit DMA\n");
	}
	else
	{
	    printk( "USB unknown DMA channel\n");
	}
}
#endif
/***************USB reset for select config8 -Levis***************************/
void usb_reset_for_cfg8(int enable)
{
	int count;

	count = 1000;
	if(enable)
	{
		udc_disable();
		do{
			count--;
			udelay(1);
			}while(count);
			
		udc_enable();
		initialize_descriptors();
		udc_clear_mask_UDCCR( UDCCR_SRM | UDCCR_REM); 
		UICR0 &= ~(UICR0_IM0 | UICR0_IM1 );
	}
}

/***usb gpio setting by Levis***/
int usb_gpio_init(void)
{
	GPSR1 = 0x02000000;	//Set GPIO57 to 1,to set USB transceiver to receive mode
	GPCR1 = 0x01000000;	//Clear GPIO56 to 0, to set USB transceiver to normal mode

	GPDR0 &= 0xfffffdff;  	//Setup GPIO09 Input
	GPDR1 &= 0xfffffffa;		//Setup GPIO32/34 Input
	GPDR1 |= 0x03000080;		//Setup GPIO57/56/39 Output

	GAFR0_L = (GAFR0_L & 0xfff3ffff)|0x00040000; 	//GPIO09 to01
	GAFR1_L = (GAFR1_L & 0xffff3fcc)|0x0000c022;	//GPIO39/34/32 to 11/10/10
	GAFR1_U = (GAFR1_U & 0xfff0ffff)|0x00050000;	//GPIO57/56 to 01/01

    return 0;
}

int stop_usb(void)
{
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Module Initialization and Shutdown
//////////////////////////////////////////////////////////////////////////////
/*
 * usbctl_init()
 * Module load time. Allocate dma and interrupt resources. Setup /proc fs
 * entry. Leave UDC disabled.
 */
int __init usbctl_init( void )
{
	int retval = 0;
#if 0
	/***add by Levis. Initialize USB***/
	GPSRy = 0x02000000;	//Set GPIO57 to 1,to set USB transceiver to receive mode
	GPCRy = 0x01000000;	//Clear GPIO56 to 0, to set USB transceiver to normal mode

	GPDRx &= 0xfffffdff;  	//Setup GPIO09 Input
	GPDRy &= 0xfffffffa;		//Setup GPIO32/34 Input
	GPDRy |= 0x03000080;		//Setup GPIO57/56/39 Output

	GAFR0x = (GAFR0x & 0xfff3ffff)|0x00040000; 	//GPIO09 to01
	GAFR0y = (GAFR0y & 0xffff3fcc)|0x0000c022;	//GPIO39/34/32 to 11/10/10
	GAFR1y = (GAFR1y & 0xfff0ffff)|0x00050000;	//GPIO57/56 to 01/01
/**************************************************************************/
#endif

//      usb_gpio_init();
	udc_disable();

	memset( &usbd_info, 0, sizeof( usbd_info ) );

#if CONFIG_PROC_FS
	create_proc_read_entry ( PROC_NODE_NAME, 0, NULL, usbctl_read_proc, NULL);
#endif

#if 0
	/* setup rx dma */
	usbd_info.dmach_rx = pxa_request_dma("USB receive", DMA_PRIO_MEDIUM, irq_handler, 0 /*data; DMA_Ser0UDCRd*/);
	if (usbd_info.dmach_rx < 0) {
		printk("%sunable to register for rx dma rc=%d\n", pszMe, usbd_info.dmach_rx );
		goto err_rx_dma;
	}

	/* setup tx dma */
	usbd_info.dmach_tx = pxa_request_dma("USB receive", DMA_PRIO_MEDIUM, irq_handler, 0 /*data; DMA_Ser0UDCRd*/);
	if (usbd_info.dmach_tx < 0) {
 
		printk("%sunable to register for tx dma rc=%d\n",pszMe,usbd_info.dmach_tx);
		goto err_tx_dma;
	}
#endif

	/* now allocate the IRQ. */
	retval = request_irq(IRQ_USB, udc_int_hndlr, SA_INTERRUPT, "PXA USB core", NULL);
	if (retval) {
		printk("%sCouldn't request USB irq rc=%d\n",pszMe, retval);
		goto err_irq;
	}
/*
	udc_enable();
	initialize_descriptors();
	udc_clear_mask_UDCCR( UDCCR_SRM | UDCCR_REM); 
	UICR0 &= ~(UICR0_IM0 | UICR0_IM1 );
*/
	printk( "PXA USB Controller Core Initialized\n");
	return 0;

err_irq:
#if 0
	pxa_free_dma(usbd_info.dmach_tx);
	usbd_info.dmach_tx = 0;
err_tx_dma:
	pxa_free_dma(usbd_info.dmach_rx);
	usbd_info.dmach_rx = 0;
err_rx_dma:
#endif
	return retval;
}
/*
 * usbctl_exit()
 * Release DMA and interrupt resources
 */
void __exit usbctl_exit( void )
{
	printk("Unloading PXA USB Controller\n");

	udc_disable();

#if CONFIG_PROC_FS
	remove_proc_entry ( PROC_NODE_NAME, NULL);
#endif

#if 0
	pxa_free_dma(usbd_info.dmach_rx);
	pxa_free_dma(usbd_info.dmach_tx);
#endif
	free_irq(IRQ_USB, NULL);
}

module_init( usbctl_init );
module_exit( usbctl_exit );

EXPORT_SYMBOL( pxa_usb_open );
EXPORT_SYMBOL( pxa_usb_start );
EXPORT_SYMBOL( pxa_usb_stop );
EXPORT_SYMBOL( pxa_usb_close );
EXPORT_SYMBOL( pxa_usb_get_descriptor_ptr );
EXPORT_SYMBOL( usb_gpio_init );
EXPORT_SYMBOL( stop_usb );
/*
EXPORT_SYMBOL( pxa_usb_set_string_descriptor );
EXPORT_SYMBOL( pxa_usb_get_string_descriptor );
EXPORT_SYMBOL( pxa_usb_kmalloc_string_descriptor );
*/







