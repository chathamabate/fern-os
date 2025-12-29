
.include "os_defs.s"

.section .multiboot
header_start:
    .long 0xE85250D6               
    .long 0                       
    .long header_end - header_start 
    .long -(0xE85250D6 + 0 + (header_end - header_start)) 

entry_tag:
    .short 3
    .short 0
    .long  12
    .long  _start

    /* pad */
    .long 0

gfx_tag:
    .short 5
    .short 0
    .long  20

    .long  FERNOS_GFX_WIDTH
    .long  FERNOS_GFX_HEIGHT
    .long  FERNOS_GFX_BPP

    .long 0 // pad

null_tag: 
    .short 0    
    .short 0    
    .long  8    

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

    /*
    Upon entering the kernel, grub should load %eax with some multiboot2 magic number
    and %ebx with the physical address of the multiboot2 information structure.

    We'll pass these both directly as args into `start_kernel`.
    */
    pushl %ebx 
    pushl %eax
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


