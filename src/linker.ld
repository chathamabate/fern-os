

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


MEMORY 
{
    /* Nothing goes in the preface area! These all will be 4MB aligned to help with paging. */

    prologue : ORIGIN = PROLOGUE_START, LENGTH = PROLOGUE_END - PROLOGUE_START + 1
    fernos   : ORIGIN = FERNOS_START,   LENGTH = FERNOS_END - FERNOS_START + 1
    epilogue : ORIGIN = EPILOGUE_START, LENGTH = EPILOGUE_END - EPILOGUE_START + 1
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


    .multiboot : {
        /* The truth is that this is not working correctly!)*/
    } > fernos

    .ro.shared : {
        . = ALIGN(4K);
        _ro_shared_start = .;
		*(.multiboot)
        libs_*.a:*.o(.text)
        libs_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_shared_end = .;
    } > fernos

    .ro.kernel : {
        . = ALIGN(4K);
        _ro_kernel_start = .;
        libk_*.a:*.o(.text)
        libk_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_kernel_end = .;
    } > fernos

    .ro.user : {
        . = ALIGN(4K);
        _ro_user_start = .;
        libu_*.a:*.o(.text)
        libu_*.a:*.o(.rodata)
        . = ALIGN(4K);
        _ro_user_end = .;
    } > fernos

    .rw.shared : {
        . = ALIGN(4K);
        _rw_shared_start = .;
        libs_*.a:*.o(.bss)
        libs_*.a:*.o(COMMON)
        libs_*.a:*.o(.data)
        . = ALIGN(4K);
        _rw_shared_end = .;
    } > fernos

    .rw.kernel : {
        . = ALIGN(4K);
        _rw_kernel_start = .;
        libk_*.a:*.o(.bss)
        libk_*.a:*.o(COMMON)
        libk_*.a:*.o(.data)
        . = ALIGN(4K);
        _rw_kernel_end = .;
    } > fernos

    .rw.user : {
        . = ALIGN(4K);
        _rw_user_start = .;
        libu_*.a:*.o(.bss)
        libu_*.a:*.o(COMMON)
        libu_*.a:*.o(.data)
        . = ALIGN(4K);
        _rw_user_end = .;
    } > fernos


    /*
     * Other than during initialization, when the kernel thread is running, it will have a shared
     * data area and a kernel data area.
     *
     * When a user thread is running, it will have a shared data area and a user data area.
     *
     * The user data which is initially loaded will be what is used for the first task.
     * After that, data will be copied using forks.
     */

    /* Shared Sections */
    /* Stack should not be defined in the linker script! */

}
