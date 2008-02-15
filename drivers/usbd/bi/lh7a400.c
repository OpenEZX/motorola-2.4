/*
 * linux/drivers/usbd/bi/lh7a400.c -- USB Device Controller driver. 
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2002 Embedix
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Tom Rushworth (tbr@belcarra.com)
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
 * References are to the revised LHA7400 Preliminary User's Guide, chapters 15 and 18.
 *
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("sl@belcarra.com, tbr@belcarra.com");
MODULE_DESCRIPTION ("USB Device SharpSec Bus Interface");
MODULE_LICENSE("GPL");

USBD_MODULE_INFO ("LH7A400");

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>

#include <linux/proc_fs.h>
#include <linux/vmalloc.h>


#include <linux/netdevice.h>

#include <asm/irq.h>
#include <asm/system.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

#include "lh7a400.h"
#include "lh7a400-hardware.h"

#include "trace.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


static struct usb_device_instance *udc_device;	// required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb ep0_urb;

static unsigned char usb_address;

extern unsigned int udc_interrupts;
unsigned int tx_interrupts;
unsigned int rx_interrupts;
unsigned int sus_interrupts;
unsigned int req_interrupts;
unsigned int sof_interrupts;

/* ********************************************************************************************* */
/* IO
 */

char * sharp_usb_regs[] = {
      // 0x0             0x4             0x8             0xc
        "FAddr",        "PM",           "in int",       "reserved",             // 0x00
        "out int",      "reserved",     "int",          "in int en",            // 0x10
        "reserved",     "out int en",   "reserved",     "int en",               // 0x20
        "frm num 1",    "frm num 2",    "index",        "reserved",             // 0x30

        "in maxp",      "in csr1",      "in crs2",      "out maxp",             // 0x40
        "out csr1",     "out csr2",     "fifo wc1",     "fifo wc2",             // 0x50
        "reserved",     "reserved",     "reserved",     "reserved",             // 0x60
        "reserved",     "reserved",     "reserved",     "reserved",             // 0x70

        "ep0 fifo",     "ep1 fifo",     "ep2 fifo",     "ep3 fifo",             // 0x80
};


typedef enum ep_type {
        ep_control, ep_bulk_in, ep_bulk_out, ep_interrupt
} ep_type_t;

typedef struct ep_regs {
        u8 size;
        ep_type_t ep_type;
        u32 fifo;
        u32 csr1;
        u32 csr2;
} ep_regs_t;

/*
 * map logical to physical 
 */
struct ep_regs ep_regs[6] = {
        { size: EP0_PACKETSIZE, ep_type: ep_control, fifo: USB_EP0_FIFO, csr1: USB_EP0_CSR, csr2: USB_EP0_CSR, },
        { size: 64, ep_type: ep_bulk_in, fifo: USB_EP1_FIFO, csr1: USB_IN_CSR1, csr2: USB_IN_CSR2, } ,
        { size: 64, ep_type: ep_bulk_out, fifo: USB_EP2_FIFO, csr1: USB_OUT_CSR1, csr2: USB_OUT_CSR2, } ,
        { size: 64, ep_type: ep_interrupt, fifo: USB_EP3_FIFO, csr1: USB_IN_CSR1, csr2: USB_IN_CSR2, },
};


static __inline__ char *sharp_ureg_name(unsigned int port)
{
        if (port == USB_RESET) {
                return "usb reset";
        }
        port = (port & 0xff) >> 2;
        return (port < (sizeof(sharp_usb_regs))/4) ? sharp_usb_regs[port] : "unknown";
}

static __inline__ u32 sharp_url(unsigned int port)
{
        u32 val;
        if (!port) {
                TRACE_MSG32("RL INVALID PORT %d", port);
                return 0;
        }
        val =  *(volatile u32 *) IO_ADDRESS(port);
        //printk(KERN_INFO"sharp_url: %x %x\n", val, port);
        TRACE_R(sharp_ureg_name(port), val);
        return val;
}

static __inline__ void sharp_uwl(u32 val, unsigned int port)
{
        //printk(KERN_INFO"sharp_uwl: %x %x\n", val, port);
        if (!port) {
                TRACE_MSG32("WL INVALID PORT %d", port);
                return;
        }
        *(volatile u32 *)IO_ADDRESS(port) = val;
        TRACE_W(sharp_ureg_name(port), val);
}

