
GIT_TOP ?= $(shell git rev-parse --show-toplevel)

BUILD_DIR   := $(GIT_TOP)/build
INSTALL_DIR := $(BUILD_DIR)/install
INCLUDE_DIR := $(INSTALL_DIR)/include
MODS_DIR    := $(GIT_TOP)/src

THIS_FILE := $(MODS_DIR)/Makefile

# I like to list the modules in order of dependence.
MODS := \
		k_startup \
		k_bios_term \
		k_sys \
		s_util 

# Static memory layout header and linker generation:

$(INCLUDE_DIR) $(INSTALL_DIR):
	mkdir -p $@

####################################################################################################

# Layout Header and LD script generation.

# On startup FernOS will have 4 Linker Script specified areas.
# Each area must have an offset and size which are both multiples of 4MB.
#
# Ends are exlusive here...

PROLOGUE_START :=        0x0
PROLOGUE_END   :=   0x3FFFFF

FERNOS_START   :=   0x400000
FERNOS_END     := 0xBFFFFFFF

EPILOGUE_START := 0xC0000000
EPILOGUE_END   := 0xFFFFFFFF

SECT_DEFS := PROLOGUE_START PROLOGUE_END \
			 FERNOS_START FERNOS_END \
			 EPILOGUE_START EPILOGUE_END 

C_MACRO_GEN = echo "\#define $1 $($1)" >> $@;
LD_DEF_GEN = echo "$1 = ($($1));" >> $@;

OS_DEFS_H := $(INCLUDE_DIR)/os_defs.h
$(OS_DEFS_H): $(THIS_FILE) | $(INCLUDE_DIR)
	echo "#pragma once" > $@
	$(foreach def,$(SECT_DEFS),$(call C_MACRO_GEN,$(def)))

OS_DEFS_LD := $(INSTALL_DIR)/os_defs.ld
$(OS_DEFS_LD): $(THIS_FILE) | $(INSTALL_DIR)
	rm $@; touch $@;
	$(foreach def,$(SECT_DEFS),$(call LD_DEF_GEN,$(def)))

####################################################################################################

# Install Headers

HDRS_INSTALL_TARS := $(addprefix hdrs.install.,$(MODS))

.PHONY: hdrs.install $(HDRS_INSTALL_TARS)
hdrs.install: $(HDRS_INSTALL_TARS) $(OS_DEFS_H)

$(HDRS_INSTALL_TARS): hdrs.install.%:
	make -C $(MODS_DIR)/$* hdrs.install test_hdrs.install

# Install Libs (For now we'll just always build the test and normal libs)

LIB_INSTALL_TARS := $(addprefix lib.install.,$(MODS))
.PHONY: lib.unsafe_install lib.install $(LIB_INSTALL_TARS)

# This is "unsafe" since there is no gaurantee that the headers have been installed.
lib.unsafe_install: $(LIB_INSTALL_TARS)
$(LIB_INSTALL_TARS): lib.install.%:
	make -C $(MODS_DIR)/$* lib.install test_lib.install

lib.install: hdrs.install
	make -C $(MODS_DIR) lib.unsafe_install

# Clangd Helpers

CLANGD_TARS := $(addprefix clangd.,$(MODS))
.PHONY: clangd $(CLANGD_TARS)

$(CLANGD_TARS): clangd.%:
	make -C $(MODS_DIR)/$* clangd

clangd: $(CLANGD_TARS)

CLANGD_CLEAN_TARS := $(addprefix clean.clangd.,$(MODS))
.PHONY: clean.clangd $(CLANGD_CLEAN_TARS)

$(CLANGD_CLEAN_TARS): clean.clangd.%:
	make -C $(MODS_DIR)/$* clean.clangd

clean.clangd: $(CLANGD_CLEAN_TARS)

# Finally now for actual kernel packaging!

OS_NAME := fernos
C_COMPILER := i686-elf-gcc

LDSCRIPT := $(MODS_DIR)/linker.ld

BIN_FILE := $(BUILD_DIR)/$(OS_NAME).bin

$(BUILD_DIR):
	mkdir -p $@

LIB_FLAGS := $(foreach mod,$(MODS),-l$(mod) -l$(mod)_test)

$(BIN_FILE): lib.install $(OS_DEFS_LD) | $(BUILD_DIR)
	$(C_COMPILER) -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) $(LIB_FLAGS) -lgcc

.PHONY: bin qemu.bin

bin: $(BIN_FILE)

qemu.bin: $(BIN_FILE)
	qemu-system-x86_64 -m 4G -kernel $(BIN_FILE) -monitor stdio

# NOTE: When we start working with disk read/write... I'd really like to figure out ISO packaging!

# Clean

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OS_DEFS_LD)
