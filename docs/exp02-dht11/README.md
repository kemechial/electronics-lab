# Experiment 2: DHT11 temperature and humidity

Firmware: [src/exp02_dht11/](../../src/exp02_dht11/) · PlatformIO env: `exp02_dht11`

Sources, cited as `DHT p.N`, `DS p.N` and `RM p.N`. The PDFs are in `docs/ref/` and are not committed.

| Tag | Document |
|-----|----------|
| DHT | `DHT11.PDF`, OSEPP translation of the manufacturer datasheet (9 pages) |
| DS | `DS_stm32f401cc.pdf`, STM32F401xB/xC datasheet DS9716 Rev 11 |
| RM | `rm0368-…pdf`, reference manual RM0368 Rev 6 |

## Goal

Read a bare DHT11 over its single-wire bus without blocking the CPU. The firmware sends the start pulse from an open-drain GPIO, captures every edge of the reply with TIM3 input capture, and decodes the 40 bits with a threshold derived from the measured pulse widths. It records the measured protocol timing against the datasheet values and compares the readings with a reference instrument.

## Circuit

```
3V3 ──┬──────────────┬─────────── DHT11 pin 1 VDD
      │            [100 nF]
      │              │
    [R pull-up       GND ──────── DHT11 pin 4 GND
     4.7 kΩ]
      │
PA6 ──┴────────────────────────── DHT11 pin 2 DATA   (pin 3: not connected, DHT p.5)
(TIM3_CH1, AF2, open-drain, no internal pull)

PA9 (USART1 TX) ──> USB-UART RX, 115200 8N1      PC13 LED toggles on every good read
```

The OLED is not connected in this experiment.

| Item | Value | Source |
|------|-------|--------|
| Supply | 3.3 V (Black Pill 3V3); see "Supply tests" for the 5 V variants | DHT p.5: 3 to 5.5 V |
| Pull-up | 4.7 kΩ (`PULLUP_OHM`), **not measured yet** | DHT p.5: ~5 kΩ for cable < 20 m |
| Decoupling | 100 nF VDD to GND | DHT p.5 |
| DATA pin | PA6 = TIM3_CH1 on AF2 | DS p.44, Table 9 |
| Timer | TIM3, 16-bit, 84 MHz max timer clock | DS p.26, Table 4 |

## Protocol timing: datasheet vs measured

Measured over several hundred reads. Values not reported for this session are marked "not recorded"; they come from the `# timing:` line and the first-read edge dump.

| Phase | Datasheet | Source | Firmware setting / limit | Measured |
|-------|-----------|--------|--------------------------|----------|
| Wait after power-up | ≥ 1 s | DHT p.5 | first read at tick ≥ 1000 ms | not recorded |
| Read interval | ≥ 1 s | DHT p.8 | 2000 ms (`READ_PERIOD_MS`) | — |
| MCU start low | ≥ 18 ms | DHT p.6, Fig. 3 | 20 ms on a 1 ms tick, so ≥ 19 ms | not recorded |
| MCU release → sensor response | 20–40 µs | DHT p.6, Fig. 3 | — | not recorded |
| Response low | 80 µs | DHT p.7, Fig. 3 | accepted 40–120 µs | not recorded |
| Response high | 80 µs | DHT p.7, Fig. 3 | accepted 40–120 µs | not recorded |
| Bit start low | 50 µs | DHT p.7, Fig. 4/5 | — | not recorded |
| High for "0" | 26–28 µs | DHT p.7, Fig. 4 | classified by derived `thr` (47 µs) | **21–26 µs**, 0 to 7 µs below the datasheet value |
| High for "1" | 70 µs | DHT p.8, Fig. 5 | classified by derived `thr` (47 µs) | **68–71 µs** |
| Final low after bit 40 | 50 µs | DHT p.8 | — | not recorded |
| Whole frame | about 4 ms | DHT p.5 | capture window 10 ms | complete every time (timeout = 0) |

### Calculation

