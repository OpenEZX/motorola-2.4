/*
 * linux/drivers/usbd/bi/lh7a400-hardware.h -- L7205 USB controller driver. 
 *
 * Copyright (c) 2002, 2003 Belcarra
 * Copyright (c) 2002 Lineo
 *
 * By: 
 *      Chris Lynne <cl@belcarra.com>, 
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
 * Memory map
 */

#define USB_FA					0x80000200 	// function address register
#define USB_PM					0x80000204	// power management register

#define USB_IN_INT				0x80000208	// IN interrupt register bank (EP0-EP3)
#define USB_OUT_INT				0x80000210	// OUT interrupt register bank (EP2)
#define USB_INT					0x80000218	// interrupt register bank

#define USB_IN_INT_EN				0x8000021C	// IN interrupt enable register bank
#define USB_OUT_INT_EN				0x80000224	// OUT interrupt enable register bank
#define USB_INT_EN				0x8000022C	// USB interrupt enable register bank

#define USB_FRM_NUM1				0x80000230	// Frame number1 register
#define USB_FRM_NUM2				0x80000234	// Frame number2 register
#define USB_INDEX				0x80000238	// index register

#define USB_IN_MAXP				0x80000240	// IN MAXP register
#define USB_IN_CSR1				0x80000244	// IN CSR1 register/EP0 CSR register
#define USB_EP0_CSR				0x80000244	// IN CSR1 register/EP0 CSR register
#define USB_IN_CSR2				0x80000248	// IN CSR2 register
#define USB_OUT_MAXP				0x8000024C	// OUT MAXP register

#define USB_OUT_CSR1				0x80000250	// OUT CSR1 register
#define USB_OUT_CSR2				0x80000254	// OUT CSR2 register 
#define USB_OUT_FIFO_WC1			0x80000258	// OUT FIFO write count1 register
#define USB_OUT_FIFO_WC2			0x8000025C	// OUT FIFO write count2 register

#define USB_RESET				0x8000044C	// USB reset register

#define	USB_EP0_FIFO				0x80000280
#define	USB_EP1_FIFO				0x80000284
#define	USB_EP2_FIFO				0x80000288
#define	USB_EP3_FIFO				0x8000028c

/*
 * USB reset register
 */
#define USB_RESET_APB			(1<<1)		//resets USB APB control side WRITE
#define USB_RESET_IO			(1<<0)		//resets USB IO side WRITE

/*
 * USB function address register 
 */
#define USB_FA_ADDR_UPDATE			(1<<7)	//
#define USB_FA_FUNCTION_ADDR			(0x7F)	//

/*
 * Power Management register
 */
#define USB_PM_USB_ENABLE			(1<<4)
#define USB_PM_USB_RESET			(1<<3)
#define USB_PM_UC_RESUME			(1<<2)
#define USB_PM_SUSPEND_MODE			(1<<1)
#define USB_PM_ENABLE_SUSPEND			(1<<0)

/*
 * IN interrupt register
 */
#define USB_IN_INT_EP3_IN			(1<<3)
#define USB_IN_INT_EP1_IN			(1<<1)
#define USB_IN_INT_EP0				(1<<0)

/*
 * OUT interrupt register
 */
#define USB_OUT_INT_EP2_OUT			(1<<2)

/*
 * USB interrupt register
 */
#define USB_INT_RESET_INT			(1<<2)
#define USB_INT_RESUME_INT			(1<<1)
#define USB_INT_SUSPEND_INT			(1<<0)

/*
 * USB interrupt enable register
 */
#define USB_INT_EN_USB_RESET_INTER		(1<<2)
#define USB_INT_EN_RESUME_INTER		(1<<1)
#define USB_INT_EN_SUSPEND_INTER		(1<<0)

/*
 * INCSR1 register
 */
#define USB_IN_CSR1_CLR_DATA_TOGGLE		(1<<6)
#define USB_IN_CSR1_SENT_STALL			(1<<5)
#define USB_IN_CSR1_SEND_STALL			(1<<4)
#define USB_IN_CSR1_FIFO_FLUSH			(1<<3)
#define USB_IN_CSR1_FIFO_NOT_EMPTY		(1<<1)
#define USB_IN_CSR1_IN_PKT_RDY			(1<<0)

/*
 * INCSR2 register
 */
#define USB_IN_CSR2_AUTO_SET			(1<<7)
#define USB_IN_CSR2_USB_DMA_EN			(1<<4)

/*
 * OUT CSR1 register
 */
#define USB_OUT_CSR1_CLR_DATA_REG		(1<<7)
#define USB_OUT_CSR1_SENT_STALL			(1<<6)
#define USB_OUT_CSR1_SEND_STALL			(1<<5)
#define USB_OUT_CSR1_FIFO_FLUSH			(1<<4)
#define USB_OUT_CSR1_FIFO_FULL			(1<<1)
#define USB_OUT_CSR1_OUT_PKT_RDY		(1<<0)

/*
 * OUT CSR2 register
 */
#define USB_OUT_CSR2_AUTO_CLR			(1<<7)
#define USB_OUT_CSR2_USB_DMA_EN			(1<<4)

/*
 * EP0 CSR
 */
#define USB_EP0_CSR_SERVICED_SETUP_END		(1<<7)
#define USB_EP0_CSR_SERVICED_OUT_PKT_RDY	(1<<6)
#define USB_EP0_CSR_SEND_STALL			(1<<5)
#define USB_EP0_CSR_SETUP_END			(1<<4)

#define USB_EP0_CSR_DATA_END			(1<<3)
#define USB_EP0_CSR_SENT_STALL			(1<<2)
#define USB_EP0_CSR_IN_PKT_RDY			(1<<1)
#define USB_EP0_CSR_OUT_PKT_RDY			(1<<0)

/*
 * IN/OUT MAXP register
 */
#define USB_OUT_MAXP_MAXP			(0xF)
#define USB_IN_MAXP_MAXP			(0xF)


