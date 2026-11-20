#!/usr/bin/env python3
"""Analyze release v0.1.0 and compare its ARM vector table to the boot port.

Downloads no files itself: pass locally available release and boot images.
Release binary is the readback-verified Mbed CE reference; the port is a
minimal independent image. Differences in code addresses are expected.
"""
import argparse
import hashlib
import pathlib
import struct

BASE = 0x08000000
FLASH_SIZE = 256 * 1024
RAM_BASE = 0x20000000
RAM_SIZE = 64 * 1024  # Accept original IAR's wider RAM bound for comparison.
CORE_VECTORS = [
    "MSP", "Reset", "NMI", "HardFault", "MemManage", "BusFault",
    "UsageFault", "reserved7", "reserved8", "reserved9", "reserved10",
    "SVC", "DebugMon", "reserved13", "PendSV", "SysTick",
]
DEVICE_VECTORS = [
    "WWDG", "PVD", "TAMPER", "RTC", "FLASH", "RCC", "EXTI0",
    "EXTI1", "EXTI2", "EXTI3", "EXTI4", "DMA1_Channel1",
    "DMA1_Channel2", "DMA1_Channel3", "DMA1_Channel4",
    "DMA1_Channel5", "DMA1_Channel6", "DMA1_Channel7",
    "ADC1_2", "USB_HP_CAN1_TX", "USB_LP_CAN1_RX0",
    "CAN1_RX1", "CAN1_SCE", "EXTI9_5", "TIM1_BRK",
    "TIM1_UP", "TIM1_TRG_COM", "TIM1_CC", "TIM2", "TIM3",
    "TIM4", "I2C1_EV", "I2C1_ER", "I2C2_EV", "I2C2_ER",
    "SPI1", "SPI2", "USART1", "USART2", "USART3",
    "EXTI15_10", "RTCAlarm", "USBWakeUp", "TIM8_BRK",
    "TIM8_UP", "TIM8_TRG_COM", "TIM8_CC", "ADC3",
    "FSMC", "SDIO", "TIM5", "SPI3", "UART4", "UART5",
    "TIM6", "TIM7", "DMA2_Channel1", "DMA2_Channel2",
    "DMA2_Channel3", "DMA2_Channel4_5",
]
NAMES = CORE_VECTORS + DEVICE_VECTORS

EXPECTED_RELEASE_SHA256 = (
    "c790f815bd599462151d17711d04dcfd6243747608622eded8dd985b772e57af"
)


def inspect(label, path):
    data = pathlib.Path(path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if len(data) < len(NAMES) * 4 or len(data) > FLASH_SIZE:
        raise SystemExit(f"{label}: invalid image size {len(data)}")
    words = struct.unpack_from("<" + "I" * len(NAMES), data)
    msp, reset = words[:2]
    if not RAM_BASE <= msp <= RAM_BASE + RAM_SIZE or msp & 7:
        raise SystemExit(f"{label}: invalid/alignment MSP 0x{msp:08X}")
    if not reset & 1 or not BASE <= (reset & ~1) < BASE + len(data):
        raise SystemExit(f"{label}: invalid reset vector 0x{reset:08X}")
    print(f"{label}: bytes={len(data)}, sha256={digest}")
    print(f"{label}: MSP=0x{msp:08X}; reset=0x{reset:08X}")
    return words, digest


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--release", required=True)
    p.add_argument("--port", required=True)
    args = p.parse_args()
    old, oldhash = inspect("release-v0.1.0", args.release)
    if oldhash != EXPECTED_RELEASE_SHA256:
        raise SystemExit("Release SHA-256 mismatch: refusing to compare unexpected image")
    new, _ = inspect("cmsis-boot", args.port)
    print("Vector comparison (release, CMSIS boot):")
    for i, name in enumerate(NAMES):
        print(f"  {i:02d} {name:20} 0x{old[i]:08X} 0x{new[i]:08X}")
    # Core/IRQ vector counts and flash layout must be the same; function
    # addresses and RAM top need not be identical across runtime implementations.
    print("PASS: release SHA, image bounds, both MSP/reset vectors and vector counts")
    print("NOTE: binary comparison alone does NOT prove HSE speed, PLL, PC5,")
    print("      interrupt semantics, power-up safety, or RF hardware behavior.")


if __name__ == "__main__":
    main()
