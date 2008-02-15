#   File: Makefile
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

CONFIG_SD8385=y

# Specify Kernel source tree by Wireless Extension
# Valid options for Bulverde: 14, 15, 16
KERNEL_WE=16

ifeq ($(CONFIG_SD8385),y)
CONFIG_WPRM_DRV=y
CONFIG_WPRM_DRV_TRACING=n
endif

ifeq ($(CONFIG_SD8385),y)
CONFIG_RF8015=y
endif

ifeq ($(CONFIG_RF8015),y)
export CONFIG_RF8015
endif

ifeq ($(CONFIG_SD8385),y)
CONFIG_BULVERDE=y
endif

# Enable debugging?
# DEBUG LEVEL n/0/1/2:
# n: NO DEBUG
# 0: Only PRINTK
# 1: PRINTK and PRINTK1
# 2: PRINTK, PRINTK1, ENTER and LEAVE
CONFIG_DEBUG=n

#RR scheduling policy
CONFIG_SCHED_RR=y

# Compile for Version 3 firmware
CONFIG_VER3=y

# Intel Bulverde Mainstone Register
CONFIG_MAINSTONE=n

# Enable Ad-Hoc mode Power Save support
CONFIG_AD_HOC_PS=y

# Enable Power Save support
CONFIG_PS=y
CONFIG_PS_PRESLEEP=y
CONFIG_FW_WAKEUP_METHOD=y
CONFIG_FW_WAKEUP_TIME=n

# Enable Bus Powersave
CONFIG_BUS_POWERSAVE=y

# Enable DEEP SLEEP CMD
CONFIG_DEEP_SLEEP_CMD=y

# Enable Host Wakeup and New Power Save
CONFIG_HOST_WAKEUP=y

# Enable WPA
CONFIG_WPA=y

# Enable WPA2
CONFIG_WPA2=y

# Enable 802.11d
CONFIG_802_11D=y

# Enable TLV_SCAN
CONFIG_TLV_SCAN=y

# Configure New Associate
CONFIG_NEW_ASSOCIATE=y

# Configure New Associate Response
CONFIG_NEW_ASSOCIATE_RSP=n

# Configure Helper Image 
CONFIG_HELPER_IMAGE=y

#To print a dump of Tx and Packet sent
CONFIG_TX_PRINT_DEBUG=n

#To print a dump of Rx and Packet sent
CONFIG_RX_PRINT_DEBUG=n

#To include progressive scan
CONFIG_PROGRESSIVE_SCAN=y

# BlueTooth Co-existence Arbitration (BCA) Configuration
CONFIG_BCA=y

# PCB Rev 4 Boards 
CONFIG_PCB_REV4=n
		
# Advanced Encryption Standard
CONFIG_ADHOCAES=y

# RF calibration
CONFIG_CAL_DATA=y

# Linux Dynamic Power Management
CONFIG_PM=y

# Auto Frequency Control
CONFIG_AFC=y

# Reassociation done by the user 
CONFIG_USER_DOES_PS_AFTER_REASSOCIATION=y

# Enable this to start the TX/RX Throughput Tests 
CONFIG_THROUGHPUT_TEST=n

#ENABLE 802.11h
CONFIG_802_11H_TPC=n

# Enable CCX Support
CONFIG_CCX = n

#enable TLV_ASSOCIATION
CONFIG_TLV_ASSOCIATION=y

#Enable Background scan
CONFIG_BG_SCAN=y

#Enable specific scan
CONFIG_EXTSCAN=y

# Enable LEDs Control
CONFIG_LED_GPIO_CTRL=y

# Enable SUBSCRIBE_EVENT
CONFIG_SUBSCRIBE_EVENT_CTRL=y

#Enable UAPSD
CONFIG_WMM_UAPSD=y

#Enable WMM
CONFIG_WMM=y

# Enable WMM AIFS
CONFIG_WMM_AIFS=y

#Enable DMA Performance
CONFIG_DMA_NO_WAIT=y

# Enable debug counters in /proc/net/wlan
CONFIG_PROC_DEBUG=n

