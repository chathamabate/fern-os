

# NOTE: This module is freestanding utility code.
# IT DEPENDS ON NOTHING!!!

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= s_util

# REQUIRED: Names (NOT PAHTS) of all non-test .h files found in the include folder
_HDRS  ?=   ansi.h \
			datetime.h \
			elf.h \
			err.h \
			hash.h \
			misc.h \
			ps2_scancodes.h \
			rand.h \
			str.h \
			test.h

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= str.c misc.c rand.c hash.c ps2_scancodes.c \
			   ansi.c

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?= str.S

# REQUIRED: Names (NOT PAHTS) of all test .h files found in the include test folder
_TEST_HDRS 	?= 	ansi.h \
				misc.h \
				str.h

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= str.c \
			   misc.c \
			   ansi.c

include ../mod_stub.mk

