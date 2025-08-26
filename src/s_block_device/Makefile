

# NOTE: This module is to help with dynamic memory allocation in
# both kernel or user space.

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= s_block_device

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= mem_block_device.c cached_block_device.c \
			   fat32.c fat32_dir.c file_sys.c fat32_file_sys.c

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?= 

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= block_device.c mem_block_device.c cached_block_device.c \
			   fat32.c fat32_dir.c file_sys_helpers.c file_sys.c \
			   fat32_file_sys.c

-include ../mod_stub.mk

