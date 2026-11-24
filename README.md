# FX9A

Firmware for the FX-9A amateur-radio transceiver, originating from BG2FX.
`master` retains Mbed CE; `cmsis-ll-port` contains the native CMSIS-Core / STM32 LL implementation.

## Build from the CLI

Install Python 3.8+, Git, CMake, Ninja and an ARM GNU toolchain (`arm-none-eabi-*`).
From the repository root:

```sh
# Download missing firmware dependencies, configure, build and verify.
./tools/build_radio.py --init-deps

# Subsequent builds reuse the configured directory and installed dependencies.
./tools/build_radio.py
```

The helper detects the checkout and builds its full radio application. Output is
`build/cli-cmsis-ll/fx9a_full.bin` on `cmsis-ll-port`, or
`build/cli-mbed/fx9a.bin` on `master`. Image/link checks run before success is
reported, and `build-manifest.json` records the selected options and image SHA256.

Useful build options:

- `--build-dir /path/to/build`: choose a separate build directory.
- `--toolchain-bin /path/to/arm-toolchain/bin`: select the ARM compiler tools.
- `--jobs 8`: set build parallelism.
- `--board-revision ORIGINAL`: select the original filter map; default is `2016_03`.
- `--vfo-accel`: enable experimental VFO acceleration; default is OFF.

An existing build cache for another checkout, generator or compiler is refused.
Choose a new build directory rather than overwriting that cache. Dependency
sources are downloaded only when missing and `--init-deps` is supplied; Mbed's
own configure step may also prepare its Python environment.

## Build and flash

Connect ST-Link/SWD to the powered radio, let the existing firmware finish booting,
release PTT/paddles and connect a secure dummy load. OpenOCD with ST-Link support
must be available for flashing.

```sh
# Inspect the complete build/flash plan without doing any work or contacting USB.
./tools/build_radio.py --flash --dry-run

# Build and verify, then back up/program/verify/reset. Type FLASH at the prompt.
./tools/build_radio.py --flash

# Flash an explicit image that has already been built.
./tools/flash_radio.py build/cli-cmsis-ll/fx9a_full.bin
# On master, use build/cli-mbed/fx9a.bin instead.
```

Flashing runs only after a successful build and image checks. The flash helper
checks the target, obtains two matching flash, option-byte and EEPROM backups,
and verifies the new image and preserved regions before resetting. It does not
program EEPROM settings or option bytes. Backups, hashes and logs are saved
under `backups/flash-<UTC timestamp>/`.

Both `build_radio.py --flash` and `flash_radio.py` accept:

- `--branch BRANCH`: switch this same worktree and build its working files, including local edits. The flash tool builds and verifies before flashing; omit its image argument with this option. Conflicting branch switches stop without stashing or discarding edits.
- `--serial SERIAL`: select an ST-Link.
- `--openocd /path/to/openocd`: select the OpenOCD executable.
- `--backup-dir /path/to/new-directory`: choose a new backup directory.
- `--yes`: skip the prompt while retaining the selected backup and verification checks.
- `--skip-eeprom-backup`: skip EEPROM backup and preservation verification; retain flash and option-byte backups and checks. EEPROM backup is enabled by default.

The EEPROM backup requires the existing firmware's initialized GPIO configuration.
Blank or damaged boards need a separate recovery procedure. Both firmware builds
and offline helper tests pass; the complete helper flash sequence still needs
validation on the radio. Check physical startup/display and RX operation before TX.

See [full build/flashing instructions and failure handling](tools/FLASHING.md).
After committing reviewed changes, use `./tools/pushrepo.py --branch BRANCH`
to switch this same clean worktree, push to `origin`, and verify the remote SHA.
Run helper regressions with `python3 tools/test_build_radio.py` and
`python3 tools/test_flash_radio.py` (`tclsh` required for flash tests).

## Firmware documentation

- [Mbed CE build and port notes](MBED_PORT.md)
- [Settings persistence and CAT behavior](SETTINGS_AND_CAT.md)
- [Implementation status and remaining hardware checks](TODO.md)
- [Change history](CHANGELOG.md)
