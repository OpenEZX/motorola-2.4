#
# Serial device layer for USB Device subsystem.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.
# Copyright (C) 2003 RTSoft.

O_TARGET	:= serial_fd_drv.o

# Multipart drivers.
list-multi	:= serial_fd.o
serial_fd-objs	:= serial.o serproto.o crc10.o

# Object file lists.
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.
obj-$(CONFIG_USBD_SERIAL)   += serial_fd.o

# The global Rules.make.
include $(TOPDIR)/Rules.make

# Link rule for multi-part driver.
serial_fd.o: $(serial_fd-objs)
	$(LD) -r -o $@ $(serial_fd-objs)
