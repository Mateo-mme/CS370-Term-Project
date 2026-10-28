/* Test-only controls for the mock GPIO backend (gpio_mock_open in gpio_backend.h).
 *
 * The mock has a virtual monotonic clock starting at 0. A script of timestamped
 * edges drives it two ways at once:
 *   - line levels: an edge takes effect once the virtual clock reaches its
 *     timestamp, so polled reads see it (poll mode);
 *   - delivery: wait_edge hands out edges in order, advancing the clock to the
 *     edge time plus a configurable interrupt latency (irq mode). */
#ifndef GPIO_MOCK_H
#define GPIO_MOCK_H

#include "gpio_backend.h"
#include "mono_clock.h"

#include <stddef.h>

/* Queue an edge. Edges are kept in timestamp order (equal timestamps keep
 * insertion order). Returns 0, or -1 on bad pin or OOM. */
int gpio_mock_push_edge(gpio_backend *b, unsigned int pin, gpio_edge_type type,
                        uint64_t timestamp_ns);

/* Set a line level directly (later scripted edges still override it). */
int gpio_mock_set_level(gpio_backend *b, unsigned int pin, int level);

/* Simulated delay between an edge and wait_edge returning it. Default 0. */
void gpio_mock_set_irq_latency(gpio_backend *b, uint64_t latency_ns);

/* Virtual time each read() takes, so busy-polling loops advance the clock.
 * Default 0. */
void gpio_mock_set_read_cost(gpio_backend *b, uint64_t cost_ns);

/* Simulated HC-SR04: when TRIG goes high -> low, ECHO rises delay_ns later
 * and stays high for width_ns. width_ns == 0 means no echo at all (timeout).
 * Disabled by default. */
void gpio_mock_set_echo(gpio_backend *b, uint64_t delay_ns, uint64_t width_ns);

/* Number of queued edges not yet delivered by wait_edge. */
size_t gpio_mock_pending(const gpio_backend *b);

/* Number of writes made to a pin (e.g. TRIG pulses). */
unsigned gpio_mock_write_count(const gpio_backend *b, unsigned int pin);

uint64_t gpio_mock_now(const gpio_backend *b);

/* A mono_clock driven by this mock's virtual time. sleep_until advances it;
 * wall time is the Unix epoch plus virtual time. */
mono_clock gpio_mock_clock(gpio_backend *b);

#endif
