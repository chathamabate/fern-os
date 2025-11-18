
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
FERNOS_APP_START      :=  0x02000000
FERNOS_APP_END        :=  0x02FFFFFF

# Where User application arguments will be loaded.
FERNOS_APP_ARGS_START := 0x03000000
FERNOS_APP_ARGS_END   := 0x03003FFF

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
			 FERNOS_APP_ARGS_START FERNOS_APP_ARGS_END \
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

INCLUDE_DIRS := $(addprefix $(INCLUDE_DIR)/,$(MODS))
$(INCLUDE_DIRS): $(INCLUDE_DIR)/%: | $(INCLUDE_DIR)
	$(MY_MAKE) -C $(SRC_DIR)/$* hdrs.install test_hdrs.install 

ALL_INCLUDES := $(INCLUDE_DIRS) $(OS_DEFS_H) $(OS_DEFS_S)

# NOTE: This target is "shallow" because it doesn't check for modifications at all.
# If the include directories exist, it does NOTHING!
.PHONY: hdrs.install.shallow
hdrs.install.shallow: $(ALL_INCLUDES)
	$(call DONE_MSG,Headers Installed [shallow])

# These targets are NOT "shallow" since they always defer to the module makefile which DOES
# check for modification!
HDRS_INSTALL_TARS := $(addprefix hdrs.install.,$(MODS))
$(HDRS_INSTALL_TARS): hdrs.install.%:
	$(MY_MAKE) -C $(SRC_DIR)/$* hdrs.install test_hdrs.install 

.PHONY: hdrs.install $(HDRS_INSTALL_TARS)
hdrs.install: $(HDRS_INSTALL_TARS) $(OS_DEFS_H) $(OS_DEFS_S)
	$(call DONE_MSG,Headers Installed)

# Install Libraries

# NOTE: The LIBS and TEST_LIBS targets are Shallow since they ONLY CHECK FOR EXISTENCE!!!
LIBS := $(patsubst %,$(INSTALL_DIR)/lib%.a,$(MODS))
$(LIBS): $(INSTALL_DIR)/lib%.a: | $(ALL_INCLUDES)
	$(MY_MAKE) -C $(SRC_DIR)/$* lib.install

TEST_LIBS := $(patsubst %,$(INSTALL_DIR)/lib%_test.a,$(MODS))
$(TEST_LIBS): $(INSTALL_DIR)/lib%_test.a: | $(INSTALL_DIR) $(ALL_INCLUDES)
	$(MY_MAKE) -C $(SRC_DIR)/$* test_lib.install

ALL_LIBS := $(LIBS) $(TEST_LIBS)

.PHONY: lib.install.shallow
lib.install.shallow: $(ALL_LIBS)
	$(call DONE_MSG,Libraries Installed [shallow])

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

# Remember this will not attempt to rebuild libs if they already exist!
$(ELF_FILE): $(ALL_LIBS) $(OS_DEFS_LD) | $(BUILD_DIR)
	$(C_COMPILER) -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) \
		-Wl,--start-group $(LIB_FLAGS) -lgcc -Wl,--end-group

.PHONY: elf.shallow elf.shallow.dummy
elf.shallow.dummy: $(ELF_FILE)
	@echo > /dev/null
elf.shallow: $(ELF_FILE)
	$(call DONE_MSG,Kernel ELF Built [shallow])

.PHONY: elf
elf: lib.install $(OS_DEFS_LD)
	$(MY_MAKE) -C $(SRC_DIR) elf.shallow.dummy
	$(call DONE_MSG,Kernel ELF Built)

####################################################################################################

# User Apps

# NOTE: ADD APPLICATION NAMES HERE!
APPS := Test

APPS_DIR  := $(SRC_DIR)/apps

APPS_BUILD := $(BUILD_DIR)/apps
APPS_OUT := $(BUILD_DIR)/apps/out

# The symbols file is useful for compiling user application binaries.  (Depends on SHALLOW kernel rules)
$(ELF_SYMS): $(ELF_FILE) | $(BUILD_DIR)
	i686-elf-objcopy --extract-symbol $(ELF_FILE) $@

$(APPS_BUILD) $(APPS_OUT):
	mkdir -p $@

APP_BUILDS := $(addprefix $(APPS_BUILD)/,$(APPS))

$(APP_BUILDS): $(APPS_BUILD)/%: $(ELF_SYMS) | $(APPS_BUILD) $(APPS_OUT)
	$(MY_MAKE) -C $(APPS_DIR)/$* \
		BUILD_DIR=$@ \
		OUT_DIR=$(APPS_OUT) \
		INSTALL_DIR=$(INSTALL_DIR) \
		ELF_SYMS=$(ELF_SYMS) \
		bin

# NOTE: VERY IMPORTANT!
# Apps specific makefile are always invoked! However, they depend on the shallow kernel rules!

.PHONY: apps $(APP_BUILDS)
apps: $(APP_BUILDS)
	$(call DONE_MSG,Apps Built)

# Potentially move this out of here?

APPS_CLANGD_TARS := $(addprefix apps.clangd.,$(APPS))
.PHONY: apps.clangd $(APPS_CLANGD_TARS)

$(APPS_CLANGD_TARS): apps.clangd.%: 
	$(MY_MAKE) -C $(APPS_DIR)/$* \
		INSTALL_DIR=$(INSTALL_DIR) \
		clangd

apps.clangd: $(APPS_CLANGD_TARS)
	$(call DONE_MSG,App Clangd Installed)

APPS_CLEAN_CLANGD_TARS := $(addprefix apps.clean.clangd.,$(APPS))
.PHONY: apps.clean.clangd $(APPS_CLEAN_CLANGD_TARS)

$(APPS_CLEAN_CLANGD_TARS): apps.clean.clangd.%:
	$(MY_MAKE) -C $(APPS_DIR)/$* \
		clean.clangd

apps.clean.clangd: $(APPS_CLEAN_CLANGD_TARS)
	$(call DONE_MSG,App Clangd Cleaned)

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
#
# EDIT: (11/12/25) mtools will also give a file a LFN if the files name has mixed case!
# For example, naming an app "Test" instead of "test" will result in an LFN!

DISK := $(BUILD_DIR)/hdd.img


.PHONY: disk
$(DISK): $(APPS_BUILD) | $(BUILD_DIR)
	qemu-img create -f raw $@ 256M
	mkfs.fat -F 32 -s 2 $@
	mcopy -i $@ $(APPS_BUILD)/out ::/Bin
	$(call DONE_MSG,Disk Built)

disk: $(DISK)

####################################################################################################

# Bringing it all together

.PHONY: all

all: bin disk

# NOTE: This depends on the DISK existing.
# Order only deps are annoying, so it's not listed here.
qemu.bin: $(ELF_FILE)
	qemu-system-x86_64 \
		-m 4G \
		-kernel $(ELF_FILE) \
		-drive file=$(DISK),format=raw,index=0,media=disk \
		-display sdl \
		-full-screen \
		-monitor stdio
		
# Clean

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OS_DEFS_LD)
