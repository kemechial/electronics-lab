# Experiment 7a: LED brightness pattern (hardware PWM, Blue Pill)

Firmware: [src/exp07a_led_pattern/](../../src/exp07a_led_pattern/) · PlatformIO env: `exp07a_led_pattern` · Board: STM32F103C8T6 Blue Pill (clone), 72 MHz.

## Goal

Play a repeating brightness pattern on the exp06 LED with hardware PWM on TIM3_CH3 (PB0). The duty is updated every 1 ms from SysTick, with no `HAL_Delay` and integer math only.

## Circuit

Unchanged from [exp06](../exp06-transistor-led/README.md): PB0 → RB (2.186 kΩ) → base of the Diotec 2N2222A (1 = E, 2 = B, 3 = C); LED with R_LED = 323.6 Ω from 3V3 to the collector; emitter to GND.

## PWM

| Item | Value | Source |
|------|-------|--------|
| Pin | PB0 = TIM3_CH3, default alternate function | DS5319 Table 5, p.29 |
| Remap | none: TIM3_REMAP = 00 (reset value) puts CH3 on PB0 | RM0008 Table 44, p.178 |
| Timer clock | 2 × PCLK1 = 72 MHz (APB1 prescaler /2); computed at run time from RCC_CFGR | RM0008 p.94 |
| Mode | PWM mode 1 (OC3M = 110), CCR3 preload (OC3PE) | RM0008 p.387, TIMx_CCMR2 p.416 |
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

## Optional later step

Logic analyzer capture on PB0 to verify the 1 kHz frequency and the duty. The clip needs to be placed on the PB0 row first; the user will be asked before this.
