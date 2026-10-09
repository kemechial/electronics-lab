# electronics-lab

Basic-to-advanced electronics experiments driven by an STM32 Black Pill (WeAct STM32F401CCU6), PlatformIO + `stm32cube` framework.

Layout: one PlatformIO env per experiment (`[env:expNN_name]`), sources in `src/expNN_name/` selected by `build_src_filter`, write-up in `docs/expNN-name/README.md`, size history in `DEVLOG.md`.

## Known STM32 HAL Gotchas

These rules were first written for a Blue Pill (STM32F103) and apply to every STM32 board here. Clock values are board-specific — see the Black Pill section below.

1. Always call `HAL_Init()` before `SystemClock_Config()` — `HAL_Init()` initializes SysTick, which `HAL_GetTick()` depends on.
2. Always enable the GPIO clock before configuring any pin — `__HAL_RCC_GPIOC_CLK_ENABLE()` for PC13, `__HAL_RCC_GPIOA_CLK_ENABLE()` for PA pins, etc.
3. PC13 is active-low — `GPIO_PIN_RESET` = LED on, `GPIO_PIN_SET` = LED off.
4. `SystemClock_Config` must configure HSE + PLL to the board's maximum clock — never leave it on the default HSI. (Blue Pill: 8 MHz crystal → 72 MHz. Black Pill: see below. Do not mix them up.)
5. Clone chips — CPUTAPID check disabled in `platformio.ini` (`upload_flags = -c` / `set CPUTAPID 0`), never remove this.
6. **`stm32xxxx_it.c` (or an equivalent handler) is not optional** — the CMSIS startup file weak-aliases `SysTick_Handler` to `Default_Handler`, which is just an infinite `b .` loop. If nothing in the project defines `SysTick_Handler`, the first SysTick interrupt (fired as soon as `HAL_Init()` enables it) traps the CPU forever — before `main()`'s loop ever runs. Symptom: flash reports SUCCESS, chip resets, but nothing happens (e.g. LED stays latched in its initial state). Fix: define
   ```c
   void SysTick_Handler(void)
   {
       HAL_IncTick();
   }
   ```
   Because each experiment builds only its own folder, **every experiment needs its own `stm32f4xx_it.c`**.

## Black Pill (WeAct STM32F401CC) specifics

- MCU: STM32F401CCU6, Cortex-M4F, 256 KB flash, 64 KB RAM, max SYSCLK **84 MHz**. Board: `blackpill_f401cc`.
- Crystal: **HSE = 25 MHz**. The board JSON does not set it; the value comes from the framework's `stm32f4xx_hal_conf.h` (`HSE_VALUE 25000000U`). Each experiment has `#if HSE_VALUE != 25000000U #error` as a guard.
- Clock tree for 84 MHz: HSE 25 MHz → PLLM 25 (1 MHz) → PLLN 336 (VCO 336 MHz) → PLLP /4 = **84 MHz**, PLLQ 7 = 48 MHz (USB). AHB /1 = 84 MHz, APB1 /2 = 42 MHz (its max; timers on APB1 run at 84 MHz), APB2 /1 = 84 MHz. `FLASH_LATENCY_2`, `PWR_REGULATOR_VOLTAGE_SCALE2`.
- ADC clock = PCLK2 / 4 = 21 MHz (max 36 MHz).
- Programmer: ST-Link V2 over SWD (`upload_protocol = stlink`).
- LED: PC13, active-low.
- **PA0 is the user button (KEY) — do not use it for experiments.**
- Debug UART: USART1 TX on PA9 (AF7), 115200 8N1.

## Blue Pill (STM32F103C8T6, clone) specifics

Used by exp06 onward where stated; taken from `C:\Projects\stm32-experiments\CLAUDE.md`. Do not apply Black Pill clock values or cite F401 documents for the F103 (F103 sources: DS5319, RM0008, see docs/ref/INDEX.md).

- Board `bluepill_f103c8`, framework `stm32cube`, `upload_protocol = stlink`, `upload_flags = -c` / `set CPUTAPID 0` (clone, never remove).
- `HAL_Init()` before `SystemClock_Config()`; enable each GPIO clock before configuring its pins.
- Clock: **HSE 8 MHz** (`HSE_VALUE 8000000U` in the F1 HAL config) → PLL ×9 = **72 MHz**; APB1 /2 = 36 MHz, APB2 /1; `FLASH_LATENCY_2`. Never leave it on HSI.
- PC13 LED is active-low; PC13–PC15 can only sink/source ±3 mA (DS5319 p.64).
- Every experiment needs its own `stm32f1xx_it.c` with `SysTick_Handler` calling `HAL_IncTick()`.
- exp06 transistor: Diotec 2N2222A, TO-92, pin order **1 = E, 2 = B, 3 = C** (Diotec p.1), not the onsemi P2N2222A order (1 = C, 3 = E). Orientation verified with a multimeter (β 223, reversed 13).

