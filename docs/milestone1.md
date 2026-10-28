# Milestone 1: requirements and evidence

Scope: detect motion (PIR) and measure distance (HC-SR04) as one integrated
system on a Raspberry Pi 3B, with interrupt-driven input compared against
polling. The camera and the multi-process architecture (Mechanism E in
`problemMemo.md`) are later milestones.

Commits: `04f46d1` (Phase 0) through `efc6345` (Phase 5), plus `16265f2`
(input pull-down fix) and the Phase 6 docs/transcripts commit.

## Requirements

| Requirement | How it is met | Evidence |
|---|---|---|
| Core in C17 or C23 | All of `src/` and `tools/*.c` are C17 (`-std=c17`) | `Makefile` (`CSTD`) |
| Compiles cleanly with `-Wall -Wextra -Werror` | Every object, tool and test is built with these flags | `make check-flags`: clean rebuild + all 10 test programs, zero warnings |
| PIR and HC-SR04 form one integrated system | The PIR gates ranging (`--range=motion`, default): `motion_start` triggers the HC-SR04 right away and then every `--range-ms` until `motion_end`. Motion plus an echo within `--alert-cm` produces one `threat` event per episode | `src/pipeline.c` (`ranging_active`, `emit_distance`); `tests/mock/test_pipeline_integrated.c` |
| Interrupt-driven input | libgpiod v2 edge events: the kernel services the GPIO IRQ and timestamps each edge with CLOCK_MONOTONIC; the loop blocks in `wait_edge` until an edge or a deadline | `src/gpio_libgpiod.c`, `run_irq` in `src/pipeline.c` |
| Compared with polling: event-latency distributions | Same pipeline in `--mode=poll` at 100 µs / 1 ms / 10 ms. A separate stimulus process drives a GPIO5→GPIO6 loopback 1000× per configuration; latency = handler time − stimulus time; percentiles + histograms | `results/summary.csv`, `results/latency_*.csv`, `results/hist_*.csv`, `results/README.md` |
| Compared with polling: CPU utilization | `getrusage` over each run, idle (30 s) and under load | `results/summary.csv` (`load_cpu_pct`, `idle_cpu_pct`) |
| Data flows through the real pipeline into persistent storage | Every event → `event_record` → `csv_store` (append-only, one `write` + `fsync` per row, header on new file). The benchmark's receiver *is* the production binary (`crop_guard --pir-pin=6 --range=off`) | `src/csv_store.c`; `tests/unit/test_csv_store.c`; mock pipeline tests read the CSV back from disk; `results/raw/events_*.csv` |
| Hardware tested on real hardware; mocked, software-only and physical tests clearly distinguished | Three categories, separated at **link time**: `tests/unit` links only the logic objects (no GPIO backend at all); `tests/mock` adds the scripted `gpio_mock` with a virtual clock; neither links libgpiod. Physical checks are manual, with a dated log | `Makefile`; `tests/hw/README.md` (procedures and category table); `tests/hw/RESULTS.md` (every hardware run, including failures) |
| HC-SR04 ECHO protected by a voltage divider | 1 kΩ series / 2.2 kΩ to ground | `docs/wiring.md`. **Note:** this gives ≈3.44 V, slightly above 3.3 V; not yet measured (see gaps) |
| Raw Claude Code `.jsonl` transcripts preserved | Copied byte-for-byte with checksums | `transcripts/`, `tools/save_transcripts.sh` (`make transcripts`) |

## Headline results (Pi 3B, 1000 edges per configuration)

| Mechanism | p50 | p99 | missed | CPU idle |
|---|---:|---:|---:|---:|
| irq | 83 µs | 3.86 ms | 0 | 0.05 % |
| poll 100 µs | 68 µs | 140 µs | 0 | 12.9 % |
| poll 1 ms | 538 µs | 13.2 ms | 0 | 2.10 % |
| poll 10 ms | 4.86 ms | 9.99 ms | 2 | 0.39 % |

Interrupts give sub-100 µs typical latency at near-zero idle CPU and never
lose an edge. Polling must run at 100 µs to match that latency, which costs
about 260× the idle CPU, and at 10 ms it missed a short pulse. Tails of tens
of ms in every mode come from the per-row `fsync` in the event loop, not from
the input mechanism. Full analysis and limitations: `results/README.md`.

## Test inventory

| Category | Programs | What they cover |
|---|---|---|
| Software-only (`make test-unit`) | `test_csv`, `test_csv_store`, `test_hcsr04`, `test_latency_match`, `test_metrics`, `test_pir` | Echo width → cm, range and timeouts; PIR debounce state machine; CSV escaping/formatting; append-only file storage; percentiles/histograms; stimulus↔event matching |
| Mocked HW (`make test-mock`) | `test_mock_backend`, `test_pipeline_pir`, `test_pipeline_range`, `test_pipeline_integrated` | The real pipeline in irq and poll modes against scripted edges and a simulated HC-SR04, writing a real CSV that is read back |
| Physical HW (`tests/hw/`) | manual procedures | See `tests/hw/RESULTS.md` |

All mocked and software-only tests also pass under UBSan. Mutation checks
(deliberately broken code) were run for the pipeline, ranging and
integration logic, and each mutation was caught.

## Physical validation status

| Check | Status |
|---|---|
| GPIO access, both modes (probe) | pass |
| HC-SR04 ranging, irq and poll | pass: works in both modes; ≈410 cm `echo_ok=0` rows are the sensor's own "no echo" pulse |
| Integrated system idle (no false pings or threats) | pass |
| Interrupt vs. polling benchmark (loopback) | pass: 3998/4000 matched; 2 missed by 10 ms polling (a finding) |
| PIR | Diagnosed as not driving GPIO26 (floating line; an earlier "detection" was noise on an input with no pull resistor). After a physical fix the user reports detection with the pull-down check; `crop_guard` hand-wave runs still pending |

## Known gaps

1. **PIR through `crop_guard`:** HW-PIR-1/2 (hand-wave, poll and irq) and
   HW-INT-1 (walk-up threat) need to be run and logged now that the PIR responds.
2. **HC-SR04 accuracy (HW-RNG-1):** not yet measured against tape-measured
   distances; current evidence shows it works, not how accurate it is.
3. **ECHO voltage (HW-RNG-3):** 1 kΩ/2.2 kΩ gives ≈3.44 V by calculation.
   Measure it; if it is above 3.3 V, change R1 to 1.2 kΩ or R2 to 2.0 kΩ.
4. **Storage in the event loop:** per-row `fsync` causes the latency tails.
   Moving storage to a separate process (Mechanism E) is the planned fix and
   should be re-benchmarked.
5. **Final transcript copy:** run `make transcripts` after this session's last
   message and commit, since the session file keeps growing until then.
