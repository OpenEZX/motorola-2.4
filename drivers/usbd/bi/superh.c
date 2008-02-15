/*
 * linux/drivers/usbd/bi/superh.c -- USB Device Controller driver. 
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2000, 2001, 2002 Lineo
 *
 * By: 
 *      Stuart Lynne <sl@lbelcarra.com>, 
 *      Tom Rushworth <tbr@belcarra.com>, 
 *      Bruce Balden <balden@belcarra.com>
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

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("sl@belcarra.com, tbr@belcarra.com");
MODULE_DESCRIPTION ("USB Device SuperH Bus Interface");
USBD_MODULE_INFO ("superh_bi 0.1-alpha");
MODULE_LICENSE("GPL");

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/delay.h>

#include <asm/types.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/sh7727.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

#include "superh.h"
#include "superh-hardware.h"
#include "trace.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/*
 * Define what interrupts are high versus low priority
 */

#define F0_HIGH (EP1_FULL | EP2_TR | EP2_EMPTY )
#define F0_LOW  (BRST | SETUP_TS | EP0o_TS | EP0i_TR | EP0i_TS)

#define F1_HIGH (0)
#define F1_LOW  (EP3_TR | EP3_TS | VBUSF)


static struct usb_device_instance *udc_device;	// required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb *ep0_urb;
static unsigned char usb_address;

extern unsigned int udc_interrupts;
unsigned int udc_f0_interrupts;
unsigned int udc_f1_interrupts;
unsigned long udc_vbusf_time;

/* ********************************************************************************************* */

/*
 * IO
 */

void and_b (unsigned short mask, unsigned long addr)
{
	ctrl_outb (ctrl_inw (addr) & mask, addr);
}

void or_b (unsigned short mask, unsigned long addr)
{
	ctrl_outb (ctrl_inw (addr) | mask, addr);
}

void and_w (unsigned short mask, unsigned long addr)
{
	ctrl_outw (ctrl_inw (addr) & mask, addr);
}

void or_w (unsigned short mask, unsigned long addr)
{
	ctrl_outw (ctrl_inw (addr) | mask, addr);
}

// IO addresses for each physical ports FIFO
unsigned long ep_address_o[4] = { USBEPDR0O, USBEPDR1, 0L, 0L, };
unsigned long ep_address_i[4] = { USBEPDR0I, 0L, USBEPDR2, USBEPDR3, };

// IO masks for each physical ports trigger
unsigned char ep_trigger_o[4] = { EP0o_RDFN, EP1_RDFN, 0L, 0L, };
unsigned char ep_trigger_i[4] = { EP0i_PKTE, 0L, EP2_PKTE, EP3_PKTE, };
unsigned char ep_datastatus_i[4] = { EP0i_DE, 0, EP2_DE, EP3_DE, };

/**
 * superh_write_buffer - write a buffer to the superh fifo
 * @ep: endpoint
 * @b: pointer to buffer to write
 * @size: number of bytes to write
 */
static /*__inline__*/ void superh_write_buffer (unsigned char ep, unsigned char *b, unsigned char size)
{
        TRACE_MSG32("WRITE: %d", size);

        if (size && b) {
                TRACE_SENT(b);
        }
        

	if ((ep < UDC_MAX_ENDPOINTS) && ep_address_i[ep]) {
		while (size-- > 0) {
			ctrl_outb (*b++, ep_address_i[ep]);
		}
		ctrl_outb (ep_trigger_i[ep], USBTRG);
	}
}

/**
 * superh_read_buffer - fill a buffer from the superh fifo
 * @ep: endpoint
 * @b: pointer to buffer to fill
 * @size: number of bytes to read
 */
static /*__inline__*/ void superh_read_buffer (unsigned char ep, unsigned char *b, unsigned char size)
{
	if ((ep < UDC_MAX_ENDPOINTS) && ep_address_o[ep]) {
		while (size-- > 0) {
			*b++ = ctrl_inb (ep_address_o[ep]);
		}
		ctrl_outb (ep_trigger_o[ep], USBTRG);
	}
}

