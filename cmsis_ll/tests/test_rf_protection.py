#!/usr/bin/env python3
"""Run production TX transitions and SWR ISR with interrupt injection."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
board = "\n".join((root / "cmsis_ll/app" / path).read_text(encoding="latin1")
                  for path in ("bsp_board.c", "radio_ui.c", "radio_rf.c"))
header = (root / "RadioStation/BSP/bsp_board.h").read_text(encoding="latin1")


def function(name):
    match = re.search(r"(?:static )?(?:void|bool|uint8_t|uint16_t) " + name +
                      r"\([^)]*\)\s*\{", board)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (board[end] == "{") - (board[end] == "}")
        end += 1
    return board[match.start():end]


program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tune_logic.h"
enum { RADIO_USB = 1, LSB, CWA, CWB, N = 1, L = 0, H = 1 };
enum { TPItem = 1, RADIOItem = 2, TXRXItem = 4, SWRCHKItem = 8,
       RXCHKItem = 16, RFCHKItem = 32 };
typedef unsigned TP_LevelDef;
static struct { unsigned mode; } RADIO;
static struct { uint32_t val; } FREQ;
static unsigned Mode;
static char mode_label[8];
static struct { unsigned func; } FIL;
static struct { TP_LevelDef level; } TP;
static struct { unsigned item; } DRMgr, AIMgr;
static struct { uint8_t vswr; } MENU;
static bool isRun, PttPressed;
static volatile bool RfIsTx;
''' + re.search(r"^extern .*\bhswr_timeout;", header, re.M).group() + '\n' + \
    re.search(r"^(?:volatile )?uint16_t hswr_timeout\s*=.*;", board, re.M).group() + r'''
static volatile uint8_t swr_trip_pending;
static uint8_t swr_high_samples;
static volatile uint32_t fx9a_ticks_10ms;
static Fx9aTuneLogic tune_logic;
static TP_LevelDef tune_saved_power;
static volatile bool tune_active;
static volatile bool tune_carrier_on, tune_swr_sampled;
static volatile uint16_t tune_last_swr_adc;
static bool tune_result_visible;
static uint32_t tune_result_tick, tune_result_frequency;
#define TUNE_RESULT_TICKS 500u
static unsigned result_marker, result_valid, displayed_swr, meter_clears;
static bool gate, microphone, carrier;
static unsigned primask, pending_irq, trips, gate_enables, synth_calls;
static uint16_t reflected;
static bool sample_on_carrier_off;
enum { NONE, SYNTH, SETTLE, GATE, MICROPHONE, CARRIER };
static unsigned injection;
void RF_SwrSampleISR(void);
void RF_SwrService(void);
void RF_ToTx(void);
void RF_ToRx(void);
void Tune_Stop(void);
bool Tune_IsActive(void);

static void deliver_irq(void)
{
    if (!primask && pending_irq) {
        pending_irq = 0;
        unsigned before = swr_trip_pending;
        RF_SwrSampleISR();
        if (!before && swr_trip_pending) ++trips;
    }
}
static void inject(unsigned point)
{
    if (injection == point) {
        injection = NONE;
        reflected = 800;
        pending_irq = 1;
        deliver_irq();
    }
}
static uint32_t __get_PRIMASK(void) { return primask; }
static void __disable_irq(void) { primask = 1; }
static void __set_PRIMASK(uint32_t value)
{
    primask = value;
    deliver_irq();
}
static uint16_t adc_swr(void) { return reflected; }
static void MICControlOff(void) { microphone = false; }
static void MICControlOn(void)
{
    assert(!hswr_timeout && !swr_trip_pending);
    microphone = true;
    inject(MICROPHONE);
}
static void tx_control_off(void) { gate = false; }
static void tx_control_on(void)
{
    assert(!hswr_timeout && !swr_trip_pending);
    gate = true;
    ++gate_enables;
    inject(GATE);
}
static void CWT_Off(void)
{
    carrier = false;
    if (sample_on_carrier_off) { reflected = 0; RF_SwrSampleISR(); }
}
static void CWT_On(void)
{
    assert(!hswr_timeout && !swr_trip_pending);
    carrier = true;
    inject(CARRIER);
}
static void QuietModeOn(void) {}
static void QuietModeOff(void) {}
static void CW_FilterControl(unsigned value) { (void)value; }
static void CW_BFOControl(unsigned value) { (void)value; }
#define TIM4 4
static void TMR_Start(unsigned timer) { (void)timer; }
static void TMR_Stop(unsigned timer) { (void)timer; }
static void RF_ProgramSynth(void) { ++synth_calls; inject(SYNTH); }
static void delay_ms(unsigned ms) { (void)ms; inject(SETTLE); }
static void power_high_off(void) {}
static void power_high_on(void) {}
static void oled_lsb_usb(const char *text) { strcpy(mode_label, text); }
static void voice_control_start(void) {}
static void voice_control_stop(void) {}
static void keyer_auto_setup(void) {}
static void oled_h_vswr_disp(void) {}
static bool MeterCal_Active(void) { return false; }
static uint8_t MeterCal_Grid(uint8_t meter, uint16_t adc)
{ (void)meter; (void)adc; return 0; }
static void oled_swr_grid(uint8_t grid) { displayed_swr = grid; }
static void oled_tune_result(uint8_t visible, uint8_t valid)
{ result_marker = visible; result_valid = valid; }
static void oled_txrx_clear_grid(void) { displayed_swr = 0; ++meter_clears; }
static bool RadioRF_HasSample(void) { return tune_swr_sampled; }
static uint16_t RadioRF_LastSample(void) { return tune_last_swr_adc; }
''' + '\n'.join(function(name) for name in (
    "MODE_Refresh", "SWR_DisplayGrid", "Tune_ResultDraw", "Tune_ResultExpire",
    "RadioUI_TuneStarting", "RadioUI_TuneStopped",
    "Tune_IsActive", "Tune_Start", "Tune_Stop", "Tune_Task", "swr_trip_level",
    "RF_SwrSampleISR", "RF_SwrService", "RF_Tx", "RF_Rx", "RF_ToRx",
    "RF_ToTx")) + r'''
static void reset(unsigned mode)
{
    RADIO.mode = mode;
    FREQ.val = 705000;
    Mode = mode;
    mode_label[0] = 0;
    FIL.func = N;
    TP.level = H;
    MENU.vswr = 9;
    DRMgr.item = AIMgr.item = 0;
    isRun = true;
    PttPressed = RfIsTx = tune_active = false;
    tune_carrier_on = tune_swr_sampled = tune_result_visible = false;
    tune_last_swr_adc = tune_result_tick = tune_result_frequency = 0;
    result_marker = result_valid = displayed_swr = meter_clears = 0;
    hswr_timeout = swr_trip_pending = swr_high_samples = 0;
    fx9a_ticks_10ms = 0;
    gate = microphone = carrier = false;
    primask = pending_irq = trips = gate_enables = synth_calls = 0;
    reflected = 0;
    sample_on_carrier_off = false;
    injection = NONE;
}
int main(void)
{
    (void)__get_PRIMASK;
    (void)__disable_irq;
    for (unsigned mode = RADIO_USB; mode <= CWB; ++mode) {
        reset(mode);
        RF_ToTx();
        assert(gate && RfIsTx && !hswr_timeout);
        assert(microphone == (mode == RADIO_USB || mode == LSB));
        assert(!primask);
        RF_ToRx();
        assert(!gate && !microphone && !RfIsTx);

        for (unsigned point = SYNTH; point <= MICROPHONE; ++point) {
            if (point == MICROPHONE && mode >= CWA) continue;
            reset(mode);
            injection = point;
            RF_ToTx();
            assert(trips == 1 && hswr_timeout && swr_trip_pending);
            assert(!gate && !microphone && !primask);
            RF_SwrService();
            assert(!RfIsTx && !gate && !swr_trip_pending && hswr_timeout);
            unsigned enabled = gate_enables, programmed = synth_calls;
            RF_ToTx(); /* A held PTT, CAT TX or queued CW cannot bypass trip. */
            assert(gate_enables == enabled && synth_calls == programmed);
            assert(!RfIsTx && !gate);
            hswr_timeout = 0; /* Existing input-release acknowledgement. */
            reflected = 0;
            RF_ToTx();
            assert(gate && RfIsTx);
        }
    }
    for (unsigned point = SYNTH; point <= CARRIER; ++point) {
        if (point == MICROPHONE) continue; /* TUNE keeps MIC muted. */
        reset(RADIO_USB);
        injection = point;
        bool started = Tune_Start();
        assert(trips == 1 && hswr_timeout && !gate && !microphone);
        if (started) RF_SwrService();
        assert(!tune_active && !RfIsTx && TP.level == H);
    }
    reset(RADIO_USB);
    hswr_timeout = 1;
    assert(!Tune_Start() && !gate && TP.level == H && !synth_calls);
    reset(RADIO_USB);
    assert(Tune_Start());
    assert(gate && carrier && !microphone && TP.level == L);
    Tune_Stop();
    assert(!gate && !carrier && !RfIsTx && TP.level == H);
    assert(!strcmp(mode_label, "USB "));

    const char *labels[] = {0, "USB ", "LSB ", "CW/A", "CW/B"};
    for (unsigned mode = RADIO_USB; mode <= CWB; ++mode) {
        for (unsigned ending = 0; ending < 5; ++ending) {
            reset(mode);
            assert(Tune_Start() && !strcmp(mode_label, "TUNE"));
            reflected = ending == 0 ? 50 : ending == 4 ? 800 : 300;
            fx9a_ticks_10ms = 40;
            RF_SwrSampleISR();
            if (ending == 0) { /* Stable low reflection completes tuning. */
                Tune_Task();
                fx9a_ticks_10ms = 90;
                RF_SwrSampleISR();
                Tune_Task();
            } else if (ending == 1) Tune_Stop(); /* Key / CAT cancellation. */
            else if (ending == 2) { PttPressed = true; Tune_Task(); }
            else if (ending == 3) { fx9a_ticks_10ms = 1000; Tune_Task(); }
            else RF_SwrService(); /* Severe protection cutoff. */
            assert(!tune_active && !RfIsTx && TP.level == H);
            assert(!strcmp(mode_label, labels[mode]));
            assert(tune_result_visible && tune_swr_sampled);
            uint16_t sample = tune_last_swr_adc;
            reflected = 0; /* Detector falls after RF stops. Do not display it. */
            oled_txrx_clear_grid();
            Tune_ResultDraw();
            assert(result_marker && result_valid);
            assert(displayed_swr == SWR_DisplayGrid(sample));
            fx9a_ticks_10ms = tune_result_tick + 499;
            Tune_ResultExpire();
            assert(tune_result_visible);
            ++fx9a_ticks_10ms;
            Tune_ResultExpire();
            assert(!tune_result_visible && !result_marker && !displayed_swr);
            assert(DRMgr.item & RXCHKItem);
        }
    }
    reset(RADIO_USB);
    assert(Tune_Start());
    Tune_Stop(); /* No 10 ms RF sample yet: show unavailable, never fake 1:1. */
    Tune_ResultDraw();
    assert(result_marker && !result_valid && !displayed_swr);
    fx9a_ticks_10ms = 0xfffffff0u;
    tune_result_tick = fx9a_ticks_10ms;
    fx9a_ticks_10ms += 499;
    Tune_ResultExpire();
    assert(tune_result_visible);
    ++fx9a_ticks_10ms;
    Tune_ResultExpire();
    assert(!tune_result_visible);

    for (unsigned action = 0; action < 3; ++action) {
        reset(RADIO_USB);
        assert(Tune_Start()); Tune_Stop(); Tune_ResultDraw();
        unsigned cleared = meter_clears;
        if (action == 0) ++FREQ.val;
        else if (action == 1) AIMgr.item = 1;
        else RfIsTx = true;
        Tune_ResultExpire();
        assert(!tune_result_visible);
        if (action == 1) assert(meter_clears == cleared); /* Do not erase menu. */
        else assert(!result_marker && meter_clears > cleared);
    }
    reset(RADIO_USB);
    assert(Tune_Start());
    reflected = 300;
    RF_SwrSampleISR();
    sample_on_carrier_off = true;
    Tune_Stop();
    assert(tune_last_swr_adc == 300); /* IRQ after RF-off cannot overwrite it. */

    /* Preserve a caller's masked state; a queued IRQ runs after unmasking. */
    reset(RADIO_USB);
    primask = 1;
    injection = GATE;
    RF_ToTx();
    assert(primask == 1 && pending_irq && trips == 0);
    __set_PRIMASK(0);
    assert(trips == 1 && !gate && !microphone);
    puts("PASS: TX protection, TUNE exits restore every mode, captured SWR hold/expiry and PRIMASK restoration");
}
'''
with tempfile.TemporaryDirectory(prefix="fx9a-rf-protection-") as directory:
    source = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    source.write_text(program)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                    "-Werror", "-O2", "-I" + str(root / "cmsis_ll/board"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
