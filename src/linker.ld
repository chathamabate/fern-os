

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

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{

    /*
     * There will be one identity mapped area for the IDT/GDT
     * which should exist in all processes.
     *
     * Each table gets one full page, which should be more than enough!
     */
    
    .multiboot : {
        . = ALIGN(4K);
		*(.multiboot)
        . = ALIGN(4K);
    } > fernos_kernel_area
    
    .sys_tables : {
        . = ALIGN(4K);
        _sys_tables_start = .;
        _gdt_start = .;
        . += 4K;
        _gdt_end = .;
        _idt_start = .;
        . += 4K;
        _idt_end = .;
        _tss_start = .;
        . += 4K;
        _tss_end = .;
        . = ALIGN(4K);
        _sys_tables_end = .;
    } > fernos_kernel_area

    /*
     * Three types of areas (shared, kernel, and user)
     * Five parts per area (text, rodata, data, common, bss)
     */

    /* Read Only Sections text + rodata */

    .text.shared : {
        . = ALIGN(4K);
        _ro_shared_start = .;
        *libs_*.a:*.o(.text)
        *libs_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_shared_end = .;
    } > fernos_kernel_area

    .text.kernel : {
        . = ALIGN(4K);
        _ro_kernel_start = .;
        *libk_*.a:*.o(.text)
        *libk_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_kernel_end = .;
    } > fernos_kernel_area

    .text.user : {
        . = ALIGN(4K);
        _ro_user_start = .;
        *libu_*.a:*.o(.text)
        *libu_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_user_end = .;
    } > fernos_kernel_area

    /* BSS and Data Sections */

    .bss.shared (NOLOAD) : {
        . = ALIGN(4K);
        _bss_shared_start = .;
        *libs_*.a:*.o(.bss)
        *libs_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_shared_end = .;
    } > fernos_kernel_area

    .data.shared : {
        . = ALIGN(4K);
        _data_shared_start = .;
        *libs_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_shared_end = .;
    } > fernos_kernel_area

    .bss.kernel (NOLOAD) : {
        . = ALIGN(4K);
        _bss_kernel_start = .;
        *libk_*.a:*.o(.bss)
        *libk_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_kernel_end = .;
    } > fernos_kernel_area

    .data.kernel : {
        . = ALIGN(4K);
        _data_kernel_start = .;
        *libk_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_kernel_end = .;
    } > fernos_kernel_area

    .bss.user (NOLOAD) : {
        . = ALIGN(4K);
        _bss_user_start = .;
        *libu_*.a:*.o(.bss)
        *libu_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_user_end = .;
    } > fernos_kernel_area

    .data.user : {
        . = ALIGN(4K);
        _data_user_start = .;
        *libu_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_user_end = .;
    } > fernos_kernel_area

    .misc : {
        . = ALIGN(4K);
        _static_area_end = .;
    } > fernos_kernel_area
}
