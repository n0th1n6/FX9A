/* STM32F1 CMSIS NVIC layer with legacy caller signatures.
 * Only the APIs needed by the FX-9A radio are implemented.
 */
#include "fx9a_legacy_types.h"



void NVIC_PriorityGroupConfig(uint32_t group)
{
    /* Preserve unrelated AIRCR bits; write mandatory VECTKEY (0x5FA). */
    SCB->AIRCR = (0x5fau << 16) | (SCB->AIRCR & 0xffffu & ~(7u << 8))
               | (group & (7u << 8));
}

void NVIC_Init(NVIC_InitTypeDef *cfg)
{
    const IRQn_Type irq = (IRQn_Type)cfg->NVIC_IRQChannel;
    if (cfg->NVIC_IRQChannelCmd != ENABLE) {
        NVIC_DisableIRQ(irq);
        return;
    }
    /* Old project uses PriorityGroup_2: two preemption bits + two subpriority
     * bits, compatible with the encoded CMSIS NVIC group value.
     */
    const uint32_t group = (SCB->AIRCR >> 8) & 7u;
    const uint32_t priority = NVIC_EncodePriority(group,
        cfg->NVIC_IRQChannelPreemptionPriority,
        cfg->NVIC_IRQChannelSubPriority);
    NVIC_SetPriority(irq, priority);
    NVIC_EnableIRQ(irq);
}

