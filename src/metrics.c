#include "metrics.h"

#include <stdlib.h>
#include <string.h>

static int cmp_i64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

int64_t latency_percentile_sorted(const int64_t *sorted, size_t n, unsigned pct)
{
    if (pct > 100)
        pct = 100;
    size_t rank = ((size_t)pct * n + 99) / 100; /* ceil(pct/100 * n) */
    if (rank < 1)
        rank = 1;
    return sorted[rank - 1];
}

int latency_summarize(const int64_t *samples, size_t n, latency_summary *out)
{
    if (n == 0)
        return -1;
    int64_t *sorted = malloc(n * sizeof *sorted);
    if (!sorted)
        return -1;
    memcpy(sorted, samples, n * sizeof *sorted);
    qsort(sorted, n, sizeof *sorted, cmp_i64);

    double sum = 0;
    for (size_t i = 0; i < n; i++)
        sum += (double)sorted[i];

    *out = (latency_summary){
        .n = n,
        .min = sorted[0],
        .p50 = latency_percentile_sorted(sorted, n, 50),
        .p90 = latency_percentile_sorted(sorted, n, 90),
        .p99 = latency_percentile_sorted(sorted, n, 99),
        .max = sorted[n - 1],
        .mean = sum / (double)n,
    };
    free(sorted);
    return 0;
}

size_t latency_histogram(const int64_t *samples, size_t n, int64_t bin_width_ns,
                         uint64_t *counts, size_t nbins)
{
    size_t negative = 0;
    memset(counts, 0, nbins * sizeof *counts);
    for (size_t i = 0; i < n; i++) {
        if (samples[i] < 0) {
            negative++;
            continue;
        }
        size_t bin = (size_t)(samples[i] / bin_width_ns);
        counts[bin < nbins ? bin : nbins - 1]++;
    }
    return negative;
}
