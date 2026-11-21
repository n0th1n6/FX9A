#include "bsp_board.h"
#include "tune_logic.h"
#include "tuning_policy.h"
#include "meter_cal.h"
#include "settings_validation.h"
#include "ll_pins.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_exti.h"
#include "bsp_frequency.h"
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

static KEY_AttrDef key;

extern KEYER_AttrDef KEYER;

DispRefreshMgr DRMgr;
AdjustItemMgr AIMgr;
FREQ_AttrDef FREQ;
RADIO_ModeDef CWMode;
// VFO永久保存
VFO_SaveDef VFO;
VOL_AttrDef VOL;
ATT_AttrDef ATT;
RADIO_AttrDef RADIO;
TP_AttrDef TP;
DC_AttrDef DC;
FIL_AttrDef FIL;
WPM_AttrDef WPM;
MENU_AttrDef MENU;
MEM_AttrDef MEM[SIZE_OF_MEM];

uint32_t adc_val;
uint8_t tmp = 0;
uint8_t MemSetAll = 0;
RADIO_ModeDef Mode;
RADIO_ModeDef MemSetMode;
TP_LevelDef TPLevel;
uint8_t NumOfMem;

bool isRun = false;

volatile bool RfIsTx = false;

volatile uint16_t hswr_timeout = 0;
static volatile uint8_t swr_trip_pending;
FIL_AttrDef CWFilter; 
/* MemSetMaxIndex is owned by the frequency/persistence module. */

#define BFO_DEFAULT_VAL 8999570         //new: RADIO_USB  old: 8998070 LSB

uint32_t BAUD[SIZE_OF_BAUD][3] = {
  {179900, 190500, 200099},//160M:      1.799.00MHZ-2.000 .99MHZ
  {349900, 365000, 400099},//80M:       3.499.00MHZ-4.000.99MHZ
  {524900, 535000, 545099},//60M:       3.499.00MHZ-3.900.99MHZ
  {699900, 705000, 730099},//40M:       6.999.00MHZ-7.300.99MHZ
  {999900, 1012500, 1015099},//30M:       9.999.00MHZ-10.150.99MHZ
  {1399900, 1417500, 1435099},//20M:       13.999.00MHZ-14.350.99MHZ
  {1806700, 1811800, 1816899},//17M:       18.067.00MHZ-18.168.99MHZ
  {2099900, 2122500, 2145099},//15M:       20.999.00MHZ-21.450.99MHZ
  {2488900, 2494000, 2499099},//12M:       24.889.00MHZ-24.990.99MHZ
  {2799900, 2885000, 2970099}//10M:       27.999.00MHZ-29.700.99MHZ
};

uint8_t CALL_BUF[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '/', '0', '9', '8', '7', '6', '5', 
  '4', '3', '2', '1', '-'};

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
void RX_AlignmentInit(void)
{
  if (MENU.rxAlignmentMagic != RX_ALIGNMENT_MAGIC ||
      MENU.rxUsbOffset < -RX_ALIGNMENT_LIMIT || MENU.rxUsbOffset > RX_ALIGNMENT_LIMIT ||
      MENU.rxLsbOffset < -RX_ALIGNMENT_LIMIT || MENU.rxLsbOffset > RX_ALIGNMENT_LIMIT)
  {
    MENU.null = CONT_TUNE_ON;
    MENU.rxUsbOffset = 0;
    MENU.rxLsbOffset = 0;
    MENU.rxAlignmentMagic = RX_ALIGNMENT_MAGIC;
  }
}

static int16_t RX_AlignmentOffset(void)
{
  if (!RF_IsRx())
    return 0;
  if (RADIO.mode == RADIO_USB)
    return MENU.rxUsbOffset;
  if (RADIO.mode == LSB)
    return MENU.rxLsbOffset;
  return 0;
}