/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */
static void /*__inline__*/ superh_out_ep1 (struct usb_endpoint_instance *endpoint)
{
	int size = ctrl_inb (USBEPSZ1);

	if (endpoint && endpoint->rcv_urb && size) {

                if (size > endpoint->rcv_packetSize) {
                        TRACE_MSG32("OUT EP1: TOO BIG %d", size);
                }
                size = MIN(size, endpoint->rcv_packetSize);
                TRACE_MSG16("OUT EP1: %d %d", size, endpoint->rcv_packetSize);
		// read data
		superh_read_buffer (1, endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length, size);
                {
                        int i;
                        unsigned char *cp = endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length;
                        for (i = 0; i < size; i += 8) {
                                TRACE_RECV(cp + i);
                        }
                }
		usbd_rcv_complete_irq (endpoint, size, 0);
	} 
        else {
		// reset fifo
		or_b (EP1_CLEAR, USBFCLR);
		usbd_rcv_complete_irq (endpoint, MIN(size, endpoint->rcv_packetSize), EINVAL);
	}
}

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */

/**
 * superh_in_epn - process tx interrupt
 * @ep:
 * @endpoint:
 *
 * Determine status of last data sent, queue new data.
 */
static /* __inline__ */ void superh_in_epn (int ep, int restart, struct usb_endpoint_instance *endpoint)
{
	if (ctrl_inb (USBIFR0) & EP2_EMPTY) {

		if ((ep < UDC_MAX_ENDPOINTS) && ep_endpoints[ep]) {
			struct usb_endpoint_instance *endpoint = ep_endpoints[ep];
			usbd_tx_complete_irq (endpoint, restart);
			if (endpoint->tx_urb) {
				struct urb *urb = endpoint->tx_urb;

				if ((urb->actual_length - endpoint->sent) > 0) {
					endpoint->last = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
					superh_write_buffer (ep, urb->buffer + endpoint->sent, endpoint->last);
				} 
                                else {
					// XXX ZLP
					endpoint->last = 0;
					superh_write_buffer (ep, urb->buffer + endpoint->sent, 0);
				}
				// enable transmit done interrupt
				switch (ep) {
				case 2:
					or_b (EP2_TR | EP2_EMPTY, USBIER0);
					break;
				case 3:
					or_b (EP3_TR | EP3_TS, USBIER1);
					break;
				}
			} 
                        else {
				// disable transmit done interrupt
				switch (ep) {
				case 2:
					if (ctrl_inb (USBIER0) & (EP2_TR | EP2_EMPTY)) {
						and_b (~(EP2_TR | EP2_EMPTY), USBIER0);
					}
					break;
				case 3:
					and_b (~(EP3_TR | EP3_TS), USBIER1);
					break;
				}
			}
		}
	}
}

/* ********************************************************************************************* */
/* Control (endpoint zero)
 */

/**
 * superh_in_ep0 - start transmit
 * @ep:
 */
static void /*__inline__*/ superh_in_ep0 (struct usb_endpoint_instance *endpoint)
{
	if (endpoint && endpoint->tx_urb) {
		struct urb *urb = endpoint->tx_urb;

		//printk (KERN_DEBUG "superh_in_ep0: length: %d\n", endpoint->tx_urb->actual_length);	// XXX 
                
                udelay(10);
		TRACE_MSG16("IN EP0: actual: %d sent: %d", endpoint->tx_urb->actual_length, endpoint->sent);

		usbd_tx_complete_irq (ep_endpoints[0], 0);


		if ((urb->actual_length - endpoint->sent) > 0) {
			endpoint->last = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
                        TRACE_SENT(urb->buffer + endpoint->sent);
			superh_write_buffer (0, urb->buffer + endpoint->sent, endpoint->last);
		} 
                else {
                        TRACE_MSG("IN EP0: finished tx_urb, send ZLP");	// XXX 
			// XXX ZLP
			endpoint->last = 0;
			superh_write_buffer (0, NULL, 0);
		}
	}
        else {
                TRACE_MSG("IN EP0: no tx_urb, send ZLP");	// XXX 
                // XXX ZLP
                endpoint->last = 0;
                superh_write_buffer (0, NULL, 0);
        }
}

/**
 * superh_out_ep0 - start transmit
 * @ep:
 */
