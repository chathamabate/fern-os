
# NOTE: This Makefile is meant to be INVOKED, NOT INCLUDED!

GIT_TOP ?= $(shell git rev-parse --show-toplevel)

# REQUIRED
#
# Where to place intermediary build artifacts. (Will be created if non-existent)
BUILD_DIR ?=

# REQUIRED
#
# Where to place the final binary such that it is ultimately
# placed in the disk image! (We expect this to already exist)
HOST_APPS_DIR ?=

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

C_COMPILER := i686-elf-gcc

APP_NAME := Test

SRC_DIR    	  := $(GIT_TOP)/src
APP_DIR 	  := $(SRC_DIR)/test_app
APP_LDSCRIPT  := $(SRC_DIR)/linker_app.ld
INCLUDE_DIR   := $(INSTALL_DIR)/include

APP   	    := $(BUILD_DIR)/$(APP_NAME)
HOST_APP 	:= $(HOST_APPS_DIR)/$(APP_NAME)

$(BUILD_DIR):
	mkdir -p $@

# This is kinda overkill because we just have one source file right now, but whatever.
_SRCS := test_app.c
SRCS  := $(addprefix $(APP_DIR)/,$(_SRCS))
OBJS  := $(patsubst %.c,$(BUILD_DIR)/%.o,$(_SRCS)) 
LIB   := $(BUILD_DIR)/lib$(APP_NAME).a

CFLAGS := -m32 -std=gnu99 -ffreestanding -Wall -Wextra -Wpedantic 

$(OBJS): $(BUILD_DIR)/%.o: $(APP_DIR)/%.c $(ELF_SYMS) | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) -I$(INCLUDE_DIR) -o $@ $<
	@echo "$?"

# NOTE: in the below recipe we give -L$(INSTALL_DIR) so we have access to os_defs.ld
# This recipe DOES NOT use any FernOS libraries directly! (The point of the symbols file)
$(APP): $(OBJS) | $(BUILD_DIR)
	$(C_COMPILER) -T $(APP_LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) \
	    -lgcc -Wl,--just-symbols=$(ELF_SYMS) $^

$(HOST_APP): $(APP)
	cp $< $@

.PHONY: bin clean

bin: $(HOST_APP)

clean:
	rm -rf $(BUILD_DIR)

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
	$(call CLANGD_HELPER,$@,$(CFLAGS) -I$(INCLUDE_DIR))

.PHONY: clangd clean.clangd

clangd: $(CLANGD)
	@echo > /dev/null

clean.clangd:
	rm -f $(CLANGD)


