/*
 * linux/drivers/usbd/bi/pxa.c -- Xscale USB Device Controller driver. 
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2000, 2001, 2002 Lineo
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Tom Rushworth <tbr@belcarra.com>, 
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


/*
 * Testing notes
 *
 * This code was developed on the Intel Lubbock with Cotulla PXA250 processor.
 *
 * The default 
 *
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

#define CONFIG_MOTO_EZX 1

MODULE_AUTHOR ("sl@belcarra.com, tbr@belcarra.com");
MODULE_DESCRIPTION ("Xscale USB Device Bus Interface");
MODULE_LICENSE("GPL");

USBD_MODULE_INFO ("pxa_bi 0.1-alpha");

const char *usbd_bi_module_info(void)
{
	return __usbd_module_info;
}


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include <linux/netdevice.h>
#include <linux/timer.h>

#include <asm/irq.h>
#include <asm/system.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch-pxa/pxa-regs.h>
#include <asm/hardware.h>
#include <asm/dma.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

#include "pxa.h"
#include "trace.h"

//#ifdef CONFIG_MOTO_EZX
#include "../../ssp/ssp_pcap.h"
//#endif

#undef USE_DMA_OUT
#undef USE_DMA_IN

unsigned int int_oscr;
static struct timer_list pxa_timer;


#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)
unsigned int dma_interrupts;

void pxa_kill(void)
{
        if ((udc_interrupts > 1000) || (dma_interrupts > 100)) {

                if ((udc_interrupts%100) == 0) {

                        printk(KERN_INFO
                                        "int[%d] %d disabling USIR[%02x %02x] "
                                        "CCR[%02x] UICR[%02x %02x] UFNHR[%02x %02x]\n",
                                        udc_interrupts, dma_interrupts, USIR1, USIR0, UDCCR, UICR1, UICR0, UFNHR, UFNLR);

                }

                UDCCR &= ~( UDCCR_UDE | UDCCR_REM );
                UICR1 = UICR0 = 0xff;
                UFNHR |= UFNHR_SIM;
                USIR1 = USIR1;
                USIR0 = USIR0;
                UDCCS0 = UDCCS0_FTF | UDCCS0_FST | UDCCS0_SA | UDCCS0_OPR;
                CKEN &= ~CKEN11_USB;
                return;

        }

}
#else /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

void pxa_kill(int kill)
{
        if (kill || (udc_interrupts > 1000) ) {

                TRACE_MSG("pxa_kill");

                if ((udc_interrupts%100) == 0) {

                        TRACE_MSG("pxa_kill");
                        printk(KERN_INFO
                                        "int[%d] disabling USIR[%02x %02x] "
                                        "CCR[%02x] UICR[%02x %02x] UFNHR[%02x %02x]\n",
                                        udc_interrupts, USIR1, USIR0, UDCCR, UICR1, UICR0, UFNHR, UFNLR);

                }

                UDCCR &= ~( UDCCR_UDE | UDCCR_REM );
                UICR1 = UICR0 = 0xff;
                UFNHR |= UFNHR_SIM;
                USIR1 = USIR1;
                USIR0 = USIR0;
                UDCCS0 = UDCCS0_FTF | UDCCS0_FST | UDCCS0_SA | UDCCS0_OPR;
                CKEN &= ~CKEN11_USB;
                return;

        }

}
#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

static int udc_suspended;
static struct usb_device_instance *udc_device;	// required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb *ep0_urb;

extern unsigned int udc_interrupts;

#if 0
int jifs(void)
{
        static unsigned long jiffies_last;
        int elapsed = jiffies - jiffies_last;

        jiffies_last = jiffies;
        return elapsed;
}
#else
int jifs(void)
{
        static unsigned long jiffies_last;
        int elapsed = OSCR - jiffies_last;

        jiffies_last = OSCR;
        return elapsed;
}
#endif



/* ********************************************************************************************* */
/* IO
 */

/*
 * Map logical to physical
 */

typedef enum ep {
        ep_control, ep_bulk_in, ep_bulk_out, ep_iso_in, ep_iso_out, ep_interrupt
} ep_t;

/*
 * PXA has lots of endpoints, but they have fixed address and type
 * so logical to physical map is limited to masking top bits so we
 * can find appropriate info.
 */


u32 _UDDRN[16] = {
        0x40600080, 0x40600100, 0x40600180, 0x40600200,
        0x40600400, 0x406000A0, 0x40600600, 0x40600680,
        0x40600700, 0x40600900, 0x406000C0, 0x40600B00, 
        0x40600B80, 0x40600C00, 0x40600E00, 0x406000E0, 
};

u32 _UBCRN[16] = {
        0,          0,          0x40600068, 0,
        0x4060006c, 0,          0,          0x40600070,
        0,          0x40600074, 0,          0,
        0x40600078, 0,          0x4060007c, 0, 
};


#define UDCCSN(x)        __REG2(0x40600010, (x) << 2)

#define UDDRN(x)        __REG(_UDDRN[x])
#define UBCRN(x)        __REG(_UBCRN[x])


#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)

struct ep_map {
        int logical;
        ep_t eptype;
        int size;
        int dma_chan;
        volatile u32 *drcmr;
        u_long dev_addr;
};

#ifdef USE_DMA_OUT
static usb_stream_t usb_stream_out = {
        name:           "PXA USB out",
        dcmd:           (DCMD_INCTRGADDR|DCMD_FLOWSRC|DCMD_BURST32|DCMD_WIDTH1),
        dma_ch:         -1,
        //drcmr:        // copy from ep_maps
        //dev_addr:     // get from UUDRN()
};
#endif /* USE_DMA_OUT */

#ifdef USE_DMA_IN
static usb_stream_t usb_stream_in = {
        name:           "PXA USB in",
        dcmd:           ((DCMD_INCSRCADDR|DCMD_FLOWTRG|DCMD_BURST32|DCMD_WIDTH1)),
        dma_ch:         -1,
        //drcmr:        // copy from ep_maps
        //dev_addr:     // get from UUDRN()
};
#endif /* USE_DMA_IN */

static struct ep_map ep_maps[16] = {
        { logical: 0, eptype: ep_control,   size: 16, },

        { logical: 1, eptype: ep_bulk_in,   size: 64, drcmr: &DRCMR25 },
        { logical: 2, eptype: ep_bulk_out,  size: 64, drcmr: &DRCMR26 },

        { logical: 3, eptype: ep_iso_in,   size: 256, drcmr: &DRCMR27 },
        { logical: 4, eptype: ep_iso_out,  size: 256, drcmr: &DRCMR28 },

        { logical: 5, eptype: ep_interrupt , size: 8, },

        { logical: 6, eptype: ep_bulk_in,   size: 64, drcmr: &DRCMR30 },
        { logical: 7, eptype: ep_bulk_out,  size: 64, drcmr: &DRCMR31 },
                                                      
        { logical: 8, eptype: ep_iso_in,   size: 256, drcmr: &DRCMR33 },
        { logical: 9, eptype: ep_iso_out,  size: 256, drcmr: &DRCMR34 },
                                                      
        { logical: 10, eptype: ep_interrupt, size: 8, },
                                                      
        { logical: 11, eptype: ep_bulk_in,  size: 64, drcmr: &DRCMR35 },
        { logical: 12, eptype: ep_bulk_out, size: 64, drcmr: &DRCMR36 },
                                                      
        { logical: 13, eptype: ep_iso_in,  size: 256, drcmr: &DRCMR37 },
        { logical: 14, eptype: ep_iso_out, size: 256, drcmr: &DRCMR38 },
                                                      
        { logical: 15, eptype: ep_interrupt, size: 8, },
};
#else /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

struct ep_map {
        int             logical;
        ep_t            eptype;
        int             size;
        int             dma_chan;
};

static struct ep_map ep_maps[16] = {
        { logical: 0, eptype: ep_control,   size: 16, },
        { logical: 1, eptype: ep_bulk_in,   size: 64, },

        { logical: 2, eptype: ep_bulk_out,  size: 64, },

        { logical: 3, eptype: ep_iso_in,   size: 256, },
        { logical: 4, eptype: ep_iso_out,  size: 256, },

        { logical: 5, eptype: ep_interrupt , size: 8, },

        { logical: 6, eptype: ep_bulk_in,   size: 64, },
        { logical: 7, eptype: ep_bulk_out,  size: 64, },
                                                      
        { logical: 8, eptype: ep_iso_in,   size: 256, },
        { logical: 9, eptype: ep_iso_out,  size: 256, },
                                                      
