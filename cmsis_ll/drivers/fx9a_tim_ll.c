/* FX-9A STM32F1 timer register-level driver.
 * Keeps only the legacy functions used by the application and removes
 * dependency on the compiled STM32 StdPeriph TIM driver in standalone builds.
 * Timer assignments: TIM2 input scan, TIM3 sidetone PWM, TIM4 CW keyer.
 */
#include "stm32f10x_tim.h"

void TIM_DeInit(TIM_TypeDef *timer)
{
    uint32_t mask = 0;
    if (timer == TIM2) mask = 1u << 0;
    else if (timer == TIM3) mask = 1u << 1;
    else if (timer == TIM4) mask = 1u << 2;
    else if (timer == TIM5) mask = 1u << 3;
    if (mask) {
        RCC->APB1RSTR |= mask;
        RCC->APB1RSTR &= ~mask;
    }
}

void TIM_TimeBaseInit(TIM_TypeDef *timer, TIM_TimeBaseInitTypeDef *cfg)
{
    uint32_t cr1 = timer->CR1;
    cr1 &= ~((3u << 8) | (1u << 4) | (3u << 5));
    cr1 |= ((uint32_t)cfg->TIM_ClockDivision & (3u << 8));
    cr1 |= ((uint32_t)cfg->TIM_CounterMode & ((1u << 4) | (3u << 5)));
    timer->CR1 = cr1;
    timer->ARR = cfg->TIM_Period;
    timer->PSC = cfg->TIM_Prescaler;
    if (timer == TIM1 || timer == TIM8)
        timer->RCR = cfg->TIM_RepetitionCounter;
    timer->EGR = 1u; /* Update and load new prescaler. */
}

void TIM_OCStructInit(TIM_OCInitTypeDef *cfg)
{
    cfg->TIM_OCMode = TIM_OCMode_Timing;
    cfg->TIM_OutputState = TIM_OutputState_Disable;
    cfg->TIM_OutputNState = TIM_OutputNState_Disable;
    cfg->TIM_Pulse = 0;
    cfg->TIM_OCPolarity = TIM_OCPolarity_High;
    cfg->TIM_OCNPolarity = TIM_OCNPolarity_High;
    cfg->TIM_OCIdleState = TIM_OCIdleState_Reset;
    cfg->TIM_OCNIdleState = TIM_OCNIdleState_Reset;
}

void TIM_OC4Init(TIM_TypeDef *timer, TIM_OCInitTypeDef *cfg)
{
    /* CCMR2 OC4M bits [14:12], output mode CC4S=00, CCER CC4E/CC4P. */
    timer->CCER &= ~(1u << 12);
    uint32_t ccmr2 = timer->CCMR2;
    ccmr2 &= ~((3u << 8) | (7u << 12));
    ccmr2 |= ((uint32_t)cfg->TIM_OCMode << 8) & (7u << 12);
    timer->CCMR2 = ccmr2;
    timer->CCR4 = cfg->TIM_Pulse;
    uint32_t ccer = timer->CCER;
    ccer &= ~((1u << 12) | (1u << 13));
    ccer |= (uint32_t)cfg->TIM_OutputState & (1u << 12);
    ccer |= ((uint32_t)cfg->TIM_OCPolarity << 12) & (1u << 13);
    timer->CCER = ccer;
}

void TIM_OC4PreloadConfig(TIM_TypeDef *timer, uint16_t enabled)
{
    if (enabled == TIM_OCPreload_Enable) timer->CCMR2 |= 1u << 11;
    else timer->CCMR2 &= ~(1u << 11);
}
void TIM_ARRPreloadConfig(TIM_TypeDef *timer, FunctionalState enabled)
{
    if (enabled == ENABLE) timer->CR1 |= 1u << 7;
    else timer->CR1 &= ~(1u << 7);
}
void TIM_ITConfig(TIM_TypeDef *timer, uint16_t mask, FunctionalState enabled)
{
    if (enabled == ENABLE) timer->DIER |= mask;
    else timer->DIER &= ~((uint32_t)mask);
}
void TIM_Cmd(TIM_TypeDef *timer, FunctionalState enabled)
{
    if (enabled == ENABLE) timer->CR1 |= 1u;
    else timer->CR1 &= ~1u;
}
void TIM_InternalClockConfig(TIM_TypeDef *timer)
{
    timer->SMCR &= ~7u;
}
void TIM_CtrlPWMOutputs(TIM_TypeDef *timer, FunctionalState enabled)
{
    if (timer != TIM1 && timer != TIM8) return;
    if (enabled == ENABLE) timer->BDTR |= 1u << 15;
    else timer->BDTR &= ~(1u << 15);
}
void TIM_SetCompare4(TIM_TypeDef *timer, uint16_t value)
{
    timer->CCR4 = value;
}
void TIM_ClearFlag(TIM_TypeDef *timer, uint16_t flag)
{
    timer->SR &= ~((uint32_t)flag);
}
ITStatus TIM_GetITStatus(TIM_TypeDef *timer, uint16_t flag)
{
    return (timer->DIER & timer->SR & flag) ? SET : RESET;
}
void TIM_ClearITPendingBit(TIM_TypeDef *timer, uint16_t flag)
{
    timer->SR &= ~((uint32_t)flag);
}
