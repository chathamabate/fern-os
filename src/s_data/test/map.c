
#include "s_data/test/map.h"
#include "s_data/map.h"

#include "k_startup/gfx.h"
#include "s_mem/allocator.h"

#include "s_util/str.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

#include "s_util/test.h"

static map_t *(*gen_map)(size_t ks, size_t vs) = NULL;
static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

static bool test_map_simple_put_and_get(void) {
    fernos_error_t err;

    map_t *mp = gen_map(sizeof(uint32_t), sizeof(uint32_t));
    TEST_TRUE(mp != NULL);

    const uint32_t NUMS_TO_ADD = 10;

    for (uint32_t i = 0; i < NUMS_TO_ADD; i++) {
        uint32_t val = i * 2;
        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                mp_put(mp, &i, &val));

        // Test that after every addition, previously added entries are still intact.
        for (uint32_t j = 0; j <= i; j++) {
            const uint32_t *key;
            uint32_t *val;

            err = mp_get_kvp(mp, &j, (const void **)&key, (void **)&val);
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

            TEST_EQUAL_UINT(j, *key);
            TEST_EQUAL_UINT(j * 2, *val);
        }
    }

    TEST_EQUAL_UINT(NUMS_TO_ADD, mp_len(mp));

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_map_simple_overwrite(void) {
    map_t *mp = gen_map(sizeof(uint32_t), sizeof(uint32_t));
    TEST_TRUE(mp != NULL);

    const uint32_t NUMS_TO_ADD = 20;

    for (uint32_t i = 0; i < NUMS_TO_ADD; i++) {
        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                mp_put(mp, &i, &i));
    }

    for (uint32_t i = 0; i < NUMS_TO_ADD; i += 2) {
        uint32_t new_val = i * 2;
        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                mp_put(mp, &i, &new_val));
    }

    for (uint32_t i = 0; i < NUMS_TO_ADD; i++) {
        uint32_t *val = mp_get(mp, &i);
        TEST_TRUE(val != NULL);

        if (i & 1) { // odd indeces were untouched!
            TEST_EQUAL_UINT(i, *val);
        } else { // even indeces were doubled!
            TEST_EQUAL_UINT(2 * i, *val);
        }
    }

    TEST_EQUAL_UINT(NUMS_TO_ADD, mp_len(mp));

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_map_simple_remove(void) {
    fernos_error_t err;

    map_t *mp = gen_map(sizeof(uint32_t), sizeof(uint8_t));
    TEST_TRUE(mp != NULL);

    const uint32_t NUMS_TO_ADD = 30;
    const uint32_t START_NUM = 50; // Must be even.
    const uint32_t END_NUM = START_NUM + NUMS_TO_ADD;

    for (uint32_t i = START_NUM; i < END_NUM; i++) {
        uint8_t val = (uint8_t)(i + 3);

        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                mp_put(mp, &i, &val));
    }

    for (uint32_t i = START_NUM; i < END_NUM; i += 2) {
        TEST_TRUE(mp_remove(mp, &i));
    }

    TEST_FALSE(mp_remove(mp, &END_NUM));
    TEST_FALSE(mp_remove(mp, NULL));

    for (uint32_t i = START_NUM; i < END_NUM; i++) {
        const uint32_t *key;
        uint8_t *val;

        err = mp_get_kvp(mp, &i, (const void **)&key, (void **)&val);

        if (i & 1) {
            TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
            TEST_EQUAL_UINT(i, *key);
            TEST_EQUAL_UINT((uint8_t)(i + 3), *val);
        } else {
            TEST_EQUAL_HEX(FOS_E_EMPTY, err);
        }
    }

    TEST_EQUAL_UINT(NUMS_TO_ADD / 2, mp_len(mp));

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_map_clear(void) {
    map_t *mp = gen_map(sizeof(uint32_t), sizeof(uint32_t));
    TEST_TRUE(mp != NULL);

    mp_clear(mp);

    TEST_EQUAL_UINT(0, mp_len(mp));

    for (uint32_t i = 0; i < 20; i++) {
        uint32_t v = i * 2;
        TEST_SUCCESS(mp_put(mp, &i, &v));
    }

    mp_clear(mp);
    TEST_EQUAL_UINT(0, mp_len(mp));

    for (uint32_t i = 0; i < 20; i++) {
        TEST_TRUE(mp_get(mp, &i) == NULL);
    }

    // Make sure adding after clearing works fine!
    for (uint32_t i = 0; i < 20; i++) {
        uint32_t v = i * 2;
        TEST_SUCCESS(mp_put(mp, &i, &v));
    }

    for (uint32_t i = 0; i < 20; i++) {
        uint32_t *v = mp_get(mp, &i);
        TEST_TRUE(v != NULL);
        TEST_EQUAL_UINT(i * 2, *v);
    }
    
    delete_map(mp);
    TEST_SUCCEED();
}