        { logical: 10, eptype: ep_interrupt, size: 8, },
                                                      
        { logical: 11, eptype: ep_bulk_in,  size: 64, },
        { logical: 12, eptype: ep_bulk_out, size: 64, },
                                                      
        { logical: 13, eptype: ep_iso_in,  size: 256, },
        { logical: 14, eptype: ep_iso_out, size: 256, },
                                                      
        { logical: 15, eptype: ep_interrupt, size: 8, },
};

#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

/***usb gpio setting ***/
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
    usbd_device_event(udc_device, DEVICE_CABLE_DISCONNECTED, 0);	
    return 0;
}
/**end**/

static __inline__ void pxa_enable_ep_interrupt(int ep)
{
        ep &= 0xf;
        if (ep < 8) {
                UICR0 &= ~(1<<ep);
        }
        else {
                UICR1 &= ~(1<<(ep-8));
        }
        TRACE_MSG16("enable interrupt: %x %x", ep, UICR0);
}

static __inline__ void pxa_disable_ep_interrupt(int ep)
{
        ep &= 0xf;
        if (ep < 8) {
                UICR0 = 1<<ep;
        }
        else {
                UICR1 = 1<<(ep-8);
        }
        TRACE_MSG16("disable interrupt: %x %x", ep, UICR0);
}

static __inline__ void pxa_ep_reset_irs(int ep)
{
        //TRACE_MSG32("reset interrupt: %d\n", ep);
        ep &= 0xf;
        if (ep < 8) {
                USIR0 = (1 << ep);
        }
        else {
                USIR1 = (1 << (ep - 8));
        }
}


/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

static void pxa_out_flush(int ep) {
        unsigned char c;
        while (UDCCSN(ep) & UDCCS_BO_RNE) {
                c = UDDRN(ep);
        }
}

/***add by Levis for usb flow control***/
typedef struct timer_data {
	int ep;
	struct usb_endpoint_instance *endpoint;
	struct usb_device_instance *device;
}timer_data_t;	
timer_data_t timer_use_data;

static void pxa_timeout(unsigned long data)
{
	timer_data_t *pxa_data;
	struct usb_endpoint_instance *endpoint;
	int ep;
	struct usb_device_instance *device;
	
	pxa_data = (timer_data_t*)data;
	ep = pxa_data->ep;
	endpoint = pxa_data->endpoint;
	device = pxa_data->device;
	
	usbd_schedule_device_bh(device);
	TRACE_MSG32("pxa_timerout ep=%d\n", ep);
 	endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);
	if(endpoint->rcv_urb)
	{
		pxa_enable_ep_interrupt(ep);
	} else{
			pxa_timer.expires = jiffies + (20*HZ/1000);
			add_timer(&pxa_timer);
	}
	
}
/***end Levis***/

#ifndef USE_DMA_OUT
static __inline__ void pxa_out_n(int ep, struct usb_endpoint_instance *endpoint, u32 cs)
{
        //u32 cs;
        while ((cs = UDCCSN(ep)) & UDCCS_BO_RPC/*|UDCCS_B0_RFS*/) {

                //TRACE_MSG16("pxa_out_n: %d cs: %02x\n", ep, cs);

                if (endpoint) {
                        if (!endpoint->rcv_urb) {
				endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
                        }
                        if (endpoint->rcv_urb) {

                                int len = 0;
				unsigned char *cp = endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length;

                                if (cp) {
                                        // read available bytes (max packetsize) into urb buffer
                                        int count = MIN(UBCRN(ep) + 1, endpoint->rcv_packetSize);
                                        while (count--) {
                                                len++;
                                                *cp++ = UDDRN(ep);
                                        }
                                }
                                else {
                                        printk(KERN_INFO"read[%d:%d] bad arguements\n", udc_interrupts, jifs());
                                        pxa_out_flush(ep);
                                }
                                // fall through if error of any type, len = 0
                                TRACE_MSG16("OUT: actual: %d len: %d", endpoint->rcv_urb->actual_length, len);
                                usbd_rcv_complete_irq (endpoint, len, 0);
                        }
                        else {
                                struct urb *tp_urb;
				int count = MIN(UBCRN(ep) + 1, endpoint->rcv_packetSize);
				TRACE_MSG32("RECV FLUSH: %d", count);	
				pxa_disable_ep_interrupt(ep);
				memset((void*)&timer_use_data, 0, sizeof(timer_data_t));
				timer_use_data.ep = ep;
				timer_use_data.endpoint = endpoint;
				tp_urb = p2surround(struct urb, link, (&endpoint->rcv)->next);
				timer_use_data.device = tp_urb->device;
				pxa_timer.expires = jiffies + (20*HZ/1000);
				pxa_timer.data = (unsigned long)&timer_use_data;
				add_timer(&pxa_timer);
                                break;
                        }
                }
                // clear RPC and interrupt
                UDCCSN(ep) = UDCCS_BO_RPC;
        }
        pxa_ep_reset_irs(ep);
}
#else /* !defined(USE_DMA_OUT) */

static void pxa_usb_out_dma_irq(int ch, void *dev_id, struct pt_regs *regs)
{
        usb_stream_t *usb_stream = dev_id;
        u_int dcsr;

        dma_interrupts++;

        pxa_kill();

        printk(KERN_INFO"DMA[%d:%d]\n", udc_interrupts, dma_interrupts);

        // Stop DMA
        dcsr = DCSR(ch);
        DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;

        if (!usb_stream->buffers) {
                printk(KERN_INFO"pxa_usb_out_dma: no buffers\n");
                return;
        }

        if (dcsr & DCSR_BUSERR) {
                printk(KERN_INFO"pxa_usb_out_dma: bus error %d\n",ch);
                return;
        }
#if 0
        if (dcsr & DCSR_ENDINTR) {

                u_long cur_dma_desc;

                // derive DMA descriptor, DDADR points to next descriptor, not current

                cur_dma_desc = DDADR(ch) - usb_stream->dma_desc_phys - sizeof(pxa_dma_desc);
        }
#endif
}

void pxa_start_out_dma(int ep, struct usb_endpoint_instance *endpoint, usb_stream_t *usb_stream) 
{

        DCSR(usb_stream->dma_ch) = DCSR_RUN;

        *usb_stream->drcmr = usb_stream->dma_ch | DRCMR_MAPVLD;

        printk(KERN_INFO"start out dma [%d:%d] dma_ch: %d drcmr: %p %x DCSR: %x DDADR: %x DSADR: %x DTADR: %x DCMD: %x\n", 
                        udc_interrupts, dma_interrupts, usb_stream->dma_ch,
                        usb_stream->drcmr, *usb_stream->drcmr, 
                        DCSR(usb_stream->dma_ch),
                        DDADR(usb_stream->dma_ch),
                        DSADR(usb_stream->dma_ch),
                        DTADR(usb_stream->dma_ch),
                        DCMD(usb_stream->dma_ch));
}

#endif /* !defined(USE_DMA_OUT) */

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */
static void pxa_in_flush(int ep) 
{
        //printk(KERN_INFO"%s: ep: %d CSN: %02x\n", __FUNCTION__, ep, UDCCSN(ep));
        UDCCSN(ep) = UDCCS_BI_FTF;
        //printk(KERN_INFO"%s: ep: %d CSN: %02x\n", __FUNCTION__, ep, UDCCSN(ep));
        UDCCSN(ep) = UDCCS_BI_TSP;
        //printk(KERN_INFO"%s: ep: %d CSN: %02x\n", __FUNCTION__, ep, UDCCSN(ep));
}

static void __inline__ pxa_start_n (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        //TRACE_MSG32("pxa_start_n ep: %d", ep);
        if (endpoint->tx_urb) {
                int last;
                struct urb *urb = endpoint->tx_urb;

                //TRACE_MSG32("pxa_start_n len: %d", urb->actual_length);

                if (( last = MIN (urb->actual_length - (endpoint->sent + endpoint->last), endpoint->tx_packetSize))) {
                        int size = last;
                        unsigned char *cp = urb->buffer + endpoint->sent + endpoint->last;

                        //TRACE_MSG32("pxa_start_n size: %d", size);

                        while (size--) {
                                UDDRN(ep) = *cp++;
                        }

                        if (( last < endpoint->tx_packetSize ) ||
                                        ( (endpoint->tx_urb->actual_length - endpoint->sent ) == last )) 
                        {
                                UDCCSN(ep) = UDCCS_BI_TSP;
                        }
                        //TRACE_MSG16("pxa_start_n size: %d UDCCSN: %d", size, UDCCSN(ep));
                        endpoint->last += last;
                }
        }
}

