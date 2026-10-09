# Hardware notes

## Logic analyzer: Saleae Logic clone (FX2, 8 channels)

- USB ID 0925:3881, WinUSB driver (Saleae LLC), sigrok driver `fx2lafw`. Details and sources: [docs/ref/logic-analyzer.md](docs/ref/logic-analyzer.md).
- The small "PWR" text near CH2 on the label is an indicator LED. All 8 channels are normal inputs.
- Connect only to breadboard rows, never directly to the Black Pill pins.
- USB: on 2026-10-09 the analyzer failed to enumerate on two hub ports ("port reset failed", or not seen at all) and works on port 1 of the Realtek USB 2.0 hub (0BDA:5411). If it drops out, suspect the port first.

### Channel map (verified 2026-10-09)

| Clip label | sigrok channel | Breadboard row | Signal | Logic | Verified by |
|------------|----------------|----------------|--------|-------|-------------|
| CH1 | **D0** | PA6 | DHT11 DATA, 4.7 kΩ pull-up | 3.3 V | 19,998 µs low start pulse + 86 edges in one read |
| CH2 | **D1** | PA9 | USART1 TX, 115200 8N1 | 3.3 V | 8.5 µs minimum pulse (1 bit = 8.68 µs); `uart` decoder on D1 gives the firmware's text line with no warnings |
| GND | — | Black Pill GND | ground | — | — |
| CH3–CH8 | D2–D7 | not connected | — | read constant 1 | no edges in 1.2 s |

**Method:** one 1.2 s capture, 4 MHz, all 8 channels, with CH1 and CH2 both connected. Each signal was identified from its waveform. This replaces the planned one-clip-at-a-time test. No cross-coupling was seen: D0 shows no UART-like pulses, and D1 shows no 20 ms pulse.
