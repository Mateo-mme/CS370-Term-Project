/* crop_guard: PIR + HC-SR04 crop-threat monitor. */
#include "csv_store.h"
#include "gpio_backend.h"
#include "hcsr04.h"
#include "mono_clock.h"
#include "pipeline.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_CHIP "/dev/gpiochip0"
#define DEFAULT_OUT  "data/events.csv"

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [options]\n"
            "  --mode=irq|poll     input mechanism (default irq)\n"
            "  --poll-us=N         poll mode sampling interval in µs (default 1000)\n"
            "  --debounce-ms=N     PIR stability window in ms (default 0 = off)\n"
            "  --duration-s=N      stop after N seconds (default 0 = until Ctrl-C)\n"
            "  --range-ms=N        trigger the HC-SR04 every N ms (>= 60; default 0 = off)\n"
            "  --out=PATH          CSV file to append to (default " DEFAULT_OUT ")\n"
            "  --chip=PATH         GPIO chip (default " DEFAULT_CHIP ")\n"
            "  --quiet             do not print events to stdout\n"
            "  --probe             open the GPIO lines, print PIR/ECHO levels, and exit\n",
            prog);
}

static int probe(const char *chip, bool use_edges)
{
    gpio_backend *b = gpio_libgpiod_open(chip, use_edges);
    if (!b) {
        fprintf(stderr, "probe: could not open GPIO lines on %s\n", chip);
        return 1;
    }
    printf("backend=%s PIR(GPIO%d)=%d ECHO(GPIO%d)=%d TRIG(GPIO%d)=output\n", b->name,
           PIN_PIR, b->read(b, PIN_PIR), PIN_ECHO, b->read(b, PIN_ECHO), PIN_TRIG);
    b->close(b);
    return 0;
}

/* Parses "--name=<unsigned>" into *out. Returns false if arg is not that option. */
static bool parse_u64_opt(const char *arg, const char *name, uint64_t *out, bool *bad)
{
    size_t len = strlen(name);
    if (strncmp(arg, name, len) != 0 || arg[len] != '=')
        return false;
    const char *val = arg + len + 1;
    char *end;
    errno = 0;
    unsigned long long v = strtoull(val, &end, 10);
    if (*val == '\0' || *val == '-' || *end != '\0' || errno)
        *bad = true;
    *out = v;
    return true;
}

/* Creates the parent directory of path if it has one (one level only). */
static void ensure_parent_dir(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash || slash == path)
        return;
    char dir[512];
    size_t len = (size_t)(slash - path);
    if (len >= sizeof dir)
        return;
    memcpy(dir, path, len);
    dir[len] = '\0';
    if (mkdir(dir, 0755) < 0 && errno != EEXIST)
        perror(dir);
}

int main(int argc, char **argv)
{
    const char *chip = DEFAULT_CHIP;
    const char *out = DEFAULT_OUT;
    bool do_probe = false;
    uint64_t poll_us = 1000, debounce_ms = 0, duration_s = 0, range_ms = 0;
    pipeline_config cfg = {.mode = PIPE_IRQ, .print_events = true};

    for (int i = 1; i < argc; i++) {
        bool bad = false;
        if (strcmp(argv[i], "--mode=irq") == 0)
            cfg.mode = PIPE_IRQ;
        else if (strcmp(argv[i], "--mode=poll") == 0)
            cfg.mode = PIPE_POLL;
        else if (strncmp(argv[i], "--chip=", 7) == 0)
            chip = argv[i] + 7;
        else if (strncmp(argv[i], "--out=", 6) == 0)
            out = argv[i] + 6;
        else if (strcmp(argv[i], "--probe") == 0)
            do_probe = true;
        else if (strcmp(argv[i], "--quiet") == 0)
            cfg.print_events = false;
        else if (parse_u64_opt(argv[i], "--poll-us", &poll_us, &bad) ||
                 parse_u64_opt(argv[i], "--debounce-ms", &debounce_ms, &bad) ||
                 parse_u64_opt(argv[i], "--duration-s", &duration_s, &bad) ||
                 parse_u64_opt(argv[i], "--range-ms", &range_ms, &bad)) {
            if (bad) {
                fprintf(stderr, "invalid number in %s\n", argv[i]);
                return 2;
            }
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (poll_us == 0) {
        fprintf(stderr, "--poll-us must be > 0\n");
        return 2;
    }
    if (range_ms != 0 && range_ms * 1000000 < HCSR04_MIN_INTERVAL_NS) {
        fprintf(stderr, "--range-ms must be 0 or >= %llu (HC-SR04 cycle time)\n",
                HCSR04_MIN_INTERVAL_NS / 1000000);
        return 2;
    }

    if (do_probe)
        return probe(chip, cfg.mode == PIPE_IRQ);

    cfg.poll_interval_ns = poll_us * 1000;
    cfg.debounce_ns = debounce_ms * 1000000;
    cfg.duration_ns = duration_s * 1000000000;
    cfg.range_interval_ns = range_ms * 1000000;

    /* No SA_RESTART: a signal must interrupt the blocking wait or sleep so the
     * loop notices stop_requested promptly. */
    struct sigaction sa = {.sa_handler = on_signal};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ensure_parent_dir(out);
    csv_store store;
    if (csv_store_open(&store, out) < 0) {
        perror(out);
        return 1;
    }

    gpio_backend *b = gpio_libgpiod_open(chip, cfg.mode == PIPE_IRQ);
    if (!b) {
        fprintf(stderr, "could not open GPIO lines on %s\n", chip);
        csv_store_close(&store);
        return 1;
    }

    mono_clock clk = mono_clock_real();
    fprintf(stderr, "crop_guard: mode=%s backend=%s out=%s%s\n", pipeline_mode_name(cfg.mode),
            b->name, out, cfg.duration_ns ? "" : " (Ctrl-C to stop)");

    pipeline_stats stats;
    int rc = pipeline_run(&cfg, b, &clk, &store, &stop_requested, &stats);

    b->close(b);
    csv_store_close(&store);

    fprintf(stderr,
            "crop_guard: stopped. events=%" PRIu64 " pir_edges=%" PRIu64 " samples=%" PRIu64
            " overruns=%" PRIu64 " ranges=%" PRIu64 " range_timeouts=%" PRIu64
            " echo_edges=%" PRIu64 " echo_samples=%" PRIu64 " store_errors=%" PRIu64 "\n",
            stats.events, stats.pir_edges, stats.samples, stats.overruns, stats.ranges,
            stats.range_timeouts, stats.echo_edges, stats.echo_samples, stats.store_errors);
    return (rc == 0 && stats.store_errors == 0) ? 0 : 1;
}
