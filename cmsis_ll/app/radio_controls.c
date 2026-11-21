/* Native FX-9A front-panel key and menu controller.
 * Extracted from bsp_board.c, with unchanged control logic and key handling.
 */
#include "bsp_board.h"
#include "tune_logic.h"
#include "tuning_policy.h"
#include "meter_cal.h"
#include "settings_validation.h"
#include "radio_state.h"
#include "radio_ui.h"
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

static KEY_AttrDef key;


static void RX_AlignmentAdjust(uint8_t pos, int16_t step)
{
  int16_t value = pos == RX_USB_MENU ? MENU.rxUsbOffset : MENU.rxLsbOffset;
  value += step;
  if (value > RX_ALIGNMENT_LIMIT) value = RX_ALIGNMENT_LIMIT;
  if (value < -RX_ALIGNMENT_LIMIT) value = -RX_ALIGNMENT_LIMIT;
  if (pos == RX_USB_MENU) MENU.rxUsbOffset = value;
  else MENU.rxLsbOffset = value;
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
          if (!RadioState_Save()) {
            oled_ts((uint8_t *)"ERR");
            break; /* Keep power held so the operator can retry/recover. */
          }
          isRun = false;
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
          RadioUI_ForceMemoryRedraw();              // 强制更新设置模式 CH: HZ
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
