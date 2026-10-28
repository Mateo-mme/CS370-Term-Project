# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CS370 Term Project (Crop-Guardians): a PIR + HC-SR04 crop-threat monitor on a Raspberry Pi 3B. The core is C17, built with `-Wall -Wextra -Werror` and libgpiod v2. Raw Claude Code `.jsonl` transcripts must be preserved for submissions; never modify or delete them.

## Commands

- Build: `make` → `build/crop_guard`
- Clean rebuild + all tests: `make check-flags`
- Software-only tests: `make test-unit`; mocked-hardware tests: `make test-mock`
- Single test: `make build/tests/unit/test_pir && ./build/tests/unit/test_pir`
- UBSan run: `make BUILD=/tmp/san CFLAGS="-O1 -g -fsanitize=undefined -fno-sanitize-recover=all" test` (ASan does not work on this Pi's 39-bit VA kernel)
- Hardware access check: `./build/crop_guard --probe` (add `--mode=poll` for polling)

## Architecture

- `src/gpio_backend.h`: GPIO interface (read/write/wait_edge) with pin constants (PIR=26, TRIG=23, ECHO=24, BCM).
- `src/gpio_libgpiod.c`: real backend. In irq mode, inputs use both-edge detection with kernel CLOCK_MONOTONIC timestamps; in poll mode they are plain inputs.
- `src/gpio_mock.c` / `gpio_mock.h`: in-memory backend with a FIFO of scripted, timestamped edges for tests.
- Pure logic, no GPIO: `hcsr04.c` (echo tracker: width→cm, range, 30 ms timeout), `pir.c` (motion state machine; debounced events keep the original edge timestamp), `csv.c` (RFC 4180 row formatting, `CSV_HEADER`), `metrics.c` (nearest-rank percentiles, histogram). `event.h` defines the record stored per event.
- Test separation is enforced at link time: `tests/unit` (software-only) links only the logic objects; `tests/mock` adds `gpio_mock`. Neither links libgpiod. Shared helpers: `tests/test_util.h` (`CHECK`, `CHECK_NEAR`, `TEST_DONE`).
- Wiring and physical validation log: `docs/wiring.md`.
