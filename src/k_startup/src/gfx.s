
.section .text

// For VBE protected mode interface, we need to load registers manually to 
// pass arguments! Not standard ABI
// 
// Return status is returned to %ax.
// 
// 0x4F in %al -> Function supported. 
// Otherwise   -> Function not supported.
//
// 0x00 in %ah -> Success
// 0x01        -> Function call failed.
// 0x02        -> Function not supported.
// 0x03        -> Function call invalid in current mode.

.global vbe_current_mode
vbe_current_mode:
    pushl %ebx

    movl 8(%esp), %ecx // This is where we'll write the mode to.

    movw $0x4F03, %ax
    call *VBE_ENTRY

    movw %bx, (%ecx)

    popl %ebx

    ret
