

MOD_NAME 	?= k_startup

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= kernel.c \
			   thread.c \
			   process.c \
			   state.c \
			   handle_cd.c \
			   plugin.c \
			   plugin_fut.c \
			   plugin_fs.c \
			   plugin_kb.c \
			   plugin_vga_cd.c \
			   action.c \
			   tss.c \
			   gdt.c \
			   idt.c \
			   page.c \
			   page_helpers.c \
			   ata_block_device.c \
			   vga_cd.c

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?= boot.s \
			   action.s

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= page.c \
			   page_helpers.c \
			   process.c \
			   ata_block_device.c

-include ../mod_stub.mk

