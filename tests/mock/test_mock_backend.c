/* Mocked-hardware: scripted edges are delivered in order with their injected
 * timestamps and update line levels, without touching any GPIO. */
#include "../test_util.h"
#include "gpio_mock.h"

int main(void)
{
    gpio_backend *b = gpio_mock_open();
    CHECK(b != NULL);
    if (!b)
        return 1;

    CHECK(b->read(b, PIN_PIR) == 0);
    CHECK(b->write(b, PIN_TRIG, 1) == 0);
    CHECK(b->write(b, PIN_TRIG, 0) == 0);
    CHECK(gpio_mock_write_count(b, PIN_TRIG) == 2);
    CHECK(b->read(b, 1000) == -1);

    gpio_edge e;
    CHECK(b->wait_edge(b, 0, &e) == 0); /* empty queue = timeout */

    CHECK(gpio_mock_push_edge(b, PIN_PIR, GPIO_EDGE_RISING, 1000) == 0);
    CHECK(gpio_mock_push_edge(b, PIN_ECHO, GPIO_EDGE_RISING, 2000) == 0);
    CHECK(gpio_mock_push_edge(b, PIN_ECHO, GPIO_EDGE_FALLING, 3000) == 0);
    CHECK(gpio_mock_push_edge(b, 99, GPIO_EDGE_RISING, 0) == -1);
    CHECK(gpio_mock_pending(b) == 3);

    CHECK(b->wait_edge(b, 0, &e) == 1);
    CHECK(e.pin == PIN_PIR && e.type == GPIO_EDGE_RISING && e.timestamp_ns == 1000);
    CHECK(b->read(b, PIN_PIR) == 1);

    CHECK(b->wait_edge(b, 0, &e) == 1);
    CHECK(e.pin == PIN_ECHO && e.timestamp_ns == 2000);
    CHECK(b->read(b, PIN_ECHO) == 1);

    CHECK(b->wait_edge(b, 0, &e) == 1);
    CHECK(e.type == GPIO_EDGE_FALLING && e.timestamp_ns == 3000);
    CHECK(b->read(b, PIN_ECHO) == 0);

    CHECK(gpio_mock_pending(b) == 0);
    CHECK(b->wait_edge(b, 0, &e) == 0);

    /* Queue grows past its initial capacity. */
    for (uint64_t i = 0; i < 100; i++)
        CHECK(gpio_mock_push_edge(b, PIN_PIR, i % 2 ? GPIO_EDGE_FALLING : GPIO_EDGE_RISING, i) == 0);
    CHECK(gpio_mock_pending(b) == 100);

    CHECK(gpio_mock_set_level(b, PIN_ECHO, 1) == 0);
    CHECK(b->read(b, PIN_ECHO) == 1);

    b->close(b);
    TEST_DONE("test_mock_backend");
}
