#include "bsp_key.h"
#include "ll_pins.h"
#include "vfo_accel.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_exti.h"
#include "bsp_encoder.h"
#include "bsp_timer.h"
#include "bsp_adc.h"
#include "bsp_oled_api.h"

// 老版本
//开关        PD6/87/K1  PD5/86/K2  PD7/88/K3  PD4/85/K4  PD0/81/K5  PD1/82/K6  
//开关        PC12/80/K7  PC11/79/K8  PC10/78/K9  PA15/77/K10 
//编码器      PB4/90/K1-A  PB3/89/K1-B  PA12/71/K9-A  PA11/70/K9-B  PA10/69/K10-A  PA9/68/K10-B
//光电编码器  PD2/83/R1-A  PD3/84/R1-B

// 新版本
//开关        PB4/90/K1   PD6/87/K3 
//编码器      PA12/71/K1-A  PA11/70/K1-B
//声音编码器  PA10/69/K2-A  PA9/68/K2-B   -> 后期 改到 57/PD10 58/PD11脚
//光电编码器  PD2/83/R1-A  PD3/84/R1-B
//PE12/43/KEY1 是点控制。PE13/44/KEY2是划控制
//PB0/35脚    PTT开关检测端（此脚使用电阻上拉到3.3V，在SSB模式下，按下话筒开关，开 
//        关把此脚对地短路，把35脚电平拉低。单片机控制相应的电路发射，此脚只在SSB模式下起作用）
#define PTT GPIO_BSRR_BS0
#define DIT GPIO_BSRR_BS12
#define DAH GPIO_BSRR_BS13

#define K1 GPIO_BSRR_BS4
#define K2 GPIO_BSRR_BS5
#define K3 GPIO_BSRR_BS6
#define K4 GPIO_BSRR_BS4
#define K5 GPIO_BSRR_BS0
#define K6 GPIO_BSRR_BS1
#define K7 GPIO_BSRR_BS12
#define K8 GPIO_BSRR_BS11

#define K1A GPIO_BSRR_BS12
#define K1B GPIO_BSRR_BS11

#define K2A GPIO_BSRR_BS10
#define K2B GPIO_BSRR_BS11

#define R1A GPIO_BSRR_BS2
#define R1B GPIO_BSRR_BS3

#define KEY_1_PORT GPIOB
#define KEY_23456R1A1BK2A2B_PORT GPIOD
#define KEY_78_PORT GPIOC
#define KEY_K1A1B_PORT GPIOA
#define PADDLE_PORT GPIOE

static KEY_AttrDef key;
static EncoderState channel_encoder;
static EncoderState volume_encoder;
static uint8_t key_scan_divider;
static uint16_t swr_sample_divider;
#ifdef FX9A_VFO_ACCEL
extern volatile uint32_t fx9a_ticks_10ms;
static Fx9aVfoAccel optical_accel;
#endif
static int32_t optical_frequency_step(int8_t sign)
{
#ifdef FX9A_VFO_ACCEL
    return fx9a_vfo_accel_step(&optical_accel, fx9a_ticks_10ms,
                               sign, (FREQ.item & TSItem) != 0u);
#else
    return sign > 0 ? ((FREQ.item & TSItem) ? 100 : 1)
                    : ((FREQ.item & TSItem) ? -100 : -1);
#endif
}

volatile uint32_t fx9a_ticks_10ms;
#ifdef FX9A_ENCODER_TRACE
/* Diagnostic only: 50 us raw contacts, normal 250 us control decoding.
 * Stop when full so ST-Link can read a consistent completed trace. */
volatile uint32_t encoder_trace[2048][2];
volatile uint32_t encoder_trace_count;
volatile uint32_t encoder_trace_enabled = 1;
static uint32_t encoder_trace_tick;
static uint8_t encoder_trace_previous = 0xff;
#endif
static void encoders_init(void);
void key_timer_callback(void);
bool key_availlble = false;
uint8_t KVal = 0;
uint8_t K9_A = 0;
uint8_t K9_B = 0;