- **Frame length**, from the figure values: the shortest is 20 + 80 + 80 + 40 × (50 + 26) + 50 = **3,270 µs** (all bits 0). The longest is 40 + 80 + 80 + 40 × (50 + 70) + 50 = **5,050 µs** (all bits 1). Both fit inside the 10 ms capture window.
- **Counter wrap:** TIM3 runs at 84 MHz / 84 = 1 MHz and is 16-bit, so it wraps every 65,536 µs (DS p.26, RM p.95 for the 2 × PCLK1 timer clock). Each width is a 16-bit difference of two capture values, which is correct for any interval shorter than 65,536 µs. The longest interval inside a frame is about 80 µs, the frame about 5 ms and the start pulse about 20 ms, so nothing can overflow. The counter may wrap during a frame; modular subtraction handles that.
- **Edge count:** 3 response edges + 80 bit edges + 1 final rising edge = 84, or 85 if the release edge is also captured. The buffer holds 100.
- **Pull-up current** while the line is low: 3.3 V / 4.7 kΩ ≈ 0.70 mA.
- **Rise time:** 2.2 · R · C_bus. With an *assumed* C_bus of 100 pF that is about 1.0 µs, small compared with the shortest pulse (26 µs).
- **Resolution:** 1 µs per tick, so every width is ±1 µs.

Protocol result:
- **Reads:** several hundred, with `crc_err=0` and `timeout=0`.
- **Checksum:** verified by hand on sample frames: the low byte of the sum of the first four bytes equals the fifth (DHT p.5).
- **Derived threshold:** `thr` = 47 µs, between the two clusters, with a 21 µs margin to `w1` min (68 µs) and a 21 µs margin to `w0` max (26 µs).

### Bit threshold: derived, not hard-coded

For each frame the firmware runs a two-means split on the 40 high widths. It starts at the midpoint of the minimum and maximum, then repeatedly sets the threshold to the midpoint of the two cluster means until it stops changing. A width ≥ `thr` is a 1. If all 40 widths are within 15 µs of each other (`MIN_CLUSTER_GAP_US`, one cluster only), it uses the measured mean bit-start low instead. That low is about 50 µs, which sits between 26–28 µs and 70 µs (DHT Fig. 4/5). Every line reports `thr` and the observed ranges `w0` and `w1`.

## Output

One line per read:

```
T=24 C RH=45 % dec=3,0 raw=2D,00,18,03,48 ok=5 crc_err=0 timeout=1 thr=49 w0=24-28us w1=68-72us
```

- `T`/`RH` are the integral bytes. `dec=<T dec>,<RH dec>` are the two decimal bytes, printed raw (see "not verified").
- `raw` is the five bytes in the order RH int, RH dec, T int, T dec, checksum (DHT p.5).
- On a CRC error, `T`/`RH`/`dec` print `-`, but `raw`, `thr`, `w0` and `w1` are still shown.
- `timeout` counts every read that doesn't produce a valid 40-bit frame within the 10 ms window: no response, too few bits, levels not alternating, or a capture overflow/overcapture. The reason of the last failure is in `dht_last_fail` for Live Watch, using the `dht11_fail_t` values.
- For the first good read only, the firmware also prints `# timing: …`, `# edge dump: …`, then `index,width_us` and one CSV row per segment. Segment *i* runs from captured edge *i* to edge *i+1*, and the levels alternate starting with `segment0_level`.

## Assumptions

| # | Assumption | Source | Status |
|---|------------|--------|--------|
| A1 | Accuracy ±5 %RH and ±2 °C, range 20–90 %RH and 0–50 °C | DHT p.3 overview | datasheet values |
| A2 | Accuracy also holds at 3.3 V; the electrical table is specified at VDD = 5 V | DHT p.8 | **assumption** |
| A3 | 4.7 kΩ is close enough to the recommended ~5 kΩ | DHT p.5 | **assumption**, value not measured |
| A4 | TIM3 clock = 2 × PCLK1 = 84 MHz because the APB1 prescaler is /2 | RM p.95 | datasheet rule; prescaler computed from `HAL_RCC_GetPCLK1Freq()` |
| A5 | Both-edge capture through CC1NP/CC1P = 11 | RM p.367 | datasheet rule |
| A6 | Read CCR1 before checking CC1OF, otherwise an overcapture can be missed | RM p.333 | datasheet rule |
| A7 | Open-drain output with ODR = 0 pulls low; AF mode releases the line to the pull-up | RM p.154 | datasheet rule |
| A8 | The IC1F = 0011 filter (8 samples at 84 MHz ≈ 95 ns) delays rising and falling edges equally, so widths are not changed | RM p.364 | **calc/assumption** |
| A9 | The pin level read in the ISR, a fraction of a µs after the edge, is the level after that edge. The shortest pulse is about 26 µs, and TIM3 has NVIC priority 0, above SysTick | calc | **assumption**, ISR latency not measured. Widths come from hardware capture and don't depend on it |
| A10 | Bus capacitance ~100 pF for the rise-time estimate | — | **assumption** |
| A11 | HAL tick 0 is close enough to sensor power-up that waiting until tick ≥ 1000 ms gives ≥ 1 s | DHT p.5 | **assumption**: the MCU and sensor power up together |

