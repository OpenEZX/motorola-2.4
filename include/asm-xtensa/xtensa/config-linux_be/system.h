#ifndef XTENSA_CONFIG_SYSTEM_H
#define XTENSA_CONFIG_SYSTEM_H

/*
 * THIS FILE IS GENERATED -- DO NOT MODIFY BY HAND
 *
 * include/asm-xtensa/xtensa/config/system.h -- HAL definitions that
 * are dependent on SYSTEM configuration.
 * 
 * Source for configuration-independent binaries (which link in a
 * configuration-specific HAL library) must NEVER include this file.
 * The HAL itself has historically included this file in some
 * instances, but this is not appropriate either because the HAL is
 * meant to be core-specific but system independent.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file "COPYING" in the main directory of
 * this archive for more details.
 *
 * Copyright (C) 2002 Tensilica Inc.
 */


/*#include <xtensa/hal.h>*/



/*----------------------------------------------------------------------
				DEVICE ADDRESSES
  ----------------------------------------------------------------------*/

/*
 *  Strange place to find these, but the configuration GUI
 *  allows moving these around to account for various core
 *  configurations.  Specific boards (and their BSP software)
 *  will have specific meanings for these components.
 */

/*  I/O Block areas:  */
#define XSHAL_IOBLOCK_CACHED_VADDR	0xE0000000
#define XSHAL_IOBLOCK_CACHED_PADDR	0xF0000000
#define XSHAL_IOBLOCK_CACHED_SIZE	0x0E000000

#define XSHAL_IOBLOCK_BYPASS_VADDR	0xF0000000
#define XSHAL_IOBLOCK_BYPASS_PADDR	0xF0000000
#define XSHAL_IOBLOCK_BYPASS_SIZE	0x0E000000

#if 0
#define XSHAL_ETHER_VADDR	0xFD030000
#define XSHAL_ETHER_PADDR	0xFD030000
#define XSHAL_UART_VADDR	0xFD050000
#define XSHAL_UART_PADDR	0xFD050000
#define XSHAL_LED_VADDR		0xFD040000
#define XSHAL_LED_PADDR		0xFD040000
#define XSHAL_FLASH_VADDR	0xF8000000
#define XSHAL_FLASH_PADDR	0xF8000000
#define XSHAL_FLASH_SIZE	0x04000000
#endif /*0*/

/*  System ROM:  */
#define XSHAL_ROM_VADDR		0xEE000000
#define XSHAL_ROM_PADDR		0xFE000000
#define XSHAL_ROM_SIZE		0x00400000
/*  Largest available area (free of vectors):  */
#define XSHAL_ROM_AVAIL_VADDR	0xEE00052C
#define XSHAL_ROM_AVAIL_VSIZE	0x003FFAD4

/*  System RAM:  */
#define XSHAL_RAM_VADDR		0xD0000000
#define XSHAL_RAM_PADDR		0x00000000
#define XSHAL_RAM_VSIZE		0x08000000
#define XSHAL_RAM_PSIZE		0x10000000
#define XSHAL_RAM_SIZE		XSHAL_RAM_PSIZE
/*  Largest available area (free of vectors):  */
#define XSHAL_RAM_AVAIL_VADDR	0xD0000370
#define XSHAL_RAM_AVAIL_VSIZE	0x07FFFC90

/*
 *  Shadow system RAM (same device as system RAM, at different address).
 *  (Emulation boards need this for the SONIC Ethernet driver
 *   when data caches are configured for writeback mode.)
 *  NOTE: on full MMU configs, this points to the BYPASS virtual address
 *  of system RAM, ie. is the same as XSHAL_RAM_* except that virtual
 *  addresses are viewed through the BYPASS static map rather than
 *  the CACHED static map.
 */
#define XSHAL_RAM_BYPASS_VADDR		0xD8000000
#define XSHAL_RAM_BYPASS_PADDR		0x00000000
#define XSHAL_RAM_BYPASS_PSIZE		0x08000000

/*  Alternate system RAM (different device than system RAM):  */
#define XSHAL_ALTRAM_VADDR		0xCFA00000
#define XSHAL_ALTRAM_PADDR		0xC0000000
#define XSHAL_ALTRAM_SIZE		0x00200000


/*----------------------------------------------------------------------
			DEVICE-ADDRESS DEPENDENT...
  ----------------------------------------------------------------------*/

/*
 *  Values written to CACHEATTR special register (or its equivalent)
 *  to enable and disable caches in various modes:
 */
#define XSHAL_CACHEATTR_WRITEBACK	0x22FFFFF1	/* enable caches in write-back mode */
#define XSHAL_CACHEATTR_WRITEALLOC	0x22FFFFF1	/* enable caches in write-allocate mode */
#define XSHAL_CACHEATTR_WRITETHRU	0x22FFFFF1	/* enable caches in write-through mode */
#define XSHAL_CACHEATTR_BYPASS		0x22FFFFF2	/* disable caches in bypass mode */
#define XSHAL_CACHEATTR_DEFAULT		XSHAL_CACHEATTR_WRITETHRU	/* default setting to enable caches */

