
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

KERNEL_DIR			:= kernel
KERNEL_SRC_DIR		:= $(SRC_DIR)/$(KERNEL_DIR)
KERNEL_BUILD_DIR	:= $(BUILD_DIR)/$(KERNEL_DIR)

KERNEL_SRCS			:= $(wildcard $(KERNEL_SRC_DIR)/*.s)

$(KERNEL_BUILD_DIR)/kernel.bin: $(KERNEL_SRCS)
	mkdir -p $(KERNEL_BUILD_DIR)
	
	# NOTE that kernel.s should include all the code from
	# all other kernel srcs.
	$(ASM) $(KERNEL_SRC_DIR)/kernel.s -f bin -o $(KERNEL_BUILD_DIR)/kernel.bin

$(BUILD_DIR)/image.bin: $(BOOT_BUILD_DIR)/boot.bin $(KERNEL_BUILD_DIR)/kernel.bin
	mkdir -p $(BUILD_DIR)
	cat $(BOOT_BUILD_DIR)/boot.bin > $(BUILD_DIR)/image.bin
	cat $(KERNEL_BUILD_DIR)/kernel.bin >> $(BUILD_DIR)/image.bin


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
#
# NOTE: the bios will tell us how many sectors we actually
# have available to us. (This is kinda just an estimate)
SECTORS := 32768

# For now, let's say there are only 4 heads.
# 4 * 1024 * 63 = 
$(BUILD_DIR)/disk.img: $(BUILD_DIR)/image.bin
	dd if=/dev/zero of=$(BUILD_DIR)/disk.img bs=512 count=$(SECTORS)
	dd if=$(BUILD_DIR)/image.bin of=$(BUILD_DIR)/disk.img conv=notrunc

clean:
	rm -r build

# Attatch our image as a hard disc.
run: $(BUILD_DIR)/disk.img
	qemu-system-i386 -hda $(BUILD_DIR)/disk.img
