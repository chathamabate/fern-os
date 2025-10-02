
#pragma once

#include "s_util/err.h"

/*
 * The keyboard tests are NOT unit tests!
 *
 * These will be interactive and print to the default out handle.
 * 
 * TODO: Consider changing how the default out handle works? Or adding more features?
 * It would be nice if the process could know what terminal it is printing to.
 * (i.e. be able to get its dimmensions)
 */

/**
 * This runs a simple command prompt. 
 *
 * Hitting <Enter> will print out what was entered.
 * Hitting <Esc> will exit the prompt.
 */
fernos_error_t test_kb_simple_prompt(void);

/**
 * This tests reading from the keyboard in weird odd amounts.
 *
 * Just type. It'll run forever.
 * You should expect to see the keycodes you typed during the "Reading" phase
 * to be printed. Then keycodes during "Sleeping" phase to be ignored.
 */
fernos_error_t test_kb_funky(void);
