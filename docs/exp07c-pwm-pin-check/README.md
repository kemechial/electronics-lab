# exp07c: PWM pin check, PB0 vs PB6

Firmware: [src/exp07c_pwm_pin_check/](../../src/exp07c_pwm_pin_check/) · PlatformIO env: `exp07c_pwm_pin_check` · Board: STM32F103C8T6 Blue Pill, 72 MHz.

## Purpose

Test PB0 (TIM3_CH3) against PB6 (TIM4_CH1) on a chip known to be a **genuine ST** STM32F103, using the same firmware for both pins. On two clone chips, TIM3_CH3 gave no PWM on PB0 while TIM4_CH1 worked on PB6 (exp07a).

## Firmware

- Constant **65 %** PWM at **1 kHz**, hardware PWM on PB0 (TIM3_CH3) and PB6 (TIM4_CH1) at the same time. PSC = 1, ARR = 35,999, CCR = 23,400.
- Timer clock 2 × PCLK1 = 72 MHz, read at run time from RCC_CFGR (RM0008 p.94).
- PC13 on-board LED blinks at 1 Hz as a heartbeat. No UART output.
- Sources in code comments: DS5319 Table 5 p.29 (PB0) and p.32 (PB6); RM0008 Tables 43–44 p.178, p.387 (PWM mode), Tables 20–21 p.161 (pin modes), p.94 (timer clock).

## Builds

| Env | Board | Pins |
|-----|-------|------|
| `exp07c_pwm_pin_check` | `bluepill_f103c8` (medium-density, 64 KB / 20 KB) | PB0 = TIM3_CH3, PB6 = TIM4_CH1 |
| `exp07c_c6` | `bluepill_f103c6` (low-density, 32 KB / 10 KB) | PB0 = TIM3_CH3 (DocID15060 Table 5 p.27), PA1 = TIM2_CH2 (DocID15060 Table 5 p.26; RM0008 Table 45 p.179). The low-density parts have no TIM4 (DocID15060 pp.18–19), so PA1 replaces PB6 as the reference. |

The C8 build flashed onto the C6 board crashed before the PWM started (timer registers 0): its RAM layout needs 20 KB, and the C6 has 10 KB (DocID15060 Table 2, p.11).

## Expected (calc)

| Quantity | Expected |
|----------|----------|
| Frequency | 1000 Hz |
| Duty | 65 % |
| DC average (meter) | 0.65 × 3.3 V = **2.1 V** (calc; scales with the actual rail) |
| GPIOB_CRL | PB0 nibble (bits 3:0) and PB6 nibble (bits 27:24) = 0xA: AF push-pull, 2 MHz (RM0008 Tables 20–21, p.161) |
| CPUID | 0x411FC231 = Cortex-M3 r1p1 (RM0008 p.94 names r1p1; the hex value is derived from it, calc) |
| DBGMCU_IDCODE DEV_ID[11:0] | 0x410, medium-density (RM0008 pp.1087–1088) |

## Results

| Chip | CPUID | DEV_ID | Pin | Measured Hz | Measured duty | DC average | GPIOB_CRL raw | State line |
|------|-------|--------|-----|-------------|---------------|------------|---------------|------------|
| STM32F103C6, r1p1 core (genuine per CPUID) | 0x411FC231 | 0x412 (low-density) | PB0 (TIM3_CH3) | **1000 Hz** (meter) | **64.9 %** (meter; set 65.0 %) | **2.154 V** | 0x4448444A (PB0 = 0xA) | env `exp07c_c6`, 2026-10-10; PC13 blinking; power source not recorded |
| same | 0x411FC231 | 0x412 | PA1 (TIM2_CH2, reference) | **1000 Hz** (meter) | **64.9 %** (meter) | **2.154 V** | GPIOA_CRL 0x444444A4 (PA1 = 0xA) | same |

Reference: **clones (CPUID 0x412FC230, r2p0): no PWM on PB0 on two chips, see the [exp07a README](../exp07a-led-pattern/README.md); not re-measured here.**

Register check (OpenOCD, read-only): TIM3 and TIM2 counting (CR1 = 0x81), PSC = 1, ARR = 35,999, CCR3 = CCR2 = 23,400 (65 %). **calc:** 2.154 V / 0.65 = 3.31 V implied rail.

## Result

TIM3_CH3 output does not reach PB0 on two r2p0 clone C8 chips (CPUID 0x412FC230, DEV_ID 0x410) but does on one r1p1 C6 chip (CPUID 0x411FC231, DEV_ID 0x412, RM0008 p.1088), with the same PB0 configuration (nibble 0xA). Both datasheets list TIM3_CH3 on PB0 (C8: DS5319 Table 5 p.29; C6: DocID15060 Table 5 p.27). **Clone difference is the most likely cause; the density difference (C6 vs C8) is a confounder; a genuine C8 would separate them.**

## Conclusion rule

| Result on the genuine chip | Conclusion |
|----------------------------|------------|
| PB0 works (and PB6 works) | Clone difference most likely (state any confounder, e.g. density) |
| PB0 does not work, PB6 works | Clone cause eliminated: investigate firmware/circuit with the logic analyzer |
| Neither works | Contact or loading problem: repeat |
