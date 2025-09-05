
#include "s_util/hash.h"
#include "s_util/str.h"
bool str_equator(const void *k0, const void *k1) {
    return str_eq(*(const char * const *)k0, *(const char * const *)k1);
}

uint32_t str_hasher(const void *k) {
    uint32_t hash = 1;
    const char *iter = *(const char * const *)k;

    while (*iter != '\0') {
        hash += ((uint8_t)*iter + 7) + (hash * 3);
        iter++;
    }

    return hash;
}