static void /*__inline__*/ superh_out_ep0 (struct usb_endpoint_instance *endpoint)
{
	if (endpoint && endpoint->rcv_urb) {
		struct urb *urb = endpoint->rcv_urb;

                int size = ctrl_inb (USBEPSZ0O);

		TRACE_MSG8("OUT EP0: actual: %d transferSize: %d size: %d", 
                                urb->actual_length, endpoint->rcv_transferSize, size, 0);

                // read data
                superh_read_buffer (0, endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length, size);
                //TRACE_RECV(endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length);

                endpoint->rcv_urb->actual_length += size;

                if ((size && (size < endpoint->rcv_packetSize)) || 
                                ((endpoint->rcv_urb->actual_length == endpoint->rcv_transferSize))) 
                {

                        //urb_append_irq(&endpoint->rcv, endpoint->rcv_urb);
                        TRACE_MSG16("OUT EP0: call recv_setup: size: %x transfer: %x", 
                                        endpoint->rcv_urb->actual_length, endpoint->rcv_transferSize);

                        //TRACE_RECV(endpoint->rcv_urb->buffer);
                        //TRACE_RECV(endpoint->rcv_urb->buffer+8);
                        //TRACE_RECV(endpoint->rcv_urb->buffer+16);

                        endpoint->state = WAIT_FOR_SETUP;

                        if (usbd_recv_setup(ep0_urb)) {
                                // setup processing failed 
                                return;
                        }
                }

        } 
        ctrl_outb (EP0o_RDFN, USBTRG);
}

/**
 * superh_ep0_setup
 */
static void superh_ep0_setup (void)
{
	if (ep_endpoints[0]) {
		int i;
		struct usb_endpoint_instance *endpoint = ep_endpoints[0];
		unsigned char *cp = (unsigned char *) &ep0_urb->device_request;

		for (i = 0; i < 8; i++) {
			cp[i] = ctrl_inb (USBEPDR0S);
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
                        ctrl_outb (EP0s_RDFN, USBTRG);
                        return;
                }

                // process setup packet
                ep0_urb->actual_length = 0;
                memset(ep0_urb->buffer, 0, ep0_urb->buffer_length);


		// process setup packet
		if (usbd_recv_setup (ep0_urb)) {
			printk (KERN_DEBUG "superh_ep0: setup failed\n");
			TRACE_MSG("superh_ep0: setup failed");
                        endpoint->state = WAIT_FOR_SETUP;
			return;
		}

		// check data direction
		if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) {

			TRACE_MSG32("superh_ep0: send zlp %d", le16_to_cpu (ep0_urb->device_request.wLength));	// XXX 

			// should be finished, send ack
                        // XXX ZLP
                        endpoint->last = 0;
			ctrl_outb (EP0s_RDFN, USBTRG);
			return;
		}

		// we should be sending data back

		// verify that we have non-zero request length
		if (!le16_to_cpu (ep0_urb->device_request.wLength)) {
			udc_stall_ep (0);
			return;
		}

		// verify that we have non-zero length response
		if (!ep0_urb->actual_length) {
			udc_stall_ep (0);
			return;
		}

		// send ack prior to sending data
		ctrl_outb (EP0s_RDFN, USBTRG);

		// start sending
		endpoint->tx_urb = ep0_urb;
		endpoint->sent = 0;
		endpoint->last = 0;
		superh_in_ep0 (endpoint);
	}
        else {
                TRACE_MSG("EP0 Setup: no endpoint");
	}
}


/* ********************************************************************************************* */
/* Interrupt Handler(s)
 */


/**
 * superh_int_hndlr_f0 - high priority interrupt handler
 *
 */
static void superh_int_hndlr_f0 (int irq, void *dev_id, struct pt_regs *regs)
{

	unsigned char f0_status;

	udc_interrupts++;
	udc_f0_interrupts++;
	f0_status = ctrl_inb (USBIFR0);
        TRACE_MSG32("F0 status %02x", f0_status); // XXX 

	if (f0_status & EP1_FULL) {
		superh_out_ep1 (ep_endpoints[1]);
#if 0
		f0_status = ctrl_inb (USBIFR0);
		if (f0_status & EP1_FULL) {
			superh_out_ep1 (ep_endpoints[1]);
		}
#endif
	} 
        else if (f0_status & (EP2_TR | EP2_EMPTY)) {
		superh_in_epn (2, 0, ep_endpoints[2]);	// XXX status?
		ctrl_outb (~(f0_status & EP2_TR), USBIFR0);
	}
}

