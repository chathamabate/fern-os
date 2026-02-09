
#include "s_data/test/ring.h"
#include "s_data/ring.h"

#include "k_startup/gfx.h"

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

#include "s_util/test.h"

static bool test_ring_basic(void) {
    // We will just attach/detach a few elements.
    ring_t ring;
    init_ring(&ring);

    ring_element_t eles[4];
    const size_t num_eles = sizeof(eles) / sizeof(eles[0]);

    for (size_t i = 0; i < num_eles; i++) {
        init_ring_element(eles + i);
    }

    ring_element_attach(eles + 0, &ring);

    TEST_EQUAL_HEX(eles + 0, ring.head);
    TEST_EQUAL_UINT(1, ring.len);
    
    ring_advance(&ring);

    TEST_EQUAL_HEX(eles + 0, ring.head);

    ring_element_attach(eles + 1, &ring);
    ring_element_attach(eles + 2, &ring);

    ring_advance(&ring);
    TEST_EQUAL_HEX(eles + 1, ring.head);

    ring_advance(&ring);
    TEST_EQUAL_HEX(eles + 2, ring.head);

    ring_advance(&ring);
    TEST_EQUAL_HEX(eles + 0, ring.head);

    TEST_EQUAL_UINT(3, ring.len);

    // Now start the detach!

    ring_element_detach(eles + 0);
    TEST_EQUAL_HEX(eles + 1, ring.head);

    TEST_EQUAL_UINT(2, ring.len);

    ring_element_detach(eles + 2);
    TEST_EQUAL_HEX(eles + 1, ring.head);

    ring_advance(&ring);
    TEST_EQUAL_HEX(eles + 1, ring.head);

    ring_element_detach(eles + 1);

    TEST_EQUAL_UINT(0, ring.len);

    TEST_SUCCEED();
}

static bool test_ring_complex(void) {
    ring_t rings[2];
    init_ring(rings + 0);
    init_ring(rings + 1);

    ring_element_t eles[4];
    const size_t num_eles = sizeof(eles) / sizeof(eles[0]);
    for (size_t i = 0; i < num_eles; i++) {
        init_ring_element(eles + i);
    }

    ring_element_attach(eles + 0, rings + 0);
    ring_element_attach(eles + 1, rings + 0);
    ring_element_attach(eles + 2, rings + 0);

    ring_element_attach(eles + 3, rings + 1);

    // This should remove element 0 from ring 0 before adding to ring 1.
    ring_element_attach(eles + 0, rings + 1);
    
    TEST_EQUAL_HEX(eles + 1, (rings + 0)->head);

    TEST_EQUAL_HEX(eles + 3, (rings + 1)->head);
    ring_advance(rings + 1);

    TEST_EQUAL_HEX(eles + 0, (rings + 1)->head);

    ring_element_attach(eles + 2, rings + 1);

    // At this point:
    // ring0: [1]
    // ring1: [0, 3, 2]

    TEST_EQUAL_UINT(1, (rings + 0)->len);
    TEST_EQUAL_UINT(3, (rings + 1)->len);

    ring_advance(rings + 1);
    TEST_EQUAL_HEX(eles + 3, (rings + 1)->head);

    // This should detach then attach all on ring 1.
    ring_element_attach(eles + 2, rings + 1);
    // ring1: [3, 0, 2]
    TEST_EQUAL_HEX(eles + 3, (rings + 1)->head);
    ring_advance(rings + 1);
    TEST_EQUAL_HEX(eles + 0, (rings + 1)->head);

    TEST_SUCCEED();
}

bool test_ring(void) {
    BEGIN_SUITE("Ring");
    RUN_TEST(test_ring_basic);
    RUN_TEST(test_ring_complex);
    return END_SUITE();
}
