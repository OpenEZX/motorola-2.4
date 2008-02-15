#
# Network device layer for USB Device subsystem.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.
# Copyright (C) 2003 RTSoft.

O_TARGET	:= net_fd_drv.o

# Multipart object and list of it's object files.
list_multi	:= net_fd.o
net_fd-objs	:= net-fd.o netproto.o 

# Object file lists.
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.
obj-$(CONFIG_USBD_NET)   += net_fd.o

# The global Rules.make.
include $(TOPDIR)/Rules.make

# Link rule for multi-part driver.
net_fd.o: $(net_fd-objs)
	$(LD) -r -o $@ $(net_fd-objs)
