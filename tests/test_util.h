/* Minimal test helpers: unlike assert(), checks keep running after a failure
 * and do not depend on NDEBUG. */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int test_failures;

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
