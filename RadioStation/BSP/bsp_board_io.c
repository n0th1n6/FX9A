#include "bsp_board.h"
#include "bsp_board_io.h"

void gpio_init(void)
{
      // GPIO enable clock and release Reset ,外部中断 辅助时钟必须打开
  RCC_APB2PeriphResetCmd(  RCC_APB2Periph_GPIOA, DISABLE);
  RCC_APB2PeriphClockCmd(  RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
      // GPIO enable clock and release Reset
  RCC_APB2PeriphResetCmd(  RCC_APB2Periph_GPIOB, DISABLE);
  RCC_APB2PeriphClockCmd(  RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
  GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);      /*使能SWD 禁用JTAG*/
  
      // GPIO enable clock and release Reset
  RCC_APB2PeriphResetCmd(  RCC_APB2Periph_GPIOC, DISABLE);
  RCC_APB2PeriphClockCmd(  RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
  
      // GPIO enable clock and release Reset
  RCC_APB2PeriphResetCmd(  RCC_APB2Periph_GPIOD, DISABLE);
  RCC_APB2PeriphClockCmd(  RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);
  
    // GPIO enable clock and release Reset
  RCC_APB2PeriphResetCmd(  RCC_APB2Periph_GPIOE, DISABLE);
  RCC_APB2PeriphClockCmd(  RCC_APB2Periph_GPIOE, ENABLE);
}

void power_key_init(void)
{
  // 34脚-PC5  电源控制脚 通电为高电平， 断电为低电平
  GPIO_InitTypeDef GPIO_InitStructure;
  
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_5;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  
  // PE15/46 VoiceControl : 启动为高  停止为低
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOE, &GPIO_InitStructure);
  
  // PB10/47 H/L : 启动为高  停止为低
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void voice_control_stop(void)
{
  GPIO_SetBits(GPIOE, GPIO_Pin_15);  
}

void voice_control_start(void)
{
  GPIO_ResetBits(GPIOE, GPIO_Pin_15);
}

void power_high_on(void)
{
  GPIO_SetBits(GPIOB, GPIO_Pin_10);  
}

void power_high_off(void)
{
  GPIO_ResetBits(GPIOB, GPIO_Pin_10);
}

void power_on(void)
{
  GPIO_SetBits(GPIOC, GPIO_Pin_5);  
}

void power_off(void)
{
  GPIO_ResetBits(GPIOC, GPIO_Pin_5); 
}

#define POWER_ON_WAIT_COUNT 3000000

void WaitForPowerOn(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  //先把电源按钮引脚设置为输入，90脚-PB4
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  uint32_t cnt=0;
  while(1)
  {
   if (!GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4))
     cnt++;
   else
     cnt=0;
   if (cnt>POWER_ON_WAIT_COUNT)
     return;
  }
}

