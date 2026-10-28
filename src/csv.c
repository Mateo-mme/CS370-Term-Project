#include "csv.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int csv_escape_field(char *dst, size_t n, const char *src)
{
    bool quote = strpbrk(src, ",\"\r\n") != NULL;
    size_t len = 0;

#define PUT(c)            \
    do {                  \
        if (len + 1 >= n) \
            return -1;    \
        dst[len++] = (c); \
    } while (0)

    if (quote)
        PUT('"');
    for (const char *p = src; *p; p++) {
        if (*p == '"')
            PUT('"');
        PUT(*p);
    }
    if (quote)
        PUT('"');
#undef PUT

    if (n == 0)
        return -1;
    dst[len] = '\0';
    return (int)len;
}

int csv_format_wall_time(char *dst, size_t n, struct timespec ts)
{
    struct tm tm;
    if (!gmtime_r(&ts.tv_sec, &tm))
        return -1;
    char date[32];
    if (strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%S", &tm) == 0)
        return -1;
    int len = snprintf(dst, n, "%s.%06ldZ", date, ts.tv_nsec / 1000);
    return (len < 0 || (size_t)len >= n) ? -1 : len;
}

const char *csv_event_name(event_kind kind)
{
    switch (kind) {
    case EV_MOTION_START:
        return "motion_start";
    case EV_MOTION_END:
        return "motion_end";
    case EV_DISTANCE:
        return "distance";
    }
    return "unknown";
}

int csv_format_record(char *dst, size_t n, const event_record *r)
{
    char wall[40], mode[64], dist[32] = "", echo[4] = "", lat[24] = "";

    if (csv_format_wall_time(wall, sizeof wall, r->t_wall) < 0 ||
        csv_escape_field(mode, sizeof mode, r->mode ? r->mode : "") < 0)
        return -1;
    if (r->kind == EV_DISTANCE) {
        if (r->distance_cm >= 0)
            snprintf(dist, sizeof dist, "%.2f", r->distance_cm);
        snprintf(echo, sizeof echo, "%d", r->echo_ok ? 1 : 0);
    }
    if (r->latency_ns >= 0)
        snprintf(lat, sizeof lat, "%" PRId64, r->latency_ns);

    int len = snprintf(dst, n, "%" PRIu64 ",%s,%s,%s,%s,%s,%s\n", r->t_mono_ns, wall, mode,
                       csv_event_name(r->kind), dist, echo, lat);
    return (len < 0 || (size_t)len >= n) ? -1 : len;
}
