# NOTE: This stub is meant to be included

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
ifeq ($(MOD_NAME),)
$(error module name is required)
endif

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?=

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?=

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?=

# REQUIRED: Where to place build artifacts
ifeq ($(BUILD_DIR),)
$(error $(MOD_NAME) requires build directory to build)
endif

# REQUIRED: Where to place .a files and headers
ifeq ($(INSTALL_DIR),)
$(error $(MOD_NAME) requires install dir to build)
endif

# OPTIONAL: Extra C flags to use
EXTRA_CFLAGS ?=

#####################################################################################

GIT_TOP := $(shell git rev-parse --show-toplevel)

# I think it's safe to say that for the entirety of this project I'll be using
# the 386 tools.
C_COMPILER := i686-elf-gcc
AR 		   := i686-elf-ar

CFLAGS := -m32 -std=gnu99 -ffreestanding -Wall -Wextra -Wpedantic $(EXTRA_CFLAGS)

# Where all module directories live
MODS_DIR := $(GIT_TOP)/src

# Uncompiled files

MOD_DIR := $(MODS_DIR)/$(MOD_NAME)
INSTALL_INC_DIR := $(INSTALL_DIR)/include

# NOTE: 
# SRC_INC_DIRS, and TEST_INC_DIRS used to both include $(INC_DIR) before
# $(INSTALL_INC_DIR). The intention was that during compilation this modules headers in the
# source tree would take precedence over its own headers which live in the install tree!
#
# When switching over to using .d files though, this file now requires that hdrs.install
# is called on the top level Makefile before any lib building targets are called via this file.
#
# This means, if you call a lib.install at the top level, it will gaurantee that all hdrs in 
# install are up to date before attempting any compilation!

INC_DIR		 := $(MOD_DIR)/include
HDRS	 	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/*.h)
TEST_HDRS	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/test/*.h)

SRC_DIR 	 := $(MOD_DIR)/src
SRC_INC_DIRS := $(SRC_DIR) $(INSTALL_INC_DIR)
SRC_INC_FLAGS:= $(addprefix -I,$(SRC_INC_DIRS))
SRCS 		 := $(addprefix $(SRC_DIR)/,$(_SRCS))
ASMS		 := $(addprefix $(SRC_DIR)/,$(_ASMS))

TEST_DIR 	  := $(MOD_DIR)/test
TEST_INC_DIRS := $(TEST_DIR) $(INSTALL_INC_DIR)
TEST_INC_FLAGS:= $(addprefix -I,$(TEST_INC_DIRS))
TEST_SRCS 	  := $(addprefix $(TEST_DIR)/,$(_TEST_SRCS))

# Stuff to be built

C_DOTDS	:= $(patsubst %.c,$(BUILD_DIR)/c_%.d,$(_SRCS))
S_DOTDS	:= $(patsubst %.S,$(BUILD_DIR)/S_%.d,$(_ASMS))

C_OBJS	:= $(patsubst %.c,$(BUILD_DIR)/c_%.o,$(_SRCS))
S_OBJS	:= $(patsubst %.S,$(BUILD_DIR)/S_%.o,$(_ASMS))

_LIB 		:= lib$(MOD_NAME).a
BUILD_LIB	:= $(BUILD_DIR)/$(_LIB)
INSTALL_LIB	:= $(INSTALL_DIR)/$(_LIB)

BUILD_TEST_DIR := $(BUILD_DIR)/test

TEST_DOTDS 		:= $(patsubst %.c,$(BUILD_TEST_DIR)/%.d,$(_TEST_SRCS))
TEST_OBJS 		:= $(patsubst %.c,$(BUILD_TEST_DIR)/%.o,$(_TEST_SRCS))

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

# The targets in this file are meant to be defined in the very order they should be invoked!

# 1) Header installation
#
# This should be done first for all modules! .d file generation must be able to discover ALL
# referenced headers!

$(INSTALL_HDRS_DIR) $(INSTALL_TEST_HDRS_DIR):
	mkdir -p $@

.PHONY: hdrs.install test_hdrs.install

$(INSTALL_HDRS): $(INSTALL_HDRS_DIR)/%.h: $(INC_DIR)/$(MOD_NAME)/%.h | $(INSTALL_HDRS_DIR)
	cp $< $@

hdrs.install: $(INSTALL_HDRS)
	@echo > /dev/null

$(INSTALL_TEST_HDRS): $(INSTALL_TEST_HDRS_DIR)/%.h: $(INC_DIR)/$(MOD_NAME)/test/%.h | $(INSTALL_TEST_HDRS_DIR)
	cp $< $@

test_hdrs.install: $(INSTALL_TEST_HDRS)
	@echo > /dev/null

# 2) .d File generation
#
# Again, this will only succeed if ALL referenced headers (even those outside this module)
# can be found via the include path!

.PHONY: lib.dotds test_lib.dotds
$(C_DOTDS): $(BUILD_DIR)/c_%.d: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(C_COMPILER) $(SRC_INC_FLAGS) -E $< -MM -MT "$(BUILD_DIR)/c_$*.o" -MF $@

$(S_DOTDS): $(BUILD_DIR)/S_%.d: $(SRC_DIR)/%.S | $(BUILD_DIR)
	$(C_COMPILER) $(SRC_INC_FLAGS) -E $< -MM -MT "$(BUILD_DIR)/S_$*.o" -MF $@

lib.dotds: $(C_DOTDS) $(S_DOTDS)

$(TEST_DOTDS): $(BUILD_TEST_DIR)/%.d: $(TEST_DIR)/%.c | $(BUILD_TEST_DIR)
	$(C_COMPILER) $(SRC_INC_FLAGS) -E $< -MM -MT "$(BUILD_TEST_DIR)/$*.o" -MF $@

test_lib.dotds: $(TEST_DOTDS)

# 3) Object compilation and library creation!
#
# We include the .d files conditionally! This way, are not attempted to be generated for targets
# which don't require them!

.PHONY: lib.build test_lib.build

ifneq ($(filter lib.build,$(MAKECMDGOALS)),)
include $(C_DOTDS) $(S_DOTDS)
endif

ifneq ($(filter test_lib.build,$(MAKECMDGOALS)),)
include $(TEST_DOTDS)
endif

# .d files will declare all significant dependencies!
$(C_OBJS): $(BUILD_DIR)/c_%.o: | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(SRC_INC_FLAGS) -o $@ $(SRC_DIR)/$*.c

$(S_OBJS): $(BUILD_DIR)/S_%.o: | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(SRC_INC_FLAGS) -o $@ $(SRC_DIR)/$*.S

$(BUILD_LIB): $(C_OBJS) $(S_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

lib.build: $(BUILD_LIB)

# Testing Build Targets

$(TEST_OBJS): $(BUILD_TEST_DIR)/%.o: | $(BUILD_TEST_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(TEST_INC_FLAGS) -o $@ $(TEST_DIR)/$*.c

$(BUILD_TEST_LIB): $(TEST_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

test_lib.build: $(BUILD_TEST_LIB)

# 4) Install compiled artifacts!
#
# Remember, headers should've been installed earlier!

.PHONY: lib.install test_lib.install

ifneq ($(filter lib.install,$(MAKECMDGOALS)),)
include $(C_DOTDS) $(S_DOTDS)
endif

ifneq ($(filter test_lib.install,$(MAKECMDGOALS)),)
include $(TEST_DOTDS)
endif

$(INSTALL_LIB): $(BUILD_LIB) | $(INSTALL_DIR)
	cp $< $@

lib.install: $(INSTALL_LIB) 
	@echo > /dev/null

$(INSTALL_TEST_LIB): $(BUILD_TEST_LIB) | $(INSTALL_DIR)
	cp $< $@

test_lib.install: $(INSTALL_TEST_LIB)
	@echo > /dev/null

# *) Clangd generation and cleanup!
# 
# (Can be called out of order just fine)

# Clangd Files

# Expects 
# $(1): Clangd File
# $(2): Flags
define CLANGD_HELPER
echo "CompileFlags:" > $1
echo "  Add:" >> $1
$(foreach fl,$(2),echo "  - $(fl)" >> $1;)
endef

# Here we include $(INC_DIR) just so we can see our changes while editing without needing
# to call hdrs.install

INC_CLANGD_INC_DIRS := $(INC_DIR) $(INSTALL_INC_DIR)
INC_CLANGD_INC_FLAGS:= $(addprefix -I,$(INC_CLANGD_INC_DIRS))
INC_CLANGD := $(INC_DIR)/.clangd
$(INC_CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) $(INC_CLANGD_INC_FLAGS))

SRC_CLANGD_INC_DIRS := $(SRC_DIR) $(INC_DIR) $(INSTALL_INC_DIR)
SRC_CLANGD_INC_FLAGS:= $(addprefix -I,$(SRC_CLANGD_INC_DIRS))
SRC_CLANGD := $(SRC_DIR)/.clangd
$(SRC_CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) $(SRC_CLANGD_INC_FLAGS))

TEST_CLANGD_INC_DIRS := $(TEST_DIR) $(INC_DIR) $(INSTALL_INC_DIR)
TEST_CLANGD_INC_FLAGS:= $(addprefix -I,$(TEST_CLANGD_INC_DIRS))
TEST_CLANGD := $(TEST_DIR)/.clangd
$(TEST_CLANGD):
	$(call CLANGD_HELPER,$@,$(CFLAGS) $(TEST_CLANGD_INC_FLAGS))

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

