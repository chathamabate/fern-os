# NOTE: This stub is meant to be included
GIT_TOP ?= $(shell git rev-parse --show-toplevel)

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?=

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?=

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?=

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?=

# OPTIONAL: Where to place build artifacts
BUILD_DIR   ?= $(GIT_TOP)/src/build/mods/$(MOD_NAME)

# OPTIONAL: Where to place .a files and headers
INSTALL_DIR ?= $(GIT_TOP)/src/build/install

# OPTIONAL: Extra C flags to use
EXTRA_CFLAGS ?=

#####################################################################################

# I think it's safe to say that for the entirety of this project I'll be using
# the 386 tools.
C_COMPILER := i686-elf-gcc
ASSEMBLER  := i686-elf-as
AR 		   := i686-elf-ar

CFLAGS := -m32 -std=gnu99 -ffreestanding -Wall -Wextra -Wpedantic $(EXTRA_CFLAGS)

# Where all module directories live
MODS_DIR := $(GIT_TOP)/src

# Uncompiled files

MOD_DIR := $(MODS_DIR)/$(MOD_NAME)
INSTALL_INC_DIR := $(INSTALL_DIR)/include

INC_DIR		 := $(MOD_DIR)/include
INC_INC_DIRS := $(INC_DIR) $(INSTALL_INC_DIR)
INC_INC_FLAGS:= $(addprefix -I,$(INC_INC_DIRS))
HDRS	 	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/*.h)
TEST_HDRS	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/test/*.h)

SRC_DIR 	 := $(MOD_DIR)/src
SRC_INC_DIRS := $(SRC_DIR) $(INC_DIR) $(INSTALL_INC_DIR)
SRC_INC_FLAGS:= $(addprefix -I,$(SRC_INC_DIRS))
ASM_INC_FLAGS:= $(addprefix -I ,$(SRC_INC_DIRS))
SRCS 		 := $(addprefix $(SRC_DIR)/,$(_SRCS))
ASMS		 := $(addprefix $(SRC_DIR)/,$(_ASMS))

TEST_DIR 	  := $(MOD_DIR)/test
TEST_INC_DIRS := $(TEST_DIR) $(INC_DIR) $(INSTALL_INC_DIR)
TEST_INC_FLAGS:= $(addprefix -I,$(TEST_INC_DIRS))
TEST_SRCS 	  := $(addprefix $(TEST_DIR)/,$(_TEST_SRCS))

# Stuff to be built

C_OBJS	:= $(patsubst %.c,$(BUILD_DIR)/c_%.o,$(_SRCS))
S_OBJS	:= $(patsubst %.s,$(BUILD_DIR)/s_%.o,$(_ASMS))

_LIB 		:= lib$(MOD_NAME).a
BUILD_LIB	:= $(BUILD_DIR)/$(_LIB)
INSTALL_LIB	:= $(INSTALL_DIR)/$(_LIB)

BUILD_TEST_DIR := $(BUILD_DIR)/test
TEST_OBJS	   := $(patsubst %.c,$(BUILD_TEST_DIR)/%.o,$(_TEST_SRCS))

_TEST_LIB		 := lib$(MOD_NAME)_test.a
BUILD_TEST_LIB 	 := $(BUILD_TEST_DIR)/$(_TEST_LIB)
INSTALL_TEST_LIB := $(INSTALL_DIR)/$(_TEST_LIB)

INSTALL_HDRS_DIR      := $(INSTALL_INC_DIR)/$(MOD_NAME)
INSTALL_HDRS := $(addprefix $(INSTALL_HDRS_DIR)/,$(notdir $(HDRS)))

INSTALL_TEST_HDRS_DIR := $(INSTALL_HDRS_DIR)/test
INSTALL_TEST_HDRS := $(addprefix $(INSTALL_TEST_HDRS_DIR)/,$(notdir $(TEST_HDRS)))

# Normal Build Targets

$(BUILD_DIR) $(BUILD_TEST_DIR) $(INSTALL_DIR):
	mkdir -p $@

.PHONY: lib.build test_lib.build

# NOTE: We only rebuild on modification to headers in this module.
# We treat installed dependencies in the install dir as unchanging.
$(C_OBJS): $(BUILD_DIR)/c_%.o: $(SRC_DIR)/%.c $(HDRS) | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(SRC_INC_FLAGS) -o $@ $<

$(S_OBJS): $(BUILD_DIR)/s_%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(ASSEMBLER) $(SRC_INC_FLAGS) -o $@ $<

lib.build: $(BUILD_LIB)
$(BUILD_LIB): $(C_OBJS) $(S_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

# Testing Build Targets

$(TEST_OBJS): $(BUILD_TEST_DIR)/%.o: $(TEST_DIR)/%.c $(TEST_HDRS) $(HDRS) | $(BUILD_TEST_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(TEST_INC_FLAGS) -o $@ $<

test_lib.build: $(BUILD_TEST_LIB)
$(BUILD_TEST_LIB): $(TEST_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

# Install Targets


$(INSTALL_HDRS_DIR) $(INSTALL_TEST_HDRS_DIR):
	mkdir -p $@

.PHONY: hdrs.install lib.install test_hdrs.install test_lib.install

hdrs.install: $(INSTALL_HDRS)
	@echo > /dev/null

$(INSTALL_HDRS): $(INSTALL_HDRS_DIR)/%.h: $(INC_DIR)/$(MOD_NAME)/%.h | $(INSTALL_HDRS_DIR)
	cp $< $@

lib.install: $(INSTALL_LIB) 
	@echo > /dev/null

$(INSTALL_LIB): $(BUILD_LIB) | $(INSTALL_DIR)
	cp $< $@

test_hdrs.install: $(INSTALL_TEST_HDRS)
	@echo > /dev/null

$(INSTALL_TEST_HDRS): $(INSTALL_TEST_HDRS_DIR)/%.h: $(INC_DIR)/$(MOD_NAME)/test/%.h | $(INSTALL_TEST_HDRS_DIR)
	cp $< $@

test_lib.install: $(INSTALL_TEST_LIB)
	@echo > /dev/null

$(INSTALL_TEST_LIB): $(BUILD_TEST_LIB) | $(INSTALL_DIR)
	cp $< $@

# Clangd Files

# Expects 
# $(1): Clangd File
# $(2): Flags
define CLANGD_HELPER
echo "CompileFlags:" > $1
echo "  Add:" >> $1
$(foreach fl,$(2),echo "  - $(fl)" >> $1;)
endef

INC_CLANGD := $(INC_DIR)/.clangd
$(INC_CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) $(INC_INC_FLAGS))

SRC_CLANGD := $(SRC_DIR)/.clangd
$(SRC_CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) $(SRC_INC_FLAGS))

TEST_CLANGD := $(TEST_DIR)/.clangd
$(TEST_CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) $(TEST_INC_FLAGS))

.PHONY: clangd

CLANGDS := $(INC_CLANGD) $(SRC_CLANGD) $(TEST_CLANGD)
clangd: $(CLANGDS) 
	@echo > /dev/null

# clean targets

.PHONY: clean clean.clangd uninstall clean.deep

clean: 
	rm -rf $(BUILD_DIR)

clean.clangd:
	rm -f $(CLANGDS)

uninstall:
	rm -f $(INSTALL_LIB)
	rm -f $(INSTALL_TEST_LIB)
	rm -rf $(INSTALL_HDRS_DIR)

clean.deep: clean clean.clangd uninstall

