

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

.global load_gdtr
.type load_gdtr, @function
load_gdtr:

    push %ebp
    movl %esp, %ebp

    sub $8, %esp
    movl $0, (%esp)
    movl $0, 4(%esp)

    // Load new GDTR values into (%esp)  
    
    movw 20(%esp), %ax
    movw %ax, (%esp)

    movl 16(%esp), %eax
    movl %eax, 2(%esp)

    // Let's also get our new data segment.

    movw 24(%esp), %ax

    lgdt (%esp)
    jmp $0x08, $._reload_cs

._reload_cs:
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    sti

    // Clean up stack.

    add $8, %esp
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
    pushl %ebp
    mov %esp, %ebp

    sub $8, %esp
    movl $0, (%esp)
    movl $0, 4(%esp)    // Temps.

    movw 20(%esp), %ax
    movw %ax, (%esp)

    movl 16(%esp), %eax
    movl %eax, 2(%esp)

    cli
    lidt (%esp)
    sti

    add $8, %esp // Pop Temps.

    pop %ebp
    ret

