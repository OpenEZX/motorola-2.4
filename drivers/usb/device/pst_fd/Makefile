#
# PST device layer for USB Device subsystem.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.
# Copyright (C) 2003 RTSoft.
# Copyright (C) 2003 Motorola.

O_TARGET	:= pst_fd_drv.o

# Multipart object and list of it's object files.
list-multi	:= pst_fd.o
pst_fd-objs	:= pst-fd.o

# Object file lists.

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.
obj-$(CONFIG_USBD_PST)   += pst_fd.o

# The global Rules.make.
include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.
pst_fd.o: $(pst_fd-objs)
	$(LD) -r -o $@ $(pst_fd-objs)




