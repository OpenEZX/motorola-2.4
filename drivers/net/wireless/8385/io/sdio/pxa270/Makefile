# File: Makefile
#
# (c) Copyright © 2003-2006, Marvell International Ltd. 
#
# (c) Copyright 2006, Motorola, Inc.
#
# This software file (the "File") is distributed by Marvell International 
# Ltd. under the terms of the GNU General Public License Version 2, June 1991 
# (the "License").  You may use, redistribute and/or modify this File in 
# accordance with the terms and conditions of the License, a copy of which 
# is available along with the File in the license.txt file or by writing to 
# the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
# 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
# ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
# this warranty disclaimer.
#
#
# 2006-Jul-07 Motorola - Add changes for Motorola specific make environment.
# 2006-Aug-16 Motorola - Change KERNELDIR to TOPDIR
#
EXTRA_CFLAGS += -Wstrict-prototypes -Wno-trigraphs \
		-fno-strict-aliasing -fno-common -pipe \
		-mapcs-32 -msoft-float -mshort-load-bytes -Uarm \
		-D__LINUX_ARM_ARCH__=5 -mcpu=xscale -mtune=xscale \
		-D__KERNEL__ -DDEBUG -DMODULE -DEXPORT_SYMTAB \
		-D__LINUX -DLINUX_STACK -D_USE_SOCKPACKET

ifeq ($(CONFIG_SD8385),y)
    EXTRA_CFLAGS += -DSD8385
endif

ifeq ($(CONFIG_WPRM_DRV),y)
    EXTRA_CFLAGS += -DWPRM_DRV
endif

ifeq ($(CONFIG_DMA_NO_WAIT),y)
    EXTRA_CFLAGS += -DDMA_NO_WAIT
endif

EXTRA_CFLAGS += -D__KERNEL__ -DMODULE  -DLNDIS_SDIO -DDEBUG \
		-I$(TOPDIR)/include \
		-I$(shell pwd) -O2 -Wall

obj-m := sdio_t.o

ifeq ($(CONFIG_WPRM_DRV),y)
     obj-m += sdio_wprm_drv.o
endif

include $(TOPDIR)/Rules.make

sdio_t.o: sdio.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS_nostdinc) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) -c -o $@ $<

.PHONY: modules

modules:
ifeq ($(CONFIG_WPRM_DRV),y)
	$(LD) -r -o sdio.o sdio_t.o sdio_wprm_drv.o
else
	cp sdio_t.o sdio.o
endif
	
