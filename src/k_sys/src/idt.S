


.section .text

.global read_idtr
read_idtr:
    pushl %ebp
    movl %esp, %ebp

    // Reseve 8 0 bytes on the stack for the gdtr value.
    pushl $0
    pushl $0

    // Read idt to the stack.
    sidt (%esp)
    
    popl %eax
    popl %edx

    movl %ebp, %esp
    popl %ebp
    ret

.global _load_idtr
_load_idtr:
    pushl %ebp
    movl %esp, %ebp

    // Push the given idtr onto the stack.
    // This is technically redundant, but whatever.
    pushl 12(%ebp)
    pushl 8(%ebp)

    lidt (%esp)

    movl %ebp, %esp
    pop %ebp
    ret
