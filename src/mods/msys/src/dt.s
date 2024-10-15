

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



