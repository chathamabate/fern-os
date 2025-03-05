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

STND_ALN_DIR := $(GIT_TOP)/retry/standalone

# Include directories of our dependencies
DEP_INC_DIRS 	:= $(patsubst %,$(STND_ALN_DIR)/%/include,$(DEPS))

# Uncompiled files

MOD_DIR := $(STND_ALN_DIR)/$(MOD_NAME)

INC_DIR		 := $(MOD_DIR)/include
INC_INC_DIRS := $(INC_DIR) $(DEP_INC_DIRS)
HDRS	 	 := $(wildcard $(INC_DIR)/$(MOD_NAME)/*.h)

SRC_DIR 	 := $(MOD_DIR)/src
SRC_INC_DIRS := $(SRC_DIR) $(INC_DIR) $(DEP_INC_DIRS)
SRCS 		 := $(addprefix $(SRC_DIR)/,$(_SRCS))

TEST_DIR 	  := $(MOD_DIR)/test
TEST_INC_DIRS := $(TEST_DIR) $(DEP_INC_DIRS)
TEST_SRCS 	  := $(addprefix $(TEST_DIR)/,$(_TEST_SRCS))
TEST_HDRS	  := $(wildcard $(TEST_DIR)/*.h)

# Compiled files

OBJS	:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(_SRCS))

_LIB 		:= lib$(MOD_NAME).a
BUILD_LIB	:= $(BUILD_DIR)/$(_LIB)
INSTALL_LIB	:= $(INSTALL_DIR)/$(_LIB)

BUILD_TEST_DIR := $(BUILD_DIR)/test
TEST_OBJS	:= $(patsubst %.c,$(BUILD_TEST_DIR)/%.o,$(_TEST_SRCS))

# Normal Targets

$(BUILD_DIR) $(BUILD_TEST_DIR) $(INSTALL_DIR):
	mkdir -p $@

# NOTE: We only rebuild on modification to headers in this module.
# We treat installed dependencies in the install dir as unchanging.
$(OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS) | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(addprefix -I,$(SRC_INC_DIRS)) -o $@ $<

$(BUILD_LIB): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

$(INSTALL_LIB): $(BUILD_LIB) | $(INSTALL_DIR)
	cp $< $@

# Testing Targets

TEST_INC_FLAGS := -I

$(TEST_OBJS): $(BUILD_TEST_DIR)/%.o: $(TEST_DIR)/%.c $(TEST_HDRS) $(HDRS) | $(BUILD_DIR)
	$(C_COMPILER) -c $(CFLAGS) $(addprefix -I,$(TEST_INC_DIRS)) -o $@ $<




