#include "radio_state.h"
#include "radio_vfo.h"
#include "settings_validation.h"
#include "settings_store.h"
#include "bsp_24lc16.h"
#include "bsp_iambic_keyer.h"

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

bool isRun = false;
FIL_AttrDef CWFilter;

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


#define BFO_DEFAULT_VAL 8999570

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

static bool settings_write_allowed = true;

static void defaults_in_ram(void)
{
  memset(&MENU, 0, sizeof MENU);
  FIL.func = W; ATT.val = 0;

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

    RadioVFO_Init();
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
      MEM[i].mode = USB;
    }

    VOL.val = 2;

}

bool RadioState_Save(void)
{
  return settings_write_allowed && Settings_Save();
}

void RadioState_ResetDefaults(void)
{
  defaults_in_ram();
  settings_write_allowed = true;
  (void)RadioState_Save();
}

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
    if (!fx9a_mode_valid(a->mode)) a->mode = band < 3u ? LSB : USB;
    if (!fx9a_mode_valid(b->mode)) b->mode = band < 3u ? LSB : USB;
  }
  for (uint8_t i = 0; i < SIZE_OF_MEM; ++i) {
    if (!fx9a_saved_frequency(MEM[i].freq, BAUD[0][0],
                              BAUD[SIZE_OF_BAUD-1u][2]))
      MEM[i].freq = BAUD[3][1];
    MEM[i].baud = fx9a_saved_band(MEM[i].baud, SIZE_OF_BAUD, 3u);
    if (!fx9a_mode_valid(MEM[i].mode)) MEM[i].mode = USB;
  }
  if (VFO.RIT < LOWER_OF_RIT || VFO.RIT > UPPER_OF_RIT) VFO.RIT = 0;
  if (VFO.XIT < LOWER_OF_XIT || VFO.XIT > UPPER_OF_XIT) VFO.XIT = 0;
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

void RadioState_Load(void)
{
  SettingsResult result = Settings_Load();
  settings_write_allowed = result != SETTINGS_CORRUPT && result != SETTINGS_IO_ERROR;
  if (result == SETTINGS_VALID || result == SETTINGS_LEGACY)
    sanitize_saved_settings();
  else
    defaults_in_ram(); /* Never overwrite unreadable or corrupt EEPROM at boot. */
  RX_AlignmentInit();
  CWFilter.func = FIL.func;



  FREQ.item |=  VFOItem + VFOAItem;
  FREQ.numOfbaud = VFO.CurrBaudOfVfoA;
  VFO_Get();

  // 全部刷新
  DRMgr.item |= ALLItem;
}
