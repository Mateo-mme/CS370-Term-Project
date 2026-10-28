/* Mocked-hardware: HC-SR04 ranging through the real pipeline in irq and poll
 * modes. The mock answers each TRIG pulse with a scripted ECHO pulse. */
#include "harness.h"
#include "hcsr04.h"

#include <stdlib.h>

/* Echo pulse for 17.15 cm: 1 ms round trip. */
#define ECHO_DELAY (500 * US)
#define ECHO_WIDTH (1 * MS)

/* Calls fn(fields, ctx) for each distance row; fields[0..6] are the columns. */
static int for_each_distance(const char *csv, void (*fn)(char **f, void *ctx), void *ctx)
{
    int rows = 0;
    char *copy = csv ? strdup(csv) : NULL;
    if (!copy)
        return 0;
    char *save_line;
    for (char *line = strtok_r(copy, "\n", &save_line); line;
         line = strtok_r(NULL, "\n", &save_line)) {
        char *f[7] = {0};
        char *p = line;
        for (int i = 0; i < 7 && p; i++) {
            f[i] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = '\0';
        }
        if (f[3] && strcmp(f[3], "distance") == 0) {
            rows++;
            fn(f, ctx);
        }
    }
    free(copy);
    return rows;
}

static void expect_17cm_polled(char **f, void *ctx)
{
    (void)ctx;
    CHECK_NEAR(strtod(f[4], NULL), 17.15, 0.05);
    CHECK(strcmp(f[5], "1") == 0);
    CHECK(strcmp(f[2], "poll") == 0);
    CHECK(f[6][0] == '\0'); /* latency unknown in poll mode */
}

static void expect_timeout(char **f, void *ctx)
{
    (void)ctx;
    CHECK(f[4][0] == '\0' && strcmp(f[5], "0") == 0 && f[6][0] == '\0');
}

static pipeline_config range_cfg(pipe_mode mode, uint64_t duration_ns)
{
    return (pipeline_config){
        .mode = mode,
        .poll_interval_ns = 1 * MS,
        .duration_ns = duration_ns,
        .range = RANGE_ALWAYS,
        .range_interval_ns = 100 * MS,
    };
}

static void test_irq_ranging(void)
{
    run_result r;
    gpio_backend *b = new_mock(50 * US);
    if (b)
        gpio_mock_set_echo(b, ECHO_DELAY, ECHO_WIDTH);
    pipeline_config cfg = range_cfg(PIPE_IRQ, 400 * MS);
    run_cfg(&r, b, &cfg, NULL);
    CHECK(r.rc == 0);
    /* First trigger after the 60 ms settle; TRIG falls 10 µs later. Then one
     * trigger per 100 ms. Edge time = ECHO fall; latency = irq delivery. */
    check_csv(&r, "61510000,1970-01-01T00:00:00.061560Z,irq,distance,17.15,1,50000\n"
                  "161520000,1970-01-01T00:00:00.161570Z,irq,distance,17.15,1,50000\n"
                  "261530000,1970-01-01T00:00:00.261580Z,irq,distance,17.15,1,50000\n"
                  "361540000,1970-01-01T00:00:00.361590Z,irq,distance,17.15,1,50000\n");
    CHECK(r.stats.ranges == 4 && r.stats.range_timeouts == 0);
    CHECK(r.stats.echo_edges == 8);
    CHECK(r.b && gpio_mock_write_count(r.b, PIN_TRIG) == 8); /* 4 pulses, high + low */
    finish(&r);
}

static void test_irq_timeout(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        gpio_mock_set_echo(b, ECHO_DELAY, 0); /* never answers */
    pipeline_config cfg = range_cfg(PIPE_IRQ, 200 * MS);
    run_cfg(&r, b, &cfg, NULL);
    /* Reported 30 ms (+1 ns) after TRIG fell; no distance, no latency. */
    check_csv(&r, "90010001,1970-01-01T00:00:00.090010Z,irq,distance,,0,\n"
                  "190020001,1970-01-01T00:00:00.190020Z,irq,distance,,0,\n");
    CHECK(r.stats.ranges == 2 && r.stats.range_timeouts == 2);
    finish(&r);
}

