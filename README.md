# FernOS

A hobby 32-bit x86 operating system.

### Building and Running

To run code, edit `src/u_startup/main.c`. This file contains the main function of the first 
user process.

To create more processes and perform other system operations, 
see `src/u_startup/include/u_startup/syscall.h` and `src/u_startup/include/u_startup/syscall_fs.h`.

```bash
# Build and run FernOS in qemu
make run

# Clean all build artifacts
make clean
```

### Requirements

The above `make` commands will only succeed if you have the `i686-elf-binutils`, `qemu`, and `mtools`.

### Contributing

Earlier in this project I'd merge in large branches in entirety. I have decided to move away from
this strategy due to high number of commits.

Feature branches should be small and clear. Branches should be squashed before being merged.


