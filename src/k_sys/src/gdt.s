
.section .text

.global read_gdtr
read_gdtr:
    pushl %ebp
    movl %esp, %ebp

    // Reseve 8 0 bytes on the stack for the gdtr value.
    pushl $0
    pushl $0

    // Read gdt to the stack.
    sgdt (%esp)
    
    popl %eax
    popl %edx

    movl %ebp, %esp
    popl %ebp
    ret

.global _load_gdtr
_load_gdtr:
    pushl %ebp
    movl %esp, %ebp

    // Push the given gdtr onto the stack.
    // This is technically redundant, but whatever.
    pushl 12(%ebp)
    pushl 8(%ebp)

    lgdt (%esp)

    // Now reload all the segments plz.
    jmp $0x08, $.seg_reload

.seg_reload:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movl %ebp, %esp
    popl %ebp
    ret

