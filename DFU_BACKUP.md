# Read-only DFU backup procedure

Do not run a download, erase, detach, leave, unprotect, reset, or flash command
until the existing radio image has been captured and verified. In particular,
never use `-D`, `-e`, `-R`, `leave`, `mass-erase`, `unprotect`, or `force` during
the backup stage.

## 1. Inspect the interface

Place the radio in DFU mode manually, then run:

```sh
dfu-util --list
system_profiler SPUSBDataType
```

Record the VID, PID, serial number, interface, alternate setting, and the exact
memory-map string. Do not assume that alternate setting zero or the STM32's
entire 256 KiB flash is exposed. The checked-in IAR application begins at
`0x08000000`, but the location and protection scheme of the radio's DFU loader
are still unknown.

## 2. Upload only after confirming the map

For a DfuSe-compatible interface which explicitly reports readable internal
flash, the read operation has this form:

```sh
mkdir -p backups
dfu-util -d VID:PID -a ALT \
  -s 0x08000000:LENGTH \
  -U backups/fx9a-original-read1.bin
```

`VID:PID`, `ALT`, and `LENGTH` are placeholders. They must come from the actual
descriptor; do not copy the example unchanged. `-U` means upload from the radio
to the computer. No reset option should be added.

If the device reports read protection or rejects upload, stop. Never use
`unprotect`: on STM32 devices that operation can erase flash.

## 3. Verify the backup

Without leaving DFU mode, repeat the same upload to a second filename, then
compare and hash the captures:

```sh
cmp backups/fx9a-original-read1.bin backups/fx9a-original-read2.bin
shasum -a 256 backups/fx9a-original-read1.bin \
  backups/fx9a-original-read2.bin
od -An -tx4 -N8 backups/fx9a-original-read1.bin
```

The two captures must be identical. For an image starting at `0x08000000`, the
first word should be a plausible SRAM stack pointer and the second a Thumb reset
handler address in flash. Preserve the two raw captures and their hashes before
doing anything else.

## External EEPROM limitation

The radio stores operating settings and memories in a separate 24LC16 EEPROM.
A normal STM32 DFU upload cannot read that chip, so an MCU flash backup alone
does not preserve the radio's current channels and settings. Dumping the 24LC16
requires a separate, hardware-safe method after its bus and power arrangement
have been verified.
