/* Pairs ground-truth stimulus edges with the events the pipeline stored, to
 * measure edge-to-handler latency for the interrupt vs. polling comparison. */
#ifndef LATENCY_MATCH_H
#define LATENCY_MATCH_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t t_ns;  /* CLOCK_MONOTONIC just before the stimulus write */
    int      level; /* level driven: 1 = rising, 0 = falling */
} stim_edge;

typedef struct {
    uint64_t handled_ns; /* CLOCK_MONOTONIC when the pipeline handled the event */
    int      level;      /* 1 = motion_start (rising), 0 = motion_end (falling) */
} recv_event;

typedef struct {
    size_t matched;  /* stimulus edges with a matching event */
    size_t missed;   /* stimulus edges with no event before the next stimulus */
    size_t spurious; /* events that matched no stimulus edge */
    size_t early;    /* events before the first stimulus edge (setup; ignored) */
} match_result;

/* Both arrays must be in time order. For each stimulus edge, takes the next
 * event with the same level that was handled at or after the edge and before
 * the next stimulus edge. Latency (handled - stimulus) is written to
 * latency_ns[k] for the k-th stimulus edge, or -1 if missed; latency_ns needs
 * n_stim entries. If match_idx is not NULL, match_idx[k] receives the index of
 * the matching event, or SIZE_MAX. */
void latency_match(const stim_edge *stim, size_t n_stim, const recv_event *ev, size_t n_ev,
                   int64_t *latency_ns, size_t *match_idx, match_result *res);

#endif