static void RX_AlignmentAdjust(uint8_t pos, int16_t step)
{
  int16_t value = pos == RX_USB_MENU ? MENU.rxUsbOffset : MENU.rxLsbOffset;
  value += step;
  if (value > RX_ALIGNMENT_LIMIT) value = RX_ALIGNMENT_LIMIT;
  if (value < -RX_ALIGNMENT_LIMIT) value = -RX_ALIGNMENT_LIMIT;
  if (pos == RX_USB_MENU) MENU.rxUsbOffset = value;
  else MENU.rxLsbOffset = value;
}



void ResetToDefault()
{
  e2p_save_write(0x5a);
    
    MENU.rxUsbOffset = 0;
    MENU.rxLsbOffset = 0;
    MENU.rxAlignmentMagic = RX_ALIGNMENT_MAGIC;
    MENU.delay = 8;
    MENU.freqOfbuzzer = 600;
    MENU.dutyOfbuzzer = 2;
    MENU.key = 0;
    MENU.vswr = 9;
    //MENU.rbfo = 8998724;
    //MENU.rbfo = 8998870-800;
    MENU.rbfo = BFO_DEFAULT_VAL; //8998070;
//    MENU.tbfo = 9000000;
//    MENU.usbbfo = 9001500;
//    MENU.lsbbfo = 8998500;
    MENU.ref = 50000000;
    WPM.val = 15;
    MENU.cwnloOffset = -1130;       //CW窄带模式本振偏移初值  BG6RDF

    vfo_init();
    RADIO.mode = LSB;
    TP.level = L;
    for(uint8_t i=0; i< MAX_CALLSIGN_LENGTH; i++)
    {
      MENU.callSign[i] = MORSE_CODE_COUNT;
    }
    for(uint8_t i=0; i<100; i++)
    {
      MEM[i].freq = 705000;
      MEM[i].baud = 3;
      MEM[i].mode = RADIO_USB;
    }
    
    VOL.val = 2;
    e2p_mem_write(0, MEM, 100);
    e2p_vfo_write(&VFO);
    e2p_fil_write(FIL.func);
    e2p_vol_write(VOL.val);
    e2p_att_write(ATT.val);
    e2p_pwm_write(WPM.val);
    e2p_menu_write(&MENU);
}

/* Validate unversioned EEPROM values before they can index band/memory
 * tables or program synthesizers. Recover in RAM without silently rewriting
 * the EEPROM; a future version/CRC format can distinguish missing reads.
 */
static void sanitize_saved_settings(void)
{
  VFO.CurrBaudOfVfoA = fx9a_saved_band(VFO.CurrBaudOfVfoA,
                                      SIZE_OF_BAUD, 3u);
  VFO.CurrBaudOfVfoB = fx9a_saved_band(VFO.CurrBaudOfVfoB,
                                      SIZE_OF_BAUD, 3u);
  for (uint8_t band = 0; band < SIZE_OF_BAUD; ++band) {
    VFO_SaveInfo *a = &VFO.VFOA[band];
    VFO_SaveInfo *b = &VFO.VFOB[band];
    if (!fx9a_saved_frequency(a->freq, BAUD[0][0],
                              BAUD[SIZE_OF_BAUD-1u][2]))
      a->freq = BAUD[band][1];
    if (!fx9a_saved_frequency(b->freq, BAUD[0][0],
                              BAUD[SIZE_OF_BAUD-1u][2]))
      b->freq = BAUD[band][1];
    if (!fx9a_mode_valid(a->mode)) a->mode = band < 3u ? LSB : RADIO_USB;
    if (!fx9a_mode_valid(b->mode)) b->mode = band < 3u ? LSB : RADIO_USB;
  }
  for (uint8_t i = 0; i < SIZE_OF_MEM; ++i) {
    if (!fx9a_saved_frequency(MEM[i].freq, BAUD[0][0],
                              BAUD[SIZE_OF_BAUD-1u][2]))
      MEM[i].freq = BAUD[3][1];
    MEM[i].baud = fx9a_saved_band(MEM[i].baud, SIZE_OF_BAUD, 3u);
    if (!fx9a_mode_valid(MEM[i].mode)) MEM[i].mode = RADIO_USB;
  }
  if (FIL.func != W && FIL.func != N) FIL.func = W;
  VOL.val = fx9a_saved_volume(VOL.val);
  WPM.val = fx9a_saved_wpm(WPM.val);
  ATT.val = ATT.val ? 1u : 0u;
  MENU.vswr = fx9a_saved_swr(MENU.vswr);
  MENU.delay = fx9a_saved_delay(MENU.delay);
  if (MENU.cwMode > 1u) MENU.cwMode = AUTO;
  if (MENU.key > 1u) MENU.key = 0u;
  if (MENU.freqOfbuzzer < 500u || MENU.freqOfbuzzer > 1200u)
    MENU.freqOfbuzzer = 600u;
  if (MENU.ref < 1000000u || MENU.ref > 100000000u)
    MENU.ref = 50000000u;
  if (MENU.cwnloOffset > MAX_CWN_LO_OFFSET ||
      MENU.cwnloOffset < -MAX_CWN_LO_OFFSET)
    MENU.cwnloOffset = -1130;
}

