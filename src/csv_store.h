/* Append-only CSV event storage. Each row is written with a single write()
 * and fsync()ed, so a crash or power loss loses at most the row in flight. */
#ifndef CSV_STORE_H
#define CSV_STORE_H

#include "event.h"

typedef struct {
    int fd;
} csv_store;

/* Opens (creating if needed) path for appending. Writes CSV_HEADER if the file
 * is empty. Returns 0, or -1 with errno set. */
int csv_store_open(csv_store *s, const char *path);

/* Returns 0, or -1 on formatting or I/O failure (errno set for I/O). */
int csv_store_append(csv_store *s, const event_record *r);

void csv_store_close(csv_store *s);

#endif
