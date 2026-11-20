/* FX-9A STM32F1 EXTI bridge using official ST LL control.
 * Separated from NVIC code so register mapping can be host tested.
 */
#include "fx9a_legacy_types.h"
#include "stm32f1xx_ll_exti.h"

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