void software_init()
{
  //e2p_save_write(0x11);

  if( e2p_save_read() == 0x5a)
  {
    e2p_mem_read(0, MEM, 100);
    VFO = e2p_vfo_read();
    FIL.func = e2p_fil_read();
    VOL.val = e2p_vol_read();
    ATT.val = e2p_att_read();
    WPM.val = e2p_pwm_read();
    MENU = e2p_menu_read();
    sanitize_saved_settings();
  }
  else  // 保存默认配置
  {
    ResetToDefault();
  }
  RX_AlignmentInit();
  CWFilter.func = FIL.func;

  
 
  FREQ.item |=  VFOItem + VFOAItem;
  FREQ.numOfbaud = VFO.CurrBaudOfVfoA;
  VFO_Get();
  
  // 全部刷新
  DRMgr.item |= ALLItem;
}

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

void MENU_SetUp(uint8_t pos)
{
  switch(pos)
  {
  case 0:
    if( MENU.key == 0)
    {
      MENU.key = 1;
    }
    break;
  case 1:
    if( MENU.delay >= 15)
    {
      MENU.delay = 15;
    }
    else
    {
      MENU.delay ++;
    }
    break;
  case 2:
    if( MENU.freqOfbuzzer >= 1200)
    {
      MENU.freqOfbuzzer = 1200;
    }
    else
    {
      MENU.freqOfbuzzer ++;
    }
    break;
  case 3:
    if( MENU.cwMode == 0)
    {
      MENU.cwMode = 1;
    }
    break;
  case 4:
    if( MENU.vswr >= 9)
    {
      MENU.vswr = 9;
    }
    else
    {
      MENU.vswr ++;
    }
    break;
 
  case 5:
    if( MENU.rbfo >= 9999999)
    {
      MENU.rbfo = 9999999;
    }
    else
    {
      if (FREQ.item & TSItem)
      {
        MENU.rbfo += 100; // 步进100HZ
      }
      else
      {
        MENU.rbfo ++;
      }
    }
    break;
  case 6:
    if( MENU.dutyOfbuzzer >= 29)
    {
      MENU.dutyOfbuzzer = 29;
    }
    else
    {
      MENU.dutyOfbuzzer ++;
    }
    break;
  case 7:
    if( MENU.ref >= 99999999)
    {
      MENU.ref = 99999999;
    }
    else
    {
      if (FREQ.item & TSItem)
      {
        MENU.ref += 100; // 步进100HZ
      }
      else
      {
        MENU.ref ++;
      }
    }
    break;
  case 8:       //CW Narrow Band LO Offset
    if (FREQ.item & TSItem)
      MENU.cwnloOffset+=100;
    else
      MENU.cwnloOffset++;
    if (MENU.cwnloOffset>MAX_CWN_LO_OFFSET)
      MENU.cwnloOffset=MAX_CWN_LO_OFFSET;
    break;
  case CONT_TUNE_MENU:
    MENU.null = CONT_TUNE_ON;
    break;
  case RX_USB_MENU:
  case RX_LSB_MENU:
    RX_AlignmentAdjust(pos, (FREQ.item & TSItem) ? 100 : 10);
    break;
  default:
    if (pos >= METER_CAL_FIRST && pos <= METER_CAL_LAST) {
      MeterCal_Adjust(pos, (FREQ.item & TSItem) ? 10 : 1);
      break;
    }
    if (pos < 9 || pos >= RX_USB_MENU) break;
    if(MENU.callSign[pos - 9] == MORSE_CODE_COUNT)
    {
      MENU.numOfcall = 0;
    }
    else if(MENU.numOfcall >= MORSE_CODE_COUNT)
    {
      MENU.numOfcall = 0;
    }
    else
    {
      MENU.numOfcall ++;
    }
    MENU.callSign[pos - 9] = MENU.numOfcall; 
    break;
  }
}

