
#include "s_data/test/id_table.h"
#include "s_data/id_table.h"

#include "k_bios_term/term.h"

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

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

bool test_id_table(void) {
    BEGIN_SUITE("ID Table");
    RUN_TEST(test_new_id_table);
    RUN_TEST(test_id_table_simple);
    RUN_TEST(test_id_table0);
    RUN_TEST(test_id_table1);
    return END_SUITE();
}
