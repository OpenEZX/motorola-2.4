/*
 * linux/drivers/usbd/bi/au1x00.c -- USB Device Controller driver. 
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2002 Lineo
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
 * Notes
 *
 * 1. EP0 - packetsize of 8 does not work, this means that PIO cannot be
 * used for IN (transmit). DMA must be used. Only 16, 32 and 64 are usable.
 * Update, even though we couldn't program the endpoint size to 8, we just
 * set it to 8 in the device descriptor and things work ok. This works and
 * we don't need or use DMA for either direction for EP0.
 *
 * 2. For each BULK transfer the first N packets must be sent with the full
 * packetsize and the last must be sent as a short packet. The packetsize
 * must be set before enabling the DMA.
 *
 * 3. For IN endpoints an interrupt will be generated for EVERY IN packet
 * including ones that will be NAK'd. To reduce overhead the interrupt
 * should only be enabled when there is data being sent and we need to know
 * when it has been sent.
 *
 * 4. The AU1x00 auto-acks many setup commands and is very sensitive to
 * latency of the irq handler processing the interrupt and clearing the ep0
 * fifo. Like the pxa we need to spool the requests and process later when
 * we receive a request for data.  There is a special test in the bulk data
 * receive handler to check if there are spooled requests to process.
 *
 * 5. There is a time sensitive problem in au_in_epn(). Without a udelay(8) 
 * sending large packets will eventually stall interrupts.
 *
 * 6. When receiving data (Bulk OUT) the UDC will not start NAKing until and
 * unless the receive FIFO is full. This means that there is no reliable way
 * to determine the end of one packet and the beginning of the next if there
 * is any undue latency in handling the interrupt for the first packet.
 *
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("sl@belcarra.com, tbr@belcarra.com");
MODULE_DESCRIPTION ("USB Device AU1x00  Bus Interface");
MODULE_LICENSE("GPL");

USBD_MODULE_INFO ("au1x00_bi 0.1-alpha");

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

#include <asm/au1000.h>
#include <asm/au1000_dma.h>
#include <asm/mipsregs.h>

#ifdef CONFIG_USBD_AU1X00_USE_ETH0
extern int get_ethernet_addr(char *ethernet_addr);
#endif

#undef CHECK_LATENCY
#undef RECORD_LATENCY

#include "au1x00.h"

#include "trace.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


static struct usb_device_instance *udc_device;	// required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb ep0_in_urb;
static struct urb ep0_out_urb;
static struct urb *pending_urb;

static unsigned char usb_address;

extern unsigned int udc_interrupts;

int au_halt_dma_expired;

#define MAX_INTR_LOOP_STATS   10
#if defined(MAX_INTR_LOOP_STATS)
static u32 interrupt_loop_stats[MAX_INTR_LOOP_STATS+1];
#endif

unsigned int udc_spurious;
unsigned int udc_spurious_last;

u32 usbd_inten;

#ifdef  RECORD_LATENCY
#define CP0_COUNTS 50
u32 cp0_counts[CP0_COUNTS];
u32 cp0_record;
#endif
u32 cp0_count;

#define MAX_EP0_PACKET_SIZE EP0_PACKETSIZE
#define MAX_EP2_PACKET_SIZE 64
#define MAX_EP3_PACKET_SIZE 64
#define MAX_EP4_PACKET_SIZE 64
#define MAX_EP5_PACKET_SIZE 64

#define CONTROL_EP 0x0
#define ISO_EP 0x1
#define BULK_EP 0x2
#define INTERRUPT_EP 0x3

#define DIRECTION_OUT 0
#define DIRECTION_IN  1<<3
#define DIRECTION_IGNORE 0


unsigned int au1x00_config_bulk[25] = {

        0x04,                                                                           // 0
        (CONTROL_EP << 4) | DIRECTION_IGNORE | (MAX_EP0_PACKET_SIZE & 0x380) >> 7,      // 1
        (MAX_EP0_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x00,

        0x24,                                                                           // 5
        (BULK_EP << 4) | DIRECTION_IN | (MAX_EP2_PACKET_SIZE & 0x380) >> 7,             // 6
        (MAX_EP2_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x02, 
                
        0x34,                                                                           // 10
        (BULK_EP << 4) | DIRECTION_IN | (MAX_EP3_PACKET_SIZE & 0x380) >> 7,             // 11
        (MAX_EP3_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x03, 

        0x44,                                                                           // 15
        (BULK_EP << 4) | DIRECTION_OUT | (MAX_EP4_PACKET_SIZE & 0x380) >> 7,            // 16
        (MAX_EP4_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x04,

        0x54,                                                                           // 20
        (BULK_EP << 4) | DIRECTION_OUT | (MAX_EP5_PACKET_SIZE & 0x380) >> 7,            // 21
        (MAX_EP5_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x05,
};

unsigned int au1x00_config_iso[25] = {

        0x04,                                                                           // 0
        (CONTROL_EP << 4) | DIRECTION_IGNORE | (MAX_EP0_PACKET_SIZE & 0x380) >> 7,      // 1
        (MAX_EP0_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x00,

        0x24,                                                                           // 5
        (ISO_EP << 4) | DIRECTION_IN | (MAX_EP2_PACKET_SIZE & 0x380) >> 7,             // 6
        (MAX_EP2_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x02, 
                
        0x34,                                                                           // 10
        (ISO_EP << 4) | DIRECTION_IN | (MAX_EP3_PACKET_SIZE & 0x380) >> 7,             // 11
        (MAX_EP3_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x03, 

        0x44,                                                                           // 15
        (BULK_EP << 4) | DIRECTION_OUT | (MAX_EP4_PACKET_SIZE & 0x380) >> 7,            // 16
        (MAX_EP4_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x04,

        0x54,                                                                           // 20
        (BULK_EP << 4) | DIRECTION_OUT | (MAX_EP5_PACKET_SIZE & 0x380) >> 7,            // 21
        (MAX_EP5_PACKET_SIZE & 0x7F) << 1, 
        0x00, 0x05,
};

/* ********************************************************************************************* */
/* IO
 */

typedef struct ep_regs {
        int rd;
        int wr;
        int cs;
        int rds;
        int wrs;
        int rx_id;
        int tx_id;
        char * rx_str;
        char * tx_str;
        int indma;    
        int outdma;
        int last;
} ep_regs_t;

struct ep_regs ep_regs[6] = {

        // XXX TX RX
        { rd: USBD_EP0RD, rds: USBD_EP0RDSTAT, /*rx_id: DMA_ID_USBDEV_EP0_RX,*/ cs: USBD_EP0CS, rx_str: "EP0 OUT RD", 
          wr: USBD_EP0WR, wrs: USBD_EP0WRSTAT, tx_id: DMA_ID_USBDEV_EP0_TX,                 tx_str: "EP0 IN WR",},  

        { rds: 0, wrs: 0, indma: -1, outdma: -1, },

