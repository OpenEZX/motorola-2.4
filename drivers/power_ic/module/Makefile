# Copyright (C) 2004-2006  Motorola, Inc.
#
# Makefile for the power IC interface modules
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
# Motorola 2005-Dec-06 - Addition of Bute support
# Motorola 2005-Apr-04 - Addition of Atlas support
# Motorola 2005-Feb-28 - Addition of Charger.c
# Motorola 2004-Dec-17 - Initial Creation

ifeq ($(CONFIG_CPU_BULVERDE),y)
    O_TARGET := power_ic_module.o

    export-objs := \
       accy.o \
       atod.o \
       audio.o \
       backup_mem.o \
       charger.o \
       lights_funlights.o \
       lights_backlight.o \
       peripheral.o \
       rtc.o  

    obj-$(CONFIG_MOT_POWER_IC) := \
       accy.o \
       atod.o \
       audio.o \
       backup_mem.o \
       charger.o \
       debounce.o \
       emu_connected_state.o \
       emu_connecting_state.o \
       emu_disconnected_state.o \
       emu_disconnecting_state.o \
       emu_state_machine.o \
       emu_utils.o \
       lights_backlight.o \
       lights_funlights.o \
       peripheral.o \
       rtc.o 
       
    obj-$(CONFIG_MOT_POWER_IC_PCAP2) += \
       lights_funlights_pcap.o \
       power_management.o \
       tcmd_ioctl.o \
       touchscreen.o
       
    obj-$(CONFIG_MOT_POWER_IC_ATLAS) += \
       lights_funlights_atlas.o

    include $(TOPDIR)/Rules.make
endif

ifeq ($(CONFIG_ARCH_SCMA11),y)
    obj-$(CONFIG_MOT_POWER_IC_ATLAS) := \
       accy.o \
       atod.o \
       audio.o \
       backup_mem.o \
       brt.o \
       charger.o \
       debounce.o \
       emu_connected_state.o \
       emu_connecting_state.o \
       emu_disconnected_state.o \
       emu_disconnecting_state.o \
       emu_state_machine.o \
       emu_utils.o \
       lights_funlights.o \
       lights_funlights_atlas.o \
       lights_backlight.o \
       owire.o \
       peripheral.o \
       rtc.o \
       tcmd_ioctl.o
endif

ifeq ($(CONFIG_MACH_ARGONPLUS_BUTE),y)
    obj-$(CONFIG_MOT_POWER_IC_ATLAS) := \
       accy.o \
       atod.o \
       audio.o \
       backup_mem.o \
       charger.o \
       debounce.o \
       emu_connected_state.o \
       emu_connecting_state.o \
       emu_disconnected_state.o \
       emu_disconnecting_state.o \
       emu_state_machine.o \
       emu_utils.o \
       peripheral.o \
       rtc.o 
endif

ifeq ($(CONFIG_MACH_ARGONLV_BUTE),y)
    obj-$(CONFIG_MOT_POWER_IC_ATLAS) := \
       accy.o \
       atod.o \
       audio.o \
       backup_mem.o \
       charger.o \
       debounce.o \
       emu_connected_state.o \
       emu_connecting_state.o \
       emu_disconnected_state.o \
       emu_disconnecting_state.o \
       emu_state_machine.o \
       emu_utils.o \
       peripheral.o \
       rtc.o 
endif

fastdep:
