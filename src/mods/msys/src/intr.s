
// This will contain all interrupt handlers.
// See _intr.c to see handler internals.

.section .text

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
