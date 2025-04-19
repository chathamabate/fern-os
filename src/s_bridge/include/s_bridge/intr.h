
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
 * This should switch to the intr_pd and stack.
 *
 * It should write the previous pd and esp to the given pointers.
 * hmmm... 
 *
 * If we are already in the interrupt context, nbd, just keep going from the current
 * stack position!
 */
void enter_intr_ctx(void);
void leave_intr_ctx(void);

void lock_up_handler(void);

void nop_master_irq_handler(void);
void nop_master_irq7_handler(void); // For spurious.

void nop_slave_irq_handler(void);
void nop_slave_irq15_handler(void); // For spurious.


void timer_handler(void);
