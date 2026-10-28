/* Minimal test helpers: unlike assert(), checks keep running after a failure
 * and do not depend on NDEBUG. */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int test_failures;

/* Creates an empty scratch file under build/tests/ (tests run from the repo
 * root via make) and stores its path in path[]. Returns 0 or -1. */
static inline int make_temp_file(char *path, size_t n, const char *stem)
{
    if ((size_t)snprintf(path, n, "build/tests/%s_XXXXXX", stem) >= n)
        return -1;
    int fd = mkstemp(path);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    close(fd);
    return 0;
}

/* Reads a whole file into a malloc'd NUL-terminated string, or NULL. */
static inline char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    char *buf = NULL;
    size_t len = 0, cap = 0, r;
    char chunk[4096];
    while ((r = fread(chunk, 1, sizeof chunk, f)) > 0) {
        if (len + r + 1 > cap) {
            cap = (len + r + 1) * 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = nb;
        }
        for (size_t i = 0; i < r; i++)
            buf[len++] = chunk[i];
    }
    fclose(f);
    if (!buf)
        buf = calloc(1, 1);
    else
        buf[len] = '\0';
    return buf;
}

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            test_failures++;                                                     \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                    \
    do {                                                                         \
        double a_ = (a), b_ = (b), d_ = a_ - b_;                                 \
        if (d_ > (tol) || -d_ > (tol)) {                                         \
            fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s = %g, expected %g\n",  \
                    __FILE__, __LINE__, #a, a_, b_);                             \
            test_failures++;                                                     \
        }                                                                        \
    } while (0)

#define TEST_DONE(name)                                                          \
    do {                                                                         \
        if (test_failures) {                                                     \
            fprintf(stderr, "%s: %d failure(s)\n", name, test_failures);         \
            return 1;                                                            \
        }                                                                        \
        printf("%s: ok\n", name);                                                \
        return 0;                                                                \
    } while (0)

#endif
