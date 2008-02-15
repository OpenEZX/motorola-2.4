/*
 *  Add unlink_urbs codes and do some updates of send out urb sequence
 *  by Levis(Levis@motorola.com) in 10/5/2004
 *
 *  Update Suspend/Resume codes by Levis(Levis@motorola.com) in 14/4/2004
 *
 *  Add Power Manager codes by Levis(Levis@motorola.com) in 20/2/2004
 * 
 *  usbipc.c created by Levis(Levis@motorola.com) 11/3/2003 
 *
 *  Copyright (c) 2003 Motoroal BJDC 
 *  This Program just implements a ipc driver based Intel's Bulverde USB Host 
 *  Controller.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/circ_buf.h>
#include <linux/usb.h>
//#include <linux/pm.h>
//#include <linux/apm_bios.h>

#include "ipcusb.h"

/*Macro defined for this driver*/
#define DRIVER_VERSION "0.3"
#define DRIVER_AUTHOR "Levis Xu <Levis@Motorola.com>"
#define DRIVER_DESC "USB IPC Driver"
#define MOTO_IPC_VID		0x22b8
#define MOTO_IPC_PID		0x3006
#define IBUF_SIZE 		32		/*urb size*/	
#define IPC_USB_XMIT_SIZE	1024
#define IPC_URB_SIZE		32	
#define IPC_USB_WRITE_INIT 	0
#define IPC_USB_WRITE_XMIT	1
#define IPC_USB_PROBE_READY	3
#define IPC_USB_PROBE_NOT_READY	4
#define DBG_MAX_BUF_SIZE	1024
#define ICL_EVENT_INTERVAL	(HZ) 
#undef BVD_DEBUG		

#define IS_EP_BULK(ep)  ((ep).bmAttributes == USB_ENDPOINT_XFER_BULK ? 1 : 0)
#define IS_EP_BULK_IN(ep) (IS_EP_BULK(ep) && ((ep).bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)
#define IS_EP_BULK_OUT(ep) (IS_EP_BULK(ep) && ((ep).bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)
/*End defined macro*/

/*global values defined*/
static struct usb_driver 		usb_ipc_driver;
static struct timer_list 		ipcusb_timer;
static struct timer_list 		suspend_timer;
static struct timer_list 		wakeup_timer;
static struct tty_struct		ipcusb_tty;		/* the coresponding tty struct, we just use flip buffer here. */
static struct tty_driver		ipcusb_tty_driver;	/* the coresponding tty driver, we just use write and chars in buff here*/
struct tty_driver *usb_for_mux_driver = NULL;
struct tty_struct *usb_for_mux_tty = NULL;
void (*usb_mux_dispatcher)(struct tty_struct *tty) = NULL;
void (*usb_mux_sender)(void) = NULL;
void (*ipcusb_ap_to_bp)(unsigned char*, int) = NULL;
void (*ipcusb_bp_to_ap)(unsigned char*, int) = NULL;
EXPORT_SYMBOL(usb_for_mux_driver);	
EXPORT_SYMBOL(usb_for_mux_tty);	
EXPORT_SYMBOL(usb_mux_dispatcher);	
EXPORT_SYMBOL(usb_mux_sender);
EXPORT_SYMBOL(ipcusb_ap_to_bp);
EXPORT_SYMBOL(ipcusb_bp_to_ap);
static int sumbit_times = 0; 	
static int callback_times = 0; 
//static unsigned long last_jiff = 0;
extern int usbh_finished_resume;
/*end global values defined*/

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#ifdef BVD_DEBUG
#define bvd_dbg(format, arg...) printk(__FILE__ ": " format "\n" , ## arg)
#else
#define bvd_dbg(format, arg...) do {} while (0)
#endif

/* USB device context */
typedef struct {
  struct list_head list;
  int size;
  char* body; 
} buf_list_t;

struct ipc_usb_data
{
	__u8 write_finished_flag;
	struct usb_device 		*ipc_dev;/* USB device handle */	
	struct urb 	 readurb_mux, writeurb_mux, writeurb_dsplog;	/* urbs */
	__u8	write_flag, ipc_flag, suspend_flag;
	char *obuf, *ibuf;
	int writesize;					/* max packet size for the output bulk endpoint */;						/* transfer buffers */
	struct circ_buf	xmit;				/* write cric bufffer */
  	struct list_head 	in_buf_list;
	char bulk_in_ep_mux, bulk_out_ep_mux, bulk_in_ep_dsplog;		/* Endpoint assignments */
	unsigned int 		ifnum;				/* Interface number of the USB device */
	struct tq_struct tqueue;	/*task queue for write urbs*/
	struct tq_struct tqueue_bp;	/*task queue for bp ready IN token*/

