# Experiment 6: NPN transistor as an LED switch (Blue Pill)

Firmware: [src/exp06_transistor_led/](../../src/exp06_transistor_led/) · PlatformIO env: `exp06_transistor_led` · Board: STM32F103C8T6 Blue Pill (clone), 72 MHz from the 8 MHz crystal.

Sources (see [docs/ref/INDEX.md](../ref/INDEX.md)): DS5319 Rev 20 (STM32F103x8/xB datasheet), RM0008 Rev 21, Diotec 2N2222A (`2n2222a.pdf`, Version 2026-03-10). The onsemi P2N2222A datasheet is **not** in docs/ref.

## Circuit

```
3V3 ── R_LED 323.6 Ω ── LED anode ─|>|─ LED cathode ── C
PB0 (push-pull) ── RB ─────────────────────────────── B    NPN "2222"
GND ───────────────────────────────────────────────── E
```

- **Firmware:** PB0 is HIGH for 2 s, then LOW for 2 s, timed with `HAL_GetTick` (no `HAL_Delay`). The PC13 on-board LED is on while PB0 is HIGH. There is no UART output, because the USART1 wiring isn't confirmed.
- **PB0:** LQFP48 pin 18, I/O, **not 5 V tolerant (not FT)** (DS5319 Table 5, p.29). Alternate function TIM3_CH3; TIM3 remap in RM0008 Table 44, p.178.

## Transistor

The transistor is a **"2222" NPN. Its exact variant and pin order are still to be confirmed by the user.**

| Item | Value | Source / status |
|------|-------|-----------------|
| Exact part (P2N2222A, 2N2222A, PN2222A, …) | | to be given by the user |
| Pin order (pin 1, 2, 3) | | to be given by the user |
| β (hFE), multimeter, correct orientation | **223** | measured (user) |
| β, collector and emitter swapped | **13** | measured (user). Cause: pin order differs between 2222 variants (TO-92 C-B-E vs E-B-C), so a wrong-variant pinout swaps C and E |

### Datasheet values: same family, not verified for this exact part

| Parameter | Datasheet | Value | Status |
|-----------|-----------|-------|--------|
| Pin order | P2N2222A p.1 | — | same family, not verified for this exact part; datasheet not in docs/ref |
| hFE | P2N2222A p.2 | — | same family, not verified for this exact part; datasheet not in docs/ref |
| VCE(sat) | P2N2222A p.2 | — | same family, not verified for this exact part; datasheet not in docs/ref |
| VBE(sat) | P2N2222A p.2 | — | same family, not verified for this exact part; datasheet not in docs/ref |
| 10 mA readings | P2N2222A p.5, Fig. 11 | — | same family, not verified for this exact part; datasheet not in docs/ref |
| hFE at IC = 10 mA, VCE = 10 V | Diotec 2N2222A p.2 | ≥ 75 | same family, not verified for this exact part |
| hFE at IC = 150 mA, VCE = 10 V | Diotec 2N2222A p.2 | 100–300 | same family, not verified for this exact part |
| VCE(sat) at IC = 150 mA, IB = 15 mA | Diotec 2N2222A p.1 | ≤ 0.3 V | same family, not verified for this exact part |
| VCEO, IC max, Ptot | Diotec 2N2222A p.1 | 40 V, 600 mA, 625 mW | same family, not verified for this exact part |

## Calculation

| Quantity | Formula | Value | Status |
|----------|---------|-------|--------|
| LED current | I_LED = (V_3V3 − V_F − VCE(sat)) / 323.6 Ω | needs V_F and VCE(sat) | calc, pending measurement |
| Base current | IB = (V_PB0,high − VBE) / RB | **1.2–2.6 mA** | user-given; RB not yet recorded |
| PB0 drive check | IB vs normal drive ±8 mA (DS5319 Table 37, p.64) | 1.2–2.6 mA < 8 mA: OK | calc |
| PB0 absolute maximum | IB vs IIO ±25 mA (DS5319 Tables 6–7, p.37) | well below 25 mA | calc |
| PB0 voltage | PB0 is not FT (DS5319 Table 5, p.29) | keep the base circuit ≤ 3.3 V | datasheet |
| Saturation margin | forced β = I_LED / IB vs measured β 223 | pending I_LED | calc, pending |

## Measurement (to fill)

| Quantity | Calculated | Measured | Deviation |
|----------|------------|----------|-----------|
| V_3V3 | 3.3 V nominal | | |
| RB | | | |
| V_PB0 (HIGH) | | | |
| VBE (on) | | | |
| VCE (on) | | | |
| Voltage across R_LED → I_LED | | | |
| IB | 1.2–2.6 mA | | |
| VCE (off) | ≈ V_3V3 − V_F(leakage) | | |
