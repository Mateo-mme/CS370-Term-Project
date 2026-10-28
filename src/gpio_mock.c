/* Mock GPIO backend. Phase 0 stub: holds line levels in memory; scripted
 * edge sequences are added in Phase 1. */
#include "gpio_backend.h"

#include <stdlib.h>

#define MOCK_MAX_PIN 64

typedef struct {
    int level[MOCK_MAX_PIN];
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
    return 0;
}

static int mock_wait_edge(gpio_backend *b, int64_t timeout_ns, gpio_edge *out)
{
    (void)b;
    (void)timeout_ns;
    (void)out;
    return 0;
}

static void mock_close(gpio_backend *b)
{
    free(b->impl);
    free(b);
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
