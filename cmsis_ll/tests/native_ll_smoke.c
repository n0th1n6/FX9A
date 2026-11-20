/* Host-simulated register checks for the native STM32F1 LL application.
 * Map low-level device registers at their reference-manual addresses.
 * Single-threaded x86 test only; no real peripherals or RF are operated.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include "stm32f1xx.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_tim.h"
#include "stm32f1xx_ll_dma.h"
#include "stm32f1xx_ll_usart.h"
#include "stm32f1xx_ll_exti.h"
#include "ll_pins.h"

static void map_peripherals(void)
{
    void *p = mmap((void *)0x40000000u, 0x30000u, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(p == (void *)0x40000000u);
}
int main(void)
{
    map_peripherals();
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC);
    assert(RCC->APB2ENR & RCC_APB2ENR_IOPCEN);

    /* The native board helper must encode both low and high GPIO bits
     * for ST LL's nonstandard STM32F1 Pin representation.
     */
    LL_GPIO_InitTypeDef cfg = {0};
    cfg.Pin = fx9a_ll_pins(GPIO_BSRR_BS5 | GPIO_BSRR_BS9);
    cfg.Mode = LL_GPIO_MODE_OUTPUT;
    cfg.Speed = LL_GPIO_SPEED_FREQ_LOW;
    cfg.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    cfg.Pull = LL_GPIO_PULL_DOWN;
    assert(LL_GPIO_Init(GPIOC, &cfg) == SUCCESS);
    assert(((GPIOC->CRL >> 20) & 0xfu) == 2u);
    assert(((GPIOC->CRH >> 4) & 0xfu) == 2u);
    GPIOC->BSRR = GPIO_BSRR_BS5;
    assert(GPIOC->BSRR == (1u << 5));

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    LL_TIM_SetPrescaler(TIM3, 3u);
    LL_TIM_SetAutoReload(TIM3, 2249u);
    LL_TIM_OC_SetMode(TIM3, LL_TIM_CHANNEL_CH4, LL_TIM_OCMODE_PWM1);
    LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
    assert(TIM3->PSC == 3u && TIM3->ARR == 2249u);
    assert((TIM3->CCMR2 & (7u << 12)) == (6u << 12));
    assert(TIM3->CCER & TIM_CCER_CC4E);

    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1,
                                    LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 16u);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
    assert(DMA1_Channel1->CNDTR == 16u);
    assert(DMA1_Channel1->CCR & DMA_CCR_CIRC);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    assert(DMA1_Channel1->CCR & DMA_CCR_EN);

    /* Direct ADC configuration in the native FX-9A application. */
    ADC1->SQR1 = 3u << 20;
    ADC1->SQR3 = 10u | 11u << 5 | 12u << 10 | 13u << 15;
    assert(((ADC1->SQR1 >> 20) & 0xfu) == 3u);
    assert((ADC1->SQR3 & 0x1fu) == 10u);

    LL_USART_SetBaudRate(USART1, 72000000u, 115200u);
    assert(USART1->BRR == 625u);
    LL_USART_Enable(USART1);
    assert(USART1->CR1 & USART_CR1_UE);

    LL_EXTI_EnableIT_0_31(1u << 2);
    LL_EXTI_EnableFallingTrig_0_31(1u << 2);
    assert(EXTI->IMR & (1u << 2));
    assert(EXTI->FTSR & (1u << 2));

    puts("PASS: native STM32F1 LL GPIO high/low masks, TIM3 PWM, DMA1, ADC, USART1, EXTI");
    puts("NOTE: host-register simulation only; hardware clock/RF behavior is unverified");
    return 0;
}
