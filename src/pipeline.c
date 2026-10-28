#include "pipeline.h"

#include "csv.h"
#include "hcsr04.h"
#include "pir.h"

#include <errno.h>
#include <stdio.h>

/* Upper bound on one blocking wait in irq mode, so *stop and the duration are
 * re-checked even when no edges arrive. */
#define IRQ_WAIT_MAX_NS 100000000LL

typedef struct {
    const pipeline_config *cfg;
    const mono_clock      *clk;
    csv_store             *store;
    pipeline_stats        *stats;
} pipe_ctx;

const char *pipeline_mode_name(pipe_mode mode)
{
    return mode == PIPE_IRQ ? "irq" : "poll";
}

static uint64_t now_ns(const pipe_ctx *p)
{
    return p->clk->now(p->clk->ctx);
}

/* Fills in the common fields, then appends r to storage (and stdout). */
static void store_event(pipe_ctx *p, event_record *r)
{
    r->mode = pipeline_mode_name(p->cfg->mode);
    p->clk->wall(p->clk->ctx, &r->t_wall);

    if (csv_store_append(p->store, r) == 0) {
        p->stats->events++;
    } else {
        if (p->stats->store_errors++ == 0)
            perror("csv_store_append");
    }

    if (p->cfg->print_events) {
        char row[256];
        if (csv_format_record(row, sizeof row, r) > 0)
            fputs(row, stdout);
        fflush(stdout);
    }
}

static int64_t latency(uint64_t edge_ns, uint64_t handled_ns, bool known)
{
    return known && handled_ns >= edge_ns ? (int64_t)(handled_ns - edge_ns) : -1;
}

/* latency_known is false in poll mode: the true edge time lies somewhere in
 * the preceding sampling interval, so the edge-to-handler latency is unknown
 * here (Phase 5 measures it against a ground-truth stimulus). */
static void emit_motion(pipe_ctx *p, pir_event ev, uint64_t edge_ns, uint64_t handled_ns,
                        bool latency_known)
{
    if (ev == PIR_NONE)
        return;
    event_record r = {
        .t_mono_ns = edge_ns,
        .kind = ev == PIR_MOTION_START ? EV_MOTION_START : EV_MOTION_END,
        .latency_ns = latency(edge_ns, handled_ns, latency_known),
    };
    store_event(p, &r);
}

/* edge_ns is the ECHO falling edge that completed the measurement, or the
 * time a timeout was detected. */
static void emit_distance(pipe_ctx *p, const hcsr04_reading *rd, uint64_t edge_ns,
                          uint64_t handled_ns, bool latency_known)
{
    bool timeout = rd->status == HCSR04_TIMEOUT;
    p->stats->ranges++;
    if (timeout)
        p->stats->range_timeouts++;
    event_record r = {
        .t_mono_ns = edge_ns,
        .kind = EV_DISTANCE,
        .distance_cm = timeout ? -1.0 : rd->cm,
        .echo_ok = rd->status == HCSR04_OK,
        .latency_ns = latency(edge_ns, handled_ns, latency_known && !timeout),
    };
    store_event(p, &r);
}

/* Sends the TRIG pulse. *trig_ns is when TRIG fell, which starts the
 * measurement. */
static int trigger(pipe_ctx *p, gpio_backend *b, uint64_t *trig_ns)
{
    if (b->write(b, PIN_TRIG, 1) < 0)
        return -1;
    uint64_t until = now_ns(p) + HCSR04_TRIG_PULSE_NS;
    while (now_ns(p) < until)
        p->clk->sleep_until(p->clk->ctx, until); /* repeat if a signal cut it short */
    if (b->write(b, PIN_TRIG, 0) < 0)
        return -1;
    *trig_ns = now_ns(p);
    return 0;
}

static bool should_stop(const pipeline_config *cfg, volatile sig_atomic_t *stop,
                        uint64_t start, uint64_t now)
{
    return (stop && *stop) || (cfg->duration_ns && now - start >= cfg->duration_ns);
}

static int64_t min_wait(int64_t timeout, uint64_t limit, uint64_t now)
{
    int64_t until = limit > now ? (int64_t)(limit - now) : 0;
    return until < timeout ? until : timeout;
}

