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

export-objs			:= mcp-core.o mcp-sa1100.o mcp-pxa.o \
				   ucb1x00-core.o keypad.o ssp_pcap_main.o ezx-ts.o ezx-button.o ezx-emu.o keylight.o keypad-panasonic.o keypad-e398.o log.o

obj-y                       +=  ssp_pcap_main.o ezx-ts.o ezx-button.o ezx-emu.o
ifneq ($(HW_PLATFORM), A780_PANASONIC)
ifneq ($(HW_PLATFORM), A780_E398)
obj-y  += keypad.o
endif
endif

ifeq ($(HW_PLATFORM), A780_PANASONIC)
obj-y += keypad-panasonic.o
endif

ifeq ($(HW_PLATFORM), A780_E398)
obj-y  += keypad-e398.o
endif

obj-$(CONFIG_MCP_SA1100)	+= mcp-core.o mcp-sa1100.o
obj-$(CONFIG_MCP_UCB1200)	+= ucb1x00-core.o
obj-$(CONFIG_MCP_UCB1200_AUDIO)	+= ucb1x00-audio.o
obj-$(CONFIG_MCP_UCB1200_TS)	+= ucb1x00-ts.o
obj-$(CONFIG_INNOVATOR_TS)	+= innovator-ts.o
obj-$(CONFIG_MCP_UCB1400_TS)	+= mcp-pxa.o ucb1x00-core.o ucb1x00-ts.o
obj-$(CONFIG_KEYLIGHT_A780)	+= keylight.o
obj-$(CONFIG_FMRADIO_E680)	+= fmradio.o

ifeq ($(CONFIG_PANIC_LOG),y)
obj-$(CONFIG_PANIC_LOG)		+= log.o
ifeq ($CONFIG_ARCH_EZX),y)
obj-$(CONFIG_ARCH_EZX)		+= ezx-log.o
endif
endif

include $(TOPDIR)/Rules.make

fastdep:
