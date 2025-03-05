# NOTE: This stub is meant to be included

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?=

# REQUIRED: All standalone modules this module must include
DEPS		?=

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?=

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?=

# REQUIRED: Where to place build artifacts
BUILD_DIR   ?=

# REQUIRED: Where to place .a files and headers
INSTALL_DIR ?=

# OPTIONAL: Which compiler to use
C_COMPILER ?= gcc

# OPTIONAL: Which archiver to use
AR ?= ar

# OPTIONAL: Extra C flags to use
EXTRA_CFLAGS ?=

#####################################################################################

CFLAGS := -Wall -Wextra -Wpedantic $(EXTRA_CFLAGS)

GIT_TOP ?= $(shell git rev-parse --show-toplevel)

# Where all module directories live
MODS_DIR := $(GIT_TOP)/retry/src

# Uncompiled files

MOD_DIR := $(MODS_DIR)/$(MOD_NAME)

INC_DIR		 := $(MOD_DIR)/include
INC_INC_DIRS := $(INC_DIR) $(INSTALL_DIR)
INC_INC_FLAGS:= $(addprefix -I,$(INC_INC_DIRS))
HDRS	 	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/*.h)
TEST_HDRS	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/test/*.h)

SRC_DIR 	 := $(MOD_DIR)/src
SRC_INC_DIRS := $(SRC_DIR) $(INC_DIR) $(INSTALL_DIR)
SRC_INC_FLAGS:= $(addprefix -I,$(SRC_INC_DIRS))
SRCS 		 := $(addprefix $(SRC_DIR)/,$(_SRCS))

TEST_DIR 	  := $(MOD_DIR)/test
TEST_INC_DIRS := $(TEST_DIR) $(INC_DIR) $(INSTALL_DIR)
TEST_INC_FLAGS:= $(addprefix -I,$(TEST_INC_DIRS))
TEST_SRCS 	  := $(addprefix $(TEST_DIR)/,$(_TEST_SRCS))

# Compiled files

OBJS	:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(_SRCS))

_LIB 		:= lib$(MOD_NAME).a
BUILD_LIB	:= $(BUILD_DIR)/$(_LIB)
INSTALL_LIB	:= $(INSTALL_DIR)/$(_LIB)

BUILD_TEST_DIR := $(BUILD_DIR)/test
TEST_OBJS	:= $(patsubst %.c,$(BUILD_TEST_DIR)/%.o,$(_TEST_SRCS))

_TEST_LIB		 := libtest_$(MOD_NAME).a
BUILD_TEST_LIB 	 := $(BUILD_TEST_DIR)/$(_TEST_LIB)
INSTALL_TEST_LIB := $(INSTALL_DIR)/$(_TEST_LIB)

# Normal Targets

$(BUILD_DIR) $(BUILD_TEST_DIR) $(INSTALL_DIR):
	mkdir -p $@

# NOTE: We only rebuild on modification to headers in this module.
# We treat installed dependencies in the install dir as unchanging.
$(OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS) | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(SRC_INC_FLAGS) -o $@ $<

$(BUILD_LIB): $(OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

$(INSTALL_LIB): $(BUILD_LIB) | $(INSTALL_DIR)
	cp $< $@

# Testing Targets

$(TEST_OBJS): $(BUILD_TEST_DIR)/%.o: $(TEST_DIR)/%.c $(TEST_HDRS) $(HDRS) | $(BUILD_TEST_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(TEST_INC_FLAGS) -o $@ $<

$(BUILD_TEST_LIB): $(TEST_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

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
	$(call CLANGD_HELPER,$@,$(INC_INC_FLAGS))

SRC_CLANGD := $(SRC_DIR)/.clangd
$(SRC_CLANGD):
	$(call CLANGD_HELPER,$@,$(SRC_INC_FLAGS))

TEST_CLANGD := $(TEST_DIR)/.clangd
$(TEST_CLANGD):
	$(call CLANGD_HELPER,$@,$(TEST_INC_FLAGS))

CLANGDS := $(INC_CLANGD) $(SRC_CLANGD) $(TEST_CLANGD)
clangd: $(CLANGDS) 
clangd.clean:
	rm -f $(CLANGDS)





