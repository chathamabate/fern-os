
#pragma once

#include <stdbool.h>

#include "k_bios_term/term.h"

// Allowing this for now.
#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

bool test_str(void);
