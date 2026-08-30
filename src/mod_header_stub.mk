# Simple check that a generated configuration Makefile was provided!

ifeq ($(CONFIG_MK),)
$(error All modules require CONFIG_MK be specified)
endif

include $(CONFIG_MK)

