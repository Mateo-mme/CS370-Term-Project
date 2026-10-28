/* Ground-truth stimulus for the latency benchmark. Toggles an output line
 * (GPIO5, jumpered to GPIO6) at random intervals, logging CLOCK_MONOTONIC
 * just before and just after each write. It runs as a separate process from
 * the receiver (crop_guard --pir-pin=6) so the stimulus does not share the
 * receiver's event loop. CLOCK_MONOTONIC is system-wide, so timestamps from
 * the two processes are comparable. */
#include "gpio_backend.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t mono_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void sleep_until_ns(uint64_t t)
{
    struct timespec ts = {.tv_sec = (time_t)(t / 1000000000ULL),
                          .tv_nsec = (long)(t % 1000000000ULL)};
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR)
        ;
}

/* xorshift64*: small, deterministic for a given seed. */
static uint64_t rng_next(uint64_t *s)
{
    *s ^= *s >> 12;
    *s ^= *s << 25;
    *s ^= *s >> 27;
    return *s * 2685821657736338717ULL;
}

static bool opt_u64(const char *arg, const char *name, uint64_t *out)
{
    size_t n = strlen(name);
    if (strncmp(arg, name, n) != 0 || arg[n] != '=')
        return false;
    *out = strtoull(arg + n + 1, NULL, 10);
    return true;
}

int main(int argc, char **argv)
{
    const char *chip = "/dev/gpiochip0", *out = NULL;
    uint64_t pin = 5, count = 1000, min_ms = 20, max_ms = 50, seed = 1, delay_ms = 500;
    uint64_t notify_pid = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--chip=", 7) == 0)
            chip = argv[i] + 7;
        else if (strncmp(argv[i], "--out=", 6) == 0)
            out = argv[i] + 6;
        else if (!(opt_u64(argv[i], "--pin", &pin) || opt_u64(argv[i], "--count", &count) ||
                   opt_u64(argv[i], "--min-ms", &min_ms) || opt_u64(argv[i], "--max-ms", &max_ms) ||
                   opt_u64(argv[i], "--seed", &seed) ||
                   opt_u64(argv[i], "--start-delay-ms", &delay_ms) ||
                   opt_u64(argv[i], "--notify-pid", &notify_pid))) {
            fprintf(stderr,
                    "usage: %s --out=PATH [--pin=5] [--count=1000] [--min-ms=20] [--max-ms=50]\n"
                    "          [--seed=1] [--start-delay-ms=500] [--notify-pid=PID] [--chip=PATH]\n"
                    "  --notify-pid  send SIGINT to PID after the last edge (stops the receiver)\n",
                    argv[0]);
            return 2;
        }
    }
    if (!out || min_ms == 0 || max_ms < min_ms || seed == 0) {
        fprintf(stderr, "stimulus: need --out, 0 < min-ms <= max-ms, seed != 0\n");
        return 2;
    }

    FILE *log = fopen(out, "w");
    if (!log) {
        perror(out);
        return 1;
    }
    const unsigned int outputs[] = {(unsigned int)pin};
    gpio_backend *b = gpio_libgpiod_open_lines(chip, NULL, 0, outputs, 1, false);
    if (!b) {
        fprintf(stderr, "stimulus: could not request GPIO%" PRIu64 " as output\n", pin);
        fclose(log);
        return 1;
    }

    /* Log in memory and write afterwards, so file I/O never sits between edges. */
    uint64_t *before = malloc(count * sizeof *before), *after = malloc(count * sizeof *after);
    if (!before || !after) {
        fprintf(stderr, "stimulus: out of memory\n");
        return 1;
    }

    uint64_t s = seed, span = (max_ms - min_ms) * 1000000ULL;
    uint64_t next = mono_ns() + delay_ms * 1000000ULL;
    int level = 0;
    for (uint64_t i = 0; i < count; i++) {
        /* Space from the edge that actually happened, not from the planned
         * time, so a late wake-up can never squeeze the next gap below min-ms
         * (polling would then miss edges because of the stimulus). */
        if (i > 0)
            next = before[i - 1];
        next += min_ms * 1000000ULL + (span ? rng_next(&s) % (span + 1) : 0);
        sleep_until_ns(next);
        level = !level;
        before[i] = mono_ns();
        if (b->write(b, (unsigned int)pin, level) < 0) {
            perror("stimulus write");
            return 1;
        }
        after[i] = mono_ns();
    }

    /* Let the receiver handle the last edge, then stop it while the line is
     * still driven (releasing it first would make GPIO6 float). */
    sleep_until_ns(mono_ns() + 200000000ULL);
    if (notify_pid)
        kill((pid_t)notify_pid, SIGINT);
    sleep_until_ns(mono_ns() + 500000000ULL);
    b->close(b);

    fprintf(log, "t_before_ns,t_after_ns,level\n");
    uint64_t write_sum = 0;
    for (uint64_t i = 0; i < count; i++) {
        fprintf(log, "%" PRIu64 ",%" PRIu64 ",%d\n", before[i], after[i], (int)(i % 2 == 0));
        write_sum += after[i] - before[i];
    }
    fclose(log);
    fprintf(stderr, "stimulus: %" PRIu64 " edges on GPIO%" PRIu64 ", mean write %.2f us\n", count,
            pin, count ? (double)write_sum / (double)count / 1000.0 : 0.0);
    free(before);
    free(after);
    return 0;
}
