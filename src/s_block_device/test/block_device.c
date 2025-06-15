
#include "s_block_device/test/block_device.h"

#include <stdbool.h>
#include "k_bios_term/term.h"
#include "s_block_device/block_device.h"
#include "s_mem/allocator.h"
#include "s_util/str.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static block_device_t *(*gen_block_device)(void) = NULL;
static block_device_t *bd = NULL;
static size_t num_sectors = 0;
static size_t sector_size = 0;
static size_t num_al_blocks = 0;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    bd = gen_block_device(); 
    TEST_TRUE(bd != NULL);

    num_sectors = bd_num_sectors(bd);
    TEST_TRUE(num_sectors >= TEST_BLOCK_DEVICE_MIN_SECTORS);

    sector_size = bd_sector_size(bd);
    TEST_TRUE(sector_size > 0);

    TEST_SUCCEED();
}

static bool posttest(void) {
    delete_block_device(bd);

    bd = NULL;
    num_sectors = 0;
    sector_size = 0;

    size_t nalb = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(num_al_blocks, nalb);

    num_al_blocks = 0;

    TEST_SUCCEED();
}

static bool test_simple_rw0(void) {
    fernos_error_t err;

    uint8_t *wbuf = da_malloc(sector_size);
    TEST_TRUE(wbuf != NULL);

    uint8_t *rbuf = da_malloc(sector_size);
    TEST_TRUE(rbuf != NULL);

    for (uint32_t i = 0; i < sector_size; i++) {
        wbuf[i] = (uint8_t)i;
    }

    err = bd_write(bd, 1, 1, wbuf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    err = bd_read(bd, 1, 1, rbuf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (uint32_t i = 0; i < sector_size; i++) {
        TEST_EQUAL_UINT((uint8_t)i, rbuf[i]);
    }

    da_free(wbuf);
    da_free(rbuf);

    TEST_SUCCEED();
}

static bool test_simple_rw1(void) {
    // Maybe, write read write read?

    fernos_error_t err;

    uint8_t *wbuf = da_malloc(sector_size);
    TEST_TRUE(wbuf != NULL);

    uint8_t *rbuf = da_malloc(sector_size);
    TEST_TRUE(rbuf != NULL);

    for (uint32_t i = 0; i < 5; i++) {
        mem_set(wbuf, i, sector_size);

        err = bd_write(bd, 1, 1, wbuf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        err = bd_read(bd, 1, 1, rbuf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_TRUE(mem_chk(rbuf, i, sector_size));
    }

    da_free(wbuf);
    da_free(rbuf);

    TEST_SUCCEED();
}

static bool test_simple_rw2(void) {
    fernos_error_t err;

    const size_t WBUF_SECTORS = 6;
    const size_t RBUF_SECTORS = 2;
    TEST_TRUE(WBUF_SECTORS > RBUF_SECTORS);

    uint8_t *wbuf = da_malloc(sector_size * WBUF_SECTORS);
    TEST_TRUE(wbuf != NULL);

    uint8_t *rbuf = da_malloc(sector_size * RBUF_SECTORS);
    TEST_TRUE(rbuf != NULL);

    for (uint32_t i = 0; i < sector_size * WBUF_SECTORS; i++) {
        wbuf[i] = (uint8_t)('a' + (i % 26));
    }

    err = bd_write(bd, 1, WBUF_SECTORS, wbuf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (uint32_t s = 1; s <= WBUF_SECTORS + 1 - RBUF_SECTORS; s++) {
        err = bd_read(bd, s, RBUF_SECTORS, rbuf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        for (uint32_t i = 0; i < sector_size * RBUF_SECTORS; i++) {
            TEST_EQUAL_HEX(wbuf[((s - 1) * sector_size) + i], rbuf[i]);
        }
    }

    da_free(wbuf);
    da_free(rbuf);

    TEST_SUCCEED();
}

static bool test_complex_rw(void) {
    const size_t MAX_SECTOR = 22;
    TEST_TRUE(MAX_SECTOR < TEST_BLOCK_DEVICE_MIN_SECTORS);
    TEST_TRUE(MAX_SECTOR % 2 == 0); // Must be even for this to work as expected.

    fernos_error_t err;

    void *buf;

    buf = da_malloc(2 * sector_size);

    // This will write [(1, 2), (3, 4) .. (MAX_SECTOR - 1, MAX_SECTOR)]
    for (uint32_t s = 1; s + 1 <= MAX_SECTOR; s += 2) {
        mem_set(buf, (uint8_t)s, sector_size);
        mem_set((uint8_t *)buf + sector_size, (uint8_t)(s + 1), sector_size);

        err = bd_write(bd, s, 2, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Now let's try reading at an offset, and maybe writing?

    // This will read [(2, 3), (4, 5) ... (MAX_SECTOR - 2, MAX_SECTOR - 1)]
    // Also it will write... [2, 4 ... MAX_SECTOR - 2]
    for (uint32_t s = 2; s + 1 <= MAX_SECTOR; s += 2) {
        err = bd_read(bd, s, 2, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_TRUE(mem_chk(buf, (uint8_t)s, sector_size));
        TEST_TRUE(mem_chk((uint8_t *)buf + sector_size, (uint8_t)(s + 1), sector_size));

        mem_set(buf, (uint8_t)(3 * s), sector_size);
        err = bd_write(bd, s, 1, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);
    }

    // Write to MAX_SECTOR to make the final check of this test work nice.
    mem_set(buf, (uint8_t)(3 * MAX_SECTOR), sector_size);
    err = bd_write(bd, MAX_SECTOR, 1, buf);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);

    for (uint32_t s = 1; s <= MAX_SECTOR; s++) {
        err = bd_read(bd, s, 1, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        uint8_t exp = s % 2 == 0 
            ? (uint8_t)(3 * s) : s;

        TEST_TRUE(mem_chk(buf, exp, sector_size));
    }

    da_free(buf);

    TEST_SUCCEED();
}

static bool test_full_rw(void) {
    // Read/Write all Sectors of the BD. (Excluding sector 0 of course)
    fernos_error_t err;

    // Number of sectors to read/write each time.
    const size_t S_FACTOR = 5;

    void *buf = da_malloc(sector_size * S_FACTOR);

    for (uint32_t i = 1, o = 0; i < num_sectors; o++) {
        size_t s_left = num_sectors - i;
        size_t s2w = s_left < S_FACTOR ? s_left : S_FACTOR;

        mem_set(buf, o, S_FACTOR * sector_size );
        err = bd_write(bd, i, S_FACTOR, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        i += s2w;
    }

    for (uint32_t i = 1, o = 0; i < num_sectors; o++) {
        size_t s_left = num_sectors - i;
        size_t s2r = s_left < S_FACTOR ? s_left : S_FACTOR;

        err = bd_read(bd, i, s2r, buf);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        TEST_TRUE(mem_chk(buf, o, S_FACTOR * sector_size));

        i += s2r;
    }

    da_free(buf);

    TEST_SUCCEED();
}

bool test_block_device(const char *name, block_device_t *(*gen)(void)) {
    gen_block_device = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_simple_rw0);
    RUN_TEST(test_simple_rw1);
    RUN_TEST(test_simple_rw2);
    RUN_TEST(test_complex_rw);
    RUN_TEST(test_full_rw); // You may want to comment this out if it takes too long.
    return END_SUITE();
}
