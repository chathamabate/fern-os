

.section .text

.global read_gdtr
.type read_gdtr, @function
read_gdtr:
    push %ebp
    movl %esp, %ebp

    // Get our User given pointer and store it in %edi.
    movl 8(%ebp), %edi   
    sgdt (%edi)

    pop %ebp
    ret

.global read_idtr
.type read_idtr, @function
read_idtr:
    push %ebp
    movl %esp, %ebp

    // Get our User given pointer and store it in %edi.
    movl 8(%ebp), %edi   
    sidt (%edi)

    pop %ebp
    ret

.global load_idtr
.type load_idtr, @function
load_idtr:
    push %ebp
    movl %esp, %ebp
    
    sub $8, %esp

    // Clear the 8 bytes we just reserved.
    movl $0, (%esp)
    movl $0, 4(%esp)

    movw 32(%esp), %ax
    movw %ax, (%esp)

    movl 24(%esp), %eax
    movl %eax, 2(%esp)

    cli
    lidt (%esp)
    sti

    add $8, %esp

    pop %ebp
    ret

