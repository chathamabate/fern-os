include ../mod_header_stub.mk

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= u_startup

# REQUIRED: Names (NOT PAHTS) of all non-test .h files found in the include folder
_HDRS  	?=  main.h \
			syscall.h \
			syscall_fs.h \
			syscall_fut.h \
			syscall_gfx.h \
			syscall_kb.h \
			syscall_pipe.h \
			syscall_shm.h \
			syscall_term.h

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= main.c \
			   syscall.c \
			   syscall_fut.c \
			   syscall_fs.c \
			   syscall_pipe.c \
			   syscall_kb.c \
			   syscall_gfx.c \
			   syscall_shm.c \
			   syscall_term.c

# REQUIRED: Names (NOT PATHS) of all .S files found in the src folder
_ASMS		?= main.S syscall.S

# REQUIRED: Names (NOT PAHTS) of all test .h files found in the include test folder
_TEST_HDRS 	?=	syscall.h \
				syscall_default_io.h \
				syscall_exec.h \
				syscall_fs.h \
				syscall_fut.h \
				syscall_gfx.h \
				syscall_kb.h \
				syscall_pipe.h \
				syscall_shm.h \
				syscall_term.h

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= syscall.c \
			   syscall_fut.c \
			   syscall_fs.c \
			   syscall_exec.c \
			   syscall_default_io.c \
			   syscall_kb.c \
			   syscall_pipe.c \
			   syscall_shm.c \
			   syscall_term.c \
			   syscall_gfx.c


# TODO: Add back testing for terminal/cd.

include ../mod_stub.mk

