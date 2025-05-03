
.section .text

.global cmp_xchg
cmp_xchg:
    // Load expected value into eax.
    movl 4(%esp), %eax

    // Load desired value into ecx.
    movl 8(%esp), %ecx

    // Load destination into edx
    movl 12(%esp), %edx
    
    lock cmpxchg %ecx, (%edx)

    // Remember, on success %eax holds the expected value.
    // On failure, %eax holds the real value of *dest.

    ret
