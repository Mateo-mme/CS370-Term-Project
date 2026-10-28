/* Shared harness for mocked-hardware pipeline tests: run the real pipeline over
 * a scripted gpio_mock, store to a real CSV file, and read it back. */
#ifndef HARNESS_H
#define HARNESS_H

#include "../test_util.h"
#include "csv.h"
#include "gpio_mock.h"
#include "pipeline.h"

#include <string.h>

#define US 1000ULL
#define MS 1000000ULL

typedef struct {
    gpio_backend  *b;
    char           path[128];
    pipeline_stats stats;
    int            rc;
    char          *csv; /* file contents after the run */
} run_result;

static inline gpio_backend *new_mock(uint64_t irq_latency_ns)
{
    gpio_backend *b = gpio_mock_open();
    if (b)
        gpio_mock_set_irq_latency(b, irq_latency_ns);
    return b;
}

/* Runs the pipeline over the scripted mock b, then reads back the CSV. */
static inline void run_cfg(run_result *r, gpio_backend *b, const pipeline_config *cfg,
                           volatile sig_atomic_t *stop)
{
    memset(r, 0, sizeof *r);
    r->b = b;
    r->rc = -1;
    if (!b || make_temp_file(r->path, sizeof r->path, "pipeline") < 0)
        return;
    csv_store store;
    if (csv_store_open(&store, r->path) < 0)
        return;
    mono_clock clk = gpio_mock_clock(b);
    r->rc = pipeline_run(cfg, b, &clk, &store, stop, &r->stats);
    csv_store_close(&store);
    r->csv = read_file(r->path);
}

static inline void finish(run_result *r)
{
    free(r->csv);
    if (r->path[0])
        unlink(r->path);
    if (r->b)
        r->b->close(r->b);
}

static inline void check_csv(const run_result *r, const char *rows)
{
    char want[2048];
    snprintf(want, sizeof want, "%s%s", CSV_HEADER, rows);
    CHECK(r->csv && strcmp(r->csv, want) == 0);
    if (r->csv && strcmp(r->csv, want) != 0)
        fprintf(stderr, "want:\n%sgot:\n%s", want, r->csv);
}

#endif
