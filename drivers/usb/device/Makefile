#
# Core of the USB Device subsystem.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.
# Copyright (C) 2003 RTSoft.

O_TARGET		:= usbd_dev.o

# Multipart objects.
list-multi		:= usbdcore.o usbdmonitor.o usbdserial.o
usbdcore-objs		:= usbd.o ep0.o usbd-debug.o hotplug.o \
                           usbd-func.o usbd-bus.o crc32tab.o
usbdmonitor-objs	:= usbd-monitor.o hotplug.o
usbdserial-objs		:= usbd-serialnumber.o hotplug.o

# Object file lists.
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Subdirectories.
subdir-y        :=
subdir-m        :=
subdir-n        :=
subdir-         :=

# Each configuration option enables a list of files.
obj-$(CONFIG_USBD)		+= usbdcore.o 
obj-$(CONFIG_USBD_MONITOR)	+= usbdmonitor.o usbdserial.o
ifeq ($(CONFIG_USBD_NET),y)
obj-y				+= net_fd/net_fd_drv.o
endif
ifeq ($(CONFIG_USBD_SERIAL),y)
obj-y				+= serial_fd/serial_fd_drv.o
endif
ifeq ($(CONFIG_USBD_TEST),y)
obj-y				+= test_fd/test_fd_drv.o
endif
ifeq ($(CONFIG_USBD_PST),y)
obj-y				+= pst_fd/pst_fd_drv.o
endif
# If one of bi drivers is non-modular, add it to obj-y list.
ifeq ($(shell echo $(CONFIG_USBD_BI_ID) | cut -f 2 -d '-'),y)
obj-y				+= bi/$(shell echo $(CONFIG_USBD_BI_ID) | cut -f 1 -d '-').o
endif

# Object files in subdirectories.
subdir-$(shell echo $(CONFIG_USBD_BI_ID) | cut -f 2 -d '-')	+= bi
subdir-$(CONFIG_USBD_NET) 					+= net_fd
subdir-$(CONFIG_USBD_SERIAL) 					+= serial_fd
subdir-$(CONFIG_USBD_TEST)					+= test_fd
subdir-$(CONFIG_USBD_PST)					+= pst_fd

# The global Rules.make.
include $(TOPDIR)/Rules.make

# Special targets to generate headers with build information.
#.PHONY: usbd-export.h usbd-build.h

#usbd-export.h:
#	echo "#define USBD_EXPORT_DATE \"`date '+%Y-%m-%d %H:%M'`\""  > $@

#usbd-build.h:
# If file doesn't exists, USBD_BUILD will be "000".
# Otherwise, USBD_BUILD will be "$USBD_BUILD + 1".
	if [ -r $@ ]; then \
	build=$$(awk '{print substr($$3, 2, length($$3) - 2)}' $@); \
	echo -n "#define USBD_BUILD " > $@; \
        printf "\"%03d\"\n" `expr $$build + 1` >> $@; else \
	echo "#define USBD_BUILD \"000\"" > $@; fi

# Link rules for multi-part drivers.
usbdmonitor.o: $(usbdmonitor-objs)
	$(LD) -r -o $@ $(usbdmonitor-objs)

usbdserial.o: $(usbdserial-objs)
	$(LD) -r -o $@ $(usbdserial-objs)

usbdcore.o: $(usbdcore-objs)
	$(LD) -r -o $@ $(usbdcore-objs)
