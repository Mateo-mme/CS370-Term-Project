/* Mocked-hardware: scripted PIR edges flow through the real pipeline (irq and
 * poll modes) into a real CSV file, which is read back and checked. */
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

static gpio_backend *new_mock(uint64_t irq_latency_ns)
{
    gpio_backend *b = gpio_mock_open();
    if (b)
        gpio_mock_set_irq_latency(b, irq_latency_ns);
    return b;
}

/* Runs the pipeline over the scripted mock b, then reads back the CSV. */
static void run(run_result *r, gpio_backend *b, pipe_mode mode, uint64_t poll_ns,
                uint64_t debounce_ns, uint64_t duration_ns, volatile sig_atomic_t *stop)
{
    memset(r, 0, sizeof *r);
    r->b = b;
    r->rc = -1;
    if (!b || make_temp_file(r->path, sizeof r->path, "pipeline") < 0)
        return;

    pipeline_config cfg = {
        .mode = mode,
        .poll_interval_ns = poll_ns,
        .debounce_ns = debounce_ns,
        .duration_ns = duration_ns,
    };
    csv_store store;
    if (csv_store_open(&store, r->path) < 0)
        return;
    mono_clock clk = gpio_mock_clock(b);
    r->rc = pipeline_run(&cfg, b, &clk, &store, stop, &r->stats);
    csv_store_close(&store);
    r->csv = read_file(r->path);
}

static void finish(run_result *r)
{
    free(r->csv);
    if (r->path[0])
        unlink(r->path);
    if (r->b)
        r->b->close(r->b);
}

static void check_csv(const run_result *r, const char *rows)
{
    char want[1024];
    snprintf(want, sizeof want, "%s%s", CSV_HEADER, rows);
    CHECK(r->csv && strcmp(r->csv, want) == 0);
    if (r->csv && strcmp(r->csv, want) != 0)
        fprintf(stderr, "want:\n%sgot:\n%s", want, r->csv);
}

static void push_motion(gpio_backend *b, uint64_t start_ns, uint64_t end_ns)
{
    gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, start_ns);
    /* ECHO activity must be ignored by the PIR path. */
    gpio_mock_push_edge(b, PIN_ECHO, GPIO_EDGE_RISING, start_ns + 2 * MS);
    gpio_mock_push_edge(b, PIN_ECHO, GPIO_EDGE_FALLING, start_ns + 3 * MS);
    gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, end_ns);
}

static void test_irq_motion(void)
{
    run_result r;
    gpio_backend *b = new_mock(50 * US);
    if (b)
        push_motion(b, 10 * MS, 50 * MS);
    run(&r, b, PIPE_IRQ, 0, 0, 100 * MS, NULL);
    CHECK(r.rc == 0);
    /* Edge time is the (kernel) edge timestamp; latency = handled - edge. */
    check_csv(&r, "10000000,1970-01-01T00:00:00.010050Z,irq,motion_start,,,50000\n"
                  "50000000,1970-01-01T00:00:00.050050Z,irq,motion_end,,,50000\n");
    CHECK(r.stats.pir_edges == 2);
    CHECK(r.stats.events == 2);
    CHECK(r.stats.samples == 0);
    CHECK(r.b && gpio_mock_now(r.b) == 100 * MS); /* ran for exactly the duration */
    finish(&r);
}

static void test_poll_motion(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        push_motion(b, 10 * MS, 50 * MS);
    run(&r, b, PIPE_POLL, 1 * MS, 0, 100 * MS, NULL);
    CHECK(r.rc == 0);
    /* Poll mode: time is the sample that saw the change; latency unknown. */
    check_csv(&r, "10000000,1970-01-01T00:00:00.010000Z,poll,motion_start,,,\n"
                  "50000000,1970-01-01T00:00:00.050000Z,poll,motion_end,,,\n");
    CHECK(r.stats.samples == 100);
    CHECK(r.stats.overruns == 0);
    CHECK(r.stats.pir_edges == 0);
    finish(&r);
}

static void test_poll_quantization(void)
{
    /* With a 10 ms interval an edge at 15 ms is only seen by the 20 ms sample:
     * the detection delay polling adds, which irq mode avoids. */
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        push_motion(b, 15 * MS, 41 * MS);
    run(&r, b, PIPE_POLL, 10 * MS, 0, 100 * MS, NULL);
    check_csv(&r, "20000000,1970-01-01T00:00:00.020000Z,poll,motion_start,,,\n"
                  "50000000,1970-01-01T00:00:00.050000Z,poll,motion_end,,,\n");
    CHECK(r.stats.samples == 10);
    finish(&r);
}

static void test_irq_debounce(void)
{
    run_result r;
    gpio_backend *b = new_mock(50 * US);
    if (b) {
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 10 * MS); /* 0.5 ms glitch */
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 10 * MS + 500 * US);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 20 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 60 * MS);
    }
    run(&r, b, PIPE_IRQ, 0, 5 * MS, 100 * MS, NULL);
    /* Committed when the debounce window expires; latency includes the window. */
    check_csv(&r, "20000000,1970-01-01T00:00:00.025000Z,irq,motion_start,,,5000000\n"
                  "60000000,1970-01-01T00:00:00.065000Z,irq,motion_end,,,5000000\n");
    CHECK(r.stats.pir_edges == 4);
    CHECK(r.stats.events == 2);
    finish(&r);
}

static void test_initially_high(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b) {
        gpio_mock_set_level(b, PIN_PIR, 1);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 5 * MS);
    }
    run(&r, b, PIPE_IRQ, 0, 0, 10 * MS, NULL);
    check_csv(&r, "5000000,1970-01-01T00:00:00.005000Z,irq,motion_end,,,0\n");
    finish(&r);
}

static void test_stop_flag(void)
{
    volatile sig_atomic_t stop = 1;
    for (int m = 0; m < 2; m++) {
        run_result r;
        gpio_backend *b = new_mock(0);
        if (b)
            push_motion(b, 1 * MS, 10 * MS);
        run(&r, b, m ? PIPE_POLL : PIPE_IRQ, 1 * MS, 0, 0, &stop);
        CHECK(r.rc == 0);
        check_csv(&r, "");
        CHECK(r.stats.events == 0);
        finish(&r);
    }
}

int main(void)
{
    test_irq_motion();
    test_poll_motion();
    test_poll_quantization();
    test_irq_debounce();
    test_initially_high();
    test_stop_flag();
    TEST_DONE("test_pipeline_pir");
}
