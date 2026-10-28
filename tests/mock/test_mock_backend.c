/* Mocked-hardware: scripted edges are delivered in order with their injected
 * timestamps, drive line levels via the virtual clock, and touch no GPIO. */
#include "../test_util.h"
#include "gpio_mock.h"

#define FOREVER 1000000000LL

static void test_delivery(void)
{
    gpio_backend *b = gpio_mock_open();
    CHECK(b != NULL);
    if (!b)
        return;

    CHECK(b->read(b, PIN_PIR) == 0);
    CHECK(b->write(b, PIN_TRIG, 1) == 0);
    CHECK(b->write(b, PIN_TRIG, 0) == 0);
    CHECK(gpio_mock_write_count(b, PIN_TRIG) == 2);
    CHECK(b->read(b, 1000) == -1);

    gpio_edge e;
    CHECK(b->wait_edge(b, 0, &e) == 0); /* empty queue = timeout */

    gpio_mock_set_irq_latency(b, 50);
    CHECK(gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 1000) == 0);
    CHECK(gpio_mock_push_edge(b, PIN_ECHO, GPIO_EDGE_RISING, 2000) == 0);
    CHECK(gpio_mock_push_edge(b, PIN_ECHO, GPIO_EDGE_FALLING, 3000) == 0);
    CHECK(gpio_mock_push_edge(b, 99, GPIO_EDGE_RISING, 4000) == -1);
    CHECK(gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 500) == -1); /* out of order */
    CHECK(gpio_mock_pending(b) == 3);

    /* Next edge is beyond the timeout: time advances by the timeout only. */
    CHECK(b->wait_edge(b, 400, &e) == 0);
    CHECK(gpio_mock_now(b) == 400);

    CHECK(b->wait_edge(b, FOREVER, &e) == 1);
    CHECK(e.pin == PIN_PIR && e.type == GPIO_EDGE_RISING && e.timestamp_ns == 1000);
    CHECK(gpio_mock_now(b) == 1050); /* edge time + irq latency */
    CHECK(b->read(b, PIN_PIR) == 1);

    CHECK(b->wait_edge(b, FOREVER, &e) == 1);
    CHECK(e.pin == PIN_ECHO && e.timestamp_ns == 2000);
    CHECK(b->read(b, PIN_ECHO) == 1);

    CHECK(b->wait_edge(b, FOREVER, &e) == 1);
    CHECK(e.type == GPIO_EDGE_FALLING && e.timestamp_ns == 3000);
    CHECK(b->read(b, PIN_ECHO) == 0);

    CHECK(gpio_mock_pending(b) == 0);
    CHECK(b->wait_edge(b, 0, &e) == 0);

    /* Queue grows past its initial capacity. */
    for (uint64_t i = 0; i < 100; i++)
        CHECK(gpio_mock_push_edge(b, PIN_PIR, i % 2 ? GPIO_EDGE_FALLING : GPIO_EDGE_RISING,
                                  10000 + i) == 0);
    CHECK(gpio_mock_pending(b) == 100);

    CHECK(gpio_mock_set_level(b, PIN_ECHO, 1) == 0);
    CHECK(b->read(b, PIN_ECHO) == 1);

    b->close(b);
}

static void test_levels_follow_virtual_time(void)
{
    /* Poll-style use: sleeping on the mock clock applies level changes without
     * consuming them from the irq delivery queue. */
    gpio_backend *b = gpio_mock_open();
    if (!b)
        return;
    mono_clock clk = gpio_mock_clock(b);
    gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 100);
    gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_FALLING, 200);

    CHECK(clk.now(clk.ctx) == 0);
    CHECK(b->read(b, PIN_PIR) == 0);
    clk.sleep_until(clk.ctx, 99);
    CHECK(b->read(b, PIN_PIR) == 0);
    clk.sleep_until(clk.ctx, 100);
    CHECK(b->read(b, PIN_PIR) == 1);
    clk.sleep_until(clk.ctx, 50); /* never goes backwards */
    CHECK(clk.now(clk.ctx) == 100);
    clk.sleep_until(clk.ctx, 250);
    CHECK(b->read(b, PIN_PIR) == 0);
    CHECK(gpio_mock_pending(b) == 2);

    struct timespec ts;
    clk.wall(clk.ctx, &ts);
    CHECK(ts.tv_sec == 0 && ts.tv_nsec == 250);
    b->close(b);
}

int main(void)
{
    test_delivery();
    test_levels_follow_virtual_time();
    TEST_DONE("test_mock_backend");
}
