/* Software-only: HC-SR04 echo-width conversion, range checks, and timeouts. */
#include "../test_util.h"
#include "hcsr04.h"

#define US 1000ULL
#define MS 1000000ULL

static void test_conversion(void)
{
    CHECK_NEAR(hcsr04_width_to_cm(0), 0.0, 1e-9);
    CHECK_NEAR(hcsr04_width_to_cm(1 * MS), 17.15, 1e-6);
    CHECK_NEAR(hcsr04_width_to_cm(5831 * US), 100.0, 0.01);
}

static void test_valid_echo(void)
{
    hcsr04_tracker t;
    hcsr04_reading r;
    hcsr04_init(&t);
    hcsr04_start(&t, 0);
    CHECK(!hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 100 * US, &r)); /* ignored before rise */
    CHECK(!hcsr04_on_edge(&t, GPIO_EDGE_RISING, 500 * US, &r));
    CHECK(hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 1500 * US, &r));
    CHECK(r.status == HCSR04_OK);
    CHECK(r.width_ns == 1 * MS);
    CHECK(r.rise_ns == 500 * US);
    CHECK_NEAR(r.cm, 17.15, 1e-6);
    CHECK(t.phase == HCSR04_IDLE);
}

static void test_stray_edges_ignored_when_idle(void)
{
    hcsr04_tracker t;
    hcsr04_reading r;
    hcsr04_init(&t);
    CHECK(!hcsr04_on_edge(&t, GPIO_EDGE_RISING, 1, &r));
    CHECK(!hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 2, &r));
    CHECK(!hcsr04_check_timeout(&t, 100 * MS, &r));
}

static void test_repeated_rise_restarts(void)
{
    hcsr04_tracker t;
    hcsr04_reading r;
    hcsr04_start(&t, 0);
    hcsr04_on_edge(&t, GPIO_EDGE_RISING, 1 * MS, &r);
    CHECK(!hcsr04_on_edge(&t, GPIO_EDGE_RISING, 2 * MS, &r));
    CHECK(hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 3 * MS, &r));
    CHECK(r.width_ns == 1 * MS);
}

static void test_out_of_range(void)
{
    hcsr04_tracker t;
    hcsr04_reading r;
    hcsr04_start(&t, 0);
    hcsr04_on_edge(&t, GPIO_EDGE_RISING, 0, &r);
    CHECK(hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 50 * US, &r)); /* ~0.86 cm */
    CHECK(r.status == HCSR04_OUT_OF_RANGE);

    hcsr04_start(&t, 0);
    hcsr04_on_edge(&t, GPIO_EDGE_RISING, 0, &r);
    CHECK(hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 25 * MS, &r)); /* ~428 cm */
    CHECK(r.status == HCSR04_OUT_OF_RANGE);
}

static void test_timeout_no_rise(void)
{
    hcsr04_tracker t;
    hcsr04_reading r;
    hcsr04_start(&t, 10 * MS);
    CHECK(!hcsr04_check_timeout(&t, 10 * MS + HCSR04_TIMEOUT_NS, &r));
    CHECK(hcsr04_check_timeout(&t, 10 * MS + HCSR04_TIMEOUT_NS + 1, &r));
    CHECK(r.status == HCSR04_TIMEOUT);
    CHECK(r.rise_ns == 0);
    CHECK(t.phase == HCSR04_IDLE);
}

static void test_timeout_stuck_high(void)
{
    hcsr04_tracker t;
    hcsr04_reading r;
    hcsr04_start(&t, 0);
    hcsr04_on_edge(&t, GPIO_EDGE_RISING, 1 * MS, &r);
    CHECK(!hcsr04_check_timeout(&t, 20 * MS, &r));
    CHECK(hcsr04_check_timeout(&t, 1 * MS + HCSR04_TIMEOUT_NS + 1, &r));
    CHECK(r.status == HCSR04_TIMEOUT);
    CHECK(r.rise_ns == 1 * MS);
    /* A late falling edge after timeout must not produce a reading. */
    CHECK(!hcsr04_on_edge(&t, GPIO_EDGE_FALLING, 40 * MS, &r));
}

int main(void)
{
    test_conversion();
    test_valid_echo();
    test_stray_edges_ignored_when_idle();
    test_repeated_rise_restarts();
    test_out_of_range();
    test_timeout_no_rise();
    test_timeout_stuck_high();
    TEST_DONE("test_hcsr04");
}
