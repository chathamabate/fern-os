
#pragma once

#include "k_sys/page.h"

/*
 * FernOS Expects that interrupts/exceptions only occur when executing a user process.
 *
 * If an exception occurs while in the kernel, the kernel should lock-up!
 */

/**
 * All interrupts must execute out of the kernel context.
 *
 * This module though does not have direct access to this context though.
 *
 * You can provide it to the module using this function. (Make sure to do this during init)
 */
void set_intr_ctx(phys_addr_t pd, const uint32_t *esp);

/**
 * The lock up handler will kinda be for initial debugging purposes.
 *
 * The action taken should never return! Maybe even print out a little message to the screen.
 */
void set_lock_up_action(void (*action)(void));
void lock_up_handler(void);

// OK, now what other handlers can be placed here too I think???