static void __inline__ pxa_in_n (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        int udccsn;

        pxa_ep_reset_irs(ep);

        // if TPC update tx urb and clear TPC
        if ((udccsn = UDCCSN(ep)) & UDCCS_BI_TPC) {

                UDCCSN(ep) = UDCCS_BI_TPC;
                usbd_tx_complete_irq(endpoint, 0);
        }

        if (udccsn & UDCCS_BI_TFS) {
                pxa_start_n(ep, endpoint);
        }

        // clear underrun, not much we can do about it
        if (udccsn & UDCCS_BI_TUR) {
                UDCCSN(ep) = UDCCS_BI_TUR;
        }
}


#ifdef USE_DMA_IN
static void pxa_usb_in_dma_irq(int ch, void *dev_id, struct pt_regs *regs)
{
        u_int dcsr;
        dcsr = DCSR(ch);
        DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;
}
#endif /* USE_DMA_IN */


/* ********************************************************************************************* */
/* Control (endpoint zero)
 */

void pxa_ep0xmit(struct usb_endpoint_instance *endpoint, u32 udccs0)
{
        int short_packet;
        int size;
        struct urb *urb = endpoint->tx_urb;

        TRACE_MSG32("          --> xmit: UDCCS0: %02x", udccs0);
	//printk("\nhere is ep0xmit!\n");
        //printk(KERN_INFO"tx[%d:%d] CS0[%02x]\n", udc_interrupts, jifs(), UDCCS0);

        // check for premature status stage - host abandoned previous IN
        if ((udccs0 & UDCCS0_OPR) && !(UDCCS0 & UDCCS0_SA) ) {

                // clear tx fifo and opr
                UDCCS0 = UDCCS0_FTF | UDCCS0_OPR;
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
                TRACE_MSG32("          <-- xmit premature status: %02x", UDCCS0);
                return;
        }

        // check for stall
        if (udccs0 & UDCCS0_SST) {
                // clear stall and tx fifo
                UDCCS0 = UDCCS0_SST | UDCCS0_FTF;
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
                TRACE_MSG32("          <-- xmit stall: %02x", UDCCS0);
                return;
        }

	/* How much are we sending this time? (May be zero!)
           (Note that later call of tx_complete() will add last to sent.) */
	if (NULL == urb) {
		size = 0;
	} else {
		endpoint->last = size = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
	}

	/* Will this be a short packet?
          (It may be the last, but still be full size, in which case we will need a ZLP later.) */
        short_packet = (size < endpoint->tx_packetSize);

        TRACE_MSG32("xmit: %d", size);

	if (size > 0 && urb->buffer) {
		// Stuff the FIFO
                unsigned char *cp = urb->buffer + endpoint->sent;

		while (size--) {
			UDDRN(0) = *cp++;
		}
        }

        // Is this the end of the data state? (We've sent all the data, plus any required ZLP.)
        if (!endpoint->tx_urb || (endpoint->last < endpoint->tx_packetSize)) {
		// Tell the UDC we are at the end of the packet.
		UDCCS0 = UDCCS0_IPR;
                endpoint->state = WAIT_FOR_OUT_STATUS;
                TRACE_MSG32("          <-- xmit wait for status: %02x", UDCCS0);
	}

        else if ((endpoint->last == endpoint->tx_packetSize) && 
                        ((endpoint->last + endpoint->sent) == ep0_urb->actual_length)  &&
                        ((ep0_urb->actual_length) < le16_to_cpu(ep0_urb->device_request.wLength)) 
                ) 
        {
		// Tell the UDC we are at the end of the packet.
                endpoint->state = DATA_STATE_NEED_ZLP;
                TRACE_MSG32("          <-- xmit need zlp: %02x", UDCCS0);
	}
        else {
                TRACE_MSG32("          <-- xmit not finished: %02x", UDCCS0);
        }
}

/*
 * accumulate additional host 2 device data into urb buffer
 */
static __inline__ void pxa_out_ep0(int ep, struct usb_endpoint_instance *endpoint, u32 udccs0)
{
        TRACE_MSG32("          --> recv: %02x", udccs0);
        if ((udccs0 & UDCCS0_OPR) && !(UDCCS0 & UDCCS0_SA) ) {
        }

        if (endpoint && endpoint->rcv_urb) {

                int len = 0;
                unsigned char *cp = endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length;

                if (cp) {
                        // read available bytes (max packetsize) into urb buffer
                        while ((UDCCS0 & UDCCS0_RNE) && (len < endpoint->rcv_packetSize)) {
                                len++;
                                *cp++ = UDDR0;
                        }
                        TRACE_MSG16("         <--  ep0 recv: packetsize: %02x len: %02x", endpoint->rcv_packetSize, len);
                }
                else {
                        TRACE_MSG32("pxa_out_ep0 flush", 0);
                        printk(KERN_INFO"read[%d:%d] bad arguements\n", udc_interrupts, jifs());
                        pxa_out_flush(ep);
                }
                endpoint->rcv_urb->actual_length += len;
                if ((len && (len < endpoint->rcv_packetSize)) || 
                                ((endpoint->rcv_urb->actual_length == endpoint->rcv_transferSize))) 
                {

                        //urb_append_irq(&endpoint->rcv, endpoint->rcv_urb);
                        TRACE_MSG16("pxa_out_ep0: call recv_setup: len: %x transfer: %x", 
                                        endpoint->rcv_urb->actual_length, endpoint->rcv_transferSize);

                        TRACE_RECV(endpoint->rcv_urb->buffer);
                        TRACE_RECV(endpoint->rcv_urb->buffer+8);
                        TRACE_RECV(endpoint->rcv_urb->buffer+16);

                        endpoint->state = WAIT_FOR_SETUP;
		        TRACE_MSG32("ep0->state <- WAIT_FOR_SETUP %d",endpoint->state);

                        if (usbd_recv_setup(ep0_urb)) {
                                // setup processing failed 
                                UDCCS0 = UDCCS0_FST;
                                TRACE_MSG32("      --> bad setup FST: %02x", udccs0);
                                return;
                        }
#ifdef CONFIG_MOTO_PST_FD
			/* MOTO_PST_FD needs to be able to postpone
			   responding until data has been collected,
			   which could be several ms, so we can't wait.
			   With any luck, no setup packet with data
			   needs this, but just in case... */
			if (-1 == ep0_urb->actual_length) {
				endpoint->state = DATA_STATE_PENDING_XMIT;
                                TRACE_MSG32("      <-- setup pending xmit: %02x", UDCCS0);
                        	// Clear SA and OPR bits
				UDCCS0 = UDCCS0_SA | UDCCS0_OPR;

				//PXA_REGS( UDCCS0, "     <-- setup recv");
				return;
			}
#endif
                }
        }
        else {
                TRACE_MSG32("pxa_out_ep0 flush", 0);
                pxa_out_flush(ep);
        }
        UDCCS0 = UDCCS0_IPR;
        UDCCS0 = UDCCS0_OPR;
}

