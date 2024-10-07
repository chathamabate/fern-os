
#ifndef UTIL_STR_H
#define UTIL_STR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool str_eq(const char *s1, const char *s2);

// Returns number of characters copied. (excluding \0)
size_t str_cpy(char *dest, const char *src);

// Right aligned unsinged int to string, returns number of padding characters
// written. Writes \0 at buf[n]. ALWAYS writes n+1 character include \0.
size_t str_of_u_ra(char *buf, size_t n, char pad, uint32_t u);

// Writes at most 11 characters including the NULL terminator.
size_t str_of_u(char *buf, uint32_t u);

size_t str_of_i_ra(char *buf, size_t n, char pad, int32_t i);

// Writes at most 12 characters including the NULL terminator.
size_t str_of_i(char *buf, int32_t i);

// Writes at most 9 characters including the NULL terminator.
// hex_digs = the number of hex digits to write.
size_t str_of_hex(char *buf, uint32_t u, uint8_t hex_digs);

// Returns true if tests succeed.
bool test_str(void);

#endif