        { wr: USBD_EP2WR, wrs: USBD_EP2WRSTAT, tx_id: DMA_ID_USBDEV_EP2_TX, cs: USBD_EP2CS, tx_str: "EP2 IN WR",}, 
        { wr: USBD_EP3WR, wrs: USBD_EP3WRSTAT, tx_id: DMA_ID_USBDEV_EP3_TX, cs: USBD_EP3CS, tx_str: "EP3 IN WR",}, 

        { rd: USBD_EP4RD, rds: USBD_EP4RDSTAT, rx_id: DMA_ID_USBDEV_EP4_RX, cs: USBD_EP4CS, rx_str: "EP4 OUT RD",},
        { rd: USBD_EP5RD, rds: USBD_EP5RDSTAT, rx_id: DMA_ID_USBDEV_EP5_RX, cs: USBD_EP5CS, rx_str: "EP5 OUT RD",},
};



static __inline__ int au_fifo_read(struct ep_regs *ep, unsigned char * cp, int bytes)
{
        u32 rd = ep->rd;
        switch (bytes) {
        case 8: *cp++ = au_readl(rd);
        case 7: *cp++ = au_readl(rd);
        case 6: *cp++ = au_readl(rd);
        case 5: *cp++ = au_readl(rd);
        case 4: *cp++ = au_readl(rd);
        case 3: *cp++ = au_readl(rd);
        case 2: *cp++ = au_readl(rd);
        case 1: *cp++ = au_readl(rd);
        }
        return bytes;
}

static __inline__ void au_fifo_write(int ep, unsigned char * cp, int bytes)
{
        u32 wr = ep_regs[ep].wr;
        switch (bytes) {
        case 8: au_writel(*cp++, wr);
        case 7: au_writel(*cp++, wr);
        case 6: au_writel(*cp++, wr);
        case 5: au_writel(*cp++, wr);
        case 4: au_writel(*cp++, wr);
        case 3: au_writel(*cp++, wr);
        case 2: au_writel(*cp++, wr);
        case 1: au_writel(*cp++, wr);
        }
        ep_regs[ep].last = bytes;
}

void __inline__ au_inten(u32 inten)
{
        //TRACE_MSG32("INTEN: %02x", inten);
        usbd_inten = inten;
        au_writel(usbd_inten, USBD_INTEN);
}

void __inline__ udc_epn_interrupt_enable(int epn)
{
        //TRACE_MSG("udc_epn_interrupt_enable");
        au_inten(usbd_inten | (1 << epn));
}

void __inline__ udc_epn_interrupt_disable(int epn)
{
        //TRACE_MSG("udc_epn_interrupt_disable");
        au_inten(usbd_inten & ~(1 << epn));
        au_writel((1 << epn) , USBD_INTSTAT);
        //au_url(USBD_INTSTAT);
}


static void __inline__ send_zlp(unsigned char epn)
{
        au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep_regs[epn].wrs);
        au_writel(0 << 1, ep_regs[epn].cs);
        au_writel(0, ep_regs[epn].wr);
}


/* DMA ***************************************************************************************** */

/*
 * These replace the equivalent routines in au1100_dma.h except that they
 * pass the actual struct dma_chan pointer instead of the channel number.
 * The bounds checking and table lookup to derive this information amounts
 * to a substantial increase in code size and latency which can simply be
 * avoided by using the structure address directly and/or doing the
 * equivalent i/o directly.
 */

#define AU_DMA_HALT_POLL 0x1000

static void __inline__ au_halt_dma(struct dma_chan *chan)
{
        int i;
        au_writel(DMA_GO, chan->io + DMA_MODE_CLEAR);

        for (i = 0; i < AU_DMA_HALT_POLL; i++) {
                if (au_readl(chan->io + DMA_MODE_READ) & DMA_HALT) {
                        return;
                }
        }
        au_halt_dma_expired++;
        TRACE_MSG32("HALT DMA POLL expired %x", au_readl(chan->io + DMA_MODE_READ));
}

static int __inline__ au_get_dma_residue(struct dma_chan *chan)
{
        int curBufCntReg;

        curBufCntReg = (au_readl(chan->io + DMA_MODE_READ) & DMA_AB) ? DMA_BUFFER1_COUNT : DMA_BUFFER0_COUNT;
        return au_readl(chan->io + curBufCntReg) & DMA_COUNT_MASK;
}

static void __inline__ au_start_dma(struct dma_chan *chan, __u8 *bp, int len)
{
        //TRACE_MSG("AU START DMA");
        if (au_readl(chan->io + DMA_MODE_READ) & DMA_AB) {
                au_writel(DMA_D1, chan->io + DMA_MODE_CLEAR);
                au_writel(len & DMA_COUNT_MASK, chan->io + DMA_BUFFER1_COUNT);
                au_writel(0, chan->io + DMA_BUFFER0_COUNT);
                au_writel(virt_to_phys(bp), chan->io + DMA_BUFFER1_START);
                au_writel(DMA_BE1, chan->io + DMA_MODE_SET);
        }
        else {
                au_writel(DMA_D0, chan->io + DMA_MODE_CLEAR);
                au_writel(len & DMA_COUNT_MASK, chan->io + DMA_BUFFER0_COUNT);
                au_writel(0, chan->io + DMA_BUFFER1_COUNT);
                au_writel(virt_to_phys(bp), chan->io + DMA_BUFFER0_START);
                au_writel(DMA_BE0, chan->io + DMA_MODE_SET);
        }
        au_writel(DMA_GO, chan->io + DMA_MODE_SET);
}

static void __inline__ au_start_dma_in(int indma, __u8 *bp, int len)
{
        struct dma_chan *chan = get_dma_chan(indma);
        au_start_dma(chan, bp, len);
}

static void __inline__ au_start_dma_out(struct dma_chan *chan, __u8 *bp)
{
        au_start_dma(chan, bp, 64);
}


/* ********************************************************************************************* */
/* Control (endpoint zero)
 */
static void au_start_out(struct usb_endpoint_instance *endpoint, int ep, int outdma);

/**
 * au_start_in_ep0 - start transmit
 * @ep:
 */
static void au_start_in_ep0 (struct usb_endpoint_instance *endpoint)
{
        struct urb *urb = endpoint->tx_urb;
        if ((urb->actual_length - endpoint->sent) > 0) {
                struct ep_regs *ep = &ep_regs[0];
                int last = ep->last = endpoint->last = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->wrs);
                au_writel(last << 1, ep->cs); // XXX
                //TRACE_MSG32("TX URB %d", last);
                //TRACE_SENT(urb->buffer + endpoint->sent);
                au_fifo_write(0, urb->buffer + endpoint->sent, last);
        } 
}


