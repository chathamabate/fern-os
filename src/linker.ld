

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


MEMORY 
{
    fernos_area   : ORIGIN = IDENTITY_AREA_FERNOS_OFFSET, LENGTH = IDENTITY_AREA_SIZE
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
        . = ALIGN(4K);

        _text_start = .;
		*(.multiboot)
        *(.text)

        . = ALIGN(4K);
        _text_end = .;
    } > fernos_area

    .rodata : {
        . = ALIGN(4K);

        _rodata_start = .;
        *(.rodata)

        . = ALIGN(4K);
        _rodata_end = .;
    } > fernos_area

    /*
     * Ok, I am going to separate, the kernel data and bss from the user data/bss.
     * The idea here being that when I implement task forking, I can easily remove access to
     * kernel data from the user task. Additionally, I can give all user tasks there own copy of
     * the data/bss easily.
     */

    .data.kernel : {
        . = ALIGN(4K);
        _data_kernel_start = .;
        libk_*.a:*.o(.data)

        . = ALIGN(4K);
        _data_kernel_end = .;
    } > fernos_area

    COMMON.kernel : {
        . = ALIGN(4K);
        _COMMON_kernel_start = .;
        libk_*.a:*.o(.COMMON)

        . = ALIGN(4K);
        _COMMON_kernel_end = .;
    }

    .bss.kernel : {
        . = ALIGN(4K);
        _bss_kernel_start = .;
        libk_*.a:*.o(.bss)

        . = ALIGN(4K);
        _bss_kernel_end = .;
    } > fernos_area

    /* User sections */

    .data.user : {
        . = ALIGN(4K);
        _data_user_start = .;
        libu_*.a:*.o(.data)

        . = ALIGN(4K);
        _data_user_end = .;
    } > fernos_area

    COMMON.user : {
        . = ALIGN(4K);
        _COMMON_user_start = .;
        libu_*.a:*.o(.COMMON)

        . = ALIGN(4K);
        _COMMON_user_end = .;
    }

    .bss.user : {
        . = ALIGN(4K);
        _bss_user_start = .;
        libu_*.a:*.o(.bss)

        . = ALIGN(4K);
        _bss_user_end = .;
    } > fernos_area

    /* Shared Sections */

    .data.shared : {
        . = ALIGN(4K);
        _data_shared_start = .;
        libs_*.a:*.o(.data)

        . = ALIGN(4K);
        _data_shared_end = .;
    } > fernos_area

    COMMON.shared : {
        . = ALIGN(4K);
        _COMMON_shared_start = .;
        libs_*.a:*.o(.COMMON)

        . = ALIGN(4K);
        _COMMON_shared_end = .;
    }

    .bss.shared : {
        . = ALIGN(4K);
        _bss_shared_start = .;
        libs_*.a:*.o(.bss)

        . = ALIGN(4K);
        _bss_shared_end = .;
    } > fernos_area

    /* Stack should not be defined in the linker script! */

}
