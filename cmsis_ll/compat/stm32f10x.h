#ifndef FX9A_STM32F10X_DEVICE_COMPAT_H
#define FX9A_STM32F10X_DEVICE_COMPAT_H

/* Temporary API bridge for legacy FX-9A BSP declarations.
 *
 * IMPORTANT: This is NOT the old 2009 STM32F10x device header.
 * STM32F1 peripheral types, IRQ numbers, bitfields, and CMSIS core
 * intrinsics now come from ST's pinned CMSIS STM32F1 device package.
 * The old StdPeriph *.h files are used only for application-facing
 * constants/structure declarations until BSP calls are converted.
 */
#include "stm32f1xx.h"
#include <stdint.h>

/* Legacy FX-9A uses USB as the upper-sideband mode enum name. The newer
 * device header defines USB as the USB peripheral pointer. No firmware code
 * uses the USB peripheral, so reserve this identifier for the radio mode. */
#ifdef USB
#undef USB
#endif

#ifndef STM32F10X_HD
#define STM32F10X_HD 1
#endif

#ifndef HSE_Value
#define HSE_Value HSE_VALUE
#endif
#ifndef HSI_Value
#define HSI_Value HSI_VALUE
#endif

/* Deprecated project shorthand retained solely for BSP compatibility. */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#endif
