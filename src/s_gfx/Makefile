include ../mod_header_stub.mk

# NOTE: This module is to help with dynamic memory allocation in
# both kernel or user space.

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= s_gfx

# REQUIRED: Names (NOT PAHTS) of all non-test .h files found in the include folder
_HDRS  ?=   gfx.h \
			gfx_manager.h \
			mono_fonts.h \
			window.h \
			window_dummy.h \
			window_qgrid.h

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= gfx.c gfx_manager.c mono_fonts.c window.c \
			   window_qgrid.c window_dummy.c

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?= 

# REQUIRED: Names (NOT PAHTS) of all test .h files found in the include test folder
_TEST_HDRS 	?= 	gfx.h \
				gfx_manager.h \
				mono_fonts.h

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= gfx.c gfx_manager.c mono_fonts.c

include ../mod_stub.mk

