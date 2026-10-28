/* Hardware abstraction for GPIO so the pipeline runs against real pins or a mock. */
#ifndef GPIO_BACKEND_H
#define GPIO_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* BCM pin numbers as wired on the Pi (see docs/wiring.md). */
enum {
    PIN_PIR  = 26,
    PIN_TRIG = 23,
    PIN_ECHO = 24,
};

typedef enum { GPIO_EDGE_RISING, GPIO_EDGE_FALLING } gpio_edge_type;

typedef struct {
    unsigned int   pin;
    gpio_edge_type type;
    uint64_t       timestamp_ns; /* CLOCK_MONOTONIC time the edge occurred */
} gpio_edge;

typedef struct gpio_backend gpio_backend;

struct gpio_backend {
    const char *name;
    /* Returns 1/0 for line level, -1 on error. */
    int  (*read)(gpio_backend *b, unsigned int pin);
    int  (*write)(gpio_backend *b, unsigned int pin, int value);
    /* Wait up to timeout_ns for one edge. Returns 1 if *out was filled,
     * 0 on timeout, -1 on error. Only valid for pins requested for edges. */
    int  (*wait_edge)(gpio_backend *b, int64_t timeout_ns, gpio_edge *out);
    void (*close)(gpio_backend *b);
    void *impl;
};

/* Real hardware via libgpiod v2: requests the given input and output lines.
 * Inputs use the internal pull-down, and get both-edge detection when
 * use_edges is true (interrupt mode) or are plain inputs otherwise (polling). Outputs start low. Either list may be
 * empty. */
gpio_backend *gpio_libgpiod_open_lines(const char *chip_path, const unsigned int *inputs,
                                       size_t n_inputs, const unsigned int *outputs,
                                       size_t n_outputs, bool use_edges);

/* The sensor lines: PIR (pir_pin) and ECHO as inputs, TRIG as output. */
gpio_backend *gpio_libgpiod_open(const char *chip_path, unsigned int pir_pin, bool use_edges);

/* Scripted backend for tests: no hardware access. */
gpio_backend *gpio_mock_open(void);

#endif
