
#pragma once

#include <stdbool.h>
#include "s_data/list.h"

/**
 * Run the list test suite given the name of the suite, and a function for generating a new empty
 * list. (The generator takes a cell size as a parameter)
 *
 * These tests assume that the list generated uses the default allocator. If not, no big deal,
 * the memory leak test just won't do anything.
 */
bool test_list(const char *name, list_t *(*gen)(size_t cs)); 
