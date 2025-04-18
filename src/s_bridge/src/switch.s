
.section .text

.global switch_k2u
switch_k2u:
    movl 4(%esp), %eax // eax gets pd.
    movl 8(%esp), %ecx // ecx gets the new stack pointer.

    movl %eax, %cr3 // Change page directory.
    movl %ecx, %esp // Go to right stack position.

    popal
    iret

.global switch_k2u_with_ret
switch_k2u_with_ret:
    movl 4(%esp), %eax  // eax gets pd.
    movl 8(%esp), %ecx 
    movl 12(%esp), %edx 

    movl %eax, %cr3 // Change page directory.
    movl %ecx, %esp // Go to right stack position.

    // The old eax should be at index 7 from the stack pointer.
    movl %edx, 28(%esp)

    popal
    iret

.global syscall
syscall:

