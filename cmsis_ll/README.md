# FX-9A — standalone CMSIS-Core / STM32F1 LL firmware

Development branch: `cmsis-ll-port`. The `master` branch retains the
previously tested Mbed CE firmware. Do **not** merge or flash the experimental
image before hardware review.

## Software architecture

The full FX-9A application now has its own startup, vector table, 16 MHz HSE /
72 MHz PLL setup, an explicit ADC /6 prescaler (12 MHz ADC clock), linker
script and main loop. The independent full firmware
uses neither Mbed CE nor any old STM32 Standard Peripheral Library **source or
header** file.

The project uses pinned official vendor components:

| Component | Version | Source |
| --- | --- | --- |
| CMSIS-Core | `5.9.0` | `ARM-software/CMSIS_5` |
| STM32F1 CMSIS device | `v4.3.5` | `STMicroelectronics/cmsis-device-f1` |
| STM32F1 LL driver headers | `v1.1.10` | `STMicroelectronics/stm32f1xx-hal-driver` |

These files are fetched into the ignored `cmsis_ll/vendor/` directory.
They are **not** copied into the repository. The selected MCU is
`STM32F103xE`, with a 16 MHz HSE.

The complete application now calls CMSIS and the official STM32F1 LL APIs directly. The obsolete project-owned compatibility drivers and the entire `cmsis_ll/compat/` directory have been deleted. Native peripheral logic lives in the `cmsis_ll/app/` sources, while `board/ll_pins.h` converts board signal bitmasks to the STM32F1 LL pin-selection encoding.

The native application uses these subsystems:

| Module | Direct CMSIS/LL responsibility |
| --- | --- |
| `app/bsp_board.c`, `app/bsp_key.c` | GPIO, AFIO, EXTI and CMSIS interrupt priorities |
| `app/bsp_timer.c`, `app/bsp_pwm.c` | TIM2/TIM3/TIM4, encoder scheduling and sidetone |
| `app/bsp_adc.c` | ADC1 scan and DMA1 circular samples |
| `app/bsp_usart.c` | USART1 and DMA1 CH4 for CAT |
| `app/bsp_ad9834.c`, `app/bsp_ad9951.c` | DDS GPIO controls |
| `app/i2c.c`, `app/bsp_oled.c` | EEPROM bus and OLED bit-banged I/O |
| Other `app/bsp_*.c` | Amplifier, audio, VFO input and other board peripherals |

`RadioStation/BSP/` retains source files inherited from the original
firmware for reference; the independent CMake target explicitly selects its
native LL counterparts. The original `master` branch remains the known-good
Mbed CE reference.

## Build

From the repository root with `git`, `cmake`, `Ninja`, Python 3, and
`arm-none-eabi-gcc` installed:

```sh
bash cmsis_ll/vendor/fetch_vendor.sh
cmake -S cmsis_ll -B build-cmsis-ll -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmsis_ll/cmake/arm-none-eabi-gcc.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-cmsis-ll
```

The independent build produces **two distinct targets**:

- `fx9a_boot.elf/.bin/.hex`: diagnostic boot image, **not** radio firmware.
- `fx9a_full.elf/.bin/.hex`: complete experimental radio application,
  including RX/TX, VFO, DDS, OLED, CAT, encoder, keyer and EEPROM code.

Both use the current flash base `0x08000000`, 256 KiB flash and 48 KiB RAM,
matching the previous Mbed target assumptions. Verify the actual STM32 chip,
the saved flash image and bootloader regions before programming anything.

## Automated checks

The GitHub Actions workflow `.github/workflows/cmsis-ll-boot.yml`:

1. Downloads vendor packages at the pinned tags.
2. Builds both independent images with ARM GCC.
3. Checks reset vectors, stack addresses and flash image bounds.
4. Checks that the linked full ELF contains the radio/interrupt handlers.
5. Fails if the standalone build includes old Mbed, old CMSIS, or 2009
   StdPeriph headers/sources.
6. Compiles `platform/device_contracts.c`, asserting MCU IRQ positions,
   selected peripheral register offsets and compatibility structure layouts.
7. Runs the existing encoder, frequency and RX-alignment regression scripts.
8. Runs `tests/native_ll_smoke.c` on Linux with vendor LL GPIO code against
   mapped, simulated STM32F1 registers (GPIO pin encoding, timers, DMA, ADC,
   USART and EXTI).
9. Authenticates the reference v0.1.0 BIN by SHA-256 and compares boot vector
   structure. The 53,808-byte v0.1.0 image is the previous Mbed CE release,
   **not** functionally equivalent to the minimal boot image.

The native host test guards STM32F1 LL pin-mask encoding (including high GPIO
pins), timer PWM bit positions, DMA channel programming and USART divisor.
The platform clock initializes SYSCLK to 72 MHz and ADC clock to 12 MHz. Verified CI run:
https://github.com/n0th1n6/FX9A/actions/runs/44703550499

The host register tests cannot model physical clock startup, interrupt timing,
I2C bus transactions, radio RF outputs or analog measurement accuracy. Passing
them is necessary but not sufficient for radio safety.

## Hardware and release validation still required

- [ ] Confirm exact STM32 model, RAM size, bootloader arrangement and flash map.
- [ ] Make a full verified backup and confirm DFU programming restrictions
      in the root `DFU_BACKUP.md`.
- [ ] Inspect initial clock, PC5 power-hold and vector behavior under SWD.
- [ ] Check GPIO mapping for the original and later board revisions.
- [ ] Check encoder, TIM3 sidetone, TIM4 keyer, ADC/DMA, EEPROM and CAT
      against the physical radio.
- [ ] Verify LO/BFO, filter switching, protection response and TX/RX
      sequencing before enabling RF output.
- [ ] Only then conduct controlled dummy-load transmit measurements.

The user-facing feature TODOs—low-power antenna-tuner TUNE mode,
continuous-tuning ON/OFF in the long-press BAND menu, TX/RX sequencing
refactor, faster SWR protection, board revision tables and calibration—are
**not automatically implemented by the LL port**. They remain separate tasks
in the root `TODO.md` and should be implemented and tested before making
a new radio firmware release.

Release reference:
https://github.com/n0th1n6/FX9A/releases/tag/v0.1.0
