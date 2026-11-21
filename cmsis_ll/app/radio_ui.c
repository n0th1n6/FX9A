#include "bsp_board.h"
#include "radio_ui.h"
#include "radio_board_io.h"
#include "radio_rf.h"
#include "radio_controls.h"
#include "radio_synth.h"
#include "meter_cal.h"
#include "bsp_oled_api.h"
#include "bsp_adc.h"
#include "bsp_baud.h"
#include "bsp_iambic_keyer.h"
#include "bsp_transmit_power.h"
#include "bsp_vol.h"
#include "bsp_ad9951.h"
#include "bsp_ad9834.h"

extern volatile uint32_t fx9a_ticks_10ms;

static uint32_t adc_val;
static RADIO_ModeDef Mode;
static RADIO_ModeDef MemSetMode;
static TP_LevelDef TPLevel;
static uint8_t FILFunc;
static uint8_t MemSetAll;
#define RF_ALPHA 0.9    //发射平滑系数
#define RX_ALPHA 0.9    //接收平滑系数
volatile static float last_rf_grid=0;
volatile static float last_rx_grid=0;
static bool tune_result_visible;
static uint32_t tune_result_tick;
static uint32_t tune_result_frequency;
#define TUNE_RESULT_TICKS 500u /* Five seconds on the independent 10 ms clock. */

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
  oled_tune_result(1u, RadioRF_HasSample());
  if (RadioRF_HasSample()) oled_swr_grid(SWR_DisplayGrid(RadioRF_LastSample()));
}

static void Tune_ResultExpire(void)
{
  if (!tune_result_visible) return;
  if (isRun && !RF_IsTx() && !AIMgr.item && FREQ.val == tune_result_frequency &&
      (uint32_t)(fx9a_ticks_10ms - tune_result_tick) < TUNE_RESULT_TICKS)
    return;
  tune_result_visible = false;
  /* Settings pages redraw their own screen; do not erase their contents. */
  if (isRun && !AIMgr.item) {
    oled_tune_result(0u, 0u);
    oled_txrx_clear_grid();
    DRMgr.item |= RF_IsTx() ? (SWRCHKItem | RFCHKItem) : RXCHKItem;
  }
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
        adc_val = tune_result_visible ? RadioRF_LastSample() : adc_swr();
        
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
        if (!tune_result_visible || RadioRF_HasSample()) oled_swr_grid(grid);
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

void RadioUI_TuneStarting(void)
{
  if (tune_result_visible) oled_tune_result(0u, 0u);
  tune_result_visible = false;
}

void RadioUI_TuneStopped(bool had_carrier)
{
  MODE_Refresh(1u);
  tune_result_visible = had_carrier || RadioRF_HasSample();
  tune_result_tick = fx9a_ticks_10ms;
  tune_result_frequency = FREQ.val;
}

void RadioUI_ForceMemoryRedraw(void)
{
  MemSetAll = 1;
}