static void au_ep0_setup(struct usb_endpoint_instance *endpoint, u32 cs, int outdma, int fake)
{
        int i;

        if (endpoint->tx_urb) {

                //TRACE_MSG32("TX URB %d", 0);
                endpoint->sent = 0;
                endpoint->last = 0;
                endpoint->tx_urb = NULL;
        }

        TRACE_MSG32( "SETUP UNQUEUING %d", ep0_out_urb.actual_length);
        
        /*
         * There may be multiple setup packets queued, but only the last may be one
         * that will return data.
         */
        for (i = 0; i <= (ep0_out_urb.actual_length - 8); i += 8) {

                memcpy(&ep0_in_urb.device_request, ep0_out_urb.buffer + i, 8);
                ep0_in_urb.actual_length = 0;

                TRACE_SETUP((struct usb_device_request *)(ep0_out_urb.buffer + i));

                /*
                 * we must react differently for various setup commands
                 */
                if (!(ep0_in_urb.device_request.bmRequestType & ~USB_REQ_DIRECTION_MASK)) {

                                // we need to simply ignore any of these
                        switch (ep0_in_urb.device_request.bRequest) {
                        case USB_REQ_GET_STATUS:
                        case USB_REQ_CLEAR_FEATURE:
                        case USB_REQ_SET_FEATURE:
                        case USB_REQ_SET_DESCRIPTOR:
                        case USB_REQ_GET_INTERFACE:
                        case USB_REQ_SYNCH_FRAME:
                        case USB_REQ_GET_CONFIGURATION: // XXX
                                TRACE_MSG32("SETUP SKIPPED %d", ep0_in_urb.device_request.bRequest);
                                continue;

                                // Fake a bus reset and then process normally
                        case USB_REQ_SET_ADDRESS:

                                if(udc_device->device_state != STATE_DEFAULT) {
                                        TRACE_MSG32("ADDRESSED - FAKE RESET %d", ep0_in_urb.device_request.bRequest);
                                        usbd_device_event (udc_device, DEVICE_RESET, 0);// XXX should be done from device event
                                }

                                // These need to be processed
                        case USB_REQ_GET_DESCRIPTOR:
                        case USB_REQ_SET_CONFIGURATION:
                        case USB_REQ_SET_INTERFACE:
                                TRACE_MSG32("GET or SET %d", ep0_in_urb.device_request.bRequest);
                        default:
                                /* FALL THROUGH */

                        }
                }

                // check data direction
                if (((ep0_in_urb.device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE)
                                && le16_to_cpu (ep0_in_urb.device_request.wLength)) 
                {

                        TRACE_MSG32("ep0 Class H2D request %04x", 
                                        le16_to_cpu(ep0_in_urb.device_request.wLength));

                        memcpy(&ep0_out_urb.device_request, &ep0_in_urb.device_request, 8);
                        endpoint->rcv_transferSize = le16_to_cpu(ep0_in_urb.device_request.wLength);
                        endpoint->state = DATA_STATE_RECV;
                        ep0_out_urb.actual_length = 0;
                        break;

                }                               

                // process
                if (usbd_recv_setup(&ep0_in_urb)) {
                        TRACE_MSG32("ep0 STALL %d", cs);
                        if (!fake) {
                                au_writel(USBDEV_CS_STALL, USBD_EP0CS);
                        }
                        break;
                }

                // check data direction
                if ((ep0_in_urb.device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) {
                        if ((ep0_in_urb.device_request.bmRequestType & ~USB_REQ_DIRECTION_MASK)) {
                                TRACE_MSG32("ep0 Class or Vendor, send ZLP %d", cs);
                                send_zlp(0);
                                break;
                        }
                }                               
                else {
                        //TRACE_MSG32("ep0 DATA %d", cs);
                        // verify that we have non-zero request length
                        if (!le16_to_cpu (ep0_in_urb.device_request.wLength)) {
                                TRACE_MSG32("ep0 DATA - wLength zero - stall %d", cs);
                                udc_stall_ep (0);
                        }       

                        // verify that we have non-zero length response
                        else if (!ep0_in_urb.actual_length) {
                                TRACE_MSG32("ep0 DATA - descriptor null - stall %d", cs);
                                udc_stall_ep (0);
                        }

                        // start sending
                        else {
                                TRACE_MSG32("ep0 DATA %d", ep0_in_urb.actual_length);
                                endpoint->tx_urb = &ep0_in_urb;
                                endpoint->sent = 0;
                                endpoint->last = 0;
                                endpoint->state = DATA_STATE_XMIT;
                                au_start_in_ep0 (endpoint);
                        }
                        break;
                }
        }
        //TRACE_MSG("setup done");
        memset(ep0_out_urb.buffer, 0, ep0_out_urb.actual_length);
        ep0_out_urb.actual_length = 0;
}


/*
* Check for spooled setup packets, and process if found.
*/ 
static void __inline__ au_check_ep0_spool(void)
{
        if (ep0_out_urb.actual_length) {
                struct usb_endpoint_instance *endpoint = ep_endpoints[0];
                TRACE_MSG32("TRY FAKE EP0 processing state: %0d", endpoint->state);
                if (endpoint->state != DATA_STATE_RECV) {
                        //TRACE_MSG("FAKE EP0 processing");
                        au_ep0_setup(endpoint, 0, -1, 1);
                }
        }
}


/*
 * au_in_ep0 - called to service an endpoint zero IN interrupt, data sent
 */
static void au_in_ep0(unsigned int epn)
{
        u32 cs;
        u32 wrs;
        struct ep_regs *ep = &ep_regs[epn];
        struct usb_endpoint_instance *endpoint;

        static int udc_ep0_in_naks = 0;
        struct urb *tx_urb;

        // check for underflow or overflow
        if ((wrs = au_readl(ep->wrs))) {
                if (wrs & USBDEV_FSTAT_UF) {
                        //TRACE_MSG32("IN UF %d", wrs);
                }
                if (wrs & USBDEV_FSTAT_OF) {
                        //TRACE_MSG32("IN OF %d", wrs);
                }
        }

        // clear stall if present
        if ((cs = au_readl(ep->cs)) & USBDEV_CS_STALL) {
                //TRACE_MSG32("CLEAR STALL %d", epn);
                cs &= ~USBDEV_CS_STALL;
                au_writel(cs, ep->cs);
                return;
        }
        TRACE_MSG16("AU_IN_EP0: %02x %02x", cs, wrs);

        if (!(endpoint = ep_endpoints[epn])) {
                return;
        }

        // check if this is a NAK
        if (cs & USBDEV_CS_NAK) {
                //TRACE_MSG32("IN NAK %04x", cs);
                /*
                 * See if there are any spooled setup commands to process. This is done here because
                 * we cannot reliably detect the difference between auto and non-auto when receiving
                 * them. If we receive a NAK it means that the other end is expecting some data AND
                 * we have time to process it. See also au_out_epn().
                 */
                if (ep0_out_urb.actual_length) {
                        au_ep0_setup(endpoint, cs, -1, 0);
                        return;
                }
                if (udc_ep0_in_naks++ < 4) {
                        return;
                }
                //TRACE_MSG("TOO MANY NAKS!!!");
        }
        udc_ep0_in_naks = 0;

        if ((tx_urb = endpoint->tx_urb)) {
                TRACE_MSG8("IN EP0 last: %d sent: %d actual: %d wLength: %d", endpoint->last, endpoint->sent, 
                                tx_urb->actual_length, le16_to_cpu(tx_urb->device_request.wLength));
                if (
                                (tx_urb->actual_length < le16_to_cpu(tx_urb->device_request.wLength)) &&
                                ((endpoint->last + endpoint->sent) == tx_urb->actual_length) &&
                                endpoint->last == endpoint->tx_packetSize 
                   )
                {
                        TRACE_MSG32("ep0 send ZLP %d", cs);
                        send_zlp(0);
                        endpoint->tx_urb = NULL;
                        endpoint->state = WAIT_FOR_SETUP;
                }
        }

        // process urb to advance buffer pointers
        usbd_tx_complete_irq(endpoint, 0);

        // continue sending if more data to be sent
        if ((tx_urb = endpoint->tx_urb)) {

                if (tx_urb->actual_length > endpoint->sent) {
                        if ((tx_urb->actual_length - endpoint->sent) < endpoint->tx_packetSize) {
                                TRACE_MSG32("starting short packet %d", tx_urb->actual_length - endpoint->sent);
                        }
                        else {
                                TRACE_MSG32("LEFT TO SEND %d", tx_urb->actual_length - endpoint->sent);
                        }
                        au_start_in_ep0(endpoint);
                }
        }
        else {
                endpoint->state = WAIT_FOR_SETUP;
        }
}


/*
 * au_out_ep0 - called to service an endpoint zero OUT interrupt, data received
 */
static void au_out_ep0(void)
{
        struct usb_endpoint_instance *endpoint;
        struct urb *urb;
        struct ep_regs *ep = &ep_regs[0];
        u32 cs;
        u32 bytes;

        if (!(endpoint = ep_endpoints[0]) || !(urb = endpoint->rcv_urb)) {
                //TRACE_MSG("SETUP endpoint or rcv urb NULL");
                return;
        }

        // check if host aborted transfer and flush the write fifo
        if (endpoint->tx_urb) {
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->wrs);
                endpoint->tx_urb->actual_length = 0;
                endpoint->tx_urb = NULL;
                endpoint->state = WAIT_FOR_SETUP;
        }

        cs = au_readl(ep->cs);
        bytes = au_readl(ep->rds) & USBDEV_FSTAT_FCNT_MASK;

        //TRACE_MSG32("AU_OUT_EP0: %02x", cs);

        if (au_fifo_read(ep, urb->buffer + urb->actual_length, bytes)) {
                 
                struct usb_device_request *device_request = (struct usb_device_request *) urb->buffer + urb->actual_length;

                TRACE_MSG32("BYTES: %d", bytes);
                TRACE_RECV((unsigned char *) (urb->buffer + urb->actual_length));

                urb->actual_length += bytes;

                /*
                 * The AU1x00 auto acks many setup commands. There are two
                 * problems with this behaviour. 
                 *
                 * First if we cannot reliably process the ack'd commands in
                 * time to get to the next command leading to a breakdown in
                 * the setup command processing.
                 *
                 * Secondly it does not reliably tell us when it is not
                 * doing so. In theory if the NAK bit is set it means that
                 * the command was acked. But testing shows that it is
                 * possible to receive (for example) a GET DESCRIPTOR with
                 * NAK set. 
                 *
                 * To get around both of these problems we simply spool all
                 * setup commands and only process them when another
                 * interrupt arrives showing that we have NAK'd an IN token
                 * asking for some data.
                 *
                 */

                if (endpoint->state == DATA_STATE_RECV) {

                        TRACE_MSG16("data state recv %d %d", endpoint->rcv_transferSize, urb->actual_length);
                        if (endpoint->rcv_transferSize == urb->actual_length) {
                                //TRACE_MSG32("ep0 out send ZLP %d", cs);
                                send_zlp(0);
                                endpoint->state = WAIT_FOR_SETUP;
#if 1
                                TRACE_RECV(urb->buffer);
                                TRACE_RECV(urb->buffer+8);
                                TRACE_RECV(urb->buffer+16);
                                TRACE_MSG32("Calling recv_setup urb: %p", urb);
                                TRACE_MSG32("Calling recv_setup urb: %d", urb->actual_length);
#endif

                                if (usbd_recv_setup(urb)) {
                                        //TRACE_MSG("setup failed");
                                }
                                else {
                                        //TRACE_MSG("setup ok");
                                }
                                urb->actual_length = 0;
                                memset(urb->buffer, 0, urb->buffer_length);
                        }
                }
                else {

                        TRACE_MSG32("EP0 OUT bytes: %d", bytes);
                        if (bytes == 8) {


                                TRACE_MSG16("Request Type: %x %x", 
                                                device_request->bmRequestType & USB_REQ_TYPE_MASK,
                                                USB_REQ_TYPE_STANDARD != (device_request->bmRequestType & USB_REQ_TYPE_MASK));

                                if ( 
                                        (USB_REQ_TYPE_STANDARD != (device_request->bmRequestType & USB_REQ_TYPE_MASK))
                                        || !(cs & USBDEV_CS_SU)
                                        || !(cs & USBDEV_CS_NAK)
                                        )
                                {
                                        TRACE_MSG32("SETUP EP0 %d", bytes);
                                        au_ep0_setup(endpoint, cs, -1, 0);
                                }
                                else {
                                        TRACE_MSG32("SETUP AUTO %d", cs);
                                        //urb->actual_length -= 8;
                                }

                        }
                        else {

                                TRACE_MSG32("BAD READ %d", bytes);
                                if (cs & USBDEV_CS_NAK) {
                                        //TRACE_MSG32("NAK %d", cs);
                                }
                                if (cs & USBDEV_CS_ACK) {
                                        //TRACE_MSG32("ACK %d", cs);
                                }
                                if (cs & USBDEV_CS_STALL) {
                                        //TRACE_MSG32("AU_OUT_EP0 STALL %d", cs);
                                }
                        }

                }
        }
        else if (cs & USBDEV_CS_SU) {
                //TRACE_MSG32("SETUP SET %02x", cs);
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->wrs);
        }
        else {
                //TRACE_MSG32("SETUP NOT SET %02x", cs);
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->wrs);
        }
}

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */

