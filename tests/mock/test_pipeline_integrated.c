/* Mocked-hardware: the integrated system. The PIR gates HC-SR04 ranging, and
 * motion plus a close echo produces one threat event per motion episode. All
 * rows go through the real pipeline into a real CSV file. */
#include "harness.h"
#include "hcsr04.h"

#define ECHO_DELAY (500 * US)
#define WIDTH_17CM (1 * MS)      /* 17.15 cm */
#define WIDTH_240CM (14 * MS)    /* 240.10 cm */

static pipeline_config integrated_cfg(pipe_mode mode, uint64_t duration_ns, double alert_cm)
{
    return (pipeline_config){
        .mode = mode,
        .poll_interval_ns = 1 * MS,
        .duration_ns = duration_ns,
        .range = RANGE_ON_MOTION,
        .range_interval_ns = 100 * MS,
        .alert_cm = alert_cm,
    };
}

static int count_rows(const char *csv, const char *event)
{
    char needle[32];
    snprintf(needle, sizeof needle, ",%s,", event);
    int n = 0;
    for (const char *p = csv; p && (p = strstr(p, needle)); p++)
        n++;
    return n;
}

static void test_irq_motion_gates_ranging(void)
{
    run_result r;
    gpio_backend *b = new_mock(50 * US);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_17CM);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 200 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 450 * MS);
    }
    pipeline_config cfg = integrated_cfg(PIPE_IRQ, 600 * MS, 200);
    run_cfg(&r, b, &cfg, NULL);
    CHECK(r.rc == 0);
    /* No pings before motion. The first ping fires as soon as motion_start is
     * handled, then one every 100 ms until motion_end. The first close reading
     * also raises the threat. */
    check_csv(&r, "200000000,1970-01-01T00:00:00.200050Z,irq,motion_start,,,50000\n"
                  "201560000,1970-01-01T00:00:00.201610Z,irq,distance,17.15,1,50000\n"
                  "201560000,1970-01-01T00:00:00.201610Z,irq,threat,17.15,1,50000\n"
                  "301570000,1970-01-01T00:00:00.301620Z,irq,distance,17.15,1,50000\n"
                  "401580000,1970-01-01T00:00:00.401630Z,irq,distance,17.15,1,50000\n"
                  "450000000,1970-01-01T00:00:00.450050Z,irq,motion_end,,,50000\n");
    CHECK(r.stats.ranges == 3 && r.stats.threats == 1);
    CHECK(r.b && gpio_mock_write_count(r.b, PIN_TRIG) == 6);
    finish(&r);
}

static void test_no_motion_no_ranging(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_17CM);
    pipeline_config cfg = integrated_cfg(PIPE_IRQ, 1000 * MS, 200);
    run_cfg(&r, b, &cfg, NULL);
    check_csv(&r, "");
    CHECK(r.b && gpio_mock_write_count(r.b, PIN_TRIG) == 0);
    finish(&r);
}

static void test_threshold_and_episodes(void)
{
    /* Two motion episodes, target at 240 cm. */
    double thresholds[] = {200, 300, 0};
    int want_threats[] = {0, 2, 0}; /* beyond 200; within 300 once per episode; off */
    for (int i = 0; i < 3; i++) {
        run_result r;
        gpio_backend *b = new_mock(0);
        if (b) {
            gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_240CM);
            gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 100 * MS);
            gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 400 * MS);
            gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 600 * MS);
            gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 900 * MS);
        }
        pipeline_config cfg = integrated_cfg(PIPE_IRQ, 1000 * MS, thresholds[i]);
        run_cfg(&r, b, &cfg, NULL);
        CHECK(count_rows(r.csv, "threat") == want_threats[i]);
        CHECK(count_rows(r.csv, "distance") == 6); /* 3 pings per 300 ms episode */
        CHECK(count_rows(r.csv, "motion_start") == 2);
        CHECK(r.stats.threats == (uint64_t)want_threats[i]);
        finish(&r);
    }
}

