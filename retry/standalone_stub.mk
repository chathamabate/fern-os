
# NOTE: This stub is meant to be invoked, not included

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?=

# REQUIRED: All standalone modules this module must include
DEPS		?=

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?=

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS 		?=

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?=

# REQUIRED: Where to place build artifacts
BUILD_DIR   ?=

# REQUIRED: Where to place .a files and headers
INSTALL_DIR ?=

# OPTIONAL: Which compiler to use
CC ?= gcc

# OPTIONAL: Extra C flags to use
EXTRA_CFLAGS ?=

#####################################################################################

CFLAGS := -Wall -Wextra -Wpedantic $(EXTRA_CFLAGS)

GIT_TOP := $(shell git rev-parse --show-toplevel)

STND_ALN_DIR := $(GIT_TOP)/retry/standalone

MOD_DIR := $(STND_ALN_DIR)/$(MOD_NAME)

INC_DIR	:= $(MOD_DIR)/include
HDRS	:= $(wildcard $(INC_DIR)/$(MOD_NAME)/*.h)

SRC_DIR := $(MOD_DIR)/src
SRCS 	:= $(addprefix $(SRC_DIR)/,$(_SRCS))
ASMS 	:= $(addprefix $(SRC_DIR)/$(_ASMS))

TEST_DIR 	:= $(MOD_DIR)/test
TEST_SRCS 	:= $(addprefix $(TEST_DIR)/,$(_TEST_SRCS))









