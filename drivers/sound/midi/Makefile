#
# Makefile for the kernel mmc device drivers.
#

O_TARGET := hwmidi.o

EXTRA_CFLAGS += -DVLIO -DHWMIDI_MIXER
#EXTRA_CFLAGS += -DMIDI_DEBUG_FLAG

obj-y :=  midicomm.o mididrv.o mamachdepk.o 
obj-m := $(O_TARGET)

midi_base_objs	:= midicomm.o mididrv.o mamachdepk.o

include $(TOPDIR)/Rules.make


clean:
	rm core -f *.o *.a
