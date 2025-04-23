
#pragma once

#include "k_sys/page.h"

/**
 * All interrupts must execute out of the kernel context.
 *
 * This module though does not have direct access to this context though.
 *
 * You can provide it to the module using this function. (Make sure to do this during init)
 */
void set_intr_ctx(phys_addr_t pd, const uint32_t *esp);

/**
 * Return to an arbitrary thread.
 */
void context_return(phys_addr_t pd, const uint32_t *esp);

/**
 * Return to an arbitrary thread with a return value! (Like from a syscall)
 */
void context_return_value(phys_addr_t pd, const uint32_t *esp, int32_t retval);

/**
 * These two functions are really just a proof of concept. Might take them out later.
 */
void enter_intr_ctx(void);
void leave_intr_ctx(void);

void lock_up_handler(void);

void nop_master_irq_handler(void);
void nop_master_irq7_handler(void); // For spurious.

void nop_slave_irq_handler(void);
void nop_slave_irq15_handler(void); // For spurious.

/**
 * The "syscall action" is specified by the kernel. 
 *
 * The enter handler will pass the following values into the action when a syscall interrupt occurs.
 *
 * This actions SHOULD NOT RETURN (in the traditional sense at least)
 *
 * If the kernel would like to return to the original thread, it should load in pd and esp manually.
 * (It should also load a return value into %eax)
 */
typedef void (*syscall_action_t)(phys_addr_t pd, const uint32_t *esp, uint32_t id, uint32_t arg);

void set_syscall_action(syscall_action_t sa);

/**
 * When this handler is entered, %eax should hold the syscall id,
 * and %ecx should hold the syscall argument.
 */
void syscall_enter_handler(void);


/**
 * Trigger a system call from userspace!
 */
int32_t trigger_syscall(uint32_t id, uint32_t arg);


/**
 * Just like syscall action, this SHOULD NOT RETURN EVER. 
 *
 * When the timer action is invoked, power is given over to the kernel.
 * The kernel can return to a user context using the context return endpoints above.
 */
typedef void (*timer_action_t)(phys_addr_t pd, const uint32_t *esp);

void set_timer_action(timer_action_t ta);

void timer_handler(void);



void random_handler(void);

