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



volatile bool RfIsTx = false;

volatile uint16_t hswr_timeout = 0;
static volatile uint8_t swr_trip_pending;

/* MemSetMaxIndex is owned by the frequency/persistence module. */



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

uint8_t FILFunc;











//在设置菜单模式下按下TS按钮，调用本函数



extern volatile uint32_t fx9a_ticks_10ms;
static Fx9aTuneLogic tune_logic;
static TP_LevelDef tune_saved_power;
static volatile bool tune_active;
static volatile bool tune_carrier_on;
static volatile bool tune_swr_sampled;
static volatile uint16_t tune_last_swr_adc;











bool Tune_IsActive(void)
{
  return tune_active;
}

bool Tune_Start(void)
{
  if (tune_active || RfIsTx || hswr_timeout || swr_trip_pending ||
      !isRun || PttPressed || AIMgr.item)
    return false;
  /* Physically select LOW power before asserting the transmitter.
   * Do not change RADIO.mode or the user's displayed RF frequency.
   */
  RadioUI_TuneStarting();
  tune_swr_sampled = false;
  tune_last_swr_adc = 0u;
  tune_carrier_on = false;
  tune_saved_power = TP.level;
  TP.level = L;
  power_high_off();
  DRMgr.item |= TPItem;
  fx9a_tune_logic_init(&tune_logic, fx9a_ticks_10ms);
  tune_active = true;
  CWT_Off();
  RF_ToTx();
  MICControlOff();
  /* A trip during DDS settling must also inhibit the TUNE carrier. */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  bool started = RfIsTx && !hswr_timeout && !swr_trip_pending;
  if (started) {
    CWT_On(); /* Continuous carrier, never the iambic keyer. */
    tune_carrier_on = true;
  }
  __set_PRIMASK(primask);
  if (!started || hswr_timeout || swr_trip_pending) {
    Tune_Stop();
    return false;
  }
  oled_lsb_usb("TUNE");
  return true;
}

void Tune_Stop(void)
{
  if (!tune_active) return;
  bool had_carrier = tune_carrier_on;
  tune_carrier_on = false; /* Freeze the RF-on reading before removing RF. */
  CWT_Off();
  MICControlOff();
  tx_control_off();
  tune_active = false; /* Restore the ordinary receive DDS calculations. */
  RF_ToRx();
  TP.level = tune_saved_power;
  if (TP.level == L) power_high_off();
  else power_high_on();
  /* MODE_Refresh normally skips an unchanged mode, leaving TUNE behind.
   * Force the original mode label back for every completion/cancel path.
   */
  RadioUI_TuneStopped(had_carrier);
  DRMgr.item |= TPItem | RADIOItem | TXRXItem | SWRCHKItem;
}

void Tune_Task(void)
{
  if (!tune_active) return;
  uint32_t tick = fx9a_ticks_10ms;
  Fx9aTuneResult result = fx9a_tune_logic_step(
      &tune_logic, tick, adc_swr(),
      PttPressed || hswr_timeout || !isRun || !RfIsTx);
  if (result != FX9A_TUNE_RUNNING)
    Tune_Stop();
}











/* Called at 10 ms intervals by TIM2's 250 us sampling ISR.
 * Do not call OLED, DDS, delay, or EEPROM functions inside this ISR.
 */
static uint8_t swr_high_samples;
static uint16_t swr_trip_level(uint8_t level)
{
  if (level <= 6u) return (uint16_t)(20u + level * 56u);
  return (uint16_t)(360u + (level - 6u) * 71u);
}

