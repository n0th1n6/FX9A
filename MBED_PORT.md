# Mbed CE port

The complete FX9A application now builds with Mbed CE and GCC. The original
application logic and STM32 Standard Peripheral Library remain under
`RadioStation/`; project-local glue in `port/` integrates them with Mbed's
startup, linker script, and runtime.

## Configure and build

```sh
git submodule update --init --depth 1
export PATH="/usr/local/opt/arm-gcc-bin@10/bin:$PATH"
cmake --fresh -S . -B build -G Ninja \
  -DMBED_TARGET=FX9A \
  -DMBED_TOOLCHAIN=GCC_ARM \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The example path selects the Homebrew ARM GCC 10 toolchain installed on this
Mac. A newer compatible ARM GNU toolchain can be used instead.

## Safety status

The build produces `build/fx9a.elf`, `build/fx9a.bin`, and `build/fx9a.hex`.
The earlier Mbed baseline passed build, flash readback, and normal power-on checks on an FX9A. The new feature backport has software validation only and has not been flashed. Keep
the RF power stages safely disabled until the power-hold, RX/TX, DDS, filter,
keyer, EEPROM, OLED, ADC, and CAT interfaces have been checked on hardware.

The checked-in original IAR release also starts at `0x08000000`, so the Mbed
image currently matches its application address. That does not prove which
flash region the radio's custom DFU loader protects or writes. Back up the
complete existing firmware and inspect the DFU memory map before any download.
The read-only sequence and prohibited DFU operations are documented in
`DFU_BACKUP.md`.

Mbed performs core and clock initialization before entering the compatibility
`main()` in `port/mbed_main.c`; the legacy application's second `SystemInit()`
call is intentionally omitted.

## Compatibility details

The application still uses the bundled 2009 STM32 Standard Peripheral Library.
The compatibility header renames its generic `gpio_*` and bit-banged `i2c_*`
entry points so they do not collide with Mbed HAL symbols.

The original CW keyer used TIM4. Mbed CE uses TIM4 as the STM32F103
microsecond ticker, so the compatibility layer redirects only the legacy TIM4
references to TIM5. TIM2 remains the application scheduler and TIM3 remains
the PWM timer.

The legacy CAT USART owns DMA1 Channel 4. The build renames only Mbed's generic
DMA1 Channel 4 handler, leaving the hardware vector connected to the legacy
USART completion handler.

The custom target defines `DMA2_Channel5_IRQn` as the STM32F103's shared
`DMA2_Channel4_5_IRQn`. This is a project-local compatibility alias for Mbed
CE's generic STM32 DMA helper; no Mbed CE submodule sources are patched.

The original IAR project uses signed plain `char` and one-byte enums. GCC is
built with the matching ABI options because enum width is part of the existing
24LC16 EEPROM format. Compile-time checks in `port/layout_checks.c` verify the
EEPROM record sizes and important runtime offsets against the original IAR ELF.

The delay implementation in `port/bsp_delay.c` preserves the four-instruction
inner loop in the original IAR release. GCC's translation of the legacy 16-bit
counter inserted an extra instruction into every iteration and slowed the
bit-banged I2C, DDS, and OLED timing.

CH and volume encoders sample both phases every 250 microseconds using TIM2.
Complete quadrature cycles produce one action, while contact bounce and
impossible two-contact jumps retain the last valid state. This prevents a
bouncing common contact from erasing an otherwise valid click. No missing
transition or direction is synthesized. The old blocking interrupt debounce delay
is removed. Button, PTT and paddle scanning retains its existing 5 ms cadence.
Run `python3 tools/test_encoders.py` to exercise the production decoder and
control actions with simulated GPIO transitions. One complete electrical cycle
still produces one action, preserving the previous knob directions and rate.
Actual encoder feel and fast-turn response require testing on the radio.

Every link runs `tools/verify_image.cmake`. The build fails if the vector base,
timer ownership, CAT USART/DMA handlers, encoder timer callback, or
calibrated delay instruction sequence no longer matches the compatibility
contract above.

Mbed CE's `MCU_STM32F103xC` target does not attach a system-clock source file.
The FX9A target therefore reuses Mbed CE's high-density STM32F103 clock source,
which supports the radio's 16 MHz crystal and configures a 72 MHz system clock.

## Continuous tuning

With CONTINUOUS TUNING set to ON, the main encoder and CAT frequency
commands tune through the gaps between amateur bands, within the existing overall range of 1.79900 to 29.70099 MHz.
Each hardware filter stays selected until the next band's existing lower
boundary is reached, in either tuning direction. Individual band upper edges
no longer stop tuning. Crossing a boundary preserves the tuned frequency and
operating mode; it does not recall the next band's saved VFO frequency.
Explicit band selection still recalls the saved per-band VFOs. Memory recall
selects its filter from the stored frequency. The EEPROM record layout is
unchanged.

Run `python3 tools/test_frequency.py` for host regression checks covering the
production encoder, CAT, boundary selection, range limits, offsets, menus,
and VFO/memory recall. Physical tuning and RF/filter behavior still need to be
checked on the radio.

## Receive-only SSB alignment

Long-press BAND to open the settings menu. Use CH to move past the callsign
characters to the RX ALIGNMENT page. Select RX USB or RX LSB with CH and use
the main tuning knob to adjust it. The step is 10 Hz, or 100 Hz with TS enabled;
each offset is limited to -1000 through +1000 Hz. Short-press BAND to exit.
Settings are saved with the existing settings on normal power-off.

Both settings default to zero, preserving the previous receiver behavior.
They shift the receive LO and BFO together, keeping the displayed RF frequency
aligned. Transmit and CW frequencies are unchanged. The original 44-byte menu
EEPROM record uses eight formerly reserved bytes for the two offsets and a
signature; older records are initialized to zero offsets without moving other
settings. Run `python3 tools/test_rx_alignment.py` for migration, persistence,
menu navigation, adjustment limits, labels, and RX/TX frequency checks.
Actual audio improvement requires alignment and listening on the radio.


## Enhancements backported to master

The Mbed application now includes the implemented CMSIS/LL branch features,
while retaining Mbed startup, legacy peripheral initialization, and the TIM5
keyer remap. Host checks do not establish hardware timing or RF performance.

- **Continuous tuning:** long-press BAND, navigate with CH to CONTINUOUS
  TUNING, and use the main tuning knob to select ON/OFF. ON retains tuning
  through band gaps; OFF limits both the knob and CAT to the selected band.
  The setting uses the existing reserved byte of the 44-byte menu record;
  legacy records default ON.
- **Antenna TUNE:** long-press V/M in normal VFO mode. The radio requests LOW
  power and a steady CW carrier without changing the selected mode. A key,
  PTT, completion, protection trip or 10-second timeout stops tuning and
  restores the original power/mode. The final RF-on reflected-detector bar
  remains for five seconds as LAST SWR; an absent sample shows SWR --.
  Frequency changes, settings entry, or TX clear the result. CAT CA1;/CA0;/CA;
  starts, cancels, or queries TUNE. Normal CW keyer tasks are suspended during
  the independent TUNE state.
- **RF sequencing/protection:** synthesizers are prepared before TX enable;
  RX return drops the gate first. TIM2 samples reflected power every 10 ms,
  independently of display refresh. TX startup respects a latched protection
  trip. Protection always uses raw detector readings, regardless of display
  calibration. The provisional 2 ms DDS settling delay needs measurement.
- **Calibration:** service pages after CONTINUOUS TUNING edit five S-meter,
  three RF-power and three reflected-detector points. Use the main knob;
  TS selects a larger step. Exit BAND settings to save. Calibration uses a
  separate CRC/version-checked 30-byte record at EEPROM address 1104; the
  legacy menu and save-marker locations remain unchanged. Invalid/absent
  calibration retains the original display conversions.
- **CAT/EEPROM:** bounds checking discards overlong frames through the next
  semicolon; a bounded mailbox runs commands in main instead of the UART
  ISR. Malformed commands are rejected, IF offsets are initialized, AG gain
  scaling is corrected, and the M4 response has sufficient buffer capacity.
  EEPROM page errors stop subsequent writes and NACK paths send STOP. Saved
  settings are validated in RAM before use. Legacy settings still have no
  checksum, and exhaustive CAT interoperability remains outstanding.
- **DDS:** frequency tuning words use rounded integer arithmetic; the dormant
  AD9834 phase conversion uses the correct 12-bit degrees mapping.

Build options (defaults preserve the later board map and unaccelerated VFO):

```sh
cmake -S . -B build -DFX9A_BOARD_REVISION=2016_03 -DFX9A_VFO_ACCEL=OFF
# Alternative original wiring and experimental optical acceleration:
cmake -S . -B build -DFX9A_BOARD_REVISION=ORIGINAL -DFX9A_VFO_ACCEL=ON
```

The existing FX9A_ENCODER_TRACE option remains available. Both filter maps
have host tests; physically confirm the fitted revision before using either.
Acceleration is disabled by default and retains TS as the coarse-step control.

Run all feature regressions with `python3 tools/test_features.py`. They cover
production tuning/encoder/menu paths, TX/TUNE interrupt injection and display
restoration, EEPROM transactions, deferred CAT framing, DDS math, filter maps,
calibration record validation and optional acceleration.

TUNE thresholds remain provisional raw ADC values: success at reflected ADC
<=75 held for 500 ms after a 400 ms settling period, severe cutoff at >=800,
and a 10-second maximum. These are not calibrated SWR ratios. Confirm GPIO,
DDS and protection timing with RF disabled before controlled dummy-load tests.

Volume changes still update RAM and the digital volume control immediately;
EEPROM saving occurs on normal power-button shutdown. Cutting the bench PSU
skips this save. Per-adjustment EEPROM writes are not part of this backport.


## Application module ownership

`bsp_board.c` handles hardware bring-up and the main-loop schedule.
`bsp_board_io.c` owns GPIO initialization, power hold, voice control, high/low
power selection and the legacy power-button wait helper. The Mbed application compiles these modules independently:

| Module | Owns |
| --- | --- |
| `radio_state.c` | Legacy shared radio data, defaults, settings load/validation and shutdown save |
| `radio_vfo.c` | VFO/band tuning policy, per-band snapshots, memory recall and VFO/memory EEPROM writes |
| `radio_rf.c` | Private RX/TX and TUNE state, DDS sequencing, SWR ISR sampling and trip handling |
| `radio_ui.c` | Display refresh, private redraw caches and held final TUNE result |
| `radio_controls.c` | Front-panel event handling and menu editing |
| `radio_cat.c` | Main-loop CAT dispatch followed by queued DMA transmission |

The original public data types and legacy API declarations in `bsp_board.h`
remain compatible with the peripheral and CAT drivers. Module headers declare
cross-module operations. GPIO helper declarations live in `bsp_board_io.h`; include it after
`bsp_board.h` so the Mbed compatibility GPIO remap is active.
RX/TX latches and ISR sample state are private to `radio_rf.c`; the legacy
`hswr_timeout` latch remains shared with the keyer. At TUNE exit, the RF module
passes a frozen sample and validity flag to UI-owned result storage. The ISR
performs no OLED or EEPROM calls.

Shutdown still saves VFO/memory, filter, volume, attenuation, WPM and menu in
the original order. Encoder edits stay in RAM until normal power-button
shutdown. CAT commands still run after the RF/control/UI/keyer tasks and
before the UART DMA queue is serviced. This source separation changes neither
the 44-byte menu layout nor the Mbed TIM5 keyer remap.

`python3 tools/test_features.py` includes a host test that links the production
state, VFO and CAT translation units separately with an in-memory EEPROM and
mock UART transport. It checks reset/shutdown write order, per-band and memory
reload, volume persistence, RAM-only edits and CAT/DMA service order. The other
regressions continue to exercise the moved production functions, including
injected SWR interrupts and UI result expiry. Physical radio behavior after
the module split still needs validation. This extraction applies to master;
the independent LL branch has not been reorganized by this change.

### BSP validation workflow

The BSP working branch adds `.github/workflows/bsp-regressions.yml` with
host feature regressions and independent ARM GCC Release builds for the
`2016_03` and `ORIGINAL` filter maps. Every ARM link runs the existing
image verification. A configured workflow is not evidence of a passing run;
review the Actions logs before merging or using a resulting firmware image.
The GPIO extraction does not change the pin mappings, startup ordering, or
power-button behavior. Physical verification remains outstanding.
