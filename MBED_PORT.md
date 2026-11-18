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
Build, flash readback, and normal power-on have been verified on an FX9A. Keep
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

The main encoder and CAT frequency commands now tune through the gaps between
amateur bands, within the existing overall range of 1.79900 to 29.70099 MHz.
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
