/* Latency distribution statistics for the interrupt vs. polling comparison. */
#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t  n;
    int64_t min, p50, p90, p99, max;
    double  mean;
} latency_summary;

/* Nearest-rank percentile (pct 0..100) of an ascending-sorted array, n > 0. */
int64_t latency_percentile_sorted(const int64_t *sorted, size_t n, unsigned pct);

/* Summarizes samples without modifying them. Returns 0, or -1 if n == 0 or
 * allocation fails. */
int latency_summarize(const int64_t *samples, size_t n, latency_summary *out);

/* Fixed-width histogram: counts[i] holds samples in [i*w, (i+1)*w) and the last
 * bin also holds everything above. counts must have nbins >= 1 entries and
 * bin_width_ns must be > 0. Negative samples are not counted; returns how many
 * there were. */
size_t latency_histogram(const int64_t *samples, size_t n, int64_t bin_width_ns,
                         uint64_t *counts, size_t nbins);

#endif
