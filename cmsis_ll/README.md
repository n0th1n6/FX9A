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
