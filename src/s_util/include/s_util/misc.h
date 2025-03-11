
#pragma once

// Converts a number n to a 64-bit constant with n 1 lsbs
#define TO_MASK64(wid) ((1LL << wid) - 1)
