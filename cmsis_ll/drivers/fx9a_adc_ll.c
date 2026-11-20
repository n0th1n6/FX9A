/* STM32F1 ADC register-level compatibility implementation.
 * ADC1 scans PC0..PC3 (channels 10..13) with DMA1 CH1 circular sampling.
 * The existing board's ADC_Value order and conversion behavior are retained.
 */
#include "stm32f10x_adc.h"

void ADC_DeInit(ADC_TypeDef *adc)
{
    if (adc == ADC1) {
        RCC->APB2RSTR |= 1u << 9;
        RCC->APB2RSTR &= ~(1u << 9);
    } else if (adc == ADC2) {
        RCC->APB2RSTR |= 1u << 10;
        RCC->APB2RSTR &= ~(1u << 10);
    }
}

void ADC_Init(ADC_TypeDef *adc, ADC_InitTypeDef *cfg)
{
    uint32_t cr1 = adc->CR1;
    cr1 &= ~((0xfu << 16) | (1u << 8));
    cr1 |= cfg->ADC_Mode & (0xfu << 16);
    if (cfg->ADC_ScanConvMode == ENABLE) cr1 |= 1u << 8;
    adc->CR1 = cr1;

    uint32_t cr2 = adc->CR2;
    cr2 &= ~((1u << 1) | (1u << 11) | (7u << 17));
    if (cfg->ADC_ContinuousConvMode == ENABLE) cr2 |= 1u << 1;
    cr2 |= cfg->ADC_DataAlign & (1u << 11);
    cr2 |= cfg->ADC_ExternalTrigConv & (7u << 17);
    adc->CR2 = cr2;
    adc->SQR1 = (adc->SQR1 & ~(0xfu << 20)) |
        (((uint32_t)cfg->ADC_NbrOfChannel - 1u) << 20);
}

void ADC_RegularChannelConfig(ADC_TypeDef *adc, uint8_t channel,
                              uint8_t rank, uint8_t sample)
{
    if (channel >= 10) {
        uint32_t shift = (uint32_t)(channel - 10u) * 3u;
        adc->SMPR1 = (adc->SMPR1 & ~(7u << shift)) |
                     (((uint32_t)sample & 7u) << shift);
    } else {
        uint32_t shift = (uint32_t)channel * 3u;
        adc->SMPR2 = (adc->SMPR2 & ~(7u << shift)) |
                     (((uint32_t)sample & 7u) << shift);
    }
    if (rank == 0 || rank > 16) return;
    uint32_t shift = ((uint32_t)(rank - 1u) % 6u) * 5u;
    volatile uint32_t *sqr =
        rank <= 6 ? &adc->SQR3 : (rank <= 12 ? &adc->SQR2 : &adc->SQR1);
    *sqr = (*sqr & ~(0x1fu << shift)) |
           (((uint32_t)channel & 0x1fu) << shift);
}

void ADC_DMACmd(ADC_TypeDef *adc, FunctionalState enabled)
{
    if (enabled == ENABLE) adc->CR2 |= 1u << 8;
    else adc->CR2 &= ~(1u << 8);
}
void ADC_Cmd(ADC_TypeDef *adc, FunctionalState enabled)
{
    if (enabled == ENABLE) adc->CR2 |= 1u;
    else adc->CR2 &= ~1u;
}
void ADC_ResetCalibration(ADC_TypeDef *adc)
{
    adc->CR2 |= 1u << 3;
}
FlagStatus ADC_GetResetCalibrationStatus(ADC_TypeDef *adc)
{
    return (adc->CR2 & (1u << 3)) ? SET : RESET;
}
void ADC_StartCalibration(ADC_TypeDef *adc)
{
    adc->CR2 |= 1u << 2;
}
FlagStatus ADC_GetCalibrationStatus(ADC_TypeDef *adc)
{
    return (adc->CR2 & (1u << 2)) ? SET : RESET;
}
void ADC_SoftwareStartConvCmd(ADC_TypeDef *adc, FunctionalState enabled)
{
    if (enabled == ENABLE) adc->CR2 |= (1u << 20) | (1u << 22);
    else adc->CR2 &= ~(1u << 22);
}
void ADC_ITConfig(ADC_TypeDef *adc, uint16_t interrupt, FunctionalState enabled)
{
    const uint32_t bit = (uint32_t)interrupt & 0xffu;
    if (enabled == ENABLE) adc->CR1 |= bit;
    else adc->CR1 &= ~bit;
}
ITStatus ADC_GetITStatus(ADC_TypeDef *adc, uint16_t interrupt)
{
    const uint32_t status_bit = ((uint32_t)interrupt >> 8) & 0xffu;
    return ((adc->SR & status_bit) && (adc->CR1 & (interrupt & 0xffu)))
        ? SET : RESET;
}
uint16_t ADC_GetConversionValue(ADC_TypeDef *adc)
{
    return (uint16_t)adc->DR;
}
void ADC_ClearITPendingBit(ADC_TypeDef *adc, uint16_t interrupt)
{
    adc->SR &= ~(((uint32_t)interrupt >> 8) & 0xffu);
}
