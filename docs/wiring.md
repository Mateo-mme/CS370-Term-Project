# Wiring (Raspberry Pi 3 Model B)

The code uses **BCM** GPIO numbers. Physical pin numbers count along the
40-pin header from pin 1 (3.3 V, at the end nearest the SD card): odd numbers
in the outer column, even numbers in the inner column. Python's
`GPIO.setmode(GPIO.BOARD)` uses physical numbers; `GPIO.BCM`, libgpiod and
this project use BCM numbers.

| Signal            | BCM    | Physical pin | Direction | Notes |
|-------------------|--------|--------------|-----------|-------|
| PIR VCC           | 5 V    | 2 or 4       | power     | |
| PIR GND           | GND    | 39 (any GND) | power     | |
| PIR OUT           | GPIO26 | 37           | input     | 3.3 V logic. Requested with the internal **pull-down** (see below) |
| HC-SR04 VCC / GND | 5 V / GND | 2 or 4 / any GND | power | |
| HC-SR04 TRIG      | GPIO23 | 16           | output    | 3.3 V drive is enough to trigger |
| HC-SR04 ECHO      | GPIO24 | 18           | input     | **5 V output, through a voltage divider** |
| Loopback stimulus | GPIO5  | 29           | output    | benchmark only; jumper to pin 31 |
| Loopback input    | GPIO6  | 31           | input     | benchmark only |

## Input pull-down

`gpio_libgpiod` requests every input with the Pi's internal pull-down
(~50 kΩ). Without it, a PIR that is not driving its output (unpowered,
disconnected, or idle on modules that only drive high) leaves GPIO26
floating. A floating input picks up noise; a hand waved near the wires is
enough. During Milestone 1 a script with no pull resistor "detected motion"
this way while the PIR was in fact dead (see `tests/hw/RESULTS.md`). The
pull-down is harmless for ECHO (already 2.2 kΩ to ground) and for the driven
loopback.

Quick check that the PIR drives the pin: this prints 1 only while you wave.

```sh
python3 -c "import RPi.GPIO as G,time; G.setmode(G.BOARD); G.setup(37,G.IN,pull_up_down=G.PUD_DOWN); [print(G.input(37), flush=True) or time.sleep(0.5) for _ in range(40)]"
```

## HC-SR04 ECHO voltage divider

Raspberry Pi GPIO pins are **not 5 V tolerant**. ECHO is wired like this:

```
HC-SR04 ECHO ──[ R1 = 1 kΩ ]──┬── GPIO24
                              │
                         [ R2 = 2.2 kΩ ]
                              │
                             GND
```

V_GPIO = 5 V × R2 / (R1 + R2) = 5 × 2.2 / 3.2 ≈ **3.44 V**

This is about 0.14 V above the 3.3 V I/O rail. It is under the commonly cited
absolute maximum (VDD_IO + 0.3 V ≈ 3.6 V), but it leaves little margin. Recommended options:

- Measure ECHO-high at GPIO24 and record it below.
- If it reads above 3.3 V, change R1 to 1.2 kΩ (≈ 3.24 V) or R2 to 2.0 kΩ (≈ 3.33 V).

Measured ECHO-high at GPIO24: _not yet measured_

## Physical checks

See `tests/hw/README.md` for procedures and `tests/hw/RESULTS.md` for the log.

Re-requesting TRIG can glitch the line and fire a ping (ECHO once read 1 when
probes ran back-to-back). The pipeline waits 60 ms after opening the lines
before the first trigger.
