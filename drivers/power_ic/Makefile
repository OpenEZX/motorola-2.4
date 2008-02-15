# Copyright (C) 2005 - 2006 - Motorola
#
# Makefile for the power IC drivers.
#

ifeq ($(CONFIG_CPU_BULVERDE),y)
    O_TARGET     := power_ic.o

    subdir-$(CONFIG_MOT_POWER_IC)       += core module

    obj-$(CONFIG_MOT_POWER_IC)       += core/power_ic_core.o module/power_ic_module.o
    
    include $(TOPDIR)/Rules.make
endif

ifeq ($(CONFIG_ARCH_SCMA11),y)
    obj-y       := core/ module/
endif

ifeq ($(CONFIG_MACH_ARGONPLUS_BUTE),y)
    obj-y       := core/ module/
endif

ifeq ($(CONFIG_MACH_ARGONLV_BUTE),y)
    obj-y       := core/ module/
endif

fastdep:
