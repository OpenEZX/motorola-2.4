#==========================================================================
#
#   File: Makefile
#
#   General Description:
#               Makefile for sd83xx mfg driver
#
#==========================================================================
#               Motorola Confidential Proprietary
#               (c) Copyright Motorola 2005, All Rights Reserved
#==========================================================================

#       File    : Makefile
#               : Marvell Semiconductor Inc
#               : Copyright (c) 2005, Marvell Semiconductor Inc


CONFIG_SD8385=y
KERNELDIR=$(EZX_LINUX_BASE)/linux/linux-2.4.17

# Specify Kernel source tree by Wireless Extension
# Valid options for Bulverde: 14, 15, 16
# Valid options for Omap1510: 15, 16
KERNEL_WE=16

CONFIG_MFG=y

ifeq ($(CONFIG_SD8385),y)
CONFIG_WPRM_DRV=y
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

# Compile for Version 3 firmware
CONFIG_VER3=y

# Intel Bulverde Mainstone Register
CONFIG_MAINSTONE=n

# Enable Ad-Hoc mode Power Save support
CONFIG_AD_HOC_PS=y

# Enable Power Save support
CONFIG_PS=y

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
CONFIG_MARVELL_PM=y

# Auto Frequency Control
CONFIG_AFC=n

# Reassociation done by the user 
CONFIG_USER_DOES_PS_AFTER_REASSOCIATION=y

# Enable this to start the TX/RX Throughput Tests 
CONFIG_THROUGHPUT_TEST=n

#ENABLE 802.11h
CONFIG_802_11H_TPC=n

#Enable Background scan
CONFIG_BG_SCAN=n

#enable V4_CMDS
CONFIG_V4_CMDS=n

# SD8385 flags
ifeq ($(CONFIG_SD8385),y)
CONFIG_MULTI_BANDS=y
CONFIG_G_RATE=n
CONFIG_NEW_ASSOCIATE_RSP=y
endif

# *** End of Configuration section ***

SD83XX_CFLAGS = -D__KERNEL__ -DMODULE \
		-O2 -Wall -Wstrict-prototypes 

ifeq ($(CONFIG_BULVERDE), y)
	SD83XX_CFLAGS += -DBULVERDE
	export CONFIG_BULVERDE
	# Don't do duplicate copying
	SD83XX_CFLAGS += -DNOMEMCOPY
	# Enable Deep Sleep
	CONFIG_DEEP_SLEEP=y
	include $(KERNELDIR)/.config
endif

SD83XX_CFLAGS += -I$(KERNELDIR)/include

ifeq ($(CONFIG_RF8015),y)
	SD83XX_CFLAGS += -DRF8015
endif

ifeq ($(CONFIG_G_RATE),y)
	SD83XX_CFLAGS += -DG_RATE
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

ifeq ($(CONFIG_WPA2),y)
	CONFIG_WPA=y
endif

ifeq ($(CONFIG_WPA),y)
	ifeq ($(CONFIG_ADHOCAES),y)
		SD83XX_CFLAGS += -DADHOCAES
	endif
endif

ifeq ($(CONFIG_CAL_DATA),y)
	SD83XX_CFLAGS += -DCAL_DATA
endif

ifeq ($(CONFIG_MARVELL_PM),y)
	SD83XX_CFLAGS += -DCONFIG_MARVELL_PM
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

ifeq ($(CONFIG_802_11D), y)
        SD83XX_CFLAGS += -DENABLE_802_11D
	CONFIG_TLV_SCAN=y
	export CONFIG_802_11D
endif

ifeq ($(CONFIG_TLV_SCAN), y)
 	SD83XX_CFLAGS += -DTLV_SCAN
endif

ifeq ($(CONFIG_BG_SCAN), y)
 	SD83XX_CFLAGS += -DBG_SCAN
	CONFIG_TLV_SCAN=y
	export CONFIG_BG_SCAN
endif

ifeq ($(CONFIG_V4_CMDS), y)
 	SD83XX_CFLAGS += -DV4_CMDS
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

ifeq ($(CONFIG_MFG),y)
	SD83XX_CFLAGS += -DMFG_CMD_SUPPORT
endif

#SD83XX_CFLAGS += -DTX_THREAD
#SD83XX_CFLAGS += -DTXRX_DEBUG

ifeq ($(CONFIG_WPRM_DRV),y)
	SD83XX_CFLAGS += -DWPRM_DRV
	export CONFIG_WPRM_DRV
endif

export  SD83XX_CFLAGS
export  KERNELDIR

EXTRA_CFLAGS += $(SD83XX_CFLAGS)

subdir-$(CONFIG_MARVELL_WIFI) += wlan if/if_sdio
subdir-$(CONFIG_MARVELL_SDIO) += io/sdio/pxa270

include $(TOPDIR)/Rules.make

.PHONY: modules
modules:
	$(LD) -r -o sd83xx_mfg.o wlan/wlan.o if/if_sdio/if_sdio.o
