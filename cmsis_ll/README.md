# FX-9A CMSIS-Core + STM32 LL Port

This directory is the development area for the Mbed-independent FX-9A firmware.

The working Mbed CE build remains at the repository root during the migration and acts as the known-good reference. The CMSIS/LL port is developed in parallel and should only replace the Mbed path after feature parity and hardware verification.

## Branch

Development branch:

`cmsis-ll-port`

The `master` branch should remain the known-good radio firmware while this port is in progress.

## Target

- MCU: STM32F103 high-density Cortex-M3 device used by the FX-9A
- Board crystal: 16 MHz
- System clock target: 72 MHz
- Build system: CMake + GCC ARM Embedded
- Runtime model: bare metal, no RTOS
- Core layer: CMSIS-Core / STM32F1 device headers
- Peripheral layer: STM32 LL where useful, direct register access for small timing-critical operations
- Application logic: retain and gradually extract the existing FX-9A radio logic

## Migration rule

Do not perform a mechanical API translation from Mbed/StdPeriph to LL.

When a subsystem is moved into this tree, also address the matching items in the root `TODO.md`. Examples:

- USART/DMA migration includes CAT buffer safety and protocol fixes.
- I2C/EEPROM migration includes error propagation and recovery.
- ADC migration includes fast SWR protection.
- DDS migration includes integer tuning-word math.
- Band control migration includes hardware-revision tables.
- UI migration includes continuous-tuning enable/disable.
- TX/RX migration includes correct DDS-before-TX sequencing.
- CW migration includes the low-power automatic-tuner TUNE state.

## Intended layout

```text
cmsis_ll/
├── CMakeLists.txt
├── README.md
├── cmake/
│   └── arm-none-eabi-gcc.cmake
├── app/                 # extracted radio/application logic
├── board/               # FX-9A pin map and hardware revision data
├── drivers/             # project-local peripheral/device drivers
├── platform/            # startup, clock, vectors, IRQ ownership
├── linker/              # STM32F103 linker script
└── tests/               # port-specific host or build-time checks
```

Directories will be added as real files are migrated rather than populated with placeholder code.

## First milestone — source prepared, hardware not yet validated

The minimal Mbed-independent boot image sources are now in the branch. The boot
image **is not the radio firmware** and has not been verified by flashing the
radio. ARM GCC build and the vector/stack sanity test **passed on GitHub Actions** (run 44695123236). No hardware boot test has been performed. It uses the
existing bundled STM32F10x CMSIS header as a *transitional dependency* and does
not yet link the modern STM32 LL library.

The minimal image:

- has its own 76-entry STM32F103 high-density vector table and reset handler;
- initializes .data and .bss without the Mbed runtime;
- latches the board's existing PC5 power-hold signal high;
- attempts 16 MHz HSE / 2 × 9 = 72 MHz PLL, with HSI fallback and debugger status;
- performs no PA, TX, DDS, microphone, CW or filter control;
- exposes debugger-readable `fx9a_boot_marker`, `fx9a_heartbeat`,
  `fx9a_clock_status`, and `fx9a_system_clock_hz`.

The planned milestone and checks are:

1. Add current STM32F1 CMSIS device support and STM32 LL sources/headers.
2. Add project-owned startup/vector table.
3. Add linker script matching the working image layout.
4. Configure the 16 MHz HSE and 72 MHz system clock.
5. Boot to a minimal `main()` without Mbed.
6. Verify vector placement, stack, clock, and a safe GPIO heartbeat or debugger-visible state.
7. Do not enable the RF PA during this milestone.

Only after this boots reliably should GPIO board control and the rest of the radio be migrated.

## Build separation

The existing root build remains the Mbed CE reference build.

The future CMSIS/LL build will use a separate build directory, for example:

```sh
cmake -S cmsis_ll -B build-cmsis-ll \
  -DCMAKE_TOOLCHAIN_FILE=cmsis_ll/cmake/arm-none-eabi-gcc.cmake
cmake --build build-cmsis-ll
```

The independent CMake build now defines `fx9a_boot.elf`, plus debug-only
`fx9a_boot.bin` and `fx9a_boot.hex`. Use an absolute toolchain file path,
for example from the repository root:

```sh
cmake -S cmsis_ll -B build-cmsis-ll -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmsis_ll/cmake/arm-none-eabi-gcc.cmake"
cmake --build build-cmsis-ll
python3 cmsis_ll/tests/verify_boot_image.py build-cmsis-ll/fx9a_boot.bin
```

The Python check verifies vector-table placement and bounds only; it does not
prove hardware compatibility. The image is **not ready for ordinary flashing**.
Both legacy IAR and Mbed use base address `0x08000000`, which could overwrite
a resident bootloader depending on the board; consult `DFU_BACKUP.md` and
back up the device before any programming attempt. The flash and RAM limits
follow the current Mbed target (256 KiB flash / 48 KiB RAM), not the IAR file's
different RAM region. Confirm the actual chip marking and memory map first.

