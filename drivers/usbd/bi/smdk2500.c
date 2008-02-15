/*
 * linux/drivers/usbd/bi/smdk2500.c -- USB Device Controller driver. 
 *
 * Copyright (c) 2002, 2003 Belcarra
 *
 * By: 
 *      Chris Lynne <cl@belcarra.com>, 
 *      Bruce Balden (balden@belcarra.com)
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
-----------------------------------------------------
 Function Address Register          [0xf00e0000] = 0x00000000
 Power Management Register          [0xf00e0004] = 0x00000000
 Interrupt Register                 [0xf00e0008] = 0x00000000
 Interrupt Enable Register          [0xf00e000c] = 0x00000400
 Frame Number Register              [0xf00e0010] = 0x00000000
 Disconnect Timer Register          [0xf00e0014] = 0x00000001
 Endpoint0 Common Status Register   [0xf00e0018] = 0x00000001
 Endpoint1 Common Status Register   [0xf00e001c] = 0x00000401
 Endpoint2 Common Status Register   [0xf00e0020] = 0x00000401
 Endpoint3 Common Status Register   [0xf00e0024] = 0x00000401
 Endpoint4 Common Status Register   [0xf00e0028] = 0x00000401
 Write Count Register for Endpoint0 [0xf00e0030] = 0x00000000
 Write Count Register for Endpoint1 [0xf00e0034] = 0x00000000
 Write Count Register for Endpoint2 [0xf00e0038] = 0x00000000
 Write Count Register for Endpoint3 [0xf00e003c] = 0x00000000
 Write Count Register for Endpoint4 [0xf00e0040] = 0x00000000
 Endpoint0 FIFO                     [0xf00e0080] = 0x00000000
 Endpoint1 FIFO                     [0xf00e0084] = 0x2792d43a
 Endpoint2 FIFO                     [0xf00e0088] = 0x9531f414
 Endpoint3 FIFO                     [0xf00e008c] = 0xdefc3cf9
 Endpoint4 FIFO                     [0xf00e0090] = 0x56349d75



 
 */


/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("cl@belcarra.com, balden@belcarra.com");
MODULE_DESCRIPTION ("USB Device SAMSUNG Bus Interface");
MODULE_LICENSE("GPL");

USBD_MODULE_INFO ("SMDK2500");

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>

#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/ctype.h>


#include <linux/netdevice.h>

#include <asm/arch/irq.h>
#include <asm/system.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <linux/delay.h>
#include <asm/arch/timers.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

#include "smdk2500.h"
#include "smdk2500-hardware.h"

#if defined(CONFIG_USBD_SMDK2500_USE_ETH0) || defined(CONFIG_USBD_SMDK2500_USE_ETH1)
//extern unsigned char * get_MAC_address( char * name);

#include "asm/uCbootstrap.h"

#define HWADDR0 "HWADDR0"
#define HWADDR1 "HWADDR1"

static int errno;
_bsc1(char *, getbenv, char *, a)

#define get_MAC_address(x) getbenv(x)

#endif

#include "trace.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


static struct usb_device_instance *udc_device;	// required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb ep0_urb;

/*
 * udc_connected_status is used to track whether the higher levels want us to be connected
 * or disconnected. It is set by udc_connect() and reset by udc_disconnect(). 
 *
 * If udc_connected_status is reset then a USB RESET will not re-enable interrupts. 
 *
 * udc_disconnect() will disconnect from the bus using the DISCON facility which causes
 * the host to attempt to re-enumerate. This will fail because interrupts are disable.
 *
 * udc_connect() will disconnect from the bus, but after the RESET event interrupts will be 
 * enabled and enumeration will succeed.
 *
 */
static unsigned char udc_connected_status;

static unsigned char usb_address;

extern unsigned int udc_interrupts;
unsigned int tx_interrupts;
unsigned int rx_interrupts;
unsigned int sus_interrupts;
unsigned int req_interrupts;
unsigned int sof_interrupts;

unsigned char hexdigit (char c)
{
        c = __tolower(c);
	return isxdigit (c) ? (isdigit (c) ? (c - '0') : (c - 'a' + 10)) : 0;
}

/* ********************************************************************************************* */
/* IO
 */

