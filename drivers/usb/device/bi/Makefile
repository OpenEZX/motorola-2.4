#
# Low-level bus interface drivers for USB Device subsystem.
#
# Copyright (C) 2001 Lineo, Inc.
# Copyright (C) 2001 Hewlett-Packard Co.
# Copyright (C) 2003 RTSoft.

O_TARGET	:= bi_drv.o

# Multipart drivers (all).
list-multi	:= gen_bi.o sl11_bi.o sl11_bi.o sa1100_bi.o superh_bi.o pxa_bi.o
gen_bi-objs	:= gen.o usbd-bi.o
sl11_bi-objs	:= sl11.o usbd-bi.o
sa1100_bi-objs	:= sa1100.o usbd-bi.o
superh_bi-objs	:= superh.o usbd-bi.o
pxa_bi-objs	:= pxa.o usbd-bi.o

# Clear all object file lists.
obj-y		:= 
obj-m		:=
obj-n		:=
obj-		:=

# Each configuration option enables a list of files.
obj-$(CONFIG_USBD_SL11_BUS)	+= sl11_bi.o
obj-$(CONFIG_USBD_GENERIC_BUS)	+= gen_bi.o
obj-$(CONFIG_USBD_SA1100_BUS)	+= sa1100_bi.o
obj-$(CONFIG_USBD_SUPERH_BUS)	+= superh_bi.o
obj-$(CONFIG_USBD_L7205_BUS)	+= l7205_bi.o
obj-$(CONFIG_USBD_PXA_BUS)	+= pxa_bi.o

# The global Rules.make.
include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.
gen_bi.o: $(gen_bi-objs)
	$(LD) -r -o $@ $(gen_bi-objs)

sl11_bi.o: $(sl11_bi-objs)
	$(LD) -r -o $@ $(sl11_bi-objs)

sa1100_bi.o: $(sa1100_bi-objs)
	$(LD) -r -o $@ $(sa1100_bi-objs)

superh_bi.o: $(superh_bi-objs)
	$(LD) -r -o $@ $(superh_bi-objs)

l7205_bi.o: $(l7205_bi-objs)
	$(LD) -r -o $@ $(l7205_bi-objs)

pxa_bi.o: $(pxa_bi-objs)
	$(LD) -r -o $@ $(pxa_bi-objs)