void RF_SwrSampleISR(void)
{
  if (!RfIsTx || swr_trip_pending) {
    swr_high_samples = 0u;
    return;
  }
  uint16_t reflected = adc_swr();
  bool tune = Tune_IsActive();
  if (tune && tune_carrier_on) {
    /* Save before a protection trip removes RF. OLED work stays in main. */
    tune_last_swr_adc = reflected;
    tune_swr_sampled = true;
  }
  /* Severe reflected power cuts the RF gate on the first 10 ms sample,
   * irrespective of the user's ordinary SWR protection setting.
   */
  if (reflected >= FX9A_TUNE_SEVERE_REFLECTED_ADC) {
    MICControlOff();
    tx_control_off();
    swr_trip_pending = 1u;
    hswr_timeout = 1u;
    return;
  }
  if (MENU.vswr == 0u ||
      (tune && (uint32_t)(fx9a_ticks_10ms - tune_logic.started_at) <
               FX9A_TUNE_SETTLE_TICKS)) {
    swr_high_samples = 0u;
    return;
  }
  uint16_t limit = swr_trip_level(MENU.vswr > 9u ? 9u : MENU.vswr);
  if (reflected >= limit) {
    /* Normal TX: 3 samples / 30 ms. TUNE: 30 / 300 ms to allow match-up. */
    uint8_t required = tune ? 30u : 3u;
    if (++swr_high_samples >= required) {
      MICControlOff();
      tx_control_off();
      swr_trip_pending = 1u;
      hswr_timeout = 1u;
    }
  } else swr_high_samples = 0u;
}

void RF_SwrService(void)
{
  if (!swr_trip_pending) return;
  swr_trip_pending = 0u;
  if (Tune_IsActive()) Tune_Stop();
  else RF_ToRx();
  oled_h_vswr_disp();
}

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
static void RF_ProgramSynth(void)
{
  uint32_t freq = FREQ.val;
  if (!(FREQ.item & MEMItem)) {
    if (RfIsTx && !Tune_IsActive() && (FREQ.item & XITItem)) {
      int64_t adjusted = (int64_t)freq + VFO.XIT;
      if (adjusted > 0) freq = (uint32_t)adjusted;
    } else if (!RfIsTx && (FREQ.item & RITItem)) {
      int64_t adjusted = (int64_t)freq + VFO.RIT;
      if (adjusted > 0) freq = (uint32_t)adjusted;
    }
  }
  lo_update(Get_DDSLO(freq), MENU.ref);
  bfo_update(Get_DDSBFO(), MENU.ref);
}

void RF_Tx(void)
{
  /* Prepare a silent path; RF control stays OFF until the DDS has settled. */
  MICControlOff();
  QuietModeOn();
  CW_FilterControl(0);
  if (Tune_IsActive() || RADIO.mode == CWA || RADIO.mode == CWB) {
    CW_BFOControl(1);
    if (!Tune_IsActive()) TMR_Start(TIM4);
    else TMR_Stop(TIM4);
  } else {
    CW_BFOControl(0);
    TMR_Stop(TIM4);
  }
  RF_ProgramSynth();
  delay_ms(2); /* First hardware gate must verify synthesizer settling. */
  /* TIM2 can trip while the DDS settles. Commit the gate and MIC together
   * only if no trip is latched; otherwise the ISR's shutdown must win.
   * Keep the slow synthesizer/delay work outside this critical section.
   */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (RfIsTx && !hswr_timeout && !swr_trip_pending) {
    tx_control_on();
    if (!Tune_IsActive() && RADIO.mode != CWA && RADIO.mode != CWB)
      MICControlOn();
  }
  __set_PRIMASK(primask);
}

void RF_Rx(void)
{
  /* Remove RF and microphone drive before changing any DDS or filters. */
  MICControlOff();
  tx_control_off();
  QuietModeOn();
  CW_BFOControl(0);
  if (RADIO.mode == CWA || RADIO.mode == CWB)
    CW_FilterControl(FIL.func == N ? 1 : 0);
  RF_ProgramSynth();
  delay_ms(2);
  QuietModeOff();
}

void RF_ToRx(void)
{
  /* RF_Rx must operate with RfIsTx already cleared so Get_DDSLO()
   * restores the receive offset instead of the transmit LO.
   * Drop the physical TX control first, with the microphone muted.
   */
  MICControlOff();
  tx_control_off();
  RfIsTx = false;
  RF_Rx();
  DRMgr.item |= TXRXItem;
}

void RF_ToTx(void)
{
  if (RfIsTx || hswr_timeout || swr_trip_pending) return;
  /* RfIsTx informs LO/BFO calculation, NOT the physical PA gate. */
  RfIsTx = true;
  RF_Tx();
  DRMgr.item |= TXRXItem;
}

bool RF_IsTx()
{
  return RfIsTx;
}

bool RF_IsRx()
{
  return !RfIsTx;
}




