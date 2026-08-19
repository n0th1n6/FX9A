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
This is build validation only: it has not been flashed or run on an FX9A. Keep
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

IAR's function-local no-optimization pragma protects the rotary encoder's
20,000-iteration debounce delay. GCC ignores that pragma, so `port/iar_compat.h`
applies the equivalent GCC function attribute only while compiling
`bsp_key.c`; otherwise the optimizer removes the delay completely.

Every link runs `tools/verify_image.cmake`. The build fails if the vector base,
timer ownership, CAT USART/DMA handlers, encoder debounce function, or
calibrated delay instruction sequence no longer matches the compatibility
contract above.

Mbed CE's `MCU_STM32F103xC` target does not attach a system-clock source file.
The FX9A target therefore reuses Mbed CE's high-density STM32F103 clock source,
which supports the radio's 16 MHz crystal and configures a 72 MHz system clock.
