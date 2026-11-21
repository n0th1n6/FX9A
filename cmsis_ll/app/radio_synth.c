/* LO/BFO frequency calculations extracted from the native LL board.
 * No changes to RF tuning arithmetic or RX alignment settings.
 */
#include "bsp_board.h"
#include "radio_synth.h"

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
