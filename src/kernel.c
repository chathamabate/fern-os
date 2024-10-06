 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

#include "terminal/out.h"
#include "msys/io.h"

void kernel_main(void) 
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}
