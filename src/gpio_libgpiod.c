/* Real GPIO backend on the Linux GPIO character device via libgpiod v2.
 * In edge mode, PIR and ECHO are requested with both-edge detection: the kernel
 * services the GPIO interrupt and timestamps each edge with CLOCK_MONOTONIC. */
#include "gpio_backend.h"

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>

#define EDGE_BUF_CAP 16

typedef struct {
    struct gpiod_chip *chip;
    struct gpiod_line_request *req;
    struct gpiod_edge_event_buffer *evbuf;
    int ev_count; /* events read into evbuf */
    int ev_next;  /* next one to hand out */
} hw_state;

static int hw_read(gpio_backend *b, unsigned int pin)
{
    hw_state *s = b->impl;
    enum gpiod_line_value v = gpiod_line_request_get_value(s->req, pin);
    if (v == GPIOD_LINE_VALUE_ERROR)
        return -1;
    return v == GPIOD_LINE_VALUE_ACTIVE;
}

static int hw_write(gpio_backend *b, unsigned int pin, int value)
{
    hw_state *s = b->impl;
    return gpiod_line_request_set_value(
        s->req, pin, value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

static int hw_wait_edge(gpio_backend *b, int64_t timeout_ns, gpio_edge *out)
{
    hw_state *s = b->impl;
    if (!s->evbuf)
        return -1; /* opened in polling mode */

    if (s->ev_next >= s->ev_count) {
        int r = gpiod_line_request_wait_edge_events(s->req, timeout_ns);
        if (r <= 0)
            return r;
        r = gpiod_line_request_read_edge_events(s->req, s->evbuf, EDGE_BUF_CAP);
        if (r < 0)
            return -1;
        s->ev_count = r;
        s->ev_next = 0;
        if (r == 0)
            return 0;
    }

    struct gpiod_edge_event *ev = gpiod_edge_event_buffer_get_event(s->evbuf, s->ev_next++);
    out->pin = gpiod_edge_event_get_line_offset(ev);
    out->type = gpiod_edge_event_get_event_type(ev) == GPIOD_EDGE_EVENT_RISING_EDGE
                    ? GPIO_EDGE_RISING
                    : GPIO_EDGE_FALLING;
    out->timestamp_ns = gpiod_edge_event_get_timestamp_ns(ev);
    return 1;
}

static void hw_close(gpio_backend *b)
{
    hw_state *s = b->impl;
    if (s->evbuf)
        gpiod_edge_event_buffer_free(s->evbuf);
    if (s->req)
        gpiod_line_request_release(s->req);
    if (s->chip)
        gpiod_chip_close(s->chip);
    free(s);
    free(b);
}

gpio_backend *gpio_libgpiod_open(const char *chip_path, unsigned int pir_pin, bool use_edges)
{
    const unsigned int inputs[] = {pir_pin, PIN_ECHO};
    const unsigned int outputs[] = {PIN_TRIG};
    return gpio_libgpiod_open_lines(chip_path, inputs, 2, outputs, 1, use_edges);
}

gpio_backend *gpio_libgpiod_open_lines(const char *chip_path, const unsigned int *inputs,
                                       size_t n_inputs, const unsigned int *outputs,
                                       size_t n_outputs, bool use_edges)
{

    gpio_backend *b = calloc(1, sizeof *b);
    hw_state *s = calloc(1, sizeof *s);
    struct gpiod_line_settings *in_set = gpiod_line_settings_new();
    struct gpiod_line_settings *out_set = gpiod_line_settings_new();
    struct gpiod_line_config *lcfg = gpiod_line_config_new();
    struct gpiod_request_config *rcfg = gpiod_request_config_new();
    bool ok = b && s && in_set && out_set && lcfg && rcfg;

    if (ok) {
        b->impl = s;
        s->chip = gpiod_chip_open(chip_path);
        if (!s->chip) {
            perror(chip_path);
            ok = false;
        }
    }
    if (ok) {
        gpiod_line_settings_set_direction(in_set, GPIOD_LINE_DIRECTION_INPUT);
        /* The PIR drives OUT high on motion but does not drive it low at rest,
         * so without a pull-down the line floats. Harmless for ECHO (already
         * 2.2 kΩ to ground) and for driven lines such as the benchmark loopback. */
        gpiod_line_settings_set_bias(in_set, GPIOD_LINE_BIAS_PULL_DOWN);
        if (use_edges) {
            gpiod_line_settings_set_edge_detection(in_set, GPIOD_LINE_EDGE_BOTH);
            gpiod_line_settings_set_event_clock(in_set, GPIOD_LINE_CLOCK_MONOTONIC);
        }
        gpiod_line_settings_set_direction(out_set, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(out_set, GPIOD_LINE_VALUE_INACTIVE);
        ok = (n_inputs == 0 ||
              gpiod_line_config_add_line_settings(lcfg, inputs, n_inputs, in_set) == 0) &&
             (n_outputs == 0 ||
              gpiod_line_config_add_line_settings(lcfg, outputs, n_outputs, out_set) == 0);
        gpiod_request_config_set_consumer(rcfg, "crop_guard");
    }
    if (ok) {
        s->req = gpiod_chip_request_lines(s->chip, rcfg, lcfg);
        if (!s->req) {
            perror("gpiod_chip_request_lines");
            ok = false;
        }
    }
    if (ok && use_edges && n_inputs > 0) {
        s->evbuf = gpiod_edge_event_buffer_new(EDGE_BUF_CAP);
        ok = s->evbuf != NULL;
    }

    gpiod_request_config_free(rcfg);
    gpiod_line_config_free(lcfg);
    gpiod_line_settings_free(out_set);
    gpiod_line_settings_free(in_set);

    if (!ok) {
        if (b && s)
            hw_close(b);
        else {
            free(s);
            free(b);
        }
        return NULL;
    }

    b->name = use_edges ? "libgpiod-irq" : "libgpiod-poll";
    b->read = hw_read;
    b->write = hw_write;
    b->wait_edge = hw_wait_edge;
    b->close = hw_close;
    return b;
}
