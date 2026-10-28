/* CSV formatting for event records (RFC 4180 quoting). File I/O lives in csv_store. */
#ifndef CSV_H
#define CSV_H

#include "event.h"

#include <stddef.h>
#include <time.h>

#define CSV_HEADER "t_mono_ns,t_wall_iso,mode,event,distance_cm,echo_ok,latency_ns\n"

/* Each function writes a NUL-terminated string into dst and returns its length,
 * or -1 if it does not fit in n bytes. */

/* Quotes the field if it contains a comma, quote, CR or LF; doubles inner quotes. */
int csv_escape_field(char *dst, size_t n, const char *src);

/* UTC ISO-8601 with microseconds, e.g. 2026-10-28T17:26:28.123456Z */
int csv_format_wall_time(char *dst, size_t n, struct timespec ts);

const char *csv_event_name(event_kind kind);

/* One CSV row including the trailing newline. Distance and echo_ok are empty
 * for motion events; latency is empty when negative (unknown). */
int csv_format_record(char *dst, size_t n, const event_record *r);

#endif
