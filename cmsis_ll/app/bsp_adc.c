#include "bsp_adc.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_adc.h"
#include "stm32f1xx_ll_dma.h"

/* PC0 supply, PC1 reflected power, PC2 forward power, PC3 receive level.
 * Preserve the original 4-channel x 4-sample DMA ring and meter indexing.
 */
volatile uint16_t ADC_Value[16]; /* Written by DMA, sampled asynchronously by TIM2 ISR */

void ADC_Config(void)
{
    /* ADC1 DMA1 Channel 1 circular transfers, 16-bit peripheral and memory. */
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)(uintptr_t)&ADC1->DR);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)(uintptr_t)ADC_Value);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 16u);
    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_HIGH);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    /* Reset ADC1 before configuring scan sequence. ADCCLK is 12 MHz
     * because platform/clock.c sets the /6 prescaler at 72 MHz SYSCLK.
     */
    LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_ADC1);
    LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_ADC1);

    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->CR2 = ADC_CR2_CONT | ADC_CR2_DMA | (7u << 17);
    ADC1->SQR1 = 3u << 20; /* 4 conversions */
    ADC1->SQR2 = 0u;
    ADC1->SQR3 = 10u | (11u << 5) | (12u << 10) | (13u << 15);
    ADC1->SMPR1 = (7u << 0) | (7u << 3) | (7u << 6) | (7u << 9);
    ADC1->CR2 |= ADC_CR2_ADON;

    /* Minimal ADC stabilization prior to calibration. Hardware validation
     * must confirm timing on the fitted STM32F103 variant.
     */
    for (volatile uint32_t i = 0; i < 2000u; ++i) { __NOP(); }
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL) {}
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL) {}
    ADC1->CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_SWSTART;
}

void adc_init(void)
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1 | LL_APB2_GRP1_PERIPH_GPIOC);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    /* PC0..PC3 analog input mode: CNF/MODE bits are all zero. */
    GPIOC->CRL &= ~0x0000ffffu;
    ADC_Config();
}

uint16_t adc_batt(void)
{
    return (ADC_Value[0] + ADC_Value[4] + ADC_Value[8] + ADC_Value[12]) / 4u;
}
uint16_t adc_rf(void)
{
    return (ADC_Value[1] + ADC_Value[5] + ADC_Value[9] + ADC_Value[13]) / 4u;
}
uint16_t adc_swr(void)
{
    return (ADC_Value[2] + ADC_Value[6] + ADC_Value[10] + ADC_Value[14]) / 4u;
}
uint16_t adc_rx(void)
{
    return (ADC_Value[3] + ADC_Value[7] + ADC_Value[11] + ADC_Value[15]) / 4u;
}
void adc_start(void)
{
    ADC1->CR2 |= ADC_CR2_EXTTRIG | ADC_CR2_SWSTART;
}
void ADC1_2_IRQHandler(void)
{
    /* Legacy interrupt path; normally the ADC samples via circular DMA. */
    if ((ADC1->SR & ADC_SR_EOC) && (ADC1->CR1 & ADC_CR1_EOCIE)) {
        DC.val = (uint16_t)ADC1->DR;
        DRMgr.item |= DCItem;
    }
}
