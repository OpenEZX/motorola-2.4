/*
 * linux/drivers/usb/device/bi/pxa.c -- Intel PXA250 USB Device Controller driver. 
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2002, 2003 RTSoft
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *
 * Minor fixes and basic DMA support by:
 *      Dmitry Antipov <antipov@rtsoft.msk.ru>
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
 * Testing notes:
 *
 * This code was developed on the Intel Lubbock with Cotulla PXA250 processor.
 * Note DMA OUT seems works starting from C0 chip revision only.
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/delay.h>

#include <asm/arch-pxa/pxa-regs.h>
#include <asm/hardware.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/dma.h>

#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../usbd-module.h"
#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

#include "pxa.h"

MODULE_AUTHOR ("sl@lineo.com, tbr@lineo.com, antipov@rtsoft.msk.ru");
MODULE_DESCRIPTION ("Xscale USB Device Bus Interface");
MODULE_LICENSE ("GPL");
USBD_MODULE_INFO ("pxa_bi 0.2-alpha");

#if defined(CONFIG_USBD_NET) || defined(CONFIG_USBD_NET_MODULE)
//#define USE_DMA 1

MODULE_PARM (dma, "i");
MODULE_PARM_DESC (dma, "use DMA (0 - don't use, 1 - DMA IN only, "
		       "2 - DMA OUT only, 3 - both (default)");
#endif

#ifdef USE_DMA

#if defined(CONFIG_USBD_NET_IN_PKTSIZE)
#define DMA_IN_BUFSIZE CONFIG_USBD_NET_IN_PKTSIZE
#else
#error "Can't determine buffer size for DMA IN"
#endif

#if defined(CONFIG_USBD_NET_IN_ENDPOINT)
#define IN_ENDPOINT CONFIG_USBD_NET_IN_ENDPOINT
#else
#error "Can't determine IN endpoint"
#endif

#if defined(CONFIG_USBD_NET_OUT_PKTSIZE)
#define DMA_OUT_BUFSIZE CONFIG_USBD_NET_OUT_PKTSIZE
#else
#error "Can't determine buffer size for DMA OUT"
#endif

#if defined(CONFIG_USBD_NET_OUT_ENDPOINT)
#define OUT_ENDPOINT CONFIG_USBD_NET_OUT_ENDPOINT
#else
#error "Can't determine OUT endpoint"
#endif

/* #define DMA_TRACEMSG(fmt, args...) printk (fmt, ## args) */
#define DMA_TRACEMSG(fmt, args...) 

#define PXA_DMA_IN 1
#define PXA_DMA_OUT 2
static int dma = PXA_DMA_IN |  PXA_DMA_OUT;

/* This datatype contains all information required for DMA transfers */
typedef struct usb_pxa_dma {
        int dmach;
        struct usb_endpoint_instance *endpoint;
        int size;
        int ep;
        unsigned char *dma_buf;
        dma_addr_t dma_buf_phys;
} usb_pxa_dma_t;

#endif /* USE_DMA */

unsigned int int_oscr;

#undef PXA_TRACE

#ifdef PXA_TRACE
typedef enum pxa_trace_type {
        pxa_regs, pxa_setup, pxa_xmit, pxa_ccr, pxa_iro
} pxa_trace_type_t;


typedef struct pxa_regs {
        u32     cs0;
        char *  msg;
} pxa_regs_t;

typedef struct pxa_xmit {
        u32     size;
} pxa_xmit_t;

typedef struct pxa_ccr {
        u32     ccr;
        char *  msg;
} pxa_ccr_t;

typedef struct pxa_iro {
        u32     iro;
        char *  msg;
} pxa_iro_t;

typedef struct pxa_trace {
        pxa_trace_type_t        trace_type;
        u32     interrupts;
        u32     ocsr;
        u64     jiffies;
        union {
                pxa_regs_t       regs;
                pxa_xmit_t       xmit;
                pxa_ccr_t        ccr;
                pxa_iro_t        iro;
                struct usb_device_request       setup;
        } trace;
} pxa_trace_t;


#define TRACE_MAX       10000

int trace_next;
pxa_trace_t *pxa_traces;

static __inline__ void PXA_REGS(u32 cs0, char *msg)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_regs;
                p->trace.regs.cs0 = cs0;
                p->trace.regs.msg = msg;
        }
}

static __inline__ void PXA_SETUP(struct usb_device_request *setup)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_setup;
                memcpy(&p->trace.setup, setup, sizeof(struct usb_device_request));
        }
}

static __inline__ void PXA_XMIT(u32 size)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_xmit;
                p->trace.xmit.size = size;
        }
}

static __inline__ void PXA_CCR(u32 ccr, char *msg)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_ccr;
                p->trace.ccr.ccr = ccr;
                p->trace.ccr.msg = msg;
        }
}

static __inline__ void PXA_IRO(u32 iro, char *msg)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_iro;
                p->trace.iro.iro = iro;
                p->trace.iro.msg = msg;
        }
}
#else
static __inline__ void PXA_REGS(u32 cs0, char *msg)
{
}

static __inline__ void PXA_SETUP(struct usb_device_request *setup)
{
}

static __inline__ void PXA_XMIT(u32 size)
{
}

static __inline__ void PXA_CCR(u32 ccr, char *msg)
{
}

static __inline__ void PXA_IRO(u32 iro, char *msg)
{
}
#endif

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

static __inline__ void pxa_enable_ep_interrupt(int ep)
{
        ep &= 0xf;
#ifdef USE_DMA
	if ((dma & PXA_DMA_OUT) && (ep == OUT_ENDPOINT)) {
		DMA_TRACEMSG ("PXA: skip enabling EP%d due to DMA\n", OUT_ENDPOINT);
		return;
	}
#endif
        if (ep < 8) {
                UICR0 &= ~(1<<ep);
        }
        else {
                UICR1 &= ~(1<<(ep-8));
        }
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
}

