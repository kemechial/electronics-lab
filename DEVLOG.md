# DEVLOG

Flash/RAM after every successful release build (`pio run`). RAM = `.data` + `.bss` (static), as reported by PlatformIO.

| Date | Env | Change | Flash (B) | RAM (B) | Budget |
|------|-----|--------|-----------|---------|--------|
| 2026-10-07 | exp01_rc_step | First build: RC step, measured R = 9830 Ω, C = 10460 nF, VDDA = 3266 mV, N_SAMPLES = 800, DEBUG_ENABLED = 1 | 7,480 | 3,568 | < 20 KB / < 16 KB ✅ |

### 2026-10-07 exp01_rc_step: top 10 symbols (baseline)

| Symbol | Section | Size (B) |
|--------|---------|----------|
| discharge_buf | .bss | 1,600 |
| charge_buf | .bss | 1,600 |
| main | .text | 988 |
| HAL_RCC_OscConfig | .text | 854 |
| __udivmoddi4 (64-bit division, libgcc) | .text | 716 |
| HAL_GPIO_Init | .text | 436 |
| g_pfnVectors | .isr_vector | 404 |
| HAL_ADC_Init | .text | 332 |
| HAL_RCC_ClockConfig | .text | 308 |
| HAL_ADC_ConfigChannel | .text | 288 |

The two sample buffers (2 × 800 × uint16) account for 3,200 B of the RAM.

## 2026-10-07 exp01_rc_step: first measurement, analysis tooling (no firmware change)

- **Hypothesis:** with the measured C (10.46 µF), the measured τ should match R·C = 102,822 µs to within the component measurement uncertainty. The low plateau (66 mV below 3V3) and the non-zero discharge tail (8 to 12 counts) come from leakage, ADC reference/gain, ADC/ground offset, source impedance or dielectric absorption. All of these are untested; see docs/exp01-rc-step/README.md.
- **Change:** added `scripts/analyze_rc.py`, a least-squares exponential fit per phase with τ ± standard error, RMS residual and a plot to `docs/exp01-rc-step/rc_curves.png`. Filled in the README Measurement and Deviation sections. Firmware unchanged, so no new size entry.
- **Observation:** charge τ = 101,954 µs (−0.84 %), discharge τ = 104,713 µs (+1.84 %), plateau 3200 mV, tail 8 to 12 counts, charge V_0 = 17 mV. With nominal C = 10 µF the deviations would be +3.7 % / +6.5 %.
- **Conclusion:** inconclusive until the fit and the calibration test are done. The measured C explains most of the deviation from nominal. The 2.7 % charge/discharge asymmetry is not explained.
- **Next step:** DC calibration test with the multimeter: compare ADC readings with DMM readings at 0 V, at mid-scale and with PB0 held HIGH. Also run `analyze_rc.py` on a captured log.