/*----------------------------------------------------------------------
			ISS (Instruction Set Simulator) SPECIFIC ...
  ----------------------------------------------------------------------*/

#define XSHAL_ISS_CACHEATTR_WRITEBACK	0x1122222F	/* enable caches in write-back mode */
#define XSHAL_ISS_CACHEATTR_WRITEALLOC	0x1122222F	/* enable caches in write-allocate mode */
#define XSHAL_ISS_CACHEATTR_WRITETHRU	0x1122222F	/* enable caches in write-through mode */
#define XSHAL_ISS_CACHEATTR_BYPASS	0x2222222F	/* disable caches in bypass mode */
#define XSHAL_ISS_CACHEATTR_DEFAULT	XSHAL_ISS_CACHEATTR_WRITEBACK	/* default setting to enable caches */

#define XSHAL_ISS_PIPE_REGIONS	0
#define XSHAL_ISS_SDRAM_REGIONS	0


/*----------------------------------------------------------------------
			XT2000 BOARD SPECIFIC ...
  ----------------------------------------------------------------------*/

#define XSHAL_XT2000_CACHEATTR_WRITEBACK	0x21FFFFFF	/* enable caches in write-back mode */
#define XSHAL_XT2000_CACHEATTR_WRITEALLOC	0x21FFFFFF	/* enable caches in write-allocate mode */
#define XSHAL_XT2000_CACHEATTR_WRITETHRU	0x21FFFFFF	/* enable caches in write-through mode */
#define XSHAL_XT2000_CACHEATTR_BYPASS		0x22FFFFFF	/* disable caches in bypass mode */
#define XSHAL_XT2000_CACHEATTR_DEFAULT		XSHAL_XT2000_CACHEATTR_WRITEBACK	/* default setting to enable caches */

#define XSHAL_XT2000_PIPE_REGIONS	0x00001000	/* BusInt pipeline regions */
#define XSHAL_XT2000_SDRAM_REGIONS	0x00000005	/* BusInt SDRAM regions */


/*----------------------------------------------------------------------
				VECTOR SIZES
  ----------------------------------------------------------------------*/

/*
 *  Sizes allocated to vectors by the system (memory map) configuration.
 *  These sizes are constrained by core configuration (eg. one vector's
 *  code cannot overflow into another vector) but are dependent on the
 *  system or board (or LSP) memory map configuration.
 *
 *  Whether or not each vector happens to be in a system ROM is also
 *  a system configuration matter, sometimes useful, included here also:
 */
#define XSHAL_RESET_VECTOR_SIZE	0x000004E0
#define XSHAL_RESET_VECTOR_ISROM	1
#define XSHAL_USER_VECTOR_SIZE	0x0000001C
#define XSHAL_USER_VECTOR_ISROM	0
#define XSHAL_PROGRAMEXC_VECTOR_SIZE	XSHAL_USER_VECTOR_SIZE	/* for backward compatibility */
#define XSHAL_USEREXC_VECTOR_SIZE	XSHAL_USER_VECTOR_SIZE	/* for backward compatibility */
#define XSHAL_KERNEL_VECTOR_SIZE	0x0000001C
#define XSHAL_KERNEL_VECTOR_ISROM	0
#define XSHAL_STACKEDEXC_VECTOR_SIZE	XSHAL_KERNEL_VECTOR_SIZE	/* for backward compatibility */
#define XSHAL_KERNELEXC_VECTOR_SIZE	XSHAL_KERNEL_VECTOR_SIZE	/* for backward compatibility */
#define XSHAL_DOUBLEEXC_VECTOR_SIZE	0x000000E0
#define XSHAL_DOUBLEEXC_VECTOR_ISROM	0
#define XSHAL_WINDOW_VECTORS_SIZE	0x00000180
#define XSHAL_WINDOW_VECTORS_ISROM	0
#define XSHAL_INTLEVEL2_VECTOR_SIZE	0x0000000C
#define XSHAL_INTLEVEL2_VECTOR_ISROM	0
#define XSHAL_INTLEVEL3_VECTOR_SIZE	0x0000000C
#define XSHAL_INTLEVEL3_VECTOR_ISROM	0
#define XSHAL_INTLEVEL4_VECTOR_SIZE	0x0000000C
#define XSHAL_INTLEVEL4_VECTOR_ISROM	1
#define XSHAL_DEBUG_VECTOR_SIZE		XSHAL_INTLEVEL4_VECTOR_SIZE
#define XSHAL_DEBUG_VECTOR_ISROM	XSHAL_INTLEVEL4_VECTOR_ISROM


#endif /*XTENSA_CONFIG_SYSTEM_H*/

