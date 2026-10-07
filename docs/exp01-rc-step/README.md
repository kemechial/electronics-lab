# Experiment 1: RC step response

Firmware: [src/exp01_rc_step/](../../src/exp01_rc_step/) · PlatformIO env: `exp01_rc_step`

## Goal

Apply a voltage step to an RC low-pass network and measure the charge and discharge curves of the capacitor with a timer-triggered ADC. Determine the time constant τ from the measured curves and compare it to the value calculated from the measured R and C.

## Circuit

```
PB0 (GPIO push-pull) ──[ R 9.83 kΩ ]──┬── PA1 (ADC1_IN1)
                                      │
                                    [ C 10.46 µF ]
                                      │
GND ──────────────────────────────────┘

PA9 (USART1 TX) ──> USB-UART RX, 115200 8N1      PC13 LED blinks during capture
```

| Item | Value | Source |
|------|-------|--------|
| R | 9830 Ω | measured (DMM) |
| C | 10.46 µF (10460 nF) | measured |
| VDDA / step amplitude | 3266 mV | measured at the 3V3 pin |
| Sample period | 1000 µs (TIM2 TRGO → ADC1) | config |
| Samples per curve | 800 (800 ms ≈ 7.8 τ) | config |
| ADC sample time | 480 cycles at 21 MHz → 23.4 µs per conversion | config |

Electrolytic capacitor: the + lead goes to the node, the − lead to GND.

Sequence (every 10 s): PB0 LOW for 3 s → PB0 HIGH and capture the charge curve → hold HIGH for 0.5 s → PB0 LOW and capture the discharge curve → print the CSV and the summary.

## Calculation

Time constant from the measured components (integer math, as in `config.h`):

τ = R · C = 9830 Ω × 10460 nF = 102,821,800 ns ≈ **102,822 µs ≈ 102.8 ms**

Charge: V(t) = V_final · (1 − e^(−t/τ)). Discharge: V(t) = V_0 · e^(−t/τ). With V = 3266 mV and ADC counts = mV × 4095 / 3266:

| t | t / τ | Charge (mV) | Charge (counts) | Discharge (mV) | Discharge (counts) |
|---|-------|-------------|-----------------|----------------|--------------------|
| 0 ms | 0 | 0 | 0 | 3266 | 4095 |
| 10 ms | 0.10 | 303 | 380 | 2963 | 3715 |
| 50 ms | 0.49 | 1258 | 1577 | 2008 | 2518 |
| 103 ms | 1.0 | 2067 (63.2 %) | 2591 | 1199 (36.8 %) | 1504 |
| 206 ms | 2.0 | 2826 | 3543 | 440 | 552 |
| 308 ms | 3.0 | 3103 | 3890 | 163 | 205 |
| 514 ms | 5.0 | 3244 | 4067 | 22 | 28 |
| 799 ms | 7.8 | 3265 | 4093 | 1 | 2 |

How the firmware measures τ: V_0 = first sample, V_final = mean of the last 16 samples, threshold = V_0 + 0.632 · (V_final − V_0). τ is the first time the curve crosses that threshold, linearly interpolated between the two samples around it. For the discharge, the same formula gives the 36.8 % point. After 7.8 τ the curve is still 0.04 % short of its end value, which biases τ low by about 0.05 %; that is negligible.

## Measurement

_To be filled in from the serial monitor output._

| Quantity | Calculated | Measured (charge) | Measured (discharge) | Deviation |
|----------|------------|-------------------|----------------------|-----------|
| Final voltage | 3266 mV / 0 mV | | | |
| τ | 102,822 µs | | | |

## Deviation and cause

_To be filled in after the measurement._ Possible causes to check:

- Tolerance and measurement error of the C value (the DMM capacitance range is usually ±1 to 3 %).
- GPIO output resistance (a few tens of Ω) adds to R, so τ comes out roughly 0.3 % higher.
- Capacitor leakage puts a resistor in parallel with C. That lowers V_final and the effective τ.
- PB0 HIGH may not reach VDDA exactly. If it does reach VDDA, the ADC reads 4095 and clips at the top of the charge curve.
- Timing: sample k is triggered at k × 1000 µs. The ADC holds its sample about 23 µs after the trigger. That is a 0.02 % offset.

## Findings

_To be filled in._
