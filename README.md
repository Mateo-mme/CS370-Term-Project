# CS370-Term-Project

Crop-Guardians: a Raspberry Pi 3B device that detects potential crop threats
with a PIR motion sensor and an HC-SR04 distance sensor (see `problemMemo.md`).
The core is C17 and uses libgpiod v2. Wiring is in `docs/wiring.md`.

## Build

```sh
sudo apt install libgpiod-dev   # one-time
make                            # builds build/crop_guard (-Wall -Wextra -Werror)
make check-flags                # clean rebuild + all tests
```

## Run

```sh
./build/crop_guard                          # integrated system, interrupt-driven; Ctrl-C to stop
./build/crop_guard --mode=poll --poll-us=1000
./build/crop_guard --alert-cm=150 --range-ms=200
./build/crop_guard --range=always           # range continuously (testing/benchmarks)
./build/crop_guard --range=off              # PIR only
./build/crop_guard --duration-s=60 --out=data/run.csv --quiet
./build/crop_guard --probe                  # hardware access check only
./build/crop_guard --help                   # all options
```

How the sensors work together (default `--range=motion`):

1. The **PIR** reports `motion_start` and `motion_end`. The HC-SR04 stays idle while nothing moves.
2. On `motion_start` the **HC-SR04** pings right away, then every `--range-ms`
   (default 100, minimum 60) until `motion_end`. A new episode still waits at
   least 60 ms after the previous ping, and the first ping waits 60 ms after startup.
3. The first in-range reading closer than `--alert-cm` (default 200; 0 = off)
   during an episode stores one **`threat`** event: motion and proximity together.

Events are appended to `data/events.csv` by default:

| Column        | Meaning |
|---------------|---------|
| `t_mono_ns`   | CLOCK_MONOTONIC edge time: the kernel timestamp (irq) or the sample that saw the change (poll). For distance rows this is the ECHO falling edge, or the time a timeout was detected |
| `t_wall_iso`  | UTC time the event was handled |
| `mode`        | `irq` or `poll` |
| `event`       | `motion_start`, `motion_end`, `distance`, `threat` |
| `distance_cm` | distance and threat rows; empty when the echo timed out |
| `echo_ok`     | distance and threat rows; 1 if the reading is in the 2–400 cm range |
| `latency_ns`  | edge-to-handler time, irq mode only. Empty in poll mode, where the true edge time is unknown |

In poll mode, ranging busy-reads ECHO from the trigger until the echo ends.
PIR sampling pauses for that time, which is the cost of polling that the irq
mode avoids.

## Tests

| Target           | Kind          | Hardware needed |
|------------------|---------------|-----------------|
| `make test-unit` | software-only | no              |
| `make test-mock` | mocked HW     | no              |
| `tests/hw/`      | physical HW   | yes (manual; log in `tests/hw/RESULTS.md`) |
