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
| 2026-10-28 | integrated idle | default `--range=motion`, 5 s, irq and poll | 0 rows, 0 TRIG pulses, 0 PIR edges in both modes; poll: 4972 samples, 12 overruns | pass (no false pings/threats) |
| 2026-10-28 | integrated, always | `--range=always --duration-s=2` | 20/20 distances, 0 threats (no motion) | pass |
| 2026-10-28 | stimulus tool | `stimulus --count=200 --min-ms=5 --max-ms=10` (no receiver) | Gaps 5.09–10.04 ms (after the fix below); mean write 6.4 µs | pass |
| 2026-10-28 | idle CPU (preliminary) | receiver `--pir-pin=6 --range=off`, 5 s, no stimulus | irq 0.049 %; poll 100 µs 12.53 %; poll 1 ms 1.93 %; poll 10 ms 0.34 % | observation (the full benchmark replaces this) |
| 2026-10-28 | HW-BENCH-0 | GPIO5→GPIO6 jumper | Before fitting: GPIO6 read 1 for GPIO5 = 0. After fitting: GPIO6 followed 1, 0, 1, 0 | pass |
| 2026-10-28 | HW-BENCH-1 | `make bench` (1000 edges × 4 configurations) | 3998/4000 matched, 0 spurious; 2 missed at poll 10 ms (a short pulse during a loop stall). See `results/README.md` | pass, with findings |
| _pending_  | HW-PIR-0  | `gpiomon` while waving | | |
| _pending_  | HW-INT-1  | walk-up threat, irq and poll | | |
| _pending_  | HW-RNG-1  | tape-measured target, 10/30/50/100 cm | | |
| _pending_  | HW-RNG-2  | no target | | |
| _pending_  | HW-RNG-3  | multimeter on GPIO24 | | |

## Notes

- **Stimulus spacing bug (found on hardware, fixed):** the first version
  scheduled edges at absolute times, so a late wake-up (several ms for the first
  edge, about 1.6 ms once later) squeezed the next gap to 0.12 ms, below the
  minimum. Edges are now spaced from the previous *actual* edge time.

- **No PIR edge has been seen on hardware yet,** across about 3 minutes of runs.
  This is consistent with nobody moving, but it is not yet proof that the PIR works. Run HW-PIR-0 next.

- **≈410 cm `echo_ok=0` rows:** these come from the HC-SR04 clone itself. When
  no reflection returns, it still holds ECHO high for about 23.9 ms. The pipeline
  classifies these as out of range, as intended.
- **Ranging interval:** use `--range-ms=100` or more. At the 60 ms datasheet
  minimum, late reflections from the previous ping cause many missed echoes.
- **CPU, ranging at 100 ms for 3 s:** irq used user 11 ms + sys 2 ms; poll
  used user 130 ms + sys 228 ms (176 358 busy ECHO reads, 30 PIR overruns).
  These are single runs; Phase 5 makes the proper measurement.
