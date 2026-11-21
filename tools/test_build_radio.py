#!/usr/bin/env python3
"""Build CLI orchestration tests; mocked tools never access hardware/network."""
import contextlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_radio


class BuildCLI(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fx9a-build-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()/'source with spaces'
        self.root.mkdir()
        self.build = self.root/'build/test'
        self.events = []
        self.fail_build = self.fail_verify = self.bad_image = False
        self.native()
        self.patch_root = patch.object(build_radio, 'ROOT', self.root)
        self.patch_root.start(); self.addCleanup(self.patch_root.stop)
        self.patch_which = patch.object(build_radio.shutil, 'which', side_effect=lambda name, **kwargs: '/fake/bin/'+name)
        self.patch_which.start(); self.addCleanup(self.patch_which.stop)
        self.patch_run = patch.object(build_radio, 'run', side_effect=self.fake_run)
        self.mock_run = self.patch_run.start(); self.addCleanup(self.patch_run.stop)

    def dependencies(self):
        if (self.root/'cmsis_ll').exists():
            paths = ['cmsis_ll/vendor/cmsis-core/CMSIS/Core/Include/core_cm3.h',
                     'cmsis_ll/vendor/cmsis-device-f1/Include/stm32f1xx.h',
                     'cmsis_ll/vendor/stm32f1xx-hal-driver/Inc/stm32f1xx_ll_gpio.h']
        else:
            paths = ['mbed-os/tools/cmake/mbed_toolchain_setup.cmake']
        for name in paths:
            p=self.root/name; p.parent.mkdir(parents=True, exist_ok=True); p.touch()

    def native(self):
        (self.root/'cmsis_ll').mkdir()
        (self.root/'cmsis_ll/CMakeLists.txt').touch()
        self.dependencies()

    def fake_run(self, command, env):
        self.events.append(command)
        if command[0] == 'bash' or command[:3] == ['git','submodule','update']:
            self.dependencies()
        if '--build' in command:
            if self.fail_build:
                raise subprocess.CalledProcessError(1, command)
            self.build.mkdir(parents=True, exist_ok=True)
            binary = self.build/('fx9a_full.bin' if 'fx9a_full.elf' in command else 'fx9a.bin')
            binary.write_bytes(bytes(8192) if self.bad_image else struct.pack('<II',0x2000c000,0x08000131)+bytes(8192))
        if self.fail_verify and ('-P' in command or any('verify_full_image.py' in arg for arg in command)):
            raise subprocess.CalledProcessError(1, command)

    def cli(self, *args):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return build_radio.main(['--build-dir',str(self.build),*args])

    def test_native_build_then_flash_only_verified_image(self):
        self.assertEqual(self.cli('--flash','--yes','--serial','serial','--openocd','/custom/openocd',
                                  '--backup-dir',str(self.root/'backup'),'--vfo-accel','--board-revision','ORIGINAL'),0)
        self.assertIn('-DFX9A_VFO_ACCEL=ON',self.events[0])
        self.assertIn('-DFX9A_BOARD_REVISION=ORIGINAL',self.events[0])
        self.assertIn('--target',self.events[1]); self.assertIn('fx9a_full.elf',self.events[1])
        self.assertTrue(any('verify_full_image.py' in arg for arg in self.events[-2]))
        self.assertEqual(self.events[-1][2],str(self.build/'fx9a_full.bin'))
        self.assertIn('--yes',self.events[-1]); self.assertIn('/custom/openocd',self.events[-1])
        manifest=json.loads((self.build/'build-manifest.json').read_text())
        self.assertEqual(manifest['firmware'],'cmsis-ll'); self.assertTrue(manifest['vfo_accel'])

    def test_mbed_checkout_and_build_only(self):
        import shutil
        shutil.rmtree(self.root/'cmsis_ll'); self.dependencies()
        self.assertEqual(self.cli(),0)
        self.assertIn('-DMBED_TOOLCHAIN=GCC_ARM',self.events[0])
        self.assertIn('-DFX9A_VFO_ACCEL=OFF',self.events[0])
        self.assertIn('fx9a',self.events[1]); self.assertIn('-P',self.events[-1])
        self.assertFalse(any('flash_radio.py' in arg for command in self.events for arg in command))

    def test_dry_run_is_read_only_even_with_flash_and_init(self):
        self.assertEqual(self.cli('--dry-run','--flash','--init-deps'),0)
        self.mock_run.assert_not_called(); self.assertFalse(self.build.exists())

    def test_build_or_verify_failure_never_flashes_stale_binary(self):
        self.build.mkdir(parents=True); (self.build/'fx9a_full.bin').write_bytes(b'stale')
        for failure in ('fail_build','fail_verify','bad_image'):
            with self.subTest(failure=failure):
                self.events.clear(); setattr(self,failure,True)
                self.assertEqual(self.cli('--flash'),1)
                self.assertFalse(any('flash_radio.py' in arg for command in self.events for arg in command))
                self.assertFalse((self.build/'build-manifest.json').exists())
                setattr(self,failure,False)

    def test_foreign_cache_is_preserved_and_refused(self):
        self.build.mkdir(parents=True)
        cache=self.build/'CMakeCache.txt'; original='CMAKE_HOME_DIRECTORY:INTERNAL=/another/project\n'
        cache.write_text(original)
        self.assertEqual(self.cli('--flash'),1); self.mock_run.assert_not_called()
        self.assertEqual(cache.read_text(),original)

    def test_missing_dependencies_require_explicit_download(self):
        dep=self.root/'cmsis_ll/vendor/cmsis-core/CMSIS/Core/Include/core_cm3.h'; dep.unlink()
        self.assertEqual(self.cli(),1); self.mock_run.assert_not_called()
        self.assertEqual(self.cli('--init-deps'),0); self.assertEqual(self.events[0][0],'bash')

    def test_relative_flash_paths_preserve_callers_directory(self):
        self.assertEqual(self.cli('--flash','--backup-dir','relative-backup','--openocd','./custom/openocd'),0)
        command=self.events[-1]
        self.assertEqual(command[command.index('--backup-dir')+1],str(Path('relative-backup').resolve()))
        self.assertEqual(command[command.index('--openocd')+1],str(Path('custom/openocd').resolve()))

    def test_toolchain_directory_must_contain_requested_compiler(self):
        self.assertEqual(self.cli('--toolchain-bin',str(self.root)),1)
        self.mock_run.assert_not_called()

    def test_flash_options_require_flash(self):
        with self.assertRaises(SystemExit) as raised:
            self.cli('--yes')
        self.assertEqual(raised.exception.code,2); self.mock_run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
