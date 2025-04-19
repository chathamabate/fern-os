
#include "k_sys/page.h"

/**
 * The difficult part here is that all handlers must switch into kernel space.
 * I don't really know how to best organize this at this point...
 *
 * The handler must exist in the shared space, but must have access to the kernel 
 * structures. Kinda tricky tbh..
 *
 * This is allow for some cyclic feeling stuff...
 * Maybe I can give some more contextual stuff that the handlers use??
 *
 * Not a bad idea tbh...
 * The behavoir is given dynamically by the kernel at initializiation time??
 * Not a terrible idea IMO.
 * 
 * Lot's of things to think about here though tbh...
 */

/**
 * When going from user to kernel space, we must switch into a new context for working in the
 * kernel. Since this module is somewhat decoupled from everything else. It will not directly
 * access kernel fields. 
 *
 * Instead, use this funciton at init time to tell the bridge how it should behave!
 *
 * pd should be the kernel page directory.
 * esp should be the stack pointer to load when entering the stack.
 * r is the syscall routine.
 */
void set_bridge_context(phys_addr_t pd, const uint32_t *esp, 
        void (*r)(uint32_t, uint32_t, uint32_t, uint32_t));

/**
 * Interrupt 48 is used for system calls in FernOS.
 */
#define FOS_SYSCALL_INT (48)

/**
 * When we are in the kernel, we switch to userspace when scheduling a user thread.
 *
 * We will load some page directory and stack pointer, then call popal followed by iret.
 */
void switch_k2u(phys_addr_t pd, uint32_t *esp);

/**
 * We are in the kernel and we want to schedule a thread while also providing some
 * return value to the thread.
 *
 * The intent is to use this when returning from system calls.
 */
void switch_k2u_with_ret(phys_addr_t pd, uint32_t *esp, uint32_t eax);

/**
 * The interrupt handler invoked for syscalls.
 *
 * We need some sort of context for invoking system calls right???
 */
void syscall_handler(void);


/**
 * Perform a syscall, FROM USER SPACE!
 */
uint32_t syscall(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
