# FernOS

My own 32-bit x86 operating system!

### Building and Running

Right now, FernOS is just a kernel. There is no shell/UI. 

To run code, edit `src/u_startup/main.c`. This file contains the main function of the first
and only user process spawned automatically by FernOS.

To create more processes and perform other system operations, 
see `src/u_startup/include/u_startup/syscall.h`.

__NOTE:__ In its current condition, FernOS has no disk driver. The build process uses `qemu`'s
`-kernel` flag to boot directly from an ELF file.

```bash
# Build and run FernOS in qemu
make qemu.bin

# Clean all build artifacts
make clean
```


