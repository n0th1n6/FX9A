#include "bsp_board.h"
#include "tune_logic.h"
#include "tuning_policy.h"
#include "meter_cal.h"
#include "settings_validation.h"
#include "radio_state.h"
#include "ll_pins.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_exti.h"
#include "bsp_frequency.h"
#include "radio_synth.h"
#include "bsp_oled_api.h"
#include "bsp_key.h"
#include "bsp_adc.h"
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
#include "radio_cat.h"
#include "radio_controls.h"
#include "radio_ui.h"
#include "radio_rf.h"
#include "radio_board_io.h"



extern KEYER_AttrDef KEYER;


uint8_t tmp = 0;




uint8_t NumOfMem;








/* Settings persistence is owned by settings_store. */



//用于信号平滑







void gpio_init()
{
      // GPIO enable clock and release Reset ,外部中断 辅助时钟必须打开
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_GPIOA);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA | LL_APB2_GRP1_PERIPH_AFIO);
      // GPIO enable clock and release Reset
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_GPIOB);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOB | LL_APB2_GRP1_PERIPH_AFIO);
  (AFIO->MAPR = (AFIO->MAPR & ~(7u << 24)) | (2u << 24));      /*使能SWD 禁用JTAG*/
  
      // GPIO enable clock and release Reset
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_GPIOC);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC | LL_APB2_GRP1_PERIPH_AFIO);
  
      // GPIO enable clock and release Reset
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_GPIOD);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOD | LL_APB2_GRP1_PERIPH_AFIO);
  
    // GPIO enable clock and release Reset
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_GPIOE);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOE);
}

void power_key_init()
{
  // 34脚-PC5  电源控制脚 通电为高电平， 断电为低电平
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(GPIO_BSRR_BS5);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT; GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(GPIOC, &GPIO_InitStructure);
  
  // PE15/46 VoiceControl : 启动为高  停止为低
  GPIO_InitStructure.Pin = fx9a_ll_pins(GPIO_BSRR_BS15);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT; GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(GPIOE, &GPIO_InitStructure);
  
  // PB10/47 H/L : 启动为高  停止为低
  GPIO_InitStructure.Pin = fx9a_ll_pins(GPIO_BSRR_BS10);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_OUTPUT; GPIO_InitStructure.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void voice_control_stop()
{
  (GPIOE->BSRR = (GPIO_BSRR_BS15));  
}

void voice_control_start()
{
  (GPIOE->BRR = (GPIO_BSRR_BS15));
}

void power_high_on()
{
  (GPIOB->BSRR = (GPIO_BSRR_BS10));  
}

void power_high_off()
{
  (GPIOB->BRR = (GPIO_BSRR_BS10));
}

void power_on()
{
  (GPIOC->BSRR = (GPIO_BSRR_BS5));  
}

void power_off()
{
  (GPIOC->BRR = (GPIO_BSRR_BS5)); 
}

uint8_t save;
MENU_AttrDef tMem;
uint8_t buf[6];
VFO_SaveDef TVFO;

/* Old EEPROM images have no alignment signature. Defaults preserve the
 * original SSB frequencies; never reinterpret unrelated reserved bytes. */










/* Validate unversioned EEPROM values before they can index band/memory
 * tables or program synthesizers. Recover in RAM without silently rewriting
 * the EEPROM; a future version/CRC format can distinguish missing reads.
 */




#define POWER_ON_WAIT_COUNT 3000000
//等待电源按钮按下一段时间  BG6RDF
void WaitForPowerOn()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
  //先把电源按钮引脚设置为输入，90脚-PB4
  GPIO_InitStructure.Pin = fx9a_ll_pins(GPIO_BSRR_BS4);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_FLOATING;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  uint32_t cnt=0;
  while(1)
  {
   if (!((GPIOB->IDR & (GPIO_BSRR_BS4)) != 0u))
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
  
  software_init();
  MeterCal_Init();
  key_init();           //初始化按钮、编码器和电键的GPIO，初始化GPIO中断和中断向量表，检测手键还是自动键
  
  //如果K4,K6被按下，则恢复到默认值
  uint16_t portValue=(GPIOD->IDR);
  if (!(portValue & GPIO_BSRR_BS4) &&  !(portValue & GPIO_BSRR_BS1))  //K4, K6也被按下
    ResetToDefault();

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













//在设置菜单模式下按下TS按钮，调用本函数










































/* Called at 10 ms intervals by TIM2's 250 us sampling ISR.
 * Do not call OLED, DDS, delay, or EEPROM functions inside this ISR.
 */







void board_task()
{
  RF_SwrService();
  Tune_Task();
  key_task();
  
  board_refresh_manager();
  
  if( RADIO.mode == CWA || RADIO.mode == CWB)
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

/* Keep frequency, filter selection, and VFO persistence in agreement without
 * recalling another band's saved frequency when tuning across a boundary. */




// 在W/V切换、VFOA/B切换、波段切换及菜单退出情况下，被调用


// 在频率和模式更新的情况下，被调用






/* Update synthesizers synchronously during TX/RX transitions.
 * Display refresh must not be responsible for RF safety sequencing.
 */

















