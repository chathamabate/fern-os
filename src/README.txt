
Organization Notes:

The idea is that there will be functions which should ONLY ever be executed by the kernel
thread to prevent data races.

All modules with prefix `k_` contain functions of this type.

If a user task wants to interact with the kernel thread, it must perform a syscall style 
interrupt.

(`term` right now is for testing purposes and will probably be removed/remade later)

There will also be functions which are "standalone", in that they can execute in any 
environment. Modules of such type will be prefixed `s_`.
