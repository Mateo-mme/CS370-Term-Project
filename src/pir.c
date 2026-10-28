#include "pir.h"

static pir_event commit(pir_state *s, int level)
{
    s->level = level;
    s->pending = false;
    return level ? PIR_MOTION_START : PIR_MOTION_END;
}

void pir_init(pir_state *s, uint64_t debounce_ns, int initial_level)
{
    *s = (pir_state){.debounce_ns = debounce_ns, .level = initial_level ? 1 : 0};
}

pir_event pir_tick(pir_state *s, uint64_t now_ns, uint64_t *event_ns)
{
    if (!s->pending || now_ns < s->pending_ns || now_ns - s->pending_ns < s->debounce_ns)
        return PIR_NONE;
    *event_ns = s->pending_ns;
    return commit(s, s->pending_level);
}

pir_event pir_on_level(pir_state *s, int level, uint64_t ts_ns, uint64_t *event_ns)
{
    level = level ? 1 : 0;

    /* A pending change that already outlasted the debounce window is real,
     * even if the line moves again now. With debounce > 0 the new level can
     * only become pending below, so at most one event is returned. */
    pir_event ev = pir_tick(s, ts_ns, event_ns);

    if (s->pending) {
        if (level == s->level)
            s->pending = false; /* glitch: returned before it was stable */
        return ev;
    }
    if (level == s->level)
        return ev;
    if (s->debounce_ns == 0) {
        *event_ns = ts_ns;
        return commit(s, level);
    }
    s->pending = true;
    s->pending_level = level;
    s->pending_ns = ts_ns;
    return ev;
}

bool pir_deadline(const pir_state *s, uint64_t *deadline_ns)
{
    if (!s->pending)
        return false;
    *deadline_ns = s->pending_ns + s->debounce_ns;
    return true;
}