## Safety

During port bring-up:

- keep RF power stages disabled where practical;
- compare behavior against the current Mbed CE image;
- preserve EEPROM layout and saved radio state;
- verify DDS, filters, PTT, SWR protection, and CW timing before transmitting;
- use a dummy load for first TX tests.

## Next verification gate

- [x] Build the standalone image with ARM GCC and run the vector test (GitHub Actions run 44695123236).
- [ ] Review `fx9a_boot.map` and full ELF/vector contents before programming (the CI job already generated and checked basic vector data).
- [ ] Confirm STM32 chip marking, RAM capacity, bootloader and flash layout.
- [ ] Obtain full flash backup and verify readback.
- [ ] Test through SWD with PA disabled; inspect PC5 and clock status.
- [ ] Replace the bundled 2009 CMSIS headers with current CMSIS-Core/device pack
      and introduce STM32F1 LL drivers for subsequent peripheral migrations.

Build verification: https://github.com/n0th1n6/FX9A/actions/runs/44695123236

## Full application — low-level driver migration status

The independent CMake project builds two targets:

- `fx9a_boot.elf`: minimal diagnostic image.
- `fx9a_full.elf`: complete experimental radio application linked with
  project-owned reset/clock code and no Mbed CE runtime.

**First-stage peripheral replacement complete:** all compiled 2009
StdPeriph driver source files have been removed from the standalone full
target. Project-local register-level implementations now handle:

| Subsystem | Standalone low-layer file |
| --- | --- |
| GPIO + AFIO | `drivers/fx9a_gpio_ll.c` |
| Clock enable/reset + bus clocks | `drivers/fx9a_rcc_ll.c` |
| NVIC + EXTI | `drivers/fx9a_irq_ll.c` |
| TIM2/TIM3/TIM4 | `drivers/fx9a_tim_ll.c` |
| DMA1 | `drivers/fx9a_dma_ll.c` |
| ADC1 | `drivers/fx9a_adc_ll.c` |
| USART1 | `drivers/fx9a_usart_ll.c` |

The full standalone image has its own `platform/radio_main.c` and uses TIM4
for the CW keyer without Mbed's TIM4-to-TIM5 remapping. Its compile/link test
and pre-existing radio logic regression tests have passed in GitHub Actions.

**Do not confuse this stage with finished ST LL integration.** The new drivers
are *direct-register / LL-style compatibility implementations* using
the old `stm32f10x.h` CMSIS device header plus old declaration headers.
Official modern `stm32f1xx_ll_*.h` API adoption, new CMSIS device headers,
and complete removal of old compatibility declarations are still open.
That work is required before calling the requested **full STM32 LL migration**
complete.

Also, compile/link and host-level tests cannot guarantee the UART, DMA, ADC,
timer, GPIO alternate-function, boot, or RF hardware behavior; this build
is not cleared to flash or transmit. The user-facing TUNE and continuous
tuning enable/disable features are separate TODO items and are not yet included.

## Port completion gates

- [x] Full app builds independently of Mbed CE.
- [x] All old STM32 StdPeriph driver `.c` files removed from standalone target.
- [x] GPIO/RCC/IRQ/TIM/DMA/ADC/USART register-level replacements compile/link.
- [x] Separate standalone `main()` and original TIM4 ownership.
- [x] Full image vector checks and existing host radio logic tests.
- [x] CI enforces required radio symbols and rejects linked StdPeriph source.
- [ ] Integrate official ST STM32F1 LL headers/drivers with modern CMSIS device.
- [ ] Remove the 2009 compatibility headers and old API aliases.
- [ ] Add register-level host simulation or emulated-peripheral tests for
      clock configuration, UART/DMA, ADC, EXTI, timers and GPIO pin mappings.
- [ ] Verify the physical MCU marking and memory map.
- [ ] Full RF-disabled hardware bring-up, followed by measured RX/TX tests.

## v0.1.0 release binary reference

The public v0.1.0 release contains a 53,808-byte Mbed CE firmware image
(`fx9a-v0.1.0.bin`), described in the release notes as matching the image
flashed and read back from the tested radio.

The `cmsis_ll/tests/compare_release.py` utility compares the authenticated
release binary to the standalone boot image. CI downloads the release, verifies
its SHA-256 against the GitHub release digest, checks both initial stack pointers
and Thumb reset vectors, and prints all 76 vector-table entries side by side.
The comparison **passed** in GitHub Actions run 44695773339.

This is a structural check only. It does not prove identical interrupts,
correct PLL setup, bootloader preservation, analog/RF behavior or readiness to
flash the standalone boot image. The source code of the port still uses
transitional 2009 CMSIS headers; STM32 LL peripheral integration is pending.

Reference: https://github.com/n0th1n6/FX9A/releases/tag/v0.1.0
Comparison run: https://github.com/n0th1n6/FX9A/actions/runs/44695773339
