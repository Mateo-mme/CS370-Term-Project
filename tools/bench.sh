#!/usr/bin/env bash
# Interrupt vs. polling benchmark on real hardware.
#
# Needs a jumper from GPIO5 (physical pin 29) to GPIO6 (physical pin 31).
# For each configuration it runs the real pipeline (crop_guard --pir-pin=6
# --range=off) twice:
#   idle: no stimulus, IDLE_S seconds    -> CPU utilization at rest
#   load: tools/stimulus toggles GPIO5 COUNT times at random 20-50 ms spacing
#         -> per-edge latency (stimulus -> handler) and CPU under load
# Results: results/summary.csv, results/latency_<label>.csv,
# results/hist_<label>.csv, raw logs in results/raw/, environment in
# results/env.txt.
#
# Overrides: COUNT, MIN_MS, MAX_MS, IDLE_S, SEED, OUT, CONFIGS
#   e.g. COUNT=200 IDLE_S=5 CONFIGS="irq poll:1000" tools/bench.sh
set -euo pipefail
cd "$(dirname "$0")/.."

BIN=build/crop_guard
STIM=build/tools/stimulus
ANA=build/tools/analyze
COUNT=${COUNT:-1000}
MIN_MS=${MIN_MS:-20}
MAX_MS=${MAX_MS:-50}
IDLE_S=${IDLE_S:-30}
SEED=${SEED:-1}
OUT=${OUT:-results}
CONFIGS=${CONFIGS:-"irq poll:100 poll:1000 poll:10000"}
RAW=$OUT/raw

for f in "$BIN" "$STIM" "$ANA"; do
    [[ -x $f ]] || { echo "bench: $f missing; run make" >&2; exit 1; }
done

check_loopback() {
    local v got pid
    for v in 1 0 1 0; do
        gpioset -c gpiochip0 5=$v &
        pid=$!
        sleep 0.2
        got=$(gpioget --numeric -c gpiochip0 6)
        kill $pid
        wait $pid 2>/dev/null || true
        if [[ $got != "$v" ]]; then
            echo "bench: loopback check failed (GPIO5=$v, GPIO6=$got)." >&2
            echo "bench: fit a jumper from GPIO5 (pin 29) to GPIO6 (pin 31)." >&2
            exit 1
        fi
    done
}

cpu_pct() { grep -o 'cpu_pct=[0-9.]*' "$1" | tail -1 | cut -d= -f2; }

check_loopback
mkdir -p "$RAW"
rm -f "$OUT/summary.csv"
{
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "model: $(tr -d '\0' < /proc/device-tree/model)"
    echo "kernel: $(uname -r)"
    echo "governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/a)"
    echo "cpu_mhz_cur: $(( $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo 0) / 1000 ))"
    echo "loadavg: $(cat /proc/loadavg)"
    echo "params: COUNT=$COUNT MIN_MS=$MIN_MS MAX_MS=$MAX_MS IDLE_S=$IDLE_S SEED=$SEED"
    echo "configs: $CONFIGS"
    echo "git: $(git rev-parse --short HEAD 2>/dev/null)$(git diff --quiet 2>/dev/null || echo '+dirty')"
} > "$OUT/env.txt"

for cfg in $CONFIGS; do
    if [[ $cfg == irq ]]; then
        label=irq
        mode=(--mode=irq)
    else
        poll_us=${cfg#poll:}
        label=poll_${poll_us}us
        mode=(--mode=poll "--poll-us=$poll_us")
    fi
    common=(--pir-pin=6 --range=off --quiet)
    echo "== $label: idle ${IDLE_S}s" >&2
    rm -f "$RAW/idle_$label.csv"
    "$BIN" "${mode[@]}" "${common[@]}" --duration-s="$IDLE_S" --out="$RAW/idle_$label.csv" \
        2> "$RAW/idle_$label.log"
    idle=$(cpu_pct "$RAW/idle_$label.log")

    echo "== $label: load, $COUNT edges" >&2
    rm -f "$RAW/events_$label.csv"
    "$BIN" "${mode[@]}" "${common[@]}" --out="$RAW/events_$label.csv" 2> "$RAW/load_$label.log" &
    recv=$!
    sleep 1
    "$STIM" --count="$COUNT" --min-ms="$MIN_MS" --max-ms="$MAX_MS" --seed="$SEED" \
        --notify-pid="$recv" --out="$RAW/stim_$label.csv" 2>> "$RAW/load_$label.log"
    wait "$recv"
    load=$(cpu_pct "$RAW/load_$label.log")

    "$ANA" --stim="$RAW/stim_$label.csv" --events="$RAW/events_$label.csv" --label="$label" \
        --latency-out="$OUT/latency_$label.csv" --hist-out="$OUT/hist_$label.csv" \
        --summary="$OUT/summary.csv" --load-cpu-pct="$load" --idle-cpu-pct="$idle"
done

echo >&2
column -s, -t < "$OUT/summary.csv"
