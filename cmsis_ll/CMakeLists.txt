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

# Transitional complete radio build: preserve the production application and
# legacy peripherals while replacing the Mbed startup/runtime. The original
# root CMake build remains untouched and continues to use Mbed CE.
option(FX9A_BUILD_FULL "Build the full radio application without Mbed" ON)
if(FX9A_BUILD_FULL)
    set(FX9A_BSP_SOURCES
        bsp_24lc16.c bsp_ad9834.c bsp_ad9951.c bsp_adc.c bsp_baud.c
        bsp_board.c bsp_dac.c bsp_debug.c bsp_iambic_keyer.c bsp_kenwood.c
        bsp_key.c bsp_oled.c bsp_oled_api.c bsp_pwm.c bsp_receive_att.c
        bsp_timer.c bsp_transmit_power.c bsp_usart.c bsp_vol.c i2c.c
    )
    list(TRANSFORM FX9A_BSP_SOURCES PREPEND
        "${FX9A_ROOT}/RadioStation/BSP/")

    set(FX9A_STDPERIPH_SOURCES
        misc.c stm32f10x_adc.c stm32f10x_dma.c stm32f10x_exti.c
        stm32f10x_gpio.c stm32f10x_rcc.c stm32f10x_tim.c
        stm32f10x_usart.c
    )
    list(TRANSFORM FX9A_STDPERIPH_SOURCES PREPEND
        "${FX9A_ROOT}/RadioStation/StdPeriph_Driver/")

    add_executable(fx9a_full.elf
        platform/startup_stm32f103_hd.s
        platform/clock.c
        ${FX9A_ROOT}/port/mbed_main.c
        ${FX9A_ROOT}/port/bsp_delay.c
        ${FX9A_ROOT}/port/legacy_clock.c
        ${FX9A_ROOT}/port/layout_checks.c
        ${FX9A_BSP_SOURCES}
        ${FX9A_STDPERIPH_SOURCES}
    )
    target_include_directories(fx9a_full.elf PRIVATE
        ${FX9A_ROOT}/port
        ${FX9A_ROOT}/RadioStation/BSP
        ${FX9A_ROOT}/RadioStation/CMSIS
        ${FX9A_ROOT}/RadioStation/StdPeriph_Driver
    )
    target_compile_definitions(fx9a_full.elf PRIVATE
        STM32F10X_HD
        HSE_Value=16000000
        USE_STDPERIPH_DRIVER
    )
    target_compile_options(fx9a_full.elf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-std=c11;-Os;-fshort-enums;-fsigned-char;-Wno-deprecated-declarations;-Wno-int-conversion>
    )
    target_link_options(fx9a_full.elf PRIVATE
        -nostartfiles
        "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103xc.ld"
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/fx9a_full.map"
        -Wl,--gc-sections
        -Wl,--print-memory-usage
    )
    target_link_libraries(fx9a_full.elf m c gcc)
    set_target_properties(fx9a_full.elf PROPERTIES LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f103xc.ld")
    add_custom_command(TARGET fx9a_full.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:fx9a_full.elf>
            "${CMAKE_CURRENT_BINARY_DIR}/fx9a_full.bin"
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:fx9a_full.elf>
            "${CMAKE_CURRENT_BINARY_DIR}/fx9a_full.hex"
        COMMENT "Generate full-application experimental image (not RF-verified)"
        VERBATIM
    )
endif()