char * smdk_usb_regs[] = {
        "usb FA",       // 0x000 0
        "usb PM",       // 0x000 4
        "usb INTR",     // 0x000 8
        "usb INTRE",    // 0x000 c

        "usb FN",       // 0x001 0
        "usb DISCONN",  // 0x001 4
        "usb EP0CSR",   // 0x001 8
        "usb EP1CSR",   // 0x001 c

        "usb EP2CSR",   // 0x002 0
        "usb EP3CSR",   // 0x002 4
        "usb EP4CSR",   // 0x002 8
        "",             // 0x002 c

        "usb WCEP0",    // 0x003 0
        "usb WCEP1",    // 0x003 4
        "usb WCEP2",    // 0x003 8
        "usb WCEP3",    // 0x003 c

        "usb WCEP4",    // 0x004 0
        "",             // 0x004 4
        "",             // 0x004 8
        "",             // 0x004 c

        "",             // 0x005 0
        "",             // 0x005 4
        "",             // 0x005 8
        "",             // 0x005 c

        "",             // 0x006 0
        "",             // 0x006 4
        "",             // 0x006 8
        "",             // 0x006 c

        "",             // 0x007 0
        "",             // 0x007 4
        "",             // 0x007 8
        "",             // 0x007 c

        "usb EP0",      // 0x008 0
        "usb EP1",      // 0x008 4
        "usb EP2",      // 0x008 8
        "usb EP3",      // 0x008 c

        "usb EP4",      // 0x090 0
};

typedef enum ep_type {
        ep_control, ep_bulk_in, ep_bulk_out, ep_interrupt
} ep_type_t;

typedef struct ep_regs {
        u32 csr;
        u32 wc;
        u32 fifo;
        u32 mask;
        ep_type_t ep_type;
        u8 size;
	u8 maxp_mask;
} ep_regs_t;

/*
 * map logical to physical 
 */
struct ep_regs ep_regs[UDC_MAX_ENDPOINTS] = {

        { csr: USB_EP0CSR, wc: USB_WCEP0, fifo: USB_EP0, size: EP0_PACKETSIZE, maxp_mask: USB_MAXP_64, ep_type: ep_control,   
        mask: USB_INTR_EP0I },

        { csr: USB_EP1CSR, wc: USB_WCEP1, fifo: USB_EP1, size: 32, maxp_mask: USB_MAXP_32, ep_type: ep_interrupt, 
        mask: USB_INTR_EP1I },

        { csr: USB_EP2CSR, wc: USB_WCEP2, fifo: USB_EP2, size: 32, maxp_mask: USB_MAXP_32, ep_type: ep_interrupt, 
        mask: USB_INTR_EP2I },

        { csr: USB_EP3CSR, wc: USB_WCEP3, fifo: USB_EP3, size: 64, maxp_mask: USB_MAXP_64, ep_type: ep_bulk_in,   
        mask: USB_INTR_EP3I },

        { csr: USB_EP4CSR, wc: USB_WCEP4, fifo: USB_EP4, size: 64, maxp_mask: USB_MAXP_64, ep_type: ep_bulk_out,  
        mask: USB_INTR_EP4I },

};


static __inline__ char *smdk_ureg_name(unsigned int port)
{
        u32 saved = port;
        port = (port >> 2) & 0xff;
        if (port >= (sizeof(smdk_usb_regs)/4)) {
                TRACE_MSG16("uname: %x %x", port, sizeof(smdk_usb_regs)/4);
                TRACE_MSG32("uname: %x", saved);
                TRACE_MSG32("uname: %x", port);
        }
        return (port < (sizeof(smdk_usb_regs)/4)) ? smdk_usb_regs[port] : "unknown";
}

static __inline__ u32 smdk_url(unsigned int port)
{
        u32 val;
        if (!port) {
                TRACE_MSG32("RL INVALID PORT %d", port);
                return 0;
        }
        val =  *(volatile u32 *) port;
        TRACE_R(smdk_ureg_name(port), val);
        return val;
}

static __inline__ void smdk_uwl(u32 val, unsigned int port)
{
        if (!port) {
                TRACE_MSG32("WL INVALID PORT %d", port);
                return;
        }
        *(volatile u32 *)(port) = val;
        TRACE_W(smdk_ureg_name(port), val);
}

static __inline__ void smdk_uwl_or(u32 val, unsigned int port)
{
        if (!port) {
                TRACE_MSG32("WL INVALID PORT %d", port);
                return;
        }
        *(volatile u32 *)(port) |= val;
        TRACE_W(smdk_ureg_name(port), val);
}

static __inline__ int smdk_fifo_read(int epn, unsigned char * cp, int max)
{
        int bytes;
        unsigned int *ip = (unsigned int *)cp;
        struct ep_regs *ep = &ep_regs[epn];
        u32 fifo = ep->fifo;
        int count;
        int n;

        // get count, ensure it is less than max
        count = ((*(volatile u32 *)ep->wc) & USB_WCEPN_WRTCNT) >> 16;
        //TRACE_MSG32("fifo read: %d", count);
        bytes = count = MIN(count, max);

        // convert to word count
        count = (count + 3) / 4;

        // number of interations
        n = (count + 3) >>2;

        // do copy
        switch (count % 4) {
        case 0: do {
                        *ip++ = *(volatile u32 *) fifo;
                case 3:
                        *ip++ = *(volatile u32 *) fifo;
                case 2:
                        *ip++ = *(volatile u32 *) fifo;
                case 1:
                        *ip++ = *(volatile u32 *) fifo;
                } while (--n > 0);
        }

        return bytes;
}

