# Logic analyzer: tool and sources

Every sigrok-cli option used in this repo is taken from the sources below, not from memory. Each entry gives the source it came from.

## Tool

| Item | Value | Source |
|------|-------|--------|
| sigrok-cli | 0.8.0-git-f44dd91 | `sigrok-cli --version` |
| libsigrok | 0.6.0-git-883c2ac | `sigrok-cli --version` |
| libsigrokdecode | 0.6.0-git-71f4514 (Python 3.4.4) | `sigrok-cli --version` |
| Location | `C:\Program Files\sigrok\sigrok-cli\sigrok-cli.exe`, not on PATH | user; resolved by `scripts/sigrok_cli.py` |
| Lookup order | `SIGROK_CLI` env var → `sigrok-cli` on PATH → the path above | user requirement, implemented in `scripts/sigrok_cli.py` |

## Device

| Item | Value | Source |
|------|-------|--------|
| USB ID | 0925:3881 "Saleae Logic USB Logic Analyzer" | Windows device list (`Get-PnpDevice`, read-only) |
| USB driver | WinUSB, provider Saleae LLC, `oem39.inf` | `Get-PnpDeviceProperty` (read-only); already installed, not changed |
| sigrok driver | `fx2lafw`, "Saleae Logic", conn=1.43 (bus.address, changes when replugged) | `sigrok-cli --scan`, `sigrok-cli -d fx2lafw --show` |
| Firmware | `fx2lafw-saleae-logic.fw` in `sigrok-cli\share\sigrok-firmware` | directory listing |
| Channels | 8: D0–D7, channel group "Logic" | `--show` |
| Sample rates | 20 k, 25 k, 50 k, 100 k, 200 k, 250 k, 500 k, 1 M, 2 M, 3 M, 4 M, 6 M, 8 M, 12 M, 16 M, 24 M, 48 MHz | `--show` |
| Triggers | `0` low, `1` high, `r` rising, `f` falling, `e` either edge | `--show` (letters); meanings per `--help` `-t/--triggers` (to confirm against the man page) |
| Options | `continuous`, `limit_frames`, `limit_samples`, `conn`, `captureratio` | `--show` |
| Input threshold | **unknown**, assumed about 1.4–1.65 V | not given by `--show`, no schematic or datasheet for this clone; assumption (see the exp02 README) |

## Command-line options used

From `sigrok-cli --help`. The man page is not installed with the Windows build: no `.1` or HTML file exists under `C:\Program Files\sigrok`.

| Option | Meaning (from `--help`) |
|--------|-------------------------|
| `--scan` | Scan for devices |
| `--show` | Show device/format/decoder details |
| `-d, --driver` | The driver to use. Always `-d fx2lafw`, so other USB devices are not probed. A plain `--scan` also probes the FTDI USB-serial adapter as an `ftdi-la` device. |
| `-c, --config` | Device configuration options |
| `-C, --channels` | Channels to use |
| `-t, --triggers` | Trigger configuration |
| `-w, --wait-trigger` | Wait for trigger |
| `--time` | How long to sample (ms) |
| `--samples` | Number of samples to acquire |
| `-o, --output-file` / `-O, --output-format` | Save output to file / output format |
| `-i, --input-file` / `-I, --input-format` | Load input from file / input format |
| `-P, --protocol-decoders` / `-A, --protocol-decoder-annotations` | Protocol decoders to run / annotations to show |
| `-L, --list-supported` | List supported devices/modules/decoders |

## Decoders used

| Decoder | Options (from `-P <id> --show`) | Use |
|---------|---------------------------------|-----|
| `uart` | `tx=<ch>`, `baudrate` (default 115200), `data_bits` (8), `parity` (none), `stop_bits` (1.0); binary class `tx` (`-B uart=tx`) | decode USART1 TX |
| `am230x` | `sda=<ch>`, `device` ('am230x/rht' or 'dht11'); annotation classes `bit`, `byte`, `checksum` (plus humidity/temperature, not used) | cross-check bits/bytes/checksum only. Needs the high idle before the start edge, so it can't decode a capture triggered on the start falling edge. Its docs recommend a sample rate of at least 200 kHz. |

## Not verified

- **`-c` syntax for more than one option** (for example `samplerate` together with `captureratio` for a pre-trigger): `--help` doesn't document it and it hasn't been tested. Captures so far use a single `-c samplerate=...`.
- **48 MHz:** listed by `--show`, but not used or validated. 24 MHz is used for DHT captures.

## History

- 2026-10-09: first scan found only the `demo` device. Windows showed "Unknown USB device (port reset failed)", VID_0000&PID_0001, on a hub port; on another hub port the analyzer was not seen at all. On a dedicated port it enumerated as 0925:3881 with WinUSB. No driver was changed and Zadig was not run.
