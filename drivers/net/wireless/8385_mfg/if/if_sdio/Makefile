#==========================================================================
#
#   File: Makefile
#
#   General Description:
#               Makefile for if_sdio.o
#
#==========================================================================
#               Motorola Confidential Proprietary
#               (c) Copyright Motorola 2005, All Rights Reserved
#==========================================================================

EXTRA_CFLAGS    += $(SD83XX_CFLAGS)
EXTRA_CFLAGS    += -I../../wlan -I../../os/linux -I. 

ifeq ($(CONFIG_BULVERDE),y)
    EXTRA_CFLAGS += -I../../io/sdio/pxa270
endif

obj-m := if_sdio.o

include $(TOPDIR)/Rules.make
