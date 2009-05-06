#==========================================================================
#
#   File: Makefile
#
#   General Description:
#               Makefile for wlan.o, wlanconfig & wlancis
#
#==========================================================================
#               Motorola Confidential Proprietary
#               (c) Copyright Motorola 2005, All Rights Reserved
#==========================================================================

EXTRA_CFLAGS    += $(SD83XX_CFLAGS)
EXTRA_CFLAGS    += -I. -I../os/linux -I../io/sdio/pxa270 -I../if/if_sdio

list-multi := wlan.o

wlan-objs :=	wlan_main.o wlan_fw.o wlan_wext.o \
		wlan_rx.o wlan_tx.o \
		wlan_cmd.o wlan_cmdresp.o \
		wlan_proc.o

obj-m := wlan.o

wlan.o: $(wlan-objs)
	$(LD) -r -o $@ $(wlan-objs)

include $(TOPDIR)/Rules.make

.PHONY: modules
modules: 
ifeq ($(CONFIG_BULVERDE), y)
		$(CC) -DDEBUG -DWLANCONFIG -Wall -Wstrict-prototypes \
			-DDEEP_SLEEP -DCAL_DATA \
			-O2 -o wlanconfig wlan_config.c 
endif

	$(CC) -Wall -Wstrict-prototypes -static -s -O2 -o wlancis wlancis.c