void __inline__ pxa_ep0setup(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        if ((udccs0 & (UDCCS0_SA | UDCCS0_OPR | UDCCS0_RNE)) == (UDCCS0_SA | UDCCS0_OPR | UDCCS0_RNE)) {

                int len = 0;
                int max = 8;
                unsigned char *cp = (unsigned char *)&ep0_urb->device_request;

                TRACE_MSG32("      --> setup: %02x", udccs0);

                //memset(cp, 0, max);

                while (max-- /*&& (UDCCS0 & UDCCS0_RNE)*/) {
                        len++;
                        *cp++ = UDDR0;
                }

                TRACE_SETUP(&ep0_urb->device_request);

                // if this is host 2 device with non-zero wLength we need to wait for data
                if (((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) &&
                        (le16_to_cpu (ep0_urb->device_request.wLength)))
                {
                        endpoint->rcv_transferSize = le16_to_cpu(ep0_urb->device_request.wLength);
                        ep0_urb->actual_length = 0;
                        endpoint->rcv_urb = ep0_urb;
                        endpoint->state = DATA_STATE_RECV;
                        memset(ep0_urb->buffer, 0, ep0_urb->buffer_length);
                        TRACE_MSG32("      --> setup host2device: %02x", ep0_urb->device_request.wLength);
                //        UDCCS0 = UDCCS0_IPR;
                        UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
                        return;
                }

                // process setup packet
                ep0_urb->actual_length = 0;
                memset(ep0_urb->buffer, 0, ep0_urb->buffer_length);
                if (usbd_recv_setup(ep0_urb)) {
                        // setup processing failed 
                        UDCCS0 = UDCCS0_FST;
                        endpoint->state = WAIT_FOR_SETUP;
                        TRACE_MSG32("      --> bad setup FST: %02x", udccs0);
                        return;
                }
                TRACE_MSG32("         --> recv_setup finished actual: %d", ep0_urb->actual_length);
	
#ifdef CONFIG_MOTO_PST_FD
			if (-1 == ep0_urb->actual_length) {
				endpoint->state = DATA_STATE_PENDING_XMIT;
                                TRACE_MSG32("      <-- setup pending xmit: %02x", UDCCS0);
                        	// Clear SA and OPR bits
				UDCCS0 = UDCCS0_SA | UDCCS0_OPR;

				//PXA_REGS( UDCCS0, "     <-- setup recv");
				return;
			}
#endif

                // check direction
                if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) 
                {
                        // Control Write - we are receiving more data from the host

                        // allow for premature IN (c.f. 12.5.3 #8)
                        UDCCS0 = UDCCS0_IPR;
                        TRACE_MSG32("      <-- H2D setup nodata: %02x",UDCCS0);
                }
                else {
                        // Control Read - we are sending data to the host

                        TRACE_MSG32("      <-- D2H setup: %02x",UDCCS0);
                        // verify that we have non-zero request length
                        if (!le16_to_cpu (ep0_urb->device_request.wLength)) {
                                udc_stall_ep (0);
                                TRACE_MSG32("      <-- setup stalling zero wLength: %02x", UDCCS0);
                                return;
                        }
                        // verify that we have non-zero length response
                        if (!ep0_urb->actual_length) {
                                udc_stall_ep (0);
                                TRACE_MSG32("      <-- setup stalling zero response: %02x", UDCCS0);
                                return;
                        }
#if 0
#ifdef CONFIG_MOTO_PST_FD
			/* MOTO_PST_FD needs to be able to postpone
			   responding until data has been collected,
			   which could be several ms, so we can't wait. */
			if (-1 == ep0_urb->actual_length) {
				endpoint->state = DATA_STATE_PENDING_XMIT;
                                TRACE_MSG32("      <-- setup pending xmit: %02x", UDCCS0);
                        	// Clear SA and OPR bits
				UDCCS0 = UDCCS0_SA | UDCCS0_OPR;

				//PXA_REGS( UDCCS0, "     <-- setup recv");
				return;
			}
#endif
#endif

                        TRACE_SENT(ep0_urb->buffer);
                        TRACE_SENT(ep0_urb->buffer+8);
                        TRACE_SENT(ep0_urb->buffer+24);

                        // start sending
                        endpoint->tx_urb = ep0_urb;
                        endpoint->sent = 0;
                        endpoint->last = 0;
                        endpoint->state = DATA_STATE_XMIT;
                	TRACE_MSG32("ep0->state %d (XMIT):", endpoint->state);
			pxa_ep0xmit(endpoint, UDCCS0);

                        // Clear SA and OPR bits
                        UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
                        TRACE_MSG32("      <-- setup data: %02x", UDCCS0);
                }
        }
}

void __inline__ pxa_ep0setup_rne(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        if ((udccs0 & (UDCCS0_SA | UDCCS0_OPR )) == (UDCCS0_SA | UDCCS0_OPR )) {

                int len = 0;
                int max = 8;
                unsigned char *cp = (unsigned char *)&ep0_urb->device_request;

                TRACE_MSG32("      --> setup: %02x", udccs0);

                //memset(cp, 0, max);

                while (max-- /*&& (UDCCS0 & UDCCS0_RNE)*/) {
                        len++;
                        *cp++ = UDDR0;
                }

                TRACE_SETUP(&ep0_urb->device_request);

                // if this is host 2 device with non-zero wLength we need to wait for data
                if (((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) &&
                        (le16_to_cpu (ep0_urb->device_request.wLength)))
                {
                        endpoint->rcv_transferSize = le16_to_cpu(ep0_urb->device_request.wLength);
                        ep0_urb->actual_length = 0;
                        endpoint->rcv_urb = ep0_urb;
                        endpoint->state = DATA_STATE_RECV;
                        memset(ep0_urb->buffer, 0, ep0_urb->buffer_length);
                        TRACE_MSG32("      --> setup host2device: %02x", ep0_urb->device_request.wLength);
                //        UDCCS0 = UDCCS0_IPR;
                        UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
                        return;
                }

                // process setup packet
                ep0_urb->actual_length = 0;
                memset(ep0_urb->buffer, 0, ep0_urb->buffer_length);
                if (usbd_recv_setup(ep0_urb)) {
                        // setup processing failed 
                        UDCCS0 = UDCCS0_FST;
                        endpoint->state = WAIT_FOR_SETUP;
                        TRACE_MSG32("      --> bad setup FST: %02x", udccs0);
                        return;
                }
                TRACE_MSG32("         --> recv_setup finished actual: %d", ep0_urb->actual_length);
	
#ifdef CONFIG_MOTO_PST_FD
			if (-1 == ep0_urb->actual_length) {
				endpoint->state = DATA_STATE_PENDING_XMIT;
                                TRACE_MSG32("      <-- setup pending xmit: %02x", UDCCS0);
                        	// Clear SA and OPR bits
				UDCCS0 = UDCCS0_SA | UDCCS0_OPR;

				//PXA_REGS( UDCCS0, "     <-- setup recv");
				return;
			}
#endif

                // check direction
                if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) 
                {
                        // Control Write - we are receiving more data from the host

                        // allow for premature IN (c.f. 12.5.3 #8)
                        UDCCS0 = UDCCS0_IPR;
                        TRACE_MSG32("      <-- H2D setup nodata: %02x",UDCCS0);
                }
                else {
                        // Control Read - we are sending data to the host

                        TRACE_MSG32("      <-- D2H setup: %02x",UDCCS0);
                        // verify that we have non-zero request length
                        if (!le16_to_cpu (ep0_urb->device_request.wLength)) {
                                udc_stall_ep (0);
                                TRACE_MSG32("      <-- setup stalling zero wLength: %02x", UDCCS0);
                                return;
                        }
                        // verify that we have non-zero length response
                        if (!ep0_urb->actual_length) {
                                udc_stall_ep (0);
                                TRACE_MSG32("      <-- setup stalling zero response: %02x", UDCCS0);
                                return;
                        }
#if 0
#ifdef CONFIG_MOTO_PST_FD
			/* MOTO_PST_FD needs to be able to postpone
			   responding until data has been collected,
			   which could be several ms, so we can't wait. */
			if (-1 == ep0_urb->actual_length) {
				endpoint->state = DATA_STATE_PENDING_XMIT;
                                TRACE_MSG32("      <-- setup pending xmit: %02x", UDCCS0);
                        	// Clear SA and OPR bits
				UDCCS0 = UDCCS0_SA | UDCCS0_OPR;

				//PXA_REGS( UDCCS0, "     <-- setup recv");
				return;
			}
#endif
#endif

                        TRACE_SENT(ep0_urb->buffer);
                        TRACE_SENT(ep0_urb->buffer+8);
                        TRACE_SENT(ep0_urb->buffer+24);

                        // start sending
                        endpoint->tx_urb = ep0_urb;
                        endpoint->sent = 0;
                        endpoint->last = 0;
                        endpoint->state = DATA_STATE_XMIT;
                	TRACE_MSG32("ep0->state %d (XMIT):", endpoint->state);
			pxa_ep0xmit(endpoint, UDCCS0);

                        // Clear SA and OPR bits
                        UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
                        TRACE_MSG32("      <-- setup data: %02x", UDCCS0);
                }
        }
}

