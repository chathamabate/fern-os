

.section .text

.global enter_user_ctx
enter_user_ctx:
    movl 4(%esp), %edi // Store the context address.

    subl $(15 * 4), %esp // reserve room for the context.
    movl %esp, %esi     // Store the top of context.

    movl %esp, %ebp
    pushl $(15 * 4)  // n
    pushl %edi       // src
    pushl %esi       // dest
    call mem_cpy
    movl %ebp, %esp

    // One last thing, we need to modify the temp_esp value in the context
    // so that popal works correctly.
    movl %esp, %esi
    addl $(9 * 4), %esi

    movl %esp, %edi
    addl $(4 * 4), %edi

    movl %esi, (%edi)

    // Load page directory.
    popl %eax
    movl %eax, %cr3 // Not working as expected...

    // Load general purpose registers.
    popal

    // Skip error value.
    addl $4, %esp

    // We assume that the context uses the same selector for data segments
    // and stack segments.
    movl 16(%esp), %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    // Finally RETURN!
    iret




