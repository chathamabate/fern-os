
#pragma once

#include <stdbool.h>
#include "s_mem/allocator.h"

/**
 * Run the allocator suite given the name of the suite, and a generator for creating a fresh 
 * instance of the allocator.
 *
 * This suite assumes the given allocator has at least 2 MB of memory at its disposal.
 *
 * NOTE: I am a little frustrated that these tests rely on the allocator having a certain amount
 * of memory. It'd be better if these tests succeeded regardless of what allocator was being used.
 */
bool test_allocator(const char *name, allocator_t *(*gen)(void));

