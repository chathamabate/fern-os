

GIT_TOP ?= $(shell git rev-parse --show-toplevel)

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= thingy

# REQUIRED: All standalone modules this module must include
DEPS		?=

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= thingy.c

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= thingy.c

# REQUIRED: Where to place build artifacts
BUILD_DIR   ?= $(GIT_TOP)/retry/src/thingy/build

# REQUIRED: Where to place .a files and headers
INSTALL_DIR ?= $(GIT_TOP)/retry/src/thingy/install

# OPTIONAL: Which compiler to use
C_COMPILER ?= gcc

# OPTIONAL: Which archiver to use
AR ?= ar

# OPTIONAL: Extra C flags to use
EXTRA_CFLAGS ?=

-include ../mod_stub.mk

A = $(1)

a:
	echo $(call A,2)
