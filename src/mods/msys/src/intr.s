
// This will contain all interrupt handlers.
// See _intr.c to see handler internals.

.section .text

.macro pushad
    .byte 0x60
.endm

.macro popad
    .byte 0x61
.endm

.global default_exception_handler
.type default_exception_handler, @function
default_exception_handler:
    pushad
    cld
    call _default_exception_handler
    call lock_up

.global default_exception_with_err_code_handler
.type default_exception_with_err_code_handler, @function
default_exception_with_err_code_handler:
    pushad
    cld
    call lock_up

.global default_handler
.type default_handler, @function
default_handler:
    pushad
    cld
    call _default_handler
    popad
    iret

.global enable_intrs
.type enable_intrs, @function
enable_intrs:
    sti
    ret

.global disable_intrs
.type disable_intrs, @function
disable_intrs:
    cli
    ret
