
// This will contain all interrupt handlers.
// See _intr.c to see handler internals.

.section .text

.global my_handler
my_handler:
    pushad
    cld
    call _my_handler
    popad
    iret