/**
 * superh_int_hndlr_f1 - low priority interrupt handler
 *
 */
static void superh_int_hndlr_f1 (int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char f0_status;
	unsigned char f1_status;

	udc_interrupts++;
	udc_f1_interrupts++;
	f0_status = ctrl_inb (USBIFR0);
	f1_status = ctrl_inb (USBIFR1);
	ctrl_outb (~(f0_status & F0_LOW), USBIFR0);
	ctrl_outb (~(f1_status & F1_LOW), USBIFR1);

        TRACE_MSG16("F1 status %02x %02x", f0_status, f1_status); // XXX 

#if 0
        if (udc_interrupts > 2000) {
                udc_disable_interrupts(NULL);
                TRACE_MSG("UDC DISABLE");
                return;
        }
#endif

	if (f1_status & VBUSF) {
                TRACE_MSG("VBUSF");
		/*
		 * XXX vbusf can bounce, probably should disarm vbusf interrupt, for now, just check 
		 * that we don't handle it more than once.
		 */
		if (!udc_vbusf_time || ((jiffies - udc_vbusf_time) > 1)) {
			if (udc_device) {
				if (f1_status & VBUSMN) {
                                        TRACE_MSG("VBUSF VBUSMN set");
				} 
                                else {
                                        TRACE_MSG("VBUSF VBUSMN reset");
				}
			}
		}
		udc_vbusf_time = jiffies;
	} 
        else {
		udc_vbusf_time = 0;
	}
	if (f0_status & BRST) {
                TRACE_MSG("BRST");

		// reset fifo's and stall's
		or_b (EP3_CLEAR | EP1_CLEAR | EP2_CLEAR | EP0o_CLEAR | EP0i_CLEAR, USBFCLR);
		or_b (0, USBEPSTL);

		if (udc_device->device_state != DEVICE_RESET) {
                        TRACE_MSG("DEVICE RESET");
			usbd_device_event_irq (udc_device, DEVICE_RESET, 0);
#if 1
			/*
			 * superh won't give us set address, configuration or interface, but at least
			 * we know that at this point we know that a host is talking to us, close
			 * enough.
			 */
			usbd_device_event (udc_device, DEVICE_ADDRESS_ASSIGNED, 0);
			udc_device->configuration = 0;
			usbd_device_event (udc_device, DEVICE_CONFIGURED, 0);
			udc_device->interface = 1;	// no options
			udc_device->alternate = 0;	// no options
			usbd_device_event (udc_device, DEVICE_SET_INTERFACE, 0);
#endif
		}
	}
	if (f0_status & SETUP_TS) {
                TRACE_MSG("SETUP_TS");
		or_b (EP0o_CLEAR | EP0i_CLEAR, USBFCLR);
		superh_ep0_setup ();
	}
	if (f0_status & EP0i_TR) {
                TRACE_MSG("ep0o TR - Transfer Request");
		usbd_tx_complete_irq (ep_endpoints[0], 0);
		superh_in_ep0 (ep_endpoints[0]);
	}
	if (f0_status & EP0o_TS) {
                TRACE_MSG("ep0o TS - Receive Complete");
                superh_out_ep0 (ep_endpoints[0]);
	}
	if (f0_status & EP0i_TS) {
                TRACE_MSG("ep0i TS - Transmit Complete");
	}
	if (f1_status & EP3_TR) {
                TRACE_MSG("EP3 TR - Transfer Request");
		superh_in_epn (3, 0, ep_endpoints[3]);	// XXX status?
	}
	if (f1_status & EP3_TS) {
                TRACE_MSG("EP3 TS - Transmit Complete");
                superh_in_epn (3, 0, ep_endpoints[3]);	// XXX status?
	}
}


/* ********************************************************************************************* */


/* ********************************************************************************************* */
/* Start of public functions.  */

/**
 * udc_start_in_irq - start transmit
 * @urb: urb
 *
 * Called by bus interface driver to see if we need to start a data transmission.
 */