	spinlock_t lock;
}; 
struct ipc_usb_data *bvd_ipc;

#ifdef BVD_DEBUG
static void bvd_dbg_hex(__u8 *buf, int len)
{
	static unsigned char tbuf[ DBG_MAX_BUF_SIZE ];

	int i;
	int c;

	if( len <= 0 ) {
		return;
	}  

	c = 0;  
	for( i=0; (i < len) && (c < (DBG_MAX_BUF_SIZE - 3)); i++){
		sprintf(&tbuf[c], "%02x ",buf[i]);
		c += 3;
	}
	tbuf[c] = 0;

	printk("%s: %s\n", __FUNCTION__, tbuf);
}
#endif
static int unlink_urbs(struct urb *urb)
{	
	unsigned long	flags;
	int 	retval;

	spin_lock_irqsave(&bvd_ipc->lock, flags);
	
	retval = usb_unlink_urb (urb);
	if (retval != -EINPROGRESS && retval != 0)
	{
		printk("unlink urb err, %d", retval);
	}
	
	spin_unlock_irqrestore(&bvd_ipc->lock, flags);
	return retval;
}
static void append_to_inbuf_list(struct urb *urb)
{
	buf_list_t *inbuf;
	int count = urb->actual_length;
	
	inbuf = (buf_list_t*)kmalloc( sizeof(buf_list_t), GFP_KERNEL );
	if ( !inbuf ) {
		printk( "append_to_inbuf_list: (%d) out of memory!\n", sizeof(buf_list_t) );
		return;
	}
	
	inbuf->size = count;
	inbuf->body = (char*)kmalloc( sizeof(char)*count, GFP_KERNEL );
	if ( !(inbuf->body) ) 
	{
		kfree(inbuf);
		printk( "append_to_inbuf_list: (%d) out of memory!\n", sizeof(char)*count );
		return;
	}
	memcpy(inbuf->body, (unsigned char*)urb->transfer_buffer, count);
	list_add_tail( &inbuf->list, &bvd_ipc->in_buf_list);
}

static void ipcusb_timeout(unsigned long data)
{
	struct tty_struct *tty = &ipcusb_tty;	
	struct urb *urb;
	
	bvd_dbg("ipcusb_timeout***");

 	urb = (struct urb *)data;
	if(tty->flip.count == 0)
	{
		while(!(list_empty(&bvd_ipc->in_buf_list)))
		{
			int count;
			buf_list_t *inbuf;
			struct list_head* ptr=NULL;			
			
			ptr = bvd_ipc->in_buf_list.next;
			inbuf = list_entry (ptr, buf_list_t, list);			
			count = inbuf->size;
			if ((tty->flip.count + count) <= TTY_FLIPBUF_SIZE) 
			{
				memcpy(tty->flip.char_buf_ptr, inbuf->body, count);	
				tty->flip.count += count;	
				tty->flip.char_buf_ptr += count;  					

				list_del (ptr);
				kfree (inbuf->body);
				inbuf->body = NULL;
				kfree (inbuf);				
			} else{
				bvd_dbg("ipcusb_timeout: bvd_ipc->in_buf_list empty!");
				break;
			}
		}

		if (usb_mux_dispatcher)
		{
			usb_mux_dispatcher(tty);	/**call Liu changhui's func.**/	
		}
		
		if(list_empty(&bvd_ipc->in_buf_list))
		{
			urb->actual_length = 0;
			urb->dev = bvd_ipc->ipc_dev;
			if (usb_submit_urb(urb))
			{
				bvd_dbg("ipcusb_timeout: failed resubmitting read urb");
			}
			bvd_dbg("ipcusb_timeout: resubmited read urb");
		} else{				
			ipcusb_timer.data = (unsigned long)urb;
			mod_timer(&ipcusb_timer, jiffies+(10*HZ/1000));
		}
	} else{
		/***for test flow control***/
		if (usb_mux_dispatcher)
		{
			usb_mux_dispatcher(tty);	/**call Liu changhui's func.**/	
		}
		/***end test***/

		mod_timer(&ipcusb_timer, jiffies+(10*HZ/1000));
		bvd_dbg("ipcusb_timeout: add timer again!");
	}	
}

