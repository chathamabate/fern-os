

BUILD_DIR := build
OBJS_DIR := $(BUILD_DIR)/objs
ISO_DIR := $(BUILD_DIR)/iso

all: $(BUILD_DIR)/myos.bin

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

$(ISO_DIR):
	mkdir -p $(ISO_DIR)/boot/grub

# Only one boot assembly file as of now.
$(OBJS_DIR)/boot.o: boot.s $(OBJS_DIR)
	i686-elf-as boot.s -o $(OBJS_DIR)/boot.o

$(OBJS_DIR)/kernel.o: kernel.c $(OBJS_DIR)
	i686-elf-gcc -c kernel.c -o $(OBJS_DIR)/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

$(BUILD_DIR)/myos.bin: $(OBJS_DIR)/boot.o $(OBJS_DIR)/kernel.o
	i686-elf-gcc -T linker.ld -o $(BUILD_DIR)/myos.bin -ffreestanding -O2 -nostdlib $^ -lgcc

$(BUILD_DIR)/myos.iso: $(ISO_DIR) $(BUILD_DIR)/myos.bin grub.cfg
	cp $(BUILD_DIR)/myos.bin $(ISO_DIR)/boot/myos.bin
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/myos.iso $(ISO_DIR)

run_bin: $(BUILD_DIR)/myos.bin
	qemu-system-i386 -kernel $(BUILD_DIR)/myos.bin

# Not working!
run_cdrom: $(BUILD_DIR)/myos.iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/myos.iso

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run_cdrom clean
