
#ifndef UTIL_STR_H
#define UTIL_STR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool str_eq(const char *s1, const char *s2);

// Returns str_len(src)
size_t str_cpy(char *dest, const char *src);
size_t str_len(const char *src);

// Writes at most 11 characters including the NULL terminator.
size_t str_of_u(char *buf, uint32_t u);

// Writes at most 12 characters including the NULL terminator.
size_t str_of_i(char *buf, int32_t i);

// Writes at most 9 characters including the NULL terminator.
// hex_digs = the number of hex digits to write.
size_t str_of_hex(char *buf, uint32_t u, uint8_t hex_digs);

// NOTE: Each of the below calls always write n + 1 characters!
// (That is they always write \0)

void str_la(char *buf, size_t n, char pad, const char *s);
void str_ra(char *buf, size_t n, char pad, const char *s);
void str_center(char *buf, size_t n, char pad, const char *s);

// Returns true if tests succeed.
bool test_str(void);

#endif
