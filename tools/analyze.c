/* Latency analysis for the interrupt vs. polling benchmark. Reads the
 * stimulus log and the receiver's event CSV (the real pipeline's storage),
 * pairs them, and writes per-edge latencies, a histogram, and one summary row.
 * Handler time is t_mono_ns + latency_ns for irq rows (kernel edge timestamp
 * plus kernel-to-handler delay) and t_mono_ns for poll rows (the sample that
 * saw the change). */
#include "latency_match.h"
#include "metrics.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LINE_MAX_LEN 512

typedef struct {
    void  *data;
    size_t n, cap, elem;
} vec;

static void *vec_push(vec *v)
{
    if (v->n == v->cap) {
        size_t cap = v->cap ? v->cap * 2 : 1024;
        void *d = realloc(v->data, cap * v->elem);
        if (!d) {
            fprintf(stderr, "analyze: out of memory\n");
            exit(1);
        }
        v->data = d;
        v->cap = cap;
    }
    return (char *)v->data + v->elem * v->n++;
}

/* Splits a CSV line (no quoted fields in our files) in place. */
static int split(char *line, char **f, int max)
{
    int n = 0;
    line[strcspn(line, "\r\n")] = '\0';
    for (char *p = line; n < max; n++) {
        f[n] = p;
        p = strchr(p, ',');
        if (!p)
            return n + 1;
        *p++ = '\0';
    }
    return n;
}

static int load_stim(const char *path, vec *stim)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }
    char line[LINE_MAX_LEN], *fl[3];
    if (!fgets(line, sizeof line, f)) { /* header */
        fclose(f);
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        if (split(line, fl, 3) != 3)
            continue;
        stim_edge *e = vec_push(stim);
        e->t_ns = strtoull(fl[0], NULL, 10);
        e->level = atoi(fl[2]);
    }
    fclose(f);
    return 0;
}

/* Fills ev with handler times and t_mono (edge time as recorded) in kmono. */
static int load_events(const char *path, vec *ev, vec *kmono, vec *handler_lat)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }
    char line[LINE_MAX_LEN], *fl[7];
    if (!fgets(line, sizeof line, f)) {
        fclose(f);
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        if (split(line, fl, 7) != 7)
            continue;
        int level;
        if (strcmp(fl[3], "motion_start") == 0)
            level = 1;
        else if (strcmp(fl[3], "motion_end") == 0)
            level = 0;
        else
            continue;
        uint64_t t_mono = strtoull(fl[0], NULL, 10);
        int64_t lat = fl[6][0] ? strtoll(fl[6], NULL, 10) : -1;
        recv_event *e = vec_push(ev);
        e->handled_ns = t_mono + (lat > 0 ? (uint64_t)lat : 0);
        e->level = level;
        *(uint64_t *)vec_push(kmono) = t_mono;
        *(int64_t *)vec_push(handler_lat) = lat;
    }
    fclose(f);
    return 0;
}

/* Bin width: p99 / 25, rounded up to 1, 2 or 5 x 10^k ns. */
static int64_t nice_bin_width(int64_t p99)
{
    int64_t raw = p99 / 25 > 1 ? p99 / 25 : 1;
    for (int64_t mag = 1;; mag *= 10)
        for (int m = 1; m <= 5; m += (m == 1 ? 1 : 3))
            if (m * mag >= raw)
                return m * mag;
}

static const char *opt(const char *arg, const char *name)
{
    size_t n = strlen(name);
    return strncmp(arg, name, n) == 0 && arg[n] == '=' ? arg + n + 1 : NULL;
}

