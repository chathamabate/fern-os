include ../mod_header_stub.mk

# NOTE: This module is to help with dynamic memory allocation in
# both kernel or user space.

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= s_block_device

# REQUIRED: Names (NOT PAHTS) of all non-test .h files found in the include folder
_HDRS  ?=   block_device.h \
			cached_block_device.h \
			fat32.h \
			fat32_dir.h \
			fat32_file_sys.h \
			file_sys.h \
			mem_block_device.h

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= mem_block_device.c cached_block_device.c \
			   fat32.c fat32_dir.c file_sys.c fat32_file_sys.c

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?= 

# REQUIRED: Names (NOT PAHTS) of all test .h files found in the include test folder
_TEST_HDRS 	?= 	block_device.h \
				cached_block_device.h \
				fat32.h \
				fat32_dir.h \
				fat32_file_sys.h \
				file_sys.h \
				file_sys_helpers.h \
				mem_block_device.h

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= block_device.c mem_block_device.c cached_block_device.c \
			   fat32.c fat32_dir.c file_sys_helpers.c file_sys.c \
			   fat32_file_sys.c

include ../mod_stub.mk

