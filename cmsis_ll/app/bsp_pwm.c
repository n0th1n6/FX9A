#include "bsp_pwm.h"
#include "stm32f1xx_ll_tim.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_bus.h"

//TIM3_CH4/PB1/36脚   输出音频信号，音频频率在500HZ-1KHZ之间可调（信号随KEY1/KEY2长短变化，同步于CW）

//占空比，取值范围为0-100
int dutyfactor = 50;

// F = 128 + 127sin(2*pi*n/N)
int SinTable[] = {255, 254, 246, 234, 219, 199, 177, 153, 128, 103, 79, 57, 37, 22, 10, 2,
1, 2, 10, 22, 37, 57, 79, 103, 128, 153, 177, 199, 219, 234, 246, 255};

//uint16_t SinTable[] = {255, 246, 219, 177, 128, 79, 37, 10,
//1, 10, 37, 79, 128, 177, 219, 246, 255};

uint32_t SinPara[32];

volatile uint8_t SampleTimes = 0;
#define PWM_CLK_FREQ 72000000   //BG6RDF
uint16_t pwm_period = 0;

uint8_t BuzzerIsRunning = 0;
void pwm_init()
{
  /* PB1 = TIM3 CH4 alternate-function push-pull output. */
  LL_GPIO_InitTypeDef pin = {0};
  pin.Pin = LL_GPIO_PIN_1;
  pin.Mode = LL_GPIO_MODE_ALTERNATE;
  pin.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  pin.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  pin.Pull = LL_GPIO_PULL_DOWN;
  LL_GPIO_Init(GPIOB, &pin);

  const uint32_t prescaler = 4u;
  pwm_period = PWM_CLK_FREQ / 8000u / prescaler;
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
  LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM3);
  LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM3);

  LL_TIM_SetPrescaler(TIM3, prescaler - 1u);
  LL_TIM_SetAutoReload(TIM3, pwm_period - 1u);
  LL_TIM_SetCounterMode(TIM3, LL_TIM_COUNTERMODE_UP);
  LL_TIM_OC_SetMode(TIM3, LL_TIM_CHANNEL_CH4, LL_TIM_OCMODE_PWM1);
  LL_TIM_OC_SetPolarity(TIM3, LL_TIM_CHANNEL_CH4, LL_TIM_OCPOLARITY_HIGH);
  LL_TIM_OC_SetCompareCH4(TIM3, dutyfactor * pwm_period / 100u);
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH4);
  LL_TIM_EnableARRPreload(TIM3);
  LL_TIM_GenerateEvent_UPDATE(TIM3);
  LL_TIM_ClearFlag_UPDATE(TIM3);
  LL_TIM_EnableIT_UPDATE(TIM3);
}

void pwm_freq_set(uint32_t hz)
{
  if (hz == 0u || hz > (PWM_CLK_FREQ / 64u)) return;
  hz *= 32u; /* 32 sine samples per cycle */
  const uint32_t prescaler = 2u;
  const uint32_t period = PWM_CLK_FREQ / hz / prescaler;
  if (period == 0u || period > 65536u) return;
  pwm_period = (uint16_t)period;
  LL_TIM_SetPrescaler(TIM3, prescaler - 1u);
  LL_TIM_SetAutoReload(TIM3, period - 1u);
  LL_TIM_GenerateEvent_UPDATE(TIM3);
  LL_TIM_ClearFlag_UPDATE(TIM3);
}

void sin_para_get()
{
  for(uint8_t i=0; i<32; i++)
  {
    SinPara[i] = SinTable[i] * pwm_period / 256;        //除以256是归一化，
    SinPara[i] = SinPara[i] * MENU.dutyOfbuzzer / 10;
  }
}

void pwm_start()
{
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);                  //channel 4打开BG6RDF
  LL_TIM_EnableCounter(TIM3);	
  //TIM_CtrlPWMOutputs(TIM3,ENABLE);    //只用于TIM1和TIM8  BG6RDF
}

void pwm_stop()
{
  //TIM_CtrlPWMOutputs(TIM3,DISABLE);   //只用于TIM1和TIM8  BG6RDF
  LL_TIM_DisableCounter(TIM3);
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);                  //channel 4关闭 BG6RDF
  LL_TIM_OC_SetCompareCH4(TIM3, 0);            //BG6RDF
  SampleTimes = 15;                     //BG6RDF
}

void buzzer_freq_set(uint32_t hz)
{
  pwm_freq_set(hz);
}

void buzzer_start()
{
  if (BuzzerIsRunning == 0)
  {
    BuzzerIsRunning = 1;
    pwm_start();
  }
}

void buzzer_stop()
{
  BuzzerIsRunning = 0;
  pwm_stop();
}

void TIM3_IRQHandler(void)
{
  if(LL_TIM_IsActiveFlag_UPDATE(TIM3) && LL_TIM_IsEnabledIT_UPDATE(TIM3))     //检测是否发生溢出更新事件  
  {
    LL_TIM_ClearFlag_UPDATE(TIM3);      //清除TIM3的中断待处理位 
    SampleTimes = (SampleTimes + 1) % 32;
    LL_TIM_OC_SetCompareCH4(TIM3, SinPara[SampleTimes]);
  }
}