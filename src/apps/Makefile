
# This Makefile is meant to be invoked, NOT INCLUDED!

# REQUIRED
#
# Where to place build artifacts
BUILD_DIR ?=

# REQUIRED
#
# Where FernOS built libraries and copied headers are placed.
# NOTE: We expect the INCLUDE_DIR to be at $(INSTALL_DIR)/include
INSTALL_DIR ?=

# REQUIRED
#
# ELF Symbols file. 
ELF_SYMS ?=

####################################################################################################

# What apps will be built by this Makefile.
# Each entry should be the name of a directory in `src/apps`.
# This Makefile requires that each app contains a Makefile with target `bin`.
# This target should take args
_APPS := Test

####################################################################################################

GIT_TOP   := $(shell git rev-parse --show-toplevel)
SRC_DIR   := $(GIT_TOP)/src
APPS_DIR  := $(SRC_DIR)/apps

# Where to place app binaries!
OUT_DIR   := $(BUILD_DIR)/out

C_COMPILER := i686-elf-gcc

$(BUILD_DIR) $(OUT_DIR):
	mkdir -p $@

APP_PHONY_TARS := $(addprefix app.,$(_APPS))
.PHONY: apps $(APP_PHONY_TARS)

$(APP_PHONY_TARS): app.%: | $(BUILD_DIR) $(OUT_DIR)
	make -C $(APPS_DIR)/$* \
		BUILD_DIR=$(BUILD_DIR)/$* \
		OUT_DIR=$(OUT_DIR) \
		INSTALL_DIR=$(INSTALL_DIR) \
		ELF_SYMS=$(ELF_SYMS) \
		bin

apps: $(APP_PHONY_TARS)