void udc_start_in_irq (struct urb *urb)
{
        struct usb_endpoint_instance *endpoint = urb->endpoint;
        TRACE_MSG("start in irq");
	if (endpoint) {
		// XXX should verify logical address mapping to physical 2
		superh_in_epn (endpoint->endpoint_address & 0xf, 0, endpoint);
	}
}

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
	// XXX move clock enable to udc_enable, add code to udc_disable to disable them
        TRACE_MSG("udc_init");

	// Reset and then Select Function USB1_pwr_en out (USB) c.f. Section 26, Table 26.1 PTE2
	and_w (PN_PB2_MSK, PECR);
	or_w (PN_PB2_OF, PECR);

	// Reset and then Select Function UCLK c.f. Section 26, Table 26.1, PTD6
	and_w (PN_PB6_MSK, PDCR);
	or_w (PN_PB6_OF, PDCR);

	// Stop USB module prior to setting clocks c.f. Section 9.2.3
	and_b (~MSTP14, STBCR3);
	or_b (MSTP14, STBCR3);

	// Select external clock, 1/1 divisor c.f. Section 11.3.1
	or_b (USBDIV_11 | USBCKS_EC, EXCPGCR);

	// Start USB c.f. Section 9.2.3
	and_b (~MSTP14, STBCR3);

	// Disable pullup c.f. Section 23.5.19
	or_b (PULLUP_E, USBDMA);
        udelay(100);

	// Set port 1 to function, disabled c.f. Section 22.2.1
	or_w (USB_TRANS_TRAN | USB_SEL_FUNC, EXPFC);

	// Enable pullup c.f. Section 23.5.19
	//and_b (~PULLUP_E, USBDMA);

	// reset fifo's and stall's
	or_b (EP3_CLEAR | EP1_CLEAR | EP2_CLEAR | EP0o_CLEAR | EP0i_CLEAR, USBFCLR);
	or_b (0, USBEPSTL);

	// setup interrupt priority by using the interrupt select registers
	ctrl_outb (F0_LOW, USBISR0);
	ctrl_outb (F1_LOW, USBISR1);

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
	return (((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) || (packetsize > 64)) ? 0 : (logical_endpoint & 0xf);
}

/**
 * udc_set_ep - setup endpoint 
 * @ep:
 * @endpoint:
 *
 * Associate a physical endpoint with endpoint_instance
 */
