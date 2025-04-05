

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "s_util/misc.h"
#include "s_util/ansii.h"

#define HBAR "-----------------------------\n"

static const char *_running_suite_name;
static const char *_running_test_name;
static uint32_t _failures;
static uint32_t _total_run;

#ifndef PRETEST 
#define PRETEST() true
#endif

#ifndef POSTTEST
#define POSTTEST() true
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

static inline void BEGIN_SUITE(const char *name) {
    _running_suite_name = name;
    _failures = 0;
    _total_run = 0;

    LOGF_METHOD("Entering Test Suite: " ANSII_BRIGHT_CYAN_FG "%s" ANSII_RESET "\n", _running_suite_name);
    LOGF_METHOD(HBAR);
}

/**
 * Returns true on success, false on failure.
 */
static inline bool END_SUITE(void) {
    LOGF_METHOD(HBAR);
    if (_failures == 0) {
        LOGF_METHOD(ANSII_GREEN_FG "Suite Passed\n" ANSII_RESET);
    } else {
        LOGF_METHOD(ANSII_RED_FG "Suite Failed\n" ANSII_RESET);
        LOGF_METHOD("%u Failure(s) / %u Test(s)\n", _failures, _total_run); 
    }
    LOGF_METHOD("\n");

    return _failures == 0;
}

static inline void _run_test(const char *name, bool (*t)(void)) {
    _running_test_name = name;

    bool res = true;

    if (res) {
        res = PRETEST();
    }

    if (res) {
        res = t();
    }

    if (res) {
        res = POSTTEST();
    }

    _total_run++;

    if (res) {
        LOGF_PREFIXED("%s\n", ANSII_GREEN_FG "PASSED" ANSII_RESET);
    } else {
        LOGF_PREFIXED("%s\n", ANSII_RED_FG "FAILED" ANSII_RESET);
        _failures++;
    }
}

#define RUN_TEST(test) _run_test(#test, test)



