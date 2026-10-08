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
| Supply | 3.3 V (Black Pill 3V3) | DHT p.5: 3 to 5.5 V |
| Pull-up | 4.7 kΩ (`PULLUP_OHM`), **not measured yet** | DHT p.5: ~5 kΩ for cable < 20 m |
| Decoupling | 100 nF VDD to GND | DHT p.5 |
| DATA pin | PA6 = TIM3_CH1 on AF2 | DS p.44, Table 9 |
| Timer | TIM3, 16-bit, 84 MHz max timer clock | DS p.26, Table 4 |

## Protocol timing: datasheet vs measured

The measured column comes from the `# timing:` line and the edge dump that the firmware prints once, at the first good read.

| Phase | Datasheet | Source | Firmware setting / limit | Measured |
|-------|-----------|--------|--------------------------|----------|
| Wait after power-up | ≥ 1 s | DHT p.5 | first read at tick ≥ 1000 ms | |
| Read interval | ≥ 1 s | DHT p.8 | 2000 ms (`READ_PERIOD_MS`) | |
| MCU start low | ≥ 18 ms | DHT p.6, Fig. 3 | 20 ms on a 1 ms tick, so ≥ 19 ms | `start_low=` |
| MCU release → sensor response | 20–40 µs | DHT p.6, Fig. 3 | — | `release_to_response=` |
| Response low | 80 µs | DHT p.7, Fig. 3 | accepted 40–120 µs | `resp_low=` |
| Response high | 80 µs | DHT p.7, Fig. 3 | accepted 40–120 µs | `resp_high=` |
| Bit start low | 50 µs | DHT p.7, Fig. 4/5 | — | `bit_low=` min–max |
| High for "0" | 26–28 µs | DHT p.7, Fig. 4 | classified by derived `thr` | `w0=` min–max |
| High for "1" | 70 µs | DHT p.8, Fig. 5 | classified by derived `thr` | `w1=` min–max |
| Final low after bit 40 | 50 µs | DHT p.8 | — | `final_low=` |
| Whole frame | about 4 ms | DHT p.5 | capture window 10 ms | |

### Calculation

- **Frame length**, from the figure values: the shortest is 20 + 80 + 80 + 40 × (50 + 26) + 50 = **3,270 µs** (all bits 0). The longest is 40 + 80 + 80 + 40 × (50 + 70) + 50 = **5,050 µs** (all bits 1). Both fit inside the 10 ms capture window.
- **Counter wrap:** TIM3 runs at 84 MHz / 84 = 1 MHz and is 16-bit, so it wraps every 65,536 µs (DS p.26, RM p.95 for the 2 × PCLK1 timer clock). Each width is a 16-bit difference of two capture values, which is correct for any interval shorter than 65,536 µs. The longest interval inside a frame is about 80 µs, the frame about 5 ms and the start pulse about 20 ms, so nothing can overflow. The counter may wrap during a frame; modular subtraction handles that.
- **Edge count:** 3 response edges + 80 bit edges + 1 final rising edge = 84, or 85 if the release edge is also captured. The buffer holds 100.
- **Pull-up current** while the line is low: 3.3 V / 4.7 kΩ ≈ 0.70 mA.
- **Rise time:** 2.2 · R · C_bus. With an *assumed* C_bus of 100 pF that is about 1.0 µs, small compared with the shortest pulse (26 µs).
- **Resolution:** 1 µs per tick, so every width is ±1 µs.

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

## Reference comparison

To be filled in with readings from a reference instrument, taken at the same place and time and after the DHT11 has settled (response time 6–30 s, DHT p.4).

Reference instrument: _______ (model, stated accuracy: _______)

| # | Time | DHT11 T (°C) | T dec | Ref T (°C) | ΔT (°C) | DHT11 RH (%) | RH dec | Ref RH (%) | ΔRH (%RH) | Within ±2 °C / ±5 %RH? |
|---|------|--------------|-------|------------|---------|--------------|--------|------------|-----------|------------------------|
| 1 | | | | | | | | | | |
| 2 | | | | | | | | | | |
| 3 | | | | | | | | | | |
| 4 | | | | | | | | | | |
| 5 | | | | | | | | | | |

## Findings

_Pending: not run on hardware yet._ To fill in: the measured timing table, the `thr`/`w0`/`w1` spread, the ok/crc_err/timeout counts over a long run, and the comparison with the reference instrument.

## Not read / not verified

- **Not run on hardware.** The decoder was checked by porting `decode()`/`derive_threshold()` line for line to Python and running 3,000 synthetic frames. Those frames had ±6 µs jitter, 16-bit counter wrap and the release edge sometimes captured and sometimes not; there were 0 failures and every corrupted checksum was detected. The C code itself was not run on a host: no host compiler is installed.
- **DHT p.4 detailed specification table:** text extraction scrambled its columns and symbols (degree signs, ±). The accuracy values used above come from the p.3 overview instead.
- **DHT timing figures:** Fig. 3, 4 and 5 are images. I read them from images extracted from the PDF, not from text. I matched the figures to pages 6–8 by their order and captions.
- **Decimal bytes:** DHT p.5 says the data has decimal and integral parts but doesn't say how the decimal byte is encoded: tenths, a sign bit, or always 0. The firmware prints it raw and doesn't combine it with the integral part.
- **Temperatures below 0 °C:** not covered (DHT p.3 range is 0–50 °C), so no sign handling.
- **DS electrical characteristics of PA6** (VIL/VIH at 3.3 V, injection current): not read.
- **RM GPIO:** whether the TIM3_CH1 input sees the pin while it is in output mode was not checked. The decoder works either way, because it searches for the response pattern.
- **RM DMA request mapping for TIM3_CH1:** not read; the firmware uses an interrupt instead.
- **Pull-up value:** `PULLUP_OHM` is the nominal 4.7 kΩ, not measured with a DMM. The experiment rules ask for it to be measured.
- **Other files in `docs/ref/`** (SSD1306, AN2834, AN5537, PM0214): not read. This experiment doesn't need them.
