#
# Makefile for the kernel USBD (device not host) drivers.
#
# Copyright (c) 2002 Belcarra
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.

# Subdirs.
# This is a bit complex, because some subdirs are for
# proprietary code, and are simply not present in a
# general distribution.

TOPDIR ?= ../../..

# The all-CAPS *_DIRS get nuked in the new versions
# of Rules.make, so use only the subdir-* methods.
subdir-y 	:= 
subdir-m 	:=
subdir-n 	:=
subdir- 	:=

# Function Drivers
#subdir-$(CONFIG_USBD_AUDIO) += audio_fd
subdir-$(CONFIG_USBD_NETWORK) += network_fd
subdir-$(CONFIG_USBD_SERIAL) += serial_fd
subdir-$(CONFIG_USBD_ACM) += acm_fd
subdir-$(CONFIG_USBD_PST) += pst_fd
# Bus Interface Drivers
subdir-$(CONFIG_USBD_AU1X00_BUS) += bi
subdir-$(CONFIG_USBD_GENERIC_BUS) += bi
subdir-$(CONFIG_USBD_LH7A400_BUS) += bi
subdir-$(CONFIG_USBD_PXA_BUS) += bi
subdir-$(CONFIG_USBD_SA1100_BUS) += bi
subdir-$(CONFIG_USBD_SL11_BUS) += bi
subdir-$(CONFIG_USBD_SMDK2500_BUS) += bi
subdir-$(CONFIG_USBD_SUPERH_BUS) += bi

# The target object and module list name.

O_TARGET	:= usbdev.o

# Objects that export symbols.

export-objs	:= usbd.o

# Multipart objects.

list-multi		:= usbdcore.o usbdmonitor.o usbdserial.o
usbdcore-objs		:= usbd.o ep0.o usbd-debug.o hotplug.o usbd-func.o usbd-bus.o
usbdmonitor-objs	:= usbd-monitor.o hotplug.o
usbdprocfs-objs		:= usbd-procfs.o
usbdserial-objs		:= usbd-serialnumber.o hotplug.o

# Optional parts of multipart objects.

# Object file lists.

#obj-y		:= usbd-bus.o usbd-func.o crc8.o crc10.o crc16.o crc32.o
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.

obj-$(CONFIG_USBD)		+= usbdcore.o 
obj-$(CONFIG_USBD_MONITOR)	+= usbdmonitor.o 
obj-$(CONFIG_USBD)		+= usbdprocfs.o 
obj-$(CONFIG_USBD_MONITOR)	+= usbdserial.o 

# Object files in subdirectories

#
ifeq ($(CONFIG_USBD_NETWORK),y)
	obj-y += network_fd/network_drv.o
endif

ifeq ($(CONFIG_USBD_SERIAL),y)
	obj-y += serial_fd/serial_fd_drv.o
endif

ifeq ($(CONFIG_USBD_ACM),y)
	obj-y += acm_fd/acm_fd_drv.o
endif

ifeq ($(CONFIG_USBD_PST),y)
	obj-y += pst_fd/pst_fd_drv.o
endif

# Yech.  This isn't the best way to do this, but there isn't a config flag
# common to all the possible bus interfaces
ifeq ($(CONFIG_USBD),y)
	obj-y +=        bi/bi.o
endif


# Extract lists of the multi-part drivers.
# The 'int-*' lists are the intermediate files used to build the multi's.

multi-y		:= $(filter $(list-multi), $(obj-y))
multi-m		:= $(filter $(list-multi), $(obj-m))
int-y		:= $(sort $(foreach m, $(multi-y), $($(basename $(m))-objs)))
int-m		:= $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))

# Files that are both resident and modular: remove from modular.

obj-m		:= $(filter-out $(obj-y), $(obj-m))
int-m		:= $(filter-out $(int-y), $(int-m))

# Translate to Rules.make lists.

O_OBJS		:= $(filter-out $(export-objs), $(obj-y))
OX_OBJS		:= $(filter     $(export-objs), $(obj-y))
M_OBJS		:= $(sort $(filter-out $(export-objs), $(obj-m)))
MX_OBJS		:= $(sort $(filter     $(export-objs), $(obj-m)))
MI_OBJS		:= $(sort $(filter-out $(export-objs), $(int-m)))
MIX_OBJS	:= $(sort $(filter     $(export-objs), $(int-m)))

# The global Rules.make.

include $(TOPDIR)/Rules.make
EXTRA_CFLAGS += -Wno-format -Werror

# Link rules for multi-part drivers.

usbdprocfs.o: $(usbdprocfs-objs)
	$(LD) -r -o $@ $(usbdprocfs-objs)

usbdmonitor.o: $(usbdmonitor-objs)
	$(LD) -r -o $@ $(usbdmonitor-objs)

usbdserial.o: $(usbdserial-objs)
	$(LD) -r -o $@ $(usbdserial-objs)

usbdcore.o: $(usbdcore-objs)
	$(LD) -r -o $@ $(usbdcore-objs)

# dependencies:

usbd-monitor.o: usbd-build.h
usbd-serial.o: usbd-build.h
usbd.o: usbd-build.h

#usbd-export.h:
#	echo "#define USBD_EXPORT_DATE    \"`date '+%Y-%m-%d %H:%M'`\""  > $@

usbd-build.h:
	echo "#define USBD_BUILD          \"000\"" > $@
	

# local

%.h:%.p
	release inc build < $< > $@
	cp $@ $<

release.h: release.p

inc-build:
	release inc build < release.p > release.h
	cp release.h release.p


menuconfig:
	cd $(TOPDIR); make menuconfig

xconfig:
	cd $(TOPDIR); make xconfig



