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

ifeq ($(CONFIG_WPRM_DRV),y)
	wlan-objs += wlan_wprm_drv.o
endif

ifeq ($(CONFIG_WMM), y)
	wlan-objs += wlan_wmm.o
endif

ifeq ($(CONFIG_802_11D), y)
	wlan-objs += wlan_11d.o
endif

ifeq ($(CONFIG_802_11H_TPC), y)
	wlan-objs += wlan_11h.o
endif

ifeq ($(CONFIG_CCX), y)
	wlan-objs += wlan_ccx.o
endif

ifeq ($(CONFIG_PROC_DEBUG), y)
	wlan-objs += wlan_debug.o
endif

ifeq ($(CONFIG_READ_HELPER_FW), y)
	wlan-objs += wlan_fops.o
endif

obj-m := wlan.o

wlan.o: $(wlan-objs)
	$(LD) -r -o $@ $(wlan-objs)

include $(TOPDIR)/Rules.make

ifeq ($(CONFIG_BG_SCAN), y)
	TOOL_CFLAGS += -DBG_SCAN -DTLV_SCAN
endif

ifeq ($(CONFIG_EXTSCAN), y)
	TOOL_CFLAGS += -DEXTSCAN
endif

ifeq ($(CONFIG_CAL_DATA), y)
	TOOL_CFLAGS += -DCAL_DATA
endif

ifeq ($(CONFIG_WMM), y)
	TOOL_CFLAGS += -DWMM
endif

ifeq ($(CONFIG_SUBSCRIBE_EVENT_CTRL), y)
	TOOL_CFLAGS += -DSUBSCRIBE_EVENT_CTRL
endif

ifeq ($(CONFIG_CIPHER_TEST), y)
	TOOL_CFLAGS += -DCIPHER_TEST
endif

.PHONY: modules
modules: 
ifeq ($(CONFIG_BULVERDE), y)
		$(CC) $(TOOL_CFLAGS) -DDEBUG -DWLANCONFIG -Wall -Wstrict-prototypes \
			-DDEEP_SLEEP \
			-I$(shell pwd)/../os/linux \
			-O2 -o wlanconfig wlan_config.c 
endif

	$(CC) $(TOOL_CFLAGS) -Wall -Wstrict-prototypes -static -s -O2 -o wlancis wlancis.c