#Enable SLIDING_WIN_AVG for SNR/NF/RSSI
CONFIG_SLIDING_WIN_AVG=y

#Enable Proc read firmware
CONFIG_PROC_READ_FIRMWARE=y

CONFIG_ATIMGEN=y

CONFIG_CIPHER_TEST=n

# Enable FATAL_ERROR_HANDLE
CONFIG_FATAL_ERROR_HANDLE=y

# SD8385 flags
ifeq ($(CONFIG_SD8385),y)
CONFIG_MULTI_BANDS=y
CONFIG_G_RATE=n
CONFIG_ADHOC_GRATE=y
CONFIG_NEW_ASSOCIATE_RSP=y
CONFIG_AUTO_TX=y
endif

# Manufacturing firmware support
CONFIG_MANF_CMD_SUPPORT=y

# *** End of Configuration section ***

SD83XX_CFLAGS = -D__KERNEL__ -DMODULE \
		-O2 -Wall -Wstrict-prototypes 

ifeq ($(CONFIG_BULVERDE), y)
	SD83XX_CFLAGS += -DBULVERDE_SDIO
	export CONFIG_BULVERDE
	# Don't do duplicate copying
	SD83XX_CFLAGS += -DNOMEMCOPY
	# Enable Deep Sleep
	CONFIG_DEEP_SLEEP=y
	include $(TOPDIR)/.config
endif

SD83XX_CFLAGS += -I$(TOPDIR)/include

ifeq ($(CONFIG_RF8015),y)
	SD83XX_CFLAGS += -DRF8015
endif

ifeq ($(CONFIG_G_RATE),y)
	SD83XX_CFLAGS += -DG_RATE
endif

ifeq ($(CONFIG_ADHOC_GRATE), y)
	SD83XX_CFLAGS += -DADHOC_GRATE
endif

ifeq ($(CONFIG_VER3),y)
	SD83XX_CFLAGS += -DFWVERSION3
endif

ifeq ($(KERNEL_WE),14)
	CONFIG_WPA=n
	CONFIG_WPA2=n
endif

ifeq ($(CONFIG_WPA),y)
	SD83XX_CFLAGS += -DWPA
endif

ifeq ($(CONFIG_WPA2),y)
	SD83XX_CFLAGS += -DWPA2
endif

ifeq ($(CONFIG_PS),y)
	SD83XX_CFLAGS += -DPS_REQUIRED
ifeq ($(CONFIG_PS_PRESLEEP),y)
	SD83XX_CFLAGS += -DPS_PRESLEEP
endif
endif

ifeq ($(CONFIG_BUS_POWERSAVE),y)
	SD83XX_CFLAGS += -DBUS_POWERSAVE
endif

ifeq ($(CONFIG_THROUGHPUT_TEST),y)
	SD83XX_CFLAGS += -DTHROUGHPUT_TEST
endif

ifeq ($(CONFIG_USER_DOES_PS_AFTER_REASSOCIATION),y)
	SD83XX_CFLAGS += -DUSER_DOES_PS_AFTER_REASSOCIATION
endif

ifeq ($(CONFIG_AD_HOC_PS),y)
	SD83XX_CFLAGS += -DAD_HOC_PS
endif

ifeq ($(CONFIG_DEEP_SLEEP_CMD),y)
	SD83XX_CFLAGS += -DDEEP_SLEEP_CMD
endif

ifeq ($(CONFIG_DEEP_SLEEP),y)
	SD83XX_CFLAGS += -DDEEP_SLEEP
endif

ifeq ($(CONFIG_HOST_WAKEUP),y)
	SD83XX_CFLAGS += -DHOST_WAKEUP
ifeq ($(CONFIG_FW_WAKEUP_METHOD),y)
	SD83XX_CFLAGS += -DFW_WAKEUP_METHOD
endif
ifeq ($(CONFIG_FW_WAKEUP_TIME),y)
	SD83XX_CFLAGS += -DFW_WAKEUP_TIME
endif
endif

ifeq ($(CONFIG_BCA),y)
	SD83XX_CFLAGS += -DBCA
