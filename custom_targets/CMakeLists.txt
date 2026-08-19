add_library(mbed-fx9a INTERFACE)

target_sources(mbed-fx9a
    INTERFACE
        PeripheralPins.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../mbed-os/targets/TARGET_STM/TARGET_STM32F1/TARGET_STM32F103xE/system_clock.c
)

target_include_directories(mbed-fx9a
    INTERFACE
        .
)

target_link_libraries(mbed-fx9a INTERFACE mbed-stm32f103xc)