static bool test_map_simple_iter(void) {
    map_t *mp = gen_map(sizeof(uint16_t), sizeof(uint64_t));
    TEST_TRUE(mp != NULL);

    for (uint16_t i = 0; i < 32; i++) {
        uint64_t val = i * i;
        TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                mp_put(mp, &i, &val));
    }

    uint32_t found = 0;
    
    mp_reset_iter(mp);

    const uint16_t *key;
    uint64_t *val;

    fernos_error_t err;

    for (err = mp_get_iter(mp, (const void **)&key, (void **)&val);
            err == FOS_E_SUCCESS; err = mp_next_iter(mp, (const void **)&key, (void **)&val)) {
        
        uint64_t exp_val = (uint64_t)*key * (uint64_t)*key;
        TEST_EQUAL_UINT(exp_val, *val);

        found |= 1 << *key;
    }

    TEST_EQUAL_HEX(FOS_E_EMPTY, err);
    TEST_EQUAL_HEX(~(uint32_t)0, found);

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_map_put_null(void) {
    map_t *mp = gen_map(sizeof(uint32_t), sizeof(uint64_t));
    TEST_TRUE(mp != NULL);

    // Maps should be able to handle being given NULL as the value pointer.

    const uint32_t k = 10;
    TEST_SUCCESS(mp_put(mp, &k, NULL));

    const uint32_t *kp;
    uint64_t *vp;
    TEST_SUCCESS(mp_get_kvp(mp, &k, (const void **)&kp, (void **)&vp));

    TEST_EQUAL_UINT(k, *kp);

    TEST_TRUE(vp != NULL);
    *vp = 13; // Writing to the value pointer should work!
    vp = NULL; // we are going to modify the map, we will assume the kvp's address changes.

    // Now let's overwrite twice, once with a real value, and then again with NULL!
    const uint32_t t = 12;
    uint64_t val = 100;
    TEST_SUCCESS(mp_put(mp, &t, &val));
    TEST_SUCCESS(mp_put(mp, &t, NULL));

    TEST_SUCCESS(mp_get_kvp(mp, &t, NULL, (void **)&vp));
    TEST_EQUAL_UINT(val, *vp);

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_map_complex0(void) {
    // Keys will be strings with 4 significant characters.
    map_t *mp = gen_map(sizeof(char) * 5, sizeof(uint64_t));
    TEST_TRUE(mp != NULL);

    for (char c0 = 'a'; c0 <= 'z'; c0++) {
        for (char c1 = 'a'; c1 <= 'z'; c1++) {
            char id[5] = {
                c0, c1, c0, c1, '\0'
            };

            uint64_t val = c0 + c1;

            TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                    mp_put(mp, id, &val));
        }
    }

    // maybe remove every entry which starts with 'c'?
    // Double every entry which starts with 'k'?

    for (char c1 = 'a'; c1 <= 'z'; c1++) {
        char rem_id[5] = {
            'c', c1, 'c', c1, '\0'
        };

        TEST_TRUE(mp_remove(mp, rem_id));

        char update_id[5] = {
            'k', c1, 'k', c1, '\0'
        };

        uint64_t *val = mp_get(mp, update_id);
        TEST_TRUE(val != NULL);
        *val *= 2;
    }

    TEST_EQUAL_UINT((26 * 26) - 26, mp_len(mp));

    uint32_t iters = 0;

    const char *key;
    uint64_t *val;
    fernos_error_t err;

    mp_reset_iter(mp);

    for (err = mp_get_iter(mp, (const void **)&key, (void **)&val);
            err == FOS_E_SUCCESS; err = mp_next_iter(mp, (const void **)&key, (void **)&val)) {

        TEST_TRUE(key[0] != 'c');

        uint64_t exp_val = key[0] + key[1];
        if (key[0] == 'k') {
            exp_val *= 2;
        }

        TEST_EQUAL_UINT(exp_val, *val);
        
        iters++;
    }

    TEST_EQUAL_HEX(FOS_E_EMPTY, err);
    TEST_EQUAL_UINT((26 * 26) - 26, iters);

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_map_complex1(void) {
    /*
     * Maybe for this one, add again after removing??
     */

    map_t *mp = gen_map(sizeof(uint64_t), sizeof(uint64_t));
    TEST_TRUE(mp != NULL);

    for (uint32_t i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < 10; j++) {
            uint64_t key = (10 * i) + j; 
            uint64_t val = key * 2;

            TEST_EQUAL_HEX(FOS_E_SUCCESS, 
                    mp_put(mp, &key, &val));
        }

        // Remove odds.
        for (uint32_t j = 1; j < 10; j += 2) {
            uint64_t key = (10 * i) + j; 
            TEST_TRUE(mp_remove(mp, &key)); 
        }

        // Halve evens.
        for (uint32_t j = 0; j < 10; j += 2) {
            uint64_t key = (10 * i) + j; 

            uint64_t *val = mp_get(mp, &key);

            TEST_TRUE(val != NULL);
            TEST_EQUAL_UINT(key * 2, *val);

            *val /= 2;
        }
    }

    // Now iterate over everything to see things still make sense?

    mp_reset_iter(mp);

    const uint64_t *key_ptr;
    uint64_t *val_ptr;

    fernos_error_t err;

    uint32_t iters = 0;

    for (err = mp_get_iter(mp, (const void **)&key_ptr, (void **)&val_ptr); 
            err == FOS_E_SUCCESS; err = mp_next_iter(mp, (const void **)&key_ptr, (void **)&val_ptr)) {
        
        TEST_EQUAL_UINT(*key_ptr, *val_ptr);
        iters++;
    }

    TEST_EQUAL_UINT(50, iters);

    delete_map(mp);

    TEST_SUCCEED();
}

