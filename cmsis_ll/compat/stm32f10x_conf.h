#ifndef FX9A_STANDALONE_STM32F10X_CONF_H
#define FX9A_STANDALONE_STM32F10X_CONF_H

/* Temporary type/constant declarations while the BSP is migrated to STM32F1
 * LL. These headers must not supply compiled old StdPeriph GPIO/RCC/EXTI/
 * NVIC implementations.
 *
 * No Mbed compatibility remapping: the original CW keyer uses TIM4, and
 * TIM5 is free. gpio_* / i2c_* names also remain unchanged.
 */
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#define assert_param(expr) ((void)0)

#endif
