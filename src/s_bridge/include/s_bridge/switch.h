
#include "k_sys/page.h"

/**
 * Interrupt 48 is used for system calls in FernOS.
 */
#define FOS_SYSCALL_INT (48)

/**
 * In fernos, a system call takes 5 arbitrary arguments and returns a value.
 *
 * When a system call interrupt occurs, the handler will pass all arguments to a certain routine.
 * That routine can be set at runtime here.
 */
void set_syscall_routine(uint32_t (*r)(uint32_t, uint32_t, uint32_t, uint32_t));

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
 */
void syscall_hndlr(void);


/**
 * Perform a syscall, FROM USER SPACE!
 */
uint32_t syscall(phys_addr_t pd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
