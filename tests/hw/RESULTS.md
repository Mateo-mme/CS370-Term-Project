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
| 2026-10-28 | HW-PIR-0  | `gpiomon -c gpiochip0 26` while waving (run by user) | No edges printed | **fail** |
| 2026-10-28 | PIR diagnosis | `gpioget -b pull-up/pull-down` on GPIO26, then on every free GPIO (4, 7–22, 25–27) | GPIO26 follows the bias (1 with pull-up, 0 with pull-down), so it is floating and not driven. Every other free GPIO is also floating, including GPIO7 (physical pin 26, ruling out a BCM/physical mix-up) | PIR OUT is not driving any header pin: check its power and wiring |
| 2026-10-28 | PIR recheck after rewiring to 4/39/37 and then swapping wires | Pull-up/pull-down test on GPIO26 and all free GPIOs, uptime 3–5 min (past warm-up) | Still floating; no free GPIO driven | **fail** |
| 2026-10-28 | PIR live monitor | `timeout 60 gpiomon -b pull-down -c gpiochip0 26`; the user was asked to wave | 0 edges in 60 s (the transcript doesn't confirm that anyone was waving during the window) | **fail**: next step is to measure the module's VCC–GND and OUT–GND with a multimeter |
| 2026-10-28 | PIR via RPi.GPIO, no pull (user's `~/pir.py`, BOARD 37 = GPIO26) | `GPIO.setup(37, GPIO.IN)`, so bias is off | Printed "Motion Detected!" when the user waved | **false positive**: a floating input picks up noise from a nearby hand |
| 2026-10-28 | `crop_guard` 60 s, irq, integrated (as-is bias) | `--mode=irq --duration-s=60` | 0 PIR edges; 2 stray ECHO edges at startup, ignored as intended | fail |
| 2026-10-28 | `gpiomon -b pull-down` on GPIO7 and GPIO26, 90 s | rules out a BOARD/BCM pin mix-up | 0 edges (user activity in the window not confirmed) | fail |
| 2026-10-28 | `crop_guard` 40 s, irq, **pull-down fix**, run by the user while waving | `--duration-s=40 --out=data/hw_int_irq.csv` | Header only, 0 PIR edges | fail |
| 2026-10-28 | RPi.GPIO **with PUD_DOWN** on BOARD 37, run by the user while waving | prints the level every 0.5 s for 20 s | All 0 | **fail: confirms the PIR is not driving GPIO26.** Next: multimeter at the module (VCC–GND ≈ 5 V; OUT–GND 0 → 3.3 V on motion), or swap in another PIR module |
| 2026-10-28 | PIR after a physical fix by the user (what changed is not yet recorded) | same RPi.GPIO PUD_DOWN check on BOARD 37 | User reports that it now detects when they wave | pass (user-reported); `crop_guard` HW-PIR-1/2 and HW-INT-1 still to run |
| _pending_  | HW-INT-1  | walk-up threat, irq and poll | | |
| _pending_  | HW-RNG-1  | tape-measured target, 10/30/50/100 cm | | |
| _pending_  | HW-RNG-2  | no target | | |
| _pending_  | HW-RNG-3  | multimeter on GPIO24 | | |

## Notes

- **Stimulus spacing bug (found on hardware, fixed):** the first version
  scheduled edges at absolute times, so a late wake-up (several ms for the first
  edge, about 1.6 ms once later) squeezed the next gap to 0.12 ms, below the
  minimum. Edges are now spaced from the previous *actual* edge time.

- **Pull-down on inputs (fix, 2026-10-28):** `gpio_libgpiod` now requests its
  inputs with the internal pull-down. Before the fix, an unconnected or dead
  PIR left GPIO26 floating, and noise (for example a hand moving near the
  wires) could be stored as motion. Verified afterwards: probe PIR=0 at rest;
  ranging 20/20 (mean 54.8 cm); benchmark loopback 40/40 in irq and poll.
- **PIR not connected or not powered (diagnosed 2026-10-28):** no PIR edge has
  appeared in any run, `gpiomon` showed nothing while waving, and GPIO26 floats
  (it follows the internal pull). A powered push-pull PIR output (HC-SR501,
  AM312) holds the line low when idle, so it would read 0 even with the pull-up.
  Quick check after rewiring: `gpioget --numeric -b pull-up -c gpiochip0 26` should print 0.

- **≈410 cm `echo_ok=0` rows:** these come from the HC-SR04 clone itself. When
  no reflection returns, it still holds ECHO high for about 23.9 ms. The pipeline
  classifies these as out of range, as intended.
- **Ranging interval:** use `--range-ms=100` or more. At the 60 ms datasheet
  minimum, late reflections from the previous ping cause many missed echoes.
- **CPU, ranging at 100 ms for 3 s:** irq used user 11 ms + sys 2 ms; poll
  used user 130 ms + sys 228 ms (176 358 busy ECHO reads, 30 PIR overruns).
  These are single runs; Phase 5 makes the proper measurement.
