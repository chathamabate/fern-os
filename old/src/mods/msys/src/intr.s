
// This will contain all interrupt handlers.
// See _intr.c to see handler internals.

.section .text

.macro pushad
    .byte 0x60
.endm

.macro popad
    .byte 0x61
.endm

.global nop_exception_handler
.type nop_exception_handler, @function
nop_exception_handler:
    iret 

// It'd be cool if I had a handler for every exception type tbh...

.global default_exception_handler
default_exception_handler:
    // pushad // Not needed, we don't return.
    cld
    call _default_exception_handler
    call lock_up

.global default_exception_with_err_code_handler
default_exception_with_err_code_handler:
    // pushad // Not needed, we don't return.
    cld

    // damn, this is actually being called now??

    // Ok before doing any locking up... can we get the 
    // Error code actually pushed?

    mov %esp, %ebp
    pushl %esp
    pushl $12
    call term_put_trace
    mov %ebp, %esp

    call lock_up

.global default_handler
default_handler:
    pushad
    cld

    call _default_handler
    popad
    iret

// Some other exception handlers to help i

.global enable_intrs
enable_intrs:
    sti
    ret

.global disable_intrs
disable_intrs:
    cli
    ret
