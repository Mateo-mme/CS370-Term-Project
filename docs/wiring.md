# Wiring (Raspberry Pi 3 Model B, BCM numbering)

| Signal            | Pi GPIO (BCM) | Direction | Notes                                   |
|-------------------|---------------|-----------|-----------------------------------------|
| PIR OUT           | GPIO26        | input     | PIR module output is 3.3 V logic        |
| HC-SR04 TRIG      | GPIO23        | output    | 3.3 V drive is enough to trigger        |
| HC-SR04 ECHO      | GPIO24        | input     | **5 V output, through voltage divider** |
| Loopback stimulus | GPIO5         | output    | Phase 5 benchmark only; jumper to GPIO6 |
| Loopback input    | GPIO6         | input     | Phase 5 benchmark only                  |

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

## Physical checks performed

| Date       | Check                                   | Result                                                      |
|------------|-----------------------------------------|-------------------------------------------------------------|
| 2026-10-28 | `crop_guard --probe` (irq and poll)     | Lines 23/24/26 requested OK; PIR=0, ECHO=0 at idle (6 runs) |

Observed once: ECHO read 1 when probes ran back-to-back. Re-requesting TRIG can
glitch the line and fire a ping. The ranging code waits ≥ 60 ms after opening
the lines before the first measurement.
