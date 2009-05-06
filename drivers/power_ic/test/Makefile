include ../../../Rules.make

# If INCLUDEDIR not specified, then assume we are building from 
# the driver's test dir - the linux include directory is 3 levels above
# this. 
ifndef INCLUDEDIR
   INCLUDEDIR = ../../../include
endif

# This can be cleaned up later, but for now pick the compiler based on
# the location we're building from. The path to the compiler differs
# for ezx and juix. If building from $(EZX_BASE)/jem/tools, this should be
# OK.
ifeq ($(PWD),$(EZX_BASE)/jem/hardhat/linux-2.4.x/drivers/power_ic/test)
    CC = $(EZX_BASE)/jem/tools/bin/iwmmxt_le-gcc
else
ifeq ($(PWD),$(EZX_LINUX_BASE)/linux/linux-2.4.17/drivers/power_ic/test)
    CC = $(EZX_TOOLS_BASE)/linux/toolchain/toolchain-3.3/devkit/arm/iwmmxt_le/bin/iwmmxt_le-gcc
endif
endif

CFLAGS = -I $(INCLUDEDIR) -Wall -g


TARGETS = test_pcap

all: clean $(TARGETS)
all_targets: all

test_pcap: test_pcap.c
	$(CC) $(CFLAGS) -o test_pcap test_pcap.c

clean:
	rm -f $(TARGETS) 