/**
 * au_start_in - start transmit
 * @ep:
 */
static void __inline__ au_start_in (unsigned int epn, struct usb_endpoint_instance *endpoint)
{
        struct urb *urb = endpoint->tx_urb;
        int indma;
        struct ep_regs *ep = &ep_regs[epn];

        //TRACE_MSG32("au start in %d", ep);

        if ((urb->actual_length - endpoint->sent) > 0) {

                unsigned char *bp = urb->buffer + endpoint->sent;
                int last;

                /*
                 * The au1x00 will start to send when the first byte is loaded into the FIFO, either
                 * by DMA or PIO. The packetsize must be set first.
                 */

                indma = ep->indma;

                // disable interrupts, they will be re-enabled by tx_dma interrupt
                udc_epn_interrupt_disable(epn);

                last = ep->last = endpoint->last = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);

                // invalidate cache
                dma_cache_wback_inv((unsigned long) bp, last);

                //au_writel(endpoint->last << 1, ep->cs);
                au_writel(last << 1, ep->cs);

                au_start_dma_in(indma, bp, last);
        } 
}


static __inline__ void au_in_epn(unsigned int epn, u32 cs, u32 wrs)
{
        int rc = 0;
        struct ep_regs *ep = &ep_regs[epn];
        struct usb_endpoint_instance *endpoint;

        //TRACE_MSG16("AU IN EPN - cs: %x wrs: %x", cs, wrs);

        if (epn && (ep->last > 8)) {
                //TRACE_MSG16("AU IN EPN - DMA ACTIVE epn %d last %d", epn, ep->last);
                udc_epn_interrupt_disable(epn);
                return;
        }
        
        ep->last = 0;

        // check for underflow or overflow
        if (wrs) {
                rc = 1;
                if (wrs & USBDEV_FSTAT_UF) {
                        //TRACE_MSG16("AU IN EPN - UF epn %d wrs: %x", epn, wrs);
                        // set rc to indicate an error
                }
                if (wrs & USBDEV_FSTAT_OF) {
                        //TRACE_MSG16("AU IN EPN - OF epn %d wrs: %x", epn, wrs);
                }
                // flush the fifo 
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->wrs); // XXX
        }

        if (cs & USBDEV_CS_NAK) {
                //TRACE_MSG16("AU IN EPN - NAK cs %x wrs: %x", cs, wrs);
                if (ep->last && ((wrs&0x1f) == ep->last)) {
                        //TRACE_MSG("AU IN EPN - ignoring NAK");
                        return;
                }
                rc = 1;
        }
        else {
                //TRACE_MSG16("AU IN EPN - ACK cs %x wrs: %x", cs, wrs);
        }


        // clear stall if present
        if (cs & USBDEV_CS_STALL) {
                //TRACE_MSG32("AU IN EPN - CLEAR STALL %d", epn);
                cs &= ~USBDEV_CS_STALL;
                au_writel(cs, ep->cs);
                return;
        }

        // make sure we have a data structure
        if ((endpoint = ep_endpoints[epn])) {
                struct urb *tx_urb;

                //TRACE_MSG8("AU IN EPN epn: %d rc: %d last: %d sent: %d", epn, rc, endpoint->last, endpoint->sent);

                // process urb to advance buffer pointers (or not if rc set)
                usbd_tx_complete_irq(endpoint, rc);

                // continue sending if more data to be sent
                if ((tx_urb = endpoint->tx_urb) && (tx_urb->actual_length > endpoint->sent)) {

                        au_start_in(endpoint->endpoint_address&0xf, endpoint);

                        // XXX magic delay - without this large packets will eventually stall the transmit
                        // XXX and all traffic in both directions will stop. 
                        udelay(8);

                        //TRACE_MSG32("AU IN EPN - LEFT TO SEND %d", tx_urb->actual_length - endpoint->sent);
                        return;
                }
        }

        /*
         * The AU1x00 will generate an interrupt for EVERY IN packet. If we do
         * not have any data we need to disable the interrupt for the endpoint
         * to eliminate the overhead for processing them. The interrupt must be
         * re-enabled when DMA is finished and we actually want to know that
         * the endpoint has finished.
         *
         * Leaving the interrupt enabled also interfers with the DMA process. It is 
         * not apparante why.
         *
         */
        udc_epn_interrupt_disable(epn);
}