static __inline__ void smdk_fifo_write(int epn, unsigned char * cp, int count)
{
        unsigned int *ip = (unsigned int *)cp;
        struct ep_regs *ep = &ep_regs[epn];
        u32 fifo = ep->fifo; 
        int n;
        
        // c.f. 10.5.12 - MCU first writes the byte-count number of data to
        // be loaded into FIFO, then write data into FIFO.
 
        // write count
        *(volatile u32 *) ep->wc = count;
        
        // convert to words
        count = (count + 3) / 4;

        // number of interations
        n = (count + 3) >> 2; 
        
        // do copy
        switch (count %4) {
        case 0:
                do { 
                        *(volatile u32 *) fifo = *ip++;
                case 3:
                        *(volatile u32 *) fifo = *ip++;
                case 2: 
                        *(volatile u32 *) fifo = *ip++;
                case 1: 
                        *(volatile u32 *) fifo = *ip++;
                } while (--n > 0);
        }       
}


/*
static __inline__ void smdk_disable_interrupts(void){
	smdk_uwl(0, USB_INTRE);
        printk(KERN_INFO"smdk_disable_interrupts: %d %x\n", ep, smdk_url(USB_INTRE));
}

void __inline__ udc_epn_interrupt_enable(int ep)
{
        smdk_uwl(smdk_url(USB_INTRE) | (1 << ep), USB_INTRE);
        printk(KERN_INFO"udc_epn_interrupt_enable: %d %x\n", ep, smdk_url(USB_INTRE));
}

void __inline__ udc_epn_interrupt_disable(int ep)
{
        smdk_uwl(smdk_url(USB_INTRE) & ~(1 << ep), USB_INTRE);
        printk(KERN_INFO"udc_epn_interrupt_disable: %d %x\n", ep, smdk_url(USB_INTRE));
}
*/

/* ********************************************************************************************* */
/* Control (endpoint zero)
 */

static void smdk_ep0_in_zlp(struct usb_endpoint_instance *endpoint, u32 csr)
{
        struct ep_regs *ep = &ep_regs[0];
        smdk_uwl_or(USB_EP0CSR_USBINRDY | USB_EP0CSR_USBDEND, ep->csr);
        endpoint->state = WAIT_FOR_SETUP;
}

