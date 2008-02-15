#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= ezx.o

obj-y :=
obj-m :=
obj-n :=
obj-  :=

export-objs := generic.o irq.o dma.o sram.o pxabus_ldm.o

# Common support (must be linked before board specific support)
#obj-y += generic.o irq.o dma.o usb_sw.o
obj-y += generic.o irq.o dma.o 

# Specific board support
obj-$(CONFIG_ARCH_EZX_A780) += a780.o pxabus_ldm.o
obj-$(CONFIG_ARCH_EZX_E680) += a780.o pxabus_ldm.o

# Misc features
obj-$(CONFIG_PM) += pm.o sleep.o useroff.o
obj-$(CONFIG_APM) += apm.o

# below 2 lines have to be exist due to ugly code
#+++what if PM and DPM are both on??
obj-$(CONFIG_DPM) += pm.o sleep.o

obj-$(CONFIG_KGDB_SERIAL) += kgdb-serial.o
obj-$(CONFIG_CPU_BULVERDE) += sram.o
obj-$(CONFIG_DPM) += bulverde_dpm.o mainstone_dpm.o bulverde_freq.o bulverde_voltage.o

include $(TOPDIR)/Rules.make