int main(int argc, char **argv)
{
    const char *stim_path = NULL, *ev_path = NULL, *label = NULL, *lat_out = NULL,
               *hist_out = NULL, *summary = NULL, *load_cpu = "", *idle_cpu = "";
    for (int i = 1; i < argc; i++) {
        const char *v;
        if ((v = opt(argv[i], "--stim")))
            stim_path = v;
        else if ((v = opt(argv[i], "--events")))
            ev_path = v;
        else if ((v = opt(argv[i], "--label")))
            label = v;
        else if ((v = opt(argv[i], "--latency-out")))
            lat_out = v;
        else if ((v = opt(argv[i], "--hist-out")))
            hist_out = v;
        else if ((v = opt(argv[i], "--summary")))
            summary = v;
        else if ((v = opt(argv[i], "--load-cpu-pct")))
            load_cpu = v;
        else if ((v = opt(argv[i], "--idle-cpu-pct")))
            idle_cpu = v;
        else {
            fprintf(stderr,
                    "usage: %s --stim=PATH --events=PATH --label=NAME [--latency-out=PATH]\n"
                    "          [--hist-out=PATH] [--summary=PATH] [--load-cpu-pct=X]\n"
                    "          [--idle-cpu-pct=X]\n",
                    argv[0]);
            return 2;
        }
    }
    if (!stim_path || !ev_path || !label) {
        fprintf(stderr, "analyze: --stim, --events and --label are required\n");
        return 2;
    }

    vec stim = {.elem = sizeof(stim_edge)}, ev = {.elem = sizeof(recv_event)};
    vec kmono = {.elem = sizeof(uint64_t)}, hlat = {.elem = sizeof(int64_t)};
    if (load_stim(stim_path, &stim) < 0 || load_events(ev_path, &ev, &kmono, &hlat) < 0)
        return 1;
    if (stim.n == 0) {
        fprintf(stderr, "analyze: no stimulus edges in %s\n", stim_path);
        return 1;
    }

    int64_t *lat = malloc(stim.n * sizeof *lat);
    size_t *idx = malloc(stim.n * sizeof *idx);
    int64_t *ok = malloc(stim.n * sizeof *ok);
    if (!lat || !idx || !ok)
        return 1;
    match_result m;
    latency_match(stim.data, stim.n, ev.data, ev.n, lat, idx, &m);

    const stim_edge *se = stim.data;
    FILE *lf = lat_out ? fopen(lat_out, "w") : NULL;
    if (lf)
        fprintf(lf, "stim_ns,level,latency_ns,edge_detect_ns,handler_ns\n");
    size_t n_ok = 0;
    for (size_t i = 0; i < stim.n; i++) {
        if (lat[i] >= 0)
            ok[n_ok++] = lat[i];
        if (!lf)
            continue;
        if (lat[i] < 0) {
            fprintf(lf, "%" PRIu64 ",%d,,,\n", se[i].t_ns, se[i].level);
            continue;
        }
        /* irq: split total into stimulus->kernel timestamp and kernel->handler. */
        int64_t h = ((int64_t *)hlat.data)[idx[i]];
        uint64_t km = ((uint64_t *)kmono.data)[idx[i]];
        if (h >= 0)
            fprintf(lf, "%" PRIu64 ",%d,%" PRId64 ",%" PRId64 ",%" PRId64 "\n", se[i].t_ns,
                    se[i].level, lat[i], (int64_t)(km - se[i].t_ns), h);
        else
            fprintf(lf, "%" PRIu64 ",%d,%" PRId64 ",,\n", se[i].t_ns, se[i].level, lat[i]);
    }
    if (lf)
        fclose(lf);

    latency_summary s = {0};
    if (n_ok == 0 || latency_summarize(ok, n_ok, &s) < 0) {
        fprintf(stderr, "analyze: %s: no matched edges (matched=%zu missed=%zu)\n", label,
                m.matched, m.missed);
        return 1;
    }

    if (hist_out) {
        enum { NBINS = 40 };
        int64_t w = nice_bin_width(s.p99);
        uint64_t counts[NBINS];
        latency_histogram(ok, n_ok, w, counts, NBINS);
        FILE *hf = fopen(hist_out, "w");
        if (hf) {
            fprintf(hf, "bin_lo_us,bin_hi_us,count\n");
            for (int b = 0; b < NBINS; b++) {
                if (b == NBINS - 1)
                    fprintf(hf, "%.3f,,%" PRIu64 "\n", (double)(b * w) / 1000.0, counts[b]);
                else
                    fprintf(hf, "%.3f,%.3f,%" PRIu64 "\n", (double)(b * w) / 1000.0,
                            (double)((b + 1) * w) / 1000.0, counts[b]);
            }
            fclose(hf);
        }
    }

    if (summary) {
        struct stat st;
        bool fresh = stat(summary, &st) != 0 || st.st_size == 0;
        FILE *sf = fopen(summary, "a");
        if (sf) {
            if (fresh)
                fprintf(sf, "label,stim_edges,matched,missed,spurious,min_us,p50_us,p90_us,"
                            "p99_us,max_us,mean_us,load_cpu_pct,idle_cpu_pct\n");
            fprintf(sf, "%s,%zu,%zu,%zu,%zu,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%s\n", label,
                    stim.n, m.matched, m.missed, m.spurious, (double)s.min / 1e3,
                    (double)s.p50 / 1e3, (double)s.p90 / 1e3, (double)s.p99 / 1e3,
                    (double)s.max / 1e3, s.mean / 1e3, load_cpu, idle_cpu);
            fclose(sf);
        }
    }

    printf("%-12s n=%zu matched=%zu missed=%zu spurious=%zu  latency us: min %.1f  p50 %.1f"
           "  p90 %.1f  p99 %.1f  max %.1f  mean %.1f\n",
           label, stim.n, m.matched, m.missed, m.spurious, (double)s.min / 1e3,
           (double)s.p50 / 1e3, (double)s.p90 / 1e3, (double)s.p99 / 1e3, (double)s.max / 1e3,
           s.mean / 1e3);
    free(lat);
    free(idx);
    free(ok);
    free(stim.data);
    free(ev.data);
    free(kmono.data);
    free(hlat.data);
    return 0;
}
