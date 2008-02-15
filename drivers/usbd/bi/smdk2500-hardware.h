/*
 * linux/drivers/usbd/bi/samsung-hardware.h
 *
 * Copyright (c) 2002, 2003 Belcarra
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

// USB special Registers

#define USB_FA			0xF00E0000 		// USB function address register Read/Write
#define USB_PM			0xF00E0004 		// USB power management register Read/Write
#define USB_INTR    		0xF00E0008 		// USB interrupt register Read/Write
#define USB_INTRE		0xF00E000C 		// USB interrupt enable register Read/Write
#define USB_FN			0xF00E0010 		// USB frame number register Read 
#define USB_DISCONN		0xF00E0014 		// USB disconnect timer register Read/Write
#define USB_EP0CSR		0xF00E0018 		// USB endpoint 0 common status register Read/Write
#define USB_EP1CSR		0xF00E001C 		// USB endpoint 1 common status register Read/Write
#define USB_EP2CSR		0xF00E0020 		// USB endpoint 2 common status register Read/Write 
#define USB_EP3CSR		0xF00E0024 		// USB endpoint 3 common status register Read/Write
#define USB_EP4CSR		0xF00E0028 		// USB endpoint 4 common status register Read/Write
#define USB_WCEP0		0xF00E0030		// USB write count register for endpoint 0 Read/Write
#define USB_WCEP1		0xF00E0034 		// USB write count register for endpoint 1 Read/Write
#define USB_WCEP2		0xF00E0038 		// USB write count register for endpoint 2 Read/Write
#define USB_WCEP3		0xF00E003C 		// USB write count register for endpoint 3 Read/Write
#define USB_WCEP4		0xF00E0040 		// USB write count register for endpoint 4 Read/Write
#define USB_EP0			0xF00E0080 		// USB endpoint 0 FIFO Read/Write
#define USB_EP1			0xF00E0084 		// USB endpoint 1 FIFO Read/Write
#define USB_EP2			0xF00E0088 		// USB endpoint 2 FIFO Read/Write 
#define USB_EP3			0xF00E008C 		// USB endpoint 3 FIFO Read/Write
#define USB_EP4			0xF00E0090 		// USB endpoint 4 FIFO Read/Write

// USB function address register Read/Write
#define USB_FA_FAF		0x3F			// MCU writes address here
#define USB_FA_BAU		(1<<7)			// Address update

// USB power management register Read/Write
#define USB_PM_SUSE		1			// 1 enable suspendmode, 0 disable suspend mode
#define USB_PM_SUSM		(1<<1)			// Set when suspend mode is enterd
#define USB_PM_RU  		(1<<2)			// resume?
#define USB_PM_RST 		(1<<3)			// reset
#define USB_PM_ISOU		(1<<7)			// ISO update

// USB interrupt register Read/Write
#define USB_INTR_EP0I		1			// End point 0 interrupt, see docs 10-15 for details
#define USB_INTR_EP1I		(1<<1)			// End point 1 interrupt, see docs 10-15 for details
#define USB_INTR_EP2I		(1<<2)			// End point 2 interrupt, see docs 10-15 for details
#define USB_INTR_EP3I		(1<<3)			// End point 3 interrupt, see docs 10-15 for details
#define USB_INTR_EP4I		(1<<4)			// End point 4 interrupt, see docs 10-15 for details
#define USB_INTR_SUSI		(1<<8)			// Suspend interrupt
#define USB_INTR_RESI		(1<<9)			// Resume interrupt
#define USB_INTR_RSTI		(1<<10)			// Reset interrupt
#define USB_INTR_DISCI		(1<<11)			// disconnect interrupt

// USB interrupt enable register Read/Write
#define USB_INTRE_EP0IEN 	1			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_EP1IEN 	(1<<1)			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_EP2IEN 	(1<<2)			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_EP3IEN 	(1<<3)			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_EP4IEN 	(1<<4)			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_SUSIEN 	(1<<8)			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_RSTIEN 	(1<<10)			// 0 corresponding interrupt is disabled, 1 enabled
#define USB_INTRE_DISCIEN 	(1<<11)			// 0 corresponding interrupt is disabled, 1 enabled

// USB frame number register Read 
#define USB_FN_FN		0x7FF			// Frame number from sof packet

// USB disconnect timer register Read/Write
#define USB_DISCONN_CNTVLE  	0x7FFFFF		// Disconnect time value
#define USB_DISCONN_DISSTRT 	(1<<31)			// disconnect operation, see docs 10-20 for detail

// USB endpoint 0 common status register Read/Write
#define USB_EP0CSR_MAXP		0xf			// MAXP size value
#define USB_EP0CSR_MAXPSET	(1<<7)			// MAXP size settable
#define USB_EP0CSR_USBORDY	(1<<24)			// ?
#define USB_EP0CSR_USBINRDY	(1<<25)			// In packet ready
#define USB_EP0CSR_USBSTSTALL	(1<<26)			// sent stall
#define USB_EP0CSR_USBDEND	(1<<27)			// Data end
#define USB_EP0CSR_USBSETEND	(1<<28)			// Setup end
#define USB_EP0CSR_USBSDSTALL	(1<<29)			// send stall
#define USB_EP0CSR_USBSVORDY	(1<<30)			// serviced out ready
#define USB_EP0CSR_USBSVSET	(1<<31)			// service setup end

// USB endpoint 1 common status register Read/Write
#define USB_EP1CSR_MAXP		0x7			// MAXP size value
#define USB_EP1CSR_MAXPSET	(1<<7)			// MAXP size settable
#define USB_EP1CSR_OISO		(1<<8)			// Out mode, ISO mode
#define USB_EP1CSR_OATCLR	(1<<9)			// Out mode, Autoclear	
#define USB_EP1CSR_MODE		(1<<10)			// In/Out mode selction
#define USB_EP1CSR_IISO		(1<<11)			// In mode, ISO mode
#define USB_EP1CSR_IATSET	(1<<12)			// In mode Auto set
#define USB_EP1CSR_CSR2SET	(1<<15)			// CSR2 set table
#define USB_EP1CSR_OORDY	(1<<16)			// Out mode, Out packet ready
#define USB_EP1CSR_OOFULL	(1<<17)			// Out mode FIFO full
#define USB_EP1CSR_OOVER	(1<<18)			// Out mode FIFO overrun
#define USB_EP1CSR_ODERR	(1<<19)			// Out more data error
#define USB_EP1CSR_OFFLUSH	(1<<20)			// Out mode, FIFO flush
#define USB_EP1CSR_OSDSTALL	(1<<21)			// Out mode send stall
#define USB_EP1CSR_OSTSTALL	(1<<22)			// out mode sent stall
#define USB_EP1CSR_OCLTOG	(1<<23)			// out mode clear data toggle
#define USB_EP1CSR_IINRDY	(1<<24)			// in mode in packet ready
#define USB_EP1CSR_INEMP	(1<<25)			// in mode, fifo not empty
#define USB_EP1CSR_IUNDER	(1<<26)			// in mode, under run
#define USB_EP1CSR_IFFLUSH	(1<<27)			// in mode fifo flush
#define USB_EP1CSR_ISDSTALL	(1<<28)			// in mode send stall
#define USB_EP2CSR_ISTSALL	(1<<29)			// in mode sent stall
#define USB_EP2CSR_ICLTOG	(1<<30)			// in mode clear data toggle

// USB endpoint 2 common status register Read/Write
#define USB_EP2CSR_MAXP		0x7			// MAXP size value
#define USB_EP2CSR_MAXPSET	(1<<7)			// MAXP size settable
#define USB_EP2CSR_OISO		(1<<8)			// Out mode, ISO mode
#define USB_EP2CSR_OATCLR	(1<<9)			// Out mode, Autoclear	
#define USB_EP2CSR_MODE		(1<<10)			// In/Out mode selction
#define USB_EP2CSR_IISO		(1<<11)			// In mode, ISO mode
#define USB_EP2CSR_IATSET	(1<<12)			// In mode Auto set
#define USB_EP2CSR_CSR2SET	(1<<15)			// CSR2 set table
#define USB_EP2CSR_OORDY	(1<<16)			// Out mode, Out packet ready
#define USB_EP2CSR_OOFULL	(1<<17)			// Out mode FIFO full
#define USB_EP2CSR_OOVER	(1<<18)			// Out mode FIFO overrun
#define USB_EP2CSR_ODERR	(1<<19)			// Out more data error
#define USB_EP2CSR_OFFLUSH	(1<<20)			// Out mode, FIFO flush
#define USB_EP2CSR_OSDSTALL	(1<<21)			// Out mode send stall
#define USB_EP2CSR_OSTSTALL	(1<<22)			// out mode sent stall
#define USB_EP2CSR_OCLTOG	(1<<23)			// out mode clear data toggle
#define USB_EP2CSR_IINRDY	(1<<24)			// in mode in packet ready
#define USB_EP2CSR_INEMP	(1<<25)			// in mode, fifo not empty
#define USB_EP2CSR_IUNDER	(1<<26)			// in mode, under run
#define USB_EP2CSR_IFFLUSH	(1<<27)			// in mode fifo flush
#define USB_EP2CSR_ISDSTALL	(1<<28)			// in mode send stall
#define USB_EP2CSR_ISTSALL	(1<<29)			// in mode sent stall
#define USB_EP2CSR_ICLTOG	(1<<30)			// in mode clear data toggle


//USB endpoint 3 common status register Read/Write
#define USB_EP3CSR_MAXP		0xF			// MAXP size value
#define USB_EP3CSR_MAXPSET	(1<<7)			// MAXP size settable
#define USB_EP3CSR_OISO		(1<<8)			// Out mode, ISO mode
#define USB_EP3CSR_OATCLR	(1<<9)			// Out mode, Autoclear	
#define USB_EP3CSR_MODE		(1<<10)			// In/Out mode selction
#define USB_EP3CSR_IISO		(1<<11)			// In mode, ISO mode
#define USB_EP3CSR_IATSET	(1<<12)			// In mode Auto set
#define USB_EP3CSR_CSR2SET	(1<<15)			// CSR2 set table
#define USB_EP3CSR_OORDY	(1<<16)			// Out mode, Out packet ready
#define USB_EP3CSR_OOFULL	(1<<17)			// Out mode FIFO full
#define USB_EP3CSR_OOVER	(1<<18)			// Out mode FIFO overrun
#define USB_EP3CSR_ODERR	(1<<19)			// Out more data error
#define USB_EP3CSR_OFFLUSH	(1<<20)			// Out mode, FIFO flush
#define USB_EP3CSR_OSDSTALL	(1<<21)			// Out mode send stall
#define USB_EP3CSR_OSTSTALL	(1<<22)			// out mode sent stall
#define USB_EP3CSR_OCLTOG	(1<<23)			// out mode clear data toggle
#define USB_EP3CSR_IINRDY	(1<<24)			// in mode in packet ready
#define USB_EP3CSR_INEMP	(1<<25)			// in mode, fifo not empty
#define USB_EP3CSR_IUNDER	(1<<26)			// in mode, under run
#define USB_EP3CSR_IFFLUSH	(1<<27)			// in mode fifo flush
#define USB_EP3CSR_ISDSTALL	(1<<28)			// in mode send stall
#define USB_EP3CSR_ISTSALL	(1<<29)			// in mode sent stall
#define USB_EP3CSR_ICLTOG	(1<<30)			// in mode clear data toggle

// USB endpoint 4 common status register Read/Write
#define USB_EP4CSR_MAXP		0xF			// MAXP size value
#define USB_EP4CSR_MAXPSET	(1<<7)			// MAXP size settable
#define USB_EP4CSR_OISO		(1<<8)			// Out mode, ISO mode
#define USB_EP4CSR_OATCLR	(1<<9)			// Out mode, Autoclear	
#define USB_EP4CSR_MODE		(1<<10)			// In/Out mode selction
#define USB_EP4CSR_IISO		(1<<11)			// In mode, ISO mode
#define USB_EP4CSR_IATSET	(1<<12)			// In mode Auto set
#define USB_EP4CSR_CSR2SET	(1<<15)			// CSR2 set table
#define USB_EP4CSR_OORDY	(1<<16)			// Out mode, Out packet ready
#define USB_EP4CSR_OOFULL	(1<<17)			// Out mode FIFO full
#define USB_EP4CSR_OOVER	(1<<18)			// Out mode FIFO overrun
#define USB_EP4CSR_ODERR	(1<<19)			// Out more data error
#define USB_EP4CSR_OFFLUSH	(1<<20)			// Out mode, FIFO flush
#define USB_EP4CSR_OSDSTALL	(1<<21)			// Out mode send stall
#define USB_EP4CSR_OSTSTALL	(1<<22)			// out mode sent stall
#define USB_EP4CSR_OCLTOG	(1<<23)			// out mode clear data toggle
#define USB_EP4CSR_IINRDY	(1<<24)			// in mode in packet ready
#define USB_EP4CSR_INEMP	(1<<25)			// in mode, fifo not empty
#define USB_EP4CSR_IUNDER	(1<<26)			// in mode, under run
#define USB_EP4CSR_IFFLUSH	(1<<27)			// in mode fifo flush
#define USB_EP4CSR_ISDSTALL	(1<<28)			// in mode send stall
#define USB_EP4CSR_ISTSALL	(1<<29)			// in mode sent stall
#define USB_EP4CSR_ICLTOG	(1<<30)			// in mode clear data toggle

// USB endpoint N common status register Read/Write
#define USB_EPNCSR_MAXPSET	(1<<7)			// MAXP size settable
#define USB_EPNCSR_OISO		(1<<8)			// Out mode, ISO mode
#define USB_EPNCSR_OATCLR	(1<<9)			// Out mode, Autoclear	
#define USB_EPNCSR_MODE		(1<<10)			// In/Out mode selction
#define USB_EPNCSR_IISO		(1<<11)			// In mode, ISO mode
#define USB_EPNCSR_IATSET	(1<<12)			// In mode Auto set
#define USB_EPNCSR_CSR2SET	(1<<15)			// CSR2 set table
#define USB_EPNCSR_OORDY	(1<<16)			// Out mode, Out packet ready
#define USB_EPNCSR_OOFULL	(1<<17)			// Out mode FIFO full
#define USB_EPNCSR_OOVER	(1<<18)			// Out mode FIFO overrun
#define USB_EPNCSR_ODERR	(1<<19)			// Out more data error
#define USB_EPNCSR_OFFLUSH	(1<<20)			// Out mode, FIFO flush
#define USB_EPNCSR_OSDSTALL	(1<<21)			// Out mode send stall
#define USB_EPNCSR_OSTSTALL	(1<<22)			// out mode sent stall
#define USB_EPNCSR_OCLTOG	(1<<23)			// out mode clear data toggle
#define USB_EPNCSR_IINRDY	(1<<24)			// in mode in packet ready
#define USB_EPNCSR_INEMP	(1<<25)			// in mode, fifo not empty
#define USB_EPNCSR_IUNDER	(1<<26)			// in mode, under run
#define USB_EPNCSR_IFFLUSH	(1<<27)			// in mode fifo flush
#define USB_EPNCSR_ISDSTALL	(1<<28)			// in mode send stall
#define USB_EPNCSR_ISTSTALL	(1<<29)			// in mode sent stall
#define USB_EPNCSR_ICLTOG	(1<<30)			// in mode clear data toggle

// MAXP encodings (maximum packet size)
#define USB_MAXP_0              0x0                     //
#define USB_MAXP_8              0x1                     //Max 8 bytes
#define USB_MAXP_16             0x2
#define USB_MAXP_24             0x3
#define USB_MAXP_32             0x4
#define USB_MAXP_40             0x5
#define USB_MAXP_48             0x6
#define USB_MAXP_56             0x7
#define USB_MAXP_64             0x8


// USB write count register for endpoint 0 Read/Write
#define USB_WCEP0_CPUWRTCNT	0x7F			// CPU write count
#define USB_WCEP0_WRTCNT	(0x7F<<16)		// Write count

// USB write count register for endpoint 1 Read/Write
#define USB_WCEP1_CPUWRTCNT	0x3F			// CPU write count
#define USB_WCEP1_WRTCNT	(0x3F<<16)		// Write count

// USB write count register for endpoint 2 Read/Write
#define USB_WCEP2_CPUWRTCNT	0x3F			// CPU write count
#define USB_WCEP2_WRTCNT	(0x3F<<16)		// Write count

// USB write count register for endpoint 3 Read/Write
#define USB_WCEP3_CPUWRTCNT	0x7F			// CPU write count
#define USB_WCEP3_WRTCNT	(0x7F<<16)		// Write count

// USB write count register for endpoint 4 Read/Write
#define USB_WCEP4_CPUWRTCNT	0x7F			// CPU write count
#define USB_WCEP4_WRTCNT	(0x7F<<16)		// Write count

// USB write count register for endpoint N Read/Write
#define USB_WCEPN_CPUWRTCNT	0x7F			// CPU write count
#define USB_WCEPN_WRTCNT	(0x7F<<16)		// Write count




/*
 * GDMA bit definitions
 */

#define DCON_RE			0x01

#define	DCON_MODE_SOFT		(0x0 << 1)
#define	DCON_MODE_EXT		(0x1 << 1)
#define	DCON_MODE_HTX		(0x2 << 1)
#define	DCON_MODE_HRX		(0x3 << 1)
#define	DCON_MODE_DIN		(0x4 << 1)
#define	DCON_MODE_DOUT		(0x5 << 1)

#define	DCON_SB_SINGLE		0x00
#define	DCON_SB_BLOCK		0x08

#define	DCON_FB_ENABLE		0x20

#define	DCON_TS_8		(0x0 << 6)
#define	DCON_TS_16		(0x1 << 6)
#define	DCON_TS_32		(0x2 << 6)

#define	DCON_SD_INC		(0x0 << 8)
#define	DCON_SD_DEC		(0x1 << 8)
#define	DCON_SD_FIXED		(0x2 << 8)

#define	DCON_DD_INC		(0x0 << 10)
#define	DCON_DD_DEC		(0x1 << 10)
#define	DCON_DD_FIXED		(0x2 << 10)

#define	DCON_IE			(0x1 << 12)
#define	DCON_BS			(0x1 << 31)