static __inline__ void pxa_ep0(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        int j = 0;

        TRACE_MSG32("  --> ep0: %02x", udccs0);
        
#ifdef CONFIG_MOTO_PST_FD
        if ((NULL == endpoint || endpoint->state != DATA_STATE_PENDING_XMIT) &&(udccs0 & UDCCS0_SST)) 
#else
        if ((udccs0 & UDCCS0_SST)) 
#endif
     	{
     		pxa_ep_reset_irs(0);
                UDCCS0 = UDCCS0_SST;
                TRACE_MSG32("  --> ep0 clear SST: %02x", udccs0);
                if (endpoint) {
                        endpoint->state = WAIT_FOR_SETUP;
                        endpoint->tx_urb = NULL;
                }
                return;
        }


        if (!endpoint) {
                printk(KERN_INFO"ep0[%d:%d] endpoint Zero is NULL CS0[%02x]\n", udc_interrupts, jifs(), UDCCS0);
                pxa_ep_reset_irs(0);
                UDCCS0 = UDCCS0_IPR | UDCCS0_OPR | UDCCS0_SA;
                TRACE_MSG32("  ep0 NULL: %02x", UDCCS0);
                return;
        }

        if (endpoint->tx_urb) {
                usbd_tx_complete_irq (endpoint, 0);
                if (!endpoint->tx_urb) {
                        if (endpoint->state != DATA_STATE_NEED_ZLP) {
                                endpoint->state = WAIT_FOR_OUT_STATUS;
		        	TRACE_MSG32("ep0->state <- WAIT_FOR_OUT_STATUS %d",endpoint->state);
                        }
                }
        }

#ifdef CONFIG_MOTO_PST_FD
        if ((endpoint->state != DATA_STATE_PENDING_XMIT) && (endpoint->state != WAIT_FOR_SETUP) && (udccs0 & UDCCS0_SA))
#else
        if ((endpoint->state != WAIT_FOR_SETUP) && (udccs0 & UDCCS0_SA))
#endif
	{
                TRACE_MSG32("  --> ep0 early SA: %02x", udccs0);
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
        }

        switch (endpoint->state) {
        case DATA_STATE_NEED_ZLP:
                UDCCS0 = UDCCS0_IPR;
                endpoint->state = WAIT_FOR_OUT_STATUS;
                TRACE_MSG32("  --> ep0 need ZLP: %02x", udccs0);
                break;

        case WAIT_FOR_OUT_STATUS:
                if ((udccs0 & (UDCCS0_OPR | UDCCS0_SA)) == UDCCS0_OPR) {
                        UDCCS0 = UDCCS0_OPR; /*UDCCS0 |= UDCCS0_OPR;*/
                }
                TRACE_MSG32("  --> ep0 WAIT for STATUS: %02x", UDCCS0);
                endpoint->state = WAIT_FOR_SETUP;

        case WAIT_FOR_SETUP:
	case DATA_STATE_RECV:
                do {
                        TRACE_MSG32("  UDCCS0: %02x", UDCCS0);
                        switch (endpoint->state) {
                        case DATA_STATE_RECV:
                                pxa_out_ep0(0, endpoint, UDCCS0);
                                UDCCS0 = UDCCS0_OPR; /*UDCCSO |= UDCCS0_OPR;*/
                                //pxa_kill(1);
                                break;
                        case WAIT_FOR_SETUP:
                                pxa_ep0setup(endpoint, UDCCS0);
#ifdef CONFIG_MOTO_PST_FD
		        	if (NULL != endpoint &&
				    endpoint->state == DATA_STATE_PENDING_XMIT) 				{
					pxa_ep_reset_irs(0);
					if ( udccs0 & UDCCS0_SST ) {
						UDCCS0 = UDCCS0_SST | UDCCS0_OPR | UDCCS0_SA;
					}
					return;
				}
#endif
                                break;
                        default:
                                break;
                        }
                        if (udccs0 & UDCCS0_SST) {
                                pxa_ep_reset_irs(0);
                                UDCCS0 = UDCCS0_SST | UDCCS0_OPR | UDCCS0_SA;
                                TRACE_MSG32("  --> ep0 clear SST: %02x", udccs0);
                                if (endpoint) {
                                        endpoint->state = WAIT_FOR_SETUP;
                                        endpoint->tx_urb = NULL;
                                        endpoint->rcv_urb = NULL;
                                }
                                return;
                        }

                        if (j++ > 2) {
                                u32 udccs0 = UDCCS0;
                                TRACE_MSG32("  UDCCS0 wait: %02x", udccs0);
                                if ((udccs0 & (UDCCS0_OPR | UDCCS0_SA | UDCCS0_RNE)) == (UDCCS0_OPR | UDCCS0_SA)) {
                                	pxa_ep0setup_rne(endpoint, UDCCS0);
                				UDCCS0 = UDCCS0_OPR | UDCCS0_SA;
						TRACE_MSG32("  ep0 force: %02x", UDCCS0);
                                }
                                else {
                                        UDCCS0 = UDCCS0_OPR | UDCCS0_SA;
                                        TRACE_MSG32("  ep0 force and return: %02x", UDCCS0);
                                        break;
                                }
                        }
                } while (UDCCS0 & (UDCCS0_OPR | UDCCS0_RNE) && (j++ < 100));
                break;

        case DATA_STATE_XMIT:
#ifdef CONFIG_MOTO_PST_FD
        case DATA_STATE_PENDING_XMIT:
#endif
                pxa_ep0xmit(endpoint, UDCCS0);
                break;
        }

        pxa_ep_reset_irs(0);
        TRACE_MSG32("  <-- ep0: %02x", UDCCS0);
}

/* ********************************************************************************************* */
/* Interrupt Handler
 */

/**
 * int_hndlr - interrupt handler
 *
 */
static void int_hndlr (int irq, void *dev_id, struct pt_regs *regs)
{
        int usir1,usir0;
        int udccr;
        int ep;

        int_oscr = OSCR;
	udc_interrupts++;


#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)
        pxa_kill();
#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

        //pxa_kill(0);

        // check for common, high priority interrupts first, i.e. per endpoint service requests
        // XXX if ISO supported it might be necessary to give the ISO endpoints priority
        
        //TRACE_MSG16("------------------------> USIRO: %02x UDCCR: %02x", USIR0, UDCCR);

        usir0 = USIR0;
	usir1 = USIR1;
	while (usir0  || usir1) {
                u32 udccs0 = UDCCS0;

                u32 cs2 = UDCCSN(2); // XXX this seems to help with short packet reads, bizarre!

                //TRACE_MSG8("------------------------> USIRO: %02x UDCCSN: %02x %02x", usiro, UDCCSN(1), UDCCSN(2), 0);

                for (ep = 0; usir0; usir0 >>= 1, ep++) {
                        if (usir0 & 1) {
                                switch (ep_maps[ep].eptype) {
                                case ep_control:
                                        pxa_ep0(ep_endpoints[0], udccs0);
                                        break;

                                case ep_bulk_in:
                                case ep_interrupt:
                                        //pxa_kill(1);
                                        pxa_in_n(ep, ep_endpoints[ep]);
                                        break;

                                case ep_bulk_out:
#ifndef USE_DMA_OUT
                                        pxa_out_n(ep, ep_endpoints[ep], cs2);
                                        break;
#endif /* USE_DMA_OUT */
                                case ep_iso_in:
                                case ep_iso_out:
                                        pxa_ep_reset_irs(ep);
                                        break;
                                }
                        }
                }
		for(ep = 8; usir1; usir1 >>=1, ep++) {
                        if (usir1 & 1) {
                                switch (ep_maps[ep].eptype) {
				case ep_control:
					break;

	                        case ep_bulk_in:
                                case ep_interrupt:
                                        //pxa_kill(1);
					//pxa_ep_reset_irs(ep);
                                        pxa_in_n(ep, ep_endpoints[ep]);
					//pxa_ep_reset_irs(ep);
					//printk("send in interrupt\n");
                                        break;

                                case ep_bulk_out:
#ifndef USE_DMA_OUT
                                        pxa_out_n(ep, ep_endpoints[ep], cs2);
                                        break;
#endif /* USE_DMA_OUT */
                                case ep_iso_in:
                                case ep_iso_out:
                                        pxa_ep_reset_irs(ep);
                                        break;
                                }
                        }
		}
        }

        // sof interrupt
        if (UFNHR & UFNHR_SIR) {
                UFNHR = UFNHR_SIR;
        }

        // uncommon interrupts
        if ((udccr = UDCCR) & (UDCCR_RSTIR | UDCCR_RESIR | UDCCR_SUSIR)) {

                // UDC Reset
                if (udccr & UDCCR_RSTIR) {
                        TRACE_MSG32("------------------------> Reset: %02x", udccr);
                        //printk(KERN_INFO"int_hndlr[%d:%d] Reset\n", udc_interrupts, jifs());
                                        
                        udc_suspended = 0;
                        usbd_device_event (udc_device, DEVICE_RESET, 0);
                        usbd_device_event (udc_device, DEVICE_ADDRESS_ASSIGNED, 0);	
                        UDCCR |= UDCCR_RSTIR;
                }

                // UDC Resume
                if (udccr & UDCCR_RESIR) {
                        TRACE_MSG32("------------------------> Resume: %02x", udccr);
                        if (udc_suspended) {
                                udc_suspended = 0;
                                usbd_device_event (udc_device, DEVICE_BUS_ACTIVITY, 0);
                        }
                        UDCCR |= UDCCR_RESIR;
                }

                // UDC Suspend
                if (udccr & UDCCR_SUSIR) {
                        TRACE_MSG32("------------------------> Suspend: %02x", udccr);
                        if (!udc_suspended) {
                                udc_suspended = 1;
                                usbd_device_event (udc_device, DEVICE_BUS_INACTIVE, 0);
                        }
                        UDCCR |= UDCCR_SUSIR;
                }
        }
        //TRACE_MSG16("<-- Interrupt: UDCCR: %02x UICR0: %02x", UDCCR, UICR0);
}


