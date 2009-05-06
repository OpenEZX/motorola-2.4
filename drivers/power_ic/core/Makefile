# Copyright 2005-2006 - Motorola, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#
#  Author     Date                Comment
# ======== ===========   =====================================================
# Motorola 2005-Dec-06 - Minor modification to file names
# Motorola 2005-Mar-25 - Expanded power IC conditionals
# Motorola 2005-Feb-28 - File re-written from scratch
# Motorola 2006-Feb-10 - Add ArgonLV support
# Motorola 2006-Jul-04 - Remove hardcode from Macau source code.
# Motorola 2006-Aug-10 - Resolve all the OSS related issues. 
# Motorola 2006-Oct-25 - Update comments.

ifeq ($(CONFIG_CPU_BULVERDE),y)
    O_TARGET := power_ic_core.o

    export-objs := \
        event.o \
        external.o 

    obj-$(CONFIG_MOT_POWER_IC) := \
        core.o \
        event.o \
        external.o \
        thread.o

    obj-$(CONFIG_MOT_POWER_IC_PCAP2) += \
        eoc_register.o \
        fl_register.o \
        pcap_register.o \
        pxa_ssp.o \
        spi_main.o \
        spi_user.o

    obj-$(CONFIG_MOT_POWER_IC_ATLAS) += \
        atlas_register.o

    include $(TOPDIR)/Rules.make
endif

ifeq ($(CONFIG_ARCH_SCMA11),y)
    obj-$(CONFIG_MOT_POWER_IC) := \
        core.o \
        event.o \
        external.o  \
        thread.o

    obj-$(CONFIG_MOT_POWER_IC_ATLAS) += \
        atlas_register.o

ifeq ($(CONFIG_USR_BLK_DEV),y)
    obj-$(CONFIG_MOT_POWER_IC) += \
        sdhc_main.o \
        sdhc_user.o \
        usr_blk_dev.o
endif
endif

ifeq ($(CONFIG_MACH_ARGONPLUS_BUTE),y)
    obj-$(CONFIG_MOT_POWER_IC) := \
        core.o \
        event.o \
        external.o \
        thread.o
        
    obj-$(CONFIG_MOT_POWER_IC_ATLAS) += \
        atlas_register.o
endif

ifeq ($(CONFIG_MACH_ARGONLV_BUTE),y)
    obj-$(CONFIG_MOT_POWER_IC) := \
        core.o \
        event.o \
        external.o \
        thread.o
        
    obj-$(CONFIG_MOT_POWER_IC_ATLAS) += \
        atlas_register.o
endif

fastdep:
