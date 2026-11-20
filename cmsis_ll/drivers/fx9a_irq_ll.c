/* STM32F1 NVIC/EXTI register-level layer with legacy caller signatures.
 * Only the APIs needed by the FX-9A radio are implemented.
 */
#include "fx9a_legacy_types.h"
#include "stm32f1xx_ll_exti.h"


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

void EXTI_Init(EXTI_InitTypeDef *cfg)
{
    const uint32_t bits = cfg->EXTI_Line;
    if (cfg->EXTI_LineCmd == DISABLE) {
        if (cfg->EXTI_Mode == EXTI_Mode_Interrupt) LL_EXTI_DisableIT_0_31(bits);
        else EXTI->EMR &= ~bits;
        return;
    }
    EXTI->IMR &= ~bits;
    EXTI->EMR &= ~bits;
    if (cfg->EXTI_Mode == EXTI_Mode_Interrupt) LL_EXTI_EnableIT_0_31(bits);
    else EXTI->EMR |= bits;
    EXTI->RTSR &= ~bits;
    EXTI->FTSR &= ~bits;
    if (cfg->EXTI_Trigger == EXTI_Trigger_Rising ||
        cfg->EXTI_Trigger == EXTI_Trigger_Rising_Falling) EXTI->RTSR |= bits;
    if (cfg->EXTI_Trigger == EXTI_Trigger_Falling ||
        cfg->EXTI_Trigger == EXTI_Trigger_Rising_Falling) EXTI->FTSR |= bits;
}

ITStatus EXTI_GetITStatus(uint32_t line)
{
    return ((EXTI->PR & EXTI->IMR & line) != 0u) ? SET : RESET;
}
void EXTI_ClearITPendingBit(uint32_t line)
{
    LL_EXTI_ClearFlag_0_31(line); /* Write-one-to-clear. */
}
