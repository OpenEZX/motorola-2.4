/*
 *  linux/include/asm-arm/arch-pxa/serial.h
 * 
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#define BAUD_BASE	921600

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE	3

#ifdef	_DEBUG_BOARD
#define STD_SERIAL_PORT_DEFNS	\
	{	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:              BAUD_BASE,      \		
		iomem_base:		&BTUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_BTUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&FFUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_FFUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&STUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_STUART,	\
		flags:			STD_COM_FLAGS,	\
	}
#else
#define STD_SERIAL_PORT_DEFNS	\
	{	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&HWUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_HWUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&FFUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_FFUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		64,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		&BTUART,	\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM,	\
		irq:			IRQ_BTUART,	\
		flags:			STD_COM_FLAGS,	\
	}
#endif
#define EXTRA_SERIAL_PORT_DEFNS




