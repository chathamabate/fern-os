
GIT_TOP ?= $(shell git rev-parse --show-toplevel)

BUILD_DIR   := $(GIT_TOP)/build
INSTALL_DIR := $(BUILD_DIR)/install
MODS_DIR    := $(GIT_TOP)/src


# While the order they exist here doesn't matter, I like to list
# the modules in order of independence.
#
# kernel depends on everything
# fstndutil depends on nothing
MODS := \
		kernel \
		term \
		msys \
		fstndutil 


# Install Headers

HDRS_INSTALL_TARS := $(addprefix hdrs.install.,$(MODS))

.PHONY: hdrs.install $(HDRS_INSTALL_TARS)
hdrs.install: $(HDRS_INSTALL_TARS)

$(HDRS_INSTALL_TARS): hdrs.install.%:
	make -C $(MODS_DIR)/$* hdrs.install test_hdrs.install

# Install Libs (For now we'll just always build the test and normal libs)

LIB_INSTALL_TARS := $(addprefix lib.install.,$(MODS))
.PHONY: lib.unsafe_install lib.install $(LIB_INSTALL_TARS)

# This is "unsafe" since there is no gaurantee that the headers have been installed.
lib.unsafe_install: $(LIB_INSTALL_TARS)
$(LIB_INSTALL_TARS): lib.install.%:
	make -C $(MODS_DIR)/$* lib.install test_lib.install

lib.install: hdrs.install
	make -C $(MODS_DIR) lib.unsafe_install

# Clangd Helpers

CLANGD_TARS := $(addprefix clangd.,$(MODS))
.PHONY: clangd $(CLANGD_TARS)

$(CLANGD_TARS): clangd.%:
	make -C $(MODS_DIR)/$* clangd

clangd: $(CLANGD_TARS)

CLANGD_CLEAN_TARS := $(addprefix clean.clangd.,$(MODS))
.PHONY: clean.clangd $(CLANGD_CLEAN_TARS)

$(CLANGD_CLEAN_TARS): clean.clangd.%:
	make -C $(MODS_DIR)/$* clean.clangd

clean.clangd: $(CLANGD_CLEAN_TARS)

# Finally now for actual kernel packaging!

OS_NAME := fernos
C_COMPILER := i386-elf-gcc

LDSCRIPT := $(MODS_DIR)/linker.ld

BIN_FILE := $(BUILD_DIR)/$(OS_NAME).bin

$(BUILD_DIR):
	mkdir -p $@

LIB_FLAGS := $(foreach mod,$(MODS),-l$(mod) -ltest_$(mod))

$(BIN_FILE): lib.install | $(BUILD_DIR)
	$(C_COMPILER) -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib -L$(INSTALL_DIR) $(LIB_FLAGS) -lgcc

.PHONY: bin qemu.bin

bin: $(BIN_FILE)

qemu.bin: $(BIN_FILE)
	qemu-system-i386 -kernel $(BIN_FILE)

# NOTE: When we start working with disk read/write... I'd really like to figure out ISO packaging!

# Clean

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR)
