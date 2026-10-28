/* Software-only: pairing stimulus edges with stored events. */
#include "../test_util.h"
#include "latency_match.h"

static void test_all_matched(void)
{
    stim_edge s[] = {{100, 1}, {200, 0}, {300, 1}};
    recv_event e[] = {{50, 0}, {110, 1}, {205, 0}, {330, 1}};
    int64_t lat[3];
    match_result m;
    size_t idx[3];
    latency_match(s, 3, e, 4, lat, idx, &m);
    CHECK(m.matched == 3 && m.missed == 0 && m.spurious == 0 && m.early == 1);
    CHECK(lat[0] == 10 && lat[1] == 5 && lat[2] == 30);
    CHECK(idx[0] == 1 && idx[1] == 2 && idx[2] == 3);
}

static void test_missed_and_spurious(void)
{
    /* Edge at 200 never arrives; an extra rising event shows up at 250. A
     * response that lands after the next stimulus counts as missed. */
    stim_edge s[] = {{100, 1}, {200, 0}, {300, 1}, {400, 0}};
    recv_event e[] = {{150, 1}, {250, 1}, {410, 1}, {450, 0}, {999, 1}};
    int64_t lat[4];
    match_result m;
    size_t idx[4];
    latency_match(s, 4, e, 5, lat, idx, &m);
    CHECK(lat[0] == 50);
    CHECK(lat[1] == -1 && idx[1] == SIZE_MAX);
    CHECK(lat[2] == -1); /* the 410 event lies in the next window */
    CHECK(lat[3] == 50);
    CHECK(m.matched == 2 && m.missed == 2);
    CHECK(m.spurious == 3); /* 250 (wrong level), 410 (wrong level), 999 (trailing) */
}

static void test_empty(void)
{
    int64_t lat[1];
    match_result m;
    stim_edge s[] = {{100, 1}};
    latency_match(s, 1, NULL, 0, lat, NULL, &m);
    CHECK(m.missed == 1 && lat[0] == -1);
    recv_event e[] = {{5, 1}};
    latency_match(NULL, 0, e, 1, lat, NULL, &m);
    CHECK(m.spurious == 1 && m.matched == 0);
}

int main(void)
{
    test_all_matched();
    test_missed_and_spurious();
    test_empty();
    TEST_DONE("test_latency_match");
}
