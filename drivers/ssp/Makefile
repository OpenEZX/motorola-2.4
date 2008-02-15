#
# Makefile for the kernel Plug-and-Play device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..

O_TARGET := ssp.o

export-objs := ssp_pcap_main.o

obj-y += ssp_pcap_main.o

include $(TOPDIR)/Rules.make

