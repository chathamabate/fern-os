
/* Linker Script for Application Binary files */

#include "ld_config.h"

/* 
 * app_main should have signature:
 *  fernos_error_t app_main(const char * const *args, size_t num_args); 
 *  OR
 *  fernos_error_t app_main(void);
 */
ENTRY(app_main)

/*
 * In would be nice if in the future this linker script (or another) gave the user
 * the ability to compile and link C++ code. But, I am starting to learn this is actually
 * a little more difficult than expected.
 */

MEMORY 
{
    /* Likely only the kernel area will be needed below by this linker script */
    fernos_kernel_area : ORIGIN = FC_CORE_VMEM_KERNEL_START, LENGTH = FC_CORE_VMEM_KERNEL_END - FC_CORE_VMEM_KERNEL_START + 1

    fernos_app_area : ORIGIN = FC_CORE_VMEM_APP_START, LENGTH = FC_CORE_VMEM_APP_END - FC_CORE_VMEM_APP_START + 1
    fernos_app_args_area : ORIGIN = FC_CORE_VMEM_APP_ARGS_START, LENGTH = FC_CORE_VMEM_APP_ARGS_END - FC_CORE_VMEM_APP_ARGS_START + 1

    fernos_free_area : ORIGIN = FC_CORE_VMEM_FREE_START, LENGTH = FC_CORE_VMEM_FREE_END - FC_CORE_VMEM_FREE_START + 1
    fernos_stack_area : ORIGIN = FC_CORE_VMEM_STACK_START, LENGTH = FC_CORE_VMEM_STACK_END - FC_CORE_VMEM_STACK_START + 1
}


SECTIONS
{
    .text : ALIGN(4K) {
        *(.text)
        *(.rodata)
    } > fernos_app_area

    .bss (NOLOAD) : ALIGN(4K) {
        *(.bss)
        *(COMMON)
    } > fernos_app_area

    .data : ALIGN(4K) {
        *(.data)
    } > fernos_app_area
}
