

MOD_NAME 	?= k_startup

# REQUIRED: Names (NOT PAHTS) of all non-test .h files found in the include folder
_HDRS  ?=   action.h \
			ata_block_device.h \
			fwd_defs.h \
			gdt.h \
			gfx.h \
			handle.h \
			idt.h \
			kernel.h \
			page.h \
			page_helpers.h \
			plugin.h \
			plugin_fs.h \
			plugin_fut.h \
			plugin_gfx.h \
			plugin_kb.h \
			plugin_pipe.h \
			plugin_shm.h \
			process.h \
			stacks.h \
			state.h \
			thread.h \
			tss.h

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
			   plugin_shm.c \
			   action.c \
			   tss.c \
			   gfx.c \
			   gdt.c \
			   idt.c \
			   page.c \
			   page_helpers.c \
			   ata_block_device.c 

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?= boot.S \
			   action.S

# REQUIRED: Names (NOT PAHTS) of all test .h files found in the include test folder
_TEST_HDRS 	?= 	ata_block_device.h \
				page.h \
				page_helpers.h \
				process.h

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= page.c \
			   page_helpers.c \
			   process.c \
			   ata_block_device.c

include ../mod_stub.mk

