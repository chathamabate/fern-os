

.section .text

.global outb
outb:
    pushl %ebp
    movl %esp, %ebp

    movl 0x8(%ebp), %edx
    movl 0xc(%ebp), %eax
    outb %al, %dx    // Write it out.
    movl %eax, 0x0
    movl %edx, 0x0
    
    movl %ebp, %esp
    popl %ebp 

    ret

.global inb
inb:
    pushl %ebp
    movl %esp, %ebp

    movl 0x8(%ebp), %edx

    movl $0, %eax
    inb %dx, %al

    movl %ebp, %esp
    popl %ebp 

    // At this point %eax should contain the received byte padded
    // with a bunch of 0s.

    ret

