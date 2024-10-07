
#include "util/str.h"
#include "msys/test.h"

static bool test_str_eq(void) {
    TEST_TRUE(str_eq("Hello", "Hello")); 
    TEST_TRUE(str_eq("H", "H")); 
    TEST_TRUE(str_eq("4294967295","4294967295"));
    TEST_FALSE(str_eq("A", "H")); 
    TEST_FALSE(str_eq("Hell", "Hello"));
    TEST_FALSE(str_eq("HEllo", "Hello")); 
    TEST_SUCCEED();
}

static bool test_str_cpy(void) {
    char buf[100];

    TEST_TRUE(str_cpy(buf, "HELLO") == 5);
    TEST_TRUE(str_eq(buf, "HELLO"));

    TEST_TRUE(str_cpy(buf, "") == 0);
    TEST_TRUE(str_eq(buf, ""));

    TEST_SUCCEED();
}

typedef struct _test_str_of_u_ra_case_t {
    size_t n;
    char pad;
    uint32_t u;

    const char *exp_str;
    size_t exp_pads;
} test_str_of_u_ra_case_t;

static const test_str_of_u_ra_case_t TEST_STR_OF_U_RA_CASES[] = {
    {
        .n = 0,
        .pad = ' ',
        .u = 0,

        .exp_str = "",
        .exp_pads = 0,
    },
    {
        .n = 5,
        .pad = ' ',
        .u = 0,

        .exp_str = "    0",
        .exp_pads = 4,
    },
    {
        .n = 1,
        .pad = ' ',
        .u = 0,

        .exp_str = "0",
        .exp_pads = 0,
    },
    {
        .n = 3,
        .pad = ' ',
        .u = 123,

        .exp_str = "123",
        .exp_pads = 0,
    },
    {
        .n = 2,
        .pad = ' ',
        .u = 123,

        .exp_str = "23",
        .exp_pads = 0,
    },
    {
        .n = 3,
        .pad = ' ',
        .u = 1000,

        .exp_str = "000",
        .exp_pads = 0,
    },
    {
        .n = 12,
        .pad = '*',
        .u = 0xFFFFFFFF,

        .exp_str = "**4294967295",
        .exp_pads = 2,
    },
};
static const size_t TEST_STR_OF_U_RA_CASES_LEN = 
    sizeof(TEST_STR_OF_U_RA_CASES) / sizeof(test_str_of_u_ra_case_t);

static bool test_str_of_u_ra(void) {
    char buf[100];
    for (size_t i = 0; i < TEST_STR_OF_U_RA_CASES_LEN; i++) {
        const test_str_of_u_ra_case_t *c = 
            &(TEST_STR_OF_U_RA_CASES[i]);

        size_t pads = str_of_u_ra(buf, c->n, c->pad, c->u);
        TEST_TRUE(str_eq(buf, c->exp_str));
        TEST_TRUE(pads == c->exp_pads);
    }
    TEST_SUCCEED();
}

static bool test_str_of_u(void) {
    // This kinda just uses str_of_u_ra, so don't need
    // that many comprehensive tests here...

    char buf[100];

    TEST_TRUE(str_of_u(buf, 0xFFFFFFFF) == 10);
    TEST_TRUE(str_eq(buf, "4294967295"));

    TEST_TRUE(str_of_u(buf, 0) == 1);
    TEST_TRUE(str_eq(buf, "0"));

    TEST_TRUE(str_of_u(buf, 1024) == 4);
    TEST_TRUE(str_eq(buf, "1024"));

    TEST_SUCCEED();
}

static bool test_str_of_i_ra(void) {
    // kinda just uses str_of_u_ra again...

    char buf[100];
    
    TEST_TRUE(str_of_i_ra(buf, 5, ' ', -1) == 3);
    TEST_TRUE(str_eq(buf, "   -1"));

    TEST_TRUE(str_of_i_ra(buf, 1, ' ', -1) == 0);
    TEST_TRUE(str_eq(buf, "1"));

    TEST_TRUE(str_of_i_ra(buf, 12, ' ', 0x80000000) == 1);
    TEST_TRUE(str_eq(buf, " -2147483648"));

    TEST_TRUE(str_of_i_ra(buf, 5, ' ', 126) == 2);
    TEST_TRUE(str_eq(buf, "  126"));

    TEST_SUCCEED();
}


static bool test_str_of_i(void) {
    char buf[100];

    TEST_TRUE(str_of_i(buf, 100) == 3);
    TEST_TRUE(str_eq(buf, "100"));

    TEST_TRUE(str_of_i(buf, -1) == 2);
    TEST_TRUE(str_eq(buf, "-1"));

    TEST_TRUE(str_of_i(buf, 0x80000000) == 11);
    TEST_TRUE(str_eq(buf, "-2147483648"));

    TEST_SUCCEED();
}

typedef struct _test_str_of_hex_case_t {
    uint32_t in_val;
    uint8_t digits;
    const char *exp_str;
} test_str_of_hex_case_t;

static const test_str_of_hex_case_t TEST_STR_OF_HEX_CASES[] = {
    {
        .in_val = 0xF,
        .digits = 1,
        .exp_str = "F"
    },
    {
        .in_val = 0xF,
        .digits = 3,
        .exp_str = "00F"
    },
    {
        .in_val = 0xAA1100,
        .digits = 8,
        .exp_str = "00AA1100"
    },
};
static const size_t TEST_STR_OF_HEX_CASES_LEN = 
    sizeof(TEST_STR_OF_HEX_CASES) / sizeof(test_str_of_hex_case_t);

static bool test_str_of_hex(void) {
    char buf[20];
    for (size_t i = 0; i < TEST_STR_OF_HEX_CASES_LEN; i++) {
        const test_str_of_hex_case_t *c = &(TEST_STR_OF_HEX_CASES[i]);
        size_t chars = str_of_hex(buf, c->in_val, c->digits);
        TEST_TRUE(str_eq(buf, c->exp_str));
        TEST_TRUE(chars == c->digits);
    }
    TEST_SUCCEED();
}

bool test_str(void) {
    TEST_TRUE(test_str_eq());
    TEST_TRUE(test_str_cpy());
    
    TEST_TRUE(test_str_of_u_ra());
    TEST_TRUE(test_str_of_u());

    TEST_TRUE(test_str_of_i_ra());
    TEST_TRUE(test_str_of_i());

    TEST_TRUE(test_str_of_hex());

    TEST_SUCCEED();
}
