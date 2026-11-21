# Settings persistence and CAT behavior

Both `master` (Mbed) and `cmsis-ll-port` use the same persisted radio record sizes,
offsets and mode values. Volume changes remain RAM-only until a successful
power-button shutdown. Cutting bench-PSU power does not save pending changes.

The settings store reads/writes all 100 memory channels, both VFO banks, current
band selections, RIT/XIT, filter, volume, attenuator, key speed and menu. A fixed snapshot is taken with a short interrupt-mask section; I2C transfers
run with the previous interrupt state restored. Each write is read back against
the intended snapshot bytes. The original save marker stays at 1600.
Calibration at 1104..1133 is independent and is never changed by a settings save.

A 20-byte metadata record at 1152..1171 contains two identifying signatures,
format version 1, an in-progress/complete flag, payload CRC32 and header CRC32.
The payload checksum covers the existing meaningful records in their defined
order; unused EEPROM gaps are excluded. A verified in-progress record precedes
every payload write, and the complete record follows all verified writes.
This detects incomplete saves; there is no second copy for automatic rollback.

Existing radios with no metadata load the legacy records with semantic bounds
checks. Boot never writes defaults to EEPROM automatically. If a read fails or
metadata/checksum is invalid, the radio uses safe RAM defaults and blocks normal
settings saves so the stored bytes remain available for recovery. An explicit
factory reset initializes and saves new defaults. If shutdown saving fails, the
radio displays `ERR` in the TS area and retains power, allowing another save
attempt. A read/corruption fault at boot requires recovery or explicit reset.
Verify these display and power-hold behaviors on hardware before deployment.

Older firmware can read the unchanged legacy records. If older firmware saves
settings after metadata has been created, the checksum becomes stale: use an
explicit reset or restore a coherent EEPROM backup before returning to these
images. There is no automatic destructive migration.

CAT parsing now validates supported write lengths and values, answers malformed
or unsupported commands with `?;`, separates VFO A/B access, clamps key speed,
converts CAT offset Hz to the radio's 10 Hz resolution, and serializes IF offsets
with a sign. A conflicting write during TUNE cancels TUNE and is rejected; RX
cancels it directly. Writes affecting radio state are rejected during TX or
front-panel setup. Queries continue to work. The TX queue drops an entire reply
when capacity is insufficient, preventing partial protocol frames.

The implemented legacy read-only compatibility replies remain fixed for features
that this radio does not implement (for example split operation and DSP controls).
This is a subset of the [Kenwood reference](https://www.kenwood.com/i/products/info/amateur/pdf/ts590_g_pc_command_en_rev3.pdf),
not full TS-590 emulation. MR01/M4 remain the existing FX-9A extension.

Run `python3 tools/test_settings_cat.py` and `python3 tools/test_transport.py`.
These link production modules with mocked EEPROM/RF and exercise interrupted
saves at each operation and byte, metadata corruption, legacy settings, failure
propagation, parser fuzzing, all implemented queries and RF/TUNE interlocks.
RF behavior, synthesizer settling, real CAT clients and EEPROM timing still need
connected-radio checks. No hardware flash is part of these changes.