## Supply tests (sensor 1)

| Supply to DHT11 VDD | Measured VDD | RH reading | Notes |
|---------------------|--------------|------------|-------|
| Black Pill 3V3 pin | 3.3 V (nominal) | 34 % | default circuit |
| Black Pill 5V pin, board powered by USB-C only | 5 V (nominal) | 36 % | |
| Black Pill 5V pin, board powered by the ST-Link (ST-Link back-feeds the 5V pin) | about 3.26 V (measured) | 34 % | the 5V pin sits near 3.3 V; reverse path through the regulator not verified, see A13 |

The 2 %RH difference is about twice the ±1 %RH repeatability (DHT p.4). A supply effect is **not established**: this is one sensor and a few readings, at 1 %RH resolution, taken at different times, so the difference is not separated from drift or equilibration.

## Reference comparison

Reference instrument: model not recorded, **accuracy unknown** (A12). It read 23 °C / 49 %RH.

| Device | T (°C) | RH (%) | ΔRH vs reference (%RH) | Within ±5 %RH (DHT p.3)? |
|--------|--------|--------|------------------------|--------------------------|
| Reference | 23 | 49 | — | — |
| Sensor 1 | not recorded here | 34–36 | −13 to −15 | no |
| Sensor 2 | 25, then 24 | 20–21 | −28 to −29 | no, **suspect** |

Sensor 2 is marked **suspect**: it is about 14 %RH below sensor 1 under the same conditions and close to the bottom of the 20–90 %RH range (DHT p.3).

### Vapour pressure and dew point (calc)

**calc:** Magnus formula with Sonntag (1990) constants:

- e_s(T) = 6.112 · exp(17.62 · T / (243.12 + T)) hPa
- e = RH · e_s(T)
- T_d = 243.12 · ln(e / 6.112) / (17.62 − ln(e / 6.112))

If all devices measure the same air, the vapour pressure e should be the same even when their temperatures differ. Sensor 1's temperature is not in this session's record, so its row uses the reference's 23 °C (A14).

| Device | T used (°C) | RH (%) | e (hPa) | Dew point (°C) |
|--------|-------------|--------|---------|----------------|
| Reference | 23 | 49 | 13.7 | 11.7 |
| Sensor 1 | 23 (assumed) | 34–36 | 9.5–10.1 | 6.3–7.1 |
| Sensor 2 | 25 | 20–21 | 6.3–6.6 | 0.5–1.1 |
| Sensor 2 | 24 | 20–21 | 6.0–6.3 | −0.4–0.3 |

**calc:** to read the reference's 13.7 hPa as 34–36 %RH, the air at sensor 1 would have to be 28.2–29.2 °C. To read it as 20–21 %RH, the air at sensor 2 would have to be 37.8–38.7 °C, while sensor 2 itself reported 24–25 °C. The other way round: if the air at sensor 2 really is 24–25 °C, the reference's vapour pressure corresponds to 43.5–46.1 %RH there, not 20–21 %RH.

### Hypotheses (all untested)

| # | Hypothesis | Status | Notes |
|---|------------|--------|-------|
| H1 | Sensor offsets: each DHT11 reads low by a fixed amount, sensor 2 much more than sensor 1 | untested | outside ±5 %RH (DHT p.3); a 3 %RH shift after exposure outside the working range is described in DHT p.8, recovery procedure on p.9 |
| H2 | Reference offset | untested | reference accuracy unknown (A12) |
| H3 | Microclimate: different air at the sensors and at the reference | untested | the calc above says a temperature difference alone would need 28–39 °C at the sensors; a local humidity difference is not ruled out |
| H4 | Slow equilibration: the sensors had not settled | untested | readings were not taken as a time series |

