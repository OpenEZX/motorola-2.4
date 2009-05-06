#
#  Motorola VIDEO CLASS device layer for USB Device subsystem.
#
#  Copyright (C) 2005,2006 Motorola Inc.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published
#  by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Revision History:
#                    Modification   
# Author                 Date        Description of Changes
# -----------------  ------------   --------------------------------
# Motorola   	     01/10/2006     Initial Version

O_TARGET	:= uvc_fd_drv.o

export-objs	:= uvc.o
# Multipart object and list of it's object files.
list-multi	:= uvc_fd.o
uvc_fd-objs	:= uvc.o

# Object file lists.

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.
obj-$(CONFIG_USBD_UVC)   +=  uvc_fd.o

# The global Rules.make.
include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.
uvc_fd.o: $(uvc_fd-objs)
	$(LD) -r -o $@ $(uvc_fd-objs)




