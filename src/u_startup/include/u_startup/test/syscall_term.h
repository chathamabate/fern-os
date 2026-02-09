
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
 * When a deregistered event is received, the child process will exit
 * and the parent process will return FOS_E_SUCCESS.
 *
 * This is really testing that reading events behaves correctly when 
 * done my multiple copied terminal handles.
 */
fernos_error_t test_terminal_fork0(handle_t h_t);

