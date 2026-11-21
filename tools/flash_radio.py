#!/usr/bin/env python3
"""Flash a full FX-9A .bin via ST-Link/OpenOCD after verified backups."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FLASH_BASE = 0x08000000
FLASH_SIZE = 256 * 1024
EEPROM_SIZE = 2048


def tcl(value):
    """Quote a single Tcl word without allowing substitutions, including paths."""
    value = str(value)
    for old, new in [('\\', '\\\\'), ('"', '\\"'), ('$', '\\$'), ('[', '\\['),
                     (']', '\\]'), ('\n', '\\n'), ('\r', '\\r')]:
        value = value.replace(old, new)
    return '"' + value + '"'


def validate_image(data):
    if not 4096 <= len(data) <= FLASH_SIZE:
        raise ValueError('Use a full radio .bin image (4 KiB to 256 KiB), not the boot test image.')
    stack, reset = struct.unpack_from('<II', data)
    if stack != 0x2000C000:
        raise ValueError(f'Unexpected initial stack 0x{stack:08x}; FX-9A builds use 0x2000c000.')
    if not reset & 1 or not FLASH_BASE <= reset & ~1 < FLASH_BASE + len(data):
        raise ValueError('Reset vector is not a Thumb address inside this image.')


def checked_pair(directory, stem, size, hexadecimal=False):
    copies = []
    for number in (1, 2):
        path = directory / f'{stem}{number}'
        data = bytes.fromhex(path.with_suffix('.hex.txt').read_text()) if hexadecimal else path.with_suffix('.bin').read_bytes()
        if len(data) != size:
            raise ValueError(f'Incomplete backup: {stem}{number} ({len(data)} bytes).')
        if hexadecimal:
            path.with_suffix('.bin').write_bytes(data)
        copies.append(data)
    if copies[0] != copies[1]:
        raise ValueError(f'{stem} backups differ; refusing to program.')
    return copies[0]


def run_openocd(args, directory, stage, body):
    script = directory / f'{stage}.tcl'
    script.write_text('''init
set fx9a_failure [catch {
    halt
    wait_halt 2000
    set device [expr {[lindex [read_memory 0xe0042000 32 1] 0] & 0xfff}]
    set size [lindex [read_memory 0x1ffff7e0 16 1] 0]
    if {$device != 0x414 || $size != 256} { error "Expected STM32F1 high-density 0x414 with 256 KiB flash" }
''' + body + '''
} fx9a_reason]
if {$fx9a_failure} {
    echo "FX9A FAILED: $fx9a_reason"
    shutdown error
} else {
    shutdown
}
''')
    command = [args.openocd, '-f', 'interface/stlink.cfg', '-c', 'transport select hla_swd']
    if args.serial:
        command += ['-c', 'adapter serial ' + tcl(args.serial)]
    command += ['-f', 'target/stm32f1x.cfg', '-c', 'gdb_port disabled; tcl_port disabled; telnet_port disabled', '-f', str(script)]
    print(f'\n[{stage}] OpenOCD log: {directory / (stage + ".log")}', flush=True)
    with (directory / f'{stage}.log').open('w') as log:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            for line in process.stdout:
                print(line, end='', flush=True)
                log.write(line)
            if process.wait():
                raise RuntimeError(f'OpenOCD {stage} failed; see {stage}.log.')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait()


def eeprom_commands(directory):
    return ('set fx9a_eeprom_embedded 1\nset fx9a_eeprom_dir ' + tcl(directory) +
            '\nsource ' + tcl(ROOT / 'tools/read_eeprom_stlink.tcl') + '\n')


def flash(args, directory, image):
    selected = directory / 'selected-image.bin'
    selected.write_bytes(image)  # Freeze the selected bytes before touching hardware.
    backup = ''
    for number in (1, 2):
        backup += f'dump_image {tcl(directory / f"flash-read{number}.bin")} 0x08000000 0x40000\n'
        backup += f'dump_image {tcl(directory / f"options-read{number}.bin")} 0x1ffff800 16\n'
    backup += f'dump_image {tcl(directory / "uid.bin")} 0x1ffff7e8 12\n'
    run_openocd(args, directory, 'backup', backup + eeprom_commands(directory))
    original = checked_pair(directory, 'flash-read', FLASH_SIZE)
    options = checked_pair(directory, 'options-read', 16)
    eeprom = checked_pair(directory, 'fx9a-eeprom-read', EEPROM_SIZE, hexadecimal=True)
    uid = (directory / 'uid.bin').read_bytes()
    if len(uid) != 12:
        raise ValueError('Incomplete device identity backup.')
    manifest = {'image': str(args.image), 'image_sha256': hashlib.sha256(image).hexdigest(),
                'uid': uid.hex(), 'flash_sha256': hashlib.sha256(original).hexdigest(),
                'eeprom_sha256': hashlib.sha256(eeprom).hexdigest(),
                'options_sha256': hashlib.sha256(options).hexdigest(), 'status': 'backups_verified'}
    (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    # Recheck identity and original flash before erase; refuse swapped/changed targets.
    guard = (f'verify_image {tcl(directory / "uid.bin")} 0x1ffff7e8 bin\n'
             f'verify_image {tcl(directory / "options-read1.bin")} 0x1ffff800 bin\n')
    post = directory / 'after'
    post.mkdir()
    run_openocd(args, directory, 'program', guard +
                f'verify_image {tcl(directory / "flash-read1.bin")} 0x08000000 bin\n' +
                f'flash write_image erase {tcl(selected)} 0x08000000 bin\n' +
                f'verify_image {tcl(selected)} 0x08000000 bin\n' +
                f'dump_image {tcl(post / "flash.bin")} 0x08000000 0x40000\n' +
                f'dump_image {tcl(post / "options.bin")} 0x1ffff800 16\n' + eeprom_commands(post))
    result = (post / 'flash.bin').read_bytes()
    # F103 high-density flash erases in 2 KiB pages. Only final-page padding may change.
    boundary = (len(image) + 2047) // 2048 * 2048
    expected = image + b'\xff' * (boundary - len(image)) + original[boundary:]
    if result != expected:
        raise ValueError('Flash readback differs from image or untouched flash tail.')
    if (post / 'options.bin').read_bytes() != options:
        raise ValueError('Option bytes changed unexpectedly; leaving CPU halted.')
    if checked_pair(post, 'fx9a-eeprom-read', EEPROM_SIZE, hexadecimal=True) != eeprom:
        raise ValueError('EEPROM changed unexpectedly; leaving CPU halted.')
    # Start firmware only after host-side checks, including EEPROM preservation.
    run_openocd(args, directory, 'start', guard +
                f'verify_image {tcl(post / "flash.bin")} 0x08000000 bin\nreset run\n')
    manifest['status'] = 'flashed_verified_reset'
    (directory / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'\nFlashed, verified and reset. Backups: {directory}\nCheck the physical display and RX operation before TX.')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, epilog='Connect ST-Link to the powered radio; release PTT/paddles and use a secure dummy load. Requires Python 3 and OpenOCD with ST-Link support. Never writes EEPROM settings or option bytes.')
    parser.add_argument('image', type=Path, help='explicit full radio .bin from either branch')
    parser.add_argument('--openocd', default='openocd', help='OpenOCD executable (default: PATH)')
    parser.add_argument('--serial', help='select an ST-Link serial number')
    parser.add_argument('--backup-dir', type=Path, help='new directory for backups/logs (default: repo/backups/flash-UTC timestamp)')
    parser.add_argument('--yes', action='store_true', help='skip the FLASH prompt; backups and verification remain mandatory')
    parser.add_argument('--dry-run', action='store_true', help='validate image and show plan without contacting hardware or creating files')
    args = parser.parse_args(argv)
    try:
        args.image = args.image.expanduser().resolve()
        if args.image.suffix.lower() != '.bin':
            raise ValueError('Supply a raw .bin image; ELF/HEX files are not accepted.')
        image = args.image.read_bytes()
        validate_image(image)
        print(f'Image: {args.image}\nBytes: {len(image)}\nSHA256: {hashlib.sha256(image).hexdigest()}')
        print('Plan: check STM32 -> compare two flash/options/EEPROM backups -> program -> verify readback and preserved EEPROM/options -> reset.')
        if args.dry_run:
            return 0
        if not shutil.which(args.openocd):
            raise ValueError(f'OpenOCD executable not found: {args.openocd}')
        if not args.yes and input('Type FLASH to continue: ').strip() != 'FLASH':
            print('Cancelled; hardware was not contacted.')
            return 0
        directory = (args.backup_dir or ROOT / 'backups' / datetime.now(timezone.utc).strftime('flash-%Y%m%dT%H%M%S.%fZ')).expanduser().resolve()
        directory.mkdir(parents=True, exist_ok=False)
        print(f'Backups/logs: {directory}', flush=True)
        flash(args, directory, image)
        return 0
    except (OSError, ValueError, RuntimeError, EOFError, KeyboardInterrupt) as error:
        print(f'ERROR: {error}\nNo further programming or reset will be attempted. If hardware was contacted, the CPU may remain halted; retain the backups and logs.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
