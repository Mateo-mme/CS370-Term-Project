/* PIR motion state machine with an optional stability debounce.
 * A level change is committed only after it holds for debounce_ns; the event
 * keeps the timestamp of the original edge so latency measurements are not
 * inflated by the debounce window. */
#ifndef PIR_H
#define PIR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { PIR_NONE, PIR_MOTION_START, PIR_MOTION_END } pir_event;

typedef struct {
    uint64_t debounce_ns;
    int      level;         /* committed level: 1 = motion */
    bool     pending;
    int      pending_level;
    uint64_t pending_ns;    /* edge time of the pending change */
} pir_state;

void pir_init(pir_state *s, uint64_t debounce_ns, int initial_level);

/* Report the line level observed at ts_ns (from an edge or a poll sample).
 * Returns a committed event, with its edge time in *event_ns. */
pir_event pir_on_level(pir_state *s, int level, uint64_t ts_ns, uint64_t *event_ns);

/* Commit a pending change that has been stable for debounce_ns by now_ns. */
pir_event pir_tick(pir_state *s, uint64_t now_ns, uint64_t *event_ns);

/* If a change is pending, stores when pir_tick should next run and returns true. */
bool pir_deadline(const pir_state *s, uint64_t *deadline_ns);

#endif
