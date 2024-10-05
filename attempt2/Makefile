
TOP_DIR := $(shell git rev-parse --show-toplevel)/attempt2

OS_NAME := myos

SRC_DIR := $(TOP_DIR)/src
BUILD_DIR := $(TOP_DIR)/build
ISO_DIR := $(BUILD_DIR)/iso

ASMS := boot.s
FULL_ASM_OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASMS))

SRCS := kernel.c
FULL_SRC_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

LDSCRIPT := $(TOP_DIR)/linker.ld

BIN_FILE := $(BUILD_DIR)/$(OS_NAME).bin

ISO_FILE := $(BUILD_DIR)/$(OS_NAME).iso
GRUB_CFG_FILE := $(ISO_DIR)/boot/grub/grub.cfg

$(BUILD_DIR) $(ISO_DIR)/boot/grub:
	mkdir -p $@

$(FULL_ASM_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	i386-elf-as $< -o $@

$(FULL_SRC_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	i386-elf-gcc -c $< -o $@ -std=gnu99 -ffreestanding -O2 -Wall -Wextra

$(BIN_FILE): $(FULL_ASM_OBJS) $(FULL_SRC_OBJS) $(LDSCRIPT)
	i386-elf-gcc -T $(LDSCRIPT) -o $@ -ffreestanding -O2 -nostdlib $(FULL_ASM_OBJS) $(FULL_SRC_OBJS) -lgcc

bin: $(BIN_FILE)

qemu.bin: $(BIN_FILE)
	qemu-system-i386 -kernel $(BIN_FILE)


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


#$(ISO_DIR)/boot/grub/grub.cfg
#
#all: $(BUILD_DIR)/myos.bin
#
#$(OBJS_DIR):
#	mkdir -p $(OBJS_DIR)
#
#$(ISO_DIR):
#	mkdir -p $(ISO_DIR)/boot/grub
#
#
#$(OBJS_DIR)/kernel.o: kernel.c | $(OBJS_DIR)
#	i386-elf-gcc -c kernel.c -o $(OBJS_DIR)/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
#
#$(BUILD_DIR)/myos.bin: $(OBJS_DIR)/boot.o $(OBJS_DIR)/kernel.o
#	i386-elf-gcc -T linker.ld -o $(BUILD_DIR)/myos.bin -ffreestanding -O2 -nostdlib $^ -lgcc
#
#$(BUILD_DIR)/myos.iso: $(ISO_DIR) $(BUILD_DIR)/myos.bin grub.cfg
#	cp $(BUILD_DIR)/myos.bin $(ISO_DIR)/boot/myos.bin
#	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
#	grub-mkrescue -o $(BUILD_DIR)/myos.iso $(ISO_DIR)
#
#iso: $(BUILD_DIR)/myos.iso
#
#run_bin: $(BUILD_DIR)/myos.bin
#	qemu-system-i386 -kernel $(BUILD_DIR)/myos.bin
#
## Not working!
#run_cdrom: $(BUILD_DIR)/myos.iso
#	qemu-system-i386 -cdrom $(BUILD_DIR)/myos.iso
#
#clean:
#	rm -rf $(BUILD_DIR)
#
#.PHONY: all run_cdrom clean