static void usb_ipc_read_bulk(struct urb *urb)
{	
	buf_list_t *inbuf;
	int count = urb->actual_length;
	struct tty_struct *tty = &ipcusb_tty;	
	
 	bvd_dbg("usb_ipc_read_bulk: begining!");
	if (urb->status)
	{
		printk("nonzero read bulk status received: %d\n", urb->status);
	}
	
 	bvd_dbg("usb_ipc_read_bulk: urb->actual_length=%d", urb->actual_length);
 	bvd_dbg("usb_ipc_read_bulk: urb->transfer_buffer:");	

#ifdef BVD_DEBUG 	
	bvd_dbg_hex((unsigned char*)urb->transfer_buffer, urb->actual_length);
#endif

	if( count>0 && ((*ipcusb_bp_to_ap) != NULL) )
	{
		(*ipcusb_bp_to_ap)(urb->transfer_buffer, urb->actual_length);
	}
 
 	if(!(list_empty(&bvd_ipc->in_buf_list)))	
	{
		int need_mux = 0;

 		bvd_dbg("usb_ipc_read_bulk: some urbs in_buf_list");	
		if(count > 0)
		{
			bvd_ipc->suspend_flag = 1;
			append_to_inbuf_list(urb);	/**append the current received urb**/	
#if 0		
			if(jiffies - last_jiff > ICL_EVENT_INTERVAL) 
			{
				last_jiff = jiffies;
				queue_apm_event(KRNL_ICL, NULL);
			}	
#endif
		}
		
		while(!(list_empty(&bvd_ipc->in_buf_list)))
		{
			struct list_head* ptr=NULL;	
			ptr = bvd_ipc->in_buf_list.next;
			inbuf = list_entry (ptr, buf_list_t, list);			
			count = inbuf->size;
			if ((tty->flip.count + count) <= TTY_FLIPBUF_SIZE) 
			{	
				need_mux = 1;
				memcpy(tty->flip.char_buf_ptr, inbuf->body, count);	
				tty->flip.count += count;	
				tty->flip.char_buf_ptr += count;  				

				list_del (ptr);
				kfree (inbuf->body);
				inbuf->body = NULL;
				kfree (inbuf);				
			} else{
				bvd_dbg("usb_ipc_read_bulk: bvd_ipc->in_buf_list empty!");
				break;
			}			
		}	

		if (usb_mux_dispatcher && need_mux) 
		{
			usb_mux_dispatcher(tty);	/**call Liu changhui's func.**/					
		}

		if(list_empty(&bvd_ipc->in_buf_list))
		{
			urb->actual_length = 0;
			urb->dev = bvd_ipc->ipc_dev;
			if (usb_submit_urb(urb))
			{
				bvd_dbg("usb_ipc_read_bulk: failed resubmitting read urb");
			}
			bvd_dbg("usb_ipc_read_bulk: resubmited read urb");
		} else{				
			ipcusb_timer.data = (unsigned long)urb;
			mod_timer(&ipcusb_timer, jiffies+(10*HZ/1000));
		}
	}else if ((count > 0) && ((tty->flip.count + count) <= TTY_FLIPBUF_SIZE)) {
 		bvd_dbg("usb_ipc_read_bulk: no urbs in_buf_list");	
		bvd_ipc->suspend_flag = 1;
		memcpy(tty->flip.char_buf_ptr, (unsigned char*)urb->transfer_buffer, count);	
		tty->flip.count += count;	
		tty->flip.char_buf_ptr += count;    

		if (usb_mux_dispatcher)
		{
			usb_mux_dispatcher(tty);/**call Liu changhui's func.**/
		}

		urb->actual_length = 0;
		urb->dev = bvd_ipc->ipc_dev;
		if (usb_submit_urb(urb))
		{
			bvd_dbg("failed resubmitting read urb");
		}
#if 0
		if(jiffies - last_jiff > ICL_EVENT_INTERVAL) 
		{
			last_jiff = jiffies;
			queue_apm_event(KRNL_ICL, NULL);
		}
#endif	
		bvd_dbg("usb_ipc_read_bulk: resubmited read urb");
	}else if(count > 0) {
			bvd_ipc->suspend_flag = 1;
			append_to_inbuf_list(urb);
			ipcusb_timer.data = (unsigned long)urb;
			mod_timer(&ipcusb_timer, jiffies+(10*HZ/1000));
#if 0			
			if(jiffies - last_jiff > ICL_EVENT_INTERVAL) 
			{
				last_jiff = jiffies;
				queue_apm_event(KRNL_ICL, NULL);
			}
#endif	
	}
		
	bvd_dbg("usb_ipc_read_bulk: completed!!!");		
}

