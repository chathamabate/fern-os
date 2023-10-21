
ASM	:= nasm

SRC_DIR 	:= src
BUILD_DIR 	:= build

.PHONY: all clean

all: $(BUILD_DIR)/floppy.img

BOOT_DIR 	:= boot

BOOT_SRC_DIR 	:= $(SRC_DIR)/$(BOOT_DIR)
BOOT_BUILD_DIR 	:= $(BUILD_DIR)/$(BOOT_DIR)

# Bootloader will only ever be in one asm file.
# (boot.s) (It will compile to exactly 512 bytes)

$(BOOT_BUILD_DIR)/boot.bin: $(BOOT_SRC_DIR)/boot.s
	mkdir -p $(BOOT_BUILD_DIR)
	$(ASM) $(BOOT_SRC_DIR)/boot.s -f bin -o $(BOOT_BUILD_DIR)/boot.bin

$(BUILD_DIR)/floppy.img: $(BOOT_BUILD_DIR)/boot.bin
	dd if=/dev/zero of=$(BUILD_DIR)/floppy.img bs=512 count=2880
	dd if=$(BOOT_BUILD_DIR)/boot.bin of=$(BUILD_DIR)/floppy.img conv=notrunc

clean:
	rm -r build
