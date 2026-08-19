cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.31)

set(MBED_APP_JSON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed_app.json5)
set(CUSTOM_TARGETS_JSON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/custom_targets.json5)

include(mbed-os/tools/cmake/mbed_toolchain_setup.cmake)

project(FX9A_Mbed_Port LANGUAGES C CXX ASM)

include(mbed_project_setup)

add_subdirectory(custom_targets)
add_subdirectory(mbed-os)

# The legacy USART owns DMA1 Channel 4 directly. Rename only Mbed's generic
# DMA implementation symbol; the startup vector must retain the original name.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os/targets/TARGET_STM/stm_dma_utils.c
    TARGET_DIRECTORY mbed-baremetal
    PROPERTIES COMPILE_DEFINITIONS
        DMA1_Channel4_IRQHandler=mbed_DMA1_Channel4_IRQHandler
)

# IAR's pragma immediately before IntDelay() disables optimization for that
# debounce function. GCC ignores the pragma, so supply the equivalent function
# attribute through a source-specific forced include.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/RadioStation/BSP/bsp_key.c
    PROPERTIES COMPILE_OPTIONS
        "-include;${CMAKE_CURRENT_SOURCE_DIR}/port/iar_compat.h;-Wno-unknown-pragmas"
)

set(FX9A_BSP_SOURCES
    RadioStation/BSP/bsp_24lc16.c
    RadioStation/BSP/bsp_ad9834.c
    RadioStation/BSP/bsp_ad9951.c
    RadioStation/BSP/bsp_adc.c
    RadioStation/BSP/bsp_baud.c
    RadioStation/BSP/bsp_board.c
    RadioStation/BSP/bsp_dac.c
    RadioStation/BSP/bsp_debug.c
    RadioStation/BSP/bsp_iambic_keyer.c
    RadioStation/BSP/bsp_kenwood.c
    RadioStation/BSP/bsp_key.c
    RadioStation/BSP/bsp_oled.c
    RadioStation/BSP/bsp_oled_api.c
    RadioStation/BSP/bsp_pwm.c
    RadioStation/BSP/bsp_receive_att.c
    RadioStation/BSP/bsp_timer.c
    RadioStation/BSP/bsp_transmit_power.c
    RadioStation/BSP/bsp_usart.c
    RadioStation/BSP/bsp_vol.c
    RadioStation/BSP/i2c.c
)

set(FX9A_STDPERIPH_SOURCES
    RadioStation/StdPeriph_Driver/misc.c
    RadioStation/StdPeriph_Driver/stm32f10x_adc.c
    RadioStation/StdPeriph_Driver/stm32f10x_dma.c
    RadioStation/StdPeriph_Driver/stm32f10x_exti.c
    RadioStation/StdPeriph_Driver/stm32f10x_gpio.c
    RadioStation/StdPeriph_Driver/stm32f10x_rcc.c
    RadioStation/StdPeriph_Driver/stm32f10x_tim.c
    RadioStation/StdPeriph_Driver/stm32f10x_usart.c
)

add_executable(fx9a
    port/mbed_main.c
    port/bsp_delay.c
    port/legacy_clock.c
    port/layout_checks.c
    ${FX9A_BSP_SOURCES}
    ${FX9A_STDPERIPH_SOURCES}
)
target_include_directories(fx9a PRIVATE
    port
    RadioStation/BSP
    RadioStation/CMSIS
    RadioStation/StdPeriph_Driver
)
target_compile_definitions(fx9a PRIVATE
    STM32F10X_HD
    USE_STDPERIPH_DRIVER
)
target_compile_options(fx9a PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-deprecated-declarations;-fshort-enums;-fsigned-char>
)
target_link_libraries(fx9a mbed-baremetal)
mbed_set_post_build(fx9a)

add_custom_command(
    TARGET fx9a POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DELF_FILE=$<TARGET_FILE:fx9a>
        -DNM_TOOL=${CMAKE_NM}
        -DOBJDUMP_TOOL=${CMAKE_OBJDUMP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_image.cmake
    COMMENT "Verify FX9A vector, interrupt ownership, and calibrated delay"
    VERBATIM
)