static void smdk_ep0_in(struct usb_endpoint_instance *endpoint, u32 csr)
{
        int size;
        struct urb *urb;
        struct ep_regs *ep = &ep_regs[0];
        
        //TRACE_MSG32("ep0_in: %x", csr);
        
        smdk_uwl_or(USB_EP0CSR_USBSVORDY, ep->csr);

        if (!(urb = endpoint->tx_urb)) {
                TRACE_MSG("NULL URB:");
                smdk_uwl_or(USB_EP0CSR_USBSDSTALL | USB_EP0CSR_USBDEND, ep->csr);
                return;
        }       
        
        endpoint->last = size = MIN(urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
        TRACE_MSG32("size: %d", size);
        
        if (size && urb->buffer) {
                smdk_fifo_write(0, urb->buffer + endpoint->sent, size);
        }       
        
        if (!endpoint->tx_urb || (endpoint->last < endpoint->tx_packetSize)) {
                endpoint->state = WAIT_FOR_SETUP;
                //TRACE_MSG("ep0_in: wait for status");
                smdk_uwl_or(USB_EP0CSR_USBINRDY | USB_EP0CSR_USBDEND, ep->csr);
        }       
        else if ((endpoint->last == endpoint->tx_packetSize) &&
                                ((endpoint->last + endpoint->sent) == urb->actual_length) &&
                                (urb->actual_length < le16_to_cpu(urb->device_request.wLength))
                )
        {
                //TRACE_MSG("ep0_in: need zlp");
                endpoint->state = DATA_STATE_NEED_ZLP;
                //smdk_uwl_or(USB_EP0CSR_USBINRDY | USB_EP0CSR_USBDEND, ep->csr);
                smdk_uwl_or(USB_EP0CSR_USBINRDY, ep->csr);
        }
        else {
                TRACE_MSG("ep0_in: not finished");
                smdk_uwl_or(USB_EP0CSR_USBINRDY, ep->csr);
        }

        //TRACE_MSG("ep0_in: finished");

}

static void smdk_ep0_out(struct usb_endpoint_instance *endpoint, u32 csr)
{
        //int i;
        struct ep_regs *ep = &ep_regs[0];

        //TRACE_MSG("ep0_out: ");

        if (endpoint && endpoint->rcv_urb) {
                int len = 0;
                unsigned char *cp = endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length;

                if (cp) {
                        len = smdk_fifo_read(0, cp, endpoint->rcv_packetSize);
                }
                else {
                        // flush
                }
                endpoint->rcv_urb->actual_length += len;

                if ((len && (len < endpoint->rcv_packetSize)) ||
                                ((endpoint->rcv_urb->actual_length == endpoint->rcv_transferSize))) 
                {
                        //TRACE_RECV(endpoint->rcv_urb->buffer);
                        //TRACE_RECV(endpoint->rcv_urb->buffer + 8);
                        //TRACE_RECV(endpoint->rcv_urb->buffer + 16);

                        endpoint->state = WAIT_FOR_SETUP;

                        if (usbd_recv_setup(&ep0_urb)) {
                                // stall
                                TRACE_MSG("recv setup failed - STALL");
                                smdk_uwl_or(USB_EP0CSR_USBSDSTALL | USB_EP0CSR_USBSVORDY, ep->csr);
                                return;
                        }
                        smdk_uwl_or(USB_EP0CSR_USBDEND | USB_EP0CSR_USBSVORDY, ep->csr);
                        return;
                }
                smdk_uwl_or(USB_EP0CSR_USBSVORDY, ep->csr);
        }
}

static void smdk_ep0_setup(struct usb_endpoint_instance *endpoint, u32 csr)
{
        int size;
        struct ep_regs *ep = &ep_regs[0];

        // reset
        endpoint->tx_urb = NULL;
        ep0_urb.actual_length = 0;
        memset(ep0_urb.buffer, 0, ep0_urb.buffer_length);

        // read request
        if ((size = smdk_fifo_read(0, (unsigned char *)&ep0_urb.device_request, 8))) {
                TRACE_SETUP(&ep0_urb.device_request);
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
                smdk_uwl_or(USB_EP0CSR_USBSVORDY, ep->csr);
                return;
        }               
                
        /*
         * device 2 host or no data setup command, process immediately
         */
        if (usbd_recv_setup(&ep0_urb)) {
                /* setup processing failed, force stall */
                TRACE_MSG32("      --> bad setup FST: %02x", csr);
                endpoint->state = WAIT_FOR_SETUP;
                smdk_uwl_or(USB_EP0CSR_USBSDSTALL | USB_EP0CSR_USBSVORDY, ep->csr);
                return;
        }

        // check direction
        if ((ep0_urb.device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE)
        {       
                // Control Write - we are receiving more data from the host
                
                // allow for premature IN (c.f. 12.5.3 #8)
                TRACE_MSG32("      <-- setup nodata: %02x",csr);
                smdk_uwl_or(USB_EP0CSR_USBDEND | USB_EP0CSR_USBSVORDY, ep->csr);
                return;
        }
        else {
                // Control Read - we are sending data to the host
                //
                TRACE_MSG32("      <-- setup control read: %02x",csr);

                // verify that we have non-zero request length
                if (!le16_to_cpu (ep0_urb.device_request.wLength)) {
                        udc_stall_ep (0);
                        TRACE_MSG32("      <-- setup stalling zero wLength: %02x", csr);
                        smdk_uwl_or(USB_EP0CSR_USBSDSTALL | USB_EP0CSR_USBDEND | USB_EP0CSR_USBSVORDY, ep->csr);
                        return;
                }
                // verify that we have non-zero length response
                if (!ep0_urb.actual_length) {
                        udc_stall_ep (0);
                        TRACE_MSG32("      <-- setup stalling zero response: %02x", csr);
                        smdk_uwl_or(USB_EP0CSR_USBSDSTALL | USB_EP0CSR_USBDEND | USB_EP0CSR_USBSVORDY, ep->csr);
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
                //sharp_uwli(USB_EP0_CSR_SERVICED_OUT_PKT_RDY /*| USB_EP0_CSR_DATA_END*/, 0, USB_EP0_CSR); // XXX
                //smdk_uwl_or(USB_EP0CSR_USBSVORDY, ep->csr);
                smdk_ep0_in(endpoint, csr);
                return;
        }
}


static void smdk_ep0(void)
{
        struct ep_regs *ep = &ep_regs[0];
        struct usb_endpoint_instance *endpoint = ep_endpoints[0];

        u32 csr;

        //TRACE_MSG32("ep0: %d", 0);

        csr = smdk_url(ep->csr);


        if (csr & USB_EP0CSR_USBSDSTALL) {
                smdk_uwl(0, ep->csr);
                smdk_url(ep->csr);
        }

        // Sent Stall?
        if (csr & USB_EP0CSR_USBSTSTALL) {
                TRACE_MSG("ep0: SETUP STALLED");
                smdk_uwl_or(USB_EP0CSR_USBSVSET, ep->csr);
                endpoint->state = WAIT_FOR_SETUP;
                return;
        }

        // Setup End?
        if (csr & USB_EP0CSR_USBSVSET) {
                TRACE_MSG("ep0: SETUP END");
                smdk_uwl_or(USB_EP0CSR_USBSVSET, ep->csr);
                endpoint->state = WAIT_FOR_SETUP;
                // XXX reset state etc
                return;
        }

        // receive
        if (csr & USB_EP0CSR_USBORDY) {

                if (endpoint->tx_urb) {
                        endpoint->tx_urb = NULL;
                }

                switch(endpoint->state) {
                case WAIT_FOR_SETUP:
                        smdk_ep0_setup(endpoint, csr);
                        break;
                case DATA_STATE_RECV:
                        smdk_ep0_out(endpoint, csr);
                        break;
                default:
                        // stall?
                        break;
                }
                return;
        }

        switch (endpoint->state) {
        case DATA_STATE_XMIT:
                // transmitting?
                //TRACE_MSG("ep0: XMIT");
                if (endpoint->tx_urb) {
                        struct urb *tx_urb;
                        // process urb to advance buffer pointers
                        usbd_tx_complete_irq(endpoint, 0);

                        // continue sending if more data to be sent
                        if ((tx_urb = endpoint->tx_urb)) {

                                if (tx_urb->actual_length > endpoint->sent) {
                                        if ((tx_urb->actual_length - endpoint->sent) < endpoint->tx_packetSize) {
                                                TRACE_MSG32("starting short packet %d", 0);
                                                //start_in(endpoint->endpoint_address&0xf, endpoint);
                                                smdk_ep0_in(endpoint, csr);
                                        }
                                        else {
                                                TRACE_MSG32("LEFT TO SEND %d", tx_urb->actual_length - endpoint->sent);
                                                //start_in(endpoint->endpoint_address&0xf, endpoint);
                                                smdk_ep0_in(endpoint, csr);
                                        }
                                }
                                else if ((endpoint->sent < le16_to_cpu(tx_urb->device_request.wLength)) &&
                                                !(endpoint->sent % endpoint->tx_packetSize))
                                {
                                        //send_zlp(0);
                                        smdk_ep0_in(endpoint, csr);
                                        endpoint->tx_urb = NULL;
                                }
                        }
                }
                break;

        case DATA_STATE_NEED_ZLP:
                //TRACE_MSG("ep0: ZLP");
                smdk_ep0_in_zlp(endpoint, csr);
                break;

        default:
                //TRACE_MSG("ep0: UNKNOWN");
                // XXX Stall?
                //smdk_uwl_or(USB_EP0CSR_USBSDSTALL | USB_EP0CSR_USBDEND | USB_EP0CSR_USBSVORDY, ep->csr);
                break;
        }
        return;
}

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */

static void __inline__ send_zlp(unsigned char ep)
{
        TRACE_MSG32("send_data ZLP %d", ep);
        smdk_uwl(0, ep_regs[ep].wc); 
        smdk_uwl_or(USB_EPNCSR_IINRDY, ep_regs[ep].csr);
}

/**
 * start_in - start transmit
 * @ep:
 */
static void start_in (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        //TRACE_MSG32("start_in %d", ep);
        if (endpoint && endpoint->tx_urb) {

		struct urb *urb = endpoint->tx_urb;
                //TRACE_MSG32("start_in %d", urb->actual_length - endpoint->sent);

		if ((urb->actual_length - endpoint->sent) > 0) {
			endpoint->last = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
//#define USE_DMA_IN 
#ifdef USE_DMA_IN
                        /*
                         * The most we can do is to use DMA to fill the FIFO IFF we have a full
                         * sized data packet, this may (or may not) be marginally faster.
                         *
                         * First tests seem to not be too favourable, but write buffer was disabled
                         * and test size is small. 340 vs 360 (dma vs no-dma)
                         */
                        if (endpoint->last >= endpoint->tx_packetSize) {
                                smdk_uwl(endpoint->tx_packetSize, ep_regs[ep].wc); 
                                *(volatile *)DTCR4 = endpoint->tx_packetSize;
                                *(volatile *) DSAR4 = urb->buffer + endpoint->sent;
                                *(volatile *)DRER4 = DCON_RE;
                        }
                        else {
                                smdk_fifo_write(ep, urb->buffer + endpoint->sent, endpoint->last);
                                *(volatile *) ep_regs[ep].csr |= USB_EPNCSR_IINRDY;
                        }
#else
			smdk_fifo_write(ep, urb->buffer + endpoint->sent, endpoint->last);
                        if (endpoint->last < endpoint->tx_packetSize) {
                                *(volatile u32 *) ep_regs[ep].csr |= USB_EPNCSR_IINRDY;
                        }
#endif
		} 
	}
}

static void smdk_in_epn(unsigned int epn)
{
        u32 csr;
        int rc = 0;
        struct ep_regs *ep = &ep_regs[epn];
        struct usb_endpoint_instance *endpoint = ep_endpoints[epn];
        struct urb *tx_urb = endpoint->tx_urb;

        //TRACE_MSG32("smdk_in_epn %d", ep);

        //csr = smdk_url(ep->csr);
        csr = *(volatile u32 *) ep->csr;

        // still busy
        if (csr & USB_EPNCSR_IINRDY) {
                TRACE_MSG("IINRDY");
                return;
        }
        // not empty
        if (csr & USB_EPNCSR_INEMP) {
                TRACE_MSG("INEMP");
                return;
        }
        // underflow
        if (csr & USB_EPNCSR_IUNDER) {
                smdk_uwl_or(USB_EPNCSR_IUNDER, ep->csr);
                TRACE_MSG("IUNDER");
                // XXX force stall?
                return;
        }
        if (csr & USB_EPNCSR_ISTSTALL) {
                TRACE_MSG("ISTSTALL");
                smdk_uwl_or(USB_EPNCSR_ISTSTALL, ep->csr);
                return;
        }


        // process urb to advance buffer pointers (or not if rc set)
        usbd_tx_complete_irq(endpoint, rc);

        // continue sending if more data to be sent
        if (tx_urb && (tx_urb->actual_length > endpoint->sent)) {
                //TRACE_MSG32("smdk_in_epn - LEFT TO SEND %d", tx_urb->actual_length - endpoint->sent);
                start_in(endpoint->endpoint_address&0xf, endpoint);
        }
}


/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

static void smdk_out_epn(unsigned int epn)
{
        u32 csr;
        struct urb *urb;
        struct ep_regs *ep = &ep_regs[epn];
        struct usb_endpoint_instance *endpoint = ep_endpoints[epn];
        int count;

        if (!endpoint->rcv_urb) {
                TRACE_MSG32("smdk_out_epn - rcv_urb was NULL %d", epn);
                usbd_rcv_complete_irq(endpoint, 0, 0);
        }

        //csr = smdk_url(ep->csr);
        csr = *(volatile u32 *)ep->csr;

        if (csr & (USB_EPNCSR_OOVER | USB_EPNCSR_ODERR | USB_EPNCSR_OFFLUSH | USB_EPNCSR_OSTSTALL)) {
                usbd_rcv_complete_irq(endpoint, 0, 1);
                THROW(error_flush);
        }

//#define USE_DMA_OUT
#ifdef USE_DMA_OUT
        /*
         * The best we can do is use DMA to unload the already full FIFO. Since we don't
         * want to implement a DMA ISR we just have to hope that DMA actually completes
         * before the network layer gets around to trying to use the data.
         *
         * First performance tests not favourable, 540 vs 617 (dma vs nodma). This would
         * indicate that DMA is taking longer to unload the fifo than PIO. 
         */
        count = (*(volatile *)(ep->wc) & USB_WCEPN_WRTCNT) >> 16;
        if ((urb = endpoint->rcv_urb)) {
                *(volatile *) DTCR5 = count;
                *(volatile *) DDAR5 = urb->buffer + urb->actual_length;
                *(volatile *) DRER5 = DCON_RE;
                usbd_rcv_complete_irq(endpoint, count, 0);
        }
        else {
                THROW(error_flush);
                usbd_rcv_complete_irq(endpoint, count, 1);
        }
#else
        if ((urb = endpoint->rcv_urb)) {
                usbd_rcv_complete_irq(endpoint, 
                                smdk_fifo_read(epn, urb->buffer + urb->actual_length, endpoint->rcv_packetSize),
                                0);
        }
        else {
                count = ((*(volatile u32 *)ep->wc) & USB_WCEPN_WRTCNT) >> 16;
                usbd_rcv_complete_irq(endpoint, MIN(count, endpoint->rcv_packetSize), 1);
                THROW(error_flush);
        }
#endif
        CATCH(error_flush) {
                smdk_uwl(USB_EPNCSR_OFFLUSH, ep->csr);
                usbd_rcv_complete_irq(endpoint, 0, 1);
        }
}

/* ********************************************************************************************* */
/* Interrupt Handler
 */

static void udc_int_req (int irq, void *dev_id, struct pt_regs *regs)
{
        //int i = 0;
        u32 intr;
        //u32 tcnt1;
        //tcnt1 = *(volatile u32 *)TCNT1;
        
        udc_interrupts++;

        // read and clear interrupts
        intr = *(volatile u32 *)USB_INTR;
        *(volatile u32 *)USB_INTR = intr;

        // handle the high priority interrupts, hard coded to get best performance
        if (intr & USB_INTR_EP4I) {
                smdk_out_epn(4);
        }
        if (intr & USB_INTR_EP3I) {
                smdk_in_epn(3);
        }

        // handle less common, low priority interrupts

        if (intr & (USB_INTR_SUSI | USB_INTR_RESI | USB_INTR_RSTI | USB_INTR_DISCI | 
                                USB_INTR_EP0I | USB_INTR_EP1I | USB_INTR_EP2I)) 
        {
                if (intr & USB_INTR_RSTI) {
                        // handle bus reset
                        if (udc_connected_status) {
                                udc_init();
                                TRACE_MSG("RESET");
                                usbd_device_event (udc_device, DEVICE_RESET, 0);
                                smdk_uwl(0x0D1F, USB_INTRE);
                        }
                }

                if (intr & USB_INTR_EP0I) {
                        smdk_ep0();
                }

                if (intr & USB_INTR_EP1I) {
                        smdk_in_epn(1);
                }

                if (intr & USB_INTR_EP2I) {
                        smdk_in_epn(2);
                }

                if (intr & USB_INTR_SUSI) {
                        TRACE_MSG("SUSPEND");
                        // disable suspend interrupt
                }
                if (intr & USB_INTR_RESI) {
                        TRACE_MSG("RESUME");
                        // re-enable suspend interrupt
                }
                if (intr & USB_INTR_DISCI) {
                        TRACE_MSG("DISCONNECT");
                        // handle disconnect finished
                }
        }
        //TRACE_MSG32("%8u", (tcnt1 - *(volatile u32 *)TCNT1) / CONFIG_USBD_SMDK2500_BCLOCK);
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
        //TRACE_MSG32("start_in_irq: %x", endpoint);
        if (urb->endpoint) {
                start_in(endpoint->endpoint_address&0xf, endpoint);
        }
}

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
        int i;

        // disable interrupts
        smdk_uwl(0, USB_INTRE);

        for (i = 0; i < 5; i++) {

                struct ep_regs *ep = &ep_regs[i];

                if (ep->csr) {
                        switch (ep->ep_type) {
                        case ep_control:
                                TRACE_MSG("CONTROL");
                                smdk_uwl(USB_EPNCSR_CSR2SET | USB_EP0CSR_USBSVORDY | USB_EP0CSR_USBSVSET, ep->csr);
                                break;

                        case ep_bulk_in:
                                TRACE_MSG("INT");
                                smdk_uwl(USB_EPNCSR_CSR2SET | USB_EPNCSR_IATSET | USB_EPNCSR_MODE | USB_EPNCSR_IFFLUSH, ep->csr);
                                break;

                        case ep_interrupt:
                                TRACE_MSG("IN");
                                smdk_uwl(USB_EPNCSR_CSR2SET | USB_EPNCSR_IATSET | USB_EPNCSR_MODE | USB_EPNCSR_IFFLUSH, ep->csr);
                                break;

                        case ep_bulk_out:
                                TRACE_MSG("OUT");
                                smdk_uwl(USB_EPNCSR_CSR2SET | USB_EPNCSR_OATCLR | USB_EPNCSR_OFFLUSH, ep->csr);
                                break;
                        }
                        smdk_uwl(ep->maxp_mask | USB_EPNCSR_MAXPSET, ep->csr);
                        smdk_url(ep->csr);
                }
        }

#ifdef USE_DMA_IN
        // bulk in
        *(volatile *) DDAR4 = USB_EP3;
        *(volatile *) DSAR4 = 0;
        *(volatile *) DCON4 = DCON_SB_SINGLE | DCON_MODE_SOFT | DCON_TS_32 | DCON_SD_INC | DCON_DD_FIXED;
#endif
#ifdef USE_DMA_OUT
        // bulk out
        *(volatile *) DDAR5 = 0;
        *(volatile *) DSAR5 = USB_EP4;
        *(volatile *) DCON5 = DCON_SB_SINGLE | DCON_MODE_SOFT | DCON_TS_32 | DCON_SD_FIXED | DCON_DD_INC;
#endif
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
        // send stall
	if (ep < UDC_MAX_ENDPOINTS) {
                smdk_uwl_or(USB_EPNCSR_ISDSTALL, ep_regs[ep].csr);
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
        // clear data toggle
	if (ep < UDC_MAX_ENDPOINTS) {
                smdk_uwl_or(USB_EPNCSR_ICLTOG, ep_regs[ep].csr);
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
        smdk_uwl(USB_FA_BAU | address, USB_FA);
	usb_address = address;
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
        TRACE_MSG32("udc_setup_ep: %d", ep);
	if (ep < UDC_MAX_ENDPOINTS) {

		ep_endpoints[ep] = endpoint;

                // ep0
                if (ep == 0) {
                        // ep0 urb
                        ep0_urb.device = device;
                        usbd_alloc_urb_data (&ep0_urb, 512);
                        memset(ep0_urb.buffer, 0, ep0_urb.buffer_length);
                        ep0_urb.actual_length = 0;

                        endpoint->rcv_urb = &ep0_urb;
                }
                // IN
                else if (endpoint->endpoint_address & 0x80) {
                }
                // OUT
                else if (endpoint->endpoint_address) {
                        TRACE_MSG32("udc_setup_ep: OUT FILL %d", ep);
			usbd_fill_rcv (device, endpoint, 10);
                        endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
                }
	}

#if defined(CONFIG_USBD_SMDK2500_USE_ETH0) || defined(CONFIG_USBD_SMDK2500_USE_ETH1)
        if (ep == 0) {
#if defined(CONFIG_USBD_SMDK2500_USE_ETH0)
                unsigned char *ptr = get_MAC_address(HWADDR0);
                printk(KERN_INFO"ETH0: %s %s\n", HWADDR0, ptr);
#else
                unsigned char *ptr = get_MAC_address(HWADDR1);
                printk(KERN_INFO"ETH1: %s %s\n", HWADDR1, ptr);
#endif
                if (ptr) {
                        int i;
                        char *cp = ptr;

                        for (i = 0; i < ETH_ALEN; i++) {
                                device->dev_addr[i] =
                                        hexdigit (*cp++) << 4 |
                                        hexdigit (*cp++);
                                cp++;
                        }
                }
                printk(KERN_INFO"setup: %02x %02x %02x %02x %02x %02x\n",
                                device->dev_addr[0],
                                device->dev_addr[1],
                                device->dev_addr[2],
                                device->dev_addr[3],
                                device->dev_addr[4],
                                device->dev_addr[5]
                      );
        }
#endif
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
                                ep0_urb.device = ep0_urb.device = NULL;
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
        udc_connected_status = 1;
        smdk_uwl(USB_DISCONN_DISSTRT | 0x100000, USB_DISCONN);
}


/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect (void)
{
        udc_connected_status = 0;
        smdk_uwl(USB_DISCONN_DISSTRT | 0x100000, USB_DISCONN);
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
        smdk_uwl(0x0D1F, USB_INTRE);
        //printk(KERN_INFO"udc_all_interrupts: %x\n", smdk_url(USB_INTRE));
}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts (struct usb_device_instance *device)
{
        smdk_uwl(0x0D1F, USB_INTRE);
        //printk(KERN_INFO"udc_suspended_interrupts: %x\n", smdk_url(USB_INTRE));
}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts (struct usb_device_instance *device)
{
        smdk_uwl(0, USB_INTRE);
        //printk(KERN_INFO"udc_disable_interrupts: %x\n", smdk_url(USB_INTRE));
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
}


/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable (void)
{
	// disable UDC XXX
        //smdk_uwl(0x0000, USBD_ENABLE);

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
                ep0_urb.device = ep0_urb.device = NULL;
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
	//usbd_device_event (device, DEVICE_RESET, 0);	// XXX should be done from device event
}


/* ********************************************************************************************* */

/**
 * udc_name - return name of USB Device Controller
 */
char *udc_name (void)
{
	return UDC_NAME;
}

#ifdef LOW_LEVEL_ARM_IRQ
static struct irqaction irq_udc_req =
{
	handler: udc_int_req, flags: 0, mask: 0, name: "UDC Req", dev_id: NULL, next: NULL
};
#endif

/**
 * udc_request_udc_irq - request UDC interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_udc_irq (void)
{
#ifdef LOW_LEVEL_ARM_IRQ
	setup_arm_irq(SRC_IRQ_USB, &irq_udc_req);
#else
	TRACE_MSG32("Requesting IRQ %d", SRC_IRQ_USB);
	if(request_irq(SRC_IRQ_USB, udc_int_req, SA_INTERRUPT, UDC_NAME, &ep_regs[0])){
		TRACE_MSG("Request not successful");
                return -EINVAL;
	}

#endif
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
/*
        free_irq (XXXXXXXXXX, NULL);
*/
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
	//printk ("\n");
}


