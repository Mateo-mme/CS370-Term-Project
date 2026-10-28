#include "csv_store.h"

#include "csv.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t w = write(fd, buf, len);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += w;
        len -= (size_t)w;
    }
    return 0;
}

int csv_store_open(csv_store *s, const char *path)
{
    s->fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (s->fd < 0)
        return -1;

    struct stat st;
    if (fstat(s->fd, &st) < 0 ||
        (st.st_size == 0 && (write_all(s->fd, CSV_HEADER, strlen(CSV_HEADER)) < 0 ||
                             fsync(s->fd) < 0))) {
        int saved = errno;
        close(s->fd);
        s->fd = -1;
        errno = saved;
        return -1;
    }
    return 0;
}

int csv_store_append(csv_store *s, const event_record *r)
{
    char row[256];
    int len = csv_format_record(row, sizeof row, r);
    if (len < 0)
        return -1;
    if (write_all(s->fd, row, (size_t)len) < 0)
        return -1;
    return fsync(s->fd);
}

void csv_store_close(csv_store *s)
{
    if (s->fd >= 0)
        close(s->fd);
    s->fd = -1;
}
