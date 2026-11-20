cmake_minimum_required(VERSION 3.20)

project(FX9A_CMSIS_LL LANGUAGES C ASM)

# This is the independent build entry point for the CMSIS-Core + STM32 LL port.
# The working Mbed CE build remains at the repository root during migration.
#
# A firmware target will be added after the following are present:
#   - CMSIS-Core / STM32F1 device headers
#   - STM32F1 LL sources/headers
#   - project-owned startup/vector code
#   - linker script for the FX-9A STM32F103
#   - minimal platform main/clock implementation
#
# Keeping configuration successful at this stage makes the directory usable
# without implying that a flashable image already exists.

set(FX9A_MCU "STM32F103xC" CACHE STRING "FX-9A MCU family")
set(FX9A_HSE_HZ 16000000 CACHE STRING "FX-9A external crystal frequency")
set(FX9A_SYSCLK_HZ 72000000 CACHE STRING "FX-9A target system clock")

message(STATUS "FX-9A CMSIS/LL port scaffold")
message(STATUS "MCU: ${FX9A_MCU}")
message(STATUS "HSE: ${FX9A_HSE_HZ} Hz")
message(STATUS "SYSCLK: ${FX9A_SYSCLK_HZ} Hz")
message(STATUS "No firmware target yet; see cmsis_ll/README.md")
