#___________________________________________________________________________
#					Prelude
# This never changes.
# It pushes "d" on the stack, then assigns "dir" (set by our includer) to it.
#
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)

#___________________________________________________________________________
#					Subdirectories
#
# For each subdirectory (if any) of the current directory, set "dir" to
# the subdirectory name and include the make file fragment from that
# subdirectory. The names of the make file fragments are arbitrary, but
# should either be all the same or follow a discernable pattern. None of
# the code should depend on the ordering of the subdirectories. If a
# subdirectory is not necessarily present, the corresponding include
# should be a soft include.
#
dir	:= $(d)/bi
include		$(dir)/Inc-bi.make
dir	:= $(d)/network_fd
include		$(dir)/Inc-network_fd.make
dir	:= $(d)/serial_fd
include		$(dir)/Inc-serial_fd.make
dir	:= $(d)/rndis_fd
-include	$(dir)/Inc-rndis_fd.make

#___________________________________________________________________________
#					Local variables
#
# All variable names here must have a _$(d) suffix to ensure uniqueness.

# GENS_$(d) is a list of generated .c/.h files to be built in this directory.
#
GENS_$(d)	:= $(d)/usbd-build.h $(d)/usbd-export.h

# MODS_$(d) is a list of all the modules to be built in this directory.
#
MODS_$(d)	:= $(d)/usbdcore.o $(d)/usbdmonitor.o			\
		   $(d)/usbdprocfs.o $(d)/usbdserial.o

# For each element of MODS_$(d), define a variable whose name is the
# filename part of the element, suffixed with _$(d), and whose content
# is the list of object files which are to be linked to produce the module.
#
usbdcore.o_$(d)	:= $(d)/usbd.o $(d)/ep0.o $(d)/usbd-debug.o		\
		   $(d)/hotplug.o $(d)/usbd-func.o $(d)/usbd-bus.o

usbdmonitor.o_$(d)	:= $(d)/usbd-monitor.o $(d)/hotplug.o

usbdprocfs.o_$(d)	:= $(d)/usbd-procfs.o

usbdserial.o_$(d)	:= $(d)/usbd-serialnumber.o $(d)/hotplug.o

# The following line does not change. It builds a list of the object
# files needed for the modules in this directory. Should the need arise,
# other object files could be appended to this line.
#
OBJS_$(d)	:= $(sort $(foreach m,$(MODS_$(d):$(d)/%=%),$($(m)_$(d))))

# The following line does not change. It builds a list of the dependency
# files associated with the OBJ_$(d) files. The list is used below.
#
DEPS_$(d)	:= $(OBJS_$(d):%.o=%.d)

#___________________________________________________________________________
#					Accumulate globals
#
# These assignments gather locally-built lists into global lists.
#
TGT_GEN		:= $(TGT_GEN) $(GENS_$(d))
TGT_MOD		:= $(TGT_MOD) $(MODS_$(d))
OBJS		:= $(OBJS)    $(OBJS_$(d))

CLEAN		:= $(CLEAN) $(GENS_$(d)) $(OBJS_$(d)) $(DEPS_$(d)) $(MODS_$(d))

#___________________________________________________________________________
#					Local rules
#
# Dependencies and commands for actually building things in this directory.
#
$(OBJS_$(d)):	CF_TGT := -I$(d)

$(d)/usbdcore.o:	$(usbdcore.o_$(d))
	$(LINK)

$(d)/usbdmonitor.o:	$(usbdmonitor.o_$(d))
	$(LINK)

$(d)/usbdprocfs.o:	$(usbdprocfs.o_$(d))
	$(LINK)

$(d)/usbdserial.o:	$(usbdserial.o_$(d))
	$(LINK)

$(d)/usbd-build.h:
	echo "#define USBD_BUILD          \"000\"" > $@

$(d)/usbd-export.h:
	echo "#define USBD_EXPORT_DATE    \"`date '+%Y-%m-%d %H:%M'`\""  > $@

#___________________________________________________________________________
#					Postlude
#
# These lines never change.
# Note that the dependency files are included conditionally.
# When the corresponding .o file does not exist, the .d file is not required,
# since the corresponding .c file will necessarily be compiled. A successful
# compile creates not only the .o file but also the .d file.
#
-include	$(DEPS_$(d))
# Pop "d" from the stack.
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
#___________________________________________________________________________
