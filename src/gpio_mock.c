/* Mock GPIO backend: line levels in memory, a script of timestamped edges, and
 * a virtual clock. No hardware access. */
#include "gpio_mock.h"

#include <stdlib.h>

#define MOCK_MAX_PIN 64

typedef struct {
    int        level[MOCK_MAX_PIN];
    unsigned   writes[MOCK_MAX_PIN];
    gpio_edge *edges;
    size_t     count, cap;
    size_t     applied;   /* edges whose level change has taken effect */
    size_t     delivered; /* edges handed out by wait_edge */
    uint64_t   now_ns;
    uint64_t   irq_latency_ns;
} mock_state;

/* Apply every scripted level change that has happened by the current time. */
static void apply_due(mock_state *s)
{
    while (s->applied < s->count && s->edges[s->applied].timestamp_ns <= s->now_ns) {
        const gpio_edge *e = &s->edges[s->applied++];
        s->level[e->pin] = e->type == GPIO_EDGE_RISING;
    }
}

static void advance_to(mock_state *s, uint64_t t_ns)
{
    if (t_ns > s->now_ns)
        s->now_ns = t_ns;
    apply_due(s);
}

static int mock_read(gpio_backend *b, unsigned int pin)
{
    mock_state *s = b->impl;
    apply_due(s);
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
    mock_state *s = b->impl;
    if (s->delivered == s->count ||
        (timeout_ns >= 0 && s->edges[s->delivered].timestamp_ns > s->now_ns + (uint64_t)timeout_ns)) {
        if (timeout_ns > 0)
            advance_to(s, s->now_ns + (uint64_t)timeout_ns);
        return 0;
    }
    *out = s->edges[s->delivered++];
    advance_to(s, out->timestamp_ns);
    advance_to(s, s->now_ns + s->irq_latency_ns);
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
    if (pin >= MOCK_MAX_PIN || (s->count && timestamp_ns < s->edges[s->count - 1].timestamp_ns))
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

void gpio_mock_set_irq_latency(gpio_backend *b, uint64_t latency_ns)
{
    mock_state *s = b->impl;
    s->irq_latency_ns = latency_ns;
}

size_t gpio_mock_pending(const gpio_backend *b)
{
    const mock_state *s = b->impl;
    return s->count - s->delivered;
}

unsigned gpio_mock_write_count(const gpio_backend *b, unsigned int pin)
{
    const mock_state *s = b->impl;
    return pin < MOCK_MAX_PIN ? s->writes[pin] : 0;
}

uint64_t gpio_mock_now(const gpio_backend *b)
{
    const mock_state *s = b->impl;
    return s->now_ns;
}

static uint64_t clock_now(void *ctx)
{
    return gpio_mock_now(ctx);
}

static int clock_sleep_until(void *ctx, uint64_t t_ns)
{
    gpio_backend *b = ctx;
    advance_to(b->impl, t_ns);
    return 0;
}

static void clock_wall(void *ctx, struct timespec *ts)
{
    uint64_t now = gpio_mock_now(ctx);
    ts->tv_sec = (time_t)(now / 1000000000ULL);
    ts->tv_nsec = (long)(now % 1000000000ULL);
}

mono_clock gpio_mock_clock(gpio_backend *b)
{
    return (mono_clock){
        .now = clock_now,
        .sleep_until = clock_sleep_until,
        .wall = clock_wall,
        .ctx = b,
    };
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
