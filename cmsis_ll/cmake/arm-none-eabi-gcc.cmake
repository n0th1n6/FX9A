set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(FX9A_CPU_FLAGS
    "-mcpu=cortex-m3 -mthumb"
    CACHE STRING "CPU flags for the FX-9A STM32F103"
)

set(CMAKE_C_FLAGS_INIT "${FX9A_CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${FX9A_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${FX9A_CPU_FLAGS} -Wl,--gc-sections")