void MENU_SetDown(uint8_t pos)
{
  switch(pos)
  {
  case 0:
    if( MENU.key == 1)
    {
      MENU.key = 0;
    }
    break;
  case 1:
    if( MENU.delay <= 1)
    {
      MENU.delay = 1;
    }
    else
    {
      MENU.delay --;
    }
    break;
  case 2:
    if( MENU.freqOfbuzzer <= 500)
    {
      MENU.freqOfbuzzer = 500;
    }
    else
    {
      MENU.freqOfbuzzer --;
    }
    break;
  case 3:
    if( MENU.cwMode == 1)
    {
      MENU.cwMode = 0;
    }
    break;
  case 4:
    if( MENU.vswr <= 0)
    {
      MENU.vswr = 0;
    }
    else
    {
      MENU.vswr --;
    }
    break;

  case 5:
    if( MENU.rbfo <= 0)
    {
      MENU.rbfo = 0;
    }
    else
    {
      if (FREQ.item & TSItem)
      {
        if( MENU.rbfo >= 100)
        {
          MENU.rbfo -= 100; // 步进100HZ
        }
      }
      else
      {
        MENU.rbfo --;
      }
    }
    break;
  case 6:
    if( MENU.dutyOfbuzzer <= 1)
    {
      MENU.dutyOfbuzzer = 1;
    }
    else
    {
      MENU.dutyOfbuzzer --;
    }
    break;
  case 7:
    if( MENU.ref <= 0)
    {
      MENU.ref = 0;
    }
    else
    {
      if (FREQ.item & TSItem)
      {
        if( MENU.ref >= 100)
        {
          MENU.ref -= 100; // 步进100HZ
        }
      }
      else
      {
        MENU.ref --;
      }
    }
    break;
  case 8:       //CW Narrow Band LO Offset
    if (FREQ.item & TSItem)
      MENU.cwnloOffset-=100;
    else
      MENU.cwnloOffset--;
    if (MENU.cwnloOffset<(-MAX_CWN_LO_OFFSET))
      MENU.cwnloOffset=-MAX_CWN_LO_OFFSET;
    break;
  case CONT_TUNE_MENU:
    MENU.null = CONT_TUNE_OFF;
    break;
  case RX_USB_MENU:
  case RX_LSB_MENU:
    RX_AlignmentAdjust(pos, (FREQ.item & TSItem) ? -100 : -10);
    break;
  default:
    if (pos >= METER_CAL_FIRST && pos <= METER_CAL_LAST) {
      MeterCal_Adjust(pos, (FREQ.item & TSItem) ? -10 : -1);
      break;
    }
    if (pos < 9 || pos >= RX_USB_MENU) break;
    if(MENU.callSign[pos - 9] == MORSE_CODE_COUNT)      // pos中包含了前面的菜单
    {
      MENU.numOfcall = MORSE_CODE_COUNT - 1;
    }
    else if(MENU.numOfcall <= 0)
    {
      MENU.numOfcall = MORSE_CODE_COUNT;
    }
    else
    {
      MENU.numOfcall --;
    }
    MENU.callSign[pos  -9] = MENU.numOfcall; 
    break;
  }
}

