
#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * A normal "dummy window" is a self managed window which just logs events it receives.
 * This test basically turns the given terminal handle into a dummy window, just here in 
 * userspace.
 *
 * When a deregistered event is received, this function exits with FOS_E_SUCCESS.
 * Otherwise, it runs indefinitely.
 */
fernos_error_t test_userspace_dummy_term(handle_t h_t);

/**
 * This tests forking and using the same terminal from two different 
 * user processes.
 *
 * Both processes will exit when a deregistered event is received.
 */
fernos_error_t test_terminal_fork0(handle_t h_t);

