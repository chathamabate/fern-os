
.section .text

.global enable_intrs
enable_intrs:
    sti
    ret

.global disable_intrs
disable_intrs:
    cli
    ret

.global nop_handler
nop_handler:
    iret