static void usb_ipc_write_bulk(struct urb *urb)
{
	callback_times++;
	bvd_ipc->write_finished_flag = 1;
	
	bvd_dbg("usb_ipc_write_bulk: begining!");
	//printk("%s: write_finished_flag=%d\n", __FUNCTION__, bvd_ipc->write_finished_flag);
	
	if (urb->status)
	{
		printk("nonzero write bulk status received: %d\n", urb->status);		
	}

	if (usb_mux_sender)
	{
		usb_mux_sender();		/**call Liu changhui's func**/
	}

	//printk("usb_ipc_write_bulk: mark ipcusb_softint!\n");
	queue_task(&bvd_ipc->tqueue, &tq_immediate);
	mark_bh(IMMEDIATE_BH);

	bvd_dbg("usb_ipc_write_bulk: finished!");
}

static void wakeup_timeout(unsigned long data)
{
	GPSR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
	bvd_dbg("wakup_timeout: send GPIO_MCU_INT_SW signal!");	
}
static void suspend_timeout(unsigned long data)
{
	if(bvd_ipc->suspend_flag == 1)
	{
		bvd_ipc->suspend_flag = 0;
		mod_timer(&suspend_timer, jiffies+(5000*HZ/1000));
		bvd_dbg("suspend_timeout: add the suspend timer again");	
	}else
	{
		unlink_urbs(&bvd_ipc->readurb_mux);
		UHCRHPS3 = 0x4;
		mdelay(40);
		bvd_dbg("suspend_timeout: send SUSPEND signal! UHCRHPS3=0x%x", UHCRHPS3);
	}
}

static void ipcusb_xmit_data()
{	
	int c, count = IPC_URB_SIZE;
	int result = 0;
	int buf_flag = 0;
	int buf_num = 0;

	//printk("%s: sumbit_times=%d, callback_times=%d\n", __FUNCTION__, sumbit_times, callback_times);
	if(bvd_ipc->write_finished_flag == 0)
	{
		//printk("%s: write_finished_flag=%d, return!\n", __FUNCTION__, bvd_ipc->write_finished_flag); 
		return;
	}
 
	while (1) 
	{
		c = CIRC_CNT_TO_END(bvd_ipc->xmit.head, bvd_ipc->xmit.tail, IPC_USB_XMIT_SIZE);
		if (count < c)
		  c = count;
		if (c <= 0) {		  
		  break;
		}
		memcpy(bvd_ipc->obuf+buf_num, bvd_ipc->xmit.buf + bvd_ipc->xmit.tail, c);
		buf_flag = 1;	
		bvd_ipc->xmit.tail = ((bvd_ipc->xmit.tail + c) & (IPC_USB_XMIT_SIZE-1));
		count -= c;
		buf_num += c;
	}    

	if(buf_num == 0)
	{
		bvd_dbg("ipcusb_xmit_data: buf_num=%d, add suspend_timer", buf_num);
		bvd_ipc->suspend_flag = 0;
		mod_timer(&suspend_timer, jiffies+(5000*HZ/1000));
	}

	bvd_dbg("ipcusb_xmit_data: buf_num=%d", buf_num);
	bvd_dbg("ipcusb_xmit_data: bvd_ipc->obuf: ");

#ifdef BVD_DEBUG	
	bvd_dbg_hex((bvd_ipc->obuf)-buf_num, buf_num);
#endif

	if( buf_flag )
	{
		
		bvd_ipc->writeurb_mux.transfer_buffer_length = buf_num;
		bvd_dbg("ipcusb_xmit_data: copy data to write urb finished! ");
		
		if((UHCRHPS3 & 0x4) == 0x4)
		{
			static int ret;
			int time = 0;	

			/**if BP sleep, wake up BP first**/
			set_GPIO_mode(GPIO_IN | 41);
			if(GPIO_is_high(41))
			{
				if(GPIO_is_high(GPIO_MCU_INT_SW))
					GPCR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
				else{
					GPSR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
				}
				time = jiffies;
				while(GPIO_is_high(41) && (jiffies < (time+HZ)))					;
				if(GPIO_is_high(41))
				{
					printk("%s: Wakeup BP timeout! BP state is %d\n", __FUNCTION__, GPIO_is_high(41));		
				}
				if(GPIO_is_high(GPIO_MCU_INT_SW))
					GPCR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
				else{
					GPSR(GPIO_MCU_INT_SW) = GPIO_bit(GPIO_MCU_INT_SW);
				}
			}

			/** Resume BP**/ 
			UHCRHPS3 = 0x8;
			mdelay(40);
			bvd_dbg("ipcusb_xmit_data: Send RESUME signal! UHCRHPS3=0x%x", UHCRHPS3);
			/*send IN token*/
			bvd_ipc->readurb_mux.actual_length = 0;
			bvd_ipc->readurb_mux.dev = bvd_ipc->ipc_dev;
			if(ret = usb_submit_urb(&bvd_ipc->readurb_mux))
			{
				printk("ipcusb_xmit_data: usb_submit_urb(read mux bulk) failed! status=%d\n", ret);
			}
			bvd_dbg("ipcusb_xmit_data: Send a IN token successfully!");
		}

		sumbit_times++;
		bvd_ipc->write_finished_flag = 0;
		//printk("%s: clear write_finished_flag:%d\n", __FUNCTION__, bvd_ipc->write_finished_flag);
		bvd_ipc->writeurb_mux.dev = bvd_ipc->ipc_dev;
		if(result = usb_submit_urb(&bvd_ipc->writeurb_mux))
		{
			warn("ipcusb_xmit_data: funky result! result=%d\n", result);
		}
		
		bvd_dbg("ipcusb_xmit_data: usb_submit_urb finished! result:%d", result);
	
	}		
}

