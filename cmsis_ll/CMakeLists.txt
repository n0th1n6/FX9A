cmake_minimum_required(VERSION 3.20)

# Configure with -DCMAKE_TOOLCHAIN_FILE=cmsis_ll/cmake/arm-none-eabi-gcc.cmake
project(FX9A_CMSIS_LL LANGUAGES C ASM)

set(FX9A_HSE_HZ 16000000 CACHE STRING "FX-9A external crystal frequency")
set(FX9A_SYSCLK_HZ 72000000 CACHE STRING "FX-9A target system clock")
if(NOT FX9A_HSE_HZ STREQUAL "16000000" OR NOT FX9A_SYSCLK_HZ STREQUAL "72000000")
    message(FATAL_ERROR "Boot milestone is validated only for 16 MHz HSE / 72 MHz SYSCLK")
endif()

set(FX9A_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(fx9a_boot.elf
    platform/startup_stm32f103_hd.s
    platform/clock.c
    platform/main.c
)
target_include_directories(fx9a_boot.elf PRIVATE
    "${FX9A_ROOT}/RadioStation/CMSIS"
)
target_compile_definitions(fx9a_boot.elf PRIVATE
    STM32F10X_HD
    HSE_Value=16000000
)
target_compile_options(fx9a_boot.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-std=c11;-Os;-ffreestanding;-fno-builtin;-Wall;-Wextra>
)
target_link_options(fx9a_boot.elf PRIVATE
    -nostdlib
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103xc.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/fx9a_boot.map"
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)
target_link_libraries(fx9a_boot.elf gcc)
set_target_properties(fx9a_boot.elf PROPERTIES LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103xc.ld")

add_custom_command(TARGET fx9a_boot.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:fx9a_boot.elf>
        "${CMAKE_CURRENT_BINARY_DIR}/fx9a_boot.bin"
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:fx9a_boot.elf>
        "${CMAKE_CURRENT_BINARY_DIR}/fx9a_boot.hex"
    COMMENT "Generate debug-only boot test images (do not flash without full backup)"
    VERBATIM
)