endif

ifeq ($(CONFIG_NEW_ASSOCIATE),y)
	SD83XX_CFLAGS += -DNEW_ASSOCIATION
endif

ifeq ($(CONFIG_NEW_ASSOCIATE_RSP),y)
	SD83XX_CFLAGS += -DNEW_ASSOCIATION_RSP
endif

ifeq ($(CONFIG_HELPER_IMAGE),y)
	SD83XX_CFLAGS += -DHELPER_IMAGE
endif

ifeq ($(CONFIG_SD8385),y)
	SD83XX_CFLAGS += -DSD8385
	export CONFIG_SD8385
endif

ifeq ($(CONFIG_TX_PRINT_DEBUG),y)
	SD83XX_CFLAGS += -DTX_PRINT_DEBUG
endif

ifeq ($(CONFIG_RX_PRINT_DEBUG),y)
	SD83XX_CFLAGS += -DRX_PRINT_DEBUG
endif

# Send dummy packets to indicate transmit done
#	SD83XX_CFLAGS += -DDUMMY_PACKET

ifeq ($(CONFIG_PROGRESSIVE_SCAN),y)
	SD83XX_CFLAGS += -DPROGRESSIVE_SCAN
endif

ifeq ($(CONFIG_PCB_REV4),y)
	SD83XX_CFLAGS += -DPCB_REV4
endif

ifeq ($(CONFIG_WPA),y)
	ifeq ($(CONFIG_ADHOCAES),y)
		SD83XX_CFLAGS += -DADHOCAES
	endif
endif

ifeq ($(CONFIG_CAL_DATA),y)
	SD83XX_CFLAGS += -DCAL_DATA
endif

ifeq ($(CONFIG_PM),y)
	SD83XX_CFLAGS += -DCONFIG_PM
endif

ifeq ($(CONFIG_MULTI_BANDS),y)
	SD83XX_CFLAGS += -DMULTI_BANDS
endif

ifeq ($(CONFIG_AFC),y)
	SD83XX_CFLAGS += -DAUTO_FREQ_CTRL
endif

ifeq ($(CONFIG_802_11H_TPC), y)
	CONFIG_802_11D=y
	SD83XX_CFLAGS += -DENABLE_802_11H_TPC
	export CONFIG_802_11H_TPC
endif

ifeq ($(CONFIG_CCX), y)
	SD83XX_CFLAGS += -DCCX -DIWGENIE_SUPPORT -DTIMESTAMP_SCAN_RESULTS
	SD83XX_CFLAGS += -DREASSOCIATION_SUPPORT
	export CONFIG_CCX
endif

ifeq ($(CONFIG_802_11D), y)
        SD83XX_CFLAGS += -DENABLE_802_11D
	CONFIG_TLV_SCAN=y
	export CONFIG_802_11D
endif

ifeq ($(CONFIG_BG_SCAN), y)
 	SD83XX_CFLAGS += -DBG_SCAN
	CONFIG_TLV_SCAN=y
        export CONFIG_BG_SCAN
endif

ifeq ($(CONFIG_EXTSCAN), y)
 	SD83XX_CFLAGS += -DEXTSCAN
        export CONFIG_EXTSCAN
endif

ifeq ($(CONFIG_TLV_SCAN), y)
 	SD83XX_CFLAGS += -DTLV_SCAN
        export CONFIG_TLV_SCAN
endif

ifeq ($(CONFIG_WMM_UAPSD), y)
        SD83XX_CFLAGS += -DWMM_UAPSD
	CONFIG_WMM=y
        export CONFIG_WMM_UAPSD
endif

ifeq ($(CONFIG_WMM), y)
 	SD83XX_CFLAGS += -DWMM
	export CONFIG_WMM
endif

ifeq ($(CONFIG_WMM_AIFS), y)
 	SD83XX_CFLAGS += -DWMM_AIFS
	export CONFIG_WMM_AIFS
endif

ifeq ($(CONFIG_TLV_ASSOCIATION), y)
   SD83XX_CFLAGS +=  -DTLV_ASSOCIATION
