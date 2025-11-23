

# REQUIRED: Name of the standalone module (MUST BE THE SAME AS IT"S DIRECTORY NAME)
MOD_NAME 	?= u_startup

# REQUIRED: Names (NOT PATHS) of all .c files found in the src folder
_SRCS 		?= main.c \
			   syscall.c \
			   syscall_fut.c \
			   syscall_cd.c \
			   syscall_fs.c \
			   syscall_vga_cd.c \
			   syscall_pipe.c \
			   syscall_kb.c

# REQUIRED: Names (NOT PATHS) of all .s files found in the src folder
_ASMS		?= main.s syscall.s

# REQUIRED: Names (NOT PATHS) of all .c files in the test folder
_TEST_SRCS 	?= syscall.c \
			   syscall_fut.c \
			   syscall_fs.c \
			   syscall_exec.c \
			   syscall_cd.c \
			   syscall_default_io.c \
			   syscall_kb.c \
			   syscall_pipe.c

-include ../mod_stub.mk

