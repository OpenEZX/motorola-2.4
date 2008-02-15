/*
 *  linux/include/asm-arm/arch-pxa/memory.h
 * 
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/*
 * We don't have highmem on ARM (for now), so TASK_SIZE is always
 * going to be equals to the kernel base.
 */

#define TASK_SIZE	((unsigned long)(CONFIG_KERNEL_START & 0xffc00000))
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

#define PAGE_OFFSET	((unsigned long)(CONFIG_KERNEL_START & 0xffc00000))

/*
 * Physical DRAM offset.
 */
#define PHYS_OFFSET	(0xa0000000UL)
#define PHYS_TO_NID(addr)	(0)

/*
 * physical vs virtual ram conversion
 */
#define __virt_to_phys__is_a_macro
#define __phys_to_virt__is_a_macro
#define __virt_to_phys(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(x)	((x) - PHYS_OFFSET + PAGE_OFFSET)

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *		address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *		to an address that the kernel can use.
 */
#define __virt_to_bus__is_a_macro
#define __bus_to_virt__is_a_macro
#define __virt_to_bus(x)	 __virt_to_phys(x)
#define __bus_to_virt(x)	 __phys_to_virt(x)

/*
 * by zxf, 12/18/02
 * for exiting sleep and user off mode
 */
#define FLAG_ADDR	PHYS_OFFSET
#define RESUME_ADDR	(PHYS_OFFSET + 4)
#define BPSIG_ADDR	(PHYS_OFFSET + 8)

#define USER_OFF_FLAG	0x5a5a5a5a
#define SLEEP_FLAG	0x6b6b6b6b
#define OFF_FLAG	0x7c7c7c7c
#define REFLASH_FLAG	0x0C1D2E3F

#define WDI_FLAG	0xbb00dead
#define NO_FLAG		0xaa00dead

#endif
