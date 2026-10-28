/* Test-only controls for the mock GPIO backend (gpio_mock_open in gpio_backend.h). */
#ifndef GPIO_MOCK_H
#define GPIO_MOCK_H

#include "gpio_backend.h"

#include <stddef.h>

/* Queue an edge. wait_edge returns queued edges in FIFO order and applies each
 * one to the line level as it is delivered. Returns 0, or -1 on bad pin / OOM. */
int gpio_mock_push_edge(gpio_backend *b, unsigned int pin, gpio_edge_type type,
                        uint64_t timestamp_ns);

/* Set a line level directly, as a polled read would see it. */
int gpio_mock_set_level(gpio_backend *b, unsigned int pin, int level);

/* Number of queued edges not yet delivered. */
size_t gpio_mock_pending(const gpio_backend *b);

/* Number of writes made to a pin (e.g. TRIG pulses). */
unsigned gpio_mock_write_count(const gpio_backend *b, unsigned int pin);

#endif
