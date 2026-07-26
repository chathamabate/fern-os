
# The below diagram outlines the most significant targets of this Makefile and their relationships!
#
# Certain targets are actually built entirely in this Makefile. For example the disk, iso, and elf.
# While others require invoking other Makefiles! For example hdrs.install, lib.install, and apps

####### DEPENDENCY GRAPH OVERVIEW #######
# 										#
#                hdrs.install 			#
# 	                  | 				#
#                     V 				#
#                lib.install 			#         
# 	                  | 				#
#                     V 				#
#                    elf 				#
#                     | 				#
#            ---------* 				#
#            |        | 				#
#            V        | 				#
#           apps      V 				#
#            |       iso  				#
#            V        | 				#
#           disk      |  				#
#            |        V 				#
#            *-----> run  				#
# 										#
#########################################

####################################################################################################


GIT_TOP ?= $(shell git rev-parse --show-toplevel)

SRC_DIR    := $(GIT_TOP)/src
BUILD_DIR   := $(GIT_TOP)/src/build
INSTALL_DIR := $(BUILD_DIR)/install
INCLUDE_DIR := $(INSTALL_DIR)/include
VENV_DIR := $(BUILD_DIR)/venv

MY_MAKE := @make --no-print-directory
DONE_MSG = @echo -e "\x1B[1;97m>> \x1B[1;92m$1 \x1B[1;97m<<\x1b[0m\n"

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
		s_gfx \
		s_util 

# Static memory layout header and linker generation:

$(BUILD_DIR) $(INCLUDE_DIR) $(INSTALL_DIR):
	mkdir -p $@

####################################################################################################

FERNCONF_VERSION := 0.1.5
VENV_ACTIVATE := $(VENV_DIR)/bin/activate

$(VENV_DIR):
	mkdir -p $@
	python3 -m venv $(VENV_DIR)
	. $(VENV_ACTIVATE) && pip install mypy fernconf==$(FERNCONF_VERSION)

.PHONY: venv
venv: $(VENV_DIR)

SCHEMA_PY := $(SRC_DIR)/config_schema.py
MYPY_TOKEN := $(BUILD_DIR)/mypy_token

# The MyPy token confirms that the schema python file has been type checked since its last update!
$(MYPY_TOKEN): $(SCHEMA_PY) | $(VENV_DIR)
	. $(VENV_ACTIVATE) && mypy $(SCHEMA_PY)
	touch $@

CONFIG_JSON := $(SRC_DIR)/config.json

CONFIG_C  := $(INCLUDE_DIR)/c_config.h
$(CONFIG_C): $(CONFIG_JSON) $(MYPY_TOKEN) | $(INCLUDE_DIR)
	. $(VENV_ACTIVATE) && python $(SCHEMA_PY) $< -f gcc -o $@

CONFIG_AS := $(INCLUDE_DIR)/as_config.h
$(CONFIG_AS): $(CONFIG_JSON) $(MYPY_TOKEN) | $(INCLUDE_DIR)
	. $(VENV_ACTIVATE) && python $(SCHEMA_PY) $< -f gas -o $@

CONFIG_LD := $(INSTALL_DIR)/ld_config.h
$(CONFIG_LD): $(CONFIG_JSON) $(MYPY_TOKEN) | $(INSTALL_DIR)
	. $(VENV_ACTIVATE) && python $(SCHEMA_PY) $< -f ld -o $@

# I haven't figured out yet how we are going to fit in a generated make file into the build
# process!
CONFIG_MK := $(INSTALL_DIR)/mk_config.mk
$(CONFIG_MK): $(CONFIG_JSON) $(MYPY_TOKEN) | $(INSTALL_DIR)
	. $(VENV_ACTIVATE) && python $(SCHEMA_PY) $< -f make -o $@
t: $(CONFIG_MK)

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

# Where shared memory pages will live
FERNOS_SHARED_START :=  0x60000000
FERNOS_SHARED_END   :=  0x8FFFFFFF

# Area where stacks will live in fernos.
FERNOS_STACK_START  := 0xA0000000
FERNOS_STACK_END    := 0xBFFFFFFF

FERNOS_END     := 0xBFFFFFFF

EPILOGUE_START := 0xC0000000
EPILOGUE_END   := 0xFFFFFFFF

FERNOS_GFX_WIDTH  := 1024
FERNOS_GFX_HEIGHT := 768
FERNOS_GFX_BPP    := 32

