#
# Makefile for the memory technology device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#
# $Id: Makefile,v 1.68 2002/11/27 10:35:08 gleixner Exp $

export-objs	:= mtdcore.o mtdpart.o redboot.o cmdlinepart.o afs.o mtdconcat.o

nftl-objs	:= nftlcore.o nftlmount.o
mtdblock-objs	:= mtdblock-core.o

BELOW25		:= $(shell echo $(PATCHLEVEL) | sed s/[1234]/y/)

ifeq ($(BELOW25),y)
obj-y           += chips/chipslink.o maps/mapslink.o \
			devices/devlink.o nand/nandlink.o
obj-m           :=
obj-n           :=
obj-            :=

O_TARGET	:= mtdlink.o

list-multi	:= nftl.o mtdblock.o

mod-subdirs	:= 
subdir-y	:= chips maps devices nand
subdir-m	:= $(subdir-y)
mtdblock-objs	+= mtdblock-24.o
else
obj-y           += chips/ maps/ devices/ nand/
mtdblock-objs	+= mtdblock-25.o
endif

#                       *** BIG UGLY NOTE ***
#
# The shiny new inter_module_xxx has introduced yet another ugly link
# order dependency, which I'd previously taken great care to avoid.
# We now have to ensure that the chip drivers are initialised before the
# map drivers, and that the doc200[01] drivers are initialised before
# docprobe.
#
# We'll hopefully merge the doc200[01] drivers and docprobe back into
# a single driver some time soon, but the CFI drivers are going to have
# to stay like that.
#
# Urgh.
# 
# dwmw2 21/11/0

# Core functionality.
obj-$(CONFIG_MTD)		+= mtdcore.o
obj-$(CONFIG_MTD_CONCAT)	+= mtdconcat.o
obj-$(CONFIG_MTD_PARTITIONS)	+= mtdpart.o
obj-$(CONFIG_MTD_REDBOOT_PARTS) += redboot.o
obj-$(CONFIG_MTD_BOOTLDR_PARTS) += bootldr.o
obj-$(CONFIG_MTD_CMDLINE_PARTS) += cmdlinepart.o
obj-$(CONFIG_MTD_AFS_PARTS)	+= afs.o

# 'Users' - code which presents functionality to userspace.
obj-$(CONFIG_MTD_CHAR)		+= mtdchar.o
obj-$(CONFIG_MTD_BLOCK)		+= mtdblock.o
obj-$(CONFIG_MTD_BLOCK_RO)	+= mtdblock_ro.o
obj-$(CONFIG_FTL)		+= ftl.o
obj-$(CONFIG_NFTL)		+= nftl.o


include $(TOPDIR)/Rules.make

# below 2.5 only
ifeq ($(BELOW25),y)
nftl.o: $(nftl-objs)
	$(LD) -r -o $@ $(nftl-objs)

mtdblock.o: $(mtdblock-objs)
	$(LD) -r -o $@ $(mtdblock-objs)
endif
