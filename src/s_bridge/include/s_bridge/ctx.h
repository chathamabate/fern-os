
#pragma once

#include "k_sys/page.h"
#include <stdint.h>

typedef struct _user_ctx_t user_ctx_t;

/**
 * NOTE: An interrupt action receives a pointer to a context.
 * This context lives on the kernel stack.
 *
 * In the case of a privilege change, the full context is useable!
 *
 * However, in the case where there was no privilege change,  NEVER EVER EVER access
 * the esp and ss fields. These will be nonexistent, and thus could write over important info,
 * or cause a page fault!
 *
 * To check if a privilege change occured, you can check the cs value in the context.
 * A userspace CS means a privilege change occured!
 */
typedef void (*intr_action_t)(user_ctx_t *ctx);

/**
 * When an interrupt occurs, we will switch page tables.
 *
 * Make sure to set the intr ctx pd before enabling interrupts!
 *
 * This is deprecated! 
 *
 * NOTE: Now we assume that the very first value of the kernel stack
 * holds the kernel page table physical address.
 *
 * NOTE: This being said, all interrupt handlers expect to only ever
 * trigger from user mode; interrupts should always be blocked while in
 * the kernel.
 */
// void set_intr_ctx_pd(phys_addr_t pd);

/**
 * Some no-op handlers.
 */

void nop_master_irq_handler(void);
void nop_master_irq7_handler(void); // For spurious.

void nop_slave_irq_handler(void);
void nop_slave_irq15_handler(void); // For spurious.

/**
 * This structure resembles EXACTLY what should be pushed onto the stack during a
 * privilege change.
 *
 * Some of these things are pushed automatically, others must be pushed by the handlers.
 */
struct _user_ctx_t {
    // The below values are ALL expected to be pushed explicitly.
   
    /**
     * We expect that the given context uses the same data segment in all segment
     * registers. Having this stored in the context here makes my life a lot easier.
     *
     * Remember, the `ss` field below only exists when switching from user to kernel!
     */
    uint32_t ds;

    uint32_t cr3;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t temp_esp; // Basically a nop. (Set to the correct value during switch)
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax; 

    // A space holder used to help when context switching.
    
    uint32_t reserved;

    // Error is only pushed autmatically when an error occurs.
    // It'll be ignored when returning to the user context from the kernel.

    uint32_t err;

    // Below values are ALWAYS pushed automatically.

    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
} __attribute__ ((packed));

/**
 * This function does not return.
 */
void return_to_ctx(user_ctx_t *ctx);

/**
 * Set the general protection fault action.
 */
void set_gpf_action(intr_action_t ia);

/**
 * General protextion fault handler.
 */
void gpf_handler(void);

/**
 * When the timer handler is invoked by an interrupt, it saves the
 * current context on the interrupt stack, then calls a custom action
 * with a pointer to the context.
 *
 * The timer actions should NEVER EVER return.
 *
 * To return to a user context, use `enter_user_ctx` above.
 */

void set_timer_action(intr_action_t ta);

/**
 * NOTE: The timer handler assumes that a privilege change occured when entering
 * the handler.
 *
 * If I ever decided to introduce kernel space concurrency, this will have to change.
 */
void timer_handler(void);




