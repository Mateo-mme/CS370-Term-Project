# Physical hardware validation results

Platform: Pi 3 Model B Rev 1.2, kernel 6.18.50+rpt-rpi-v8, libgpiod 2.2.1.

| Date       | Test      | Command (abridged) | Observed | Result |
|------------|-----------|--------------------|----------|--------|
| 2026-10-28 | probe     | `--probe`, irq and poll | Lines 23/24/26 requested; PIR=0 and ECHO=0 idle over 6 runs | pass |
| 2026-10-28 | HW-PIR-3  | `timeout -s INT 3 crop_guard --mode=irq` | Clean exit at 3.006 s with summary; CPU user 1 ms + sys 9 ms | pass |
| 2026-10-28 | HW-PIR-3  | `timeout -s INT 3 crop_guard --mode=poll` | Clean exit at 3.006 s; 2996 samples, 1 overrun; CPU user 16 ms + sys 53 ms | pass |
| 2026-10-28 | HW-PIR-1  | poll, 1 ms, 60 s | 59 897 samples, 56 overruns, **0 motion events**, store_errors=0. Not confirmed that anyone waved during this window | **inconclusive, rerun** |
| _pending_  | HW-PIR-2  | irq hand-wave | | |
| 2026-10-28 | ranging smoke | `--range-ms=100 --duration-s=3`, irq then poll | 30/30 readings in each mode, 0 timeouts. irq ok rows: 53.1–59.2 cm (mean 55.4). poll: 57.4–58.1 cm (mean 57.4). 5–6 rows per run ≈410 cm with `echo_ok=0` | pass (works); accuracy untested |
| 2026-10-28 | ranging, alternating | `--range-ms=60 --duration-s=2`, irq/poll × 3 rounds | sd per round 0.12–1.34 cm in **both** modes, with no consistent winner (uncontrolled scene). At 60 ms, 40–70 % of pings had no echo vs. ~17 % at 100 ms | observation |
| _pending_  | HW-RNG-1  | tape-measured target, 10/30/50/100 cm | | |
| _pending_  | HW-RNG-2  | no target | | |
| _pending_  | HW-RNG-3  | multimeter on GPIO24 | | |

## Notes

- **≈410 cm `echo_ok=0` rows:** these come from the HC-SR04 clone itself. When
  no reflection returns, it still holds ECHO high for about 23.9 ms. The pipeline
  classifies these as out of range, as intended.
- **Ranging interval:** use `--range-ms=100` or more. At the 60 ms datasheet
  minimum, late reflections from the previous ping cause many missed echoes.
- **CPU, ranging at 100 ms for 3 s:** irq used user 11 ms + sys 2 ms; poll
  used user 130 ms + sys 228 ms (176 358 busy ECHO reads, 30 PIR overruns).
  These are single runs; Phase 5 makes the proper measurement.
