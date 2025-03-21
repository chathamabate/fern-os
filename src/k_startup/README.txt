
k_startup implements basic initialization/utility code to be used by
the kernel. It is stateful.

The kernel modules it depends on are only k_bios_term for debugging,
and k_sys.

The idea being that future drivers will depend on code in k_startup.
The kernel will depend on all drivers and k_startup.

         k_kernel
             |
             V
.... Various Stateful Drivers ....
        |      |       |
        V      V       V
            k_startup
                |
                V
            k_bios_term
                |
                V
              k_sys
