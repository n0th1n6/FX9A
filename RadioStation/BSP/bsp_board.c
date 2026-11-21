#include "bsp_board.h"
#include "bsp_board_io.h"
#include "radio_state.h"
#include "radio_rf.h"
#include "radio_controls.h"
#include "radio_ui.h"
#include "radio_cat.h"
#include "meter_cal.h"
#include "bsp_key.h"
#include "bsp_adc.h"
#include "bsp_oled.h"
#include "bsp_oled_api.h"
#include "bsp_baud.h"
#include "bsp_iambic_keyer.h"
#include "bsp_24lc16.h"
#include "bsp_transmit_power.h"
#include "bsp_vol.h"
#include "bsp_ad9951.h"
#include "bsp_ad9834.h"
#include "bsp_pwm.h"
#include "bsp_timer.h"
#include "bsp_usart.h"

void gpio_init()
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

void power_key_init()
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

void voice_control_stop()
{
  GPIO_SetBits(GPIOE, GPIO_Pin_15);  
}

void voice_control_start()
{
  GPIO_ResetBits(GPIOE, GPIO_Pin_15);
}

void power_high_on()
{
  GPIO_SetBits(GPIOB, GPIO_Pin_10);  
}

void power_high_off()
{
  GPIO_ResetBits(GPIOB, GPIO_Pin_10);
}

void power_on()
{
  GPIO_SetBits(GPIOC, GPIO_Pin_5);  
}

void power_off()
{
  GPIO_ResetBits(GPIOC, GPIO_Pin_5); 
}

#define POWER_ON_WAIT_COUNT 3000000

void WaitForPowerOn()
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

void board_init()
{
  gpio_init();          //启动GPIO
  tp_init();            //一些GPIO引脚的设置
  QuietModeOn();        //静音
  
  /* Match the original radio image: assert PC5 power hold without a
   * second button-wait loop while the supply is still momentary. */
  
  vol_init();           //设置音量控制引脚
  
  power_key_init();     //设置电源，高低功率控制引脚
  power_on();           //PC5设置高电平，启动主电源
  i2c_init();
  
  delay_ms(50);
  vol_control(0);
  e2p_reset();
  
  RadioState_Load();
  MeterCal_Init();
  key_init();           //初始化按钮、编码器和电键的GPIO，初始化GPIO中断和中断向量表，检测手键还是自动键
  
  //如果K4,K6被按下，则恢复到默认值
  uint16_t portValue=GPIO_ReadInputData(GPIOD);
  if (!(portValue & GPIO_BSRR_BS4) &&  !(portValue & GPIO_BSRR_BS1))  //K4, K6也被按下
    RadioState_ResetDefaults();

  oled_init();
  adc_init();
  baud_init();
  pwm_init();           //设置PWM，用于侧音
  keyer_init();
  
    
  ad9834_init();
  ad9951_init();
  
  usart_init();
  //oled_h_vswr_disp();
  RF_ToRx();
  isRun = true;
  
  //
//  buzzer_freq_set(500);
//  sin_para_get();
//  buzzer_start();
}

void board_task()
{
  RF_SwrService();
  Tune_Task();
  RadioControls_Task();
  
  RadioUI_Refresh();
  
  if (!Tune_IsActive() && (RADIO.mode == CWA || RADIO.mode == CWB))
  {
    if(FREQ.item & CALLItem)
    {
      call_task();
    }
    else if(MENU.cwMode == MANUAL && AIMgr.item <= 0)
    {
      keyer_manual_task();
    }
    else if(AIMgr.item <= 0)
    {
      keyer_auto_task();
    }
  }
  
  RadioCAT_Task();
}
