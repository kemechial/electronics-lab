# exp07c: PWM pin check, PB0 vs PB6

Firmware: [src/exp07c_pwm_pin_check/](../../src/exp07c_pwm_pin_check/) · PlatformIO env: `exp07c_pwm_pin_check` · Board: STM32F103C8T6 Blue Pill, 72 MHz.

## Purpose

Test PB0 (TIM3_CH3) against PB6 (TIM4_CH1) on a chip known to be a **genuine ST** STM32F103, using the same firmware for both pins. On two clone chips, TIM3_CH3 gave no PWM on PB0 while TIM4_CH1 worked on PB6 (exp07a).

## Firmware

- Constant **65 %** PWM at **1 kHz**, hardware PWM on PB0 (TIM3_CH3) and PB6 (TIM4_CH1) at the same time. PSC = 1, ARR = 35,999, CCR = 23,400.
- Timer clock 2 × PCLK1 = 72 MHz, read at run time from RCC_CFGR (RM0008 p.94).
- PC13 on-board LED blinks at 1 Hz as a heartbeat. No UART output.
- Sources in code comments: DS5319 Table 5 p.29 (PB0) and p.32 (PB6); RM0008 Tables 43–44 p.178, p.387 (PWM mode), Tables 20–21 p.161 (pin modes), p.94 (timer clock).

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
| Genuine ST (to fill) | | | PB0 | | | | | |
| Genuine ST (to fill) | | | PB6 | | | | | |

Reference: **clones (CPUID 0x412FC230, r2p0): no PWM on PB0 on two chips, see the [exp07a README](../exp07a-led-pattern/README.md); not re-measured here.**

## Conclusion rule

| Result on the genuine chip | Conclusion |
|----------------------------|------------|
| PB0 works (and PB6 works) | Clone difference confirmed |
| PB0 does not work, PB6 works | Clone cause eliminated: investigate firmware/circuit with the logic analyzer |
| Neither works | Contact or loading problem: repeat |