void udc_setup_ep (struct usb_device_instance *device, unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        //printk (KERN_INFO "%s: ep: %d\n", __FUNCTION__, ep);
	if (ep < UDC_MAX_ENDPOINTS) {

		ep_endpoints[ep] = endpoint;
		// ep0
		if (ep == 0) {
		}
		// IN
		else if (endpoint->endpoint_address & 0x80) {
		}
		// OUT
		else if (endpoint->endpoint_address) {
			usbd_fill_rcv (device, endpoint, 5);
			endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
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
        //printk (KERN_INFO "%s: ep: %d\n", __FUNCTION__, ep);
	if (ep < UDC_MAX_ENDPOINTS) {
		struct usb_endpoint_instance *endpoint;

		if ((endpoint = ep_endpoints[ep])) {
			ep_endpoints[ep] = NULL;
			usbd_flush_ep (endpoint);
		}
	}
}

/* ********************************************************************************************* */

/**
 * udc_connected - is the USB cable connected
 *
 * Return non-zero if cable is connected.
 */
int udc_connected ()
{
        //printk (KERN_INFO "%s:\n", __FUNCTION__);
	return (ctrl_inb (USBIFR1) & VBUSMN) ? 1 : 0;
}

/**
 * udc_connect - enable pullup resistor
 *
 * Turn on the USB connection by enabling the pullup resistor.
 */
void udc_connect (void)
{
	// enable pullup
        //printk (KERN_INFO "%s:\n", __FUNCTION__);
	and_b (~PULLUP_E, USBDMA);
}

/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect (void)
{
        //printk (KERN_INFO "%s:\n", __FUNCTION__);
	// enable pullup
	or_b (PULLUP_E, USBDMA);
}

/* ********************************************************************************************* */

/**
 * udc_enable_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts (struct usb_device_instance *device)
{
        TRACE_MSG("ALL INTERRUPTS");
	// set interrupt mask
	
        or_b (BRST | EP1_FULL | SETUP_TS | EP0o_TS | EP0i_TR | EP0i_TS, USBIER0);
	or_b (EP3_TR | EP3_TS | VBUSF, USBIER1);


        TRACE_MSG16("ALL INTERRUPTS: IER0: %02x IER1: %02x", ctrl_inb(USBIER0), ctrl_inb(USBIER1));
}

/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts (struct usb_device_instance *device)
{
        printk (KERN_INFO "%s:\n", __FUNCTION__);
}

/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts (struct usb_device_instance *device)
{
        printk (KERN_INFO "%s:\n", __FUNCTION__);
	// reset interrupt mask
	ctrl_outb (0x0, USBIER0);
	ctrl_outb (0x0, USBIER1);
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
        printk (KERN_INFO "%s:\n", __FUNCTION__);

	// save the device structure pointer
	udc_device = device;

	// ep0 urb
	if (!ep0_urb) {
		if (!(ep0_urb = usbd_alloc_urb (device, 0, 512))) {
			printk (KERN_ERR "ep0_enable: usbd_alloc_urb failed\n");
		}
	} 
        else {
		printk (KERN_ERR "udc_enable: ep0_urb already allocated\n");
	}

        udc_all_interrupts(device);

	// XXX enable UDC
 
	// Enable pullup c.f. Section 23.5.19
	and_b (~PULLUP_E, USBDMA);
}

/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable (void)
{
        printk (KERN_INFO "%s:\n", __FUNCTION__);

	// XXX disable UDC

	// reset device pointer
	udc_device = NULL;

	// ep0 urb
	if (ep0_urb) {
		usbd_dealloc_urb (ep0_urb);
		ep0_urb = NULL;
	}
}

/**
 * udc_startup_events - allow udc code to do any additional startup
 */
void udc_startup_events (struct usb_device_instance *device)
{
        printk (KERN_INFO "%s:\n", __FUNCTION__);
	usbd_device_event (device, DEVICE_INIT, 0);
	usbd_device_event (device, DEVICE_CREATE, 0);
	usbd_device_event (device, DEVICE_HUB_CONFIGURED, 0);

	// XXX the following could be snuck into get descriptor LANGID

        usbd_device_event (udc_device, DEVICE_ADDRESS_ASSIGNED, 0);
        udc_device->configuration = 0;
        usbd_device_event (udc_device, DEVICE_CONFIGURED, 0);
        udc_device->interface = 1;	// no options
        udc_device->alternate = 0;	// no options
        usbd_device_event (udc_device, DEVICE_SET_INTERFACE, 0);

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
 */
int udc_request_udc_irq ()
{
	if (request_irq (USBF0_IRQ, superh_int_hndlr_f0, SA_INTERRUPT | SA_SAMPLE_RANDOM, UDC_NAME
			 " USBD Bus Interface (high priority)", NULL) != 0) 
        {
		printk (KERN_INFO "usb_ctl: Couldn't request USB irq F0\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * udc_request_cable_irq - request Cable interrupt
 */
int udc_request_cable_irq ()
{
	if (request_irq (USBF1_IRQ, superh_int_hndlr_f1, SA_INTERRUPT | SA_SAMPLE_RANDOM,
			 UDC_NAME " USBD Bus Interface (low priority)", NULL) != 0) 
        {
		printk (KERN_INFO "usb_ctl: Couldn't request USB irq F1\n");
		free_irq (USBF0_IRQ, NULL);
		return -EINVAL;
	}
	return 0;
}

/**
 * udc_request_udc_io - request UDC io region
 */
int udc_request_io ()
{
	return 0;
}

/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq ()
{
	free_irq (USBF0_IRQ, NULL);
}

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq ()
{
	free_irq (USBF1_IRQ, NULL);
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io ()
{
}

/**
 * udc_regs - dump registers
 */
void udc_regs (void)
{
#if 0
	printk (KERN_INFO
		"[%d:%d:%d] IFR[%02x:%02x] IER[%02x:%02x] ISR[%02x:%02x] DASTS[%02x] EPSTL[%02x] EPSZ1[%02x] DMA[%02x]\n",
		udc_interrupts, udc_f0_interrupts, udc_f1_interrupts,
		ctrl_inb (USBIFR0), ctrl_inb (USBIFR1), ctrl_inb (USBIER0), ctrl_inb (USBIER1),
		ctrl_inb (USBISR0), ctrl_inb (USBISR1), ctrl_inb (USBDASTS), ctrl_inb (USBEPSTL),
		ctrl_inb (USBEPSZ1), ctrl_inb (USBDMA)
	    );
#endif
}