//在设置菜单模式下按下TS按钮，调用本函数
void MENU_Ts(uint8_t pos)
{
  switch(pos)
  {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
    FREQ.item &= ~TSItem;
    oled_ts("   ");
    break;
  case 5:
    MENU.rbfo /= 100;
    MENU.rbfo *= 100;
    break;
  case 6: //CW-VOL
    FREQ.item &= ~TSItem;
    oled_ts("   ");
    break;
//  case 6:
//    MENU.tbfo /= 100;
//    MENU.tbfo *= 100;
//    break;
//  case 7:
//    MENU.usbbfo /= 100;
//    MENU.usbbfo *= 100;
//    break;
//  case 8:
//    MENU.lsbbfo /= 100;
//    MENU.lsbbfo *= 100;
//    break;
  case 7:
    MENU.ref /= 100;
    MENU.ref *= 100;
    break;
  
  case RX_USB_MENU:
  case RX_LSB_MENU:
    /* TS changes the step size; do not round or alter the saved offset. */
    break;
  default:
    FREQ.item &= ~TSItem;
    oled_ts("   ");
    break;
  }
}


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

uint32_t Get_DDSLO(uint32_t freq)
{
  if (Tune_IsActive()) return freq * 10u;
  int32_t tmp;
  if( RADIO.mode == RADIO_USB)
  {
    tmp = freq * 10 + 1500; // 主频频率 倍10
  }
  else if(RADIO.mode == LSB)
  {
    tmp = freq * 10 - 1500;
  }
  else  //CW
  {
    if( RF_IsTx() )
    {
      tmp = freq * 10;
    }
    else
    {
      if( FIL.func == N )
      {
        //CW窄带模式下加上本振偏移量，用于补偿晶体滤波器中心频率偏移
        tmp = freq * 10 + MENU.cwnloOffset;     //BG6RDF
      }
      else
      {
        tmp = freq * 10 + 800;
      }
    }
  }
  
  /* Shift the passband without shifting the displayed RF tuning. */
  tmp += RX_AlignmentOffset();
  return tmp;
}

