

// We are going to load the user given arg into here.
.section .bss
gdtr_store_addr: 
    // 48 bits needed!
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

.section .text

.global read_gdtr
.type read_gdtr, @function
read_gdtr:
    push %ebp
    movl %esp, %ebp

    // Get our User given pointer and store it in %edi.
    movl 8(%ebp), %edi   

    // Store the address we will be copying gdtr value info
    // from.
    leal (gdtr_store_addr), %esi
    
    // Store the value of the gdtr into our .bss section.
    sgdt (gdtr_store_addr)

    // Move size.
    movw (%esi), %ax
    movw %ax, (%edi)
    
    // Move Offset.
    movl 2(%esi), %eax
    movl %eax, 2(%edi)

    pop %ebp
    ret