## Experiment rules

- **Every experiment records calculated versus measured values.** Component values are measured (DMM) where possible and marked "measured" in `config.h`; the README has a Calculation section (expected values from the measured components) and a Measurement section (results), plus the deviation and its cause.
- **No floating point in output.** No `printf` float formatting — print scaled integers (mV, µs, 1/100 %).
- **Size policy.** After every successful release build (`pio run`, not a debug build) record flash and RAM use in [DEVLOG.md](DEVLOG.md).
  - Budget for exp01: **< 20 KB flash, < 16 KB RAM**.
  - Before proposing any size optimisation, report the top 10 largest symbols (`arm-none-eabi-nm --size-sort -r -S .pio/build/<env>/firmware.elf | head -10`).

## Operating model

### Roles

- **User:** the physical side and safety (wiring, power, probes, motors, flashing), the acceptance decision, and the sanity check of numbers.
- **Claude:** reading the documents, writing code, running instruments, analysis, documentation and commits.

### Trust levels

| Level | Applies to | Rule |
|-------|------------|------|
| 1 | First use of a new tool or method | One independent verification, then record in HARDWARE_NOTES.md or the experiment README that it was done. |
| 2 | Routine repeat of a tool or method already verified at level 1 | Do not stop for confirmation; report and continue. |
| 3 | New wiring, external supply, motors or inductive loads, flashing, anything public (e.g. `git push`, publishing) | Always wait for the user's approval. |

### Stop and ask when

- Two sources disagree.
- A number looks implausible.
- Something needs a system change (PATH, drivers, registry).
- A conclusion relies on an assumption that cannot be verified from `docs/ref`.

### Report format after each task

At most 10 lines: what was done; what was measured, with evidence file paths; what is calc; what is assumption; what the user must do physically. No long explanations unless the user asks.

### Working rules

1. Chat replies are at most 10 lines by default. Use tables only when the user asks or the data needs one.
2. Long answers only when the user writes "detay" or "açıkla".
3. Do exactly the requested scope. A possible extra gets one line; never do it unasked.
4. One verification per claim. No test harnesses or simulations unless requested.
5. Labels: README files and assumption tables keep calc/assumption labels (Document-first protocol). In chat replies, label only what a decision depends on.
6. Before stating a parameter, check its value in code, config or the datasheet. Parameters in prompts from the desktop chat are unverified: check them, and say in one line if they differ (e.g. the firmware reads every 2 s; a prompt said 1 s, which is only the datasheet minimum, DHT p.8).
7. When the user answers a check qualitatively, record it in one line and move on.

## Debug session notes (Blue Pill PC13 blink, from stm32-experiments)

- What was tried first: reviewed `HAL_Init()`/`SystemClock_Config()` ordering, GPIO clock enable ordering, and active-low pin logic — all were already correct, so none of these were the actual bug.
- What failed: build and flash both reported SUCCESS and the chip reset, but PC13 never blinked.
- Root cause found by inspecting `startup_stm32f103xb.s` in the PlatformIO package cache: `SysTick_Handler` was weak-aliased to `Default_Handler`, an infinite loop, because the project had no `stm32f1xx_it.c` defining it.
- What worked: added a `SysTick_Handler` calling `HAL_IncTick()`.

## Live Watch (Cortex-Debug) gotchas

(The full debug log, `LIVE_WATCH.md`, lives in the older stm32-experiments repo.)

1. `.vscode/launch.json` is regenerated by PlatformIO — any extra key added to the `platformio-debug` configs (e.g. `liveWatch`) gets silently stripped, and that debug adapter doesn't support Live Watch anyway. Use a separate `"type": "cortex-debug"` config instead (requires the `marus25.cortex-debug` extension); PlatformIO's regenerator never touches non-`platformio-debug` entries.
2. After installing `marus25.cortex-debug` via CLI, reload the VS Code window before it will work.
3. This machine has two ARM toolchains installed: `toolchain-gccarmnoneeabi@1.70201.0` (GDB 8, used for the build) and unversioned `toolchain-gccarmnoneeabi` (GDB 13). Live Watch requires GDB ≥ 9 — point the debug config's `gdbPath`/`armToolchainPath` at the unversioned one.
4. A Live Watch session needs `build_type = debug` (otherwise no DWARF type info). Size numbers for DEVLOG.md always come from the release build, never from the debug build.
5. Watch variables (exp01: `tau_measured_us`, `tau_discharge_us`) in the **Cortex Live Watch** panel, not the plain **Watch** panel — plain Watch only evaluates while halted.
