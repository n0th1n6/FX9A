#include "bsp_timer.h"
#include "stm32f1xx_ll_tim.h"
#include "stm32f1xx_ll_bus.h"

extern volatile uint32_t fx9a_system_clock_hz;

CallBack Tmr0CallBack;
CallBack Tmr2CallBack;
CallBack Tmr4CallBack;

void TMR_NVIC_Config(void)
{
    /* Same preemption/subpriority split as the legacy priority group 2. */
    NVIC_SetPriorityGrouping(5u);
    NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(5u, 3u, 3u));
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_SetPriority(TIM4_IRQn, NVIC_EncodePriority(5u, 0u, 1u));
    NVIC_EnableIRQ(TIM4_IRQn);
    NVIC_SetPriority(TIM3_IRQn, NVIC_EncodePriority(5u, 0u, 1u));
    NVIC_EnableIRQ(TIM3_IRQn);
}
void TMR_Init(TIM_TypeDef *timer, uint32_t hz, uint32_t period, CallBack cBack)
{
    TMR_NVIC_Config();
    uint32_t peripheral = 0u;
    if (timer == TIM2) {
        peripheral = LL_APB1_GRP1_PERIPH_TIM2;
        Tmr2CallBack = cBack;
    } else if (timer == TIM3) {
        peripheral = LL_APB1_GRP1_PERIPH_TIM3;
    } else if (timer == TIM4) {
        peripheral = LL_APB1_GRP1_PERIPH_TIM4;
        Tmr4CallBack = cBack;
    }
    if (peripheral == 0u || hz == 0u || period == 0u) return;
    LL_APB1_GRP1_EnableClock(peripheral);
    LL_APB1_GRP1_ForceReset(peripheral);
    LL_APB1_GRP1_ReleaseReset(peripheral);

    /* TIM2/3/4 run at 72 MHz with APB1 prescaler /2 (timer clock x2). */
    const uint32_t ticks = fx9a_system_clock_hz / (hz * period);
    if (ticks == 0u || ticks > 65536u || period > 65536u) return;
    LL_TIM_SetPrescaler(timer, ticks - 1u);
    LL_TIM_SetAutoReload(timer, period - 1u);
    LL_TIM_SetCounterMode(timer, LL_TIM_COUNTERMODE_UP);
    LL_TIM_GenerateEvent_UPDATE(timer);
    LL_TIM_ClearFlag_UPDATE(timer);
    LL_TIM_EnableARRPreload(timer);
}
uint16_t TMR_GetTime(TIM_TypeDef *timer) { return (uint16_t)LL_TIM_GetCounter(timer); }
void TMR_SetTime(TIM_TypeDef *timer, uint16_t cnt) { LL_TIM_SetCounter(timer, cnt); }
void TMR_IRQEnable(TIM_TypeDef *timer) { LL_TIM_EnableIT_UPDATE(timer); }
void TMR_Start(TIM_TypeDef *timer) { LL_TIM_EnableCounter(timer); }
void TMR_Stop(TIM_TypeDef *timer) { LL_TIM_DisableCounter(timer); }
void TIM2_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM2) && LL_TIM_IsEnabledIT_UPDATE(TIM2)) {
        LL_TIM_ClearFlag_UPDATE(TIM2);
        if (Tmr2CallBack) Tmr2CallBack();
    }
}
void TIM4_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM4) && LL_TIM_IsEnabledIT_UPDATE(TIM4)) {
        LL_TIM_ClearFlag_UPDATE(TIM4);
        if (Tmr4CallBack) Tmr4CallBack();
    }
}
