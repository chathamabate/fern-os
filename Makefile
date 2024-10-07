
CC := i386-elf-gcc
AS := i386-elf-as
TOP_DIR := $(shell git rev-parse --show-toplevel)

OS_NAME := myos

MODS := terminal \
		util \
		msys

SRCS := kernel.c
ASMS := boot.s

SRC_DIR := $(TOP_DIR)/src
MODS_DIR := $(SRC_DIR)/mods
BUILD_DIR := $(TOP_DIR)/build
ISO_DIR := $(BUILD_DIR)/iso

FULL_ASM_OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASMS))
FULL_SRC_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

LDSCRIPT := $(TOP_DIR)/linker.ld

MOD_BUILD_FOLDERS := $(foreach m,$(MODS),$(MODS_DIR)/$(m)/build)
MOD_LIBS := $(foreach m,$(MODS),$(MODS_DIR)/$(m)/build/lib$(m).a)

BIN_FILE := $(BUILD_DIR)/$(OS_NAME).bin

ISO_FILE := $(BUILD_DIR)/$(OS_NAME).iso
GRUB_CFG_FILE := $(ISO_DIR)/boot/grub/grub.cfg

.PHONY: all bin qemu.bin iso qemu.cd clean clangd clangd.clean

all: bin

$(BUILD_DIR) $(ISO_DIR)/boot/grub:
	mkdir -p $@

$(FULL_ASM_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(AS) $< -o $@

$(FULL_SRC_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) -c $< -o $@ -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Wpedantic $(foreach m,$(MODS),-I$(MODS_DIR)/$(m)/include)

# We'll assume if the folder exists, the build artifacts were created.
# And up-to-date. You'll need to run a clean first if you'd like to rebuild modules too.
$(MOD_BUILD_FOLDERS): $(MODS_DIR)/%/build:
	make -C $(MODS_DIR)/$* lib

$(BIN_FILE): $(FULL_ASM_OBJS) $(FULL_SRC_OBJS) $(LDSCRIPT) | $(MOD_BUILD_FOLDERS)
	$(CC) -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib $(FULL_ASM_OBJS) $(FULL_SRC_OBJS) $(MOD_LIBS) -lgcc

bin: $(BIN_FILE)

qemu.bin: $(BIN_FILE)
	qemu-system-i386 -kernel $(BIN_FILE)

# Creating an ISO with multi boot I think will only work
# on a linux computer... I don't think grub is really equipped
# to work on my mac?

$(ISO_FILE): $(BIN_FILE) | $(ISO_DIR)/boot/grub
	cp $(BIN_FILE) $(ISO_DIR)/boot
	@echo "menuentry \"$(OS_NAME)\" {" > $(GRUB_CFG_FILE)
	@echo "    multiboot /boot/$(OS_NAME).bin" >> $(GRUB_CFG_FILE)
	@echo "}" >> $(GRUB_CFG_FILE)
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

iso: $(ISO_FILE)

qemu.cd: $(ISO_FILE)
	qemu-system-i386 -cdrom $(ISO_FILE)

clean:
	rm -rf $(BUILD_DIR)
	true $(foreach m,$(MODS),&& make -C $(MODS_DIR)/$(m) clean)

$(SRC_DIR)/.clangd:
	echo "CompileFlags:" > $@
	echo "  Add:" >> $@
	$(foreach m,$(MODS),echo "  - -I$(MODS_DIR)/$(m)/include" >> $@;)

clangd: $(SRC_DIR)/.clangd

clangd.clean:
	rm -f $(SRC_DIR)/.clangd