static void test_irq_out_of_range(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        gpio_mock_set_echo(b, ECHO_DELAY, 24 * MS); /* clone "no echo" pulse */
    pipeline_config cfg = range_cfg(PIPE_IRQ, 100 * MS);
    run_cfg(&r, b, &cfg, NULL);
    check_csv(&r, "84510000,1970-01-01T00:00:00.084510Z,irq,distance,411.60,0,0\n");
    CHECK(r.stats.range_timeouts == 0);
    finish(&r);
}

static void test_irq_ranging_with_motion(void)
{
    run_result r;
    gpio_backend *b = new_mock(50 * US);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, ECHO_WIDTH);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 100 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 161 * MS); /* mid-echo */
    }
    pipeline_config cfg = range_cfg(PIPE_IRQ, 300 * MS);
    run_cfg(&r, b, &cfg, NULL);
    check_csv(&r, "61510000,1970-01-01T00:00:00.061560Z,irq,distance,17.15,1,50000\n"
                  "100000000,1970-01-01T00:00:00.100050Z,irq,motion_start,,,50000\n"
                  "161000000,1970-01-01T00:00:00.161050Z,irq,motion_end,,,50000\n"
                  "161520000,1970-01-01T00:00:00.161570Z,irq,distance,17.15,1,50000\n"
                  "261530000,1970-01-01T00:00:00.261580Z,irq,distance,17.15,1,50000\n");
    CHECK(r.stats.pir_edges == 2 && r.stats.ranges == 3);
    finish(&r);
}

static void test_poll_ranging(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, ECHO_WIDTH);
        gpio_mock_set_read_cost(b, 1 * US); /* each busy-poll read takes 1 µs */
    }
    pipeline_config cfg = range_cfg(PIPE_POLL, 300 * MS);
    run_cfg(&r, b, &cfg, NULL);
    CHECK(r.rc == 0);
    CHECK(for_each_distance(r.csv, expect_17cm_polled, NULL) == 3);
    CHECK(r.stats.ranges == 3);
    /* ~1.5 ms of busy reads per measurement at 1 µs each */
    CHECK(r.stats.echo_samples > 3 * 1400 && r.stats.echo_samples < 3 * 1600);
    /* each busy measurement outlasts the 1 ms PIR slot */
    CHECK(r.stats.overruns >= 3);
    finish(&r);
}

static void test_poll_timeout(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, 0);
        gpio_mock_set_read_cost(b, 1 * US);
    }
    pipeline_config cfg = range_cfg(PIPE_POLL, 200 * MS);
    run_cfg(&r, b, &cfg, NULL);
    CHECK(for_each_distance(r.csv, expect_timeout, NULL) == 2);
    CHECK(r.stats.range_timeouts == 2);
    /* A timeout blocks PIR sampling for the full 30 ms. */
    CHECK(r.stats.echo_samples > 2 * 29000);
    finish(&r);
}

static void test_ranging_off(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        gpio_mock_set_echo(b, ECHO_DELAY, ECHO_WIDTH);
    pipeline_config cfg = range_cfg(PIPE_IRQ, 300 * MS);
    cfg.range = RANGE_OFF;
    run_cfg(&r, b, &cfg, NULL);
    check_csv(&r, "");
    CHECK(r.b && gpio_mock_write_count(r.b, PIN_TRIG) == 0);
    finish(&r);
}

int main(void)
{
    test_irq_ranging();
    test_irq_timeout();
    test_irq_out_of_range();
    test_irq_ranging_with_motion();
    test_poll_ranging();
    test_poll_timeout();
    test_ranging_off();
    TEST_DONE("test_pipeline_range");
}
