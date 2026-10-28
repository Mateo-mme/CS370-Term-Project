/* A sensor event as it flows through the pipeline into storage. */
#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <time.h>

/* EV_THREAT: motion plus an in-range echo closer than the alert distance,
 * stored at most once per motion episode. */
typedef enum { EV_MOTION_START, EV_MOTION_END, EV_DISTANCE, EV_THREAT } event_kind;

typedef struct {
    uint64_t        t_mono_ns;   /* CLOCK_MONOTONIC time of the underlying edge */
    struct timespec t_wall;      /* CLOCK_REALTIME when the event was handled */
    const char     *mode;        /* "irq" or "poll" */
    event_kind      kind;
    double          distance_cm; /* EV_DISTANCE/EV_THREAT; negative = no echo (timeout) */
    int             echo_ok;     /* EV_DISTANCE/EV_THREAT: 1 if reading in range */
    int64_t         latency_ns;  /* handled - edge time; negative if unknown */
} event_record;

#endif
