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
./build/crop_guard                                # irq mode, PIR only; Ctrl-C to stop
./build/crop_guard --range-ms=100                 # also range with the HC-SR04 every 100 ms
./build/crop_guard --mode=poll --poll-us=1000 --range-ms=100
./build/crop_guard --duration-s=60 --out=data/run.csv --quiet
./build/crop_guard --probe                        # hardware access check only
./build/crop_guard --help                         # all options
```

Events are appended to `data/events.csv` by default:

| Column        | Meaning |
|---------------|---------|
| `t_mono_ns`   | CLOCK_MONOTONIC edge time: the kernel timestamp (irq) or the sample that saw the change (poll). For distance rows this is the ECHO falling edge, or the time a timeout was detected |
| `t_wall_iso`  | UTC time the event was handled |
| `mode`        | `irq` or `poll` |
| `event`       | `motion_start`, `motion_end`, `distance` |
| `distance_cm` | distance rows only; empty when the echo timed out |
| `echo_ok`     | distance rows only; 1 if the reading is in the 2–400 cm range |
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
