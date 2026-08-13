
# This is a stub which helps with simple app building!
# NOTE: Your apps by no means need to use this stub. It is very much optional!
# This stub supports .c and .h files defined in the apps subdir directly.

# This Makefile is both meant to be included and invoked with arguments!

####################################################################################################

# The following args are meant to be defined in the Makefile which INCLUDES
# this stub!

# REQUIRED
#
# The name of this application. MUST be the same as this apps directory name.
ifeq ($(APP_NAME),)
$(error App specified with no app name)
endif

# REQUIRED
#
# The names of all .c files within `src/apps/$(APP_NAME)`
_SRCS ?=

####################################################################################################

# The following args are meant to be given when the Makefile which includes this
# stub is invoked.

# REQUIRED
#
# Where to place intermediary build artifacts
ifeq ($(BUILD_DIR),)
$(error $(APP_NAME) has no build directory specified)
endif

# REQUIRED
#
# Where to place final binary
ifeq ($(OUT_DIR),)
$(error $(APP_NAME) has no out directory specified)
endif

# REQUIRED
#
# Where FernOS built libraries and copied headers are placed. (MUST EXIST)
# NOTE: We expect the INCLUDE_DIR to be at $(INSTALL_DIR)/include
ifeq ($(INSTALL_DIR),)
$(error $(APP_NAME) has no install directory specified)
endif

# REQUIRED
#
# ELF Symbols file. (MUST EXIST)
ifeq ($(ELF_SYMS),)
$(error $(APP_NAME) has no elf symbols file specified)
endif

# REQUIRED
#
# What linkerscript to use (MUST EXIST)
# 
# This should really be the same for all apps. It's just that now linkerscripts in this project
# are preprocessed. It is the caller's responsibility to create the linkerscript and tell
# me where it is.
ifeq ($(APP_LDSCRIPT),)
$(error $(APP_NAME) has no linkerscript specified)
endif

####################################################################################################

GIT_TOP := $(shell git rev-parse --show-toplevel)
SRC_DIR := $(GIT_TOP)/src
INCLUDE_DIR   := $(INSTALL_DIR)/include
APP_DIR := $(SRC_DIR)/apps/$(APP_NAME)
INCLUDE_FLAGS := -I$(APP_DIR) -I$(INCLUDE_DIR)
APP := $(OUT_DIR)/$(APP_NAME)

C_COMPILER := i686-elf-gcc

SRCS := $(addprefix $(APP_DIR)/,$(_SRCS))

DOTDS 	:= $(patsubst %.c,$(BUILD_DIR)/%.d,$(_SRCS))
OBJS 	:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(_SRCS))

# This stub is similar to mod_stub.mk (just a little simpler)
# To generate the .d files, all referenced headers MUST be installed!

$(BUILD_DIR) $(OUT_DIR):
	mkdir -p $@

$(DOTDS): $(BUILD_DIR)/%.d: $(APP_DIR)/%.c | $(BUILD_DIR)
	$(C_COMPILER) $(INCLUDE_FLAGS) -E $< -MM -MT "$(BUILD_DIR)/$*.o" -MF $@

.PHONY: dotds
dotds: $(DOTDS)

ifneq ($(filter bin,$(MAKECMDGOALS)),)
include $(DOTDS)
endif

# We rebuild an object if any of its source file or referenced headers change (using .d files)
CFLAGS := -m32 -std=gnu99 -ffreestanding -Wall -Wextra -Wpedantic 
$(OBJS): $(BUILD_DIR)/%.o: | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) -I$(INCLUDE_DIR) -I$(APP_DIR) -o $@ $<

# NOTE: in the below recipe we give -L$(INSTALL_DIR) so we have access to os_defs.ld
# This recipe DOES NOT use any FernOS libraries directly! (The point of the symbols file)
$(APP): $(OBJS) $(ELF_SYMS) $(APP_LDSCRIPT) | $(OUT_DIR)
	$(C_COMPILER) -T $(APP_LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) \
	    -lgcc -Wl,--just-symbols=$(ELF_SYMS) $(OBJS)

.PHONY: bin
bin: $(APP)
	@echo > /dev/null

# Expects 
# $(1): Clangd File
# $(2): Flags
define CLANGD_HELPER
echo "CompileFlags:" > $1
echo "  Add:" >> $1
$(foreach fl,$(2),echo "  - $(fl)" >> $1;)
endef

CLANGD := $(APP_DIR)/.clangd
$(CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) -I$(INCLUDE_DIR) -I$(APP_DIR))

.PHONY: clangd clean.clangd

clangd: $(CLANGD)
	@echo > /dev/null

clean.clangd:
	rm -f $(CLANGD)
