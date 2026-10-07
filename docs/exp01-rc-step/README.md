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

### Firmware summary (threshold crossing), first run

| Quantity | Calculated (measured C = 10.46 µF) | Measured, charge | Measured, discharge |
|----------|------------------------------------|------------------|---------------------|
| Start voltage V_0 | 0 mV / 3266 mV | 17 mV (≈ 21 counts) | 3215 mV |
| Final voltage | 3266 mV / 0 mV | 3200 mV (≈ 4012 counts, 66 mV below 3V3) | 8 mV (tail at 8 to 12 counts ≈ 6 to 10 mV) |
| τ | 102,822 µs | 101,954 µs | 104,713 µs |
| Deviation from R·C | — | −0.84 % | +1.84 % (firmware prints +1.83: integer truncation) |

mV values are ADC counts × 3266 / 4095, so they assume VDDA = 3266 mV.

### Nominal vs measured component value

| C used for R·C | Expected τ | Charge deviation | Discharge deviation |
|----------------|------------|------------------|---------------------|
| 10 µF nominal | 98,300 µs | +3.7 % | +6.5 % |
| 10.46 µF measured | 102,822 µs | −0.84 % | +1.84 % |

Using the measured C instead of the nominal value cuts the deviation by about a factor of 4. The charge and discharge results still differ by 2.7 % from each other. A single R·C cannot produce that difference.

### Least-squares fit

_Pending._ Capture a log and run the fit:

```
pio device monitor --baud 115200 --port COM5 --filter log2file
python scripts/analyze_rc.py logs/device-monitor-YYMMDD-HHMMSS.log
```

Run the monitor from the project root. The log is written to `logs/device-monitor-YYMMDD-HHMMSS.log`. The script fits V(t) = Vf + (V0 − Vf)·e^(−t/τ) with Vf, V0 and τ free for each phase. It prints τ ± its standard error and the RMS residual, and saves `rc_curves.png` (points, fits, residuals) in this folder.

| Phase | τ fit (µs) | Standard error (µs) | RMS residual (mV) | Deviation from R·C |
|-------|------------|---------------------|-------------------|--------------------|
| charge | | | | |
| discharge | | | | |

## Deviation and cause

All causes below are **hypotheses**. None has been tested yet. The τ deviation (−0.84 % charge, +1.84 % discharge) is within the likely uncertainty of the C measurement. The open questions are the charge/discharge asymmetry, the 66 mV low plateau and the non-zero discharge tail.

### Measurement resolution: estimate, not tested

Sample spacing is 1 ms, about 1 % of τ, so taking the nearest sample would give about 1 % resolution. The firmware interpolates linearly between the two samples around the threshold, which should do much better:

- The interpolation error on this curve is about 0.01 mV, which is negligible.
- At the crossing the curve moves about 11.5 mV/ms. Noise of ±2 counts (1.6 mV) then gives about ±0.14 ms, or ±0.14 % of τ.

On that estimate, resolution can't explain a 2.7 % charge/discharge difference. To confirm, look at the cycle-to-cycle spread in the fit output and compare the fitted τ with the threshold τ.

### H1. Capacitor leakage: untested

At steady state, a 66 mV drop across R means about 66 mV / 9830 Ω ≈ 6.7 µA flows through R. That would be an equivalent leakage resistance of about 480 kΩ across C.

- What it would explain: the low plateau.
- What doesn't fit: a constant leakage resistance would shorten **both** τ values by about 2 % (τ = (R ∥ R_leak)·C). The discharge τ is longer, so leakage alone doesn't fit.
- Test: hold PB0 HIGH and measure the voltage across R with the DMM, then repeat with C removed.

### H2. ADC reference or gain error: untested

The plateau reads 4012/4095 = 98.0 % of full scale. If PB0 HIGH reaches VDD and VDDA = VDD, the reading should be close to 4095.

- What it would explain: VDDA ≠ VDD, or a gain error in the ADC.
- Expected effect on τ: a purely linear gain error cancels out of τ. The threshold is taken relative to the measured V_0 and V_final, so it would explain the levels but not the τ deviation.
- Test: DC calibration. Apply known voltages, measure each with the DMM and compare with the ADC reading.

### H3. ADC offset or ground offset: untested

The discharge tail reads 8 to 12 counts (6 to 10 mV). An exponential with τ ≈ 105 ms should be at about 1.6 mV (2 counts) after 799 ms. The charge V_0 is 17 mV after 3 s of discharge.

- What it would explain: the tail level and V_0. Possible sources are ADC offset error, a voltage difference between the capacitor's ground return and the MCU ground, or PB0 LOW not being at 0 V.
- Expected effect on τ: a constant offset cancels out of τ, for the same reason as in H2.
- Test: DC calibration at 0 V (PA1 shorted to GND at the pin), and measure the ground difference with the DMM.

### H4. Source impedance effect: untested

The ADC samples through R, but the 10 µF capacitor holds the node. Charge sharing with the ADC sampling capacitor (a few pF) should therefore be negligible.

- Pin input leakage current flows through R. The datasheet maximum is on the order of 1 µA (check the F401 datasheet). That could shift the reading by up to about 10 mV, the same size as the tail offset.
- Different GPIO driver resistance for HIGH and LOW (tens of Ω) would change τ by at most about 0.5 %.
- Test: repeat the curve with a smaller R and a larger C (same τ), and compare the offsets.

### H5. Dielectric absorption or non-ideal electrolytic C: untested

A slow recovery component in an electrolytic capacitor would raise the tail and lengthen the apparent discharge τ. That fits the +1.84 % discharge result, the 8 to 12 count tail and the 17 mV start voltage.

- Test: check whether the fit residuals show a systematic slow component at the end of the discharge. Repeat the run with a film capacitor of similar value.

## Findings

_Inconclusive so far._ The measured-C calculation agrees with both τ values to within 2 %. Explaining the charge/discharge asymmetry, the plateau and the tail needs the least-squares fit and a DC calibration test.
