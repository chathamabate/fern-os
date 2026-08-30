include ../mod_header_stub.mk

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= k_sys

# REQUIRED: Names (NOT PAHTS) of all non-test .h files found in the include folder
_HDRS  ?=   ata.h \
			debug.h \
			dt.h \
			gdt.h \
			idt.h \
			intr.h \
			io.h \
			kb.h \
			m2.h \
			page.h \
			tss.h

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= tss.c gdt.c idt.c intr.c ata.c kb.c m2.c

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?= tss.S io.S page.S gdt.S idt.S intr.S debug.S

# REQUIRED: Names (NOT PAHTS) of all test .h files found in the include test folder
_TEST_HDRS 	?=

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= test.c

include ../mod_stub.mk