static __inline__ void pxa_ep_reset_irs(int ep)
{
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

#ifdef USE_DMA
/* 
 * FCS stuff, taken from usbdnet.c. CRC table now in ../crc32tab.c.
 */
extern const uint32_t crc32tab[];

#define CRC32_INITFCS     0xffffffff  /* Initial FCS value */
#define CRC32_GOODFCS     0xdebb20e3  /* Good final FCS value */

#define CRC32_FCS(fcs, c) (((fcs) >> 8) ^ crc32tab[((fcs) ^ (c)) & 0xff])

/* fcs_compute32 - memcpy and calculate fcs */
static uint32_t __inline__ fcs_compute32(unsigned char *sp, int len, uint32_t fcs)
{
	for (;len-- > 0; fcs = CRC32_FCS(fcs, *sp++));
	return fcs;
}

static usb_pxa_dma_t usb_pxa_dma_out = {-1, NULL, 0, 0, NULL, 0};

/* Do some h/w setup for DMA OUT */
static inline void pxa_rx_dma_hw_init (void)
{
	DCSR(usb_pxa_dma_out.dmach) |= DCSR_NODESC;
	DCMD(usb_pxa_dma_out.dmach) = DCMD_FLOWSRC | DCMD_INCTRGADDR | DCMD_WIDTH1 | 
		DCMD_BURST32 | DCMD_ENDIRQEN | DMA_OUT_BUFSIZE;
	DSADR(usb_pxa_dma_out.dmach) = _UDDRN[usb_pxa_dma_out.ep];
	DTADR(usb_pxa_dma_out.dmach) = usb_pxa_dma_out.dma_buf_phys;
	DRCMR26 = usb_pxa_dma_out.dmach | DRCMR_MAPVLD;
}

static void pxa_rx_dma_irq (int dmach, void *dev_id, struct pt_regs *regs)
{
	int dcsr = DCSR(usb_pxa_dma_out.dmach);
	
	DMA_TRACEMSG ("PXA: out dma IRQ [%d %d], DCSR 0x%x\n", UBCRN(usb_pxa_dma_out.ep), 
		      ep_endpoints[usb_pxa_dma_out.ep]->rcv_packetSize, dcsr);
	/* Stop DMA */
	DCSR(usb_pxa_dma_out.dmach) &= ~DCSR_RUN;
	/* TODO: busy-wait until DMA is really stopped ? */

	if (dcsr & DCSR_BUSERR)
		/* Is there a way to handle this error somehow ? */
                printk (KERN_WARNING "PXA: Bus error in USB RX DMA "
                        "- USB data transfer may be incorrect\n");
        
        else if (dcsr & DCSR_ENDINTR) {
		DMA_TRACEMSG ("PXA: normal IRQ\n");
		if (!usb_pxa_dma_out.endpoint->rcv_urb) {
			usb_pxa_dma_out.endpoint->rcv_urb = first_urb_detached (&usb_pxa_dma_out.endpoint->rdy);
		}
		if (usb_pxa_dma_out.endpoint->rcv_urb) {
			int recvsize;
			unsigned char *cp = usb_pxa_dma_out.endpoint->rcv_urb->buffer + 
				usb_pxa_dma_out.endpoint->rcv_urb->actual_length;
			
			/* Copy all new data to current urb buffer */
			memcpy (cp, usb_pxa_dma_out.dma_buf, DMA_OUT_BUFSIZE);
			
			if (usb_pxa_dma_out.dma_buf[DMA_OUT_BUFSIZE - 1] == 0xff) {
				/* May be end-of-urb data - must calculate FCS and
				   compare with FCS calculated on the host side */
				uint32_t oldfcs, newfcs;

				/* See usbdnet.c for explanation of this */
				oldfcs = (usb_pxa_dma_out.dma_buf[DMA_OUT_BUFSIZE - 2] << 24) |
					 (usb_pxa_dma_out.dma_buf[DMA_OUT_BUFSIZE - 3] << 16) |
					 (usb_pxa_dma_out.dma_buf[DMA_OUT_BUFSIZE - 4] << 8) |
					 (usb_pxa_dma_out.dma_buf[DMA_OUT_BUFSIZE - 5]);
				newfcs = fcs_compute32
					 (usb_pxa_dma_out.endpoint->rcv_urb->buffer,
					  usb_pxa_dma_out.endpoint->rcv_urb->actual_length + DMA_OUT_BUFSIZE - 5,
					  CRC32_INITFCS);
				newfcs = ~newfcs;
				/* If FCSs are equal, the last byte of new data
				   is 0xff mark and will be ignored */
				recvsize = (oldfcs == newfcs) ? (DMA_OUT_BUFSIZE - 1) : DMA_OUT_BUFSIZE;
			}
			else
				recvsize = DMA_OUT_BUFSIZE;
			DMA_TRACEMSG ("PXA: urb exists, real size %d\n", recvsize);
			usbd_rcv_complete_irq (usb_pxa_dma_out.endpoint, recvsize, 0);
		}
	}

	/* clear RPC and interrupt */
        UDCCSN(usb_pxa_dma_out.ep) |= UDCCS_BO_RPC;
        pxa_ep_reset_irs(usb_pxa_dma_out.ep);

	/* Clear end-of-interrupt */
        DCSR(usb_pxa_dma_out.dmach) |= DCSR_ENDINTR;

	/* Reinitialize and restart DMA */
	pxa_rx_dma_hw_init ();
	DCSR(usb_pxa_dma_out.dmach) |= DCSR_RUN;
}
#endif /* USE_DMA */

static __inline__ void pxa_out_n(int ep, struct usb_endpoint_instance *endpoint)
{
        if (UDCCSN(ep) & UDCCS_BO_RPC) {

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
                                usbd_rcv_complete_irq (endpoint, len, 0);
                        }
                        else {
                                pxa_out_flush(ep);
                        }
                }
        }
        // clear RPC and interrupt
        UDCCSN(ep) = UDCCS_BO_RPC;
        pxa_ep_reset_irs(ep);
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
/**************************/

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */
#ifdef USE_DMA
static usb_pxa_dma_t usb_pxa_dma_in = {-1, NULL, 0, 0, NULL, 0};

