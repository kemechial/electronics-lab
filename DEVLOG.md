# DEVLOG

Flash/RAM after every successful release build (`pio run`). RAM = `.data` + `.bss` (static), as reported by PlatformIO.

| Date | Env | Change | Flash (B) | RAM (B) | Budget |
|------|-----|--------|-----------|---------|--------|
| 2026-10-07 | exp01_rc_step | First build: RC step, measured R = 9830 Ω, C = 10460 nF, VDDA = 3266 mV, N_SAMPLES = 800, DEBUG_ENABLED = 1 | 7,480 | 3,568 | < 20 KB / < 16 KB ✅ |
| 2026-10-07 | exp01_rc_step | VDDA from VREFINT each cycle (VDDA_MV fallback); capture-start diagnostics: pre-edge node voltage, edge-to-first-sample delay (DWT) | 8,116 (+636) | 3,588 (+20) | < 20 KB / < 16 KB ✅ |
| 2026-10-07 | exp01_rc_step | Verified precondition replaces fixed 3 s discharge (node < 20 mV, 50 ms poll, 10 s timeout skips cycle) | 8,336 (+220) | 3,588 (+0) | < 20 KB / < 16 KB ✅ |
| 2026-10-08 | exp02_dht11 | New experiment: DHT11 on PA6, TIM3 both-edge input capture (IRQ), derived bit threshold | 7,444 | 692 | no budget set for exp02 |

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

## 2026-10-07 exp01_rc_step: runtime VDDA and capture-start diagnostics

- **Hypothesis:** H2 (ADC reference/gain) and H3 (offset) from docs/exp01-rc-step/README.md need the real VDDA instead of the 3266 mV constant. The charge V_0 = 17 mV could be either a node that wasn't fully discharged before the edge or a late first sample. Measuring the node right before the edge and the edge-to-first-sample delay separates these two cases.
- **Change:** every cycle measures VDDA from VREFINT (ADC1 channel 17, 480 cycles, 1 discarded + 16 averaged). VDDA = VREFINT_CAL × 3300 / average, the same formula as `__LL_ADC_CALC_VREFANALOG_VOLTAGE`. `VREFINT_CAL_ADDR` (0x1FFF7A2A) and `VREFINT_CAL_VREF` (3300) are present in the installed `stm32f4xx_ll_adc.h` (STM32CubeF4 1.28.3). Results outside 1700 to 3600 mV fall back to `VDDA_MV`. All mV conversions use the measured value. Before each edge, 4 back-to-back conversions of the node are averaged (pre_edge). The DWT cycle counter times the edge to the end of the first conversion (start_delay). New lines: `# vdda: ...` at the start of every cycle, and `# charge|discharge: pre_edge=... start_delay=...` before each phase's CSV rows. CSV format unchanged.
- **Size:** release build 8,116 B flash (+636), 3,588 B RAM (+20). Top symbols unchanged except `main` 988 → 1,260 B and `print_summary` (288 B) now in the top 10; no optimisation proposed.
- **Observation:** not yet run on hardware.
- **Expected:** start_delay ≈ 23 to 24 µs (480 + 12 ADC cycles at 21 MHz = 23.4 µs, plus poll latency). The sample itself is held about 0.6 µs before EOC. If charge pre_edge ≈ 17 mV, the node was not at 0 V before the edge, so the 17 mV is not a timing effect.
- **Next step:** flash, capture a log, compare `# vdda` with the DMM reading at the 3V3 pin, then the DC calibration test.

## 2026-10-07 exp01_rc_step: verified discharge precondition

- **Hypothesis:** the fixed 3 s discharge does not prove the capacitor is empty (charge V_0 was 17 mV in the first run). Checking the node voltage before every charge step makes the starting condition explicit and logged.
- **Change:** `DISCHARGE_MS` is removed. Before each charge, PB0 is driven LOW and the node is read at once and then every `PRECONDITION_POLL_MS` (50 ms), each read averaging 4 conversions in mV with the measured VDDA. It stops when the node is below `PRECONDITION_MV` (20 mV) or after `PRECONDITION_TIMEOUT_MS` (10000 ms). One line per cycle: `# precondition: node=<mV> mV after <ms> ms OK|TIMEOUT`. On TIMEOUT the cycle is skipped and retried in the next cycle. Steps b–e moved into `run_measurement()`, unchanged. Runtime VDDA (requested again this session) was already in place from the previous entry; τ is still computed from raw counts.
- **Size:** release build 8,336 B flash (+220), 3,588 B RAM (+0). Top 10 unchanged except `main` 1,260 → 1,428 B; no optimisation proposed.
- **Observation:** not yet run on hardware.
- **Expected:** the previous cycle's discharge capture, printing and idle time leave PB0 LOW for about 5 to 6 s before the next precondition. If the tail settles at 6 to 10 mV as in the first run, the precondition should usually pass on the first read (`after 0 ms OK`). The LOW time before the charge step is now about 3 s shorter than before. TIMEOUT would mean the node stays at 20 mV or more, which points at H3/H4/H5.
- **Next step:** flash, capture a log, check the precondition lines and the charge pre_edge/V_0 together.

## 2026-10-08 exp02_dht11: new experiment, DHT11 with TIM3 input capture

