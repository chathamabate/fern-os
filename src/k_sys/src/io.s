

.section .text

.global outb
outb:
    pushl %ebp
    movl %esp, %ebp

    movl 0x8(%ebp), %edx
    movl 0xc(%ebp), %eax
    outb %al, %dx    // Write it out.
    movl $0x0, %eax
    movl $0x0, %edx
    
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

.global outw
outw:
    pushl %ebp
    movl %esp, %ebp

    movl 0x8(%ebp), %edx
    movl 0xc(%ebp), %eax
    outw %ax, %dx
    movl $0x0, %eax
    movl $0x0, %edx
    
    movl %ebp, %esp
    popl %ebp 

    ret

.global inw
inw:
    pushl %ebp
    movl %esp, %ebp

    movl 0x8(%ebp), %edx

    movl $0, %eax
    inw %dx, %ax

    movl %ebp, %esp
    popl %ebp 

    // At this point %eax should contain the received word padded
    // with a bunch of 0s.

    ret

