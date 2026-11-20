/* Standalone STM32F1 RCC register-level compatibility driver.
 * The legacy RCC implementation is not linked in the independent target.
 * Keeps old BSP signatures while replacing hardware access with a small
 * low-layer based on the actual RCC registers.
 */
#include "stm32f10x_rcc.h"

void RCC_AHBPeriphClockCmd(uint32_t mask, FunctionalState state)
{
    if (state == ENABLE) RCC->AHBENR |= mask;
    else RCC->AHBENR &= ~mask;
}
void RCC_APB1PeriphClockCmd(uint32_t mask, FunctionalState state)
{
    if (state == ENABLE) RCC->APB1ENR |= mask;
    else RCC->APB1ENR &= ~mask;
}
void RCC_APB2PeriphClockCmd(uint32_t mask, FunctionalState state)
{
    if (state == ENABLE) RCC->APB2ENR |= mask;
    else RCC->APB2ENR &= ~mask;
}
void RCC_APB1PeriphResetCmd(uint32_t mask, FunctionalState state)
{
    if (state == ENABLE) RCC->APB1RSTR |= mask;
    else RCC->APB1RSTR &= ~mask;
}
void RCC_APB2PeriphResetCmd(uint32_t mask, FunctionalState state)
{
    if (state == ENABLE) RCC->APB2RSTR |= mask;
    else RCC->APB2RSTR &= ~mask;
}
void RCC_GetClocksFreq(RCC_ClocksTypeDef *clocks)
{
    static const uint8_t ahb_shift[16] = {
        0,0,0,0,0,0,0,0,1,2,3,4,6,7,8,9
    };
    static const uint8_t apb_shift[8] = {0,0,0,0,1,2,3,4};
    const uint32_t cfgr = RCC->CFGR;
    uint32_t sysclock;
    switch ((cfgr >> 2) & 3u) {
        case 0: sysclock = 8000000u; break; /* HSI */
        case 1: sysclock = HSE_Value; break; /* HSE */
        case 2: {
            uint32_t input = (cfgr & (1u << 16)) ? HSE_Value : 4000000u;
            if (cfgr & (1u << 17)) input /= 2u;
            uint32_t mul = ((cfgr >> 18) & 0xfu) + 2u;
            if (mul > 16u) mul = 16u;
            sysclock = input * mul;
            break;
        }
        default: sysclock = 8000000u; break;
    }
    clocks->SYSCLK_Frequency = sysclock;
    clocks->HCLK_Frequency = sysclock >> ahb_shift[(cfgr >> 4) & 0xfu];
    clocks->PCLK1_Frequency = clocks->HCLK_Frequency >>
        apb_shift[(cfgr >> 8) & 7u];
    clocks->PCLK2_Frequency = clocks->HCLK_Frequency >>
        apb_shift[(cfgr >> 11) & 7u];
    /* ADC prescaler: ADCPRE 00=/2, 01=/4, 10=/6, 11=/8 */
    clocks->ADCCLK_Frequency = clocks->PCLK2_Frequency /
        (2u * (((cfgr >> 14) & 3u) + 1u));
}
