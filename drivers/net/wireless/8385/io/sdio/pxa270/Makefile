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

ifeq ($(CONFIG_WPRM_DRV),y)
    EXTRA_CFLAGS += -DWPRM_DRV
endif

ifeq ($(CONFIG_DMA_NO_WAIT),y)
    EXTRA_CFLAGS += -DDMA_NO_WAIT
endif

EXTRA_CFLAGS += -D__KERNEL__ -DMODULE  -DLNDIS_SDIO -DDEBUG \
		-I$(KERNELDIR)/include \
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
	
