#include "pipeline.h"

#include "csv.h"
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
        .mode = pipeline_mode_name(p->cfg->mode),
        .kind = ev == PIR_MOTION_START ? EV_MOTION_START : EV_MOTION_END,
        .latency_ns = latency_known && handled_ns >= edge_ns ? (int64_t)(handled_ns - edge_ns)
                                                             : -1,
    };
    p->clk->wall(p->clk->ctx, &r.t_wall);

    if (csv_store_append(p->store, &r) == 0) {
        p->stats->events++;
    } else {
        if (p->stats->store_errors++ == 0)
            perror("csv_store_append");
    }

    if (p->cfg->print_events) {
        char row[256];
        if (csv_format_record(row, sizeof row, &r) > 0)
            fputs(row, stdout);
        fflush(stdout);
    }
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
    const mono_clock *clk = p->clk;
    uint64_t start = clk->now(clk->ctx);
    uint64_t edge_ns;
    pir_state pir;

    int level = b->read(b, PIN_PIR);
    if (level < 0)
        return -1;
    pir_init(&pir, p->cfg->debounce_ns, level);

    for (;;) {
        uint64_t now = clk->now(clk->ctx);
        if (should_stop(p->cfg, stop, start, now))
            break;

        /* Wake for the earliest of: debounce deadline, end of run, max wait. */
        int64_t timeout = IRQ_WAIT_MAX_NS;
        uint64_t limit;
        if (pir_deadline(&pir, &limit))
            timeout = min_wait(timeout, limit, now);
        if (p->cfg->duration_ns)
            timeout = min_wait(timeout, start + p->cfg->duration_ns, now);

        gpio_edge e;
        int r = b->wait_edge(b, timeout, &e);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            perror("wait_edge");
            return -1;
        }
        uint64_t handled = clk->now(clk->ctx);

        if (r == 1 && e.pin == PIN_PIR) {
            p->stats->pir_edges++;
            pir_event ev = pir_on_level(&pir, e.type == GPIO_EDGE_RISING, e.timestamp_ns,
                                        &edge_ns);
            emit_motion(p, ev, edge_ns, handled, true);
        }
        emit_motion(p, pir_tick(&pir, handled, &edge_ns), edge_ns, handled, true);
    }
    return 0;
}

static int run_poll(pipe_ctx *p, gpio_backend *b, volatile sig_atomic_t *stop)
{
    const mono_clock *clk = p->clk;
    uint64_t start = clk->now(clk->ctx);
    uint64_t next = start;
    uint64_t edge_ns;
    pir_state pir;

    int level = b->read(b, PIN_PIR);
    if (level < 0)
        return -1;
    pir_init(&pir, p->cfg->debounce_ns, level);

    for (;;) {
        uint64_t now = clk->now(clk->ctx);
        if (should_stop(p->cfg, stop, start, now))
            break;

        level = b->read(b, PIN_PIR);
        if (level < 0) {
            perror("gpio read");
            return -1;
        }
        uint64_t sampled = clk->now(clk->ctx);
        p->stats->samples++;
        emit_motion(p, pir_on_level(&pir, level, sampled, &edge_ns), edge_ns, sampled, false);
        emit_motion(p, pir_tick(&pir, sampled, &edge_ns), edge_ns, sampled, false);

        next += p->cfg->poll_interval_ns;
        now = clk->now(clk->ctx);
        if (next < now) {
            p->stats->overruns++;
            next = now;
        }
        clk->sleep_until(clk->ctx, next); /* EINTR: loop re-checks *stop */
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
