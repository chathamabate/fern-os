
GIT_TOP ?= $(shell git rev-parse --show-toplevel)

SRC_DIR    := $(GIT_TOP)/src
BUILD_DIR   := $(GIT_TOP)/src/build
INSTALL_DIR := $(BUILD_DIR)/install
INCLUDE_DIR := $(INSTALL_DIR)/include

MY_MAKE := @make --no-print-directory
DONE_MSG = @echo "\x1B[1;97m>> \x1B[1;92m$1 \x1B[1;97m<<\x1b[0m\n"

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
# All ranges below are inclusive! (These all must be numeric constants)

PROLOGUE_START  :=       0x0
PROLOGUE_END    :=  0x3FFFFF

FERNOS_START   :=   	0x00400000

# Where Kernel parts are placed.
FERNOS_KERNEL_START := 	0x00400000
FERNOS_KERNEL_END   := 	0x01FFFFFF

# Where User applications will be loaded.
FERNOS_APP_START    :=  0x02000000
FERNOS_APP_END      :=  0x02FFFFFF

# Where the dynamic memory area will live for all processes
FERNOS_FREE_START   :=  0x10000000
FERNOS_FREE_END     :=  0x3FFFFFFF

# Area where stacks will live in fernos.
FERNOS_STACK_START  := 0xA0000000
FERNOS_STACK_END    := 0xBFFFFFFF

FERNOS_END     := 0xBFFFFFFF

EPILOGUE_START := 0xC0000000
EPILOGUE_END   := 0xFFFFFFFF

SECT_DEFS := PROLOGUE_START PROLOGUE_END \
			 FERNOS_START \
			 FERNOS_KERNEL_START FERNOS_KERNEL_END \
			 FERNOS_APP_START FERNOS_APP_END \
			 FERNOS_FREE_START FERNOS_FREE_END \
			 FERNOS_STACK_START FERNOS_STACK_END \
			 FERNOS_END \
			 EPILOGUE_START EPILOGUE_END 

C_MACRO_GEN = echo "\#define $1 $($1)UL" >> $@;
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
	$(call DONE_MSG,Headers Installed)

$(HDRS_INSTALL_TARS): hdrs.install.%:
	$(MY_MAKE) -C $(SRC_DIR)/$* hdrs.install test_hdrs.install 

# Install Libs (For now we'll just always build the test and normal libs)

LIB_INSTALL_TARS := $(addprefix lib.install.,$(MODS))
.PHONY: lib.unsafe_install lib.install $(LIB_INSTALL_TARS)

# This is "unsafe" since there is no gaurantee that the headers have been installed.
lib.unsafe_install: $(LIB_INSTALL_TARS)
$(LIB_INSTALL_TARS): lib.install.%:
	$(MY_MAKE) -C $(SRC_DIR)/$* lib.install test_lib.install

lib.install: hdrs.install
	$(MY_MAKE) -C $(SRC_DIR) lib.unsafe_install
	$(call DONE_MSG,Libraries Installed)

# Clangd Helpers

CLANGD_TARS := $(addprefix clangd.,$(MODS))
.PHONY: clangd $(CLANGD_TARS)

$(CLANGD_TARS): clangd.%:
	$(MY_MAKE) -C $(SRC_DIR)/$* clangd

clangd: $(CLANGD_TARS)

CLANGD_CLEAN_TARS := $(addprefix clean.clangd.,$(MODS))
.PHONY: clean.clangd $(CLANGD_CLEAN_TARS)

$(CLANGD_CLEAN_TARS): clean.clangd.%:
	$(MY_MAKE) -C $(SRC_DIR)/$* clean.clangd

clean.clangd: $(CLANGD_CLEAN_TARS)

####################################################################################################

# Kernel Compilation

OS_NAME := fernos
C_COMPILER := i686-elf-gcc

LDSCRIPT := $(SRC_DIR)/linker.ld

ELF_FILE := $(BUILD_DIR)/$(OS_NAME).elf
ELF_SYMS := $(BUILD_DIR)/$(OS_NAME).syms

$(BUILD_DIR):
	mkdir -p $@

LIB_FLAGS := $(foreach mod,$(MODS),-l$(mod)_test -l$(mod))

# Kinda frustrating that this always runs! (But I guess it's not a huge deal)
$(ELF_FILE): lib.install $(OS_DEFS_LD) | $(BUILD_DIR)
	$(C_COMPILER) -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) \
		-Wl,--start-group $(LIB_FLAGS) -lgcc -Wl,--end-group
	$(call DONE_MSG, Kernel ELF Built)

# The symbols file is useful for compiling user application binaries.
# (This also always runs)
$(ELF_SYMS): $(ELF_FILE) | $(BUILD_DIR)
	i686-elf-objcopy --extract-symbol $< $@

.PHONY: bin qemu.bin

bin: $(ELF_FILE)

####################################################################################################

# Disk Setup

# NOTE: One annoying caveat of mtools is that they don't give an easy way to force the usage of 
# a long filename. FernOS right now ONLY recognizes entries with long filenames 
# (besides "." and "..").
#
# So, when adding files and directory to the disk from a host machine, the filenames used MUST
# have MORE than 8 characters before the extension. Otherwise, FernOS will not be able to locate
# said files. (Files/directories created from within FernOS will always be given long filenames
# and thus can have any name length)

TOY_DISK := $(BUILD_DIR)/hdd.img
CORE_APPS_GUEST := ::/core_apps
CORE_APPS_HOST  := $(BUILD_DIR)/core_apps

.PHONY: core_apps disk

$(CORE_APPS_HOST):
	mkdir -p $@

# Maybe later this will include some sort of external repo iteration!
core_apps: $(ELF_SYMS) $(OS_DEFS_LD) hdrs.install | $(CORE_APPS_HOST)
	$(MY_MAKE) -C $(SRC_DIR)/dummy_app \
		BUILD_DIR=$(BUILD_DIR)/dummy_app \
		HOST_CORE_APPS_DIR=$(CORE_APPS_HOST) \
		INSTALL_DIR=$(INSTALL_DIR) \
		ELF_SYMS=$(ELF_SYMS) \
		bin clangd

$(TOY_DISK): core_apps | $(BUILD_DIR)
	qemu-img create -f raw $@ 256M
	mkfs.fat -F 32 -s 2 $@
	mcopy -i $@ $(CORE_APPS_HOST) $(CORE_APPS_GUEST)
	$(call DONE_MSG,Disk Built)

disk: $(TOY_DISK)

####################################################################################################

# Bringing it all together

.PHONY: all

all: bin disk

# NOTE: This depends on the TOY_DISK existing.
# Order only deps are annoying, so it's not listed here.
qemu.bin: $(ELF_FILE)
	qemu-system-x86_64 \
		-m 4G \
		-kernel $(ELF_FILE) \
		-drive file=$(TOY_DISK),format=raw,index=0,media=disk \
		-display sdl \
		-full-screen \
		-monitor stdio
		
# Clean

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OS_DEFS_LD)
