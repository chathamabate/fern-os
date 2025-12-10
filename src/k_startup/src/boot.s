
.include "os_defs.s"

.section .multiboot
header_start:
    .long 0xe85250d6               
    .long 0                       
    .long header_end - header_start 
    .long -(0xe85250d6 + 0 + (header_end - header_start)) 
    
    .short 0    
    .short 0    
    .long 8    
header_end:

.section .text

.global _start
.type _start, @function
_start:

    /*
    The kernel stack will start at the end of the FERNOS Area.
    Remeber though that FERNOS_END is inclusive (We must +1)
    Also, the last 4 bytes of the stack area will always be reserved for the kernel 
    page directory (We must -4)
    */
	movl $(FERNOS_STACK_END+1-4), %esp
    movl %esp, %ebp

    call start_kernel

    /*
    start_kernel should never return.
    Thus, we should never make it here.
    */

    call lock_up

/*
Set the size of the _start symbol to the current location '.' minus its start.
This is useful when debugging or when you implement call tracing.
*/
.size _start, . - _start


