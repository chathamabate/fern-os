

/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(_start)

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

STACK_SIZE =    0x10000; /* 64 KB (Should this maybe be larger?) */

MEMORY 
{
    /* 
     * This will be mapped 1:1 from virtual memory.
     * A virtual address in this range will map to an identical physical address.
     */
    static_area : ORIGIN = 0x400000, LENGTH = 0xC00000 /* 4 MB - 16 MB */

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

	. = 0x400000; /* Actually starting at 4 MB here to make initial paging easy */

    /* Should we have one page directory?? */

    /* I believe this is just this initial stack */
    .stack BLOCK(4K) : ALIGN(4K) {
        _stack_bottom = .;
        . += STACK_SIZE;
        _stack_top = .;
    } > static_area

	/* First put the multiboot header, as it is required to be put very early
	   in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	} > static_area
 
	/* Read-only data. */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	} > static_area
 
	/* Read-write data (initialized) */
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	} > static_area
 
	/* Read-write data (uninitialized) and stack */
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	} > static_area
}
