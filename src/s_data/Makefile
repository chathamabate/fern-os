

# NOTE: This module is to help with dynamic memory allocation in
# both kernel or user space.

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= s_data

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= id_table.c map.c list.c wait_queue.c ring.c

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?= 

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= id_table.c map.c list.c wait_queue.c ring.c

-include ../mod_stub.mk

