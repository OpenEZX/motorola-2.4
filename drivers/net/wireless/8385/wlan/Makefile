#	File	: Makefile
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
# 2006-Aug-16 Motorola - Remove wlancis.c
#

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

ifeq ($(CONFIG_PROC_READ_FIRMWARE), y)
	wlan-objs += wlan_procfw.o
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

ifeq ($(CONFIG_AUTO_TX),y)
        TOOL_CFLAGS += -DAUTO_TX
endif

.PHONY: modules
modules: 
ifeq ($(CONFIG_BULVERDE), y)
		$(CC) $(TOOL_CFLAGS) -DDEBUG -DWLANCONFIG -Wall -Wstrict-prototypes \
			-DDEEP_SLEEP \
			-I$(shell pwd)/../os/linux \
			-O2 -o wlanconfig wlan_config.c 
endif
