

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

/* 
 * The beginning of the Identity area will be BIOS stuff which may still be useful during kernel setup.
 * For example, the terminal graphics buffer.
 */

/* 16 KB */
INIT_KERNEL_STACK_SIZE =  0x4000;

MEMORY 
{
    /* Nothing goes in the preface area! These all will be 4MB aligned to help with paging. */

    prologue       : ORIGIN = PROLOGUE_START, LENGTH = PROLOGUE_END - PROLOGUE_START + 1
    fernos_area    : ORIGIN = FERNOS_START,   LENGTH = FERNOS_END   - FERNOS_START + 1
    epilogue       : ORIGIN = EPILOGUE_START, LENGTH = EPILOGUE_END - EPILOGUE_START + 1
}

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* It used to be universally recommended to use 1M as a start offset,
	   as it was effectively guaranteed to be available under BIOS systems.
	   However, UEFI has made things more complicated, and experimental data
	   strongly suggests that 2M is a safer place to load. In 2016, a new
	   feature was introduced to the multiboot2 spec to inform bootloaders
	   that a kernel can be loaded anywhere within a range of addresses and
	   will be able to relocate itself to run from such a loader-selected
	   address, in order to give the loader freedom in selecting a span of
	   memory which is verified to be available by the firmware, in order to
	   work around this issue. This does not use that feature, so 2M was
	   chosen as a safer option than the traditional 1M. */

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
