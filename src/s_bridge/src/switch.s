
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

.global syscall_handler
syscall_handler:
    pushal

    // It shouldn't actually return!

    // Are you telling me the IDT must be where exactly??
    // In the bridge directory???
    // Well, I don't think that's true necessarily???
    // I think it should be though tbh...
    // UGH!!!! These handlers must be able to switch to kernel mode???
    // This is lowkey very challenging!!!!


.global syscall
syscall:
    pushl %ebp
    movl %esp %ebp

    pushl %ebx

    // The arguments.
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx // Must be restored!
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edx
    
    // Perform Syscall!
    int $48

    // It is expected that when we return to this position, 
    // eax has been loaded with a return value.

    // Restore important registers.
    popl %ebx
    popl %ebp

    ret

