# Experiment 7a: LED brightness pattern (hardware PWM, Blue Pill)

Firmware: [src/exp07a_led_pattern/](../../src/exp07a_led_pattern/) · PlatformIO env: `exp07a_led_pattern` · Board: STM32F103C8T6 Blue Pill (clone), 72 MHz.

## Goal

Play a repeating brightness pattern on the exp06 LED with hardware PWM on TIM4_CH1 (PB6). The duty is updated every 1 ms from SysTick, with no `HAL_Delay` and integer math only.

## Circuit

As [exp06](../exp06-transistor-led/README.md), except that **the base resistor is driven from PB6 instead of PB0**: PB6 → RB (2.186 kΩ) → base of the Diotec 2N2222A (1 = E, 2 = B, 3 = C); LED with R_LED = 323.6 Ω from 3V3 to the collector; emitter to GND.

## PWM

| Item | Value | Source |
|------|-------|--------|
| Pin | PB6 (LQFP48 pin 42, FT) = TIM4_CH1, default alternate function I2C1_SCL/TIM4_CH1 | DS5319 Table 5, p.32 |
| Remap | none: TIM4_REMAP = 0 (reset value) puts CH1 on PB6 | RM0008 Table 43, p.178 |
| Timer clock (TIM4, APB1) | 2 × PCLK1 = 72 MHz (APB1 prescaler /2); computed at run time from RCC_CFGR | RM0008 p.94 |
| Mode | PWM mode 1 (OC1M = 110), CCR1 preload (OC1PE) | RM0008 p.387, TIMx_CCMR1 p.413 |
| PWM_HZ | 1000 Hz | config.h |
| Prescaler | PSC = 1 (36 MHz count). PSC = 0 would need ARR = 71,999 > 16 bits | calc |
| **Duty resolution** | **36,000 steps** (ARR = 35,999), 27.8 ns per step | calc |

## Pattern

Defined in `config.h` as a table of `{start_percent, end_percent, duration_ms}` segments, repeated forever. The brightness ramps linearly from start to end in 0.01 % steps; segments with duration 0 are skipped.

| Segment | Start % | End % | Duration |
|---------|---------|-------|----------|
| 1 | 100 | 0 | 2000 ms |
| 2 | 0 | 0 | 1000 ms |
| 3 | 0 | 100 | 3000 ms |

**Brightness mapping:** `GAMMA` in config.h, default 0 (linear: duty = percent). With `GAMMA 1`, the percent goes through a precomputed gamma 2.2 table (`gamma_table.h`, 101 entries, values 0..65535, interpolated between entries) before scaling to the 36,000 steps.

### How to add a pattern

1. In `config.h`, add a new table, e.g. `static const segment_t pattern_blink[] = { {100, 100, 500}, {0, 0, 500} };`.
2. Set `#define ACTIVE_PATTERN pattern_blink`.

No other code changes: the segment count comes from `sizeof`.

## Size

Release build (`pio run -e exp07a_led_pattern`, GAMMA 0): 4,660 B flash, 128 B RAM. With GAMMA 1: 4,908 B flash.

## Findings

- **TIM3_CH3 on PB0 did not work on this board (2026-10-10).** Over the ST-Link, TIM3 read as correctly configured: counting, PSC = 1, ARR = 35,999, PWM mode 1 with preload, CC3E set, CCR3 following the pattern. PB0 was in AF push-pull mode, and AFIO_MAPR = 0 (no remap, so CH3 should be on PB0). Even so, PB0 never read high: 0 of 81 samples taken at duty > 50 %, and the meter showed about 0 V.
- **PB0 itself is fine:** exp06 drives it as a plain GPIO without problems.
- **The chip is a clone:** the CPU reports Cortex-M3 r2p0 (CPUID 0x412FC230), while RM0008 gives r1p1 for the STM32F103 (pp.1084–1085, 1092–1093). The TIM3_CH3 → PB0 path on this clone is not covered by ST's documents; cause not determined.
- **TIM4_CH1 on PB6 works:** the LED shows the expected fade pattern (user observation).

## Optional later step

Logic analyzer capture on PB6 to verify the 1 kHz frequency and the duty. The clip needs to be placed on the PB6 row first; the user will be asked before this.
