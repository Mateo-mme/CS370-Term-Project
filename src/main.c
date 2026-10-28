/* crop_guard: PIR + HC-SR04 crop-threat monitor.
 * Phase 0: argument parsing and a hardware probe. The sensing pipeline arrives
 * in later phases. */
#include "gpio_backend.h"

#include <stdio.h>
#include <string.h>

#define DEFAULT_CHIP "/dev/gpiochip0"

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [--mode=irq|poll] [--chip=PATH] [--probe]\n"
            "  --probe   open the GPIO lines, print PIR/ECHO levels, and exit\n",
            prog);
}

static int probe(const char *chip, bool use_edges)
{
    gpio_backend *b = gpio_libgpiod_open(chip, use_edges);
    if (!b) {
        fprintf(stderr, "probe: could not open GPIO lines on %s\n", chip);
        return 1;
    }
    printf("backend=%s PIR(GPIO%d)=%d ECHO(GPIO%d)=%d TRIG(GPIO%d)=output\n", b->name,
           PIN_PIR, b->read(b, PIN_PIR), PIN_ECHO, b->read(b, PIN_ECHO), PIN_TRIG);
    b->close(b);
    return 0;
}

int main(int argc, char **argv)
{
    const char *chip = DEFAULT_CHIP;
    bool use_edges = true;
    bool do_probe = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode=irq") == 0)
            use_edges = true;
        else if (strcmp(argv[i], "--mode=poll") == 0)
            use_edges = false;
        else if (strncmp(argv[i], "--chip=", 7) == 0)
            chip = argv[i] + 7;
        else if (strcmp(argv[i], "--probe") == 0)
            do_probe = true;
        else {
            usage(argv[0]);
            return 2;
        }
    }

    if (do_probe)
        return probe(chip, use_edges);

    fprintf(stderr, "crop_guard: sensing pipeline not implemented yet (Phase 0 skeleton); "
                    "use --probe\n");
    return 1;
}
