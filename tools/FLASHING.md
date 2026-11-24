# Building and flashing the FX-9A from the CLI

## Push a committed branch

```sh
./tools/pushrepo.py --branch cmsis-ll-port
./tools/pushrepo.py --branch master
./tools/pushrepo.py --branch master --dry-run
```

The utility requires a clean worktree, switches this same checkout using normal
Git rules, pushes the selected commit to the same branch on `origin`, and checks
that the remote SHA matches. It never stages files, creates commits, force-pushes,
or merges. Commit reviewed changes first. Dry runs do not switch or push.

## Build and optionally flash

Install Python 3.8+, CMake, Ninja, Git and ARM GNU tools on PATH. The build
helper selects the native CMSIS/LL application when `cmsis_ll/CMakeLists.txt`
is present; on master it builds Mbed CE. It never builds the small boot-test
image. Use the branch containing the firmware you want to install.

Select a branch automatically from this same worktree:

```sh
./tools/build_radio.py --branch cmsis-ll-port
./tools/build_radio.py --branch master --flash
./tools/flash_radio.py --branch cmsis-ll-port --skip-eeprom-backup
```

Both tools use normal `git switch` in the checkout where the tools are located.
They build the actual working files there, including uncommitted edits; they do
not create a separate worktree, stash changes, reset files or force a switch.
Conflicting edits or a branch checked out in another worktree stop the command
before building/flashing. Successful switches leave this checkout on the selected
branch. Commit or reconcile conflicting edits before changing branches.

Local branch tips take priority, followed by cached `origin` branches. If neither
exists, the tool fetches that branch from `origin` and creates a tracking branch.
It does not automatically pull existing branches. `--branch` also initializes
missing build dependencies. `--dry-run` prints the planned switch/build/flash
without changing branches or files, downloading sources, or contacting hardware;
an uncached branch prints the required fetch and stops the preview.

In `flash_radio.py`, `--branch` replaces the positional image argument and runs
build plus image verification before the normal flash flow. It accepts the same
build options (`--build-dir`, `--toolchain-bin`, `--jobs`, `--board-revision`,
`--vfo-accel`, `--init-deps`). The loaded CLI implementation is retained across
the switch, including when the selected branch has older tool scripts. Later
invocations use the scripts present in that selected branch.

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

With `--flash`, `--serial`, `--openocd`, `--backup-dir`, `--yes` and
`--skip-eeprom-backup` are forwarded to `flash_radio.py`.
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
Either branch uses the same script; without `--branch`, supply an explicit raw `.bin`.
ELF/HEX files, the small CMSIS boot-test image, incorrect vectors and oversized
images are rejected. Vector checks establish image layout, not RF behavior.

Optional arguments:

```sh
./tools/flash_radio.py /path/to/fx9a.bin --serial STLINK_SERIAL
./tools/flash_radio.py /path/to/fx9a.bin --openocd /usr/local/bin/openocd
./tools/flash_radio.py /path/to/fx9a.bin --backup-dir /path/to/new-backup-directory
./tools/flash_radio.py /path/to/fx9a.bin --yes
./tools/flash_radio.py /path/to/fx9a.bin --skip-eeprom-backup
```

`--skip-eeprom-backup` disables all EEPROM reads: both the pre-flash captures
and the post-flash preservation comparison. Flash and option-byte backups,
target identity checks and image verification remain required. The plan and
manifest explicitly record the skipped EEPROM checks; EEPROM preservation is
not verified in this mode. The default still backs up and verifies EEPROM.
For build-and-flash, use `./tools/build_radio.py --flash --skip-eeprom-backup`.

`--yes` skips the prompt; selected backup and verification checks remain required.
An existing backup directory is never reused or overwritten. Without `--backup-dir`,
backups go under the checkout's ignored `backups/flash-<UTC timestamp>/` directory.

The script freezes a copy of the selected image, halts the CPU, checks the target,
and obtains two matching full flash, option-byte and, by default, 2048-byte EEPROM captures.
It saves raw `.bin` backups, EEPROM hex text, device UID, SHA256 hashes and OpenOCD
logs. Before programming it rechecks UID, option bytes and the complete old flash,
then erases only the pages covered by the image and writes at `0x08000000`.
It checks the full readback, final-page erased padding, untouched flash tail,
option bytes and, unless skipped, EEPROM before issuing `reset run`.

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
python3 tools/test_branch_checkout.py
python3 tools/test_pushrepo.py
```
