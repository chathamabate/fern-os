

.section .text

.global outb
.type outb, @function
outb:
    push %ebp
    mov %esp, %ebp
    mov 0x8(%ebp), %edx
    mov 0xc(%ebp), %eax
    out %al, %dx    // Write it out.
    mov %eax, 0x0
    mov %edx, 0x0
    pop %ebp
    ret