endif

ifeq ($(CONFIG_LED_GPIO_CTRL),y)
 	SD83XX_CFLAGS += -DLED_GPIO_CTRL
endif

ifeq ($(CONFIG_SUBSCRIBE_EVENT_CTRL),y)
 	SD83XX_CFLAGS +=  -DSUBSCRIBE_EVENT_CTRL
	export CONFIG_SUBSCRIBE_EVENT_CTRL
endif

ifeq ($(CONFIG_ATIMGEN),y)
 	SD83XX_CFLAGS += -DATIMGEN
endif

ifeq ($(CONFIG_PROC_DEBUG),y)
 	SD83XX_CFLAGS +=  -DPROC_DEBUG
	export CONFIG_PROC_DEBUG
endif

ifeq ($(CONFIG_CIPHER_TEST),y)
 	SD83XX_CFLAGS +=  -DCIPHER_TEST
	export CONFIG_CIPHER_TEST
endif


ifdef CONFIG_SMP
   SD83XX_CFLAGS += -D__SMP__ -DSMP
endif

ifneq ($(CONFIG_DEBUG),n)
	SD83XX_CFLAGS += -DDEBUG_VER
endif

ifeq ($(CONFIG_DEBUG),0)
	CONFIG_DEBUG=y
	SD83XX_CFLAGS += -DDEBUG_LEVEL0
endif

ifeq ($(CONFIG_DEBUG),1)
	CONFIG_DEBUG=y
	SD83XX_CFLAGS += -DDEBUG_LEVEL0
	SD83XX_CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_DEBUG),2)
	CONFIG_DEBUG=y
	SD83XX_CFLAGS += -DDEBUG_LEVEL0
	SD83XX_CFLAGS += -DDEBUG_LEVEL1
	SD83XX_CFLAGS += -DDEBUG_LEVEL2
endif

ifeq ($(CONFIG_SLIDING_WIN_AVG),y)
	SD83XX_CFLAGS += -DSLIDING_WIN_AVG
endif

ifeq ($(CONFIG_PROC_READ_FIRMWARE),y)
	SD83XX_CFLAGS += -DPROC_READ_FIRMWARE
	export CONFIG_PROC_READ_FIRMWARE
endif

ifeq ($(CONFIG_MANF_CMD_SUPPORT),y)
	SD83XX_CFLAGS += -DMANF_CMD_SUPPORT
endif

ifeq ($(CONFIG_AUTO_TX),y)
	SD83XX_CFLAGS += -DAUTO_TX
	export CONFIG_AUTO_TX
endif

ifeq ($(CONFIG_SCHED_RR),y)
 	SD83XX_CFLAGS += -DDRIVER_SCHED_RR
endif

#SD83XX_CFLAGS += -DTX_THREAD
#SD83XX_CFLAGS += -DTXRX_DEBUG

ifeq ($(CONFIG_WPRM_DRV),y)
	SD83XX_CFLAGS += -DWPRM_DRV
	export CONFIG_WPRM_DRV
endif

ifeq ($(CONFIG_DMA_NO_WAIT),y)
	SD83XX_CFLAGS += -DDMA_NO_WAIT
	export CONFIG_DMA_NO_WAIT
endif

ifeq ($(CONFIG_WPRM_DRV_TRACING),y)
	SD83XX_CFLAGS += -DWPRM_DRV_TRACING
	export CONFIG_WPRM_DRV_TRACING
endif

ifeq ($(CONFIG_FATAL_ERROR_HANDLE),y)
	SD83XX_CFLAGS += -DFATAL_ERROR_HANDLE
endif

export  SD83XX_CFLAGS

EXTRA_CFLAGS += $(SD83XX_CFLAGS)

subdir-$(CONFIG_MARVELL_WIFI) += wlan if/if_sdio
subdir-$(CONFIG_MARVELL_SDIO) += io/sdio/pxa270

include $(TOPDIR)/Rules.make

.PHONY: modules
modules:
	$(LD) -r -o sd83xx.o wlan/wlan.o if/if_sdio/if_sdio.o