# All Important OS Definitions
OS_DEFS :=   PROLOGUE_START PROLOGUE_END \
			 FERNOS_START \
			 FERNOS_KERNEL_START FERNOS_KERNEL_END \
			 FERNOS_APP_START FERNOS_APP_END \
			 FERNOS_APP_ARGS_START FERNOS_APP_ARGS_END \
			 FERNOS_FREE_START FERNOS_FREE_END \
			 FERNOS_SHARED_START FERNOS_SHARED_END \
			 FERNOS_STACK_START FERNOS_STACK_END \
			 FERNOS_END \
			 EPILOGUE_START EPILOGUE_END \
			 FERNOS_GFX_WIDTH FERNOS_GFX_HEIGHT \
			 FERNOS_GFX_BPP

C_MACRO_GEN = echo "\#define $1 $($1)UL" >> $@;
LD_DEF_GEN = echo "$1 = ($($1));" >> $@; 
S_DEF_GEN = echo "\#define $1 $($1)" >> $@; 

OS_DEFS_H := $(INCLUDE_DIR)/os_defs.h
$(OS_DEFS_H): $(THIS_FILE) | $(INCLUDE_DIR)
	echo "#pragma once" > $@
	$(foreach def,$(OS_DEFS),$(call C_MACRO_GEN,$(def)))

OS_DEFS_LD := $(INSTALL_DIR)/os_defs.ld
$(OS_DEFS_LD): $(THIS_FILE) | $(INSTALL_DIR)
	rm $@; touch $@;
	$(foreach def,$(OS_DEFS),$(call LD_DEF_GEN,$(def)))

OS_DEFS_S := $(INCLUDE_DIR)/s_os_defs.h
$(OS_DEFS_S): $(THIS_FILE) | $(INCLUDE_DIR)
	rm $@; touch $@;
	$(foreach def,$(OS_DEFS),$(call S_DEF_GEN,$(def)))

####################################################################################################

# A helper macro invoking module specific make targets!
# This expects that $* resolves to the name of a module.
MOD_MAKE = $(MY_MAKE) -C $(SRC_DIR)/$* BUILD_DIR=$(BUILD_DIR)/mods/$* INSTALL_DIR=$(INSTALL_DIR)

# Install Headers

# These targets are NOT "shallow" since they always defer to the module makefile which DOES
# check for modification!
HDRS_INSTALL_TARS := $(addprefix hdrs.install.,$(MODS))
$(HDRS_INSTALL_TARS): hdrs.install.%:
	$(MOD_MAKE) hdrs.install test_hdrs.install 

.PHONY: hdrs.install $(HDRS_INSTALL_TARS)
hdrs.install: $(HDRS_INSTALL_TARS) $(OS_DEFS_H) $(OS_DEFS_S)
	$(call DONE_MSG,Headers Installed)

# Install Libraries

LIBS := $(patsubst %,$(INSTALL_DIR)/lib%.a,$(MODS))
TEST_LIBS := $(patsubst %,$(INSTALL_DIR)/lib%_test.a,$(MODS))
ALL_LIBS := $(LIBS) $(TEST_LIBS)

LIB_INSTALL_TARS := $(addprefix lib.unsafe_install.,$(MODS))
.PHONY: lib.unsafe_install lib.install $(LIB_INSTALL_TARS)

# This is "unsafe" since there is no gaurantee that the headers have been installed.
$(LIB_INSTALL_TARS): lib.unsafe_install.%:
	$(MOD_MAKE) lib.install test_lib.install
lib.unsafe_install: $(LIB_INSTALL_TARS)

# This is safe becuase we gaurantee headers are installed first!
lib.install: hdrs.install
	$(MY_MAKE) -C $(SRC_DIR) lib.unsafe_install
	$(call DONE_MSG,Libraries Installed)

# Clangd Helpers

CLANGD_TARS := $(addprefix clangd.,$(MODS))
.PHONY: clangd $(CLANGD_TARS)

$(CLANGD_TARS): clangd.%:
	$(MOD_MAKE) clangd

clangd: $(CLANGD_TARS)
	$(call DONE_MSG,Clangd Installed)

CLANGD_CLEAN_TARS := $(addprefix clean.clangd.,$(MODS))
.PHONY: clean.clangd $(CLANGD_CLEAN_TARS)

$(CLANGD_CLEAN_TARS): clean.clangd.%:
	$(MOD_MAKE) clean.clangd

clean.clangd: $(CLANGD_CLEAN_TARS)
	$(call DONE_MSG,Clang Cleaned)

####################################################################################################

# Kernel Compilation

OS_NAME := fernos
C_COMPILER := i686-elf-gcc

LDSCRIPT := $(SRC_DIR)/linker.ld

ELF_FILE := $(BUILD_DIR)/$(OS_NAME).elf
ELF_SYMS := $(BUILD_DIR)/$(OS_NAME).syms

LIB_FLAGS := $(foreach mod,$(MODS),-l$(mod)_test -l$(mod))