static bool test_set(void) {
    // A map with value size 0 is essentially a set.
    // It should still work fine, with certain functions returns special errors.
    
    fernos_error_t err;

    map_t *mp = gen_map(sizeof(uint32_t), 0);
    TEST_TRUE(mp != NULL);

    uint32_t i;

    i = 0;
    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, mp_put(mp, &i, &i));

    for (i = 0; i < 10; i++) {
        TEST_SUCCESS(mp_put(mp, &i, NULL));
    }

    mp_reset_iter(mp);
    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, mp_get_iter(mp, NULL, (void **)&i));
    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, mp_next_iter(mp, NULL, (void **)&i));

    mp_reset_iter(mp);
    uint32_t found = 0;
    const uint32_t *k;
    for (err = mp_get_iter(mp, (const void **)&k, NULL); err == FOS_E_SUCCESS;
            err = mp_next_iter(mp, (const void **)&k, NULL)) {
        uint32_t key = *k;
        TEST_TRUE(key < 10);
        TEST_TRUE((found & (1U << key)) == 0);
        found |= (1U << key);
    }

    TEST_EQUAL_HEX(FOS_E_EMPTY, err);
    TEST_EQUAL_UINT(10, mp_len(mp));

    delete_map(mp);

    TEST_SUCCEED();
}

bool test_map(const char *name, map_t *(*gen)(size_t ks, size_t vs)) {
    gen_map = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_map_simple_put_and_get);
    RUN_TEST(test_map_simple_overwrite);
    RUN_TEST(test_map_simple_remove);
    RUN_TEST(test_map_clear);
    RUN_TEST(test_map_simple_iter);
    RUN_TEST(test_map_put_null);
    RUN_TEST(test_map_complex0);
    RUN_TEST(test_map_complex1);
    RUN_TEST(test_set);
    return END_SUITE();
}

/**
 * This is kinda a retro fit. Old hash functions used to actually take a pointer to the map too.
 * This way a single hash function could deduce the key size, and thus be usable for many different
 * key types. 
 *
 * I went away from this design though, so now I am just storing the key size here so that
 * I don't need to rewrite some of the above testing code.
 */
static uint32_t key_size;

static uint32_t simple_hash(const void *key) {
    uint32_t hash = 0;

    const uint8_t *key_arr = (const uint8_t *)key;
    for (size_t i = 0; i < key_size; i++) {
        hash += key_arr[i] * (i + 7);
    }

    return hash;
}

static bool simple_eq(const void *k0, const void *k1) {
    return mem_cmp(k0, k1, key_size);
}

static map_t *chained_hash_map_gen(size_t ks, size_t vs) {
    key_size = ks;

    return new_chained_hash_map(get_default_allocator(), ks, vs, 3, 
            simple_eq, simple_hash);
}

bool test_chained_hash_map(void) {
    return test_map("Chained Hash Map", chained_hash_map_gen); 
}
