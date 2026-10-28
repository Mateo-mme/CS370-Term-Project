/* Software-only: append-only CSV storage on the real filesystem (no GPIO). */
#include "../test_util.h"
#include "csv.h"
#include "csv_store.h"

#include <string.h>

int main(void)
{
    char path[128];
    if (make_temp_file(path, sizeof path, "csv_store") < 0)
        return 1;

    event_record a = {.t_mono_ns = 1, .mode = "irq", .kind = EV_MOTION_START, .latency_ns = 10};
    event_record b = {.t_mono_ns = 2, .mode = "irq", .kind = EV_MOTION_END, .latency_ns = 20};

    csv_store s;
    CHECK(csv_store_open(&s, path) == 0);
    CHECK(csv_store_append(&s, &a) == 0);
    csv_store_close(&s);

    /* Reopening must append, not rewrite the header or truncate. */
    CHECK(csv_store_open(&s, path) == 0);
    CHECK(csv_store_append(&s, &b) == 0);
    csv_store_close(&s);

    char *got = read_file(path);
    CHECK(got != NULL);
    if (got) {
        const char *want = CSV_HEADER
            "1,1970-01-01T00:00:00.000000Z,irq,motion_start,,,10\n"
            "2,1970-01-01T00:00:00.000000Z,irq,motion_end,,,20\n";
        CHECK(strcmp(got, want) == 0);
        if (strcmp(got, want) != 0)
            fprintf(stderr, "got:\n%s", got);
        free(got);
    }
    unlink(path);

    CHECK(csv_store_open(&s, "build/tests/no_such_dir/x.csv") == -1);

    TEST_DONE("test_csv_store");
}
