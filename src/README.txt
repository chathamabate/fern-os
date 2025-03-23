
Organization Notes:

The idea is that there will be functions which should ONLY ever be executed by the kernel
thread to prevent data races. The kernel thread will only run at initialization time and during
interrupt handling.

All modules with prefix `k_` contain functions of this type.

If a user task wants to interact with the kernel thread, it must perform a syscall style 
interrupt.

There will also be functions which are "shared", in that they can execute in any 
environment. Modules of such type will be prefixed `s_`.

Finally, user libraries should be prefixed with `u_`.

Memory Image Notes:
    
 [ Prologue       ]     (4MB Aligned) Reserved for BIOS Stuff. (mapped for kernel)
 [ Shared RO Data + Code ]     ALL code and read only data.
 [ Kernel RO Data + Code ]     ALL code and read only data.
 [ User RO Data + Code   ]     ALL code and read only data.
 [ Shared Data    ]     Data + bss + COMMON for all `shared` modules.
 [ Kernel Data    ]     Data + bss + COMMON for all `kernel` modules.
 [ User Data      ]     Data + bss + COMMON for all `user` modules.
 [ Heap           ]     Grows Up
 [ ......         ]     vv
 [ ......         ]     
 [ ......         ]
 [ ......         ]
 [ ......         ]     ^^
 [ Stack          ]     Grows Down 
 [ Epilogue       ]     (4MB Aligned) Unusable Area!

(All sections listed are at least 4K aligned always)

The idea is that there will be one and only one kernel thread. It will have it's own copy of shared
and kernel data.

During initialization, one user process will be spawned with its own copy of the shared and user 
data. A user thread will never be able to directly access the kernel data/code, and the 
kernel thread will never be able to directly access the user data/code of a user process. 
To make more processes the intial user process must fork.

Right now, all threads have their own independent memory layouts. (This may change if I introduce
multithreading within processes). The kernel thread will have a stack and heap just like any other 
thread!

Threre will always be one non-present page between the end of the heap and the top of the stack.
This should gaurantee a page fault when the stack overflows.

Also, "shared data" is not like shared between processes or anything. It is just the data section
which is used by the shared libraries (i.e. the libraries useable in either kernel or user space).
Every thread will have it's own copy of the "shared data" since every thread has permission to use
the code from the shared libraries. 

(If I ever create a compiler... loaded code will need to go somewhere too...)
