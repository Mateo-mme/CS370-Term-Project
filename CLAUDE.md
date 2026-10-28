# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CS370 Term Project (Crop-Guardians): a PIR + HC-SR04 crop-threat monitor on a Raspberry Pi 3B. The core is C17, built with `-Wall -Wextra -Werror` and libgpiod v2. Raw Claude Code `.jsonl` transcripts must be preserved for submissions; never modify or delete them.

## Commands

- Build: `make` → `build/crop_guard`
- Clean rebuild + all tests: `make check-flags`
- Software-only tests: `make test-unit`; mocked-hardware tests: `make test-mock`
- Single test: `make build/tests/unit/test_smoke && ./build/tests/unit/test_smoke`
- Hardware access check: `./build/crop_guard --probe` (add `--mode=poll` for polling)

## Architecture

- `src/gpio_backend.h`: GPIO interface (read/write/wait_edge) with pin constants (PIR=26, TRIG=23, ECHO=24, BCM).
- `src/gpio_libgpiod.c`: real backend. In irq mode, inputs use both-edge detection with kernel CLOCK_MONOTONIC timestamps; in poll mode they are plain inputs.
- `src/gpio_mock.c`: in-memory backend for tests. Tests in `tests/unit` must not touch GPIO; `tests/mock` use the mock backend.
- Wiring and physical validation log: `docs/wiring.md`.
