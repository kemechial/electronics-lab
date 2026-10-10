# Experiment 4: SSD1306 128x64 I2C OLED (Blue Pill)

Firmware: [src/exp04_oled_ssd1306/](../../src/exp04_oled_ssd1306/) · PlatformIO env: `exp04_oled_ssd1306` · Board: STM32F103C8 Blue Pill **clone** (CPUID 0x412FC230 r2p0, DEV_ID 0x410), 72 MHz.

Sources: DS5319 Rev 20, RM0008 Rev 21, ES096 Rev 15 (F103 medium-density errata), SSD1306 Rev 1.1 (see [docs/ref/INDEX.md](../ref/INDEX.md)).

## Wiring

| OLED | Blue Pill | Notes |
|------|-----------|-------|
| VCC | 3V3 | single power source (USB-C or ST-Link 3V3) |
| GND | GND | |
| SCL | PB10 | I2C2_SCL, FT, LQFP48 pin 21 (DS5319 Table 5 p.30) |
| SDA | PB11 | I2C2_SDA, FT, LQFP48 pin 22 (DS5319 Table 5 p.30) |
| — | PB10, PB11 → 4.7 kΩ → 3V3 | added external pull-ups (SSD1306 p.19: both lines need pull-ups) |

- Debug UART: USART1 TX PA9 → FTDI RX (COM5), 115200 8N1. PB6 is not used (reserved for the exp07a PWM).
- Module's own pull-up: about 40–50 kΩ from SDA and SCL to VCC (user, meter). **calc:** with C_bus ≈ 100 pF (assumption), rise time ≈ 0.85 · R · C ≈ 3.8 µs, above the 1000 ns Sm maximum (RM0008 p.782). With the added 4.7 kΩ (≈ 4.2 kΩ total): ≈ 0.36 µs.
- Logic analyzer: CH1 = D0 = PB10 SCL, CH2 = D1 = PB11 SDA, GND on GND.

## Stage 1: I2C scan (2026-10-10)

Expected address: 0111 10 SA0 → **0x3C or 0x3D** (SSD1306 p.19).

### Hardware I2C2: fails on this clone

- **Setup:** PCLK1 = 36 MHz. 100 kHz: CCR 180, TRISE 37 (RM0008 pp.781–782). Values read back from the registers.
- **Without the external pull-ups:** BUSY after the first probe, then SCL held low.
- **With the 4.7 kΩ pull-ups:** every scan stops with `BUS BUSY at 0x09` (HAL timeout), so 0x3C is never reached.
- **Captures `data/scan_001.sr` (4 MHz) and `data/scan_002.sr` (24 MHz), identical:** START, address 0x08 + W at 100 kHz, NACK on the 9th clock (correct, nothing at 0x08). Then **SDA falls while SCL is high (a START), and no STOP follows**, so the peripheral keeps BUSY set.
- **HAL check:** HAL 1.8.7 `HAL_I2C_IsDeviceReady` sets `CR1.STOP` after a NACK, so a STOP is expected on the bus.
- **ES096 Rev 15 §2.8.7 (p.20, analog filter locks BUSY):** the 14-step unlock is implemented and runs OK, but the behaviour is unchanged.
- **ES096 §2.8.5 (p.19, ≤ 88 kHz):** tried at CCR 206 = 87.4 kHz. HAL adds 1 to CCR, giving 206 rather than the calculated 205. Unchanged.

### Software (bit-banged) I2C on the same pins: works

- `I2C_BITBANG 1` in config.h: open-drain GPIO, 5 µs half period (≈ 100 kHz), DWT-timed.
- **Result:** `ACK 0x3C`, `found=1` on every scan. The OLED, wiring and pull-ups are OK.

### Finding

On this r2p0 clone, hardware I2C2 issues a START instead of a STOP after a NACK, and BUSY locks. Software I2C on the same pins and the same OLED works. **Clone difference is the most likely cause.** Not tested: the same hardware-I2C code on a genuine ST C8, and I2C1 on this clone. Stages 2–3 use software I2C (`I2C_BITBANG 1`); the hardware path stays in the code (`I2C_BITBANG 0`).

## Stage 2: init + test patterns

_Pending._

## Stage 3: text and transfer time

_Pending._