static __inline__ u32 sharp_urli(unsigned int index, unsigned int port)
{
        u32 val;
        if (!port) {
                TRACE_MSG32("RL INVALID PORT %d", port);
                return 0;
        }
        *(volatile u32 *) IO_ADDRESS(USB_INDEX) = index;
        val =  *(volatile u32 *) IO_ADDRESS(port);
        //printk(KERN_INFO"sharp_urli: %x %x %x\n", val, index, port);
        TRACE_R(sharp_ureg_name(port), val);
        return val;
}

static __inline__ void sharp_uwli(u32 val, unsigned int index, unsigned int port)
{
        //printk(KERN_INFO"sharp_uwli: %x %x %x\n", val, index, port);
        TRACE_MSG32("sharp_uwli: %x", index);
        if (!port) {
                TRACE_MSG32("WL INVALID PORT %d", port);
                return;
        }
        *(volatile u32 *) IO_ADDRESS(USB_INDEX) = index;
        *(volatile u32 *) IO_ADDRESS(port) = val;
        TRACE_W(sharp_ureg_name(port), val);
}

static __inline__ int sharp_fifo_read(int ep, unsigned char * cp, int max)
{
        int bytes;
        int n;

        // XXX micro-optimization - non-iso endpoints always <= 64 bytes, WC2 will always be zero
        int count = sharp_urli(ep, USB_OUT_FIFO_WC1)/* + (sharp_urli(ep, USB_OUT_FIFO_WC2) << 8)*/;

        if (count > max) {
                count = max;
        }
        bytes = count;

        // XXX another micro-optimization - use duff's device to unroll read loop
        // XXX yes this really does work...
        
        n = (count + 3) / 4;

        switch (count % 4) {
        case 0: do {     
                        *cp++ =  *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) & 0xff;
                case 3: 
                        *cp++ =  *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) & 0xff;
                case 2: 
                        *cp++ =  *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) & 0xff;
                case 1:        
                        *cp++ =  *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) & 0xff;
                } while(--n > 0);
        }

        TRACE_MSG16("fifo_read: %d %d", ep, bytes);

        return bytes;
}

static __inline__ void sharp_fifo_write(int ep, unsigned char * cp, int count)
{
        int n;

        TRACE_MSG16("fifo_write: %d %d", ep, count);

        n = (count + 3) / 4;

        switch (count % 4) {
        case 0: do {     
                        *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) = *cp++;
                case 3: 
                        *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) = *cp++;
                case 2: 
                        *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) = *cp++;
                case 1: 
                        *(volatile u32 *) IO_ADDRESS(ep_regs[ep].fifo) = *cp++;
                } while(--n > 0);
        }
}


#if 0
void __inline__ udc_epn_interrupt_enable(int ep)
{
        sharp_uwl(sharp_url(USB_INTRE) | (1 << ep), USB_INTRE);
}

void __inline__ udc_epn_interrupt_disable(int ep)
{
        sharp_uwl(sharp_url(USB_INTRE) & ~(1 << ep), USB_INTRE);
}
#endif

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */


/**
 * start_in_irq - start transmit
 * @ep:
 */
static void start_in_irq (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        u32 csr;

        TRACE_MSG32("START_IN %d", ep);
        csr = sharp_urli(ep, USB_IN_CSR1); 

        if (!(csr & USB_IN_CSR1_FIFO_NOT_EMPTY)) {

                if (endpoint && endpoint->tx_urb) {

                        struct urb *urb = endpoint->tx_urb;
                        TRACE_MSG32("START_IN: %d", urb->actual_length - endpoint->sent);

                        // XXX we may be able or need to use FIFO_FULL information
                        // this would allow us to queue two packets, we could handle this
                        // in two ways, first just assume first fifo sent, call usbd_tx_complete_irq()
                        // and then fill again, more appropriate might be to add second packet length
                        // to endpoint->last, usbd_tx_complete_irq() would have to have a new argument
                        // to tell it how much was sent..., which would still allow bulk transfer to
                        // be marked incomplete if  SENT_STALL was noticed..

                        if ((urb->actual_length - endpoint->sent) > 0) {
                                endpoint->last = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
                                sharp_fifo_write(ep, urb->buffer + endpoint->sent, endpoint->last);
                                sharp_uwli(USB_IN_CSR1_IN_PKT_RDY, ep, USB_IN_CSR1); 
                                csr = sharp_urli(ep, USB_IN_CSR1);

                                //udc_epn_interrupt_enable(ep);
                        } 
                }
                else {
                        TRACE_MSG32("START_IN: no tx urb %d", ep);
                }
        }
        else {
                TRACE_MSG32("START_IN: FIFO NOT EMPTY %d", ep);
        }

        // XXX 
        //TRACE_MSG32("send_data ZLP %d", ep);
        //sharp_uwli(USB_IN_CSR_IN_PKT_RDY | USB_IN_CSR_DATA_END, ep, USB_IN_CSR1); 
}

