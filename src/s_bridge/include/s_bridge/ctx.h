
#pragma once

#include <stdint.h>

typedef struct _user_ctx_t user_ctx_t;

/**
 * This structure resembles EXACTLY what should be pushed onto the stack during a
 * privilege change.
 *
 * Some of these things are pushed automatically, others must be pushed by the handlers.
 */
struct _user_ctx_t {
    // The below values are ALL expected to be pushed explicitly.
   
    uint32_t cr3;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t temp_esp; // Basically a nop. (Set to the correct value during switch)
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax; 

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
void enter_user_ctx(user_ctx_t *ctx);
