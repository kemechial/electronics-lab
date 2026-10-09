# Experiment 6: NPN transistor as an LED switch (Blue Pill)

Firmware: [src/exp06_transistor_led/](../../src/exp06_transistor_led/) · PlatformIO env: `exp06_transistor_led` · Board: STM32F103C8T6 Blue Pill (clone), 72 MHz from the 8 MHz crystal.

Sources (see [docs/ref/INDEX.md](../ref/INDEX.md)): Diotec 2N2222A (`2n2222a.pdf`, TO-92, Version 2026-03-10, the datasheet for this part), DS5319 Rev 20 (STM32F103x8/xB), RM0008 Rev 21. onsemi P2N2222A: family reference only (different manufacturer, different pin order), not in docs/ref.

## Circuit

```
3V3 ── R_LED 323.6 Ω ── LED anode ─|>|─ LED cathode ── C (pin 3)
PB0 (push-pull) ── RB ─────────────────────────────── B (pin 2)   Diotec 2N2222A
GND ───────────────────────────────────────────────── E (pin 1)
```

- **Firmware:** PB0 is HIGH for 2 s, then LOW for 2 s, timed with `HAL_GetTick` (no `HAL_Delay`). The PC13 on-board LED is on while PB0 is HIGH. There is no UART output, because the USART1 wiring isn't confirmed.
- **PB0:** LQFP48 pin 18, I/O, **not 5 V tolerant (not FT)** (DS5319 Table 5, p.29). Alternate function TIM3_CH3; TIM3 remap in RM0008 Table 44, p.178.

## Transistor: Diotec 2N2222A

| Parameter | Value | Source |
|-----------|-------|--------|
| Pin order | **1 = E, 2 = B, 3 = C** | Diotec p.1 (symbol); orientation verified by the user with a multimeter |
| β, multimeter, correct orientation | 223 | measured (user) |
| β, collector and emitter reversed | 13 | measured (user) |
| VCEO / VCBO / VEBO | 40 V / 75 V / 6 V | Diotec p.1 |
| IC / ICM | 600 mA / 800 mA | Diotec p.1 |
| Ptot | 625 mW | Diotec p.1 |
| VCE(sat) | ≤ 0.3 V at IC = 150 mA, IB = 15 mA | Diotec p.1 |
| hFE min, VCE = 10 V | 35 at 0.1 mA, 50 at 1 mA, 75 at 10 mA; 100–300 at 150 mA | Diotec p.2 |
| RthJA | 200 K/W | Diotec p.2 |
| VBE(sat), switching times | **not in this datasheet** | — |
| VBE (on) | 0.7 V | **assumption**: onsemi P2N2222A Fig. 11, different manufacturer, family reference only; to be measured |

## Calculation

LED current, as given: **I_LED ≈ 3.2–4.5 mA** with R_LED = 323.6 Ω, from I_LED = (V_3V3 − V_F − VCE(sat)) / R_LED. That is not 10 mA, so the 10 mA datasheet points don't apply directly.

**calc:** base current IB = (3.3 V − 0.7 V) / RB (VBE assumed), forced β = I_LED / IB:

| RB | IB | Forced β (I_LED 3.2–4.5 mA) | vs hFE min 50 at 1 mA (Diotec p.2) |
|----|----|-----------------------------|-------------------------------------|
| 1 kΩ | 2.60 mA | 1.2–1.7 | ≪ 50: saturated |
| 2.2 kΩ | 1.18 mA | 2.7–3.8 | ≪ 50: saturated |
| 4.7 kΩ | 0.55 mA | 5.8–8.1 | ≪ 50: saturated |

All three RB values drive the transistor deep into saturation (forced β well below hFE min).

**PB0 drive check (calc):** IB ≤ 2.6 mA is below the ±8 mA normal drive (DS5319 Table 37, p.64) and the ±25 mA absolute maximum (DS5319 Tables 6–7, p.37). PB0 is not FT (DS5319 p.29), so keep the base circuit at ≤ 3.3 V.

## Measurement (2026-10-09, user, multimeter)

RB value: **<to be given by the user>**. Values marked "by subtraction" or "derived" still need a direct measurement.

| Quantity | Calculated | Measured | Deviation / note |
|----------|------------|----------|------------------|
| V_3V3 (rail) | 3.3 V nominal | ≈ 3.26 V | −0.04 V |
| R_LED | 323.6 Ω | 323.6 Ω (measured) | — |
| LED voltage V_F | — | 1.9 V | — |
| Voltage across R_LED | — | 1.3 V | — |
| IC = I_LED = V_R_LED / R_LED | 3.2–4.5 mA | **4.0 mA** | inside the calculated range |
| Voltage across RB | — | 2.52 V | — |
| IB = V_RB / RB | 2.60 mA (1 kΩ) / 1.18 mA (2.2 kΩ) | 2.52 mA (1 kΩ) / 1.15 mA (2.2 kΩ) | −3 % (1 kΩ), −3 % (2.2 kΩ) |
| VCE (on) | ≤ 0.3 V (Diotec p.1) | ≈ 0.06 V **by subtraction** (3.26 − 1.3 − 1.9) | to be measured directly |
| VBE (on) | 0.7 V (assumption) | ≈ 0.78 V derived with PB0 = 3.3 V; ≈ 0.74 V with PB0 = 3.26 V (PB0 can't exceed the rail) | to be measured directly |
| Forced β = IC / IB | 1.2–1.7 (1 kΩ) / 2.7–3.8 (2.2 kΩ) | **1.6** (1 kΩ) / **3.5** (2.2 kΩ) | inside the calculated ranges |

## Findings

- **Matches the calculation within meter resolution:** IC = 4.0 mA is inside the 3.2–4.5 mA range, IB is within 3 % of the calculation, and forced β is inside the calculated range for both RB values.
- **The transistor is deeply saturated:** forced β 1.6 or 3.5 is far below hFE min 50 at 1 mA (Diotec p.2), and VCE(on) ≈ 0.06 V (by subtraction) is well below VCE(sat) max 0.3 V (Diotec p.1).
- **Open:** the RB value; direct VCE and VBE measurements.