/**
 * sharp_in_epn - handle interrupt 
 */
static void sharp_in_epn(unsigned int ep, u32 intr)
{
        u32 csr;
        struct usb_endpoint_instance *endpoint;

        TRACE_MSG32("IN_EPN: %d", ep);
        csr = sharp_urli(ep, USB_IN_CSR1);

        if (csr & USB_IN_CSR1_SENT_STALL) {
                sharp_uwli(USB_IN_CSR1_SENT_STALL /*|USB_IN_CSR1_SEND_STALL */, ep, USB_IN_CSR1);
                //csr = sharp_urli(ep, USB_IN_CSR1);
                return;
        }

        // make sure we have a data structure
        if ((endpoint = ep_endpoints[ep])) {
                struct urb *tx_urb;

                TRACE_MSG32("tx_urb: %x", endpoint->tx_urb);

                // process urb to advance buffer pointers (or not if rc set)
                usbd_tx_complete_irq(endpoint, 0);

                TRACE_MSG32("tx_urb: %x", endpoint->tx_urb);
                if (endpoint->tx_urb) {
                        TRACE_MSG16("length: %d sent: %d", endpoint->tx_urb->actual_length, endpoint->sent);
                }
                else {
                        TRACE_MSG("NO TX_URB");
                }

                //start_in_irq(ep, endpoint);

                // continue sending if more data to be sent
                if ((tx_urb = endpoint->tx_urb) && (tx_urb->actual_length > endpoint->sent)) {
                        TRACE_MSG32("IN_EPN: LEFT TO SEND %d", tx_urb->actual_length - endpoint->sent);
                        start_in_irq(endpoint->endpoint_address&0xf, endpoint);
                }
                else {
                        TRACE_MSG("IN_EPN: finished");
                }
        }
}

/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

static __inline__ void sharp_out_epn(unsigned int ep, u32 intr)
{
        struct usb_endpoint_instance *endpoint;

        TRACE_MSG32("OUT_EPN %d", ep);

        if ((endpoint = ep_endpoints[ep])) {
                u32 csr;

                while((csr = sharp_urli(ep, USB_OUT_CSR1)) & (USB_OUT_CSR1_OUT_PKT_RDY | USB_OUT_CSR1_SENT_STALL)) {

                        struct urb *urb;

                        if (csr & USB_OUT_CSR1_SENT_STALL) {
                                TRACE_MSG("OUT_EPN: stall sent, flusgh");
                                sharp_uwli(USB_OUT_CSR1_FIFO_FLUSH, ep, USB_OUT_CSR1);
                        }

                        else if (csr & USB_OUT_CSR1_OUT_PKT_RDY) {

                                int bytes = 0;

                                if (!endpoint->rcv_urb) {

                                        TRACE_MSG32("OUT_EPN: rcv_urb was NULL %d", ep);
                                        usbd_rcv_complete_irq(endpoint, 0, 0);
                                }

                                // XXX we may be able to or need to react to FIFO_FULL and read two packets


                                if ((urb = endpoint->rcv_urb)) {
                                        bytes = sharp_fifo_read(ep, 
                                                        urb->buffer + urb->actual_length + bytes, endpoint->rcv_packetSize);

                                        sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY, ep, USB_OUT_CSR1); 
                                        usbd_rcv_complete_irq(endpoint, bytes, 0);
                                }
                                else {
                                        sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY | USB_OUT_CSR1_FIFO_FLUSH, ep, USB_OUT_CSR1); 
                                        // XXX need to read bytes
                                        usbd_rcv_complete_irq(endpoint, bytes, 1);
                                }
                        }
                }
        }
        else {
                sharp_uwli(USB_OUT_CSR1_FIFO_FLUSH, ep, USB_OUT_CSR1);
        }
}


/* ********************************************************************************************* */
/* Control (endpoint zero)
 */

/*
 * DATA_STATE_NEED_ZLP
 */
static void sharp_ep0_in_zlp(struct usb_endpoint_instance *endpoint, u32 csr)
{
        TRACE_MSG32("ep0_in_zlp: %x", csr);

        /* c.f. Table 15-14 */
        sharp_uwli(/*USB_EP0_CSR_SERVICED_OUT_PKT_RDY |*/ USB_EP0_CSR_IN_PKT_RDY | USB_EP0_CSR_DATA_END, 0, USB_EP0_CSR); 
        endpoint->state = WAIT_FOR_SETUP;
}

/*
 * DATA_STATE_XMIT
 */