static int run_irq(pipe_ctx *p, gpio_backend *b, volatile sig_atomic_t *stop)
{
    const pipeline_config *cfg = p->cfg;
    uint64_t start = now_ns(p);
    uint64_t edge_ns;
    pir_state pir;
    hcsr04_tracker rng;
    hcsr04_reading rd;
    bool ranging = cfg->range_interval_ns > 0;
    uint64_t next_trig = start + HCSR04_SETTLE_NS;

    int level = b->read(b, PIN_PIR);
    if (level < 0)
        return -1;
    pir_init(&pir, cfg->debounce_ns, level);
    hcsr04_init(&rng);

    for (;;) {
        uint64_t now = now_ns(p);
        if (should_stop(cfg, stop, start, now))
            break;

        if (ranging && rng.phase == HCSR04_IDLE && now >= next_trig) {
            uint64_t trig;
            if (trigger(p, b, &trig) < 0) {
                perror("TRIG write");
                return -1;
            }
            hcsr04_start(&rng, trig);
            next_trig = trig + cfg->range_interval_ns;
            now = now_ns(p);
        }

        /* Wake for the earliest of: debounce deadline, echo timeout, next
         * trigger, end of run, max wait. */
        int64_t timeout = IRQ_WAIT_MAX_NS;
        uint64_t limit;
        if (pir_deadline(&pir, &limit))
            timeout = min_wait(timeout, limit, now);
        if (hcsr04_deadline(&rng, &limit))
            timeout = min_wait(timeout, limit, now);
        else if (ranging)
            timeout = min_wait(timeout, next_trig, now);
        if (cfg->duration_ns)
            timeout = min_wait(timeout, start + cfg->duration_ns, now);

        gpio_edge e;
        int r = b->wait_edge(b, timeout, &e);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            perror("wait_edge");
            return -1;
        }
        uint64_t handled = now_ns(p);

        if (r == 1 && e.pin == PIN_PIR) {
            p->stats->pir_edges++;
            pir_event ev = pir_on_level(&pir, e.type == GPIO_EDGE_RISING, e.timestamp_ns,
                                        &edge_ns);
            emit_motion(p, ev, edge_ns, handled, true);
        } else if (r == 1 && e.pin == PIN_ECHO) {
            p->stats->echo_edges++;
            if (hcsr04_on_edge(&rng, e.type, e.timestamp_ns, &rd))
                emit_distance(p, &rd, e.timestamp_ns, handled, true);
        }
        emit_motion(p, pir_tick(&pir, handled, &edge_ns), edge_ns, handled, true);
        if (hcsr04_check_timeout(&rng, handled, &rd))
            emit_distance(p, &rd, handled, handled, false);
    }
    return 0;
}

/* Poll-mode ranging: busy-reads ECHO from the trigger until the echo ends or
 * times out. PIR sampling stalls meanwhile (up to ~60 ms); that CPU and
 * responsiveness cost is what polling pays compared with edge events. */
static int measure_polled(pipe_ctx *p, gpio_backend *b, volatile sig_atomic_t *stop,
                          uint64_t *trig_ns)
{
    hcsr04_tracker rng;
    hcsr04_reading rd;

    int last = b->read(b, PIN_ECHO);
    if (last < 0 || trigger(p, b, trig_ns) < 0)
        return -1;
    hcsr04_start(&rng, *trig_ns);

    while (!(stop && *stop)) {
        int level = b->read(b, PIN_ECHO);
        uint64_t t = now_ns(p);
        if (level < 0)
            return -1;
        p->stats->echo_samples++;
        if (level != last) {
            last = level;
            if (hcsr04_on_edge(&rng, level ? GPIO_EDGE_RISING : GPIO_EDGE_FALLING, t, &rd)) {
                emit_distance(p, &rd, t, t, false);
                return 0;
            }
        }
        if (hcsr04_check_timeout(&rng, t, &rd)) {
            emit_distance(p, &rd, t, t, false);
            return 0;
        }
    }
    return 0;
}

static int run_poll(pipe_ctx *p, gpio_backend *b, volatile sig_atomic_t *stop)
{
    const pipeline_config *cfg = p->cfg;
    uint64_t start = now_ns(p);
    uint64_t next = start;
    uint64_t next_trig = start + HCSR04_SETTLE_NS;
    uint64_t edge_ns;
    pir_state pir;

    int level = b->read(b, PIN_PIR);
    if (level < 0)
        return -1;
    pir_init(&pir, cfg->debounce_ns, level);

    for (;;) {
        uint64_t now = now_ns(p);
        if (should_stop(cfg, stop, start, now))
            break;

        level = b->read(b, PIN_PIR);
        if (level < 0) {
            perror("gpio read");
            return -1;
        }
        uint64_t sampled = now_ns(p);
        p->stats->samples++;
        emit_motion(p, pir_on_level(&pir, level, sampled, &edge_ns), edge_ns, sampled, false);
        emit_motion(p, pir_tick(&pir, sampled, &edge_ns), edge_ns, sampled, false);

        if (cfg->range_interval_ns && sampled >= next_trig) {
            uint64_t trig;
            if (measure_polled(p, b, stop, &trig) < 0) {
                perror("HC-SR04 poll");
                return -1;
            }
            next_trig = trig + cfg->range_interval_ns;
        }

        next += cfg->poll_interval_ns;
        now = now_ns(p);
        if (next < now) {
            p->stats->overruns++;
            next = now;
        }
        p->clk->sleep_until(p->clk->ctx, next); /* EINTR: loop re-checks *stop */
    }
    return 0;
}

int pipeline_run(const pipeline_config *cfg, gpio_backend *b, const mono_clock *clk,
                 csv_store *store, volatile sig_atomic_t *stop, pipeline_stats *stats)
{
    *stats = (pipeline_stats){0};
    pipe_ctx p = {.cfg = cfg, .clk = clk, .store = store, .stats = stats};
    return cfg->mode == PIPE_IRQ ? run_irq(&p, b, stop) : run_poll(&p, b, stop);
}
