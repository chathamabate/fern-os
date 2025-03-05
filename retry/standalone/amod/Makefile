
GIT_TOP := $(shell git rev-parse --show-toplevel)

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	:= amod

# REQUIRED: All standalone modules this module must include
DEPS		:=

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		:= thing.c

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	:= thing.c

# REQUIRED: Where to place build artifacts
BUILD_DIR   ?= $(GIT_TOP)/build/$(MOD_NAME)

# REQUIRED: Where to place .a files and headers
INSTALL_DIR ?= $(GIT_TOP)/install

-include ../standalone_stub.mk
