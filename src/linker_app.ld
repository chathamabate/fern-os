
/* Linker Script for Application Binary files */

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

INCLUDE "os_defs.ld"

MEMORY 
{
    /* Nothing goes in the preface area! These all will be 4MB aligned to help with paging. */

    prologue       : ORIGIN = PROLOGUE_START, LENGTH = PROLOGUE_END - PROLOGUE_START + 1

    /* Likely only the kernel area will be needed below by this linker script */
    fernos_kernel_area : ORIGIN = FERNOS_KERNEL_START, LENGTH = FERNOS_KERNEL_END - FERNOS_KERNEL_START + 1

    fernos_app_area : ORIGIN = FERNOS_APP_START, LENGTH = FERNOS_APP_END - FERNOS_APP_START + 1
    fernos_app_args_area : ORIGIN = FERNOS_APP_ARGS_START, LENGTH = FERNOS_APP_ARGS_END - FERNOS_APP_ARGS_START + 1

    fernos_free_area : ORIGIN = FERNOS_FREE_START, LENGTH = FERNOS_FREE_END - FERNOS_FREE_START + 1
    fernos_stack_area : ORIGIN = FERNOS_STACK_START, LENGTH = FERNOS_STACK_END - FERNOS_STACK_START + 1

    epilogue       : ORIGIN = EPILOGUE_START, LENGTH = EPILOGUE_END - EPILOGUE_START + 1
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
