# FernOS

A hobby 32-bit x86 operating system.

### Building and Running

Right now, FernOS is just a kernel. There is no shell/UI. 

To run code, edit `src/u_startup/main.c`. This file contains the main function of the first 
user process.

To create more processes and perform other system operations, 
see `src/u_startup/include/u_startup/syscall.h` and `src/u_startup/include/u_startup/syscall_fs.h`.

__NOTE:__ In its current condition, FernOS does not boot from a disk image. The build process uses `qemu`'s
`-kernel` flag to boot directly from an ELF file.

```bash
# Build and run FernOS in qemu
make qemu.bin

# Clean all build artifacts
make clean
```

### Requirements

The above `make` commands will only success if you have the `i686-elf-binutils` and `qemu`.