uint32_t Get_DDSBFO()
{
  if (Tune_IsActive()) return 0u;
  uint32_t tmp;
  if( RADIO.mode == RADIO_USB)
  {
    //tmp = MENU.usbbfo; 
    tmp = 9001500 + RX_AlignmentOffset();
  }
  else if(RADIO.mode == LSB)
  {
    //tmp = MENU.lsbbfo;
    tmp = 8998500 + RX_AlignmentOffset();
  }
  else
  {
    if( RF_IsRx() )
    {
      if( FIL.func == N )
      {
        tmp = MENU.rbfo;
      }
      else
      {
        tmp = 9001500;
      }
    }
    else
    {
      tmp = 0;  // TBFO 固定为零 
    }
  }
  
  return tmp;
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

void key_task()
{
  /* Any front-panel key cancels the continuous carrier; never execute that
   * key's normal radio action in the same iteration. PTT also cancels.
   */
  if (Tune_IsActive()) {
    if (PttPressed || key_ispress(&key)) {
      key_clear();
      Tune_Stop();
    }
    return;
  }
  if( RADIO.mode == RADIO_USB || RADIO.mode == LSB )
  {
    if( PttPressed && !PttFinish )
    {
      PttFinish = true;
      RF_ToTx();
    }
    else if (!PttPressed && PttFinish)
    {
      PttFinish = false;
      RF_ToRx();
      if (hswr_timeout)
      {
        hswr_timeout = 0;
        oled_h_vswr_clear();
      }
    }
  }
  
  if ( key_ispress( &key ) )
  {
    switch(key.val)
    {
    case 1:   // K1:  电源开关机按键
      if ((AIMgr.item & MENUSETItem) || RF_IsTx())
      {
        break;
      }
      
      if( key.state == keyShot)
      {
        
      }
      else if( key.state == keyLong )
      {        
        if (isRun)
        {
          isRun = false;
          
          
          e2p_mem_write(0, MEM, MemSetMaxIndex+1);
          e2p_vfo_write(&VFO);
          e2p_fil_write(FIL.func);
          e2p_vol_write(VOL.val);
          e2p_att_write(ATT.val);
          e2p_pwm_write(WPM.val);
          e2p_menu_write(&MENU);
          oled_display_off();
          delay_ms(100);
          
          power_off();
        }
      }
      
      break;
    case 2:   // V/M 信道/VFO转换键，长按储存信道功能（也是菜单退出键）
      if ((AIMgr.item & MENUSETItem) || RF_IsTx())
      {
        
      }
      else if( key.state == keyShot )
      {
        if( AIMgr.item & MEMSETItem)
        {
          AIMgr.item &= ~MEMSETItem;
          DRMgr.item |= ALLItem;
          MEM_Save();
          oled_set_screen(0x00);
        }
        else
        {
          if (FREQ.item & VFOItem)
          {
            FREQ.item &= ~VFOItem;
            FREQ.item |= MEMItem;
            DRMgr.item |= FILItem;
            MEM_Get();
          }
          else
          {
            FREQ.item &= ~MEMItem;
            FREQ.item |= VFOItem;
            
            if(FREQ.item & VFOAItem)
            {
              oled_vfo("VFO/A ");
              FREQ.numOfbaud = VFO.CurrBaudOfVfoA;
            }
            else
            {
              oled_vfo("VFO/B ");
              FREQ.numOfbaud = VFO.CurrBaudOfVfoB;
            }
            VFO_Get();
          }
          DRMgr.item |= RADIOItem;  // 更新模式
          DRMgr.item |= FREQItem;   // 更新主频和波段
          DRMgr.item |= FILItem;
        }
      }
      else if( key.state == keyLong )
      {
        if ((FREQ.item & VFOItem) && !AIMgr.item) {
          Tune_Start(); /* Long V/M when in ordinary VFO mode. */
        }
        else if(FREQ.item & MEMItem)
        {
          MEM_Get();
          oled_set_screen(0x00);
          
          AIMgr.item |= MEMSETItem;
          DRMgr.item |= MEMSETItem;   // 更新MEM设置主频
          DRMgr.item |= RADIOItem;    // 更新MEM设置模式
          DRMgr.item |= FILItem;
          MemSetAll = 1;              // 强制更新设置模式 CH: HZ
          if (FREQ.item & TSItem)
          {
            oled_ts("TS");
          }
        }
      }
      
      break;
    case 3:   // MODE键（每按一次在LSB,RADIO_USB,CW之间转换）长按切换滤波器W/N
      if ((AIMgr.item & MENUSETItem) || RF_IsTx())
      {
        
      }
      else if( key.state == keyShot )
      {
        if (RADIO.mode == LSB)
        {
          if( CWMode == CWA )
          {
            RADIO.mode = CWA;
          }
          else
          {
            RADIO.mode = CWB;
          }
        }
        else if (RADIO.mode == RADIO_USB)
        {
          RADIO.mode = LSB;         
        }
        else
        {
          RADIO.mode = RADIO_USB;
        }
        
        if( AIMgr.item & MEMSETItem)
        {
          DRMgr.item |= MEMSETItem;
        }
        else
        {
          DRMgr.item |= FREQItem;   // 更新主频
        }
        DRMgr.item |= RADIOItem;
        DRMgr.item |= FILItem;
        
      }
      else if( key.state == keyLong)
      {
        if (RADIO.mode == CWA || RADIO.mode == CWB)
        {
          if (FIL.func == W)
          {
            FIL.func = N;
            CWFilter.func = N;
          }
          else 
          {
            FIL.func = W;
            CWFilter.func = W;
          }
          DRMgr.item |= FILItem;
          if( !(AIMgr.item & MEMSETItem) )  // MEMSET下按 N 出现小CH:09
          {
            DRMgr.item |= FREQItem; // 更新BFO和LO
          }
        }
        
      }
      break;
    case 4:   // A/B键，在VFO模式下短按是VFOA/VFOB之间转换。长按是CWA/CWB之间转换（只在CW模式下）
      if ((AIMgr.item & MENUSETItem) || RF_IsTx())
      {
        
      }
      else if( key.state == keyShot && AIMgr.item <= 0)
      {
        if(FREQ.item & VFOItem)
        {
          if (FREQ.item & VFOBItem)
          {
            FREQ.item |= VFOAItem; 
            FREQ.item &= ~VFOBItem;
            oled_vfo("VFO/A ");
            FREQ.numOfbaud = VFO.CurrBaudOfVfoA;
          }
          else
          {
            FREQ.item |= VFOBItem; 
            FREQ.item &= ~VFOAItem;
            oled_vfo("VFO/B ");
            FREQ.numOfbaud = VFO.CurrBaudOfVfoB; 
          }
          VFO_Get();
          DRMgr.item |= FREQItem;
          DRMgr.item |= RADIOItem; 
          DRMgr.item |= FILItem;
        }
      }
      else if( key.state == keyLong )
      {
        if( RADIO.mode == CWA || RADIO.mode == CWB)
        {
          if (RADIO.mode == CWA)
          {
            CWMode = CWB;
            RADIO.mode = CWB;
          }else {
            RADIO.mode = CWA;
            CWMode = CWA;
          }
          if( AIMgr.item & MEMSETItem)
          {
            DRMgr.item |= MEMSETItem;
          }
          DRMgr.item |= RADIOItem;  
        }
      }
      break;
    case 5:   // 波段按键，每短按一次在所有波段之间转换。长按进入菜单
      if ( RF_IsTx())
      {
        
      }
      else if( key.state == keyShot)
      {     
        if( AIMgr.item & MENUSETItem)           // exit settings and save calibration
        {
          MeterCal_Save();
          AIMgr.item &= ~MENUSETItem;
          DRMgr.item |= ALLItem;
          buzzer_stop();
          QuietModeOff();
          oled_set_screen(0x00);                //清屏
        }
        else
        // XIT BG6RDF
        if((FREQ.item & VFOItem) && (AIMgr.item <= 0) && !(FREQ.item & XITItem))
        {
          FREQ.item |= XITItem;         //进入XIT
          FREQ.item &= ~RITItem;        //XIT与RIT互斥
          oled_rit("XIT");
          
          DRMgr.item |= FREQItem;
        }
        else if (FREQ.item & XITItem)
        {
          FREQ.item &= ~XITItem;        //退出XIT
          oled_rit("   ");
          oled_rit_txfreq_clear();
          DRMgr.item |= FREQItem;
        } 
        /*      //原来这里是波段切换按钮，现改为XIT按钮  BG6RDF
        else if (!(FREQ.item & MEMItem) || AIMgr.item & MEMSETItem)
        {
          if (FREQ.numOfbaud >= SIZE_OF_BAUD-1)
          {
            FREQ.numOfbaud = 0;
          }
          else
          {
            FREQ.numOfbaud ++; 
          }

          if( AIMgr.item & MEMSETItem)
          {
            FREQ.val = BAUD[FREQ.numOfbaud][1];
            DRMgr.item |= MEMSETItem;
          }
          else
          {
            VFO_Get();
            DRMgr.item |= FREQItem;
            DRMgr.item |= RADIOItem; 
            DRMgr.item |= FILItem;
          }
        }
        */
        
      }
      else if( key.state == keyLong )
      {
        oled_set_screen(0x00);
        
        AIMgr.item |= MENUSETItem;
        DRMgr.item |= MENUSETItem;
        MENU.numOfmenu = 0; // 让菜单全部显示更新
        if (FREQ.item & TSItem)
        {
          oled_ts("TS");
        }
      }
      break;
    case 6:   // RIT按键，短按是快速调TS键，长按进入RIT，退出也长按
      if (RF_IsTx())
      {
        
      }
      else if( key.state == keyShot )
      {
        if (FREQ.item & TSItem)
        {
          FREQ.item &= ~TSItem;
          
          oled_ts("   ");
        }
        else  // 3种状态下：信道设置,菜单设置,VFO/MEM
        {     // 作用：清零及刷新TS显示
          FREQ.item |= TSItem;          
          oled_ts("TS");
          
          if( AIMgr.item & MEMSETItem)
          {
            DRMgr.item |= MEMSETItem;
          }
          else if( AIMgr.item & MENUSETItem)
          {            
            DRMgr.item |= MENUSETItem;
          }
          else
          {
            DRMgr.item |= FREQItem;
          }
        }
      }
      else if( key.state == keyLong)
      {
        // 只在VFO模式下起作用
        if((FREQ.item & VFOItem) && (AIMgr.item <= 0) && !(FREQ.item & RITItem))
        {
          FREQ.item |= RITItem;
          FREQ.item &= ~XITItem;        //XIT与RIT互斥     BG6RDF
          oled_rit("RIT");
          
          DRMgr.item |= FREQItem;
        }
        else if (FREQ.item & RITItem)
        {
          FREQ.item &= ~RITItem;
          oled_rit("   ");
          oled_rit_txfreq_clear();
          DRMgr.item |= FREQItem;
        }
      }
      
      break;

    case 7:   // CH信道旋钮，	短按ATT设置，长按进入CALL呼叫，在呼叫过程中短按一下停止（中断）呼叫
      if (AIMgr.item & MENUSETItem)
      {
        
      }
      else if( key.state == keyShot )
      {
        if(FREQ.item & CALLItem)
        {
          FREQ.item &= ~CALLItem;
          call_exit();
          // 转到RX模式
          RF_ToRx();
        }
        else
        {
          if (RADIO.mode == CWA || RADIO.mode == CWB)
          {
            FREQ.item |= CALLItem;
            call_enter();
          }
        }
        
      }
      else if( key.state == keyLong )
      {
        if (ATT.val >= 1)
        {
          ATT.val = 0;
        }else{
          ATT.val = 1;
        }
        DRMgr.item |= ATTItem;
      }
      break;
    case 8:   // VOL音量调整，短按功率设置H/M/L长按进入CW速度调整，转动音量编码器调整WPM
      if ((AIMgr.item & MENUSETItem) || (AIMgr.item & MEMSETItem) || RF_IsTx())
      {
        
      }
      else if( key.state == keyShot )
      {
        if (AIMgr.item > 0)
        {
          if( AIMgr.item == WPMSETItem )
          {
            AIMgr.item &= ~WPMSETItem;
            oled_close_wpm();  
          }
        }
        else
        {
          //if (RADIO.mode == CWA || RADIO.mode == CWB) // 功率调整 只有在CW模式下才生效
          {
            if (TP.level == L)
            {
              TP.level = H;
              TP.val = " ";
            }
            else
            {
              TP.level = L;
              TP.val = "L";
            }
            DRMgr.item |= TPItem;
          }
        }
      }
      else if(key.state == keyLong)
      {
        AIMgr.item |= WPMSETItem;
        DRMgr.item |= WPMSETItem;
      }
      break;
    }
    
    key_clear();
  }
}

void UART_Task()
{
  usart_process_rx_commands();
  uint8_t size = 0;
  if (DMA_IsTxComplete())
  {
    size = UART_TxBufSize(); 
    if(size > 0){
      // 串口发送处理
      CopyTBufToDMA();
      UART_DMAStart( size );	
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
  
  UART_Task();
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




