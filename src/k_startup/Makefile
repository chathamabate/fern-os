

MOD_NAME 	?= k_startup

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= kernel.c \
			   thread.c \
			   process.c \
			   state.c \
			   action.c \
			   tss.c \
			   gdt.c \
			   idt.c \
			   page.c

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?= boot.s

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= page.c

-include ../mod_stub.mk

