/* Time source for the pipeline. The real clock uses CLOCK_MONOTONIC (the same
 * clock the kernel uses for GPIO edge timestamps); tests use the mock
 * backend's virtual clock so runs are deterministic. */
#ifndef MONO_CLOCK_H
#define MONO_CLOCK_H

#include <stdint.h>
#include <time.h>

typedef struct {
    uint64_t (*now)(void *ctx);
    /* Sleep until absolute monotonic time t_ns. Returns 0, or EINTR if a
     * signal interrupted the sleep. */
    int (*sleep_until)(void *ctx, uint64_t t_ns);
    /* Wall-clock time, for human-readable timestamps in storage. */
    void (*wall)(void *ctx, struct timespec *ts);
    void *ctx;
} mono_clock;

mono_clock mono_clock_real(void);

#endif
