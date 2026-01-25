

MOD_NAME 	?= k_startup

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= kernel.c \
			   thread.c \
			   process.c \
			   state.c \
			   plugin.c \
			   plugin_fut.c \
			   plugin_fs.c \
			   plugin_kb.c \
			   plugin_pipe.c \
			   plugin_gfx.c \
			   action.c \
			   tss.c \
			   gfx.c \
			   gdt.c \
			   idt.c \
			   page.c \
			   page_helpers.c \
			   ata_block_device.c 

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?= boot.s \
			   action.s

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= page.c \
			   page_helpers.c \
			   process.c \
			   ata_block_device.c

-include ../mod_stub.mk

