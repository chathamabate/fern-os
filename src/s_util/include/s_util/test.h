

#pragma once

#include <stdbool.h>

#ifndef LOGF_METHOD
#define LOGF_METHOD(...)
#endif

#ifndef FAILURE_ACTION
#define FAILURE_ACTION() return false
#endif

#define TEST_SUCCEED() return true

#define TEST_FAIL() \
    do { \
        LOGF_METHOD("Failure at %s:%u\n",__func__,__LINE__); \
        FAILURE_ACTION(); \
    } while (0)

#define TEST_TRUE(cond) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(); \
        } \
    } while (0)

#define TEST_FALSE(cond) \
    do { \
        if (cond) { \
            TEST_FAIL(); \
        } \
    } while (0)


