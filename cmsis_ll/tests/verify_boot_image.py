#!/usr/bin/env python3
"""Minimal vector-table sanity check for the standalone FX-9A boot binary.

This checks layout only. It does NOT establish hardware or clock correctness.
"""
import argparse
import pathlib
import struct

FLASH_BASE = 0x08000000
FLASH_END = FLASH_BASE + 256 * 1024
RAM_BASE = 0x20000000
RAM_END = RAM_BASE + 48 * 1024
VECTOR_COUNT = 16 + 60  # STM32F103 high-density Cortex-M3 vector entries


def check(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) < VECTOR_COUNT * 4:
        raise SystemExit(f"FAIL: image too small for {VECTOR_COUNT} vectors")
    if len(data) > FLASH_END - FLASH_BASE:
        raise SystemExit("FAIL: image exceeds 256 KiB flash")
    stack, reset = struct.unpack_from("<II", data)
    if stack != RAM_END:
        raise SystemExit(f"FAIL: stack pointer 0x{stack:08x}, expected 0x{RAM_END:08x}")
    if not (FLASH_BASE <= (reset & ~1) < FLASH_BASE + len(data)) or not (reset & 1):
        raise SystemExit(f"FAIL: reset vector 0x{reset:08x} not valid Thumb flash address")
    print(f"PASS: {len(data)} bytes, MSP=0x{stack:08x}, reset=0x{reset:08x}")
    print("Layout only. Verify clock/PC5 behavior via debugger before any RF tests.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", help="fx9a_boot.bin from the CMSIS/LL build")
    check(parser.parse_args().binary)
