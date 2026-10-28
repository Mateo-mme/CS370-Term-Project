# Physical hardware validation

These checks need the real Pi 3B and sensors (wiring: `docs/wiring.md`). They
are **manual** and are **not** run by `make test`. Record every run in
`RESULTS.md` with the date, the command, what you observed, and pass/fail.

## Test categories

| Category      | Location      | Hardware                      | How it runs        |
|---------------|---------------|-------------------------------|--------------------|
| Software-only | `tests/unit/` | none; does not link any GPIO backend | `make test-unit` |
| Mocked HW     | `tests/mock/` | none; scripted `gpio_mock` with a virtual clock | `make test-mock` |
| Physical HW   | `tests/hw/`   | Pi 3B + PIR + HC-SR04         | by hand, this file |

`make test-unit` and `make test-mock` pass without hardware. They do not show
that the sensors work; only the checks below do.

## PIR (motion)

The PIR module needs about 60 s after power-up to settle. Its *Time* pot sets
how long OUT stays high after motion, and after OUT falls it ignores motion for
about 2–3 s. Before a run, stay still and out of its view for 10 s.

**HW-PIR-1: hand-wave, poll mode**
`./build/crop_guard --mode=poll --poll-us=1000 --duration-s=60 --out=data/hw_pir_poll.csv`
Wave 3 times, 10–15 s apart.
Pass if: each wave produces one `motion_start` and then one `motion_end`; no
rows appear while idle; `latency_ns` is empty (unknown in poll mode by design);
`samples` ≈ 60 000; and `store_errors=0`.

**HW-PIR-2: hand-wave, irq mode**
The same with `--mode=irq --out=data/hw_pir_irq.csv`.
Pass if: the same rows appear, `latency_ns` is filled in, and `pir_edges` = 2 × waves.

**HW-PIR-3: clean shutdown**
Run without `--duration-s` and press Ctrl-C.
Pass if: the summary prints and the CSV ends with a complete row.

## HC-SR04 (distance)

Use a flat, hard target (a book or box) square to the sensor, with nothing
else within about 30° of its axis. Measure from the sensor's front face.

**HW-RNG-1: accuracy at known distances, both modes.** For D in 10, 30, 50 and 100 cm:
`./build/crop_guard --mode=irq --range-ms=100 --duration-s=5 --quiet --out=data/hw_rng_irq_D.csv`
Then run the same with `--mode=poll`.
Pass if: the mean of the `echo_ok=1` rows is within ±1 cm or ±3 % of D,
whichever is larger.
Also record the standard deviation and the number of `echo_ok=0` rows.

**HW-RNG-2: no target.** Point the sensor at open space (more than 4 m).
Pass if: the rows have `echo_ok=0`, either as timeouts (empty distance) or as
the clone's ~410 cm "no echo" pulse.

## Integrated system

**HW-PIR-0: is the PIR alive?** `gpiomon -c gpiochip0 26` while waving (Ctrl-C to stop).
Pass if: rising and falling edges are printed. If none appear, check the PIR's
power (5 V), its ground, and that OUT goes to GPIO26 (physical pin 37).

**HW-INT-1: walk-up threat, both modes.**
`./build/crop_guard --mode=irq --duration-s=60 --out=data/hw_int_irq.csv`
Stay still for 10 s. Then wave from more than 2.5 m away, wait 15 s, and wave
about 50 cm in front of the sensors. Repeat with `--mode=poll`.
Pass if: there are no `distance` rows while idle; `distance` rows appear only
between `motion_start` and `motion_end`; the far wave gives no `threat` (or
only once you come closer); the near wave gives exactly one `threat`, whose
distance is about 50 cm; and `store_errors=0`.

**HW-RNG-3: ECHO voltage.** Measure GPIO24 while ECHO is high (see `docs/wiring.md`).
Pass if: it reads ≤ 3.3 V.
