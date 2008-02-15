#
# Makefile for misc devices that really don't fit anywhere else.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_TARGET := misc.o

export-objs			:= ezx-ts.o log.o

obj-$(CONFIG_MCP_UCB1200_TS)	+= ezx-ts.o
obj-$(CONFIG_MCP_UCB1400_TS)	+= ezx-ts.o

obj-$(CONFIG_PANIC_LOG)		+= log.o
ifeq ($(CONFIG_PANIC_LOG),y)
obj-$(CONFIG_ARCH_LUBBOCK)	+= ezx-log.o
endif

include $(TOPDIR)/Rules.make

fastdep:
