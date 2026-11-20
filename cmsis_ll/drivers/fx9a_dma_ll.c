/* STM32F1 DMA register-level compatibility implementation.
 * Channel 1: ADC sample ring, Channel 4: USART1 TX.
 */
#include "stm32f10x_dma.h"
#include "stm32f1xx_ll_dma.h"

static uint32_t fx9a_channel_index(DMA_Channel_TypeDef *channel)
{
    if (channel == DMA1_Channel1) return 0u;
    if (channel == DMA1_Channel2) return 1u;
    if (channel == DMA1_Channel3) return 2u;
    if (channel == DMA1_Channel4) return 3u;
    if (channel == DMA1_Channel5) return 4u;
    if (channel == DMA1_Channel6) return 5u;
    if (channel == DMA1_Channel7) return 6u;
    return 0u;
}
void DMA_DeInit(DMA_Channel_TypeDef *channel)
{
    const uint32_t shift = fx9a_channel_index(channel) * 4u;
    channel->CCR = 0u;
    channel->CNDTR = 0u;
    channel->CPAR = 0u;
    channel->CMAR = 0u;
    DMA1->IFCR = 0xfu << shift;
}
void DMA_Init(DMA_Channel_TypeDef *channel, DMA_InitTypeDef *cfg)
{
    channel->CCR = 0u;
    channel->CPAR = cfg->DMA_PeripheralBaseAddr;
    channel->CMAR = cfg->DMA_MemoryBaseAddr;
    channel->CNDTR = cfg->DMA_BufferSize;
    /* StdPeriph DMA configuration constants map to the STM32F1 CCR bits. */
    channel->CCR = cfg->DMA_DIR | cfg->DMA_PeripheralInc |
        cfg->DMA_MemoryInc | cfg->DMA_PeripheralDataSize |
        cfg->DMA_MemoryDataSize | cfg->DMA_Mode |
        cfg->DMA_Priority | cfg->DMA_M2M;
}
void DMA_Cmd(DMA_Channel_TypeDef *channel, FunctionalState enabled)
{
    if (enabled == ENABLE)
        LL_DMA_EnableChannel(DMA1, fx9a_channel_index(channel) + 1u);
    else
        LL_DMA_DisableChannel(DMA1, fx9a_channel_index(channel) + 1u);
}
void DMA_ITConfig(DMA_Channel_TypeDef *channel, uint32_t interrupt,
                  FunctionalState enabled)
{
    if (enabled == ENABLE) channel->CCR |= interrupt;
    else channel->CCR &= ~interrupt;
}
void DMA_ClearFlag(uint32_t flags)
{
    DMA1->IFCR = flags;
}
