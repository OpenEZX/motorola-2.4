# Prelude

sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)


# Subdirectories

# (none)


# Local variables

MODS_$(d)	:= $(d)/pxa_bi.o

pxa_bi.o_$(d)	:= $(d)/pxa.o $(d)/usbd-bi.o $(d)/trace.o

OBJS_$(d)	:= $(sort $(foreach m,$(MODS_$(d):$(d)/%=%),$($(m)_$(d))))

DEPS_$(d)	:= $(OBJS_$(d):%.o=%.d)


# Accumulate globals

TGT_MOD		:= $(TGT_MOD) $(MODS_$(d))
OBJS		:= $(OBJS) $(OBJS_$(d))

CLEAN		:= $(CLEAN) $(OBJS_$(d)) $(DEPS_$(d)) $(MODS_$(d))


# Local rules

$(OBJS_$(d)):	CF_TGT := -I$(d) -I$(dirstack_$(sp))

$(d)/pxa_bi.o:		$(pxa_bi.o_$(d))
	$(LINK)


# Postlude

-include	$(DEPS_$(d))

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
#___________________________________________________________________________
