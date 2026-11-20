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

## First milestone

The first CMSIS/LL milestone is intentionally small:

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

At the moment the CMake entry point is scaffolding only; it intentionally does not create a firmware target until the CMSIS/device/startup/linker inputs are present.

## Safety

During port bring-up:

- keep RF power stages disabled where practical;
- compare behavior against the current Mbed CE image;
- preserve EEPROM layout and saved radio state;
- verify DDS, filters, PTT, SWR protection, and CW timing before transmitting;
- use a dummy load for first TX tests.