static void sharp_ep0_in(struct usb_endpoint_instance *endpoint, u32 csr)
{
        int size;
        struct urb *urb;
        
        TRACE_MSG32("ep0_in: %x", csr);

        if (!(urb = endpoint->tx_urb)) {
                TRACE_MSG("NULL URB:");
                sharp_uwli(/*USB_EP0_CSR_SERVICED_OUT_PKT_RDY |*/ USB_EP0_CSR_DATA_END | USB_EP0_CSR_SEND_STALL, 0, USB_EP0_CSR); 
                return;
        }

        endpoint->last = size = MIN(urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
        TRACE_MSG32("size: %d", size);

        if (size && urb->buffer) {
                sharp_fifo_write(0, urb->buffer + endpoint->sent, size);
        }

        if (!endpoint->tx_urb || (endpoint->last < endpoint->tx_packetSize)) {
                endpoint->state = WAIT_FOR_SETUP;
                TRACE_MSG("ep0_in: wait for status");
                sharp_urli(0, USB_EP0_CSR); // XXX
                sharp_uwli(/*USB_EP0_CSR_SERVICED_OUT_PKT_RDY |*/ USB_EP0_CSR_IN_PKT_RDY | USB_EP0_CSR_DATA_END, 0, USB_EP0_CSR); 
                sharp_urli(0, USB_EP0_CSR); // XXX
        }
        else if ((endpoint->last == endpoint->tx_packetSize) && 
                                ((endpoint->last + endpoint->sent) == urb->actual_length) &&
                                (urb->actual_length < le16_to_cpu(urb->device_request.wLength))
                )
        {
                endpoint->state = DATA_STATE_NEED_ZLP;
                sharp_uwli(/*USB_EP0_CSR_SERVICED_OUT_PKT_RDY |*/ USB_EP0_CSR_IN_PKT_RDY, 0, USB_EP0_CSR); 
                TRACE_MSG("ep0_in: need zlp");
        }
        else {
                TRACE_MSG("ep0_in: not finished");
                sharp_uwli(/*USB_EP0_CSR_SERVICED_OUT_PKT_RDY |*/ USB_EP0_CSR_IN_PKT_RDY, 0, USB_EP0_CSR); 
        }

        TRACE_MSG("ep0_in: finished");

}

/*
 * DATA_STATE_RECV
 */
static void sharp_ep0_out(struct usb_endpoint_instance *endpoint, u32 csr)
{
        struct urb *rcv_urb;
        TRACE_MSG32("ep0_out: %x", csr);

        if (endpoint && (rcv_urb = endpoint->rcv_urb)) {

                int bytes;

                /*
                 * read fifo
                 */
                if ((bytes = sharp_fifo_read(0, (unsigned char *)(rcv_urb->buffer + rcv_urb->actual_length), 16))) {
                        int i;
                        for (i = 0; i < bytes; i+= 8) {
                                TRACE_RECV(rcv_urb->buffer + rcv_urb->actual_length + i);
                        }
                }
                rcv_urb->actual_length += bytes;
                if ((bytes && (bytes < endpoint->rcv_packetSize)) || 
                                ((endpoint->rcv_urb->actual_length == endpoint->rcv_transferSize))) 
                {

                        //urb_append_irq(&endpoint->rcv, endpoint->rcv_urb);
                        TRACE_MSG16("pxa_out_ep0: call recv_setup: len: %x transfer: %x",
                                        endpoint->rcv_urb->actual_length, endpoint->rcv_transferSize);

                        TRACE_RECV(endpoint->rcv_urb->buffer);
                        TRACE_RECV(endpoint->rcv_urb->buffer+8);
                        TRACE_RECV(endpoint->rcv_urb->buffer+16);

                        endpoint->state = WAIT_FOR_SETUP;

                        if (usbd_recv_setup(endpoint->rcv_urb)) {
                                // setup processing failed 
                                TRACE_MSG("      --> bad setup FST");
                                sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY | USB_EP0_CSR_DATA_END | USB_EP0_CSR_SEND_STALL, 
                                                0, USB_EP0_CSR); 
                                return;
                        }

                }
        }

        sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY /*| USB_EP0_CSR_DATA_END*/, 0, USB_EP0_CSR);
}

/*
 * WAIT_FOR_SETUP
 */
