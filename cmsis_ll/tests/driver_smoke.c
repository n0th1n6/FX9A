/* Host-side software smoke test for FX-9A STM32F1 LL driver adapters.
 * Linux/x86-64 only. Map anonymous pages at the MCU peripheral address
 * range; never accesses radio hardware. Checks register programming, not
 * analog behavior or physical clock timing.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include "fx9a_legacy_types.h"

static void map_device_space(void)
{
    const uintptr_t base = 0x40000000u;
    const size_t bytes = 0x30000u;
    void *p = mmap((void *)base, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(p == (void *)base && "unable to map simulated STM32 peripheral space");
}

int main(void)
{
    map_device_space();

    /* 16 MHz HSE / 2 * 9 = 72 MHz, AHB /1, APB1 /2, APB2 /1. */
    RCC->CFGR = (2u << 2) | (4u << 8) | (1u << 16) |
                (1u << 17) | (7u << 18);
    RCC_ClocksTypeDef clocks = {0};
    RCC_GetClocksFreq(&clocks);
    assert(clocks.SYSCLK_Frequency == 72000000u);
    assert(clocks.HCLK_Frequency == 72000000u);
    assert(clocks.PCLK1_Frequency == 36000000u);
    assert(clocks.PCLK2_Frequency == 72000000u);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    assert(RCC->APB2ENR & RCC_APB2Periph_GPIOC);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE);
    assert(!(RCC->APB2ENR & RCC_APB2Periph_GPIOC));

    GPIO_InitTypeDef pin = {
        .GPIO_Pin = GPIO_Pin_5,
        .GPIO_Speed = GPIO_Speed_2MHz,
        .GPIO_Mode = GPIO_Mode_Out_PP,
    };
    GPIO_Init(GPIOC, &pin);
    assert(((GPIOC->CRL >> 20) & 0xfu) == 0x2u);
    GPIO_SetBits(GPIOC, GPIO_Pin_5);
    assert(GPIOC->BSRR == GPIO_Pin_5);
    GPIO_ResetBits(GPIOC, GPIO_Pin_5);
    assert(GPIOC->BRR == GPIO_Pin_5);
    GPIOC->IDR = GPIO_Pin_5;
    assert(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5) == Bit_SET);
    assert(GPIO_ReadInputData(GPIOC) == GPIO_Pin_5);

    TIM_TimeBaseInitTypeDef base = {
        .TIM_Prescaler = 71u,
        .TIM_CounterMode = TIM_CounterMode_Up,
        .TIM_Period = 999u,
        .TIM_ClockDivision = TIM_CKD_DIV1,
        .TIM_RepetitionCounter = 0u,
    };
    TIM_TimeBaseInit(TIM2, &base);
    assert(TIM2->PSC == 71u && TIM2->ARR == 999u);
    TIM_Cmd(TIM2, ENABLE);
    assert(TIM2->CR1 & 1u);
    TIM_Cmd(TIM2, DISABLE);
    assert(!(TIM2->CR1 & 1u));

    TIM_OCInitTypeDef output;
    TIM_OCStructInit(&output);
    output.TIM_OCMode = TIM_OCMode_PWM1;
    output.TIM_Pulse = 500;
    output.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC4Init(TIM3, &output);
    assert((TIM3->CCMR2 & (7u << 12)) == (6u << 12));
    assert(TIM3->CCR4 == 500u);
    assert(TIM3->CCER & (1u << 12));

    DMA_InitTypeDef dma = {0};
    dma.DMA_PeripheralBaseAddr = (uint32_t)(uintptr_t)&ADC1->DR;
    dma.DMA_MemoryBaseAddr = 0x20000100u;
    dma.DMA_BufferSize = 16u;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_High;
    DMA_Init(DMA1_Channel1, &dma);
    assert(DMA1_Channel1->CPAR == (uint32_t)(uintptr_t)&ADC1->DR);
    assert(DMA1_Channel1->CNDTR == 16u);
    assert(DMA1_Channel1->CCR & (1u << 5)); /* circular */
    assert(DMA1_Channel1->CCR & (1u << 7)); /* MINC */
    DMA_Cmd(DMA1_Channel1, ENABLE);
    assert(DMA1_Channel1->CCR & 1u);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    assert(!(DMA1_Channel1->CCR & 1u));

    ADC_InitTypeDef adc = {
        .ADC_Mode = ADC_Mode_Independent,
        .ADC_ScanConvMode = ENABLE,
        .ADC_ContinuousConvMode = ENABLE,
        .ADC_ExternalTrigConv = ADC_ExternalTrigConv_None,
        .ADC_DataAlign = ADC_DataAlign_Right,
        .ADC_NbrOfChannel = 4,
    };
    ADC_Init(ADC1, &adc);
    assert(ADC1->CR1 & (1u << 8)); /* scan */
    assert(ADC1->CR2 & (1u << 1)); /* continuous */
    assert(((ADC1->SQR1 >> 20) & 0xfu) == 3u);
    for (uint8_t rank = 1; rank <= 4; rank++)
        ADC_RegularChannelConfig(ADC1, rank + 9u, rank, ADC_SampleTime_239Cycles5);
    assert((ADC1->SQR3 & 0xfffffu) ==
        (10u | (11u << 5) | (12u << 10) | (13u << 15)));
    assert((ADC1->SMPR1 & 0xfffu) == 0xfffu);
    ADC_DMACmd(ADC1, ENABLE);
    assert(ADC1->CR2 & (1u << 8));
    ADC_Cmd(ADC1, ENABLE);
    assert(ADC1->CR2 & 1u);

    USART_InitTypeDef uart = {
        .USART_BaudRate = 115200u,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
    };
    USART_Init(USART1, &uart);
    assert(USART1->BRR == 625u);
    assert((USART1->CR1 & 0xcu) == 0xcu);
    USART_Cmd(USART1, ENABLE);
    assert(USART1->CR1 & (1u << 13));
    USART_Cmd(USART1, DISABLE);
    assert(!(USART1->CR1 & (1u << 13)));

    puts("PASS: RCC 72MHz model, GPIOC5, TIM2/3, DMA1, ADC1 and USART1 register programming");
    puts("CAUTION: host-simulated register writes only; not a hardware/RF validation");
    return 0;
}
