# Simple check that a generated configuration Makefile was provided!

# These targets do not need a CONFIG_MK to be specified.
# Their behavior should be identical regardless of what config
# is being worked with!
CONFIG_FREE_TARGETS := clean.clangd

ifneq ($(filter-out $(CONFIG_FREE_TARGETS),$(MAKECMDGOALS)),)
ifeq ($(CONFIG_MK),)
$(error CONFIG_MK not specified)
endif

include $(CONFIG_MK)
endif
