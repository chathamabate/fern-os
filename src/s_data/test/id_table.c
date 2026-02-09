
#include "s_data/test/id_table.h"
#include "s_data/id_table.h"

#include "k_startup/gfx.h"
#include "s_mem/allocator.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

#include "s_util/test.h"

static size_t _num_al_blocks;

static bool pretest(void) {
    _num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(_num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

static bool test_new_id_table(void) {
    id_table_t *idtb = new_da_id_table(25);
    TEST_TRUE(idtb != NULL);

    delete_id_table(idtb);

    TEST_SUCCEED();
}

static bool test_id_table_simple(void) {
    id_table_t *idtb = new_da_id_table(15);
    TEST_TRUE(idtb != NULL);

    const id_t NULL_ID = idtb_null_id(idtb);

    // Allocate 2 IDs.
    id_t id0 = idtb_pop_id(idtb);
    id_t id1 = idtb_pop_id(idtb);

    TEST_TRUE(id0 != NULL_ID && id1 != NULL_ID && id0 != id1);

    // Free one, then reallocate it.
    idtb_push_id(idtb, id0);
    id0 = idtb_pop_id(idtb);

    TEST_TRUE(id0 != NULL_ID && id0 != id1);

    // Free both.
    idtb_push_id(idtb, id0);
    idtb_push_id(idtb, id1);

    // Allocate once more!
    id0 = idtb_pop_id(idtb);
    TEST_TRUE(id0 != NULL_ID);


    delete_id_table(idtb);

    TEST_SUCCEED();
}

#define TEST_ID_TABLE0_MC 0x50U
static bool test_id_table0(void) {
    id_table_t *idtb = new_da_id_table(TEST_ID_TABLE0_MC);
    TEST_TRUE(idtb != NULL);

    const id_t NULL_ID = idtb_null_id(idtb);

    id_t tbl[TEST_ID_TABLE0_MC];
    for (uint32_t i = 0; i < TEST_ID_TABLE0_MC; i++) {
        tbl[i] = NULL_ID;
    }

    for (uint32_t i = 0; i < TEST_ID_TABLE0_MC; i++) {
        id_t id = idtb_pop_id(idtb);
        TEST_TRUE(id != NULL_ID);

        idtb_set(idtb, id, (void *)i);

        TEST_EQUAL_HEX(tbl[i], NULL_ID);
        tbl[i] = id;
    }

    TEST_EQUAL_HEX(NULL_ID, idtb_pop_id(idtb));

    for (uint32_t i = 0; i < TEST_ID_TABLE0_MC; i++) {
        TEST_EQUAL_HEX((void *)i, idtb_get(idtb, tbl[i]));
    }

    for (uint32_t i = 0; i < TEST_ID_TABLE0_MC; i++) {
        idtb_push_id(idtb, tbl[i]);
        tbl[i] = NULL_ID;
    }

    delete_id_table(idtb);

    TEST_SUCCEED();
}

#define TEST_ID_TABLE1_MC 0x50U
static bool test_id_table1(void) {
    id_table_t *idtb = new_da_id_table(TEST_ID_TABLE1_MC);
    TEST_TRUE(idtb != NULL);

    const id_t NULL_ID = idtb_null_id(idtb);

    id_t tbl[TEST_ID_TABLE1_MC];
    for (uint32_t i = 0; i < TEST_ID_TABLE1_MC; i++) {
        tbl[i] = NULL_ID;
    }

    const uint32_t FIRST_BATCH = 20;
    TEST_TRUE(FIRST_BATCH <= TEST_ID_TABLE1_MC);

    for (uint32_t i = 0; i < FIRST_BATCH; i++) {
        id_t id = idtb_pop_id(idtb); 
        TEST_TRUE(id != NULL_ID);

        idtb_set(idtb, id, (void *)i);

        TEST_EQUAL_HEX(NULL_ID, tbl[i]);
        tbl[i] = id;
    }

    // Free every other in the first batch.
    for (uint32_t i = 0; i < FIRST_BATCH; i += 2) {
        id_t id = tbl[i];
        idtb_push_id(idtb, id);
        tbl[i] = NULL_ID;
    }

    // Confirm values aren't affected.
    for (uint32_t i = 1; i < FIRST_BATCH; i += 2) {
        id_t id = tbl[i];
        TEST_EQUAL_HEX((void *)i, idtb_get(idtb, id));
    }

    // Allocate some more. (Hopefully causing some resizing)
    for (uint32_t i = FIRST_BATCH; i < TEST_ID_TABLE1_MC; i++) {
        id_t id = idtb_pop_id(idtb); 
        TEST_TRUE(id != NULL_ID);

        idtb_set(idtb, id, (void *)i);

        TEST_EQUAL_HEX(NULL_ID, tbl[i]);
        tbl[i] = id;
    }

    // Confirm values.
    for (uint32_t i = 0; i < TEST_ID_TABLE1_MC; i++) {
        id_t id = tbl[i]; 
        if (id != NULL_ID) {
            TEST_EQUAL_HEX((void *)i, idtb_get(idtb, id));
        }
    }

    delete_id_table(idtb);

    TEST_SUCCEED();
}

#define TEST_ID_TABLE_REQ_MC 0x10U
static bool test_id_table_request0(void) {
    id_table_t *idtb = new_da_id_table(TEST_ID_TABLE_REQ_MC);
    TEST_TRUE(idtb != NULL);

    const id_t NULL_ID = idtb_null_id(idtb);

    fernos_error_t err;

    for (id_t id = 0; id < TEST_ID_TABLE_REQ_MC; id += 2) {
        err = idtb_request_id(idtb, id);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
        
        idtb_set(idtb, id, (void *)id);
    }

    err = idtb_request_id(idtb, TEST_ID_TABLE_REQ_MC);
    TEST_TRUE(err != FOS_E_SUCCESS);

    err = idtb_request_id(idtb, TEST_ID_TABLE_REQ_MC + 10);
    TEST_TRUE(err != FOS_E_SUCCESS);

    err = idtb_request_id(idtb, 4);
    TEST_TRUE(err != FOS_E_SUCCESS);

    // Should be half of the ids left.
    for (uint32_t i = 0; i < TEST_ID_TABLE_REQ_MC / 2; i++) {
        id_t id = idtb_pop_id(idtb);
        TEST_TRUE(id != NULL_ID);
    }

    err = idtb_request_id(idtb, 3);
    TEST_TRUE(err != FOS_E_SUCCESS);

    for (id_t id = 0; id < TEST_ID_TABLE_REQ_MC; id += 2) {
        void *data = idtb_get(idtb, id);
        TEST_EQUAL_HEX(data, (void *)id);
    }

    // Now let's put them all back for a second.
    for (id_t id = 0; id < TEST_ID_TABLE_REQ_MC; id++) {
        idtb_push_id(idtb, id);
    }

    delete_id_table(idtb);

    TEST_SUCCEED();
}

static bool test_id_table_request1(void) {
    id_table_t *idtb = new_da_id_table(TEST_ID_TABLE_REQ_MC);
    TEST_TRUE(idtb != NULL);

    const id_t NULL_ID = idtb_null_id(idtb);

    fernos_error_t err;
    err = idtb_request_id(idtb, TEST_ID_TABLE_REQ_MC - 1);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    idtb_reset_iterator(idtb);
    size_t iters = 0;
    for (id_t id = idtb_get_iter(idtb); id != NULL_ID; id = idtb_next(idtb)) {
        TEST_EQUAL_UINT(TEST_ID_TABLE_REQ_MC - 1, id);
        iters++;
    }

    TEST_EQUAL_UINT(1, iters);

    delete_id_table(idtb);

    TEST_SUCCEED();
}

static bool test_id_table_exhaustion(void) {
    // Here we assume that the heap doesn have 2Gs of space.
    id_table_t *idtb = new_da_id_table(M_2G);
    TEST_TRUE(idtb != NULL);

    // Just want to see what happens when we push it to the limit.

    const id_t NULL_ID = idtb_null_id(idtb);

    id_t id0 = idtb_pop_id(idtb);
    TEST_TRUE(id0 != NULL_ID);

    id_t id;
    while ((id = idtb_pop_id(idtb)) != NULL_ID);

    // Now just try using the table after the fact.
    idtb_push_id(idtb, id0);
    id0 = idtb_pop_id(idtb);
    TEST_TRUE(id0 != NULL_ID);

    delete_id_table(idtb);

    TEST_SUCCEED();
}

#define TEST_ID_TABLE_ITER_MC 20
static bool test_id_table_iter(void) {
    id_table_t *idtb = new_da_id_table(TEST_ID_TABLE_ITER_MC);
    TEST_TRUE(idtb != NULL);

    const id_t NULL_ID = idtb_null_id(idtb);

    // None.
    idtb_reset_iterator(idtb);
    TEST_EQUAL_HEX(NULL_ID, idtb_next(idtb));

    // 1 element.
    id_t id0 = idtb_pop_id(idtb);
    TEST_TRUE(id0 != NULL_ID);

    idtb_reset_iterator(idtb);
    TEST_EQUAL_HEX(id0, idtb_get_iter(idtb));
    TEST_EQUAL_HEX(NULL_ID, idtb_next(idtb));

    idtb_push_id(idtb, id0);

    // Multiple elements.
    bool visited[TEST_ID_TABLE_ITER_MC];
    for (uint32_t i = 0; i < TEST_ID_TABLE_ITER_MC; i++) {
        visited[i] = false;
    }
    
    // Arbitrary amount here.
    for (uint32_t i = 0; i < 5; i++) {
        id_t id = idtb_pop_id(idtb); 
        TEST_TRUE(id != NULL_ID);

        visited[id] = true;
    }

    id_t iter;
    idtb_reset_iterator(idtb);
    for (iter = idtb_get_iter(idtb); iter != NULL_ID; iter = idtb_next(idtb)) {
        TEST_TRUE(visited[iter]);
        visited[iter] = false;
    }

    for (uint32_t i = 0; i < TEST_ID_TABLE_ITER_MC; i++) {
        TEST_FALSE(visited[i]);
    }

    delete_id_table(idtb);

    TEST_SUCCEED();
}

bool test_id_table(void) {
    BEGIN_SUITE("ID Table");
    RUN_TEST(test_new_id_table);
    RUN_TEST(test_id_table_simple);
    RUN_TEST(test_id_table0);
    RUN_TEST(test_id_table1);
    RUN_TEST(test_id_table_request0);
    RUN_TEST(test_id_table_request1);
    RUN_TEST(test_id_table_exhaustion);
    RUN_TEST(test_id_table_iter);
    return END_SUITE();
}
