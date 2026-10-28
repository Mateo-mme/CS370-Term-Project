#include "mono_clock.h"

#include <errno.h>

static uint64_t real_now(void *ctx)
{
    (void)ctx;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int real_sleep_until(void *ctx, uint64_t t_ns)
{
    (void)ctx;
    struct timespec ts = {
        .tv_sec = (time_t)(t_ns / 1000000000ULL),
        .tv_nsec = (long)(t_ns % 1000000000ULL),
    };
    int r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    return r == EINTR ? EINTR : 0;
}

static void real_wall(void *ctx, struct timespec *ts)
{
    (void)ctx;
    clock_gettime(CLOCK_REALTIME, ts);
}

mono_clock mono_clock_real(void)
{
    return (mono_clock){
        .now = real_now,
        .sleep_until = real_sleep_until,
        .wall = real_wall,
        .ctx = NULL,
    };
}
