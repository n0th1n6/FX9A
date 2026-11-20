cmake_minimum_required(VERSION 3.20)

# Configure with -DCMAKE_TOOLCHAIN_FILE=cmsis_ll/cmake/arm-none-eabi-gcc.cmake
project(FX9A_CMSIS_LL LANGUAGES C ASM)

set(FX9A_HSE_HZ 16000000 CACHE STRING "FX-9A external crystal frequency")
set(FX9A_SYSCLK_HZ 72000000 CACHE STRING "FX-9A target system clock")
if(NOT FX9A_HSE_HZ STREQUAL "16000000" OR NOT FX9A_SYSCLK_HZ STREQUAL "72000000")
    message(FATAL_ERROR "Boot milestone is validated only for 16 MHz HSE / 72 MHz SYSCLK")
endif()

set(FX9A_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
# Official STM32F1 CMSIS + LL packages. CI checks out pinned vendor tags;
# local builds should run cmsis_ll/vendor/fetch_vendor.sh first.
set(FX9A_VENDOR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set(FX9A_CMSIS_CORE "${FX9A_VENDOR}/cmsis-core/CMSIS/Core/Include")
set(FX9A_CMSIS_DEVICE "${FX9A_VENDOR}/cmsis-device-f1/Include")
set(FX9A_ST_LL "${FX9A_VENDOR}/stm32f1xx-hal-driver/Inc")
foreach(pkg FX9A_CMSIS_CORE FX9A_CMSIS_DEVICE FX9A_ST_LL)
    if(NOT EXISTS "${${pkg}}")
        message(FATAL_ERROR "Missing ${pkg} at ${${pkg}}; run bash cmsis_ll/vendor/fetch_vendor.sh")
    endif()
endforeach()
set(FX9A_ST_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/compat"
    "${FX9A_CMSIS_DEVICE}"
    "${FX9A_CMSIS_CORE}"
    "${FX9A_ST_LL}"
)
set(FX9A_ST_DEFINES STM32F103xE HSE_VALUE=16000000 HSI_VALUE=8000000)

add_executable(fx9a_boot.elf
    platform/startup_stm32f103_hd.s
    platform/clock.c
    platform/main.c
)
target_include_directories(fx9a_boot.elf PRIVATE ${FX9A_ST_INCLUDES})
target_compile_definitions(fx9a_boot.elf PRIVATE ${FX9A_ST_DEFINES})
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
        bsp_24lc16.c    
         bsp_dac.c bsp_debug.c  bsp_kenwood.c
          bsp_oled_api.c 
           
    )
    list(TRANSFORM FX9A_BSP_SOURCES PREPEND
        "${FX9A_ROOT}/RadioStation/BSP/")


    add_executable(fx9a_full.elf
        platform/startup_stm32f103_hd.s
        platform/clock.c
        drivers/fx9a_gpio_ll.c
        drivers/fx9a_rcc_ll.c
        drivers/fx9a_irq_ll.c
        drivers/fx9a_exti_ll.c
        app/bsp_timer.c
        app/bsp_pwm.c
        app/bsp_vol.c
        app/bsp_transmit_power.c
        app/bsp_usart.c
        app/bsp_adc.c
        app/bsp_key.c
        app/bsp_board.c
        app/i2c.c
        app/bsp_receive_att.c
        app/bsp_oled.c
        app/bsp_iambic_keyer.c
        app/bsp_baud.c
        app/bsp_ad9951.c
        app/bsp_ad9834.c
        ${FX9A_VENDOR}/stm32f1xx-hal-driver/Src/stm32f1xx_ll_gpio.c
        drivers/fx9a_dma_ll.c
        platform/radio_main.c
        platform/device_contracts.c
        ${FX9A_ROOT}/port/bsp_delay.c
        ${FX9A_ROOT}/port/legacy_clock.c
        ${FX9A_ROOT}/port/layout_checks.c
        ${FX9A_BSP_SOURCES}
    )
    target_include_directories(fx9a_full.elf PRIVATE
        ${FX9A_ST_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/board
        ${FX9A_ROOT}/port
        ${FX9A_ROOT}/RadioStation/BSP
    )
    target_compile_definitions(fx9a_full.elf PRIVATE ${FX9A_ST_DEFINES} USE_FULL_LL_DRIVER)
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
