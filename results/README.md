# Interrupt vs. polling: results

**Run:** 2026-10-28T22:10Z on a Pi 3 Model B Rev 1.2 (kernel 6.18.50+rpt-rpi-v8,
`ondemand` governor, load average 0.17). `make bench` with 1000 edges per
configuration, random 20–50 ms spacing, seed 1, and 30 s idle runs. Details are
in `env.txt`. `git: 0c82854+dirty` means the run used the Phase 5 code before it
was committed; the Phase 5 commit contains exactly that code.

**What is measured:** latency = the time the production pipeline's handler ran
minus the time just before `tools/stimulus` wrote GPIO5, which is jumpered to
GPIO6. The receiver is the unchanged `crop_guard` (`--pir-pin=6 --range=off`),
so each event also goes through the state machine and an fsync'd CSV write.
CPU is `getrusage` (user + sys) divided by wall time over the pipeline run.

## Summary

| Mechanism   | p50 | p90 | p99 | max | missed | CPU idle | CPU under load |
|-------------|----:|----:|----:|----:|-------:|---------:|---------------:|
| irq (edge events) | 83 µs | 129 µs | 3.86 ms | 34.7 ms | 0 / 1000 | **0.05 %** | 1.07 % |
| poll 100 µs | **68 µs** | 106 µs | **140 µs** | 27.8 ms | 0 / 1000 | 12.9 % | 12.3 % |
| poll 1 ms   | 538 µs | 944 µs | 13.2 ms | 35.7 ms | 0 / 1000 | 2.10 % | 2.63 % |
| poll 10 ms  | 4.86 ms | 9.00 ms | 9.99 ms | 25.1 ms | **2 / 1000** | 0.39 % | 1.24 % |

Source: `summary.csv`. Per-edge data: `latency_*.csv`. Distributions: `hist_*.csv`.

## Interpretation

1. **Polling latency is set by the interval.** The median is about half the
   interval (538 µs at 1 ms, 4.86 ms at 10 ms) and p90 approaches the full
   interval, as expected for edges arriving at random phase. Interrupt latency
   does not depend on any interval.

2. **Polling's CPU cost is set by the interval too.** Idle CPU rises from
   0.39 % (10 ms) to 2.1 % (1 ms) to 12.9 % (100 µs) while nothing happens,
   compared with **0.05 % for irq**. irq blocks in the kernel until an edge
   arrives. To match irq's typical latency, polling has to run at 100 µs, which
   costs about **260× the idle CPU**. On a battery- or solar-powered field
   device, that difference is the deciding factor.

3. **Polling at 100 µs has a slightly lower median than irq (68 vs. 83 µs).**
   An irq edge has to wake a sleeping process: the kernel timestamps the edge
   after a mean of 21.6 µs (max 88.8 µs over 1000 edges), and the rest is the
   wake-up and scheduling path. A 100 µs poller is effectively always awake.
   Its median advantage costs 12.9 % CPU.

4. **Polling can lose events; irq cannot.** At 10 ms, edges 608 and 609 (a
   rising edge and the falling edge 25 ms after it) were both missed. The loop
   was late (58 overruns in that run), so no sample saw the short high level.
   The kernel queues irq edges with their timestamps even when the handler is
   late, so irq stores every edge, and the stored `t_mono_ns` stays exact.

5. **Tails of tens of ms in every mode come from storage, not from the input
   mechanism.** In all 15 irq edges over 1 ms, edge detection was still fast
   (17–30 µs to the kernel timestamp); the whole delay was kernel-to-handler,
   meaning the event loop was busy. The loop fsyncs every row, and a separate
   measurement on this SD card gave fsync p50 3.8 ms, p99 10.8 ms and max
   22 ms (300 samples). A row whose fsync stalls delays whatever edge comes
   next, whichever mechanism is used. The poll overrun counts (1793 at 100 µs,
   1012 at 1 ms, 58 at 10 ms) show the same stalls. *Hypothesis, not yet
   verified:* the frequent ~3.8–4 ms irq outliers (p99) are a 250 Hz scheduler
   tick (CONFIG_HZ=250) or SD writeback briefly delaying the wake-up.
   Separating storage from the event loop would remove these tails, for example
   with the multi-process/IPC design planned in `problemMemo.md` (Mechanism E).
   A follow-up benchmark with storage moved off the event loop would confirm this.

## Limits of this measurement

- Each latency is overstated by at most the GPIO write time (mean ~6 µs),
  equally in all modes, because the stimulus time is taken just before the write.
- Single run, one seed, `ondemand` governor, otherwise idle system. CPU
  frequency scaling probably adds to irq wake-up latency; a `performance`
  governor run would show how much.
- "CPU under load" includes the fsync'd CSV writes for each event, so it is
  the CPU cost of the whole pipeline, not just the input mechanism.
- The loopback measures the GPIO input path. PIR and HC-SR04 sensor behaviour
  (hold time, echo timing) is outside its scope.