static void sharp_ep0_setup(struct usb_endpoint_instance *endpoint, u32 csr)
{
        int i;

        int bytes;
        struct urb *rcv_urb = endpoint->rcv_urb;

        //printk(KERN_INFO"sharp_ep0_setup: rcv_urb: %p\n", rcv_urb);

        if (endpoint->tx_urb) {
                endpoint->tx_urb = NULL;
        }

        ep0_urb.actual_length = 0;
        memset(ep0_urb.buffer, 0, ep0_urb.buffer_length);

        /*
         * read fifo
         */
        if ((bytes = sharp_fifo_read(0, (unsigned char *)&rcv_urb->device_request, 8))) {
                TRACE_SETUP(((struct usb_device_request *)&rcv_urb->device_request));
        }

        /*
         * if this is host 2 device with non-zero wLength we need to wait for data
         */
        if (((ep0_urb.device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) &&
                        le16_to_cpu (ep0_urb.device_request.wLength))
        {
                /*
                 * setup things to receive host 2 device data which should
                 * arrive in the next interrupt.
                 */
                TRACE_MSG32("      <-- setup control write: %02x",csr);
                endpoint->rcv_transferSize = ep0_urb.device_request.wLength;
                ep0_urb.actual_length = 0;
                endpoint->rcv_urb = &ep0_urb;
                endpoint->state = DATA_STATE_RECV;
                TRACE_MSG32("      --> setup host2device: %02x", ep0_urb.device_request.wLength);
                sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY /*| USB_EP0_CSR_DATA_END*/, 0, USB_EP0_CSR); // XXX
                return;
        }

        /*
         * device 2 host or no data setup command, process immediately
         */
        if (usbd_recv_setup(&ep0_urb)) {
                /* setup processing failed, force stall */
                TRACE_MSG32("      --> bad setup FST: %02x", csr);
                sharp_uwli(USB_EP0_CSR_SEND_STALL, 0, USB_EP0_CSR);
                endpoint->state = WAIT_FOR_SETUP;
                sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY | USB_EP0_CSR_DATA_END | USB_EP0_CSR_SEND_STALL, 0, USB_EP0_CSR); 
                return;
        }

        // check direction
        if ((ep0_urb.device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE)
        {
                // Control Write - we are receiving more data from the host

                // allow for premature IN (c.f. 12.5.3 #8)
                TRACE_MSG32("      <-- setup nodata: %02x",csr);
                sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY | USB_EP0_CSR_DATA_END, 0, USB_EP0_CSR); // XXX
        }       
        else {
                // Control Read - we are sending data to the host
                //
                TRACE_MSG32("      <-- setup control read: %02x",csr);

                // verify that we have non-zero request length
                if (!le16_to_cpu (ep0_urb.device_request.wLength)) {
                        udc_stall_ep (0);
                        TRACE_MSG32("      <-- setup stalling zero wLength: %02x", csr);
                        sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY | USB_EP0_CSR_DATA_END | USB_EP0_CSR_SEND_STALL, 
                                        0, USB_EP0_CSR); // XXX
                        return;
                }
                // verify that we have non-zero length response
                if (!ep0_urb.actual_length) {
                        udc_stall_ep (0);
                        TRACE_MSG32("      <-- setup stalling zero response: %02x", csr);
                        sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY | USB_EP0_CSR_DATA_END | USB_EP0_CSR_SEND_STALL, 
                                        0, USB_EP0_CSR); // XXX
                        return;
                }

                TRACE_SENT(ep0_urb.buffer);
                TRACE_SENT(ep0_urb.buffer+8);
                TRACE_SENT(ep0_urb.buffer+24);

                // start sending
                endpoint->tx_urb = &ep0_urb;
                endpoint->sent = 0;
                endpoint->last = 0;
                endpoint->state = DATA_STATE_XMIT;
                sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY /*| USB_EP0_CSR_DATA_END*/, 0, USB_EP0_CSR); // XXX
                sharp_urli(0, USB_EP0_CSR); // XXX
                sharp_ep0_in(endpoint, csr);
                sharp_urli(0, USB_EP0_CSR); // XXX
        }
}


/*
 * handle ep0 interrupt
 */
