
ASM	:= nasm

SRC_DIR 	:= src
BUILD_DIR 	:= build

.PHONY: all clean run

all: $(BUILD_DIR)/disk.img

BOOT_DIR 	:= boot

BOOT_SRC_DIR 	:= $(SRC_DIR)/$(BOOT_DIR)
BOOT_BUILD_DIR 	:= $(BUILD_DIR)/$(BOOT_DIR)

# Bootloader will only ever be in one asm file.
# (boot.s) (It will compile to exactly 512 bytes)

$(BOOT_BUILD_DIR)/boot.bin: $(BOOT_SRC_DIR)/boot.s
	mkdir -p $(BOOT_BUILD_DIR)
	$(ASM) $(BOOT_SRC_DIR)/boot.s -f bin -o $(BOOT_BUILD_DIR)/boot.bin

# 512 * 2880 = 1.47 MB
#
# Cylinder = 0 to 1023 (1024 possible cylinders) (10 bits)
# Head = 0 to 255 heads (8 bits)
# Sector = 1 to 63 (63 possible sectors) (6 bits)
#
# 1024 * 255 * 63 * 512 = 8 GB of Disk Space!
# Which should be more then enough!

# We will see how this goes.
# We will need to use the BIOS to get our drive geometry.
SECTORS	:= 32768

# For now, let's say there are only 4 heads.
# 4 * 1024 * 63 = 
$(BUILD_DIR)/disk.img: $(BOOT_BUILD_DIR)/boot.bin
	dd if=/dev/zero of=$(BUILD_DIR)/disk.img bs=512 count=$(SECTORS)
	dd if=$(BOOT_BUILD_DIR)/boot.bin of=$(BUILD_DIR)/disk.img conv=notrunc

clean:
	rm -r build

# Attatch our image as a hard disc.
run: $(BUILD_DIR)/disk.img
	qemu-system-i386 -hda $(BUILD_DIR)/disk.img