/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

static void __inline__ au_dump_urb(char *msg, int bytes, struct urb *urb)
{
        TRACE_MSG16(msg, bytes, urb->actual_length);
        //TRACE_MSG32("AU DUMP URB - bytes: %d", bytes);
        //for (i = 0; i < bytes; i += 8) {
        //        TRACE_RECVN(urb->buffer + urb->actual_length + i, MIN(8, bytes - i));
        //}
}

static void __inline__ au_out_epn(unsigned int epn)
{
        int bytes = 0;
        struct ep_regs *ep = &ep_regs[epn];
        int outdma = ep->outdma;
        struct dma_chan *chan = get_dma_chan(outdma);
        struct usb_endpoint_instance *endpoint = ep_endpoints[epn];
        struct urb *urb;
        struct urb *completed_urb = NULL;
        u32 cs;
        u32 rds;
        u32 nrds;

        au_halt_dma(chan);

        cs = au_readl(USBD_EP4CS);
        rds = au_readl(USBD_EP4RDSTAT);

#if 0
        if (cs & USBDEV_CS_NAK) {
                TRACE_MSG16("NAK %04x %04x", cs & USBDEV_CS_NAK, USBDEV_CS_NAK);
        }
        if (cs & USBDEV_CS_ACK) {
                TRACE_MSG16("ACK %04x %04x", cs & USBDEV_CS_ACK, USBDEV_CS_ACK);
        }
        if (cs & USBDEV_CS_BUSY) {
                TRACE_MSG16("BUSY %04x %04x", cs & USBDEV_CS_BUSY, USBDEV_CS_BUSY);
        }
#endif
        if (!endpoint) {
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->rds);
                return;
        }

        bytes = endpoint->rcv_packetSize - au_get_dma_residue(chan);

        if (!(urb = endpoint->rcv_urb)) {
                endpoint->dropped_errors++;
                //TRACE_MSG16("AU OUT EPN - rcv_urb was NULL bytes: %d rds: %d", bytes, rds);
                usbd_rcv_complete_irq(endpoint, bytes, 1);
                endpoint->rcv_error = 1;
                THROW(restart);
        }

        /*
         * The AU1X00 will continue to receive data as long as there is
         * room in the FIFO. We cannot tell when we are at the end of a
         * packet and/or have the start of a new one.
         *
         * There are only two scenarios that are guaranteed (almost) to be
         * correct:
         *
         *      64 bytes of data from DMA, empty fifo, continue Bulk OUT
         *      < 60 bytes of data and < 4 bytes in fifo, end Bulk OUT.
         *
         * There may be a third scenario that is ok:
         *
         *      0 bytes dma, 0 bytes in fifo, NAK
         *
         * Everything else is an error. In all cases we assume that it is
         * safer to drop data than to accept it in error. This allows CRC
         * or size protected encapsulations to notice bulk transfers
         * received with errors.
         *
         * In general none of the policies or strategies are able to cope
         * with all errors without missing errors and dropping good data.
         * The intent is to minimize the amount of potentially bad data
         * getting to the function driver while minimizing the amount of
         * good data that is dropped.
         */

        /*
         * Start with generic error tests, OF, UF or NAK indicate an error
         * we cannot recover from, start flushing until end of current bulk
         * transfer (wait for a short packet)
         *
         */
        if (rds & (USBDEV_FSTAT_OF | USBDEV_FSTAT_UF)) {
                TRACE_MSG32("OUT OF or UF %x", rds);
                THROW(start_flushing);
        }

        rds = rds & USBDEV_FSTAT_FCNT_MASK;
        nrds = au_readl(USBD_EP4RDSTAT);

        if (64 == bytes) {
                /*
                 * full size packet received, check that we are not flushing and
                 * that the FIFO does not have any data. If there is data in the FIFO we
                 * may not be able to restart DMA in time, so start flushing 
                 *
                 * XXX we may be able to continue if <= 4 bytes in FIFO.
                 */


                if (endpoint->rcv_error) {
                        TRACE_MSG8("FULL PACKET bytes: %d rds: %d nrds: %d cp0: %d CONTINUE FLUSHING", 
                                        bytes, rds, nrds, cp0_count);
                        THROW(start_flushing);
                }

                if ((nrds > 6)  && (nrds < 8) ) {
                        TRACE_MSG8("FIFO not empty bytes: %d rds: %d nrds: %d cp0: %d START FLUSHING", 
                                        bytes, rds, nrds, cp0_count);
                        THROW(start_flushing);
                }

                if (!urb->actual_length) {
                        //TRACE_MSG8("PACKET ok bytes: %d rds: %d nrds: %d cp0: %d ACCEPTING 64 bytes", 
                        //                bytes, rds, nrds, cp0_count);
                }

                /*
                 * optimized version of usbd_rcv_complete()
                 */
                //usbd_rcv_complete_irq(endpoint, bytes, 0);
                urb->actual_length += 64;

                //TRACE_MSG32("RECEIVED len: %d", urb->actual_length);
                //TRACE_MSG32("RECEIVED xfer: %d", endpoint->rcv_transferSize);

                if (urb->actual_length >= endpoint->rcv_transferSize) {
                        int i;
                        completed_urb = urb;
                        TRACE_MSG32("RECEIVED AAA URB: %d", urb->actual_length);

                        for (i = 0; i < urb->actual_length; i += 8) {
                                TRACE_RECV(urb->buffer + i);
                        }

                        usbd_rcv_next_irq(endpoint);
                }
                if (cs & USBDEV_CS_NAK) {

                        if (nrds && (nrds < 8) ) {
                                TRACE_MSG8("NAK bytes: %d rds: %d nrds: %d cp0: %d START FLUSHING", 
                                                bytes, rds, nrds, cp0_count);
                                THROW(start_flushing);
                        }

                        THROW(flush_fifo_restart);
                }
        }

        else if ((cs & USBDEV_CS_NAK) && (!nrds || (nrds == 8)) ) {

                /*
                 * a nak'd packet may be ok to ignore IFF the FIFO is
                 * empty(?) or completely full.
                 */

                if (endpoint->rcv_error) {
                        TRACE_MSG8("NAK bytes: %d rds: %d nrds: %d cp0: %d CONTINUE FLUSHING", 
                                        bytes, rds, nrds, cp0_count);
                        THROW(start_flushing);
                }
                THROW(flush_fifo_restart);
        }

        else {
                /* 
                 * short packet by DMA, additional data for this packet in FIFO, 
                 * more than 3 bytes is probably an error.
                 */

                if ((cs & USBDEV_CS_NAK) && nrds && (nrds < 8) ) {
                        TRACE_MSG8("NAK bytes: %d rds: %d nrds: %d cp0: %d START FLUSHING", 
                                        bytes, rds, nrds, cp0_count);
                        THROW(start_flushing);
                }
                if (nrds > 4) {
                        TRACE_MSG8("SHORT PACKET by DMA full FIFO bytes: %d rds: %d nrds: %d cp0: %d START FLUSHING", 
                                        bytes, rds, nrds, cp0_count);
                        THROW(start_flushing);
                }


                //TRACE_MSG8("SHORT bytes: %d rds: %d nrds: %d cp0: %d reading fifo", bytes, rds, nrds, cp0_count);

                bytes += au_fifo_read(ep, urb->buffer + urb->actual_length + bytes, nrds);
                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->rds);

                //au_dump_urb("incomplete packet by DMA, filled from FIFO %d %d", bytes, urb);

                if (!endpoint->rcv_error) {
                        int i;
                        urb->actual_length += bytes;
                        completed_urb = urb;
                        TRACE_MSG32("RECEIVED BBB URB: %d", urb->actual_length);

                        for (i = 0; i < urb->actual_length; i += 8) {
                                TRACE_RECV(urb->buffer + i);
                        }

                        usbd_rcv_next_irq(endpoint);
                }
                else {
                        TRACE_MSG("COMPLETED FLUSHING URB");
                        endpoint->rcv_error = 0;
                }
        }

        /*
         * default behaviour - restart dma
         */
        THROW(restart);

        CATCH(restart) {

                CATCH(start_flushing) {
                        endpoint->rcv_error = 1;
                        endpoint->rcv_urb->actual_length = 0;
                        endpoint->rcv_urb->previous_errors++;
                        au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->rds);
                        bytes = 0;
                }

                CATCH(flush_fifo_restart) {
                        au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep->rds);
                }


                if (endpoint->rcv_urb) {
                        au_start_dma_out(chan, endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length);
                }
                else {
                        TRACE_MSG("AU OUT EPN - NO BUFFER NOT RESTARTED");
                }
        }

        if (completed_urb) {
                dma_cache_inv((unsigned long) urb->buffer, urb->actual_length);
                usbd_rcv_fast_complete_irq(endpoint, urb);
        }

        /*
         * XXX Hack alert - check for spooled setup packets, process them now, better late 
         * then never. They may have a set configuration which if not processed will 
         * prevent data transfer.
         */ 
        au_check_ep0_spool();
}