static void sharp_ep0(u32 intr)
{
        struct usb_endpoint_instance *endpoint = ep_endpoints[0];

        u32 csr = sharp_urli(0, USB_EP0_CSR);
        u32 active = 0;

        TRACE_MSG32("sharp_ep0: csr: %x", csr);

        if (endpoint->tx_urb) {
                usbd_tx_complete_irq(endpoint, 0);
                if (!endpoint->tx_urb && (endpoint->state != DATA_STATE_NEED_ZLP)) {
                        endpoint->state = WAIT_FOR_SETUP;
                }
        }

        /*
         * For overview of what we should be doing see c.f. Chapter 18.1.2.4
         * We will follow that outline here modified by our own global state
         * indication which provides hints as to what we think should be
         * happening..
         */

        /*
         * if SENT_STALL is set
         *      - clear the SENT_STALL bit
         */
        if (csr & USB_EP0_CSR_SENT_STALL) {
                TRACE_MSG32("SENT STALL %x", csr);
                sharp_uwli(/*USB_EP0_CSR_SEND_STALL |*/ USB_EP0_CSR_SENT_STALL, 0, USB_EP0_CSR); // XXX 
                sharp_urli(0, USB_EP0_CSR); // XXX
                endpoint->tx_urb = NULL;
                endpoint->state = WAIT_FOR_SETUP;
        }

        /*
         * if a transfer is in progress && IN_PKT_RDY and OUT_PKT_RDY are clear
         *      - fill EP0 FIFO
         *      - if last packet
         *      -       set IN_PKT_RDY | DATA_END
         *      - else
         *              set IN_PKT_RDY
         */
        if (!( csr & (USB_EP0_CSR_IN_PKT_RDY | USB_EP0_CSR_OUT_PKT_RDY))) {

                TRACE_MSG32("ep0: IN_PKT_RDY and OUT_PKT_RDY clear: %x", csr);

                switch (endpoint->state) {
                case DATA_STATE_XMIT:
                        sharp_ep0_in(endpoint, csr);
                        return; // XXX
                case DATA_STATE_NEED_ZLP:
                        sharp_ep0_in_zlp(endpoint, csr);
                        return;
                default:
                        // Stall?
                        endpoint->tx_urb = NULL;
                        endpoint->state = WAIT_FOR_SETUP;
                        break;
                }
        }

        /* 
         * if SETUP_END is set
         *      - abort the last transfer
         *      - set SERVICED_SETUP_END_BIT
         */
        if (csr & USB_EP0_CSR_SETUP_END) {

                TRACE_MSG32("ep0: SETUP END set: %x", csr);
                sharp_uwli(USB_EP0_CSR_SERVICED_SETUP_END, 0, USB_EP0_CSR); // XXX 
                sharp_urli(0, USB_EP0_CSR); // XXX

                endpoint->tx_urb = NULL;
                endpoint->state = WAIT_FOR_SETUP;

        }

        /*
         * if OUT_PKT_RDY is set
         *      - read data packet from EP0 FIFO
         *      - decode command
         *      - if error
         *              set SERVICED_OUT_PKT_RDY | DATA_END bits | SEND_STALL
         *      - else
         *              set SERVICED_OUT_PKT_RDY | DATA_END bits 
         */
        if (csr & USB_EP0_CSR_OUT_PKT_RDY) {

                TRACE_MSG32("OUT_PKT_RDY %x", csr);

                switch (endpoint->state) {
                case WAIT_FOR_SETUP:
                        sharp_ep0_setup(endpoint, csr);
                        break;
                case DATA_STATE_RECV:
                        sharp_ep0_out(endpoint, csr);
                        break;
                default:
                        // send stall?
                        break;
                }
        }
}                


/* ********************************************************************************************* */
/* Interrupt Handler
 */

