#include "hcsr04.h"

double hcsr04_width_to_cm(uint64_t width_ns)
{
    /* Sound travels to the object and back, so halve the round trip. */
    return (double)width_ns * HCSR04_SOUND_CM_PER_NS / 2.0;
}

void hcsr04_init(hcsr04_tracker *t)
{
    *t = (hcsr04_tracker){.phase = HCSR04_IDLE};
}

void hcsr04_start(hcsr04_tracker *t, uint64_t trig_ns)
{
    *t = (hcsr04_tracker){.phase = HCSR04_WAIT_RISE, .trig_ns = trig_ns};
}

bool hcsr04_on_edge(hcsr04_tracker *t, gpio_edge_type type, uint64_t ts_ns,
                    hcsr04_reading *out)
{
    switch (t->phase) {
    case HCSR04_IDLE:
        return false;
    case HCSR04_WAIT_RISE:
        if (type == GPIO_EDGE_RISING) {
            t->rise_ns = ts_ns;
            t->phase = HCSR04_WAIT_FALL;
        }
        return false;
    case HCSR04_WAIT_FALL:
        if (type == GPIO_EDGE_RISING) {
            /* Missed the falling edge; restart from this rise. */
            t->rise_ns = ts_ns;
            return false;
        }
        break;
    }

    uint64_t width = ts_ns >= t->rise_ns ? ts_ns - t->rise_ns : 0;
    double cm = hcsr04_width_to_cm(width);
    *out = (hcsr04_reading){
        .status = (cm < HCSR04_MIN_CM || cm > HCSR04_MAX_CM) ? HCSR04_OUT_OF_RANGE
                                                             : HCSR04_OK,
        .cm = cm,
        .width_ns = width,
        .rise_ns = t->rise_ns,
    };
    t->phase = HCSR04_IDLE;
    return true;
}

bool hcsr04_check_timeout(hcsr04_tracker *t, uint64_t now_ns, hcsr04_reading *out)
{
    uint64_t since;
    switch (t->phase) {
    case HCSR04_WAIT_RISE:
        since = t->trig_ns;
        break;
    case HCSR04_WAIT_FALL:
        since = t->rise_ns;
        break;
    default:
        return false;
    }
    if (now_ns <= since || now_ns - since <= HCSR04_TIMEOUT_NS)
        return false;

    *out = (hcsr04_reading){
        .status = HCSR04_TIMEOUT,
        .rise_ns = t->phase == HCSR04_WAIT_FALL ? t->rise_ns : 0,
    };
    t->phase = HCSR04_IDLE;
    return true;
}