/* ********************************************************************************************* */
/* Interrupt Handler
 */

/**
 * udc_tx_dma_done - TX DMA interrupt handler
 *
 */
static void udc_tx_dma_done(int irq, void *dev_id, struct pt_regs *regs)
{
        int epn = (int) dev_id;
        struct usb_endpoint_instance *endpoint = ep_endpoints[epn];
        struct ep_regs *ep = &ep_regs[epn];
        u32 mode;
        int residue;

        int indma = irq - 6;
        struct dma_chan *chan = get_dma_chan(indma);

        udc_interrupts++;

        //TRACE_MSG("TX_DMA done");

        mode = au_readl(chan->io + DMA_MODE_READ);

        if (mode & (DMA_BE0 | DMA_BE1)) {
                //TRACE_MSG16("TX DMA - STILL ACTIVE epn %d active %x", epn, active);
                return;
        }


        if (mode & DMA_D0) {
                au_writel(DMA_D0, chan->io + DMA_MODE_CLEAR);
        }

        if (mode & DMA_D1) {
                au_writel(DMA_D1, chan->io + DMA_MODE_CLEAR);
        }

        if (epn) {

                au_halt_dma(chan);

                residue = au_get_dma_residue(chan);

                //TRACE_MSG16("TX DMA IRQ - epn %d residue %d", epn, residue);

                usbd_tx_complete_irq (endpoint, residue?1:0);

                ep->last = 0;
                udc_epn_interrupt_enable(endpoint->endpoint_address&0xf);
        }
}

/**
 * udc_int_req - usb interrupt handler
 *
 */
