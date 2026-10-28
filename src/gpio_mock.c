/* Mock GPIO backend: line levels in memory, a script of timestamped edges, and
 * a virtual clock. No hardware access. */
#include "gpio_mock.h"

#include <stdlib.h>
#include <string.h>

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
    uint64_t   read_cost_ns;
    bool       echo_enabled;
    uint64_t   echo_delay_ns, echo_width_ns;
} mock_state;

static int push_edge(mock_state *s, unsigned int pin, gpio_edge_type type,
                     uint64_t timestamp_ns);

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
    advance_to(s, s->now_ns + s->read_cost_ns);
    return pin < MOCK_MAX_PIN ? s->level[pin] : -1;
}

static int mock_write(gpio_backend *b, unsigned int pin, int value)
{
    mock_state *s = b->impl;
    if (pin >= MOCK_MAX_PIN)
        return -1;
    int was = s->level[pin];
    s->level[pin] = value ? 1 : 0;
    s->writes[pin]++;
    if (pin == PIN_TRIG && was && !value && s->echo_enabled && s->echo_width_ns) {
        uint64_t rise = s->now_ns + s->echo_delay_ns;
        if (push_edge(s, PIN_ECHO, GPIO_EDGE_RISING, rise) < 0 ||
            push_edge(s, PIN_ECHO, GPIO_EDGE_FALLING, rise + s->echo_width_ns) < 0)
            return -1;
    }
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

static int push_edge(mock_state *s, unsigned int pin, gpio_edge_type type,
                     uint64_t timestamp_ns)
{
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
    /* Insert in timestamp order, never before edges already applied or delivered. */
    size_t floor = s->applied > s->delivered ? s->applied : s->delivered;
    size_t i = s->count;
    while (i > floor && s->edges[i - 1].timestamp_ns > timestamp_ns)
        i--;
    memmove(&s->edges[i + 1], &s->edges[i], (s->count - i) * sizeof *s->edges);
    s->edges[i] = (gpio_edge){.pin = pin, .type = type, .timestamp_ns = timestamp_ns};
    s->count++;
    return 0;
}

int gpio_mock_push_edge(gpio_backend *b, unsigned int pin, gpio_edge_type type,
                        uint64_t timestamp_ns)
{
    return push_edge(b->impl, pin, type, timestamp_ns);
}

void gpio_mock_set_read_cost(gpio_backend *b, uint64_t cost_ns)
{
    mock_state *s = b->impl;
    s->read_cost_ns = cost_ns;
}

void gpio_mock_set_echo(gpio_backend *b, uint64_t delay_ns, uint64_t width_ns)
{
    mock_state *s = b->impl;
    s->echo_enabled = true;
    s->echo_delay_ns = delay_ns;
    s->echo_width_ns = width_ns;
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
