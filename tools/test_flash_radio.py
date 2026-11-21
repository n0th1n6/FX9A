#!/usr/bin/env python3
"""Offline CLI/Tcl integration tests. No USB/debug probe is accessed."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BACKEND = r'''
import os, sys
from pathlib import Path
root=Path(os.environ['FX9A_TEST_DEVICE'])
fault=os.environ.get('FX9A_TEST_FAULT','')
op,*args=sys.argv[1:]
with (root/'events').open('a') as f: f.write(op+'\n')
def region(address):
    address=int(address,0)
    for base,name in [(0x08000000,'flash'),(0x1ffff800,'options'),(0x1ffff7e8,'uid')]:
        if base <= address < base+(root/name).stat().st_size: return root/name,address-base
    raise ValueError(address)
if op=='dump':
    path,address,size=args; source,offset=region(address)
    data=source.read_bytes()[offset:offset+int(size,0)]
    if fault=='backup-flash' and Path(path).name=='flash-read2.bin': data=bytes([data[0]^1])+data[1:]
    Path(path).write_bytes(data)
elif op=='verify':
    path,address=args; source,offset=region(address); data=Path(path).read_bytes()
    assert source.read_bytes()[offset:offset+len(data)]==data, 'verification mismatch'
elif op=='program':
    data=Path(args[0]).read_bytes(); current=bytearray((root/'flash').read_bytes())
    end=(len(data)+2047)//2048*2048; current[:end]=b'\xff'*end; current[:len(data)]=data
    if fault=='post-image': current[100]^=1
    if fault=='post-tail': current[-1]^=1
    if fault=='post-padding': current[len(data)]^=1
    (root/'flash').write_bytes(current)
    if fault=='post-options': (root/'options').write_bytes(b'\x01'*16)
    if fault=='post-eeprom': (root/'eeprom').write_bytes(b'\x01'*2048)
elif op=='eeprom':
    directory=Path(args[0]); data=(root/'eeprom').read_bytes()
    for n in (1,2):
        value=data if not(fault=='backup-eeprom' and n==2) else b'\x01'*2048
        (directory/f'fx9a-eeprom-read{n}.hex.txt').write_text(value.hex()+'\n')
elif op=='reset': pass
'''
STUBS = r'''
proc backend {args} { return [exec $::env(FX9A_TEST_PYTHON) $::env(FX9A_TEST_BACKEND) {*}$args] }
proc init {} {}
proc halt {} {}
proc wait_halt {timeout} {}
proc read_memory {address width count} {
    if {$address == 0xe0042000} {
        if {$::env(FX9A_TEST_FAULT) eq "wrong-target"} { return 0x410 }
        return 0x414
    }
    if {$address == 0x1ffff7e0} { return 256 }
    error "unexpected device read"
}
proc dump_image {path address size} { backend dump $path $address $size }
proc verify_image {path address format} { backend verify $path $address }
proc flash {operation erase path address format} {
    if {$operation ne "write_image" || $erase ne "erase" || $address != 0x08000000 || $format ne "bin"} { error "bad flash command" }
    backend program $path
}
proc reset {mode} {
    if {$mode ne "run"} { error "bad reset command" }
    backend reset
}
proc echo {message} { puts $message }
proc shutdown {args} { if {[llength $args]} { exit 1 }; exit 0 }
rename source real_source
proc source {path} {
    if {[file tail $path] eq "read_eeprom_stlink.tcl"} {
        backend eeprom $::fx9a_eeprom_dir
    } else { uplevel 1 [list real_source $path] }
}
source [lindex $argv 0]
'''
FAKE = '''#!{python}
import os, subprocess, sys
from pathlib import Path
script=Path(sys.argv[-1])
root=Path(os.environ['FX9A_TEST_DEVICE'])
if os.environ.get('FX9A_TEST_FAULT')=='swapped-probe' and script.name=='program.tcl':
    (root/'uid').write_bytes(b'changed-probe')
sys.exit(subprocess.call(['tclsh', os.environ['FX9A_TEST_STUBS'],str(script)]))
'''


@unittest.skipUnless(shutil.which('tclsh'), 'offline Tcl integration requires tclsh')
class FlashCLI(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fx9a-flash-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name, data in [('flash', b'\xa5'*(256*1024)), ('options', bytes(range(16))),
                           ('uid', bytes(range(12))), ('eeprom', bytes(range(256))*8)]:
            (self.root/name).write_bytes(data)
        self.image = self.root/'radio $[image]".bin'
        self.data = struct.pack('<II',0x2000c000,0x08000131)+bytes(10000)
        self.image.write_bytes(self.data)
        self.backup = self.root/'backup $[folder]"'
        for name, text in [('backend.py', BACKEND), ('stubs.tcl', STUBS), ('openocd', FAKE.format(python=sys.executable))]:
            (self.root/name).write_text(text)
        (self.root/'openocd').chmod(0o755)
        self.env = dict(os.environ, FX9A_TEST_DEVICE=str(self.root), FX9A_TEST_PYTHON=sys.executable,
                        FX9A_TEST_BACKEND=str(self.root/'backend.py'), FX9A_TEST_STUBS=str(self.root/'stubs.tcl'))

    def run_cli(self, fault='', extra=(), success=False):
        result = subprocess.run([sys.executable,str(ROOT/'tools/flash_radio.py'),str(self.image),
                                 '--openocd',str(self.root/'openocd'),'--backup-dir',str(self.backup),
                                 '--yes',*extra],env=dict(self.env,FX9A_TEST_FAULT=fault),capture_output=True,text=True)
        self.assertEqual(result.returncode,0 if success else 1,result.stdout+result.stderr)
        return (self.root/'events').read_text().splitlines() if (self.root/'events').exists() else []

    def test_embedded_eeprom_reader_propagates_failure_and_restores_cpu(self):
        reader = ROOT/'tools/read_eeprom_stlink.tcl'
        # Fail at the disabled GPIO clock, before any GPIO output is changed.
        script = r'''
set fx9a_eeprom_embedded 1
set resumed 0
proc init {} { error "unexpected init" }
proc stm32f1x.cpu {operation} { return running }
proc halt {} {}
proc wait_halt {timeout} {}
proc read_memory {address width count} { return 0 }
proc echo {message} {}
proc resume {} { set ::resumed 1 }
proc shutdown {} { error "unexpected shutdown" }
set failed [catch {source [lindex $argv 0]} reason]
if {!$failed || !$resumed || ![string match "*EEPROM backup/cleanup failed*" $reason]} { error "failure contract broken: $reason" }
'''
        wrapper=self.root/'reader-test.tcl'; wrapper.write_text(script)
        result=subprocess.run(['tclsh',str(wrapper),str(reader)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_complete_flash_and_reset(self):
        events=self.run_cli(success=True)
        self.assertEqual(events[-1],'reset')
        self.assertEqual(events.count('program'),1)
        self.assertEqual((self.root/'flash').read_bytes()[:len(self.data)],self.data)
        self.assertEqual((self.root/'eeprom').read_bytes(),bytes(range(256))*8)
        self.assertEqual(json.loads((self.backup/'manifest.json').read_text())['status'],'flashed_verified_reset')

    def test_failures_before_program(self):
        for fault in ('wrong-target','backup-flash','backup-eeprom','swapped-probe'):
            with self.subTest(fault=fault):
                events=self.run_cli(fault)
                self.assertNotIn('program',events); self.assertNotIn('reset',events)
                if self.backup.exists(): shutil.rmtree(self.backup)
                (self.root/'events').unlink(missing_ok=True)

    def test_readback_failures_never_reset(self):
        for fault in ('post-image','post-tail','post-padding','post-options','post-eeprom'):
            with self.subTest(fault=fault):
                (self.root/'flash').write_bytes(b'\xa5'*(256*1024))
                (self.root/'options').write_bytes(bytes(range(16)))
                (self.root/'eeprom').write_bytes(bytes(range(256))*8)
                events=self.run_cli(fault)
                self.assertIn('program',events); self.assertNotIn('reset',events)
                shutil.rmtree(self.backup); (self.root/'events').unlink()

    def test_dry_run_never_contacts_probe(self):
        self.assertEqual(self.run_cli(extra=['--dry-run'],success=True),[])
        self.assertFalse(self.backup.exists())

    def test_invalid_image_never_contacts_probe(self):
        for data in (b'\0'*100, b'\0'*10000, self.data+b'\0'*(256*1024)):
            self.image.write_bytes(data)
            self.assertEqual(self.run_cli(),[])
            self.assertFalse(self.backup.exists())

    def test_existing_backup_directory_is_preserved(self):
        self.backup.mkdir(); (self.backup/'keep').write_text('keep')
        self.assertEqual(self.run_cli(),[])
        self.assertEqual((self.backup/'keep').read_text(),'keep')


if __name__ == '__main__':
    unittest.main()