static void udc_int_req (int irq, void *dev_id, struct pt_regs *regs)
{
        u32 intstat;
        u32 cs2;
        u32 wrs2;
#if defined(MAX_INTR_LOOP_STATS)
        u32 loopcount = 0;
#endif


#ifdef  RECORD_LATENCY
        cp0_count = (read_32bit_cp0_register(CP0_COUNT) - cp0_record) >> 9;
        if (cp0_count < CP0_COUNTS) {
                cp0_counts[cp0_count]++;
        }

#endif
        TRACE_MSG32("INT REQ: %d", cp0_count);
#ifdef  CHECK_LATENCY
        u32 cp0_count = read_32bit_cp0_register(CP0_COUNT);
#endif

        // cache control and status register values for bulk io endpoints
        cs2 = au_readl(USBD_EP2CS);
        wrs2 = au_readl(USBD_EP2WRSTAT);

        udc_interrupts++;

        // read and reset interrupt status register
        while (( intstat = au_readl(USBD_INTSTAT))) {

                //TRACE_MSG32("INTSTAT: %02x", intstat);

                // clear the interrupt(s)
                au_writel(intstat, USBD_INTSTAT);

                /* even though we disable the bulk-in interrupt (endpoint 2) prior to enabling
                 * DMA we always see one additional interrupt that is a NAK on that endpoint.
                 */
                if (!(intstat & usbd_inten)) {
                        //TRACE_MSG16("spurious %04x %04x", intstat, usbd_inten);
                        udc_spurious++;
                        continue;
                }

                /*
                 * handle bulk in and bulk out 
                 */
                if (intstat & (1 << 4)) {
                        au_out_epn(4);
                }
                if (intstat & (1 << 2)) {
                        au_in_epn(2, cs2, wrs2);
                        cs2 = au_readl(USBD_EP2CS);
                        wrs2 = au_readl(USBD_EP2WRSTAT);
                }

                /*
                 * handle control and interrupt
                 */
                if (intstat & ( ((1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | USBDEV_INT_SOF))) {

                        if (intstat & (1 << 0)) {
                                //TRACE_MSG("OUT0");
                                au_out_ep0();
                        }

                        if (intstat & (1 << 1)) {
                                //TRACE_MSG("IN0");
                                au_in_ep0(0);
                        }

                        if (intstat & (1 << 3)) {
                                //TRACE_MSG("IN3");
                                au_in_epn(3, au_readl(USBD_EP3CS), au_readl(USBD_EP3WRSTAT));
                        }

                        //if (intstat & (1 << 5)) {
                        //        //TRACE_MSG("OUT5");
                        //        au_out_epn(5);
                        //}

                        if (intstat & USBDEV_INT_SOF) {
                                if (USBD_SUSPENDED == udc_device->status) {
                                        TRACE_MSG("SUS - ACTIVITY");
                                        usbd_device_event_irq (udc_device, DEVICE_BUS_ACTIVITY, 0);
                                }
                                //au_check_ep0_spool();
                        }
                }

                /*
                 * Gather stats on how many times this loop is performed.
                 */
#if defined(MAX_INTR_LOOP_STATS)
                loopcount += 1;
#endif
                // cache control and status register values for bulk io endpoints
        }

#if defined(MAX_INTR_LOOP_STATS)
        interrupt_loop_stats[MIN(loopcount, MAX_INTR_LOOP_STATS)]++;
#endif
#if defined(CHECK_LATENCY)
        TRACE_MSG32("USB IRQ - %d", read_32bit_cp0_register(CP0_COUNT) - cp0_count);
#endif
#if defined(RECORD_LATENCY)
        cp0_record = read_32bit_cp0_register(CP0_COUNT);
#endif
}


static void udc_int_sus (int irq, void *dev_id, struct pt_regs *regs)
{
        udc_interrupts++;

        TRACE_MSG32("SUS %d", udc_interrupts);
        au_readl(USBD_INTSTAT);
        au_readl(ep_regs[0].cs);
        au_readl(ep_regs[0].wrs);

        if (udc_device) {

                switch(udc_device->status) {
                case USBD_OPENING:
                case USBD_OK:
                        TRACE_MSG("SUS - INACTIVE");
                        usbd_device_event_irq (udc_device, DEVICE_BUS_INACTIVE, 0);
                        break;
                default:
                        break;
                }
        }
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
        unsigned long flags;
        struct ep_regs *ep = &ep_regs[2]; // XXX

        local_irq_save (flags);

        udc_interrupts++;

        //TRACE_MSG32("START IN IRQ len: %d", urb->actual_length);
        if (endpoint && (ep->last == 0)) {

                /*
                 * Attempting to start the data transfer here seems to cause
                 * problems with DMA. So we simply enable the endpoint interrupt
                 * and wait for the first NAK interrupt. Then start the normal
                 * transmit process. This typically add's ~5us latency.
                 */
                udc_epn_interrupt_enable(endpoint->endpoint_address&0xf);
        }
        local_irq_restore (flags);
}


/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
        int i;
        unsigned int *ip;

        // disable interrupts
        au_inten(0);

        // enable controller
        au_writel(0x0002, USBD_ENABLE);
        udelay(100);
        au_writel(0x0003, USBD_ENABLE);
        udelay(100);


        // feed the config into the UDC
        ip = usbd_bi_iso() ? au1x00_config_iso : au1x00_config_bulk;

        for (i = 0; i < 25; i++ ) {
                au_writel(*ip, USBD_CONFIG);
                ip++;
        }

        for (i = 0; i < 6; i++) {

                if (ep_regs[i].cs) {
                        //au_readl(ep_regs[i].cs);

                        au_writel(0x40<<1, ep_regs[i].cs);

                        if (ep_regs[i].wrs) {
                                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep_regs[i].wrs);
                        }
                        if (ep_regs[i].rds) {
                                au_writel(USBDEV_FSTAT_FLUSH | USBDEV_FSTAT_UF | USBDEV_FSTAT_OF, ep_regs[i].rds);
                        }
                }
        }
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
                // reset
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
	// address cannot be setup until ack received
	usb_address = address;
}

/**
 * udc_serial_init - set a serial number if available
 */