static void test_motion_restart_respects_cycle_time(void)
{
    /* Motion ends right after a ping and restarts 20 ms later: the next ping
     * must still wait the HC-SR04's 60 ms cycle time. */
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_17CM);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 100 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 105 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 125 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 200 * MS);
    }
    pipeline_config cfg = integrated_cfg(PIPE_IRQ, 300 * MS, 200);
    run_cfg(&r, b, &cfg, NULL);
    /* Pings: TRIG falls at 100.01 ms, then not before 160.01 ms. */
    check_csv(&r, "100000000,1970-01-01T00:00:00.100000Z,irq,motion_start,,,0\n"
                  "101510000,1970-01-01T00:00:00.101510Z,irq,distance,17.15,1,0\n"
                  "101510000,1970-01-01T00:00:00.101510Z,irq,threat,17.15,1,0\n"
                  "105000000,1970-01-01T00:00:00.105000Z,irq,motion_end,,,0\n"
                  "125000000,1970-01-01T00:00:00.125000Z,irq,motion_start,,,0\n"
                  "161520000,1970-01-01T00:00:00.161520Z,irq,distance,17.15,1,0\n"
                  "161520000,1970-01-01T00:00:00.161520Z,irq,threat,17.15,1,0\n"
                  "200000000,1970-01-01T00:00:00.200000Z,irq,motion_end,,,0\n");
    finish(&r);
}

static void test_settle_before_first_ping(void)
{
    /* PIR already high at startup: ranging still waits the 60 ms settle time. */
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_17CM);
        gpio_mock_set_level(b, PIN_PIR, 1);
    }
    pipeline_config cfg = integrated_cfg(PIPE_IRQ, 100 * MS, 200);
    run_cfg(&r, b, &cfg, NULL);
    /* Motion was already in progress (no motion_start row), so the reading
     * still counts as a threat. */
    check_csv(&r, "61510000,1970-01-01T00:00:00.061510Z,irq,distance,17.15,1,0\n"
                  "61510000,1970-01-01T00:00:00.061510Z,irq,threat,17.15,1,0\n");
    finish(&r);
}

static void test_poll_motion_gates_ranging(void)
{
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b) {
        gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_17CM);
        gpio_mock_set_read_cost(b, 1 * US);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 100 * MS);
        gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 280 * MS);
    }
    pipeline_config cfg = integrated_cfg(PIPE_POLL, 500 * MS, 200);
    run_cfg(&r, b, &cfg, NULL);
    CHECK(r.rc == 0);
    CHECK(count_rows(r.csv, "motion_start") == 1 && count_rows(r.csv, "motion_end") == 1);
    CHECK(count_rows(r.csv, "distance") == 2); /* ~100 ms and ~200 ms */
    CHECK(count_rows(r.csv, "threat") == 1);
    /* Busy ECHO reads happen only during the two pings (~1.5 ms each). */
    CHECK(r.stats.echo_samples > 2 * 1400 && r.stats.echo_samples < 2 * 1600);
    finish(&r);
}

static void test_no_threat_without_motion(void)
{
    /* RANGE_ALWAYS with a close target but nobody moving: distances only. */
    run_result r;
    gpio_backend *b = new_mock(0);
    if (b)
        gpio_mock_set_echo(b, ECHO_DELAY, WIDTH_17CM);
    pipeline_config cfg = integrated_cfg(PIPE_IRQ, 300 * MS, 200);
    cfg.range = RANGE_ALWAYS;
    run_cfg(&r, b, &cfg, NULL);
    CHECK(count_rows(r.csv, "distance") == 3);
    CHECK(count_rows(r.csv, "threat") == 0);

    /* Motion ends while an echo is in flight: the reading is stored but is
     * not a threat, because the episode is over. */
    run_result r2;
    gpio_backend *b2 = new_mock(0);
    if (b2) {
        gpio_mock_set_echo(b2, ECHO_DELAY, WIDTH_17CM);
        gpio_mock_push_edge(b2, PIN_PIR, GPIO_EDGE_RISING, 100 * MS);
        gpio_mock_push_edge(b2, PIN_PIR, GPIO_EDGE_FALLING, 100 * MS + 700 * US);
    }
    pipeline_config cfg2 = integrated_cfg(PIPE_IRQ, 300 * MS, 200);
    run_cfg(&r2, b2, &cfg2, NULL);
    check_csv(&r2, "100000000,1970-01-01T00:00:00.100000Z,irq,motion_start,,,0\n"
                   "100700000,1970-01-01T00:00:00.100700Z,irq,motion_end,,,0\n"
                   "101510000,1970-01-01T00:00:00.101510Z,irq,distance,17.15,1,0\n");
    finish(&r);
    finish(&r2);
}

int main(void)
{
    test_no_threat_without_motion();
    test_irq_motion_gates_ranging();
    test_no_motion_no_ranging();
    test_threshold_and_episodes();
    test_motion_restart_respects_cycle_time();
    test_settle_before_first_ping();
    test_poll_motion_gates_ranging();
    TEST_DONE("test_pipeline_integrated");
}