static void udc_int_req (int irq, void *dev_id, struct pt_regs *regs)
{
        int flag;
	udc_interrupts++;

        //if (udc_interrupts > 500) {
        //        udc_disable_interrupts (NULL);
        //}

        do {
                u32 intr_out = sharp_url(USB_OUT_INT);
                u32 intr_in = sharp_url(USB_IN_INT);
                u32 intr_int = sharp_url(USB_INT);

                flag = 0;

                if (intr_out) {
                        sharp_uwl(intr_out, USB_OUT_INT);
                        if (intr_out & USB_OUT_INT_EP2_OUT) {
                                sharp_out_epn(2, intr_out);
                        }
                        flag = 1;
                }

                if (intr_in) {
                        sharp_uwl(intr_in, USB_IN_INT);
                        if (intr_in & USB_IN_INT_EP1_IN) {
                                sharp_in_epn(1, intr_in);
                        }
                        if (intr_in & USB_IN_INT_EP3_IN) {
                                sharp_in_epn(3, intr_in);
                        }
                        if (intr_in & USB_IN_INT_EP0) {
                                sharp_ep0(intr_in);
                        }
                        flag = 1;
                }

                if (intr_int) {
                        sharp_uwl(intr_int, USB_INT);
                        if (intr_int & USB_INT_RESET_INT) {
                                struct usb_device_instance *save_udc_device = udc_device;
                                TRACE_MSG("RESET");
                                if (usb_address) {
                                        udc_init();
                                }
                                usbd_device_event (udc_device, DEVICE_RESET, 0);
                                //usbd_device_event (udc_device, DEVICE_DESTROY, 0);
                                //usbd_device_event (udc_device, DEVICE_CREATE, 0);
                                udc_disable();
                                udc_enable(save_udc_device);
                        }
                        if (intr_int & USB_INT_RESUME_INT) {
                                TRACE_MSG("RESUME");
                                usbd_device_event (udc_device, DEVICE_BUS_ACTIVITY, 0);
                        }
                        if (intr_int & USB_INT_SUSPEND_INT) {
                                TRACE_MSG("SUSPEND");
                                usbd_device_event (udc_device, DEVICE_BUS_INACTIVE, 0);
                        }
                        flag = 1;
                }

        }
        while (flag);
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
void udc_start_in_irq (struct urb *urb)
{
        struct usb_endpoint_instance *endpoint = urb->endpoint;
        udc_interrupts++;
        TRACE_MSG("START_IN_IRQ");
        if (endpoint) {
                start_in_irq(endpoint->endpoint_address&0xf, endpoint);
        }
}

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
        int ep;

        printk(KERN_INFO"udc_init:\n");
        /*
         * C.f Chapter 18.1.1 Initializing the USB
         */

        // Disable the USB
        sharp_uwl(0, USB_PM);

        // Reset APB & I/O sides of the USB (c.f. 15.1.2.1 "correct sequence
        // is to set and clear the bit(s)")
        sharp_uwl(USB_RESET_APB | USB_RESET_IO, USB_RESET);
        sharp_uwl(0, USB_RESET);

        // Set MAXP values for each 

        printk(KERN_INFO"udc_init: AAA\n");

        for (ep = 0; ep < UDC_MAX_ENDPOINTS; ep++) {
                struct ep_regs *ep_reg = &ep_regs[ep];
                switch (ep_reg->ep_type) {
                case ep_control:        // XXX check, by implication c.f. 15.1.2.11
                case ep_bulk_in:
                case ep_interrupt:
                        sharp_uwli(ep_reg->size, ep, USB_IN_MAXP);
                        break;
                case ep_bulk_out:
                        sharp_uwli(ep_reg->size, ep, USB_OUT_MAXP);
                        break;
                }

                switch (ep_reg->ep_type) {
                case ep_control:        // XXX check, by implication c.f. 15.1.2.11
                        break;
                case ep_bulk_in:
                case ep_interrupt:
                        sharp_uwli(USB_IN_CSR1_FIFO_FLUSH, ep, ep_reg->csr1); 
                        break;
                case ep_bulk_out:
                        sharp_uwli(USB_OUT_CSR1_FIFO_FLUSH, ep, ep_reg->csr1); 
                        break;
                }
        }

        // enable interrupts
        // XXX sharp_uwl(USB_IN_INT_EP3_IN | USB_IN_INT_EP1_IN | USB_IN_INT_EP0, USB_IN_INT_EN);
        // XXX sharp_uwl(USB_OUT_INT_EP2_OUT, USB_OUT_INT_EN);
        // XXX sharp_uwl(USB_INT_RESET_INT | USB_INT_RESUME_INT, USB_INT_EN);

        // set enable suspend bit
        // XXX
        
        // Enable the USB
        // XXX sharp_uwl(USB_PM_USB_ENABLE, USB_PM);
        
        // Done
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

        printk(KERN_INFO"%s: RESET_EP: %x\n", __FUNCTION__, ep);
        TRACE_MSG32("RESET_EP: %x\n", ep);

        switch(ep) {
        case 0:
                //sharp_uwl(0, USB_IN_INT_EN);
#if 0
                // Disable the USB
                sharp_uwl(0, USB_PM);

                // Reset APB & I/O sides of the USB (c.f. 15.1.2.1 "correct sequence
                // is to set and clear the bit(s)")
                sharp_uwl(USB_RESET_APB | USB_RESET_IO, USB_RESET);
                sharp_uwl(0, USB_RESET);
                // Set MAXP values for each 

                for (ep = 0; ep < UDC_MAX_ENDPOINTS; ep++) {
                        struct ep_regs *ep_reg = &ep_regs[ep];
                        switch (ep_reg->ep_type) {
                        case ep_control:        // XXX check, by implication c.f. 15.1.2.11
                        case ep_bulk_in:
                        case ep_interrupt:
                                sharp_uwl(ep_reg->size, USB_IN_MAXP);
                                break;
                        case ep_bulk_out:
                                sharp_uwl(ep_reg->size, USB_OUT_MAXP);
                                break;
                        }
                }

                // enable interrupts
                sharp_uwl(USB_IN_INT_EP3_IN | USB_IN_INT_EP1_IN | USB_IN_INT_EP0, USB_IN_INT_EN);
                sharp_uwl(USB_OUT_INT_EP2_OUT, USB_OUT_INT_EN);
                sharp_uwl(USB_INT_RESET_INT | USB_INT_RESUME_INT, USB_INT_EN);
                // set enable suspend bit
                // XXX

                // Enable the USB
                sharp_uwl(USB_PM_USB_ENABLE, USB_PM);
                printk(KERN_INFO"%s: enale USB: %x\n", __FUNCTION__, ep);
#endif
                break;
        default:
                break;
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
	// c.f. 15.1.2.2 Table 15-4 address will be used after DATA_END is set
	usb_address = address;
        sharp_uwl(USB_FA_ADDR_UPDATE | (address & USB_FA_FUNCTION_ADDR), USB_FA);
        sharp_url(USB_FA);
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
        // XXX check type and/or direction
	return (((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) || (packetsize > 64)) ?  0 : (logical_endpoint & 0xf);
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
                        ep0_urb.device = device;
                        usbd_alloc_urb_data (&ep0_urb, 512);
                        endpoint->rcv_urb = &ep0_urb;
                }
                // IN
                else if (endpoint->endpoint_address & 0x80) {
                }
                // OUT
                else if (endpoint->endpoint_address) {
                        usbd_fill_rcv(device, endpoint, 10);
                        endpoint->rcv_urb = first_urb_detached(&endpoint->rdy);
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
                        unsigned long flags;
                        local_irq_save (flags);
                        if (ep0_urb.buffer) {
                                lkfree (ep0_urb.buffer);
                                ep0_urb.buffer = NULL;
                        }
                        local_irq_restore (flags);
                }
	}
}

