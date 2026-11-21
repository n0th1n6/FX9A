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
#include "radio_board_io.h"



extern KEYER_AttrDef KEYER;

uint32_t adc_val;
uint8_t tmp = 0;

RADIO_ModeDef Mode;
RADIO_ModeDef MemSetMode;
TP_LevelDef TPLevel;
uint8_t NumOfMem;



volatile bool RfIsTx = false;

volatile uint16_t hswr_timeout = 0;
static volatile uint8_t swr_trip_pending;

/* MemSetMaxIndex is owned by the frequency/persistence module. */



//用于信号平滑
#define RF_ALPHA 0.9    //发射平滑系数
#define RX_ALPHA 0.9    //接收平滑系数
volatile static float last_rf_grid=0;
volatile static float last_rx_grid=0;



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
void FIL_Refresh(uint8_t all)
{
  if(FILFunc != FIL.func || all)
  {
    FILFunc = FIL.func;
    if(FILFunc == W)
    {
      oled_filter(" ");
    }
    else
    {
      oled_filter("N");
    }
  }
}
void TP_Refresh(uint8_t all)
{
  if(TPLevel != TP.level || all)
  {
    TPLevel = TP.level;
    if(TPLevel == H)
    {
      oled_tx_power(" ");
    }
    else if(TPLevel == M)
    {
      oled_tx_power("M");
    }
    else
    {
      oled_tx_power("L");
    }
  }
}

void MODE_Refresh(uint8_t all)
{
  if(Mode != RADIO.mode || all)
  {
    Mode = RADIO.mode;
    if(Mode == RADIO_USB)
    {
      voice_control_start();
      oled_lsb_usb("USB ");
    }
    else if(Mode == LSB)
    {
      voice_control_start();
      oled_lsb_usb("LSB ");
    }
    else if(Mode == CWB)
    {
      voice_control_stop();
      oled_lsb_usb("CW/B");
      keyer_auto_setup();
    }
    else
    {
      voice_control_stop();
      oled_lsb_usb("CW/A");
      keyer_auto_setup();
    }
  }
}

void MEM_SetModeRefresh(uint8_t all)
{
  if(MemSetMode != RADIO.mode || all)
  {
    MemSetMode = RADIO.mode;
    if(MemSetMode == RADIO_USB)
    {
      voice_control_start();
      oled_lsb_usb_mem("USB ");
    }
    else if(MemSetMode == LSB)
    {
      voice_control_start();
      oled_lsb_usb_mem("LSB ");
    }
    else if(MemSetMode == CWB)
    {
      voice_control_stop();
      oled_lsb_usb_mem("CW/B");
    }
    else
    {
      voice_control_stop();
      oled_lsb_usb_mem("CW/A");
    }
  }
}





//在设置菜单模式下按下TS按钮，调用本函数



extern volatile uint32_t fx9a_ticks_10ms;
static Fx9aTuneLogic tune_logic;
static TP_LevelDef tune_saved_power;
static volatile bool tune_active;
static volatile bool tune_carrier_on;
static volatile bool tune_swr_sampled;
static volatile uint16_t tune_last_swr_adc;
static bool tune_result_visible;
static uint32_t tune_result_tick;
static uint32_t tune_result_frequency;
#define TUNE_RESULT_TICKS 500u /* Five seconds on the independent 10 ms clock. */

static uint8_t SWR_DisplayGrid(uint16_t adc)
{
  if (MeterCal_Active()) return MeterCal_Grid(2u, adc);
  if (adc > 574u) return 9u;
  if (adc > 360u) return (uint8_t)(6u + (adc - 360u) / 71u);
  if (adc > 20u) return (uint8_t)((adc - 20u) / 56u);
  return 0u;
}

static void Tune_ResultDraw(void)
{
  if (!tune_result_visible) return;
  oled_tune_result(1u, tune_swr_sampled);
  if (tune_swr_sampled) oled_swr_grid(SWR_DisplayGrid(tune_last_swr_adc));
}

