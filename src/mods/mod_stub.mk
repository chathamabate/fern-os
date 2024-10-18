
# Assumes the following variables are defined.

#MOD_NAME := terminal
#ASMS :=
#SRCS := out.c
#DEPS :=

CC := i386-elf-gcc
AS := i386-elf-as
TOP_DIR := $(shell git rev-parse --show-toplevel)

MOD_DIR := $(TOP_DIR)/src/mods/$(MOD_NAME)

INCLUDE_DIR := $(MOD_DIR)/include
SRC_DIR   	:= $(MOD_DIR)/src
BUILD_DIR 	:= $(MOD_DIR)/build

FULL_ASM_OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASMS))
FULL_SRC_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

# Objects will only depend on local headers btw.
HEADERS := $(wildcard $(INCLUDE_DIR)/$(MOD_NAME)/*.h)

MOD_LIB := $(BUILD_DIR)/lib$(MOD_NAME).a

INCLUDE_DIRS := $(INCLUDE_DIR) $(foreach dep,$(DEPS),$(TOP_DIR)/src/mods/$(dep)/include)
INCLUDE_FLAGS := $(addprefix -I,$(INCLUDE_DIRS))

.PHONY: lib clean

$(BUILD_DIR):
	mkdir -p $@

$(FULL_ASM_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.s $(HEADERS) | $(BUILD_DIR)
	$(AS) $< -o $@

$(FULL_SRC_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) -c $< -o $@ -std=gnu99 -ffreestanding -O0 -Wall -Wextra -Wpedantic $(INCLUDE_FLAGS)

$(MOD_LIB): $(FULL_ASM_OBJS) $(FULL_SRC_OBJS)
	i386-elf-ar rcs $@ $^

lib: $(MOD_LIB)

clean:
	rm -rf $(BUILD_DIR)

