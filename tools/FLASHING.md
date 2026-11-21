# Building and flashing the FX-9A from the CLI

## Build and optionally flash

Install Python 3.8+, CMake, Ninja, Git and ARM GNU tools on PATH. The build
helper selects the native CMSIS/LL application when `cmsis_ll/CMakeLists.txt`
is present; on master it builds Mbed CE. It never builds the small boot-test
image. Use the branch containing the firmware you want to install.

```sh
./tools/build_radio.py --init-deps   # Fetch missing source dependencies, then build.
./tools/build_radio.py               # Configure/build incrementally and verify.
./tools/build_radio.py --flash       # Hand the verified binary to the flash helper.
./tools/build_radio.py --flash --dry-run  # Preview; no files, downloads or USB access.
```

Defaults are Release, board revision `2016_03`, VFO acceleration OFF, and up to
eight parallel build jobs. Use `--board-revision ORIGINAL`, `--vfo-accel`,
`--jobs N`, `--build-dir PATH` or `--toolchain-bin PATH` to choose otherwise.
`--init-deps` downloads only missing pinned CMSIS/LL sources or initializes the
Mbed submodule. Mbed's configure step can also install its Python requirements.
For the previously used macOS ARM GCC 10 installation, select it with:

```sh
./tools/build_radio.py --toolchain-bin /usr/local/opt/arm-gcc-bin@10/bin
```

The default outputs are `build/cli-cmsis-ll/fx9a_full.bin` and
`build/cli-mbed/fx9a.bin`, respectively. Every successful build runs the branch's
image/link checks and records options plus SHA256 in `build-manifest.json`.
A failed configure, build or verification never starts flashing an old binary.
An incompatible build cache is preserved and refused; choose a different build
directory to change source trees, generators or compilers.

With `--flash`, `--serial`, `--openocd`, `--backup-dir` and `--yes` are forwarded
to `flash_radio.py`. Its backup and verification protections stay mandatory.
The build CLI never contacts hardware unless `--flash` is supplied.

## Flash a previously built image

Use Python 3 and OpenOCD with `interface/stlink.cfg` and `target/stm32f1x.cfg`.
Connect ST-Link/SWD to the powered radio, let its existing firmware finish booting,
release PTT/paddles and connect a secure dummy load. The script expects the checked
FX-9A hardware: STM32F1 high-density device ID `0x414`, 256 KiB flash and a 24LC16
on PE2/PE3/PE4. It does not supply power through ST-Link.

Build the desired firmware first. From the repository root:

```sh
# Inspect the selected binary without contacting the radio.
./tools/flash_radio.py build/cli-cmsis-ll/fx9a_full.bin --dry-run

# Back up, program, verify and reset (type FLASH at the prompt).
./tools/flash_radio.py build/cli-cmsis-ll/fx9a_full.bin
```

For Mbed, pass the full `fx9a.bin` produced in your chosen Mbed build directory.
Either branch uses the same script; it always requires an explicit raw `.bin`.
ELF/HEX files, the small CMSIS boot-test image, incorrect vectors and oversized
images are rejected. Vector checks establish image layout, not RF behavior.

Optional arguments:

```sh
./tools/flash_radio.py /path/to/fx9a.bin --serial STLINK_SERIAL
./tools/flash_radio.py /path/to/fx9a.bin --openocd /usr/local/bin/openocd
./tools/flash_radio.py /path/to/fx9a.bin --backup-dir /path/to/new-backup-directory
./tools/flash_radio.py /path/to/fx9a.bin --yes
```

`--yes` skips the prompt; backup and verification checks remain mandatory.
An existing backup directory is never reused or overwritten. Without this option,
backups go under the checkout's ignored `backups/flash-<UTC timestamp>/` directory.

The script freezes a copy of the selected image, halts the CPU, checks the target,
and obtains two matching full flash, option-byte and 2048-byte EEPROM captures.
It saves raw `.bin` backups, EEPROM hex text, device UID, SHA256 hashes and OpenOCD
logs. Before programming it rechecks UID, option bytes and the complete old flash,
then erases only the pages covered by the image and writes at `0x08000000`.
It checks the full readback, final-page erased padding, untouched flash tail,
option bytes and EEPROM before issuing `reset run`.

The EEPROM reader temporarily asserts write protection and restores the GPIO
outputs. Neither EEPROM contents nor option bytes are programmed. EEPROM reads
require the expected GPIO configuration from running FX-9A firmware; a blank MCU
or one halted before peripheral initialization is refused. This is a normal
firmware-update helper, not a blank-board or damaged-firmware recovery utility.

On failure, no later programming/reset step runs. The CPU may remain halted;
keep the printed backup directory and logs for diagnosis. Do not reset after a
failed/partial write until a complete image has been restored. A successful
readback still requires checking the physical startup/display and RX operation
before TX. The helper has been tested offline with a simulated OpenOCD target;
its full sequence has not yet been exercised on the radio.

Run the offline tests (requires `tclsh`, no ST-Link/USB access):

```sh
python3 tools/test_build_radio.py
python3 tools/test_flash_radio.py
```