static void pxa_tx_dma_irq (int dmach, void *dev_id, struct pt_regs *regs)
{
	int dcsr = DCSR(usb_pxa_dma_in.dmach);

	/* Stop DMA */
	DCSR(usb_pxa_dma_in.dmach) &= ~DCSR_RUN;

	DMA_TRACEMSG ("PXA: in dma IRQ, DCSR 0x%x\n", dcsr);
        if (dcsr & DCSR_BUSERR)
		/* I don't know better way to handle DMA bus error... */
                printk (KERN_WARNING "PXA: Bus error in USB TX DMA "
                        "- USB data transfer may be incorrect\n");
        
        else if (dcsr & DCSR_ENDINTR) {
                /* All seems ok, checking for we have short packet transmitted */
                if ((usb_pxa_dma_in.size < usb_pxa_dma_in.endpoint->tx_packetSize) ||
                    ((usb_pxa_dma_in.endpoint->tx_urb->actual_length 
                      - usb_pxa_dma_in.endpoint->sent ) == usb_pxa_dma_in.size)) {
                        UDCCSN(usb_pxa_dma_in.ep) |= UDCCS_BI_TSP;
		}
                usb_pxa_dma_in.endpoint->last += usb_pxa_dma_in.size;
        }

        /* Clear end-of-interrupt */
        DCSR(usb_pxa_dma_in.dmach) |= DCSR_ENDINTR;

        /* Now we can re-enable irq */
        pxa_enable_ep_interrupt (usb_pxa_dma_in.ep);
}

static void __inline__ pxa_start_n_dma_hw (int ep, struct usb_endpoint_instance *endpoint,
					   char *mem, int size)
{
	/* Prepare for DMA */
	memcpy (usb_pxa_dma_in.dma_buf, mem, size);
	usb_pxa_dma_in.ep = ep;
	usb_pxa_dma_in.endpoint = endpoint;
	usb_pxa_dma_in.size = size;
	
	/* Setup DMA */
	DCSR(usb_pxa_dma_in.dmach) |= DCSR_NODESC;
	DCMD(usb_pxa_dma_in.dmach) = DCMD_FLOWTRG | DCMD_INCSRCADDR | DCMD_WIDTH1 | 
		DCMD_BURST32 | DCMD_ENDIRQEN | size;
	DSADR(usb_pxa_dma_in.dmach) = usb_pxa_dma_in.dma_buf_phys;
	DTADR(usb_pxa_dma_in.dmach) = _UDDRN[ep];

	/* Map DMA (FIXME: assume ep is 1) */
	DRCMR25 = usb_pxa_dma_in.dmach | DRCMR_MAPVLD;
	
	/* Start DMA */
	DCSR(usb_pxa_dma_in.dmach) |= DCSR_RUN;
}

static void pxa_start_n_dma (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
	/* We must disable irq for this endpoint */
	pxa_disable_ep_interrupt (ep);

        if (endpoint->tx_urb) {
                struct urb *urb = endpoint->tx_urb;
                int last = MIN(urb->actual_length - (endpoint->sent + endpoint->last), 
			       endpoint->tx_packetSize);
	        if (last) {
			pxa_start_n_dma_hw (ep, endpoint, urb->buffer + endpoint->sent + 
					    endpoint->last, last);
			return;
		}
	}
        
        /* Do nothing with DMA - re-enable irq */
        pxa_enable_ep_interrupt (ep);
}
#endif /* USE_DMA */

