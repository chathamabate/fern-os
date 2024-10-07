
#ifndef MSYS_TEST_H
#define MSYS_TEST_H

#include <stdbool.h>

#define TEST_TRUE(cond) \
    do { \
        if (!(cond)) { return false; } \
    } while (0)

#define TEST_FALSE(cond) \
    do { \
        if (cond) { return false; } \
    } while (0)

#define TEST_SUCCEED() return true
#define TEST_FAIL() return false


#endif
