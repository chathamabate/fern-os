
.section .text

.global trigger_syscall
trigger_syscall:
    pushl %ebp
    movl %esp, %ebp

    // Save non-trashable registers to be restored later.
    pushl %esi
    pushl %edi

    // Ok, now we have:
    // arg3
    // arg2
    // arg1
    // arg0
    // id
    // ret addr
    // old ebp <- %ebp
    // old esi
    // old edi <- %esp

    movl 6 * 4(%ebp), %edi // arg3
    movl 5 * 4(%ebp), %esi // arg2
    movl 4 * 4(%ebp), %edx // arg1
    movl 3 * 4(%ebp), %ecx // arg0
    movl 2 * 4(%ebp),  %eax // id
    
    int $48

    popl %edi
    popl %esi
    popl %ebp
    
    // This syscall should return right here with %eax loaded
    // with the correct return value.

    ret