/* ********************************************************************************************* */

/**
 * udc_connected - is the USB cable connected
 *
 * Return non-zeron if cable is connected.
 */
int udc_connected (void)
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
}


/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect (void)
{
}


/* ********************************************************************************************* */

/**
 * udc_all_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts (struct usb_device_instance *device)
{
        sharp_uwl(USB_INT_RESET_INT | USB_INT_RESUME_INT | USB_INT_SUSPEND_INT, USB_INT_EN);
}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts (struct usb_device_instance *device)
{
        sharp_uwl(USB_INT_RESET_INT | USB_INT_RESUME_INT, USB_INT_EN);
}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts (struct usb_device_instance *device)
{
        sharp_uwl(0, USB_IN_INT_EN);
        sharp_uwl(0, USB_OUT_INT_EN);
        sharp_uwl(0, USB_INT_EN);
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

        // enable interrupts
        sharp_uwl(USB_IN_INT_EP3_IN | USB_IN_INT_EP1_IN | USB_IN_INT_EP0, USB_IN_INT_EN);
        sharp_uwl(USB_OUT_INT_EP2_OUT, USB_OUT_INT_EN);
        sharp_uwl(USB_INT_RESET_INT | USB_INT_RESUME_INT, USB_INT_EN);

        // set enable suspend bit
        // XXX
        
        // Enable the USB
        sharp_uwl(USB_PM_USB_ENABLE, USB_PM);
}


/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable (void)
{
        // Disable the USB
        sharp_uwl(0, USB_PM);

        // XXX disable USB_RESET ?

	// reset device pointer
	udc_device = NULL;
}

void udc_exit(void)
{
	// ep0 urb
        unsigned long flags;
        local_irq_save (flags);
        if (ep0_urb.buffer) {
                lkfree (ep0_urb.buffer);
                ep0_urb.buffer = NULL;
        }
        local_irq_restore (flags);
}

/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup_events (struct usb_device_instance *device)
{
	usbd_device_event (device, DEVICE_INIT, 0);
	usbd_device_event (device, DEVICE_CREATE, 0);
	usbd_device_event (device, DEVICE_HUB_CONFIGURED, 0);
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
int udc_request_udc_irq (void)
{
	if (request_irq (IRQ_USB, udc_int_req, SA_INTERRUPT /* | SA_SAMPLE_RANDOM*/, UDC_NAME "UDC Req", NULL) != 0) {
		printk (KERN_INFO "lh7a400: Couldn't request USB Req irq\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * udc_request_cable_irq - request Cable interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_cable_irq (void)
{
	return 0;
}


/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io (void)
{
        return 0;
}

/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq (void)
{
        free_irq (IRQ_USB, NULL);
}

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq (void)
{
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io (void)
{
}


/**
 * udc_regs - dump registers
 *
 * Dump registers with printk
 */
void udc_regs (void)
{
	printk ("\n");
}