### Planned tests

1. **1 h time series:** sensor 1, sensor 2 and the reference side by side, logged at a fixed interval (tests H3, H4).
2. **Saturated salt test:** each sensor in a closed container over a saturated salt solution with a known equilibrium humidity, so neither sensor depends on the reference (tests H1, H2). The equilibrium value comes from the literature, not from `docs/ref`.
3. **Logic analyzer capture** of the DATA line, to check the protocol timing (w0 below the datasheet value) independently of TIM3.
4. **Pull-up test** with 2.2 kΩ and 10 kΩ, to check whether w0 changes with rise time (see Findings).

## Findings

1. **Protocol works:** several hundred reads with `crc_err=0` and `timeout=0`. Checksums were verified by hand on sample frames.
2. **"1" bits match the datasheet:** w1 = 68–71 µs against 70 µs (DHT p.7–8).
3. **"0" bits are short:** w0 = 21–26 µs against 26–28 µs (DHT p.7). Possible causes, both untested: spread between sensors, or pull-up rise time. Note that a rise-time delay at the start of each high pulse would shorten "0" and "1" pulses by the same amount, yet w1 is within ±2 µs of 70 µs. Rise time alone therefore doesn't obviously explain the gap; the 2.2 kΩ/10 kΩ test and a logic analyzer capture will check it.
4. **Derived threshold:** 47 µs, well separated from both clusters (21 µs margin on each side), so the fixed-threshold problem the derivation was meant to avoid did not come up with these sensors.
5. **Supply:** 34 / 36 / 34 %RH at 3.3 V / 5 V / about 3.26 V. A supply effect is not established.
6. **Accuracy against the reference:** sensor 1 is 13–15 %RH low and sensor 2 is 28–29 %RH low. Both are outside ±5 %RH, but the reference accuracy is unknown, so these are differences from the reference, not established sensor errors. Sensor 2 is suspect.

## Assumptions (measurement)

| # | Assumption | Source | Status |
|---|------------|--------|--------|
| A12 | Reference instrument accuracy | — | **unknown**: model and stated accuracy not recorded |
| A13 | The AP7343 regulator on the Black Pill passes the ST-Link's 3.3 V back to the 5V pin (about 3.26 V measured) and tolerates it | — | **not verified**: the AP7343 datasheet is not in `docs/ref` and its reverse-current behaviour has not been read |
| A14 | Sensor 1 temperature taken as the reference's 23 °C for the vapour-pressure calc | — | **assumption** |
| A2 (restated) | The DHT11 electrical table is specified at VDD = 5 V (DHT p.8). Accuracy at 3.3 V is not specified, so ±5 %RH / ±2 °C at 3.3 V is an assumption | DHT p.8 | **assumption** |

## Not read / not verified

- **Hardware run:** the firmware has now run for several hundred reads with no CRC errors or timeouts. Before that, the decoder logic was also checked with a line-for-line Python port on 3,000 synthetic frames (0 failures). The C code itself was not run on a host: no host compiler is installed.
- **DHT p.4 detailed specification table:** text extraction scrambled its columns and symbols (degree signs, ±). Accuracy values come from the p.3 overview. The ±1 %RH repeatability is the value readable on p.4.
- **DHT timing figures:** Fig. 3, 4 and 5 are images. I read them from images extracted from the PDF, not from text, and matched them to pages 6–8 by their order and captions.
- **Decimal bytes:** DHT p.5 doesn't say how the decimal byte is encoded. The firmware prints it raw.
- **Temperatures below 0 °C:** not covered (DHT p.3 range 0–50 °C).
- **DS electrical characteristics of PA6** (VIL/VIH at 3.3 V): not read.
- **RM GPIO:** whether the TIM3_CH1 input sees the pin while it is in output mode was not checked. The decoder works either way.
- **RM DMA request mapping for TIM3_CH1:** not read; the firmware uses an interrupt.
- **Pull-up value:** `PULLUP_OHM` is the nominal 4.7 kΩ, not measured with a DMM.
- **AP7343 regulator datasheet:** not read; see A13.
- **Protocol timing not recorded this session:** start low, release-to-response, response low/high, bit-start low and final low.
- **Other files in `docs/ref/`** (SSD1306, AN2834, AN5537, PM0214): not read.