# Realize that ALL_LIBS doesn't actually define a rule for itself. This ELF target should NEVER
# be invoked without libs already being explicity installed previously!
$(ELF_FILE): $(ALL_LIBS) $(OS_DEFS_LD) | $(BUILD_DIR)
	$(C_COMPILER) -T $(LDSCRIPT) -o $@ -ffreestanding -nostdlib -O2 -L$(INSTALL_DIR) \
		-Wl,--start-group -lgcc -Wl,--whole-archive $(LIB_FLAGS) -Wl,--end-group 

# The symbols file is useful for compiling user application binaries.  (Depends on SHALLOW kernel rules)
$(ELF_SYMS): $(ELF_FILE) | $(BUILD_DIR)
	i686-elf-objcopy --extract-symbol $(ELF_FILE) $@

.PHONY: elf.unsafe
elf.unsafe: $(ELF_FILE) $(ELF_SYMS)
	@echo > /dev/null

.PHONY: elf
elf: lib.install
	$(MY_MAKE) -C $(SRC_DIR) elf.unsafe
	$(call DONE_MSG,Kernel ELF Built)

####################################################################################################

# User Apps

# NOTE: ADD APPLICATION NAMES HERE!
_APPS := Test \
		Shell

APPS_DIR  := $(SRC_DIR)/apps

APPS_BUILD := $(BUILD_DIR)/apps/build
APPS_OUT := $(BUILD_DIR)/apps/bin

APPS := $(addprefix $(APPS_OUT)/,$(_APPS))

# Similar to MOD_MAKE macro, but for apps
APP_MAKE = $(MY_MAKE) -C $(APPS_DIR)/$* \
				BUILD_DIR=$(APPS_BUILD)/$* \
				OUT_DIR=$(APPS_OUT) \
				INSTALL_DIR=$(INSTALL_DIR) \
				ELF_SYMS=$(ELF_SYMS) 

APP_TARS := $(addprefix apps.unsafe.,$(_APPS))
.PHONY: apps.unsafe $(APP_TARS)

$(APP_TARS): apps.unsafe.%:
	$(APP_MAKE) bin

apps.unsafe: $(APP_TARS)

.PHONY: apps
apps: elf
	$(MY_MAKE) -C $(SRC_DIR) apps.unsafe
	$(call DONE_MSG,Apps Built)

# Potentially move this out of here?

APPS_CLANGD_TARS := $(addprefix apps.clangd.,$(_APPS))
.PHONY: apps.clangd $(APPS_CLANGD_TARS)

$(APPS_CLANGD_TARS): apps.clangd.%: 
	$(APP_MAKE) clangd

apps.clangd: $(APPS_CLANGD_TARS)
	$(call DONE_MSG,App Clangd Installed)

APPS_CLEAN_CLANGD_TARS := $(addprefix apps.clean.clangd.,$(_APPS))
.PHONY: apps.clean.clangd $(APPS_CLEAN_CLANGD_TARS)

$(APPS_CLEAN_CLANGD_TARS): apps.clean.clangd.%:
	$(APP_MAKE) clean.clangd

apps.clean.clangd: $(APPS_CLEAN_CLANGD_TARS)
	$(call DONE_MSG,App Clangd Cleaned)

####################################################################################################

# ISO File (Soon disk setup will be swallowed by this section)

ISO_DIR  := $(BUILD_DIR)/iso
GRUB_CFG := $(SRC_DIR)/grub.cfg
ISO_FILE := $(BUILD_DIR)/$(OS_NAME).iso

$(ISO_DIR) $(BOOT_DIR) $(GRUB_DIR):
	mkdir -p $@

$(ISO_FILE): $(ELF_FILE) $(GRUB_CFG)
	grub-file --is-x86-multiboot2 $(ELF_FILE)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(ELF_FILE) $(ISO_DIR)/boot/
	cp $(GRUB_CFG) $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $@ $(ISO_DIR)

.PHONY: iso.unsafe iso

iso.unsafe: $(ISO_FILE)
	@echo > /dev/null

iso: elf
	$(MY_MAKE) -C $(SRC_DIR) iso.unsafe
	$(call DONE_MSG,ISO Built)

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

$(DISK): $(APPS) | $(BUILD_DIR)
	qemu-img create -f raw $@ 256M
	mkfs.fat -F 32 -s 2 $@
	mcopy -i $@ $(APPS_OUT) ::/Bin

.PHONY: disk disk.unsafe
disk.unsafe: $(DISK)
	@echo > /dev/null

disk: apps
	$(MY_MAKE) -C $(SRC_DIR) disk.unsafe
	$(call DONE_MSG,Disk Built)

####################################################################################################

# Bringing it all together

.PHONY: run run.shallow

run: iso disk
	qemu-system-x86_64 \
		-m 4G \
		-boot d \
		-cdrom $(ISO_FILE) \
		-drive file=$(DISK),format=raw,index=0,media=disk \
		-display gtk \
		-monitor stdio

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR)

