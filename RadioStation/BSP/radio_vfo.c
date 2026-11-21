#include "radio_vfo.h"
#include "radio_state.h"
#include "bsp_frequency.h"
#include "tuning_policy.h"
#include "bsp_24lc16.h"



void RadioVFO_Init(void)
{
  // –ËºÏ≤‚E2P
  for(uint8_t i=0; i<SIZE_OF_BAUD; i++)
  {
    if( i <= 2 )
    {
      VFO.VFOA[i].mode = LSB;
      VFO.VFOB[i].mode = LSB;
    }
    else
    {
      VFO.VFOA[i].mode = USB;
      VFO.VFOB[i].mode = USB;
    }
    VFO.VFOA[i].freq = BAUD[i][1];
    VFO.VFOB[i].freq = BAUD[i][1];
  }
  VFO.RIT = 0;
  VFO.XIT = 0;
  FREQ.numOfbaud = 3;  //7050000
  VFO.CurrBaudOfVfoA = 3;
  VFO.CurrBaudOfVfoB = 3;
}

bool ContinuousTuning_Enabled(void)
{
  /* Existing EEPROM images contain an unspecified value in MENU.null.
   * Only explicit OFF disables the continuous tuning behavior.
   */
  return fx9a_continuous_on(MENU.null);
}

bool Frequency_Set(uint32_t frequency)
{
  if (!fx9a_tuning_allowed(ContinuousTuning_Enabled(), FREQ.numOfbaud,
                           frequency, BAUD, SIZE_OF_BAUD))
    return false;

  uint8_t band = frequency_band(frequency, BAUD, SIZE_OF_BAUD);
  if (band != FREQ.numOfbaud && (FREQ.item & VFOItem) &&
      !(AIMgr.item & MEMSETItem))
    VFO_Save();
  FREQ.val = frequency;
  FREQ.numOfbaud = band;
  return true;
}

void Frequency_Tune(int32_t step)
{
  bool continuous = ContinuousTuning_Enabled();
  uint32_t lower = fx9a_tuning_lower(continuous, FREQ.numOfbaud,
                                     BAUD, SIZE_OF_BAUD);
  uint32_t upper = fx9a_tuning_upper(continuous, FREQ.numOfbaud,
                                     BAUD, SIZE_OF_BAUD);
  if (upper >= lower && lower != 0u)
    Frequency_Set(frequency_step(FREQ.val, step, lower, upper));
}

void VFO_Get()
{
  if(FREQ.item & VFOBItem)
  {
    FREQ.val = VFO.VFOB[FREQ.numOfbaud].freq;
    RADIO.mode = VFO.VFOB[FREQ.numOfbaud].mode;
  }
  else
  {
    FREQ.item |= VFOAItem;
    FREQ.val = VFO.VFOA[FREQ.numOfbaud].freq;
    RADIO.mode = VFO.VFOA[FREQ.numOfbaud].mode;
  }
  FREQ.numOfbaud = frequency_band(FREQ.val, BAUD, SIZE_OF_BAUD);
}

void VFO_Save()
{
  if(FREQ.item & VFOAItem)
  {
    VFO.VFOA[FREQ.numOfbaud].freq = FREQ.val;
    VFO.VFOA[FREQ.numOfbaud].mode = RADIO.mode;
    VFO.CurrBaudOfVfoA = FREQ.numOfbaud;
  }
  else if(FREQ.item & VFOBItem)
  {
    VFO.VFOB[FREQ.numOfbaud].freq = FREQ.val;
    VFO.VFOB[FREQ.numOfbaud].mode = RADIO.mode;
    VFO.CurrBaudOfVfoB = FREQ.numOfbaud;
  }
}

void MEM_Get()
{
  FREQ.val = MEM[FREQ.numOfmem].freq;
  FREQ.numOfbaud = frequency_band(FREQ.val, BAUD, SIZE_OF_BAUD);
  RADIO.mode = MEM[FREQ.numOfmem].mode;
}

void MEM_Save()
{
  MEM[FREQ.numOfmem].freq = FREQ.val;
  MEM[FREQ.numOfmem].baud = FREQ.numOfbaud;
  MEM[FREQ.numOfmem].mode = RADIO.mode;

}
