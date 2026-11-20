/* Compile-time STM32F103 hardware layout and radio EEPROM contracts.
 * No legacy STM32 Standard Peripheral Library types or API aliases.
 */
#include "stm32f1xx.h"
#include "bsp_board.h"
#include <stddef.h>
#ifndef STM32F103xE
#error "FX-9A requires STM32F103 high-density device headers"
#endif
#if HSE_VALUE != 16000000
#error "FX-9A uses 16MHz external crystal"
#endif

_Static_assert((int)DMA1_Channel4_IRQn == 14, "Wrong DMA1 CH4 IRQ vector");
_Static_assert((int)USART1_IRQn == 37, "Wrong USART1 IRQ vector");
_Static_assert((int)TIM2_IRQn == 28, "Wrong TIM2 IRQ vector");
_Static_assert((int)TIM3_IRQn == 29, "Wrong TIM3 IRQ vector");
_Static_assert((int)TIM4_IRQn == 30, "Wrong TIM4 IRQ vector");
_Static_assert((int)EXTI2_IRQn == 8, "Wrong EXTI2 IRQ vector");

_Static_assert(GPIO_BSRR_BS5 == (1u << 5), "PC5 power-hold mask mismatch");
_Static_assert(offsetof(GPIO_TypeDef, BSRR) == 0x10, "GPIO BSRR offset mismatch");
_Static_assert(offsetof(DMA_Channel_TypeDef, CCR) == 0x00, "DMA CCR offset mismatch");
_Static_assert(offsetof(USART_TypeDef, BRR) == 0x08, "USART BRR offset mismatch");
_Static_assert(offsetof(ADC_TypeDef, DR) == 0x4c, "ADC DR offset mismatch");

/* The EEPROM binary layout must not change when renaming radio mode enums. */
_Static_assert(RADIO_USB == 2, "Persisted upper-sideband mode value changed");
_Static_assert(sizeof(VFO_SaveInfo) == 5, "VFO EEPROM record size changed");
_Static_assert(sizeof(MEM_AttrDef) == 6, "Memory EEPROM record size changed");
_Static_assert(sizeof(MENU_AttrDef) == 44, "Menu EEPROM layout changed");
