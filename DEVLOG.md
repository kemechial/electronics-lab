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