static void ipcusb_softint(void *private)
{
	ipcusb_xmit_data();
}

extern void get_halted_bit(void);

static void ipcusb_softint_send_readurb(void *private)
{
	if((UHCRHPS3 & 0x4) == 0x4)
	{
		UHCRHPS3 = 0x8;
		mdelay(40);
		bvd_dbg("ipcusb_softint_send_readurb: Send RESUME signal! UHCRHPS3=0x%x", UHCRHPS3);
	}
	if (bvd_ipc->ipc_flag == IPC_USB_PROBE_READY)
	{ 
		get_halted_bit();

		/*send a IN token*/
		bvd_ipc->readurb_mux.dev = bvd_ipc->ipc_dev;
		if(usb_submit_urb(&bvd_ipc->readurb_mux))
		{
			bvd_dbg("ipcusb_softint_send_readurb: usb_submit_urb(read mux bulk) failed!");
		}
		bvd_dbg("ipcusb_softint_send_readurb: Send a IN token successfully!");
		bvd_ipc->suspend_flag = 0;
		bvd_dbg("ipcusb_softint_send_readurb: add suspend_timer");
		mod_timer(&suspend_timer, jiffies+(5000*HZ/1000));		
	}
}

static int usb_ipc_write (struct tty_struct * tty, int from_user, const unsigned char *buf, int count)
{	
	int c, ret = 0;
	int begin = 0;
		
	bvd_dbg("usb_ipc_write: count=%d, buf: ", count);
#ifdef BVD_DEBUG
	bvd_dbg_hex(buf, count);
#endif

	if(count <= 0)
	{
		return 0;
	}	

	if(*ipcusb_ap_to_bp != NULL)
		(*ipcusb_ap_to_bp)(buf, count);

	bvd_ipc->suspend_flag = 1;
	
	if ((bvd_ipc->ipc_flag == IPC_USB_PROBE_READY) && (bvd_ipc->xmit.head == bvd_ipc->xmit.tail))
{
		bvd_dbg("usb_ipc_write: set write_flag");
		bvd_ipc->write_flag = IPC_USB_WRITE_XMIT;
	}
		
	while (1) 
	{
		c = CIRC_SPACE_TO_END(bvd_ipc->xmit.head, bvd_ipc->xmit.tail, IPC_USB_XMIT_SIZE);
		if (count < c)		
			c = count;		
		if (c <= 0) {
			break;
		}
		memcpy(bvd_ipc->xmit.buf + bvd_ipc->xmit.head, buf, c);
		bvd_ipc->xmit.head = ((bvd_ipc->xmit.head + c) & (IPC_USB_XMIT_SIZE-1));
		buf += c;
		count -= c;
		ret += c;
	}	
	bvd_dbg("usb_ipc_write: ret=%d, bvd_ipc->xmit.buf: ", ret);
	
#ifdef BVD_DEBUG
	bvd_dbg_hex(bvd_ipc->xmit.buf, ret);
#endif

	if (bvd_ipc->write_flag == IPC_USB_WRITE_XMIT)
	{
		bvd_ipc->write_flag = IPC_USB_WRITE_INIT;
		bvd_dbg("usb_ipc_write: mark ipcusb_softint");
		queue_task(&bvd_ipc->tqueue, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
	}
	
	bvd_dbg("usb_ipc_write: ret=%d\n", ret);
	return ret;
}

static int usb_ipc_chars_in_buffer(struct tty_struct *tty)
{		
	return CIRC_CNT(bvd_ipc->xmit.head, bvd_ipc->xmit.tail, IPC_USB_XMIT_SIZE);
}

void usb_send_readurb( void )
{
	//printk("usb_send_readurb: begining!UHCRHPS3=0x%x, usbh_finished_resume=%d\n", UHCRHPS3, usbh_finished_resume);
	
	if(usbh_finished_resume == 0)
	{
		return;
	}	
	queue_task(&bvd_ipc->tqueue_bp, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

static void *usb_ipc_probe(struct usb_device *usbdev, unsigned int ifnum,
			   const struct usb_device_id *id)
{	
	struct usb_config_descriptor *ipccfg;
	struct usb_interface_descriptor *interface;
	struct usb_endpoint_descriptor *endpoint;		
	int ep_cnt, readsize, writesize;
	char have_bulk_in_mux, have_bulk_out_mux;

	bvd_dbg("usb_ipc_probe: vendor id 0x%x, device id 0x%x", usbdev->descriptor.idVendor, usbdev->descriptor.idProduct);

	if ((usbdev->descriptor.idVendor != MOTO_IPC_VID) || (usbdev->descriptor.idProduct != MOTO_IPC_PID))
	{	
 		return NULL;
	}
	// a2590c : dsplog interface is not supported by this driver
	if (ifnum == 2 ) {   // dsplog interface number is 2
		return NULL;
	}

	bvd_dbg("usb_ipc_probe: USB dev address:%p", usbdev);
	bvd_dbg("usb_ipc_probe: ifnum:%u", ifnum);
	
	ipccfg = usbdev->actconfig;
	bvd_dbg("usb_ipc_prob: config%d", ipccfg->bConfigurationValue);
	bvd_dbg("usb_ipc_prob: bNumInterfaces = %d", ipccfg->bNumInterfaces);

/*
 * After this point we can be a little noisy about what we are trying to configure, hehe.
 */
	if (usbdev->descriptor.bNumConfigurations != 1) 
	{
		info("usb_ipc_probe: Only one device configuration is supported.");
		return NULL;
	}

	if (usbdev->config[0].bNumInterfaces != 3) 
	{
		info("usb_ipc_probe: Only three device interfaces are supported.");
		return NULL;
	}
	
	interface = usbdev->config[0].interface[0].altsetting;
	endpoint = interface[0].endpoint;
/*
 * Start checking for two bulk endpoints or ... FIXME: This is a future enhancement...
 */	
	bvd_dbg("usb_ipc_probe: Number of Endpoints:%d", (int) interface->bNumEndpoints);	

	if (interface->bNumEndpoints != 2) 
	{
		info("usb_ipc_probe: Only two endpoints supported.");
		return NULL;
	}

	ep_cnt = have_bulk_in_mux = have_bulk_out_mux = 0;	

	bvd_dbg("usb_ipc_probe: endpoint[0] is:%x", (&endpoint[0])->bEndpointAddress);
	bvd_dbg("usb_ipc_probe: endpoint[1] is:%x ", (&endpoint[1])->bEndpointAddress);
	
	while (ep_cnt < interface->bNumEndpoints) 
	{

		if (!have_bulk_in_mux && IS_EP_BULK_IN(endpoint[ep_cnt])) 
		{			
			bvd_dbg("usb_ipc_probe: bEndpointAddress(IN) is:%x ", (&endpoint[ep_cnt])->bEndpointAddress);
			have_bulk_in_mux = (&endpoint[ep_cnt])->bEndpointAddress;
			readsize = (&endpoint[ep_cnt])->wMaxPacketSize;	
			bvd_dbg("usb_ipc_probe: readsize=%d", readsize);			
			ep_cnt++;
			continue;
		}

		if (!have_bulk_out_mux && IS_EP_BULK_OUT(endpoint[ep_cnt])) 
		{			
			bvd_dbg("usb_ipc_probe: bEndpointAddress(OUT) is:%x ", (&endpoint[ep_cnt])->bEndpointAddress);
			have_bulk_out_mux = (&endpoint[ep_cnt])->bEndpointAddress;			
			writesize = (&endpoint[ep_cnt])->wMaxPacketSize;
			bvd_dbg("usb_ipc_probe: writesize=%d", writesize);			
			ep_cnt++;
			continue;
		}
		
		info("usb_ipc_probe: Undetected endpoint ^_^ ");
		return NULL;	/* Shouldn't ever get here unless we have something weird */
	}

/*
 * Perform a quick check to make sure that everything worked as it should have.
 */

	switch(interface->bNumEndpoints) 
	{
		case 2:
			if (!have_bulk_in_mux || !have_bulk_out_mux) 
			{
				info("usb_ipc_probe: Two bulk endpoints required.");
				return NULL;
			}
			break;	
		default:
			info("usb_ipc_probe: Endpoint determination failed ^_^ ");
			return NULL;
	}	

/* Ok, now initialize all the relevant values */
	if (!(bvd_ipc->obuf = (char *)kmalloc(writesize, GFP_KERNEL))) 
	{
		err("usb_ipc_probe: Not enough memory for the output buffer.");
		kfree(bvd_ipc);		
		return NULL;
	}
	bvd_dbg("usb_ipc_probe: obuf address:%p", bvd_ipc->obuf);

	if (!(bvd_ipc->ibuf = (char *)kmalloc(readsize, GFP_KERNEL))) 
	{
		err("usb_ipc_probe: Not enough memory for the input buffer.");
		kfree(bvd_ipc->obuf);
		kfree(bvd_ipc);		
		return NULL;
	}
	bvd_dbg("usb_ipc_probe: ibuf address:%p", bvd_ipc->ibuf);

	bvd_ipc->ipc_flag = IPC_USB_PROBE_READY;	
	bvd_ipc->write_finished_flag = 1;
	bvd_ipc->suspend_flag = 1;
	bvd_ipc->bulk_in_ep_mux= have_bulk_in_mux;
	bvd_ipc->bulk_out_ep_mux= have_bulk_out_mux;		
	bvd_ipc->ipc_dev = usbdev;	
	bvd_ipc->writesize = writesize;	
	INIT_LIST_HEAD (&bvd_ipc->in_buf_list);
	bvd_ipc->tqueue.routine = ipcusb_softint;
	bvd_ipc->tqueue.data = bvd_ipc;	
	bvd_ipc->tqueue_bp.routine = ipcusb_softint_send_readurb;
	bvd_ipc->tqueue_bp.data = bvd_ipc;	
	
	/*Build a write urb*/
	FILL_BULK_URB(&bvd_ipc->writeurb_mux, usbdev, usb_sndbulkpipe(bvd_ipc->ipc_dev, bvd_ipc->bulk_out_ep_mux), 
			bvd_ipc->obuf, writesize, usb_ipc_write_bulk, bvd_ipc);
		bvd_ipc->writeurb_mux.transfer_flags |= USB_ASYNC_UNLINK;

	/*Build a read urb and send a IN token first time*/
	FILL_BULK_URB(&bvd_ipc->readurb_mux, usbdev, usb_rcvbulkpipe(usbdev, bvd_ipc->bulk_in_ep_mux),
			bvd_ipc->ibuf, readsize, usb_ipc_read_bulk, bvd_ipc);
		bvd_ipc->readurb_mux.transfer_flags |= USB_ASYNC_UNLINK;	
	
	usb_driver_claim_interface(&usb_ipc_driver, &ipccfg->interface[0], bvd_ipc);
	usb_driver_claim_interface(&usb_ipc_driver, &ipccfg->interface[1], bvd_ipc);
	
        // a2590c: dsplog is not supported by this driver
	//	usb_driver_claim_interface(&usb_ipc_driver, &ipccfg->interface[2], bvd_ipc);
	/*send a IN token first time*/
	bvd_ipc->readurb_mux.dev = bvd_ipc->ipc_dev;
	if(usb_submit_urb(&bvd_ipc->readurb_mux))
	{
		printk("usb_ipc_prob: usb_submit_urb(read mux bulk) failed!\n");
	}
	bvd_dbg("usb_ipc_prob: Send a IN token successfully!");
	
	if(bvd_ipc->xmit.head != bvd_ipc->xmit.tail)
	{
		printk("usb_ipc_probe: mark ipcusb_softint!\n");
		queue_task(&bvd_ipc->tqueue, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
	}

	printk("usb_ipc_probe: completed probe!");
	return bvd_ipc;
}

static void usb_ipc_disconnect(struct usb_device *usbdev, void *ptr)
{
	struct ipc_usb_data *bvd_ipc_disconnect = (struct ipc_usb_data *) ptr;
		
	printk("usb_ipc_disconnect:*** \n");

	if((UHCRHPS3 & 0x4) == 0)
	{	
		usb_unlink_urb(&bvd_ipc_disconnect->readurb_mux);
	}
	usb_unlink_urb(&bvd_ipc_disconnect->writeurb_mux);
        
	bvd_ipc_disconnect->ipc_flag = IPC_USB_PROBE_NOT_READY;
	kfree(bvd_ipc_disconnect->ibuf);
	kfree(bvd_ipc_disconnect->obuf);
	
	usb_driver_release_interface(&usb_ipc_driver, &bvd_ipc_disconnect->ipc_dev->actconfig->interface[0]);
        usb_driver_release_interface(&usb_ipc_driver, &bvd_ipc_disconnect->ipc_dev->actconfig->interface[1]);
        
	//a2590c: dsplog interface is not supported by this driver
	//usb_driver_release_interface(&usb_ipc_driver, &bvd_ipc_disconnect->ipc_dev->actconfig->interface[2]);

	bvd_ipc_disconnect->ipc_dev = NULL;

	printk("usb_ipc_disconnect completed!\n");
}

static struct usb_device_id usb_ipc_id_table [] = {
	{ USB_DEVICE(MOTO_IPC_VID, MOTO_IPC_PID) },
	{ }						/* Terminating entry */
};

static struct usb_driver usb_ipc_driver = {
	name:		"usb ipc",
	probe:		usb_ipc_probe,
	disconnect:	usb_ipc_disconnect,
	id_table:	usb_ipc_id_table,
	id_table:	NULL,
};

static int __init usb_ipc_init(void)
{
	int result;

	bvd_dbg ("init usb_ipc");
	/* register driver at the USB subsystem */
	result = usb_register(&usb_ipc_driver);

	if (result < 0) {
		err ("usb ipc driver could not be registered");
		return -1;
	}

	/*init the related mux interface*/
	if (!(bvd_ipc = kmalloc (sizeof (struct ipc_usb_data), GFP_KERNEL))) {
		err("usb_ipc_init: Out of memory.");		
		return NULL;
	}
	memset (bvd_ipc, 0, sizeof(struct ipc_usb_data));
	bvd_dbg ("usb_ipc_init: Address of bvd_ipc:%p", bvd_ipc);

	if (!(bvd_ipc->xmit.buf = (char *)kmalloc(IPC_USB_XMIT_SIZE, GFP_KERNEL))) 
	{
		err("usb_ipc_init: Not enough memory for the input buffer.");
		kfree(bvd_ipc);		
		return NULL;
	}
	bvd_dbg("usb_ipc_init: bvd_ipc->xmit.buf address:%p", bvd_ipc->xmit.buf);
	bvd_ipc->ipc_dev = NULL;
	bvd_ipc->xmit.head = bvd_ipc->xmit.tail = 0;
	bvd_ipc->write_flag = IPC_USB_WRITE_INIT;
	memset(&ipcusb_tty_driver, 0, sizeof(struct tty_driver));
	ipcusb_tty_driver.write = usb_ipc_write;
	ipcusb_tty_driver.chars_in_buffer = usb_ipc_chars_in_buffer;
	memset(&ipcusb_tty, 0, sizeof(struct tty_struct));
	ipcusb_tty.flip.char_buf_ptr = ipcusb_tty.flip.char_buf;
      	ipcusb_tty.flip.flag_buf_ptr = ipcusb_tty.flip.flag_buf;
	usb_for_mux_driver = &ipcusb_tty_driver;
	usb_for_mux_tty = &ipcusb_tty;
	
	/*init timers for ipcusb read process and usb suspend*/
	init_timer(&ipcusb_timer);
	ipcusb_timer.function = ipcusb_timeout;
	init_timer(&suspend_timer);
	suspend_timer.function = suspend_timeout;
	init_timer(&wakeup_timer);
	wakeup_timer.function = wakeup_timeout;
	
	info("USB Host(Bulverde) IPC driver registered.");
	info(DRIVER_VERSION ":" DRIVER_DESC);
	return 0;
}

static void __exit usb_ipc_exit(void)
{
	bvd_dbg ("cleanup bvd_ipc");

	kfree(bvd_ipc->xmit.buf);
	kfree(bvd_ipc);
		
	usb_deregister(&usb_ipc_driver);
	info("USB Host(Bulverde) IPC driver deregistered.");
}

module_init(usb_ipc_init);
module_exit(usb_ipc_exit);
EXPORT_SYMBOL(usb_send_readurb);