uint8_t kVal    = 0xff; //按键标识，键值
uint16_t kTick   = 0;    //去抖时间kTick * 20毫秒
bool scan_state = true;
uint8_t tick1s = 0;
uint8_t ditUp = 0;
uint8_t ditDown = 0;
uint8_t dahUp = 0;
uint8_t dahDown = 0;
uint8_t pttUp = 0;
uint8_t pttDown = 0;

bool DitPaddle;   // DIT键是否被按下
bool DahPaddle;   // DAH键是否被按下
bool PaddleUpdate = false;
bool PttPressed;  // PTT控制键是否被按下
bool PttFinish;   // PTT按下操作是否被执行
void KEY_EXTI_Config(void)
{
  LL_EXTI_ClearFlag_0_31((1u << 2) | (1u << 10) | (1u << 11));
  /* PD2 is EXTI2; encoder EXTI10/11 remain disabled for polling. */
  AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~(0xfu << 8)) | (3u << 8);
  LL_EXTI_EnableFallingTrig_0_31(1u << 2);
  LL_EXTI_DisableRisingTrig_0_31(1u << 2);
  LL_EXTI_EnableIT_0_31(1u << 2);
  LL_EXTI_DisableIT_0_31((1u << 10) | (1u << 11));
}
void GPIO_NVIC_Config(void)
{
  NVIC_SetPriorityGrouping(5u);
  NVIC_SetPriority(EXTI2_IRQn, NVIC_EncodePriority(5u, 0u, 0u));
  NVIC_EnableIRQ(EXTI2_IRQn);
}
void key_init()
{
  LL_GPIO_InitTypeDef GPIO_InitStructure = {0};
#ifdef FX9A_ENCODER_TRACE
  TMR_Init(TIM2, 20000, 100, key_timer_callback); // 50 us diagnostic contacts
#else
  TMR_Init(TIM2, 4000, 1000, key_timer_callback); // 250 us encoder sampling
#endif
  TMR_IRQEnable(TIM2);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(K1);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_FLOATING;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(KEY_1_PORT, &GPIO_InitStructure);
  (KEY_1_PORT->BRR = (K1));
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(PTT);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_INPUT; GPIO_InitStructure.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(KEY_1_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(K2 | K3 | K4 | K5 | K6 | R1A | R1B | K2A | K2B);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_FLOATING;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(KEY_23456R1A1BK2A2B_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(K7 | K8);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_FLOATING;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(KEY_78_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(K1A | K1B);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_FLOATING;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(KEY_K1A1B_PORT, &GPIO_InitStructure);
  
  GPIO_InitStructure.Pin = fx9a_ll_pins(DIT | DAH);
  GPIO_InitStructure.Mode = LL_GPIO_MODE_FLOATING;
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  LL_GPIO_Init(PADDLE_PORT, &GPIO_InitStructure);
  encoders_init();
  KEY_EXTI_Config();
  GPIO_NVIC_Config();
  
  // CW模式 ：单键 或 双键判断
  if (((PADDLE_PORT->IDR & (DIT)) != 0u) == 0)
  {
    MENU.cwMode = MANUAL;
  }
  else
  {
    MENU.cwMode = AUTO;
  }
}

uint8_t key_read()
{
  uint16_t port;
  
  port = (KEY_1_PORT->IDR);
  if( !(port & K1) )
  {
    return 1;
  }
  
  port = (KEY_23456R1A1BK2A2B_PORT->IDR);
  if ( !(port & K2) )
  {
    return 2; 
  }
  
  if( !(port & K3) )
  {
    return 3;
  }
  
  if( !(port & K4) )
  {
    return 4;
  }
  
  if( !(port & K5) )
  {
    return 5;
  }
  
  if( !(port & K6) )
  {
    return 6;
  }
  
  port = (KEY_78_PORT->IDR);
  if( !(port & K7) )
  {
    return 7;
  }
  
  if( !(port & K8) )
  {
    return 8;
  }
  uint8_t ptt = ((KEY_1_PORT->IDR & (PTT)) != 0u);
  if(ptt && PttPressed)         //以前是按下的但现在松开了
  {
    if(pttDown > 0)
    {
      pttDown = 0;
    }
    pttUp ++;
    if(pttUp >= 1)              //松开防抖，原值1      BG6RDF
    {
      pttUp = 0;
      PttPressed = false;

    }
  }
  else if(!ptt && !PttPressed ) //以前是松开的但现在按下了
  {
    if(pttUp > 0)
    {
      pttUp = 0;
    }
    pttDown ++;
    if(pttDown >= 1)            //按下防抖，原值1      BG6RDF
    {
      pttDown = 0;
      PttPressed = true;
    }
  }
  
  tick1s ++;
  if(tick1s % 40 == 0)          //原值10      BG6RDF
  {
    if( RF_IsRx() )
    {
      DRMgr.item |= RXCHKItem;
    }
    else
    {
      DRMgr.item |= SWRCHKItem;
      DRMgr.item |= RFCHKItem;
    }
  }
  if(tick1s == 200)             //原值50      BG6RDF
  {
    tick1s = 0;
    DRMgr.item |= DCItem;
  }
  
  port = (PADDLE_PORT->IDR);

  if(port & DIT)
  {
    if(ditDown > 0)
    {
      ditDown = 0;
    }
    ditUp ++;
    if(ditUp >= 1)      //点松开防抖，原值1      BG6RDF
    {
      ditUp = 0;
      DitPaddle = false;
    }
  }
  else
  {
    if(ditUp > 0)
    {
      ditUp = 0;
    }
    ditDown ++;
    if(ditDown >= 1)    //点按下防抖，原值1      BG6RDF
    {
      ditDown = 0;
      DitPaddle = true;
    }
  }
  if(port & DAH)
  {
    if(dahDown > 0)
    {
      dahDown = 0;
    }
    dahUp ++;
    if(dahUp >= 1)      //划松开防抖，原值1      BG6RDF
    {
      dahUp = 0;
      DahPaddle = false;
    }
  }
  else
  {
    if(dahUp > 0)
    {
      dahUp = 0;
    }
    dahDown ++;
    if(dahDown >= 1)    //划按下防抖，原值1      BG6RDF
    {
      dahDown = 0;
      DahPaddle = true;
    }
  }
  
  PaddleUpdate = true;  //用于CW模式的 Paddle更新
  
  return 0xff;
}

void key_readkeyer()
{
  uint16_t port;
  
  port = (PADDLE_PORT->IDR);
  if(port & DIT)
  {
    DitPaddle = false;
  }
  else
  {
    DitPaddle = true;
  }
  if(port & DAH)
  {
    DahPaddle = false;
  }
  else
  {
    DahPaddle = true;
  }
//  if(port & DIT)
//  {
//    if(ditDown > 0)
//    {
//      ditDown = 0;
//    }
//    ditUp ++;
//    if(ditUp >= 1)
//    {
//      ditUp = 0;
//      DitPaddle = false;
//    }
//  }
//  else
//  {
//    if(ditUp > 0)
//    {
//      ditUp = 0;
//    }
//    ditDown ++;
//    if(ditDown >= 1)
//    {
//      ditDown = 0;
//      DitPaddle = true;
//    }
//  }
//  if(port & DAH)
//  {
//    if(dahDown > 0)
//    {
//      dahDown = 0;
//    }
//    dahUp ++;
//    if(dahUp >= 1)
//    {
//      dahUp = 0;
//      DahPaddle = false;
//    }
//  }
//  else
//  {
//    if(dahUp > 0)
//    {
//      dahUp = 0;
//    }
//    dahDown ++;
//    if(dahDown >= 1)
//    {
//      dahDown = 0;
//      DahPaddle = true;
//    }
//  }
  
  PaddleUpdate = true;  //用于CW模式的 Paddle更新
}

bool key_ispress(KEY_AttrDef *rkey)
{
  *rkey = key;
  return key_availlble; 
}

void key_clear()
{
  key_availlble = false;
}

uint8_t upCnt = 0;
void key_calback( void  )
{
  uint8_t kTmp = key_read();
  
  if(kVal == 0xff)      //之前状态无按键
  {
    kVal = kTmp;
    key.state = keyNull;
    kTick = 0;
    return;
  }
  
  if(kTmp != kVal)      //和前次按键不同
  {
    upCnt ++;
    if (upCnt >= 8)     //防抖    原值是2    BG6RDF
    {
      upCnt = 0;
      if(kTmp == 0xff)    //短按弹起
      {
        if( key.state == keyShot)
        {
          key_availlble = true; 
        } 
      }
      kVal = kTmp;        //保存新按键值
      kTick = 0;
      return;
    }
  }
  else
  {
    kTick ++;
    if( upCnt > 0 )
    {
      upCnt = 0;
    }
    if(kTick == 16)          //20ms * 4 = 80ms，原值是4   BG6RDF
    {
      key.state = keyShot;
      key.val = kVal;
    }  
    
    if(kTick == 200)      //按住2秒后，原值50，为啥是2秒 20mS*50=1S?  BG6RDF
    {
      key.val = kVal;
      key.state = keyLong;
      key_availlble = true; 
    }
  }
}

void KEY_SoftwareSet(uint8_t kval, KEY_State state)
{
  if( key_availlble == false)
  {
    key.val = kval;
    key.state = state;
    key_availlble = true; 
  }
}

// 主编码器，没有防抖
// R1 - line_2
void EXTI2_IRQHandler(void)
{
  if (((EXTI->PR & EXTI->IMR & (1u << 2)) != 0u) == SET)
  {
    // 锁定状态
    if ( ATT.val )
    {
      LL_EXTI_ClearFlag_0_31(1u << 2);
      return;
    }

    if (((KEY_23456R1A1BK2A2B_PORT->IDR & (R1B)) != 0u) == 0) // A-下降沿 B-高电平 为正
    {
      // VFO和MEM模式下，调频旋钮都可用，但在MEM模式下 不记忆
      //if (AIMgr.item > 0)
      {
        if(AIMgr.item & MENUSETItem)
        {
          MENU_SetUp(MENU.numOfmenu);
        }
        else if ((FREQ.item & RITItem) && !(AIMgr.item))
        {
          if (VFO.RIT >= UPPER_OF_RIT)
          {
            VFO.RIT = UPPER_OF_RIT;
          }
          else
          {
            if (FREQ.item & TSItem)
            {
              if ((VFO.RIT + 100) < UPPER_OF_RIT)
              {
                VFO.RIT += 100;
              }
            }
            else
            {
              VFO.RIT ++;
            }
          }
        }
        else if ((FREQ.item & XITItem) && !(AIMgr.item))      //XIT BG6RDF
        {
          if (VFO.XIT >= UPPER_OF_XIT)
          {
            VFO.XIT = UPPER_OF_XIT;
          }
          else
          {
            if (FREQ.item & TSItem)
            {
              if ((VFO.XIT + 100) < UPPER_OF_XIT)
              {
                VFO.XIT += 100;
              }
            }
            else
            {
              VFO.XIT ++;
            }
          }
        }
        else
        {
          Frequency_Tune(optical_frequency_step(1));
        }
        if (AIMgr.item & MENUSETItem)
        {
          DRMgr.item |= MENUSETItem;
        }
        else if (AIMgr.item & MEMSETItem)
        {
          DRMgr.item |= MEMSETItem;
        }
        else
        {
          DRMgr.item |= FREQItem;
        }
      }
    }
    else
    {
      //if (AIMgr.item > 0)
      {
        if(AIMgr.item & MENUSETItem)
        {
          MENU_SetDown(MENU.numOfmenu);
        }
        else if ((FREQ.item & RITItem) && !(AIMgr.item))  // RIT选中时，且在非设置状态下 起作用
        {
          if (VFO.RIT <= LOWER_OF_RIT)
          {
            VFO.RIT = LOWER_OF_RIT;
          }
          else
          {
            if (FREQ.item & TSItem)
            {
              if((VFO.RIT - 100) >= LOWER_OF_RIT)
              {
                VFO.RIT -= 100;
              }
            }
            else
            {
              VFO.RIT --;
            }
          }
        }
        else if ((FREQ.item & XITItem) && !(AIMgr.item)) //XIT BG6RDF
        {
          if (VFO.XIT <= LOWER_OF_XIT)
          {
            VFO.XIT = LOWER_OF_XIT;
          }
          else
          {
            if (FREQ.item & TSItem)
            {
              if((VFO.XIT - 100) >= LOWER_OF_XIT)
              {
                VFO.XIT -= 100;
              }
            }
            else
            {
              VFO.XIT --;
            }
          }
        }
        else
        {
          Frequency_Tune(optical_frequency_step(-1));
        }

        if (AIMgr.item & MENUSETItem)
        {
          DRMgr.item |= MENUSETItem;
        }
        else if (AIMgr.item & MEMSETItem)
        {
          DRMgr.item |= MEMSETItem;
        }
        else
        {
          DRMgr.item |= FREQItem;
        }
      }
    }
    LL_EXTI_ClearFlag_0_31(1u << 2);
  }
}

/* Keep the previous physical direction: CH used B as its interrupt phase,
 * while VOL used A. Reading each port once captures a coherent phase pair. */
static uint8_t channel_phases(uint16_t port)
{
  return ((port & K1B) ? 2 : 0) | ((port & K1A) ? 1 : 0);
}

static uint8_t volume_phases(uint16_t port)
{
  return ((port & K2A) ? 2 : 0) | ((port & K2B) ? 1 : 0);
}

static void encoders_init(void)
{
  encoder_init(&channel_encoder,
               channel_phases((KEY_K1A1B_PORT->IDR)));
  encoder_init(&volume_encoder,
               volume_phases((KEY_23456R1A1BK2A2B_PORT->IDR)));
  key_scan_divider = 0;
}

static void volume_encoder_step(int8_t step)
{
  if ((AIMgr.item & MENUSETItem) || RF_IsTx())
    return;
  if (step > 0)
  {
    if (AIMgr.item == WPMSETItem)
    {
      if (WPM.val >= 60)
      {
        WPM.val = 60;
      }
      else
      {
        WPM.val ++;
      }
      DRMgr.item |= WPMSETItem;
    }
    else if (AIMgr.item & MEMSETItem)         //信道模式下，K2用于选择波段
    {
      if (FREQ.numOfbaud >= SIZE_OF_BAUD-1)
      {
        FREQ.numOfbaud = 0;
      }
      else
      {
        FREQ.numOfbaud ++;
      }
      FREQ.val=BAUD[FREQ.numOfbaud][1];
      DRMgr.item |= MEMSETItem;
    }
    else
    {
      if(VOL.val >= 60)
      {
        VOL.val = 60;
      }else{
        VOL.val ++;
      }
      DRMgr.item |= VOLItem;
    }
  }
  else
  {
    if (AIMgr.item == WPMSETItem)
    {
      if (WPM.val <= 4)
      {
        WPM.val = 4;
      }
      else
      {
        WPM.val --;
      }
      DRMgr.item |= WPMSETItem;
    }
    else if (AIMgr.item & MEMSETItem)         //信道模式下，K2用于选择波段
    {
      if (FREQ.numOfbaud == 0)
      {
        FREQ.numOfbaud = SIZE_OF_BAUD-1;
      }
      else
      {
        FREQ.numOfbaud --;
      }
      FREQ.val=BAUD[FREQ.numOfbaud][1];
      DRMgr.item |= MEMSETItem;
    }
    else
    {
      if(VOL.val <= 0)
      {
        VOL.val = 0;
      }else{
        VOL.val --;
      }
      DRMgr.item |= VOLItem;
    }
  }
}

static void channel_encoder_step(int8_t step)
{
  if (RF_IsTx() || (!(AIMgr.item & MENUSETItem) &&
      !(FREQ.item & MEMItem) && !(FREQ.item & VFOItem)))
    return;
  if (step > 0)
  {
    if(AIMgr.item == MENUSETItem)
    {
      if (MENU.numOfmenu >= SIZE_OF_MENU-1)
      {
        MENU.numOfmenu = SIZE_OF_MENU-1;
      }
      else
      {
        MENU.numOfmenu ++;
      }
      DRMgr.item |= MENUSETItem;
    }
    else if (AIMgr.item == MEMSETItem || FREQ.item & MEMItem)
    {
      if (FREQ.numOfmem >= SIZE_OF_MEM-1)
      {
        FREQ.numOfmem = SIZE_OF_MEM-1;
      }
      else
      {
        FREQ.numOfmem ++;
      }
      //FREQ.val =MEM[FREQ.numOfmem].freq;
      MEM_Get();
      if(AIMgr.item == MEMSETItem)
      {
        DRMgr.item |= MEMSETItem;
        DRMgr.item |= RADIOItem;
      }
      else
      {
        DRMgr.item |= FREQItem;
        DRMgr.item |= RADIOItem;
      }
      DRMgr.item |= FILItem;
    }
    else if (FREQ.item & VFOItem)
    {
      if (FREQ.numOfbaud >= SIZE_OF_BAUD-1)
      {
        FREQ.numOfbaud = 0;
      }
      else
      {
        FREQ.numOfbaud ++;
      }
      VFO_Get();
      DRMgr.item |= FREQItem;
      DRMgr.item |= RADIOItem;
      DRMgr.item |= FILItem;
    }
  }
  else
  {
    if(AIMgr.item == MENUSETItem)
    {
      if (MENU.numOfmenu <= 0)
      {
        MENU.numOfmenu = 0;
      }
      else
      {
        MENU.numOfmenu --;
      }
      DRMgr.item |= MENUSETItem;
    }
    else if (AIMgr.item == MEMSETItem || FREQ.item & MEMItem)
    {
      if (FREQ.numOfmem <= 0)
      {
        FREQ.numOfmem = 0;
      }
      else
      {
        FREQ.numOfmem --;
      }
      //FREQ.val = MEM[FREQ.numOfmem].freq;
      MEM_Get();
      if(AIMgr.item == MEMSETItem)
      {
        DRMgr.item |= MEMSETItem;
        DRMgr.item |= RADIOItem;
      }
      else
      {
        DRMgr.item |= FREQItem;
        DRMgr.item |= RADIOItem;
      }
      DRMgr.item |= FILItem;
    }
    else if (FREQ.item & VFOItem)
    {
      if (FREQ.numOfbaud == 0)
      {
        FREQ.numOfbaud = SIZE_OF_BAUD-1;
      }
      else
      {
        FREQ.numOfbaud --;
      }
      VFO_Get();
      DRMgr.item |= FREQItem;
      DRMgr.item |= RADIOItem;
      DRMgr.item |= FILItem;
    }
  }
}

void key_timer_callback(void)
{
  /* Fast SWR sampling must run even if diagnostic encoder trace exits early. */
#ifdef FX9A_ENCODER_TRACE
  const uint16_t swr_interval = 200u; /* 50 us ticks */
#else
  const uint16_t swr_interval = 40u;  /* 250 us ticks */
#endif
  if (++swr_sample_divider >= swr_interval) {
    swr_sample_divider = 0;
    ++fx9a_ticks_10ms;
    RF_SwrSampleISR();
  }
  uint8_t ch_phases = channel_phases((KEY_K1A1B_PORT->IDR));
  uint8_t vol_phases = volume_phases((KEY_23456R1A1BK2A2B_PORT->IDR));
#ifdef FX9A_ENCODER_TRACE
  uint8_t raw = ch_phases | (vol_phases << 2);
  ++encoder_trace_tick;
  if (encoder_trace_enabled && raw != encoder_trace_previous)
  {
    uint32_t index = encoder_trace_count;
    if (index < 2048)
    {
      encoder_trace[index][0] = encoder_trace_tick;
      encoder_trace[index][1] = raw;
      encoder_trace_count = index + 1;
    }
    else
      encoder_trace_enabled = 0;
  }
  encoder_trace_previous = raw;
  if (encoder_trace_tick % 5)
    return;
#endif
  int8_t channel = encoder_update(&channel_encoder, ch_phases);
  int8_t volume = encoder_update(&volume_encoder, vol_phases);
  /* Always consume transitions, including while controls are inhibited. */
  if (channel)
    channel_encoder_step(channel);
  if (volume)
    volume_encoder_step(volume);

  /* Preserve the existing 5 ms button, PTT and paddle scan cadence. */
  if (++key_scan_divider == 20)
  {
    key_scan_divider = 0;
    key_calback();
  }
}
