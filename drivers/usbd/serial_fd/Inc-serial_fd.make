# Prelude

sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)


# Subdirectories

# (none)


# Local variables

MODS_$(d)	:=

OBJS_$(d)	:= $(sort $(foreach m,$(MODS_$(d):$(d)/%=%),$($(m)_$(d)))) \
		   $(d)/crc10.o $(d)/mouse.o $(d)/serial.o $(d)/serproto.o

DEPS_$(d)	:= $(OBJS_$(d):%=%.d)


# Accumulate globals

TGT_MOD		:= $(TGT_MOD) $(MODS_$(d))
OBJS		:= $(OBJS) $(OBJS_$(d))

CLEAN		:= $(CLEAN) $(OBJS_$(d)) $(DEPS_$(d)) $(MODS_$(d))


# Local rules

$(OBJS_$(d)):	CF_TGT := -I$(d)


# Postlude

-include	$(DEPS_$(d))

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
#___________________________________________________________________________
