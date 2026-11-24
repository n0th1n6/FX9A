#!/usr/bin/env python3
"""Configure, build and verify this FX-9A worktree; optionally select a branch and flash."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
from flash_radio import validate_image, main as flash_main
from branch_checkout import select_branch

ROOT = Path(__file__).resolve().parents[1]


def positive(value):
    result = int(value)
    if result < 1:
        raise argparse.ArgumentTypeError('must be at least 1')
    return result


def cache_values(directory):
    values = {}
    cache = directory / 'CMakeCache.txt'
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
                key, value = line.split('=', 1)
                values[key.split(':', 1)[0]] = value
    return values


def commands(args, native, build, env, root=None):
    root = ROOT if root is None else root
    source = root / 'cmsis_ll' if native else root
    cached = cache_values(build)
    if cached.get('CMAKE_HOME_DIRECTORY') and Path(cached['CMAKE_HOME_DIRECTORY']).resolve() != source:
        raise ValueError('Build directory belongs to another source tree. Choose a new --build-dir.')
    if cached.get('CMAKE_GENERATOR', 'Ninja') != 'Ninja':
        raise ValueError('Build directory uses another generator. Choose a new --build-dir.')
    compiler = shutil.which('arm-none-eabi-gcc', path=env['PATH'])
    if compiler and cached.get('CMAKE_C_COMPILER') and Path(cached['CMAKE_C_COMPILER']).resolve() != Path(compiler).resolve():
        raise ValueError('Build directory uses another compiler. Choose a new --build-dir for this toolchain.')
    configure = ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                 '-DFX9A_BOARD_REVISION=' + args.board_revision,
                 '-DFX9A_VFO_ACCEL=' + ('ON' if args.vfo_accel else 'OFF')]
    if native:
        configure += ['-DCMAKE_TOOLCHAIN_FILE=' + str(source/'cmake/arm-none-eabi-gcc.cmake'),
                      '-DFX9A_BUILD_FULL=ON']
        target, binary, elf = 'fx9a_full.elf', build/'fx9a_full.bin', build/'fx9a_full.elf'
        verify = [[sys.executable, str(source/'tests/verify_boot_image.py'), str(binary)],
                  [sys.executable, str(source/'tests/verify_full_image.py'), '--elf', str(elf),
                   '--compile-commands', str(build/'compile_commands.json')]]
    else:
        configure += ['-DMBED_TARGET=FX9A', '-DMBED_TOOLCHAIN=GCC_ARM']
        target, binary, elf = 'fx9a', build/'fx9a.bin', build/'fx9a.elf'
        verify = [['cmake', '-DELF_FILE=' + str(elf),
                   '-DNM_TOOL=' + (shutil.which('arm-none-eabi-nm', path=env['PATH']) or 'arm-none-eabi-nm'),
                   '-DOBJDUMP_TOOL=' + (shutil.which('arm-none-eabi-objdump', path=env['PATH']) or 'arm-none-eabi-objdump'),
                   '-P', str(root/'tools/verify_image.cmake')]]
    return [configure, ['cmake', '--build', str(build), '--target', target,
                        '--parallel', str(args.jobs)], *verify], binary


def prepare_dependencies(native, initialize, env, root=None):
    root = ROOT if root is None else root
    if native:
        vendor = root/'cmsis_ll/vendor'
        required = [vendor/'cmsis-core/CMSIS/Core/Include/core_cm3.h',
                    vendor/'cmsis-device-f1/Include/stm32f1xx.h',
                    vendor/'stm32f1xx-hal-driver/Inc/stm32f1xx_ll_gpio.h']
        setup = ['bash', str(vendor/'fetch_vendor.sh')]
    else:
        required = [root/'mbed-os/tools/cmake/mbed_toolchain_setup.cmake']
        setup = ['git', 'submodule', 'update', '--init', '--depth', '1', '--', 'mbed-os']
    if not all(path.is_file() for path in required):
        if not initialize:
            raise ValueError('Build dependencies are missing. Run again with --init-deps to download them.')
        run(setup, env, root)
        if not all(path.is_file() for path in required):
            raise ValueError('Dependency setup did not produce the required source files.')


def run(command, env, root=None):
    print('+ ' + shlex.join(command), flush=True)
    subprocess.run(command, cwd=ROOT if root is None else root, env=env, check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--branch', help='switch this worktree to a local/origin branch, preserve local edits, and initialize missing dependencies')
    parser.add_argument('--build-dir', type=Path, help='out-of-source build directory (default: build/cli-cmsis-ll or build/cli-mbed)')
    parser.add_argument('--toolchain-bin', type=Path, help='directory containing arm-none-eabi tools; prepended to PATH')
    parser.add_argument('--jobs', type=positive, default=min(8, os.cpu_count() or 1))
    parser.add_argument('--board-revision', choices=('2016_03', 'ORIGINAL'), default='2016_03')
    parser.add_argument('--vfo-accel', action='store_true', help='enable experimental optical VFO acceleration (default: OFF)')
    parser.add_argument('--init-deps', action='store_true', help='download missing pinned CMSIS/LL dependencies or initialize the Mbed submodule')
    parser.add_argument('--flash', action='store_true', help='flash the successfully built/verified image using flash_radio.py')
    parser.add_argument('--yes', action='store_true', help='forward --yes to the flash helper')
    parser.add_argument('--serial', help='ST-Link serial for --flash')
    parser.add_argument('--openocd', help='OpenOCD executable for --flash')
    parser.add_argument('--backup-dir', type=Path, help='new backup directory for --flash')
    parser.add_argument('--skip-eeprom-backup', action='store_true',
                        help='with --flash, skip EEPROM backup and preservation verification')
    parser.add_argument('--dry-run', action='store_true', help='show commands without building, downloading, creating files or contacting hardware')
    args = parser.parse_args(argv)
    if not args.flash and any((args.yes, args.serial, args.openocd, args.backup_dir, args.skip_eeprom_backup)):
        parser.error('--yes/--serial/--openocd/--backup-dir/--skip-eeprom-backup require --flash')
    try:
        root, commit = ROOT, None
        native = (root/'cmsis_ll/CMakeLists.txt').is_file()
        if args.branch:
            selected = select_branch(ROOT, args.branch, args.dry_run)
            if selected is None:
                return 0
            root, commit, native = selected
        build = (args.build_dir or root/'build'/('cli-cmsis-ll' if native else 'cli-mbed')).expanduser().resolve()
        if build == root or build == Path(build.anchor) or (build/'CMakeLists.txt').is_file():
            raise ValueError('Choose an out-of-source build directory, not a source directory.')
        env = os.environ.copy()
        if args.toolchain_bin:
            directory = args.toolchain_bin.expanduser().resolve()
            if not (directory/'arm-none-eabi-gcc').is_file() or not os.access(directory/'arm-none-eabi-gcc', os.X_OK):
                raise ValueError('--toolchain-bin must contain an executable arm-none-eabi-gcc.')
            env['PATH'] = str(directory) + os.pathsep + env.get('PATH', '')
        steps, binary = commands(args, native, build, env, root)
        if args.backup_dir is not None:
            args.backup_dir = args.backup_dir.expanduser().resolve()
        if args.openocd and (os.sep in args.openocd or args.openocd.startswith('~')):
            args.openocd = str(Path(args.openocd).expanduser().resolve())
        flash_command = [sys.executable, str(ROOT/'tools/flash_radio.py'), str(binary)]
        for flag, value in (('--serial', args.serial), ('--openocd', args.openocd), ('--backup-dir', args.backup_dir)):
            if value is not None:
                flash_command += [flag, str(value)]
        if args.yes:
            flash_command += ['--yes']
        if args.skip_eeprom_backup:
            flash_command += ['--skip-eeprom-backup']
        print(f'Firmware: {"CMSIS/LL" if native else "Mbed CE"}\nImage: {binary}', flush=True)
        if args.dry_run:
            if args.init_deps or args.branch:
                print('Missing dependencies would be downloaded before configuring.')
            for step in steps + ([flash_command] if args.flash else []):
                print('+ ' + shlex.join(step))
            return 0
        required = ['cmake', 'ninja', 'arm-none-eabi-gcc', 'arm-none-eabi-ar',
                    'arm-none-eabi-objcopy', 'arm-none-eabi-nm', 'arm-none-eabi-objdump', 'arm-none-eabi-size']
        if not native:
            required += ['arm-none-eabi-g++']
        missing = [tool for tool in required if not shutil.which(tool, path=env['PATH'])]
        if missing:
            raise ValueError('Missing tools: ' + ', '.join(missing) + '. Set PATH or --toolchain-bin.')
        prepare_dependencies(native, args.init_deps or bool(args.branch), env, root)
        for step in steps:
            run(step, env, root)
        data = binary.read_bytes()
        validate_image(data)
        manifest = {'firmware': 'cmsis-ll' if native else 'mbed', 'source': str(root), 'branch': args.branch, 'commit': commit,
                    'board_revision': args.board_revision, 'vfo_accel': args.vfo_accel,
                    'image': str(binary), 'sha256': hashlib.sha256(data).hexdigest()}
        (build/'build-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(f'\nBuild and image checks passed: {binary}\nSHA256: {manifest["sha256"]}', flush=True)
        if args.flash:
            print('+ ' + shlex.join(flash_command), flush=True)
            return flash_main(flash_command[2:])
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, KeyboardInterrupt) as error:
        print(f'ERROR: {error}\nNo later build or flash step will be run.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
