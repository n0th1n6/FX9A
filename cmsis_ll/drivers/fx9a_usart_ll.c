/* STM32F1 USART register-level compatibility implementation.
 * Only USART1 is used by FX-9A CAT at 115200 8N1 with DMA1 CH4 TX.
 * Other USARTs are supported by clock lookup, not used by this board.
 */
#include "fx9a_legacy_types.h"
#include "stm32f1xx_ll_usart.h"
#include "stm32f10x_rcc.h"

void USART_DeInit(USART_TypeDef *u)
{
    if (u == USART1) {
        RCC->APB2RSTR |= 1u << 14;
        RCC->APB2RSTR &= ~(1u << 14);
    } else {
        const uint32_t mask = u == USART2 ? (1u << 17) :
                              u == USART3 ? (1u << 18) : 0u;
        if (mask) {
            RCC->APB1RSTR |= mask;
            RCC->APB1RSTR &= ~mask;
        }
    }
}
void USART_ClockInit(USART_TypeDef *u, USART_ClockInitTypeDef *cfg)
{
    uint32_t cr2 = u->CR2;
    cr2 &= ~((1u << 11) | (1u << 10) | (1u << 9) | (1u << 8));
    cr2 |= cfg->USART_Clock | cfg->USART_CPOL |
           cfg->USART_CPHA | cfg->USART_LastBit;
    u->CR2 = cr2;
}
void USART_Init(USART_TypeDef *u, USART_InitTypeDef *cfg)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    const uint32_t bus_clock = u == USART1 ?
                               clocks.PCLK2_Frequency : clocks.PCLK1_Frequency;
    const uint32_t baud = cfg->USART_BaudRate;
    /* STM32F1 USART oversamples by 16. The BRR integer representation
     * equals PCLK/baud rounded to nearest. */
    if (baud != 0u)
        u->BRR = (bus_clock + baud / 2u) / baud;
    u->CR2 = (u->CR2 & ~(3u << 12)) | cfg->USART_StopBits;
    u->CR3 = (u->CR3 & ~((1u << 8) | (1u << 9))) |
             cfg->USART_HardwareFlowControl;
    u->CR1 = (u->CR1 & ~((1u << 12) | (3u << 9) | (3u << 2))) |
             cfg->USART_WordLength | cfg->USART_Parity | cfg->USART_Mode;
}
void USART_ClearFlag(USART_TypeDef *u, uint16_t flags)
{
    /* TC and several error flags are clear-on-zero; RXNE is cleared by
     * SR/DR sequence or reading DR, not by a status-register write alone.
     */
    if (flags & USART_FLAG_RXNE) {
        volatile uint32_t sr = u->SR;
        volatile uint32_t dr = u->DR;
        (void)sr; (void)dr;
    }
    u->SR &= ~((uint32_t)flags & (1u << 6));
}
void USART_ITConfig(USART_TypeDef *u, uint16_t it, FunctionalState enabled)
{
    /* RXNE interrupt is the only enabled USART interrupt in the FX-9A BSP. */
    if (it == USART_IT_RXNE) {
        if (enabled == ENABLE) u->CR1 |= 1u << 5;
        else u->CR1 &= ~(1u << 5);
    }
}
void USART_DMACmd(USART_TypeDef *u, uint16_t mask, FunctionalState enabled)
{
    if (enabled == ENABLE) u->CR3 |= mask;
    else u->CR3 &= ~((uint32_t)mask);
}
void USART_Cmd(USART_TypeDef *u, FunctionalState enabled)
{
    if (enabled == ENABLE) LL_USART_Enable(u);
    else LL_USART_Disable(u);
}
ITStatus USART_GetITStatus(USART_TypeDef *u, uint16_t it)
{
    if (it == USART_IT_RXNE)
        return (u->SR & (1u << 5)) && (u->CR1 & (1u << 5)) ? SET : RESET;
    return RESET;
}
uint16_t USART_ReceiveData(USART_TypeDef *u)
{
    return (uint16_t)(u->DR & 0x1ffu);
}
void USART_ClearITPendingBit(USART_TypeDef *u, uint16_t it)
{
    if (it == USART_IT_RXNE) {
        volatile uint32_t sr = u->SR;
        volatile uint32_t dr = u->DR;
        (void)sr; (void)dr;
    }
}