static void __inline__ pxa_start_n (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        if (endpoint->tx_urb) {
                int last;
                struct urb *urb = endpoint->tx_urb;

                if (( last = MIN (urb->actual_length - (endpoint->sent + endpoint->last), endpoint->tx_packetSize))) 
                {
                        int size = last;
                        unsigned char *cp = urb->buffer + endpoint->sent + endpoint->last;

                        while (size--) {
                                UDDRN(ep) = *cp++;
                        }

                        if (( last < endpoint->tx_packetSize ) ||
                                        ( (endpoint->tx_urb->actual_length - endpoint->sent ) == last )) 
                        {
                                UDCCSN(ep) = UDCCS_BI_TSP;
                        }
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

/* ********************************************************************************************* */
/* Control (endpoint zero)
 */

#ifdef CONFIG_USBD_EP0_SUPPORT
/* Changed by Stanley */
void pxa_ep0recv(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
	int size=0;
	int retval;
	struct urb *urb = endpoint->rcv_urb;
	unsigned char* cp = (unsigned char*) (urb->buffer + urb->actual_length);
	
	PXA_REGS(udccs0, "         <-- setup data recv");

#ifdef EP0_DEBUG
	printk("%d %x %x\n", urb->actual_length, urb->buffer, cp );
	printk("udccs0=0x%x\n", udccs0 );
#endif

	// check for premature status stage
	if ( !(udccs0&UDCCS0_OPR) && !(udccs0&UDCCS0_IPR) ) {
		if ( urb->device_request.wLength == urb->actual_length ) {
#ifdef EP0_DEBUG
			printk("pxa_ep0recv: UDC ep0 receive all data.\n");
#endif
                       retval = usbd_recv_setup(ep0_urb);
#ifndef CONFIG_USBD_EP0_SUPPORT
		       if ( retval ) {
			       UDCCS0 = UDCCS0_FST;
                               endpoint->state = WAIT_FOR_SETUP;
			       PXA_REGS(udccs0, "      --> bad setup FST");
                               return;
		       }
#endif
#ifdef CONFIG_USBD_EP0_SUPPORT
                       switch ( retval ) {
                       case -EINVAL:
                       case USB_RECV_SETUP_NOHANDLER:
			       UDCCS0 = UDCCS0_FST;
                               endpoint->state = WAIT_FOR_SETUP;
			       PXA_REGS(udccs0, "      --> bad setup FST");
                               return;
                       case USB_RECV_SETUP_PENDING:
                             /* We won't meet the situation here, but we should consider
                              *  the situation
                              */
                               return;
                       case USB_RECV_SETUP_DONE:
                               /* skip to the next statement */
                               break;
		       }
#endif
		} else {
#ifdef EP0_DEBUG
			printk(KERN_INFO"pxa_ep0recv: Get a premature status in stage.\n");
			printk(KERN_INFO"pxa_ep0recv: request.wLength=0x%x, actual_length=0x%x\n", 
			       urb->device_request.wLength, urb->actual_length );
#endif
#if 0
			if ( urb->buffer )
				kfree( urb->buffer );
#endif
		}

		endpoint->state = WAIT_FOR_SETUP;
	}

	// receive more data
	if (( udccs0 & UDCCS0_OPR ) && !(UDCCS0 & UDCCS0_SA) ) {
		while ( UDCCS0&UDCCS0_RNE ) {
			*cp++ = UDDRN(0);
			urb->actual_length++;
		}

		UDCCS0 |= UDCCS0_OPR;
		
		// to allow to enter a premature STATUS IN stage
		UDCCS0 |= UDCCS0_IPR;
	}

	return;
}
#endif 

void pxa_ep0xmit(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
	int short_packet;
	int size;
	struct urb *urb = endpoint->tx_urb;

        PXA_REGS(udccs0, "          --> xmit");

        //printk(KERN_INFO"tx[%d:%d] CS0[%02x]\n", udc_interrupts, jifs(), UDCCS0);

        // check for premature status stage - host abandoned previous IN
        if ((udccs0 & UDCCS0_OPR) && !(UDCCS0 & UDCCS0_SA) ) {
                // clear tx fifo and opr
                UDCCS0 = UDCCS0_FTF | UDCCS0_OPR;
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
		endpoint->rcv_urb = NULL;
                PXA_REGS(UDCCS0, "          <-- xmit premature status");
                return;
        }

        // check for stall
        if (udccs0 & UDCCS0_SST) {
                // clear stall and tx fifo
                UDCCS0 = UDCCS0_SST | UDCCS0_FTF;
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
		endpoint->rcv_urb = NULL;
                PXA_REGS(UDCCS0, "          <-- xmit stall");
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

        PXA_XMIT(size);

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
                PXA_REGS(UDCCS0, "          <-- xmit wait for status");
	}

        else if ((endpoint->last == endpoint->tx_packetSize) && 
                        ((endpoint->last + endpoint->sent) == urb->actual_length)  &&
                        ((urb->actual_length) < le16_to_cpu(urb->device_request.wLength)) 
                ) 
        {
		// Tell the UDC we are at the end of the packet.
                endpoint->state = DATA_STATE_NEED_ZLP;
                PXA_REGS(UDCCS0, "          <-- xmit need zlp");
	}
        else {
                PXA_REGS(UDCCS0, "          <-- xmit not finished");
        }
}

void __inline__ pxa_ep0setup(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        if ((udccs0 & (UDCCS0_SA | UDCCS0_OPR | UDCCS0_RNE)) == (UDCCS0_SA | UDCCS0_OPR | UDCCS0_RNE)) {

                int len = 0;
		int retval = 0;
                int max = 8;
                unsigned char *cp = (unsigned char *)&ep0_urb->device_request;

                PXA_REGS(udccs0, "      --> setup");

                //memset(cp, 0, max);

                while (max-- /*&& (UDCCS0 & UDCCS0_RNE)*/) {
                        len++;
                        *cp++ = UDDR0;
                }

                PXA_SETUP(&ep0_urb->device_request);

#ifdef CONFIG_USBD_EP0_SUPPORT
/* This section is added to give some supports for setup data IN mode. It is necessary 
 * to call usbd_recv_setup * routine at end of receiving all data
 *                           - Stanley
 */
#ifdef EP0_DEBUG
		printk( KERN_INFO"URB request:\n" );
		printk( KERN_INFO"ep0.bmRequestType: 0x%x\n", ep0_urb->device_request.bmRequestType );
		printk( KERN_INFO"ep0.bRequest: 0x%x\n", ep0_urb->device_request.bRequest );
		printk( KERN_INFO"ep0.wlength: 0x%x\n", ep0_urb->device_request.wLength );
#endif

		// Controll Write - Maybe we will receive more data from the host
		if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE
			&& le16_to_cpu(ep0_urb->device_request.wLength)!=0 ) 
		{
#ifdef EP0_DEBUG
			printk("HOST2DEVICE\n");
#endif
			endpoint->state = DATA_STATE_RECV;

			ep0_urb->actual_length = 0;
			// the bugs
			endpoint->rcv_urb = ep0_urb;
                        UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
                        PXA_REGS(UDCCS0,"      <-- setup recv");

			return;
		}
#endif

                // process setup packet
		retval = usbd_recv_setup(ep0_urb);
#ifndef CONFIG_USBD_EP0_SUPPORT
		if ( retval ) {
                        UDCCS0 = UDCCS0_FST;
                        endpoint->state = WAIT_FOR_SETUP;
                        PXA_REGS(udccs0, "      --> bad setup FST");
                        return;
		}
#endif
#ifdef CONFIG_USBD_EP0_SUPPORT
		switch ( retval ) {
		case -EINVAL:
		case USB_RECV_SETUP_NOHANDLER:
                        // setup processing failed 
                        UDCCS0 = UDCCS0_FST;
                        endpoint->state = WAIT_FOR_SETUP;
                        PXA_REGS(udccs0, "      --> bad setup FST");
                        return;
		case USB_RECV_SETUP_PENDING:
#if 0
			UDCCS0 = UDCCS0_FST;
#endif
			endpoint->state = DATA_STATE_PENDING_XMIT;
			UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
#if 0
			UDCCS0 = UDCCS0_IPR;
#endif
			PXA_REGS( UDCCS0, "     <-- setup recv");
			return;
		case USB_RECV_SETUP_DONE:
			break;
		}
#endif

                // check direction
                if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) 
                {
                        // Control Write - we are receiving more data from the host
#ifdef EP0_DEBUG
			printk("HOST2DEVICE - zero length\n");
#endif
#if 0
                        // should we setup to receive data
			// Skip this condition; - Stanley
                        if (le16_to_cpu (ep0_urb->device_request.wLength)) {
                                //printk(KERN_INFO"sl11_ep0: read data %d\n",
                                //      le16_to_cpu(ep0_urb->device_request.wLength));
                                endpoint->rcv_urb = ep0_urb;
                                endpoint->rcv_urb->actual_length = 0;
				endpoint->state = DATA_STATE_RECV;
                                // XXX this has not been tested
                                // pxa_out_0 (0, endpoint);
                                PXA_REGS(UDCCS0,"      <-- setup no response");
                                return;
                        }
#endif

                        // allow for premature IN (c.f. 12.5.3 #8)
                        UDCCS0 = UDCCS0_IPR;
                        PXA_REGS(UDCCS0,"      <-- setup nodata");
                }
                else {
                        // Control Read - we are sending data to the host
#ifdef EP0_DEBUG
			printk("DEVICE2HOST\n");
#endif
                        // verify that we have non-zero request length
                        if (!le16_to_cpu (ep0_urb->device_request.wLength)) {
                                udc_stall_ep (0);
                                PXA_REGS(UDCCS0,"      <-- setup stalling zero wLength");
                                return;
                        }
                        // verify that we have non-zero length response
                        if (!ep0_urb->actual_length) {
                                udc_stall_ep (0);
                                PXA_REGS(UDCCS0,"      <-- setup stalling zero response");
                                return;
                        }

                        // start sending
                        endpoint->tx_urb = ep0_urb;
                        endpoint->sent = 0;
                        endpoint->last = 0;
                        endpoint->state = DATA_STATE_XMIT;
			pxa_ep0xmit(endpoint, UDCCS0);

                        // Clear SA and OPR bits
                        UDCCS0 = UDCCS0_SA | UDCCS0_OPR;
                        PXA_REGS(UDCCS0,"      <-- setup data");
                }
        }
}

static __inline__ void pxa_ep0(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        int j = 0;

        PXA_REGS(udccs0,"  --> ep0");


        /* If the state of the endpoint is not in DATA_STATE_PENDING_XMIT,
        * it will process STALL ack.
        * If the state is in DATA_STATE_PENDING_XMIT,
        * the spec didn't say anything.
        *
        * The situation rarely appear.
        */
#ifdef CONFIG_USBD_EP0_SUPPORT
	if ( endpoint->state!=DATA_STATE_PENDING_XMIT && (udccs0&UDCCS0_SST) ) {
#endif
#ifndef CONFIG_USBD_EP0_SUPPORT
	if ( udccs0&UDCCS0_SST ) {
#endif
                pxa_ep_reset_irs(0);
                UDCCS0 = UDCCS0_SST;
                PXA_REGS(udccs0,"  --> ep0 clear SST");
                if (endpoint) {
                        endpoint->state = WAIT_FOR_SETUP;
                        endpoint->tx_urb = NULL;
			endpoint->rcv_urb = NULL;
                }
                return;
        }


        if (!endpoint) {
                printk(KERN_INFO"ep0[%d:%d] endpoint Zero is NULL CS0[%02x]\n", udc_interrupts, jifs(), UDCCS0);
                pxa_ep_reset_irs(0);
                UDCCS0 = UDCCS0_IPR | UDCCS0_OPR | UDCCS0_SA;
                PXA_REGS(UDCCS0,"  ep0 NULL");
                return;
        }

        if (endpoint->tx_urb) {
                usbd_tx_complete_irq (endpoint, 0);
                if (!endpoint->tx_urb) {
                        if (endpoint->state != DATA_STATE_NEED_ZLP) {
                                endpoint->state = WAIT_FOR_OUT_STATUS;
                        }
                }
        }

        if ( (endpoint->state!=DATA_STATE_PENDING_XMIT) && (endpoint->state != WAIT_FOR_SETUP) && (udccs0 & UDCCS0_SA)) {
                PXA_REGS(udccs0,"  --> ep0 early SA");
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
		endpoint->rcv_urb = NULL;
        }

        switch (endpoint->state) {
        case DATA_STATE_NEED_ZLP:
                UDCCS0 = UDCCS0_IPR;
                endpoint->state = WAIT_FOR_OUT_STATUS;
                break;

        case WAIT_FOR_OUT_STATUS:
                if ((udccs0 & (UDCCS0_OPR | UDCCS0_SA)) == UDCCS0_OPR) {
                        UDCCS0 |= UDCCS0_OPR;
                }
                PXA_REGS(UDCCS0,"  --> ep0 WAIT for STATUS");
                endpoint->state = WAIT_FOR_SETUP;

        case WAIT_FOR_SETUP:
                do {
                        pxa_ep0setup(endpoint, UDCCS0);

			if ( endpoint->state == DATA_STATE_PENDING_XMIT ) {
                               if ( udccs0 & UDCCS0_SST ) {
                                       pxa_ep_reset_irs(0);
                                       UDCCS0 = UDCCS0_SST | UDCCS0_OPR | UDCCS0_SA;
                                       return;
                               }
			       break;
			}
                        if (udccs0 & UDCCS0_SST) {
                                pxa_ep_reset_irs(0);
                                UDCCS0 = UDCCS0_SST | UDCCS0_OPR | UDCCS0_SA;
                                PXA_REGS(udccs0,"  --> ep0 clear SST");
                                if (endpoint) {
                                        endpoint->state = WAIT_FOR_SETUP;
                                        endpoint->tx_urb = NULL;
                                        endpoint->rcv_urb = NULL;
                                }
                                return;
                        }

                        if (j++ > 2) {
                                u32 udccs0 = UDCCS0;
                                PXA_REGS(udccs0,"  ep0 wait");
                                if ((udccs0 & (UDCCS0_OPR | UDCCS0_SA | UDCCS0_RNE)) == (UDCCS0_OPR | UDCCS0_SA)) {
                                        UDCCS0 = UDCCS0_OPR | UDCCS0_SA;
                                        PXA_REGS(UDCCS0,"  ep0 force");
                                }
                                else {
                                        UDCCS0 = UDCCS0_OPR | UDCCS0_SA;
                                        PXA_REGS(UDCCS0,"  ep0 force and return");
                                        break;
                                }
                        }

                } while (UDCCS0 & (UDCCS0_OPR | UDCCS0_RNE));
                break;

        case DATA_STATE_XMIT:
                pxa_ep0xmit(endpoint, UDCCS0);
                break;
#ifdef CONFIG_USBD_EP0_SUPPORT
	case DATA_STATE_PENDING_XMIT:
		pxa_ep0xmit( endpoint, UDCCS0);
		break;
	case DATA_STATE_RECV:
		pxa_ep0recv(endpoint, UDCCS0);
		break;
#endif
        }

        pxa_ep_reset_irs(0);
        PXA_REGS(UDCCS0,"  <-- ep0");
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
        int usiro;
        int udccr;
        int ep;

        int_oscr = OSCR;
	udc_interrupts++;

        // check for common, high priority interrupts first, i.e. per endpoint service requests
        // XXX if ISO supported it might be necessary to give the ISO endpoints priority
        
        while ((usiro = USIR0)) {
                u32 udccs0 = UDCCS0;
                PXA_IRO(usiro, "------------------------> Interrupt");
                for (ep = 0; usiro; usiro >>= 1, ep++) {
                        if (usiro & 1) {
                                switch (ep_maps[ep].eptype) {
                                case ep_control:
                                        pxa_ep0(ep_endpoints[0], udccs0);
                                        //PXA_IRO(USIRO, "<-- Interrupt");
                                        break;

                                case ep_bulk_in:
                                case ep_interrupt:
                                        pxa_in_n(ep, ep_endpoints[ep]);
                                        break;

                                case ep_bulk_out:
#ifdef USE_DMA
					if (dma & PXA_DMA_OUT)
						printk (KERN_WARNING "PXA: bulk out IRQ with DMA "
							"out on ep %d - may be a bug\n", ep);
					else
#endif
						pxa_out_n(ep, ep_endpoints[ep]);
					pxa_ep_reset_irs(ep);
					break;
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
                        PXA_CCR(udccr, "------------------------> Reset");
                        //printk(KERN_INFO"int_hndlr[%d:%d] Reset\n", udc_interrupts, jifs());
                                        
                        udc_suspended = 0;
                        usbd_device_event (udc_device, DEVICE_RESET, 0);
                        usbd_device_event (udc_device, DEVICE_ADDRESS_ASSIGNED, 0);	
                        UDCCR |= UDCCR_RSTIR;
                }

                // UDC Resume
                if (udccr & UDCCR_RESIR) {
                        PXA_CCR(udccr, "------------------------> Resume");
                        if (udc_suspended) {
                                udc_suspended = 0;
                                usbd_device_event (udc_device, DEVICE_BUS_ACTIVITY, 0);
                        }
                        UDCCR |= UDCCR_RESIR;
                }

                // UDC Suspend
                if (udccr & UDCCR_SUSIR) {
                        PXA_CCR(udccr, "------------------------> Suspend");
                        if (!udc_suspended) {
                                udc_suspended = 1;
                                usbd_device_event (udc_device, DEVICE_BUS_INACTIVE, 0);
                        }
                        UDCCR |= UDCCR_SUSIR;
                }
        }
        PXA_CCR(UDCCR, "<-- Interrupt");
}


/* ********************************************************************************************* */


/* ********************************************************************************************* */
/*
 * Start of public functions.
 */

/**
 * udc_start_in_irq - start transmit
 * @eendpoint: endpoint instance
 *
 * Called by bus interface driver to see if we need to start a data transmission.
 */
void udc_start_in_irq (struct usb_endpoint_instance *endpoint)
{
#ifdef CONFIG_USBD_EP0_SUPPORT
        if ( endpoint->endpoint_address==0 ) {
               struct urb* urb = endpoint->tx_urb;
               if ( !urb ) {
                       printk(KERN_INFO"udc_start_in_irq()  urb is NULL!\n");
	       }
               pxa_ep0xmit( endpoint, urb );
               return;
	}
#endif
        if (UDCCSN(endpoint->endpoint_address & 0xf) & UDCCS_BI_TFS) {
#ifdef USE_DMA
                if (dma & PXA_DMA_IN)
                        pxa_start_n_dma (endpoint->endpoint_address & 0xf, endpoint);
                else
#endif
			pxa_start_n (endpoint->endpoint_address & 0xf, endpoint);
        }
}

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
        udc_disable_interrupts (NULL);
#ifdef USE_DMA
	if (dma && (usbd_function == USBD_SERIAL)) {
		printk (KERN_WARNING "PXA: DMA is not supported for serial function, disabled\n");
		dma = 0;
	}
	if ((dma & PXA_DMA_IN) &&
	    !(usb_pxa_dma_in.dma_buf = (unsigned char *)consistent_alloc 
              (GFP_KERNEL, DMA_IN_BUFSIZE, &usb_pxa_dma_in.dma_buf_phys))) {
		/* DMA IN is requested, but buffer is not allocated. Bad, disable DMA IN */
                printk (KERN_WARNING "PXA: can't allocate consistent memory for USB DMA IN\n");
                printk (KERN_WARNING "PXA: USB DMA IN is disabled\n");
		dma &= ~PXA_DMA_IN;
        }
        if ((dma & PXA_DMA_OUT) &&
	    !(usb_pxa_dma_out.dma_buf = (unsigned char *)consistent_alloc 
              (GFP_KERNEL, DMA_OUT_BUFSIZE, &usb_pxa_dma_out.dma_buf_phys))) {
                /* The same as above, but for DMA OUT */
                printk (KERN_WARNING "PXA: can't allocate consistent memory for USB DMA OUT\n");
                printk (KERN_WARNING "PXA: USB DMA OUT is disabled\n");
		dma &= ~PXA_DMA_OUT;
        }
#endif
	return 0;
}

/**
 * udc_start_in - start transmit
 * @eendpoint: endpoint instance
 *
 * Called by bus interface driver to see if we need to start a data transmission.
 */
void udc_start_in (struct usb_endpoint_instance *endpoint)
{
	if (endpoint) {
		unsigned long flags;
		local_irq_save (flags);
                udc_start_in_irq(endpoint);
		local_irq_restore (flags);
	}
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

        return ( ((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) || (packetsize > 64)) 
                ?  0 : (logical_endpoint & 0xf);
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
	if (ep < UDC_MAX_ENDPOINTS) {
		ep_endpoints[ep] = endpoint;

		// ep0
		if (ep == 0) {
		}
		// IN
		else if (endpoint->endpoint_address & 0x80) {
#ifdef USE_DMA
                        if ((dma & PXA_DMA_IN) && (ep == IN_ENDPOINT) && 
			    (usb_pxa_dma_in.dmach == -1) && usb_pxa_dma_in.dma_buf) {
                                /* DMA IN requested and buffer allocated, try to get DMA irq */
                                if ((usb_pxa_dma_in.dmach = pxa_request_dma
                                     ("USB DMA IN", DMA_PRIO_HIGH, pxa_tx_dma_irq,
				      (void *)&usb_pxa_dma_in)) < 0) {
					printk (KERN_WARNING "PXA: can't allocate DMA IRQ for USB DMA IN\n");
					printk (KERN_WARNING "PXA: USB DMA IN is disabled\n");
					consistent_free (usb_pxa_dma_in.dma_buf,
							 DMA_IN_BUFSIZE, usb_pxa_dma_in.dma_buf_phys);
					usb_pxa_dma_in.dma_buf = NULL;
					dma &= ~PXA_DMA_IN;
				}
				else
					DMA_TRACEMSG (KERN_DEBUG "PXA: USB IN DMA channel %d\n", 
						      usb_pxa_dma_in.dmach);
                        }
#endif
		}
		// OUT
		else if (endpoint->endpoint_address) {
			usbd_fill_rcv (device, endpoint, 5);
			endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
#ifdef USE_DMA
                        if ((dma & PXA_DMA_OUT) && (ep == OUT_ENDPOINT) &&
			    (usb_pxa_dma_out.dmach == -1) && usb_pxa_dma_out.dma_buf) {
                                /* The same as above, but for DMA OUT */
                                if ((usb_pxa_dma_out.dmach = pxa_request_dma
                                     ("USB DMA OUT", DMA_PRIO_HIGH, pxa_rx_dma_irq,
				      (void *)&usb_pxa_dma_out)) < 0) {
					printk (KERN_WARNING "PXA: can't allocate DMA IRQ for USB DMA OUT\n");
					printk (KERN_WARNING "PXA: USB DMA OUT is disabled\n");
					consistent_free (usb_pxa_dma_out.dma_buf,
							 DMA_OUT_BUFSIZE, usb_pxa_dma_out.dma_buf_phys);
					usb_pxa_dma_out.dma_buf = NULL;
					dma &= ~PXA_DMA_OUT;
				}
				else {
					DMA_TRACEMSG (KERN_DEBUG "PXA: USB OUT DMA channel %d\n",
						      usb_pxa_dma_out.dmach);
					usb_pxa_dma_out.ep = ep;
					usb_pxa_dma_out.endpoint = endpoint;
					usb_pxa_dma_out.size = DMA_OUT_BUFSIZE;
					
					pxa_rx_dma_hw_init ();
					/* Enable DME for BULK OUT endpoint */
					UDCCS2 |= UDCCS_BO_DME;
					DMA_TRACEMSG ("PXA: enable DME\n");

					/* Start DMA */
					DCSR(usb_pxa_dma_out.dmach) |= DCSR_RUN;
					DMA_TRACEMSG ("PXA: OUT DMA started [ep %d]\n", ep);
				}
			}
#endif
		}
		pxa_enable_ep_interrupt(ep_endpoints[ep]->endpoint_address);
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
#ifdef USE_DMA
		// IN
		else if (endpoint && endpoint->endpoint_address & 0x80) {
                        if ((dma & PXA_DMA_IN) && (ep == IN_ENDPOINT) && 
			    (usb_pxa_dma_in.dmach != -1)) {
				pxa_free_dma (usb_pxa_dma_in.dmach);
				usb_pxa_dma_in.dmach = -1;
			}
		}
		// OUT
		else if (endpoint && endpoint->endpoint_address) {
                        if ((dma & PXA_DMA_OUT) && (ep == OUT_ENDPOINT) &&
			    (usb_pxa_dma_out.dmach != -1)) {
				pxa_free_dma (usb_pxa_dma_out.dmach);
				usb_pxa_dma_out.dmach = -1;
			}
		}
#endif
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
	GPSR(1) = GPIO_GPIO(USBD_CONNECT_GPIO);
#else
	/* Set GPIO pin low to connect */
	GPCR(1) = GPIO_GPIO(USBD_CONNECT_GPIO);
#endif
#else
#warning NO USB Device connect
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
	GPCR(1) = GPIO_GPIO(USBD_CONNECT_GPIO);
#else
	/* Set GPIO pin high to disconnect */
	GPSR(1) = GPIO_GPIO(USBD_CONNECT_GPIO);
#endif
#else
#warning NO USB Device connect
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

	// ep0 urb
	if (!ep0_urb) {
		if (!(ep0_urb = usbd_alloc_urb (device, device->function_instance_array, 0, 512))) {
			printk (KERN_ERR "udc_enable: usbd_alloc_urb failed\n");
		}
	} 
        else {
		printk (KERN_ERR "udc_enable: ep0_urb already allocated\n");
	}


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
	// disable UDC
        // c.f. 12.6.1.1 UDC Enable
        UDCCR &= ~( UDCCR_UDE | UDCCR_REM );

	// c.f. 3.6.2 Clock Enable Register
	CKEN &= ~CKEN11_USB;

        // disable cable interrupt
        
	// reset device pointer
	udc_device = NULL;

	// ep0 urb

	if (ep0_urb) {
		usbd_dealloc_urb (ep0_urb);
		ep0_urb = 0;
	} else {
		printk (KERN_ERR "udc_disable: ep0_urb already NULL\n");
	}
#ifdef USE_DMA
        if (dma & PXA_DMA_IN) {
                consistent_free (usb_pxa_dma_in.dma_buf,
                                 DMA_IN_BUFSIZE, usb_pxa_dma_in.dma_buf_phys);
                usb_pxa_dma_in.dma_buf = NULL;
        }
        if (dma & PXA_DMA_OUT) {
                consistent_free (usb_pxa_dma_out.dma_buf,
                                 DMA_OUT_BUFSIZE, usb_pxa_dma_out.dma_buf_phys);
                usb_pxa_dma_out.dma_buf = NULL;
        }
#endif
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
#ifdef PXA_TRACE
#ifdef CONFIG_USBD_PROCFS
/* Proc Filesystem *************************************************************************** */
        
/* *    
 * pxa_proc_read - implement proc file system read.
 * @file        
 * @buf         
 * @count
 * @pos 
 *      
 * Standard proc file system read function.
 */         
static ssize_t pxa_proc_read (struct file *file, char *buf, size_t count, loff_t * pos)
{                                  
        unsigned long page;
        int len = 0;
        int index;

        MOD_INC_USE_COUNT;
        // get a page, max 4095 bytes of data...
        if (!(page = get_free_page (GFP_KERNEL))) {
                MOD_DEC_USE_COUNT;
                return -ENOMEM;
        }

        len = 0;
        index = (*pos)++;

        if (index == 0) {
                len += sprintf ((char *) page + len, "   Index     Ints     Jifs    Ticks\n");
        }       
                         

        if (index < trace_next) {

                u64 jifs = 1111;
                u32 ticks = 2222;
                pxa_trace_t *p = pxa_traces + index;
                unsigned char *cp;

                if (index > 0) {
                        u32 ocsr = pxa_traces[index-1].ocsr;
                        ticks = (p->ocsr > ocsr) ? (p->ocsr - ocsr) : (ocsr - p->ocsr) ;
                        jifs = p->jiffies - pxa_traces[index-1].jiffies;
                }
                
                len += sprintf ((char *) page + len, "%8d %8d %8lu ", index, p->interrupts, jifs);

                if (ticks > 1024*1024) {
                        len += sprintf ((char *) page + len, "%8dM ", ticks>>20);
                }
                else {
                        len += sprintf ((char *) page + len, "%8d  ", ticks);
                }
                switch (p->trace_type) {
                case pxa_regs:
                        len += sprintf ((char *) page + len, "CS0[%02x] %s\n", p->trace.regs.cs0, p->trace.regs.msg);
                        break;
                case pxa_setup:
                        cp = (unsigned char *)&p->trace.setup;
                        len += sprintf ((char *) page + len, 
                                        " --               request [%02x %02x %02x %02x %02x %02x %02x %02x]\n", 
                                        cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
                        break;
                case pxa_xmit:
                        len += sprintf ((char *) page + len, 
                                        " --                   sending [%02x]\n", p->trace.xmit.size);
                        break;
                case pxa_ccr:
                        len += sprintf ((char *) page + len, 
                                        "CCR[%02x] %s\n", p->trace.ccr.ccr, p->trace.ccr.msg);
                        break;
                case pxa_iro:
                        len += sprintf ((char *) page + len, 
                                        "IRO[%02x] %s\n", p->trace.iro.iro, p->trace.iro.msg);
                        break;
                }
        }

        if (len > count) {
                len = -EINVAL;
        } else if (len > 0 && copy_to_user (buf, (char *) page, len)) {
                len = -EFAULT;
        }
        free_page (page);
        MOD_DEC_USE_COUNT;
        return len;
}
/* *
 * pxa_proc_write - implement proc file system write.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Proc file system write function, used to signal monitor actions complete.
 * (Hotplug script (or whatever) writes to the file to signal the completion
 * of the script.)  An ugly hack.
 */
static ssize_t pxa_proc_write (struct file *file, const char *buf, size_t count, loff_t * pos)
{
        return count;
}

static struct file_operations pxa_proc_operations_functions = {
        read:pxa_proc_read,
        write:pxa_proc_write,
};

#endif
#endif


/* ********************************************************************************************* */
/**
 * udc_name - return name of USB Device Controller
 */
char *udc_name (void)
{
	return UDC_NAME;
}

/* Used in startup banner in usbd-bi.c. */
const char *udc_info (void)
{
	return __usbd_module_info;
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

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io ()
{
#ifdef PXA_TRACE
#ifdef CONFIG_USBD_PROCFS
        if (!(pxa_traces = vmalloc(sizeof(pxa_trace_t) * TRACE_MAX))) {
                printk(KERN_ERR"PXA_TRACE malloc failed %p %d\n", pxa_traces, sizeof(pxa_trace_t) * TRACE_MAX);
        }
        else {
                printk(KERN_ERR"PXA_TRACE malloc ok %p\n", pxa_traces);
        }
        PXA_REGS(0,"init");
        PXA_REGS(0,"test");
        {
                struct proc_dir_entry *p;

                // create proc filesystem entries
                if ((p = create_proc_entry ("pxa", 0, 0)) == NULL) {
                        printk(KERN_INFO"PXA PROC FS failed\n");
                }
                else {
                        printk(KERN_INFO"PXA PROC FS Created\n");
                        p->proc_fops = &pxa_proc_operations_functions;
                }
        }
#endif
#endif
	return 0;
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io ()
{
#ifdef PXA_TRACE
#ifdef CONFIG_USBD_PROCFS
        unsigned long flags;
        save_flags_cli (flags);
        remove_proc_entry ("pxa", NULL);
        if (pxa_traces) {
                pxa_trace_t *p = pxa_traces;
                pxa_traces = 0;
                vfree(p);
        }
        restore_flags (flags);
#endif
#endif

}

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

/*
 * dma_regs - dump DMA state
 * G.N.U., but may be useful for DMA debugging.
 */
#if 0
static void dma_regs (void)
{
	if (dma & PXA_DMA_IN)
		printk ("  DMA IN: DCSR:%x DCMD:%x DSADR:%x DTADR:%x\n",
			DCSR(usb_pxa_dma_in.dmach), DCMD(usb_pxa_dma_in.dmach),
			DSADR(usb_pxa_dma_in.dmach), DTADR(usb_pxa_dma_in.dmach));
	if (dma & PXA_DMA_OUT)
		printk ("  DMA OUT: DCSR:%x DCMD:%x DSADR:%x DTADR:%x\n",
			DCSR(usb_pxa_dma_out.dmach), DCMD(usb_pxa_dma_out.dmach),
			DSADR(usb_pxa_dma_out.dmach), DTADR(usb_pxa_dma_out.dmach));
}
#endif
