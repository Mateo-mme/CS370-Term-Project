/* HC-SR04 echo timing logic, independent of how edges are observed.
 * The same tracker consumes kernel-timestamped edges (irq) or transitions
 * found by sampling (poll). */
#ifndef HCSR04_H
#define HCSR04_H

#include "gpio_backend.h"

#include <stdbool.h>
#include <stdint.h>

#define HCSR04_TIMEOUT_NS      30000000ULL /* > 400 cm round trip (~23.3 ms) */
#define HCSR04_MIN_CM          2.0
#define HCSR04_MAX_CM          400.0
#define HCSR04_SOUND_CM_PER_NS 3.43e-5     /* 343 m/s, air at ~20 °C */
#define HCSR04_TRIG_PULSE_NS   10000ULL    /* datasheet: >= 10 µs high on TRIG */
#define HCSR04_MIN_INTERVAL_NS 60000000ULL /* datasheet: >= 60 ms between triggers */
#define HCSR04_SETTLE_NS       60000000ULL /* after opening lines (TRIG may glitch) */

typedef enum { HCSR04_OK, HCSR04_OUT_OF_RANGE, HCSR04_TIMEOUT } hcsr04_status;

typedef struct {
    hcsr04_status status;
    double        cm;       /* valid unless TIMEOUT */
    uint64_t      width_ns; /* echo pulse width; 0 on TIMEOUT */
    uint64_t      rise_ns;  /* echo rising edge; 0 if it never rose */
} hcsr04_reading;

typedef enum { HCSR04_IDLE, HCSR04_WAIT_RISE, HCSR04_WAIT_FALL } hcsr04_phase;

typedef struct {
    hcsr04_phase phase;
    uint64_t     trig_ns;
    uint64_t     rise_ns;
} hcsr04_tracker;

double hcsr04_width_to_cm(uint64_t width_ns);

void hcsr04_init(hcsr04_tracker *t);

/* Call right after the 10 µs TRIG pulse; arms the tracker for one echo. */
void hcsr04_start(hcsr04_tracker *t, uint64_t trig_ns);

/* Feed one ECHO edge. Returns true and fills *out when a measurement completes.
 * Edges arriving while idle (stray echoes) are ignored. */
bool hcsr04_on_edge(hcsr04_tracker *t, gpio_edge_type type, uint64_t ts_ns,
                    hcsr04_reading *out);

/* Returns true and fills *out with a TIMEOUT reading if the armed measurement
 * has waited longer than HCSR04_TIMEOUT_NS for its next edge. */
bool hcsr04_check_timeout(hcsr04_tracker *t, uint64_t now_ns, hcsr04_reading *out);

/* If a measurement is armed, stores the earliest time hcsr04_check_timeout
 * can report a timeout and returns true. */
bool hcsr04_deadline(const hcsr04_tracker *t, uint64_t *deadline_ns);

#endif
