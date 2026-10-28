/* Software-only: CSV escaping and record formatting. */
#include "../test_util.h"
#include "csv.h"

#include <string.h>

static void test_escape(void)
{
    char buf[64];
    CHECK(csv_escape_field(buf, sizeof buf, "irq") == 3 && strcmp(buf, "irq") == 0);
    CHECK(csv_escape_field(buf, sizeof buf, "") == 0 && strcmp(buf, "") == 0);
    CHECK(csv_escape_field(buf, sizeof buf, "a,b") > 0 && strcmp(buf, "\"a,b\"") == 0);
    CHECK(csv_escape_field(buf, sizeof buf, "say \"hi\"") > 0 &&
          strcmp(buf, "\"say \"\"hi\"\"\"") == 0);
    CHECK(csv_escape_field(buf, sizeof buf, "x\ny") > 0 && strcmp(buf, "\"x\ny\"") == 0);
    CHECK(csv_escape_field(buf, 4, "abcd") == -1);
    CHECK(csv_escape_field(buf, 4, "abc") == 3);
    CHECK(csv_escape_field(buf, 0, "") == -1);
}

static void test_wall_time(void)
{
    char buf[40];
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 123456789};
    CHECK(csv_format_wall_time(buf, sizeof buf, ts) > 0);
    CHECK(strcmp(buf, "1970-01-01T00:00:00.123456Z") == 0);
    CHECK(csv_format_wall_time(buf, 10, ts) == -1);
}

static void test_records(void)
{
    char buf[256];
    event_record d = {
        .t_mono_ns = 123456789012,
        .t_wall = {.tv_sec = 86400, .tv_nsec = 5000},
        .mode = "irq",
        .kind = EV_DISTANCE,
        .distance_cm = 42.426,
        .echo_ok = 1,
        .latency_ns = 51000,
    };
    CHECK(csv_format_record(buf, sizeof buf, &d) > 0);
    CHECK(strcmp(buf, "123456789012,1970-01-02T00:00:00.000005Z,irq,distance,42.43,1,51000\n") == 0);

    event_record m = {
        .t_mono_ns = 7,
        .t_wall = {0, 0},
        .mode = "poll",
        .kind = EV_MOTION_START,
        .latency_ns = -1,
    };
    CHECK(csv_format_record(buf, sizeof buf, &m) > 0);
    CHECK(strcmp(buf, "7,1970-01-01T00:00:00.000000Z,poll,motion_start,,,\n") == 0);

    CHECK(csv_format_record(buf, 20, &d) == -1);
}

static void test_header_matches_columns(void)
{
    /* 7 columns in the header and in every row */
    size_t commas = 0;
    for (const char *p = CSV_HEADER; *p; p++)
        commas += *p == ',';
    CHECK(commas == 6);
    CHECK(strcmp(csv_event_name(EV_MOTION_END), "motion_end") == 0);
}

int main(void)
{
    test_escape();
    test_wall_time();
    test_records();
    test_header_matches_columns();
    TEST_DONE("test_csv");
}
