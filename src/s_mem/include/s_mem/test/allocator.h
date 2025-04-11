
#pragma once

#include <stdbool.h>
#include "s_mem/allocator.h"

/**
 * Run the allocator suite given the name of the suite, and a generator for creating a fresh 
 * instance of the allocator.
 *
 * This test assumes that the allocator being tested has around a megabyte of memory at its 
 * disposal.
 */
bool test_allocator(const char *name, allocator_t *(*gen)(void));