int __init udc_serial_init (struct usb_bus_instance *bus)
{
#ifdef CONFIG_USBD_AU1X00_USE_ETH0
        char serial_number[20];
        get_ethernet_addr(bus->dev_addr);
        //printk(KERN_DEBUG"%s: dev_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__,
        //                bus->dev_addr[0], bus->dev_addr[1], bus->dev_addr[2],
        //                bus->dev_addr[3], bus->dev_addr[4], bus->dev_addr[5]);

        //snprintf(serial_number, sizeof(serial_number)-1, "%02x:%02x:%02x:%02x:%02x:%02x",
        //                bus->dev_addr[0], bus->dev_addr[1], bus->dev_addr[2],
        //                bus->dev_addr[3], bus->dev_addr[4], bus->dev_addr[5]
        //                );
        snprintf(serial_number, sizeof(serial_number)-1, "%02x%02x%02x%02x%02x%02x",
                        bus->dev_addr[0], bus->dev_addr[1], bus->dev_addr[2],
                        bus->dev_addr[3], bus->dev_addr[4], bus->dev_addr[5]
                        );
        memset(bus->dev_addr, 0, 6);
        serial_number[sizeof(serial_number)-1] = '\0';
        bus->serial_number_str = lstrdup(serial_number);
#endif
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
	return (((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) || (packetsize > 64)) ?  0 : (logical_endpoint & 0xf);
}


/**
 * udc_set_ep - setup endpoint 
 * @ep:
 * @endpoint:
 *
 * Associate a physical endpoint with endpoint_instance
 */
void udc_setup_ep (struct usb_device_instance *device, unsigned int epn,
		   struct usb_endpoint_instance *endpoint)
{
	if (epn < UDC_MAX_ENDPOINTS) {

		ep_endpoints[epn] = endpoint;

                // ep0
                if (epn == 0) {
#ifdef CONFIG_USBD_AU1X00_USE_ETH0
                        //get_ethernet_addr(device->dev_addr);
#endif
                        // ep0 urb
                        ep0_out_urb.device = device;
                        usbd_alloc_urb_data (&ep0_out_urb, 24*8);
                        memset(ep0_out_urb.buffer, 0, ep0_out_urb.buffer_length);
                        ep0_out_urb.actual_length = 0;

                        ep0_in_urb.device = device;
                        usbd_alloc_urb_data (&ep0_in_urb, 512);

                        endpoint->rcv_urb = &ep0_out_urb;
                        //if ((outdma = ep_regs[0].outdma) != -1) {
                        //        au_start_out(ep_endpoints[0], 0, outdma);
                        //}
                        endpoint->state = WAIT_FOR_SETUP;
                }
                // IN
                else if (endpoint->endpoint_address & 0x80) {
                }
                // OUT
                else if (endpoint->endpoint_address) {

                        struct ep_regs *ep = &ep_regs[epn];
                        int outdma = ep->outdma;
                        struct dma_chan *chan = get_dma_chan(outdma);

                        usbd_fill_rcv (device, endpoint, 20);
                        endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
                        if (endpoint->rcv_urb) {
                                au_start_dma_out(chan, endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length);
                        }
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
                        if (ep0_in_urb.buffer) {
                                lkfree (ep0_in_urb.buffer);
                        }
                        if (ep0_out_urb.buffer) {
                                lkfree (ep0_out_urb.buffer);
                        }
                        ep0_in_urb.buffer = ep0_out_urb.buffer = NULL;
                        ep0_in_urb.device = ep0_out_urb.device = NULL;
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
#if defined(CONFIG_MIPS_PICOENGINE_MVCI)
	extern void pico_mvci_set_usb_pullup(int);
	printk("udc_connect()\n");
	pico_mvci_set_usb_pullup(1);
#endif
}


/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect (void)
{
#if defined(CONFIG_MIPS_PICOENGINE_MVCI)
	extern void pico_mvci_set_usb_pullup(int);
	printk("udc_disconnect()\n");
	pico_mvci_set_usb_pullup(0);
#endif
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
        /*
         * Only enable receive interrupts.
         *
         * The AU1x00 will generate an interrupt for EVERY IN packet. If we do
         * not have any data we need to disable the interrupt for the endpoint
         * to eliminate the overhead for processing them. The interrupt must be
         * re-enabled when data is queued and we actually want to know about 
         * activity.
         */

        //TRACE_MSG("udc_all_interrupts");
        au_inten(0x0033/*|USBDEV_INT_SOF*/);
}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts (struct usb_device_instance *device)
{
        //TRACE_MSG("udc_suspended_interrupts");
        au_inten(0x0033/*|USBDEV_INT_SOF*/);
}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts (struct usb_device_instance *device)
{
        //TRACE_MSG("udc_disable_interrupts");
        au_inten(0);
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
	// disable UDC
        au_writel(0x0000, USBD_ENABLE);

	// reset device pointer
	udc_device = NULL;
}

void udc_exit(void)
{
	// ep0 urb
        unsigned long flags;
        local_irq_save (flags);
        if (ep0_in_urb.buffer) {
                lkfree (ep0_in_urb.buffer);
        }
        if (ep0_out_urb.buffer) {
                lkfree (ep0_out_urb.buffer);
        }
        ep0_in_urb.buffer = ep0_out_urb.buffer = NULL;
        ep0_in_urb.device = ep0_out_urb.device = NULL;
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
	usbd_device_event (device, DEVICE_RESET, 0);	// XXX should be done from device event
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
        /*
         * N.B. - do not use SA_SAMPLE_RANDOM for this device. It adds
         * significant latency and can impair the transmit service.
         */

	if (request_irq (AU1000_USB_DEV_REQ_INT, udc_int_req, SA_INTERRUPT, UDC_NAME "UDC Req", NULL) != 0) {
		printk (KERN_INFO "usb_ctl: Couldn't request USB Req irq\n");
		return -EINVAL;
	}
	if (request_irq (AU1000_USB_DEV_SUS_INT, udc_int_sus, SA_INTERRUPT, UDC_NAME "UDC Sus", NULL) != 0) {
		printk (KERN_INFO "usb_ctl: Couldn't request USB Sus irq\n");
                udc_release_udc_irq();
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


static int request_dma(int ep, int id, char *str, void dma_done(int , void *, struct pt_regs *))
{
        int dma;
        if ((dma = request_au1000_dma(id, str, dma_done, SA_INTERRUPT | SA_SAMPLE_RANDOM, (void *)ep)) < 0) {
                printk(KERN_INFO"request_io[%d] dma: %d id: %x %s FAILED\n", ep, dma, id, str);
                return -1;
        }
        return dma;
}

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io (void)
{
        int i;

        // XXX 
        for (i = 0; i < 6; i++) {
                ep_regs[i].indma = ep_regs[i].tx_id ? request_dma(i, ep_regs[i].tx_id, ep_regs[i].tx_str, udc_tx_dma_done) : -1;
                ep_regs[i].outdma = ep_regs[i].rx_id ? request_dma(i, ep_regs[i].rx_id, ep_regs[i].rx_str, NULL) : -1;
        }
        return 0;
}

/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq (void)
{

        free_irq (AU1000_USB_DEV_REQ_INT, NULL);
        free_irq (AU1000_USB_DEV_SUS_INT, NULL);

#if defined(MAX_INTR_LOOP_STATS)
        {
                u32 lc;
                /*
                 * Emit collected interrupt loop stats.
                 */
                for (lc = 0; lc <= MAX_INTR_LOOP_STATS; lc++) {
                        if (interrupt_loop_stats[lc]) {
                                printk(KERN_ERR "%s: interrupt loopcount[%02u] %9u\n", __FUNCTION__, lc,interrupt_loop_stats[lc]);
                        }
                }
                printk(KERN_ERR "%s: udc spurious interrupts %9u\n", __FUNCTION__, udc_spurious);
                printk(KERN_INFO"%s: halt_dma_expired: %d\n", __FUNCTION__, au_halt_dma_expired);
        }
#endif

#ifdef RECORD_LATENCY
        {
                int i;
                for (i = 0; i < CP0_COUNTS; i++) {
                        if (cp0_counts[i]) {
                                printk(KERN_INFO"%s: cp0_counts[%d] %d\n", __FUNCTION__, i, cp0_counts[i]);
                        }
                }
        }
#endif
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
        int j;
        for (j = 0; j < 6; j++) {
                if (ep_regs[j].indma != -1) {
                        free_au1000_dma(ep_regs[j].indma);
                }
                if (ep_regs[j].outdma != -1) {
                        free_au1000_dma(ep_regs[j].outdma);
                }
        }
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

