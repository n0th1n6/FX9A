# FX-9A — standalone CMSIS-Core / STM32F1 LL firmware

Development branch: `cmsis-ll-port`. The `master` branch retains the
previously tested Mbed CE firmware. Do **not** merge or flash the experimental
image before hardware review.

## Software architecture

The full FX-9A application now has its own startup, vector table, 16 MHz HSE /
72 MHz PLL setup, linker script and main loop. The independent full firmware
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

The complete application uses seven low-level peripheral modules:

| Module | Role |
| --- | --- |
| `drivers/fx9a_gpio_ll.c` | GPIO/AFIO; ST LL pin I/O with correct F1 LL mask encoding |
| `drivers/fx9a_rcc_ll.c` | ST LL bus clock enable/reset, bus clock calculations |
| `drivers/fx9a_irq_ll.c` | CMSIS NVIC; ST LL EXTI management |
| `drivers/fx9a_tim_ll.c` | TIM2 input scanning; TIM3 sidetone PWM; TIM4 keyer |
| `drivers/fx9a_dma_ll.c` | DMA1 CH1 ADC sampling, CH4 USART1 TX |
| `drivers/fx9a_adc_ll.c` | ADC1 scan/calibration, PC0–PC3 |
| `drivers/fx9a_usart_ll.c` | USART1 / Kenwood CAT UART |

Where an ST LL inline call is appropriate the driver uses `LL_*`;
F1 register-level access is retained where it is simpler or more deterministic.
The actual radio application has an intentionally small compatibility surface
(`compat/fx9a_legacy_types.h`) defining the *application-facing* old function
names and structures. It does **not** import the 2009 device header or old
peripheral libraries. Deleting all legacy API call sites in the original BSP
would be a separate application refactor, not a missing low-level driver.

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
8. Runs `tests/driver_smoke.c` on Linux against mapped, simulated STM32F1
   peripheral registers (RCC, GPIO, TIM, DMA, ADC and USART).
9. Authenticates the reference v0.1.0 BIN by SHA-256 and compares boot vector
   structure. The 53,808-byte v0.1.0 image is the previous Mbed CE release,
   **not** functionally equivalent to the minimal boot image.

The host register tests cannot model physical clock startup, interrupt timing,
I2C bus transactions, radio RF outputs or analog measurement accuracy. Passing
them is necessary but not sufficient for radio safety.

## Runtime validation still required

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
