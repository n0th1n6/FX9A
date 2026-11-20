#!/usr/bin/env python3
"""Build-time structural checks for the experimental standalone full radio.

These checks establish link-time completeness, NOT RF safety or equivalent
hardware behavior. They deliberately do not validate measured oscillator or
radio state.
"""
import argparse
import json
from pathlib import Path
import subprocess

REQUIRED = {
    "main", "board_init", "board_task", "RF_ToTx", "RF_ToRx",
    "TIM2_IRQHandler", "TIM3_IRQHandler", "TIM4_IRQHandler",
    "DMA1_Channel4_IRQHandler", "USART1_IRQHandler",
    "EXTI2_IRQHandler", "adc_init", "usart_init",
}
FORBIDDEN_SOURCE_FRAGMENTS = (
    "/RadioStation/StdPeriph_Driver/",
    "/RadioStation/CMSIS/",
    "/mbed-os/",
    "/port/mbed_main.c",
)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--elf", required=True)
    p.add_argument("--nm", default="arm-none-eabi-nm")
    p.add_argument("--compile-commands", required=True)
    args = p.parse_args()

    commands = json.loads(Path(args.compile_commands).read_text())
    full = [x for x in commands if "fx9a_full.elf.dir/" in
            x.get("command", "") + x.get("output", "")]
    if not full:
        raise SystemExit("FAIL: no full radio compile commands found")
    for cmd in full:
        filename = cmd["file"].replace("\\", "/")
        if any(fragment in filename for fragment in FORBIDDEN_SOURCE_FRAGMENTS):
            raise SystemExit(f"FAIL: old framework/driver source linked: {filename}")
        line = cmd["command"].replace("\\\\", "/")
        if "/RadioStation/StdPeriph_Driver" in line or "/RadioStation/CMSIS" in line:
            raise SystemExit("FAIL: legacy ST peripheral/CMSIS include path remains")
        for official in (
            "vendor/cmsis-core/CMSIS/Core/Include",
            "vendor/cmsis-device-f1/Include",
            "vendor/stm32f1xx-hal-driver/Inc",
        ):
            if official not in line:
                raise SystemExit(f"FAIL: missing official ST/CMSIS include: {official}")
        if "-DSTM32F103xE" not in line:
            raise SystemExit("FAIL: wrong STM32 device selected")
    required_drivers = {
        "fx9a_gpio_ll.c", "fx9a_rcc_ll.c", "fx9a_irq_ll.c",
        "fx9a_exti_ll.c", "bsp_timer.c", "bsp_pwm.c",
        "bsp_adc.c", "bsp_usart.c", "device_contracts.c",
    }
    sources = {Path(x["file"]).name for x in full}
    if required_drivers - sources:
        raise SystemExit(f"FAIL: missing low-layer drivers: {sorted(required_drivers - sources)}")


    result = subprocess.run(
        [args.nm, "--defined-only", args.elf],
        check=True, capture_output=True, text=True
    )
    symbols = {
        line.strip().split()[-1]
        for line in result.stdout.splitlines() if line.strip()
    }
    missing = REQUIRED - symbols
    if missing:
        raise SystemExit(f"FAIL: required radio/IRQ symbols missing: {sorted(missing)}")
    print(f"PASS: {len(full)} source units, official ST CMSIS/LL paths, no legacy drivers/headers/Mbed")
    print(f"PASS: {len(REQUIRED)} required application and IRQ symbols present")
    print("CAUTION: structural checks only; RF hardware remains unverified")

if __name__ == "__main__":
    main()