/* ********************************************************************************************* */


/* ********************************************************************************************* */
/*
 * Start of public functions.
 */

/**
 * udc_start_in_irq - start transmit
 * @endpoint:
 *
 * Called with interrupts disabled.
 * Matches declaration in usbd-bi.h
 */
void udc_start_in_irq (struct urb *urb)
{
        struct usb_endpoint_instance *endpoint = urb->endpoint;
#ifdef CONFIG_MOTO_PST_FD
	if( endpoint->endpoint_address == 0 ) {
		pxa_ep0xmit( endpoint, UDCCS0 );
		return;
	}
#if 0
	if (0 == (endpoint->endpoint_address & 0xf)) {
		// special ep0 transmit routine and URB handling needed
		// remove from ep0 tx queue, if present.
                TRACE_MSG32("start_in_irq: state=%d", endpoint->state);
                TRACE_MSG32("start_in_irq: length=%d", urb->actual_length);
		urb_detach_irq(urb);
		if (urb == endpoint->tx_urb) {
			endpoint->tx_urb = NULL;
		}
		if (endpoint->state != DATA_STATE_PENDING_XMIT) {
			// Host gave up and did something else, drop urb
                	TRACE_MSG32("not PENDING_XMIT: state=%d:", endpoint->state);
			usbd_dealloc_urb(urb);
			return;
		}
		/* Avoid memory management issues by copying urb
		   into fixed ep0_urb and releasing old one */
		if (NULL == ep0_urb || NULL == ep0_urb->buffer ||
		    ep0_urb->buffer_length < urb->actual_length) {
			// It won't fit, give up
			printk(KERN_ERR,"no room for ep0 URB data\n");
			usbd_dealloc_urb(urb);
			return;
		} 
		ep0_urb->device_request = urb->device_request;
		ep0_urb->status = urb->status;
		ep0_urb->jiffies = urb->jiffies;
		ep0_urb->event = urb->event;
		ep0_urb->data = urb->data;
		ep0_urb->privdata = urb->privdata;
		ep0_urb->actual_length = urb->actual_length;
		memcpy(ep0_urb->buffer,urb->buffer,urb->actual_length);
		// Finished with old urb, so...
		usbd_dealloc_urb(urb);

		TRACE_SENT(ep0_urb->buffer);
		TRACE_SENT(ep0_urb->buffer+8);
		TRACE_SENT(ep0_urb->buffer+24);

		//printk("\nep0_urb->buffer=%x\n", ep0_urb->buffer);
                TRACE_MSG32("ep0_urb->buffer=%x",(unsigned long) (ep0_urb->buffer));
		// start sending
		endpoint->tx_urb = ep0_urb;
		endpoint->sent = 0;
		endpoint->last = 0;
		endpoint->state = DATA_STATE_XMIT;
		TRACE_MSG32("ep0->state <- XMIT %d",endpoint->state);
		pxa_ep0xmit(endpoint,UDCCS0);
		return;
	}
#endif
#endif
        udc_interrupts++;
        //TRACE_MSG("udc_start_in_irq");
        if (UDCCSN(endpoint->endpoint_address & 0xf) & UDCCS_BI_TFS) {
                pxa_start_n (endpoint->endpoint_address & 0xf, endpoint);
        }
}

void udc_cancel_in_irq(struct urb *urb)
{
        pxa_in_flush(urb->endpoint->endpoint_address & 0xf);
}

//send ACK in UDCCFR
void udc_send_ack_to_set_conf_req(void){
  //a17400 send req  in UDCCFR
  __REG(0x40600008)|=0x80;
  
}

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
        udc_disable_interrupts (NULL);
	//a17400 enable user control in UDCCFR
	__REG(0x40600008)=0x7f;
	return 0;
}



/**
 * udc_stall_ep - stall endpoint
 * @ep: physical endpoint
 *
 * Stall the endpoint.
 */
void udc_stall_ep (unsigned int ep)
{
	if (ep < UDC_MAX_ENDPOINTS) {
		// stall
	}
}


/**
 * udc_reset_ep - reset endpoint
 * @ep: physical endpoint
 * reset the endpoint.
 *
 * returns : 0 if ok, -1 otherwise
 */
void udc_reset_ep (unsigned int ep)
{
	if (ep < UDC_MAX_ENDPOINTS) {
	}
}


/**
 * udc_endpoint_halted - is endpoint halted
 * @ep:
 *
 * Return non-zero if endpoint is halted
 */
int udc_endpoint_halted (unsigned int ep)
{
	return 0;
}


/**
 * udc_set_address - set the USB address for this device
 * @address:
 *
 * Called from control endpoint function after it decodes a set address setup packet.
 */
void udc_set_address (unsigned char address)
{
}

/**
 * udc_serial_init - set a serial number if available
 */
int __init udc_serial_init (struct usb_bus_instance *bus)
{
	return -EINVAL;
}

/* ********************************************************************************************* */

/**
 * udc_max_endpoints - max physical endpoints 
 *
 * Return number of physical endpoints.
 */
int udc_max_endpoints (void)
{
	return UDC_MAX_ENDPOINTS;
}


/**
 * udc_check_ep - check logical endpoint 
 * @lep:
 *
 * Return physical endpoint number to use for this logical endpoint or zero if not valid.
 */
