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
./build/crop_guard --probe              # hardware access check (interrupt mode)
./build/crop_guard --mode=poll --probe  # same, polling mode
```

## Tests

| Target           | Kind          | Hardware needed |
|------------------|---------------|-----------------|
| `make test-unit` | software-only | no              |
| `make test-mock` | mocked HW     | no              |
| `docs/wiring.md` | physical HW   | yes (manual)    |