- **Hypothesis:** a DHT11 can be read without blocking: open-drain start pulse, TIM3_CH1 capture of both edges at 1 µs per tick, interrupt per edge, decode after the frame. The 0/1 threshold can come from each frame's own widths instead of a constant.
- **Documents read first:** DHT p.3–8 (text, plus Fig. 3/4/5 from images extracted from the PDF), DS p.26 Table 4 and p.44 Table 9, RM p.95, 154, 333, 364, 367. What was not read or not verified is listed in docs/exp02-dht11/README.md.
- **Change:** new env `exp02_dht11` and `src/exp02_dht11/` (config.h, dht11.c/.h, main.c, stm32f4xx_it.c). exp01 untouched; its release build is still 8,336 B flash / 3,588 B RAM.
- **Size:** release build 7,444 B flash, 692 B RAM. CLAUDE.md sets no budget for exp02 (the 20 KB / 16 KB budget is for exp01). Top 10: main 1,144, HAL_RCC_OscConfig 854, dht11_poll 760, __udivmoddi4 716, HAL_GPIO_Init 436, g_pfnVectors 404, HAL_RCC_ClockConfig 308, HAL_TIM_IC_ConfigChannel 294, dht11_init 232, s_t (.bss) 200. No optimisation proposed.
- **Observation:** not yet run on hardware. Python port of the decoder on 3,000 synthetic frames (jitter ±6 µs, 16-bit wrap, release edge present or absent, 10 % corrupted checksums): 0 failures.
- **Next step:** measure the pull-up with the DMM, flash, capture the first-read timing and edge dump, fill in the timing table and the reference comparison.

## 2026-10-08 exp02_dht11: hardware results, supply and reference tests (documentation only)

- **Hypothesis:** the TIM3 capture decoder reads the DHT11 reliably. The 3.3 V supply doesn't change the RH reading. The sensor agrees with a reference instrument within ±5 %RH (DHT p.3).
- **Change:** documentation only (docs/exp02-dht11/README.md, this entry). No firmware change, so no new size entry.
- **Observation (protocol):** several hundred reads with `crc_err=0`, `timeout=0`. Checksum verified by hand on sample frames. thr = 47 µs, w1 = 68–71 µs (datasheet 70 µs, DHT p.7–8), w0 = 21–26 µs (datasheet 26–28 µs).
- **Observation (supply, sensor 1):** 34 % at 3.3 V; 36 % from the 5V pin with USB-C only; 34 % from the 5V pin with the ST-Link back-feeding it (about 3.26 V measured).
- **Observation (reference):** reference 23 °C / 49 %RH. Sensor 1 34–36 %. Sensor 2 20–21 % (T 25 then 24 °C), marked suspect. Magnus calc: vapour pressure 13.7 hPa (reference) vs 9.5–10.1 hPa (sensor 1, at 23 °C assumed) vs 6.0–6.6 hPa (sensor 2).
- **Incident:** momentary short while probing the 5V pin with the ST-Link supplying the board. The ST-Link re-enumerated on USB, and a following upload was verified OK. No overvoltage was possible, because the ST-Link was the only supply and the 5V pin was at about 3.26 V.
- **Conclusion:** the protocol works. w0 is shorter than the datasheet value, cause untested (sensor spread or pull-up rise time). A supply effect on RH is not established: 2 %RH is about twice the ±1 %RH repeatability (DHT p.4), from one sensor and a few readings. Both sensors read far below the reference, but the reference accuracy is unknown. Sensor offsets, reference offset, microclimate and slow equilibration are all untested.
- **Next step:** 1 h side-by-side time series; saturated salt test; logic analyzer capture of DATA; pull-up test with 2.2 kΩ and 10 kΩ; measure the pull-up with the DMM.

## 2026-10-09 exp02_dht11: first logic analyzer session (no firmware change)

- **State:** board powered from the ST-Link only (no USB-C); DHT11 VDD on the 3V3 pin; 3V3 about 3.23 V (user, measured 2026-10-08 evening, not during the captures); sensor 2; Saleae clone (fx2lafw) on port 1 of the Realtek hub.
- **Hypothesis:** an independent instrument confirms the firmware's TIM3 widths and decode, and shows whether w0 below 26–28 µs is a measurement artefact.
- **Change:** `scripts/sigrok_cli.py` (tool lookup), `scripts/analyze_dht_edges.py`, `docs/ref/logic-analyzer.md`, `HARDWARE_NOTES.md` (channel map CH1 = D0 PA6, CH2 = D1 PA9), captures `docs/exp02-dht11/data/chancheck_all.sr` and `capture_001.sr`/`.vcd` (24 MHz, trigger D0 falling).
- **Observation:** analyzer decode 15 00 17 00 2C, checksum OK, matches the firmware's `raw=` in the same capture. The UART decode was verified by the user in PulseView. w0 21.58–25.67 µs (27 of 30 at 24.1 µs; the last bit of bytes 1 and 3 is 25.6 µs, bit 39 is 21.6 µs); w1 71.83–71.87 µs; firmware in the same frame w0 21–26, w1 71–72. Release to response 12.5 µs (datasheet 20–40), response 84/88 µs (80/80), bit-start low 55.1 µs (50).
- **Conclusion:** the firmware and the analyzer agree to within 1 µs; both differ from the datasheet. The w0 spread depends on bit position. Rise time alone does not explain the pattern (calc); the cause is untested.
- **Issues:** the analyzer enumerates only on some hub ports. The 1.2 s channel check caught a read by chance, because the read period is 2 s (corrected in the README). `am230x` can't decode captures that start inside the start pulse.
- **Step 7 (probe loading):** user-reported "not much difference" in the firmware's w0/w1 with the clips on or off; qualitative, ranges not recorded.
- **Next step:** pull-up test with 2.2 kΩ/10 kΩ; 1 h side-by-side time series; saturated salt test.
