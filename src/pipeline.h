/* Sensor pipeline: GPIO input (interrupt-driven or polled) -> state machines
 * (PIR motion, HC-SR04 echo timing) -> CSV storage. */
#ifndef PIPELINE_H
#define PIPELINE_H

#include "csv_store.h"
#include "gpio_backend.h"
#include "mono_clock.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum { PIPE_IRQ, PIPE_POLL } pipe_mode;

typedef struct {
    pipe_mode mode;
    uint64_t  poll_interval_ns;  /* poll mode: time between samples */
    uint64_t  debounce_ns;       /* PIR stability window; 0 = none */
    uint64_t  duration_ns;       /* stop after this long; 0 = until *stop */
    uint64_t  range_interval_ns; /* HC-SR04 trigger period (>= 60 ms); 0 = off */
    bool      print_events;      /* also print each stored event to stdout */
} pipeline_config;

typedef struct {
    uint64_t pir_edges;    /* irq: PIR edges delivered by the kernel */
    uint64_t samples;      /* poll: PIR reads */
    uint64_t overruns;     /* poll: samples taken late (missed their slot) */
    uint64_t echo_edges;   /* irq: ECHO edges delivered by the kernel */
    uint64_t echo_samples; /* poll: ECHO reads while busy-waiting for the echo */
    uint64_t ranges;       /* distance measurements completed (incl. timeouts) */
    uint64_t range_timeouts;
    uint64_t events;       /* events stored */
    uint64_t store_errors; /* events that failed to store */
} pipeline_stats;

const char *pipeline_mode_name(pipe_mode mode);

/* Runs until duration_ns elapses or *stop becomes nonzero (stop may be NULL).
 * The backend must be opened with edges for PIPE_IRQ. Returns 0, or -1 on a
 * GPIO error. Storage failures are counted in stats and do not stop the run. */
int pipeline_run(const pipeline_config *cfg, gpio_backend *b, const mono_clock *clk,
                 csv_store *store, volatile sig_atomic_t *stop, pipeline_stats *stats);

#endif
