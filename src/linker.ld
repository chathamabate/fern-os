

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

KERNEL_STACK_SIZE = 64K;

/* 
 * The beginning of the Identity area will be BIOS stuff which may still be useful during kernel setup.
 * For example, the terminal graphics buffer.
 */

/* THESE MUST BE 4K ALIGNED TO HELP WITH PAGING */
FERNOS_START = IDENTITY_AREA_START + IDENTITY_AREA_FERNOS_OFFSET;
FERNOS_END =  IDENTITY_AREA_END;

MEMORY 
{
    fernos_area   : ORIGIN = FERNOS_START, LENGTH = FERNOS_END - FERNOS_START 
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

    .text : {
		*(.multiboot)
        *(.text)
    } > fernos_area

    .rodata : {
        *(.rodata)
    } > fernos_area

    /* 
     * Basically, everything in the fernos_area should be readonly except for 
     * .data, .bss, COMMON, and .kernel_stack
     */

    .data : {
        . = ALIGN(4K);

        _data_start = .;
        *(.data)
        . = ALIGN(4K);
        _data_end = .;
    } > fernos_area

    COMMON : {
        . = ALIGN(4K);

        _common_start = .;
        *(COMMON)

        . = ALIGN(4K);
        _common_end = .;
    } > fernos_area

    .bss : {
        . = ALIGN(4K);

        _bss_start = .;
        *(.bss)
        . = ALIGN(4K);
        _bss_end = .;
    } > fernos_area

    .kernel_stack : {
        . = ALIGN(4K);

        /* This may even be allowed to just be an array tbh */
        _kernel_stack_bottom = .;
        . +=  KERNEL_STACK_SIZE;
        . = ALIGN(4K);
        _kernel_stack_top = .;
    } > fernos_area
}
