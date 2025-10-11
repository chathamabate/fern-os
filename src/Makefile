
GIT_TOP ?= $(shell git rev-parse --show-toplevel)

SRC_DIR    := $(GIT_TOP)/src
BUILD_DIR   := $(GIT_TOP)/src/build
INSTALL_DIR := $(BUILD_DIR)/install
INCLUDE_DIR := $(INSTALL_DIR)/include

THIS_FILE := $(SRC_DIR)/Makefile

MODS := \
		u_concur \
		k_startup \
		u_startup \
		s_bridge \
		k_sys \
		s_block_device \
		s_elf \
		s_data \
		s_mem \
		s_util 

# Static memory layout header and linker generation:

$(INCLUDE_DIR) $(INSTALL_DIR):
	mkdir -p $@

####################################################################################################

# Layout Header and LD script generation.

PROLOGUE_START  :=       0x0
PROLOGUE_END    :=  0x3FFFFF

FERNOS_START   :=   0x400000
FERNOS_END     := 0xBFFFFFFF

EPILOGUE_START := 0xC0000000
EPILOGUE_END   := 0xFFFFFFFF

SECT_DEFS := PROLOGUE_START PROLOGUE_END \
			 FERNOS_START FERNOS_END \
			 EPILOGUE_START EPILOGUE_END 

C_MACRO_GEN = echo "\#define $1 $($1)" >> $@;
LD_DEF_GEN = echo "$1 = ($($1));" >> $@; 
S_DEF_GEN = echo ".equ $1, $($1)" >> $@; 

OS_DEFS_H := $(INCLUDE_DIR)/os_defs.h
$(OS_DEFS_H): $(THIS_FILE) | $(INCLUDE_DIR)
	echo "#pragma once" > $@
	$(foreach def,$(SECT_DEFS),$(call C_MACRO_GEN,$(def)))

OS_DEFS_LD := $(INSTALL_DIR)/os_defs.ld
$(OS_DEFS_LD): $(THIS_FILE) | $(INSTALL_DIR)
	rm $@; touch $@;
	$(foreach def,$(SECT_DEFS),$(call LD_DEF_GEN,$(def)))

OS_DEFS_S := $(INCLUDE_DIR)/os_defs.s
$(OS_DEFS_S): $(THIS_FIE) | $(INCLUDE_DIR)
	rm $@; touch $@;
	$(foreach def,$(SECT_DEFS),$(call S_DEF_GEN,$(def)))

####################################################################################################

# Install Headers

HDRS_INSTALL_TARS := $(addprefix hdrs.install.,$(MODS))

.PHONY: hdrs.install $(HDRS_INSTALL_TARS)
hdrs.install: $(HDRS_INSTALL_TARS) $(OS_DEFS_H) $(OS_DEFS_S)

$(HDRS_INSTALL_TARS): hdrs.install.%:
	make -C $(SRC_DIR)/$* hdrs.install test_hdrs.install 

# Install Libs (For now we'll just always build the test and normal libs)

LIB_INSTALL_TARS := $(addprefix lib.install.,$(MODS))
.PHONY: lib.unsafe_install lib.install $(LIB_INSTALL_TARS)

# This is "unsafe" since there is no gaurantee that the headers have been installed.
lib.unsafe_install: $(LIB_INSTALL_TARS)
$(LIB_INSTALL_TARS): lib.install.%:
	make -C $(SRC_DIR)/$* lib.install test_lib.install

lib.install: hdrs.install
	make -C $(SRC_DIR) lib.unsafe_install

# Clangd Helpers

CLANGD_TARS := $(addprefix clangd.,$(MODS))
.PHONY: clangd $(CLANGD_TARS)

$(CLANGD_TARS): clangd.%:
	make -C $(SRC_DIR)/$* clangd

clangd: $(CLANGD_TARS)

CLANGD_CLEAN_TARS := $(addprefix clean.clangd.,$(MODS))
.PHONY: clean.clangd $(CLANGD_CLEAN_TARS)

$(CLANGD_CLEAN_TARS): clean.clangd.%:
	make -C $(SRC_DIR)/$* clean.clangd

clean.clangd: $(CLANGD_CLEAN_TARS)

# Finally now for actual kernel packaging!

OS_NAME := fernos
C_COMPILER := i686-elf-gcc

LDSCRIPT := $(SRC_DIR)/linker.ld

ELF_FILE := $(BUILD_DIR)/$(OS_NAME).elf
ELF_SYMS := $(BUILD_DIR)/$(OS_NAME).syms

$(BUILD_DIR):
	mkdir -p $@

LIB_FLAGS := $(foreach mod,$(MODS),-l$(mod)_test -l$(mod))

$(ELF_FILE): lib.install $(OS_DEFS_LD) | $(BUILD_DIR)
	$(C_COMPILER) -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) \
		-Wl,--start-group $(LIB_FLAGS) -lgcc -Wl,--end-group

$(ELF_SYMS): $(ELF_FILE) | $(BUILD_DIR)
	i386-elf-objcopy --extract-symbol $< $@

DUMMY_APP_CFLAGS := -m32 -std=gnu99 -nostartfiles -ffreestanding -Wall -Wextra -Wpedantic
DUMMY_APP := $(BUILD_DIR)/dummy_app
$(DUMMY_APP): $(SRC_DIR)/dummy_app.c $(ELF_SYMS) | $(BUILD_DIR)
	$(C_COMPILER) $< -o $@ $(DUMMY_APP_CFLAGS) -O2 -nostdlib -lgcc -Wl,--just-symbols=$(ELF_SYMS) \
		-I$(INCLUDE_DIR)

t: $(DUMMY_APP)

.PHONY: bin qemu.bin

bin: $(ELF_FILE)

TOY_DISK := $(BUILD_DIR)/hdd.img
$(TOY_DISK): | $(BUILD_DIR)
	qemu-img create -f raw $(TOY_DISK) 1M

qemu.bin: $(ELF_FILE) | $(TOY_DISK)
	qemu-system-x86_64 \
		-m 4G \
		-kernel $(ELF_FILE) \
		-drive file=$(TOY_DISK),format=raw,index=0,media=disk \
		-display sdl \
		-full-screen \
		-monitor stdio
		
# NOTE: When we start working with disk read/write... I'd really like to figure out ISO packaging!

# Clean

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OS_DEFS_LD)
