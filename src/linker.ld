

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

FERNOS_LENGTH = FERNOS_END - FERNOS_START + 1;

MEMORY 
{
    /* Nothing goes in the preface area! These all will be 4MB aligned to help with paging. */

    prologue       : ORIGIN = PROLOGUE_START, LENGTH = PROLOGUE_END - PROLOGUE_START + 1

    /* 
     * We make the "fernos area" purposely smaller, so we can gaurantee enough room 
     * for an initial stack.
     */
    fernos_area    : ORIGIN = FERNOS_START, LENGTH  = FERNOS_LENGTH

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
    } > fernos_area

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
    } > fernos_area

    .text.kernel : {
        . = ALIGN(4K);
        _ro_kernel_start = .;
        *libk_*.a:*.o(.text)
        *libk_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_kernel_end = .;
    } > fernos_area

    .text.user : {
        . = ALIGN(4K);
        _ro_user_start = .;
        *libu_*.a:*.o(.text)
        *libu_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_user_end = .;
    } > fernos_area

    /* BSS and Data Sections */

    .bss.shared (NOLOAD) : {
        . = ALIGN(4K);
        _bss_shared_start = .;
        *libs_*.a:*.o(.bss)
        *libs_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_shared_end = .;
    } > fernos_area

    .data.shared : {
        . = ALIGN(4K);
        _data_shared_start = .;
        *libs_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_shared_end = .;
    } > fernos_area

    .bss.kernel (NOLOAD) : {
        . = ALIGN(4K);
        _bss_kernel_start = .;
        *libk_*.a:*.o(.bss)
        *libk_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_kernel_end = .;
    } > fernos_area

    .data.kernel : {
        . = ALIGN(4K);
        _data_kernel_start = .;
        *libk_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_kernel_end = .;
    } > fernos_area

    .bss.user (NOLOAD) : {
        . = ALIGN(4K);
        _bss_user_start = .;
        *libu_*.a:*.o(.bss)
        *libu_*.a:*.o(COMMON)
        . = ALIGN(4K);
        _bss_user_end = .;
    } > fernos_area

    .data.user : {
        . = ALIGN(4K);
        _data_user_start = .;
        *libu_*.a:*.o(.data)
        . = ALIGN(4K);
        _data_user_end = .;
    } > fernos_area

    .misc : {
        . = ALIGN(4K);
        _static_area_end = .;
    } > fernos_area
}