static void Tune_ResultExpire(void)
{
  if (!tune_result_visible) return;
  if (isRun && !RfIsTx && !AIMgr.item && FREQ.val == tune_result_frequency &&
      (uint32_t)(fx9a_ticks_10ms - tune_result_tick) < TUNE_RESULT_TICKS)
    return;
  tune_result_visible = false;
  /* Settings pages redraw their own screen; do not erase their contents. */
  if (isRun && !AIMgr.item) {
    oled_tune_result(0u, 0u);
    oled_txrx_clear_grid();
    DRMgr.item |= RfIsTx ? (SWRCHKItem | RFCHKItem) : RXCHKItem;
  }
}

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
  if (tune_result_visible) oled_tune_result(0u, 0u);
  tune_result_visible = false;
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
  MODE_Refresh(1u);
  tune_result_visible = had_carrier || tune_swr_sampled;
  tune_result_tick = fx9a_ticks_10ms;
  tune_result_frequency = FREQ.val;
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





void board_refresh_manager()
{
  Tune_ResultExpire();
  uint8_t grid = 0;
  
  if (DRMgr.item > 0)
  {
    if( RADIO.mode == CWA || RADIO.mode == CWB )
    {
      // SSB与CW切换后，存储滤波器状态
      if( CWFilter.func == N )  
      {
        FIL.func = N;
      }
      else
      {
        FIL.func = W;
      }
    }
    else
    {
      // 从CW切换到SSB模式，关闭滤波器
      FIL.func = W;
    } 
      
    if (DRMgr.item & FREQItem || DRMgr.item & ALLItem)
    {
      DRMgr.item &= ~FREQItem;  
      
      // 从设置状态退出时，更新VFO显示
      if (FREQ.item & VFOItem && DRMgr.item & ALLItem)
      {
        if(FREQ.item & VFOAItem)
        {
          oled_vfo("VFO/A ");
        }
        else
        {
          oled_vfo("VFO/B ");
        }
      }
      else if (FREQ.item & MEMItem)   // 无论是设置退出时，还是信道切换时，只要是MEM模式，更新信道号
      {
        oled_update_mem(FREQ.numOfmem);
      }
      
      
      
      if( DRMgr.item & ALLItem )  // 退出设置菜单后，重新刷新屏幕主频率
      {
        if(FREQ.item & MEMItem)
        {
          MEM_Get();
        }
        else
        {
          if( FREQ.item & VFOAItem )
          {
            FREQ.numOfbaud = VFO.CurrBaudOfVfoA;
          }
          else
          {
            FREQ.numOfbaud = VFO.CurrBaudOfVfoB;
          }
          VFO_Get();  // 此时波段变量没有变动，不需要重新读取，
        }
      }
      else if(!(FREQ.item & MEMItem)) // MEM显示退出后，不进行频率存储
      {
        VFO_Save();
      }
      
      // 从设置状态退出时，更新TS显示
      if (FREQ.item & TSItem)
      {
        if( DRMgr.item & ALLItem )
        {
          oled_ts("TS");
        }
        FREQ.val /= 100;
        FREQ.val *= 100;
      }
      
      if (FREQ.item & RITItem && !(FREQ.item & MEMItem))  // RIT使能时，显示频率=RIT + FREQ
      {
        if (DRMgr.item & ALLItem)
        {
          oled_rit("RIT");
        }
        oled_rit_txfreq(FREQ.val);
        
        oled_update_frequency(FREQ.val+VFO.RIT, (DRMgr.item & ALLItem)?1:0); 
        lo_update(Get_DDSLO(FREQ.val+VFO.RIT), MENU.ref);
        bfo_update(Get_DDSBFO(), MENU.ref);
      }
      else if (FREQ.item & XITItem && !(FREQ.item & MEMItem))   //XIT，显示频率=FREQ+XIT BG6RDF
      {
        if (DRMgr.item & ALLItem)
        {
          oled_rit("XIT");
        }
        oled_rit_txfreq(FREQ.val);
        
        oled_update_frequency(FREQ.val+VFO.XIT, (DRMgr.item & ALLItem)?1:0); 
        //XIT模式也必须刷新本振，因为VFOA/VFOB切换时，本振也在这切换
        lo_update(Get_DDSLO(FREQ.val), MENU.ref);
        bfo_update(Get_DDSBFO(), MENU.ref);
      }
      else
      {
        oled_update_frequency(FREQ.val, (DRMgr.item & ALLItem)?1:0); 
        lo_update(Get_DDSLO(FREQ.val), MENU.ref);
        bfo_update(Get_DDSBFO(), MENU.ref);
      }
      baud_chn_ctrl(FREQ.numOfbaud);  // 硬件波段控制
      
      if (DRMgr.item == 0)
      {
        return;
      }
    }
    
    if (DRMgr.item & RADIOItem || DRMgr.item & ALLItem)
    {
      DRMgr.item &= ~RADIOItem;
      if (AIMgr.item & MEMSETItem)
      {
        MEM_SetModeRefresh(MemSetAll);
        MEM_Save();
      }
      else
      {
        MODE_Refresh((DRMgr.item & ALLItem)?1:0);
      
        // 信道模式下，模式可调，但不记忆模式
        if(!(FREQ.item & MEMItem))
        {
          VFO_Save();
        }
      }
      
      if (DRMgr.item == 0)
      {
        return;
      }
    }
    
    if (DRMgr.item & FILItem || DRMgr.item & ALLItem)
    {
      DRMgr.item &= ~FILItem;
      
      if (FIL.func == N) // 显示时，只显示N， W不显示； 功率显示，是显示L
      {
        CW_FilterControl(1);
      }
      else
      {
        CW_FilterControl(0);
      }
      
      
      FIL_Refresh((DRMgr.item & ALLItem)?1:0);
      
      if (DRMgr.item == 0)
      {
        return;
      }
    }
    
    if (DRMgr.item < MEMSETItem && AIMgr.item <= 0)
    {
      if ( DRMgr.item & DCItem || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~DCItem;
        //4095-0: 2.5v-0
        // 4095 / adc = 2.5 / x => x = 2.5 * adc / 4096 
        // 分压电阻 10K 1K
        adc_val = 2500 * adc_batt();
        adc_val /= 4096;
        adc_val = adc_val * 11;
        DC.val = adc_val / 100;
        
        //DC.val = (uint32_t)((((adc_batt()*2474*61)/1000)/4096)*1.31);
        oled_update_dc(DC.val); 
        
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if ( DRMgr.item & SWRCHKItem || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~SWRCHKItem;
        
                //4096-0: 2.5v-0
        // 1.5:0.12V(196.608)  3:0.35V(573.44) 2:0.22(360.448)
        /* Hold the last RF-on sample after TUNE, rather than reading the
         * detector after returning to RX (which would misleadingly be zero).
         */
        adc_val = tune_result_visible ? tune_last_swr_adc : adc_swr();
        
        if(adc_val > 574)
        {
          grid = 9;
        }
        else if( adc_val > 360)
        {
          grid = 6 + (adc_val-360) / 71; // (574-360) / 3 = 71
        }
        else if( adc_val > 20)
        {
          grid = (adc_val-20) / 56; // (360-20) / 6 = 56
        }
        else
        {
          grid = 0;
        }
        uint8_t protection_grid = grid; /* Never trust user meter calibration for PA protection. */
        if (MeterCal_Active()) grid = MeterCal_Grid(2u, adc_val);
        if (!tune_result_visible || tune_swr_sampled) oled_swr_grid(grid);
        if (RF_IsTx() && !Tune_IsActive() && protection_grid >= MENU.vswr && MENU.vswr > 0)
        {
          RF_ToRx();
          oled_h_vswr_disp();
          hswr_timeout = 1;
        }
        
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if ( DRMgr.item & RFCHKItem || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~RFCHKItem;
        //4096-0: 2.5v-0
        // 15W:1V(1638.4)  5W:0.6V(983.04)
        adc_val = adc_rf();
        
        /*
        if(adc_val > 1638)
        {
          grid = 13;
        }
        else if( adc_val > 983)
        {
          grid = 5 + (adc_val-983) / 82; // (1638-983) / 8 = 82
        }
        else if( adc_val > 20)
        {
          grid = (adc_val-20) / 192; // (983-20) / 5 = 192
        }
        else
        {
          grid = 0;
        }*/
        
        /**
        *   2016年9月9日 修改量10W
        *       10W电压0.66V   5瓦电压0.47V
        *       4096-0: 2.5v-0
        *       10W:0.66V(1081.3)  5W:0.47V(770.04)
        */
        
        if(adc_val > 1081)
        {
          grid = 11;
        }
        else if( adc_val > 770)
        {
          grid = 6 + (adc_val-770) / 62; // (1081-770) / 5 = 62.2
        }
        else if( adc_val > 20)
        {
          grid = (adc_val-20) / 125; // (770-20) / 6 = 125
        }
        else
        {
          grid = 0;
        }
        
        //平滑，快起慢落       BG6RDF
        if (MeterCal_Active()) grid = MeterCal_Grid(1u, adc_val);
        float rf_grid=(float)grid;
        if (rf_grid<last_rf_grid)
        {
          rf_grid=last_rf_grid*RF_ALPHA+rf_grid*(1-RF_ALPHA);
          grid=(uint32_t)rf_grid;
        }
        last_rf_grid=rf_grid;
        
        if (!tune_result_visible) oled_rf_grid(grid);
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if ( DRMgr.item & RXCHKItem || DRMgr.item & ALLItem)
      {
        
        DRMgr.item &= ~RXCHKItem;
        //4096-0: 2.5v-0
        // +40:0.8V(1310.72)  +20:0.67(1097.728) 9:0.54(819.2) 6:0.04(65.536) 3:0.008(13.1072) 
        // 2.4V (3932.16)        2V (3276.8)       1.47V (2408.448)       5:0.08(131.072)        1:0.02(32.768)
        adc_val = adc_rx();
        
        if(adc_val > 3932)
        {
          grid = 25;
        }
        else if( adc_val > 3276)
        {
          grid = 21 + (adc_val-3276) / 164; // (3932-3276) / 4 = 164
        }
        else if( adc_val > 2408 )
        {
          grid = 17 + (adc_val-2408) / 217; // (3276-2408) / 4 = 217
        }
        else if( adc_val > 131 )
        {
          grid = 10 + (adc_val-131) / 325;// (2048-131) / 7 = 325
        }
        else if( adc_val > 32 )
        {
          grid = 0 + (adc_val-32) / 11; // (131-32) / 9 = 11
        }
        else
        {
          grid = 0;
        }

        //平滑，快起慢落
        if (MeterCal_Active()) grid = MeterCal_Grid(0u, adc_val);
        float rx_grid=(float)grid;
        if (rx_grid<last_rx_grid)
        {
          rx_grid=last_rx_grid*RX_ALPHA+rx_grid*(1-RX_ALPHA);
          grid=(uint32_t)rx_grid;
        }
        last_rx_grid=rx_grid;
        
        if (!tune_result_visible) oled_rx_grid(grid);
        
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if (DRMgr.item & VOLItem || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~VOLItem;
        oled_update_vol(VOL.val);
        vol_control(VOL.val);

        
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      // ATT功能去掉， LOCK频率旋钮锁定功能代替
      if (DRMgr.item & ATTItem  || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~ATTItem;
        //oled_update_att(ATT.val);   // 46-ATT : 启动为高  停止为低
        oled_button_lock(ATT.val);   // 1: LOCK锁定状态（开机默认） 0: 解锁状态
//
//        if (ATT.val >= 1)
//        {
//          rf_att_on();
//        }else{
//          rf_att_off();
//        }
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      
      
      if (DRMgr.item & TPItem || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~TPItem;
        TP_Refresh((DRMgr.item & ALLItem)?1:0); // 47脚： H-1 L-0
        if (TP.level == L)
        {
          power_high_off();
        }
        else
        {
          power_high_on();
        }
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if (DRMgr.item & TXRXItem || DRMgr.item & ALLItem)
      {
        DRMgr.item &= ~TXRXItem;
        if( RF_IsRx() )
        {
          oled_txrx("RX");
          if(FREQ.item & RITItem && !(FREQ.item & MEMItem))
          {
            lo_update(Get_DDSLO(FREQ.val+VFO.RIT), MENU.ref);
            bfo_update(Get_DDSBFO(), MENU.ref);
          }
          else if(FREQ.item & XITItem && !(FREQ.item & MEMItem))     //BG6RDF
          {
            lo_update(Get_DDSLO(FREQ.val), MENU.ref);
            bfo_update(Get_DDSBFO(), MENU.ref);
          }
          else                                                       //BG6RDF
          {
            lo_update(Get_DDSLO(FREQ.val), MENU.ref);
            bfo_update(Get_DDSBFO(), MENU.ref);
          }
        }
        else
        {
          oled_txrx("TX");
          if(FREQ.item & RITItem && !(FREQ.item & MEMItem))
          {
            lo_update(Get_DDSLO(FREQ.val), MENU.ref);
            bfo_update(Get_DDSBFO(), MENU.ref);
          }
          else if(FREQ.item & XITItem && !(FREQ.item & MEMItem))     //BG6RDF
          {
            lo_update(Get_DDSLO(FREQ.val+VFO.XIT), MENU.ref);
            bfo_update(Get_DDSBFO(), MENU.ref);
          }
          else                                                       //BG6RDF
          {
            lo_update(Get_DDSLO(FREQ.val), MENU.ref);
            bfo_update(Get_DDSBFO(), MENU.ref);
          }
        }
        oled_txrx_clear_grid();  // TX/RX meter area is shared.
        Tune_ResultDraw();
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if (DRMgr.item & ALLItem)
      {
        oled_update_rx_scaler();
        oled_update_tx_scaler();
      }
      
     
    }
    else  // ALL SET REFRESH
    {
      if (DRMgr.item & MEMSETItem)
      {
        DRMgr.item &= ~MEMSETItem; 
        if (FREQ.item & TSItem)
        {
          FREQ.val /= 100;
          FREQ.val *= 100;
        }
        oled_mem_set(FREQ.numOfmem, FREQ.val, MemSetAll);
        MemSetAll = 0;
        MEM_Save();
        baud_chn_ctrl(FREQ.numOfbaud);
        lo_update(Get_DDSLO(FREQ.val), MENU.ref);       //更新本振和BFO，这样在设置频道时接收可以同步
        bfo_update(Get_DDSBFO(), MENU.ref);
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if (DRMgr.item & WPMSETItem )
      {
        DRMgr.item &= ~WPMSETItem;
        oled_update_wpm(WPM.val); 
        
        if (DRMgr.item == 0)
        {
          return;
        }
      }
      
      if (DRMgr.item & MENUSETItem )
      {
        DRMgr.item &= ~MENUSETItem;
        if (FREQ.item & TSItem)
        {
          MENU_Ts(MENU.numOfmenu);
        }
        oled_menu_set(MENU.numOfmenu); 
        oled_menu_arrows_set(MENU.numOfmenu);
        lo_update(Get_DDSLO(FREQ.val), MENU.ref);
        bfo_update(Get_DDSBFO(), MENU.ref);
        
        if (DRMgr.item == 0)
        {
          return;
        }
      }
    }
    if (DRMgr.item & ALLItem)
    {
      oled_display_on();
      DRMgr.item &= ~ALLItem;
    }
  }
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




