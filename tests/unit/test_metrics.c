/* Software-only: latency percentiles, summary, and histogram. */
#include "../test_util.h"
#include "metrics.h"

static void test_summary(void)
{
    int64_t s[100];
    for (int i = 0; i < 100; i++)
        s[i] = 100 - i; /* descending: summarize must sort a copy */
    latency_summary sum;
    CHECK(latency_summarize(s, 100, &sum) == 0);
    CHECK(sum.n == 100);
    CHECK(sum.min == 1 && sum.max == 100);
    CHECK(sum.p50 == 50 && sum.p90 == 90 && sum.p99 == 99);
    CHECK_NEAR(sum.mean, 50.5, 1e-9);
    CHECK(s[0] == 100 && s[99] == 1); /* input untouched */
}

static void test_small_inputs(void)
{
    int64_t one = 42;
    latency_summary sum;
    CHECK(latency_summarize(&one, 1, &sum) == 0);
    CHECK(sum.min == 42 && sum.p50 == 42 && sum.p99 == 42 && sum.max == 42);
    CHECK(latency_summarize(&one, 0, &sum) == -1);

    int64_t sorted[] = {10, 20, 30};
    CHECK(latency_percentile_sorted(sorted, 3, 0) == 10);
    CHECK(latency_percentile_sorted(sorted, 3, 34) == 20);
    CHECK(latency_percentile_sorted(sorted, 3, 100) == 30);
    CHECK(latency_percentile_sorted(sorted, 3, 250) == 30);
}

static void test_histogram(void)
{
    int64_t s[] = {0, 5, 10, 19, 25, 35, 1000, -3};
    uint64_t counts[4] = {99, 99, 99, 99};
    CHECK(latency_histogram(s, 8, 10, counts, 4) == 1);
    CHECK(counts[0] == 2); /* 0, 5 */
    CHECK(counts[1] == 2); /* 10, 19 */
    CHECK(counts[2] == 1); /* 25 */
    CHECK(counts[3] == 2); /* 35 and overflow 1000 */
}

int main(void)
{
    test_summary();
    test_small_inputs();
    test_histogram();
    TEST_DONE("test_metrics");
}
