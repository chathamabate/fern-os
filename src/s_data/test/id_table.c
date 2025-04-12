
#include "s_data/test/id_table.h"
#include "s_data/id_table.h"

#include "k_bios_term/term.h"

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static bool test_new_id_table(void) {
    id_table_t *idtb = new_da_id_table(25);
    TEST_TRUE(idtb != NULL);

    id_t id = idtb_pop_id(idtb);
    id = idtb_pop_id(idtb);

    idtb_dump(idtb, term_put_fmt_s);

    delete_id_table(idtb);

    TEST_SUCCEED();
}


bool test_id_table(void) {
    BEGIN_SUITE("ID Table");
    RUN_TEST(test_new_id_table);
    return END_SUITE();
}
