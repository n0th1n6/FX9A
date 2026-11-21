/* Native STM32 LL RF state machine, tuner and fast reflected-power guard.
 * Extracted from bsp_board without changing RF sequencing or trip thresholds.
 * Display notifications run in the main loop; RF_SwrSampleISR does no OLED work.
 */
#include "bsp_board.h"
#include "radio_rf.h"
#include "radio_ui.h"
#include "radio_synth.h"
#include "radio_board_io.h"
#include "tune_logic.h"
#include "bsp_key.h"
#include "bsp_adc.h"
#include "bsp_oled_api.h"
#include "bsp_iambic_keyer.h"
#include "bsp_transmit_power.h"
#include "bsp_ad9951.h"
#include "bsp_ad9834.h"
#include "bsp_timer.h"
#include "bsp_pwm.h"

volatile bool RfIsTx = false;
volatile uint16_t hswr_timeout = 0;
static volatile uint8_t swr_trip_pending;
extern volatile uint32_t fx9a_ticks_10ms;
static Fx9aTuneLogic tune_logic;
static TP_LevelDef tune_saved_power;
static volatile bool tune_active;
static volatile bool tune_carrier_on;
static volatile bool tune_swr_sampled;
static volatile uint16_t tune_last_swr_adc;
static uint8_t swr_high_samples;

bool RadioRF_HasSample(void) { return tune_swr_sampled; }

uint16_t RadioRF_LastSample(void) { return tune_last_swr_adc; }

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
