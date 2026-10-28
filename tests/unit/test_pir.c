/* Software-only: PIR motion state machine and debounce. */
#include "../test_util.h"
#include "pir.h"

#define MS 1000000ULL

static void test_no_debounce(void)
{
    pir_state s;
    uint64_t t = 0;
    pir_init(&s, 0, 0);
    CHECK(pir_on_level(&s, 1, 100, &t) == PIR_MOTION_START);
    CHECK(t == 100);
    CHECK(pir_on_level(&s, 1, 200, &t) == PIR_NONE); /* repeated level, e.g. poll sample */
    CHECK(pir_on_level(&s, 0, 300, &t) == PIR_MOTION_END);
    CHECK(t == 300);
    CHECK(!pir_deadline(&s, &t));
}

static void test_debounce_commits_with_edge_time(void)
{
    pir_state s;
    uint64_t t = 0, deadline = 0;
    pir_init(&s, 5 * MS, 0);
    CHECK(pir_on_level(&s, 1, 1 * MS, &t) == PIR_NONE);
    CHECK(pir_deadline(&s, &deadline));
    CHECK(deadline == 6 * MS);
    CHECK(pir_tick(&s, 5 * MS, &t) == PIR_NONE);
    CHECK(pir_tick(&s, 6 * MS, &t) == PIR_MOTION_START);
    CHECK(t == 1 * MS); /* edge time, not commit time */
    CHECK(s.level == 1);
}

static void test_glitch_rejected(void)
{
    pir_state s;
    uint64_t t = 0;
    pir_init(&s, 5 * MS, 0);
    CHECK(pir_on_level(&s, 1, 0, &t) == PIR_NONE);
    CHECK(pir_on_level(&s, 0, 1 * MS, &t) == PIR_NONE);
    CHECK(pir_tick(&s, 20 * MS, &t) == PIR_NONE);
    CHECK(s.level == 0);
    CHECK(!pir_deadline(&s, &t));
}

static void test_stable_change_committed_by_next_edge(void)
{
    /* No tick between edges: the next edge must still commit the stable one. */
    pir_state s;
    uint64_t t = 0;
    pir_init(&s, 5 * MS, 0);
    CHECK(pir_on_level(&s, 1, 0, &t) == PIR_NONE);
    CHECK(pir_on_level(&s, 0, 10 * MS, &t) == PIR_MOTION_START);
    CHECK(t == 0);
    CHECK(pir_tick(&s, 15 * MS, &t) == PIR_MOTION_END);
    CHECK(t == 10 * MS);
}

static void test_initial_level(void)
{
    pir_state s;
    uint64_t t = 0;
    pir_init(&s, 0, 1);
    CHECK(pir_on_level(&s, 1, 0, &t) == PIR_NONE);
    CHECK(pir_on_level(&s, 0, 1, &t) == PIR_MOTION_END);
}

int main(void)
{
    test_no_debounce();
    test_debounce_commits_with_edge_time();
    test_glitch_rejected();
    test_stable_change_committed_by_next_edge();
    test_initial_level();
    TEST_DONE("test_pir");
}
