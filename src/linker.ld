

/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

INCLUDE "os_defs.ld"

/*
 * I'm sick of always doing these calcs.
 *
 * 0x          0 =   0  B
 * 0x          1 =   1  B
 * 0x         10 =  16  B
 * 0x        100 = 256  B
 * 0x       1000 =   4 KB
 * 0x     1_0000 =  64 KB
 * 0x    10_0000 =   1 MB
 * 0x   100_0000 =  16 MB
 * 0x  1000_0000 = 256 MB
 * 0x1_0000_0000 =   4 GB
 */

/* 16 KB */
INIT_KERNEL_STACK_SIZE =  0x4000;

/* 
 * I had some issues with the linker script behaving how I wanted when placing the initial kernel
 * stack.
 *
 * Now, the fernos area is divided into 2 sections.
 * The initial kernel stack (fernos_area_1) and everything else (fernos_area_0)
 */

FERNOS_LENGTH = FERNOS_END - FERNOS_START + 1;
FERNOS_0_LENGTH = FERNOS_LENGTH - INIT_KERNEL_STACK_SIZE;
FERNOS_1_LENGTH = INIT_KERNEL_STACK_SIZE;

MEMORY 
{
    /* Nothing goes in the preface area! These all will be 4MB aligned to help with paging. */

    prologue       : ORIGIN = PROLOGUE_START, LENGTH = PROLOGUE_END - PROLOGUE_START + 1
    fernos_area_0  : ORIGIN = FERNOS_START,                     LENGTH  = FERNOS_0_LENGTH
    fernos_area_1  : ORIGIN = FERNOS_START + FERNOS_0_LENGTH,   LENGTH  = FERNOS_1_LENGTH
    epilogue       : ORIGIN = EPILOGUE_START, LENGTH = EPILOGUE_END - EPILOGUE_START + 1
}

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{

    /*
     * Three types of areas (shared, kernel, and user)
     * Five parts per area (text, rodata, data, common, bss)
     */


    /* Read Only Sections text + rodata */

    .text.shared : {
        . = ALIGN(4K);
        _ro_shared_start = .;
		*(.multiboot)
        *libs_*.a:*.o(.text)
        *libs_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_shared_end = .;
    } > fernos_area_0

    .text.kernel : {
        . = ALIGN(4K);
        _ro_kernel_start = .;
        *libk_*.a:*.o(.text)
        *libk_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_kernel_end = .;
    } > fernos_area_0

    .text.user : {
        . = ALIGN(4K);
        _ro_user_start = .;
        *libu_*.a:*.o(.text)
        *libu_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_user_end = .;
    } > fernos_area_0

    /* BSS and Data Sections */

    .bss.shared (NOLOAD) : {
        . = ALIGN(4K);
        _bss_shared_start = .;
        *libs_*.a:*.o(.bss)
        *libs_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_shared_end = .;
    } > fernos_area_0

    .data.shared : {
        . = ALIGN(4K);
        _data_shared_start = .;
        *libs_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_shared_end = .;
    } > fernos_area_0

    .bss.kernel (NOLOAD) : {
        . = ALIGN(4K);
        _bss_kernel_start = .;
        *libk_*.a:*.o(.bss)
        *libk_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_kernel_end = .;
    } > fernos_area_0

    .data.kernel : {
        . = ALIGN(4K);
        _data_kernel_start = .;
        *libk_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_kernel_end = .;
    } > fernos_area_0

    .bss.user (NOLOAD) : {
        . = ALIGN(4K);
        _bss_user_start = .;
        *libu_*.a:*.o(.bss)
        *libu_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_user_end = .;
    } > fernos_area_0

    .data.user : {
        . = ALIGN(4K);
        _data_user_start = .;
        *libu_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_user_end = .;
    } > fernos_area_0

    .misc : {
        . = ALIGN(4K);
        _static_area_end = .;
    } > fernos_area_0

    .init_kernel_stack (NOLOAD) : {
        . = ALIGN(4K);
        _init_kstack_start = .;
        . += INIT_KERNEL_STACK_SIZE;
        . = ALIGN(4K);
        _init_kstack_end = .;
    } > fernos_area_1
}
