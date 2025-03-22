

#pragma once

#include <stdbool.h>
#include "s_util/ansii.h"

static const char *_running_test_name;

#ifndef PRETEST 
#define PRETEST()
#endif

#ifndef POSTTEST
#define POSTTEST()
#endif

#ifndef LOGF_METHOD
#define LOGF_METHOD(...)
#endif

#ifndef FAILURE_ACTION
#define FAILURE_ACTION() return false
#endif

#define LOGF_PREFIXED(...)  \
    do { \
        LOGF_METHOD("[%s] ", _running_test_name); \
        LOGF_METHOD(__VA_ARGS__); \
    } while (0)

#define TEST_SUCCEED() return true

#define TEST_FAIL() \
    do { \
        LOGF_PREFIXED("Failure at %s:%u\n",__func__,__LINE__); \
        FAILURE_ACTION(); \
    } while (0)


#define TEST_EQUAL_W_FMT(fmt, exp, act) \
    do { \
        if ((exp) != (act)) { \
            LOGF_PREFIXED("Expected: " fmt " Actual: " fmt "\n", exp, act); \
            TEST_FAIL(); \
        } \
    } while (0)

#define TEST_EQUAL_INT(exp, act) TEST_EQUAL_W_FMT("%d", exp, act)
#define TEST_EQUAL_UINT(exp, act) TEST_EQUAL_W_FMT("%u", exp, act)
#define TEST_EQUAL_HEX(exp, act) TEST_EQUAL_W_FMT("0x%X", exp, act)

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

static void _run_test(const char *name, bool (*t)(void)) {
    _running_test_name = name;

    PRETEST();
    bool res = t();
    POSTTEST();

    LOGF_PREFIXED("%s\n", res 
            ? ANSII_GREEN_FG "PASSED" ANSII_RESET 
            : ANSII_RED_FG "FAILED" ANSII_RESET
    );
}

#define RUN_TEST(test) _run_test(#test, test)



