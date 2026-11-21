#!/usr/bin/env python3
"""Run Mbed production logic regressions and portable feature tests."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
tests = root / "tools/feature_tests"
for script in ("test_frequency.py", "test_encoders.py", "test_rx_alignment.py",
               "feature_tests/test_rf_protection.py", "feature_tests/test_tune_oled.py",
               "feature_tests/test_eeprom_uart.py", "feature_tests/test_settings_startup.py"):
    subprocess.run([sys.executable, str(root / "tools" / script)], check=True)
with tempfile.TemporaryDirectory(prefix="fx9a-features-") as directory:
    for source in sorted(tests.glob("*_test.c")):
        variants = ([], ["-DFX9A_BOARD_ORIGINAL"]) if source.stem == "filter_map_test" else ([],)
        for defines in variants:
            binary = Path(directory) / source.stem
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall",
                            "-Wextra", "-Werror", "-fsanitize=undefined,address",
                            *defines, "-I", str(root / "RadioStation/BSP"),
                            str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
print("PASS: all Mbed feature regressions")