int udc_check_ep (int logical_endpoint, int packetsize)
{
        // XXX check ep table

        return ( ((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) || (packetsize > 64)) ?  0 : (logical_endpoint & 0xf);
}


/**
 * udc_set_ep - setup endpoint 
 * @ep:
 * @endpoint:
 *
 * Associate a physical endpoint with endpoint_instance
 */
void udc_setup_ep (struct usb_device_instance *device, unsigned int ep,
		   struct usb_endpoint_instance *endpoint)
{

        TRACE_MSG32("setup ep: %d:", ep);

	if (ep < UDC_MAX_ENDPOINTS) {

		ep_endpoints[ep] = endpoint;

		// ep0
		if (ep == 0) {
			//usbd_fill_rcv (device, endpoint, 5);
                        pxa_enable_ep_interrupt(ep_endpoints[ep]->endpoint_address);
		}
		// IN
		else if (endpoint->endpoint_address & 0x80) {
#ifdef USE_DMA_IN
                        usb_stream_in.ep = ep;
                        usb_stream_in.endpoint = endpoint;
                        usb_stream_in.drcmr = ep_maps[ep].drcmr;
                        usb_stream_in.dev_addr = UDDRN(ep);
                        for (i = 0; i < usb_stream_in.num; i++) {
                                usb_stream_in.dma_desc[i].dsadr = usb_stream_in.dma_buf_phys + (i * usb_stream_in.size);
                                usb_stream_in.dma_desc[i].dtadr = ep_maps[ep].dev_addr;
                                usb_stream_in.dma_desc[i].dcmd = usb_stream_in.size;
                        }
#endif /* USE_DMA_IN */
                        pxa_enable_ep_interrupt(ep_endpoints[ep]->endpoint_address);
		}
		// OUT
		else if (endpoint->endpoint_address) {

#if 0
			usbd_fill_rcv (device, endpoint, 8);
#else
			usbd_fill_rcv (device, endpoint, 32);		// w20146
#endif
			
			endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
#ifdef USE_DMA_OUT
                        //printk(KERN_INFO"udc_setup_ep: OUT alloc urbs ep: %d addr: %d num: %d size: %d dma_ch: %d\n",
                        //                ep, endpoint->endpoint_address, usb_stream_out.num, 
                        //                usb_stream_out.size, usb_stream_out.dma_ch);
                        usb_stream_out.ep = ep;
                        usb_stream_out.endpoint = endpoint;
                        usb_stream_out.drcmr = ep_maps[ep].drcmr;
                        usb_stream_out.dev_addr = _UDDRN[ep];
                        for (i = 0; i < usb_stream_out.num; i++) {

                                usb_stream_out.dma_desc[i].ddadr = usb_stream_out.dma_desc_phys + 
                                        ((i < (usb_stream_out.num - 1)) ? (i + 1) * sizeof(pxa_dma_desc) : 0);

                                usb_stream_out.dma_desc[i].dsadr = usb_stream_out.dev_addr;
                                usb_stream_out.dma_desc[i].dtadr = usb_stream_out.dma_buf_phys + (i * usb_stream_out.size);
                                usb_stream_out.dma_desc[i].dcmd = usb_stream_out.size | usb_stream_out.dcmd;

                                //printk(KERN_INFO"udc_setup_ep: dma_desc[%d] ddadr: %x dsadr: %x dtadr: %x dcmd: %x\n",
                                //                i,
                                //                usb_stream_out.dma_desc[i].ddadr,
                                //                usb_stream_out.dma_desc[i].dsadr,
                                //                usb_stream_out.dma_desc[i].dtadr,
                                //                usb_stream_out.dma_desc[i].dcmd
                                //                );
                        }
#if 1
                        DDADR(usb_stream_out.dma_ch) = usb_stream_out.buffers[0].dma_desc->ddadr;
                        //printk(KERN_INFO"udc_setup_ep: DDAR: %p %p\n", 
                        //                DDADR(usb_stream_out.dma_ch), usb_stream_out.buffers[0].dma_desc->ddadr);
                        pxa_start_out_dma(ep, endpoint, &usb_stream_out);
#endif
#endif /* USE_DMA_OUT */
                        pxa_enable_ep_interrupt(ep_endpoints[ep]->endpoint_address);
		}
	}
}

/**
 * udc_disable_ep - disable endpoint
 * @ep:
 *
 * Disable specified endpoint 
 */
void udc_disable_ep (unsigned int ep)
{
	if (ep < UDC_MAX_ENDPOINTS) {
		struct usb_endpoint_instance *endpoint;

		if ((endpoint = ep_endpoints[ep])) {
			ep_endpoints[ep] = NULL;
			usbd_flush_ep (endpoint);
		}
		if (ep == 0) {
                        //printk(KERN_INFO"udc_setup_ep: 0 do nothing\n");
		}
		// IN
		else if (endpoint->endpoint_address & 0x80) {
#ifdef USE_DMA_IN
                        usb_stream_in.ep = 0;
                        usb_stream_in.endpoint = NULL;
#endif /* USE_DMA_IN */
		}
		// OUT
		else if (endpoint->endpoint_address) {
#if 0
#ifdef USE_DMA_OUT
                        usb_stream_out.ep = 0;
                        usb_stream_out.endpoint = NULL;
#endif
#endif
		}
	}
}

/* ********************************************************************************************* */

/**
 * udc_connected - is the USB cable connected
 *
 * Return non-zeron if cable is connected.
 */
int udc_connected ()
{
	return 1;
}

/**
 * udc_connect - enable pullup resistor
 *
 * Turn on the USB connection by enabling the pullup resistor.
 */
void udc_connect (void)
{
#ifdef CONFIG_SABINAL_DISCOVERY
        /* Set GPIO pin function to I/O */
        GPAFR1_U &= ~(0x3 << ((USBD_CONNECT_GPIO & 0xF) << 1));
        /* Set pin direction to output */
        GPDR(1) |= GPIO_GPIO(USBD_CONNECT_GPIO);
#if defined(USBD_CONNECT_HIGH)
	/* Set GPIO pin high to connect */
        printk(KERN_INFO"udc_connect: HIGH\n");
	GPSR(1) |= GPIO_GPIO(USBD_CONNECT_GPIO);
#else
	/* Set GPIO pin low to connect */
        printk(KERN_INFO"udc_connect: LOW\n");
	GPCR(1) |= GPIO_GPIO(USBD_CONNECT_GPIO);
#endif
#else
#endif
#ifdef CONFIG_MOTO_EZX
	SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
	SSP_PCAP_bit_set(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
#endif
}

/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect (void)
{
#ifdef CONFIG_SABINAL_DISCOVERY
        /* Set GPIO pin function to I/O */
        GPAFR1_U &= ~(0x3 << ((USBD_CONNECT_GPIO & 0xF) << 1));
        /* Set pin direction to output */
        GPDR(1) |= GPIO_GPIO(USBD_CONNECT_GPIO);
#if defined(USBD_CONNECT_HIGH)
	/* Set GPIO pin low to disconnect */
	GPCR(1) |= GPIO_GPIO(USBD_CONNECT_GPIO);
#else
	/* Set GPIO pin high to disconnect */
	GPSR(1) |= GPIO_GPIO(USBD_CONNECT_GPIO);
#endif
#else
#endif
#ifdef CONFIG_MOTO_EZX
	SSP_PCAP_get_bit_from_PCAP(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
	SSP_PCAP_bit_clean(SSP_PCAP_ADJ_BIT_BUSCTRL_USB_PU);
#endif
}

#if 0
/**
 * udc_int_hndlr_cable - interrupt handler for cable
 */
static void udc_int_hndlr_cable (int irq, void *dev_id, struct pt_regs *regs)
{
	// GPIOn interrupt
}
#endif

/* ********************************************************************************************* */

/**
 * udc_enable_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts (struct usb_device_instance *device)
{
        int i;

        UDCCR &= ~UDCCR_SRM;    // enable suspend interrupt
        UDCCR |= UDCCR_REM;     // disable resume interrupt

        // XXX SOF UFNHR &= ~UFNHR_SIM;

        // always enable control endpoint
        pxa_enable_ep_interrupt(0);

        for (i = 1; i < UDC_MAX_ENDPOINTS; i++) {
                if (ep_endpoints[i] && ep_endpoints[i]->endpoint_address) {
                        pxa_enable_ep_interrupt(ep_endpoints[i]->endpoint_address);
                }
        }
}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts (struct usb_device_instance *device)
{
        UDCCR &= ~UDCCR_REM;    // enable resume interrupt
        UDCCR |= UDCCR_SRM;     // disable suspend interrupt
}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts (struct usb_device_instance *device)
{
        UICR0 = UICR1 = 0xff;                   // disable endpoint interrupts
        UFNHR |= UFNHR_SIM;                     // disable sof interrupt
        UDCCR |= UDCCR_REM | UDCCR_SRM;         // disable suspend and resume interrupt
}

/* ********************************************************************************************* */

/**
 * udc_ep0_packetsize - return ep0 packetsize
 */
int udc_ep0_packetsize (void)
{
	return EP0_PACKETSIZE;
}

/**
 * udc_enable - enable the UDC
 *
 * Switch on the UDC
 */
void udc_enable (struct usb_device_instance *device)
{
	// save the device structure pointer
	udc_device = device;

        printk (KERN_INFO "udc_enable:\n");

	// ep0 urb
	if (!ep0_urb) {
#ifndef CONFIG_MOTO_PST_FD
		if (!(ep0_urb = usbd_alloc_urb (device, 0, 512))) {
#else
		if (!(ep0_urb = usbd_alloc_urb (device, 0, 8192))) {
#endif
			printk (KERN_ERR "udc_enable: usbd_alloc_urb failed\n");
		}
	} 
        else {
		printk (KERN_ERR "udc_enable: ep0_urb already allocated\n");
	}
        printk (KERN_INFO "udc_enable: ep0_urb %p buffer_length: %d\n", ep0_urb, ep0_urb->buffer_length);


	// enable UDC

	// c.f. 3.6.2 Clock Enable Register
	CKEN |= CKEN11_USB;

        // c.f. 12.4.11 GPIOn and GPIOx
        // enable cable interrupt
        

        // c.f. 12.5 UDC Operation - after reset on EP0 interrupt is enabled.

        // c.f. 12.6.1.1 UDC Enable
        UDCCR |= UDCCR_UDE | UDCCR_REM;
}


/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable (void)
{
        struct urb *urb = ep0_urb;

        ep0_urb = NULL;


	// disable UDC
        // c.f. 12.6.1.1 UDC Enable
        UDCCR &= ~( UDCCR_UDE | UDCCR_REM );

	// c.f. 3.6.2 Clock Enable Register
	CKEN &= ~CKEN11_USB;

        // disable cable interrupt
        
	// reset device pointer
	udc_device = NULL;

	// ep0 urb
        usbd_dealloc_urb (urb);
}


/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup_events (struct usb_device_instance *device)
{
	usbd_device_event (device, DEVICE_INIT, 0);
	usbd_device_event (device, DEVICE_CREATE, 0);
	usbd_device_event (device, DEVICE_HUB_CONFIGURED, 0);
	usbd_device_event (device, DEVICE_RESET, 0);
}


/* ********************************************************************************************* */
/**
 * udc_name - return name of USB Device Controller
 */
char *udc_name (void)
{
	return UDC_NAME;
}

/**
 * udc_request_udc_irq - request UDC interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_udc_irq ()
{
	// request IRQ  and IO region
	if (request_irq (IRQ_USB, int_hndlr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
	     UDC_NAME " USBD Bus Interface", NULL) != 0) 
        {
		printk (KERN_INFO "usb_ctl: Couldn't request USB irq\n");
		return -EINVAL;
	}
	init_timer(&pxa_timer);
	pxa_timer.function = pxa_timeout;
	return 0;
}

/**
 * udc_request_cable_irq - request Cable interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_cable_irq ()
{
#ifdef XXXX_CABLE_IRQ
	// request IRQ  and IO region
	if (request_irq
	    (XXXX_CABLE_IRQ, int_hndlr, SA_INTERRUPT | SA_SAMPLE_RANDOM, UDC_NAME " Cable",
	     NULL) != 0) {
		printk (KERN_INFO "usb_ctl: Couldn't request USB irq\n");
		return -EINVAL;
	}
#endif
	return 0;
}

#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)


void pxa_free_dma_stream(usb_stream_t *usb_stream)
{
        printk(KERN_INFO"pxa_free_dma_stream:\n");
        if (usb_stream->dma_ch >= 0) {
                printk(KERN_INFO"pxa_free_dma_stream: pxa_free_dma\n");
                pxa_free_dma(usb_stream->dma_ch);
                usb_stream->dma_ch = -1;
        }
#if 1
        printk(KERN_INFO"pxa_free_dma_stream: dma_buf: %p\n", usb_stream->dma_buf);
        if (usb_stream->dma_buf) {
                printk(KERN_INFO"pxa_free_dma_stream: consistent_free dma_buf\n");
                consistent_free(usb_stream->dma_buf, usb_stream->num * usb_stream->size, usb_stream->dma_buf_phys);
                usb_stream->dma_buf = NULL;
        }

        printk(KERN_INFO"pxa_free_dma_stream: dma_desc: %p\n", usb_stream->dma_desc);
        if (usb_stream->dma_desc) {
                printk(KERN_INFO"pxa_free_dma_stream: consistent_free dma_desc\n");
                consistent_free(usb_stream->dma_desc, usb_stream->num * sizeof(pxa_dma_desc), usb_stream->dma_desc_phys);
                usb_stream->dma_desc = NULL;
        }

        printk(KERN_INFO"pxa_free_dma_stream: buffers: %p\n", usb_stream->buffers);
        if (usb_stream->buffers) {
                printk(KERN_INFO"pxa_free_dma_stream: lkfree buffers\n");
                lkfree(usb_stream->buffers);
                usb_stream->buffers = NULL;
        }
#endif
        printk(KERN_INFO"pxa_free_dma_stream: done\n");
}


int pxa_setup_dma(/*int ep, struct usb_endpoint_instance *endpoint,*/ usb_stream_t *usb_stream)
{
	usb_buf_t *buffers;

        pxa_dma_desc *dma_desc = NULL;
        dma_addr_t dma_desc_phys = 0;

        unsigned char *dma_buf = NULL;
        dma_addr_t dma_buf_phys = 0;

        int i;
        int num;
        int size;

        usb_stream->num = num = 4;
        usb_stream->size = size = 64;

        // DMA buffer structure array
        if (!(buffers = ckmalloc(sizeof(usb_buf_t) * usb_stream->num, GFP_KERNEL))) {
                goto err;
        }
        memset(buffers, 0, sizeof(usb_buf_t) * usb_stream->num);
        usb_stream->buffers = buffers;


        // DMA Buffer
        if (!(dma_buf = consistent_alloc(GFP_KERNEL, num * size, &dma_buf_phys))) {
                goto err;
        }
        usb_stream->dma_buf = dma_buf;
        usb_stream->dma_buf_phys = dma_buf_phys;


        // DMA descriptor array
        if (!(dma_desc = consistent_alloc(GFP_KERNEL, num * sizeof(pxa_dma_desc), &dma_desc_phys))) {
                goto err;
        }
        usb_stream->dma_desc = dma_desc;
        usb_stream->dma_desc_phys = dma_desc_phys;

        printk(KERN_INFO"dma stream dma_buf: %p dma_buf_phys: %p dma_desc: %p dma_desc_phys: %p\n",
                        usb_stream->dma_buf,
                        usb_stream->dma_buf_phys,
                        usb_stream->dma_desc,
                        usb_stream->dma_desc_phys
              );
        // setup arrays
        //
        for (i = 0; i < num; i++) {

                // usb_stream->dma_desc[i]

                //dma_desc[i].ddadr = dma_desc_phys + (i < (num - 1)) ?  (i + 1) * sizeof(pxa_dma_desc) : 0;

                // usb_stream->buffers[i]
                buffers[i].dma_buf = dma_buf + (i * size);
                buffers[i].dma_buf_phys = dma_buf_phys + (i * size);
                buffers[i].dma_desc = dma_desc + (i * sizeof(pxa_dma_desc));
                buffers[i].dma_desc_phys = dma_desc_phys + (i * sizeof(pxa_dma_desc));

                printk(KERN_INFO"dma buf[%d] dma_buf: %p dma_buf_phys: %p dma_desc: %p dma_desc_phys: %p\n",
                                i,
                                buffers[i].dma_buf,
                                buffers[i].dma_buf_phys,
                                buffers[i].dma_desc,
                                buffers[i].dma_desc_phys
                                );
        }

        // prime DMA

        //DDADR(usb_stream->dma_ch) = buffers[0].dma_desc->ddadr;

        return 0;
err:
        return -ENOMEM;
}
/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io ()
{
#ifdef USE_DMA_IN
        // XXX verify prio
        if ((usb_stream_in.dma_ch = pxa_request_dma(usb_stream_in.name, 
                                        DMA_PRIO_HIGH, pxa_usb_in_dma_irq, &usb_stream_in)) < 0) 
        {
                printk(KERN_INFO"pxa: failed to allocate IN dma channel\n");
                return -EINVAL;
        }
        if (!pxa_setup_dma(&usb_stream_in)) {
                pxa_free_dma_stream(&usb_stream_in);
        }
        printk(KERN_INFO"pxa: DMA channels in: %d\n", usb_stream_in.dma_ch);
#endif /* USE_DMA_IN */

#ifdef USE_DMA_OUT
        if ((usb_stream_out.dma_ch = pxa_request_dma(usb_stream_out.name, 
                                        DMA_PRIO_HIGH, pxa_usb_out_dma_irq, &usb_stream_out)) < 0) 
        {
                printk(KERN_INFO"pxa: failed to allocate OUT dma channel\n");
#ifdef USE_DMA_IN
                pxa_free_dma_stream(&usb_stream_in);
#endif /* USE_DMA_IN */
                return -EINVAL;
        }
        if (!pxa_setup_dma(&usb_stream_out)) {
#ifdef USE_DMA_IN
                pxa_free_dma_stream(&usb_stream_in);
#endif /* USE_DMA_IN */
                pxa_free_dma_stream(&usb_stream_out);
                return -EINVAL;
        }

        printk(KERN_INFO"pxa: DMA channels out: %d\n", usb_stream_out.dma_ch);
#endif /* USE_DMA_OUT */

	return 0;
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io ()
{

#ifdef USE_DMA_IN
        pxa_free_dma_stream(&usb_stream_in);
#endif /* USE_DMA_IN */
#ifdef USE_DMA_OUT
        pxa_free_dma_stream(&usb_stream_out);
#endif /* USE_DMA_OUT */
}

#else /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io ()
{
	return 0;
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io ()
{
}

#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */


/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq ()
{
	free_irq (IRQ_USB, NULL);
}

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq ()
{
#ifdef XXXX_IRQ
	free_irq (XXXX_CABLE_IRQ, NULL);
#endif
}


/**
 * udc_regs - dump registers
 *
 * Dump registers with printk
 */
void udc_regs (void)
{
	printk ("[%d:%d] CCR[%02x] UICR[%02x %02x] UFNH[%02x %02x] UDCCS[%02x %02x %02x %02x]\n", 
                        udc_interrupts, jifs(), UDCCR, UICR1, UICR0, UFNHR, UFNLR, UDCCS0, UDCCS1, UDCCS2, UDCCS5);
}


