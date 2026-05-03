
.section .text

.global vbe_current_mode
vbe_current_mode:
    movw $0x4F03, %ax
    call *VBE_ENTRY
    ret
