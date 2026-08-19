#ifndef FX9A_MBED_STM32F10X_CONF_H
#define FX9A_MBED_STM32F10X_CONF_H

#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#define assert_param(expr) ((void)0)

/* TIM4 backs Mbed CE's microsecond ticker on STM32F103. The original keyer
 * timer is internal-only, so redirect its legacy names to free-running TIM5. */
#undef TIM4
#define TIM4 TIM5
#define TIM4_IRQn TIM5_IRQn
#define TIM4_IRQHandler TIM5_IRQHandler
#undef RCC_APB1Periph_TIM4
#define RCC_APB1Periph_TIM4 RCC_APB1Periph_TIM5

/* Avoid collisions with identically named Mbed HAL entry points. */
#define gpio_init fx9a_gpio_init
#define i2c_init fx9a_i2c_init
#define i2c_reset fx9a_i2c_reset
#define i2c_start fx9a_i2c_start
#define i2c_stop fx9a_i2c_stop

#endif
