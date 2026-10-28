/* Mock GPIO backend: line levels in memory plus a FIFO of scripted edges with
 * injected timestamps. No hardware access. */
#include "gpio_mock.h"

#include <stdlib.h>

#define MOCK_MAX_PIN 64

typedef struct {
    int        level[MOCK_MAX_PIN];
    unsigned   writes[MOCK_MAX_PIN];
    gpio_edge *edges;
    size_t     head, count, cap;
} mock_state;

static int mock_read(gpio_backend *b, unsigned int pin)
{
    mock_state *s = b->impl;
    return pin < MOCK_MAX_PIN ? s->level[pin] : -1;
}

static int mock_write(gpio_backend *b, unsigned int pin, int value)
{
    mock_state *s = b->impl;
    if (pin >= MOCK_MAX_PIN)
        return -1;
    s->level[pin] = value ? 1 : 0;
    s->writes[pin]++;
    return 0;
}

static int mock_wait_edge(gpio_backend *b, int64_t timeout_ns, gpio_edge *out)
{
    (void)timeout_ns; /* scripted time: an empty queue is an immediate timeout */
    mock_state *s = b->impl;
    if (s->head == s->count)
        return 0;
    *out = s->edges[s->head++];
    s->level[out->pin] = out->type == GPIO_EDGE_RISING;
    return 1;
}

static void mock_close(gpio_backend *b)
{
    mock_state *s = b->impl;
    free(s->edges);
    free(s);
    free(b);
}

int gpio_mock_push_edge(gpio_backend *b, unsigned int pin, gpio_edge_type type,
                        uint64_t timestamp_ns)
{
    mock_state *s = b->impl;
    if (pin >= MOCK_MAX_PIN)
        return -1;
    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 16;
        gpio_edge *e = realloc(s->edges, cap * sizeof *e);
        if (!e)
            return -1;
        s->edges = e;
        s->cap = cap;
    }
    s->edges[s->count++] = (gpio_edge){.pin = pin, .type = type, .timestamp_ns = timestamp_ns};
    return 0;
}

int gpio_mock_set_level(gpio_backend *b, unsigned int pin, int level)
{
    mock_state *s = b->impl;
    if (pin >= MOCK_MAX_PIN)
        return -1;
    s->level[pin] = level ? 1 : 0;
    return 0;
}

size_t gpio_mock_pending(const gpio_backend *b)
{
    const mock_state *s = b->impl;
    return s->count - s->head;
}

unsigned gpio_mock_write_count(const gpio_backend *b, unsigned int pin)
{
    const mock_state *s = b->impl;
    return pin < MOCK_MAX_PIN ? s->writes[pin] : 0;
}

gpio_backend *gpio_mock_open(void)
{
    gpio_backend *b = calloc(1, sizeof *b);
    mock_state *s = calloc(1, sizeof *s);
    if (!b || !s) {
        free(b);
        free(s);
        return NULL;
    }
    *b = (gpio_backend){
        .name = "mock",
        .read = mock_read,
        .write = mock_write,
        .wait_edge = mock_wait_edge,
        .close = mock_close,
        .impl = s,
    };
    return b;
}
