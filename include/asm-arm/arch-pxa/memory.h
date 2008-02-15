/*
 *  linux/include/asm-arm/arch-pxa/memory.h
 * 
 * Copyright (C) 2004 Motorola 
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2004-Jul-21 - (Motorola) Changed definitions values for Motorola specific platform
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <linux/config.h>

/*
 * Task size: 3GB
 */
#define TASK_SIZE	(0xc0000000UL)
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

/*
 * Page offset: 3GB
 */
#define PAGE_OFFSET	(0xc0000000UL)

/*
 * Physical DRAM offset.
 */
#define PHYS_OFFSET	(0xa0000000UL)
//WUL remove for discontig
//#define PHYS_TO_NID(addr)	(0)

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

#ifdef CONFIG_DISCONTIGMEM
/*
 * Because of the wide memory address space between physical RAM banks on the
 * SA1100, it's much more convenient to use Linux's NUMA support to implement
 * our memory map representation.  Assuming all memory nodes have equal access
 * characteristics, we then have generic discontigous memory support.
 *
 * Of course, all this isn't mandatory for SA1100 implementations with only
 * one used memory bank.  For those, simply undefine CONFIG_DISCONTIGMEM.
 *
 * The nodes are matched with the physical memory bank addresses which are
 * incidentally the same as virtual addresses.
 *
 *      node 0:  0xc0000000 - 0xc7ffffff
 *      node 1:  0xc8000000 - 0xcfffffff
 *      node 2:  0xd0000000 - 0xd7ffffff
 *      node 3:  0xd8000000 - 0xdfffffff
 */ 

#define NR_NODES        16 //4

/*
 * Given a kernel address, find the home node of the underlying memory.
 */
#define KVADDR_TO_NID(addr) \
		(((unsigned long)(addr) - PAGE_OFFSET) >> NODE_MAX_MEM_SHIFT)

/*
 * Given a page frame number, convert it to a node id.
 */

#define PFN_TO_NID(pfn) \
	((pfn) - PHYS_PFN_OFFSET) >> (NODE_MAX_MEM_SHIFT - PAGE_SHIFT)

/*
 * Given a kaddr, ADDR_TO_MAPBASE finds the owning node of the memory
 * and returns the mem_map of that node.
 */
#define ADDR_TO_MAPBASE(kaddr) \
		NODE_MEM_MAP(KVADDR_TO_NID((unsigned long)(kaddr)))

#define PFN_TO_MAPBASE(pfn)     NODE_MEM_MAP(PFN_TO_NID(pfn))

/*
 * Given a kaddr, LOCAL_MAR_NR finds the owning node of the memory
 * and returns the index corresponding to the appropriate page in the
 * node's mem_map.
 */
#define LOCAL_MAP_NR(addr) \
	(((unsigned long)(addr) & (NODE_MAX_MEM_SIZE - 1)) >> PAGE_SHIFT)

/*
 * The E680 has two 32MB and 16MB DRAM areas 
 * them, so we use 26 for the node max shift to get 64MB node sizes.
 */
#define NODE_MAX_MEM_SHIFT      24 //16MB 26 //64MB   
#define NODE_MAX_MEM_SIZE       (1<<NODE_MAX_MEM_SHIFT)

#endif /* CONFIG_DISCONTIGMEM */

#endif