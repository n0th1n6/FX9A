/* Compile-time FX-9A STM32F1 / legacy-BSP ABI contract.
 * These checks do not establish hardware behavior.
 */
#include "fx9a_legacy_types.h"
#include <stddef.h>

#ifndef STM32F103xE
#error "FX-9A requires an STM32F103 high-density device declaration"
#endif
#if HSE_VALUE != 16000000
#error "FX-9A clock reference must be configured as 16 MHz HSE"
#endif

_Static_assert((int)DMA1_Channel4_IRQn == 14, "Wrong Cortex-M3 DMA vector");
_Static_assert((int)USART1_IRQn == 37, "Wrong USART1 vector");
_Static_assert((int)TIM2_IRQn == 28, "Wrong TIM2 vector");
_Static_assert((int)TIM3_IRQn == 29, "Wrong TIM3 vector");
_Static_assert((int)TIM4_IRQn == 30, "Wrong TIM4 vector");

_Static_assert(GPIO_Pin_5 == (1u << 5), "Power-hold pin mask mismatch");
_Static_assert(sizeof(GPIO_InitTypeDef) == 4, "GPIO ABI mismatch");
_Static_assert(offsetof(GPIO_InitTypeDef, GPIO_Mode) == 3, "GPIO mode offset mismatch");
_Static_assert(sizeof(NVIC_InitTypeDef) == 4, "NVIC ABI mismatch");
_Static_assert(sizeof(EXTI_InitTypeDef) == 8, "EXTI ABI mismatch");
_Static_assert(sizeof(RCC_ClocksTypeDef) == 20, "RCC clock struct mismatch");
_Static_assert(sizeof(TIM_TimeBaseInitTypeDef) == 10, "TIM base struct mismatch");
_Static_assert(sizeof(TIM_OCInitTypeDef) == 16, "TIM OC struct mismatch");
_Static_assert(sizeof(DMA_InitTypeDef) == 44, "DMA struct mismatch");
_Static_assert(sizeof(ADC_InitTypeDef) == 20, "ADC init struct mismatch");
_Static_assert(sizeof(USART_InitTypeDef) == 16, "USART struct mismatch");
_Static_assert(sizeof(USART_ClockInitTypeDef) == 8, "USART clock struct mismatch");

/* The modern ST device header must provide the reference manual register
 * offsets to the radio's power-hold and UART/DMA/ADC hardware.
 */
_Static_assert(offsetof(GPIO_TypeDef, BSRR) == 0x10, "GPIO BSRR offset mismatch");
_Static_assert(offsetof(DMA_Channel_TypeDef, CCR) == 0x00, "DMA CCR offset mismatch");
_Static_assert(offsetof(USART_TypeDef, BRR) == 0x08, "USART BRR offset mismatch");
_Static_assert(offsetof(ADC_TypeDef, DR) == 0x4C, "ADC DR offset mismatch");
