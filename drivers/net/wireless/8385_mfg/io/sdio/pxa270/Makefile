#==========================================================================
#
#   File: Makefile
#
#   General Description:
#               Makefile for sdio driver
#
#==========================================================================
#               Motorola Confidential Proprietary
#               (c) Copyright Motorola 2005, All Rights Reserved
#==========================================================================
EXTRA_CFLAGS += -Wstrict-prototypes -Wno-trigraphs \
		-fno-strict-aliasing -fno-common -pipe \
		-mapcs-32 -msoft-float -mshort-load-bytes -Uarm \
		-D__LINUX_ARM_ARCH__=5 -mcpu=xscale -mtune=xscale \
		-D__KERNEL__ -DDEBUG -DMODULE -DEXPORT_SYMTAB \
		-D__LINUX -DLINUX_STACK -D_USE_SOCKPACKET

ifeq ($(CONFIG_SD8385),y)
    EXTRA_CFLAGS += -DSD8385
endif

EXTRA_CFLAGS += -D__KERNEL__ -DMODULE  -DLNDIS_SDIO -DDEBUG \
		-DIRQ_DEBUG \
		-I$(KERNELDIR)/include \
		-I$(shell pwd) -O2 -Wall

obj-m := sdio.o

include $(TOPDIR)/Rules.make

.PHONY: modules
modules: 
	mv sdio.o sdio_mfg.o
