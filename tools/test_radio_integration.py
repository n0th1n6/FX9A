#!/usr/bin/env python3
"""Exercise production scheduler, SWR display/ISR and calibration save paths.

Hardware calls are mocked; this does not validate physical RF or EEPROM timing.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
native = (root / "cmsis_ll").is_dir()
app = root / ("cmsis_ll/app" if native else "RadioStation/BSP")
headers = root / ("cmsis_ll/board" if native else "RadioStation/BSP")


def block(text, start):
    opening = text.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def function(text, name):
    match = re.search(r"(?:static )?(?:void|bool|uint16_t) " + name +
                      r"\([^)]*\)\s*\{", text)
    assert match, name
    return block(text, match.start())


scheduler = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
enum { CWA = 3, CWB, MANUAL = 1, CALLItem = 1 };
static struct { unsigned mode; } RADIO;
static struct { unsigned item; } FREQ, AIMgr;
static struct { unsigned cwMode; } MENU;
static bool tuning;
static unsigned calls, manual, automatic;
static bool Tune_IsActive(void) { return tuning; }
static void RF_SwrService(void) {}
static void Tune_Task(void) {}
static void key_task(void) {}
static void RadioControls_Task(void) {}
static void board_refresh_manager(void) {}
static void RadioUI_Refresh(void) {}
static void RadioCAT_Task(void) {}
static void call_task(void) { ++calls; }
static void keyer_manual_task(void) { ++manual; }
static void keyer_auto_task(void) { ++automatic; }
''' + function((app / "bsp_board.c").read_text(encoding="latin1"), "board_task") + r'''
int main(void) {
    for (unsigned mode = CWA; mode <= CWB; ++mode) {
        RADIO.mode = mode;
        for (unsigned path = 0; path < 3; ++path) {
            FREQ.item = path == 0 ? CALLItem : 0;
            MENU.cwMode = path == 1 ? MANUAL : 0;
            calls = manual = automatic = 0;
            tuning = true;
            board_task();
            assert(!calls && !manual && !automatic);
            tuning = false;
            board_task();
            assert(calls == (path == 0));
            assert(manual == (path == 1));
            assert(automatic == (path == 2));
        }
    }
    puts("PASS: TUNE suspends CQ/manual/automatic CW in both CW modes; normal dispatch resumes");
}
'''

ui = (app / "radio_ui.c").read_text(encoding="latin1")
start = ui.index("      if ( DRMgr.item & SWRCHKItem")
swr_display = block(ui, start)
rf = (app / "radio_rf.c").read_text(encoding="latin1")
protection = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "tune_logic.h"
enum { SWRCHKItem = 1, ALLItem = 2 };
static struct { unsigned item; } DRMgr;
static struct { unsigned vswr; } MENU;
static unsigned adc_val, rx_calls, warnings, gate_cuts, displayed, hswr_timeout;
static bool RfIsTx, tuning, tune_carrier_on, tune_swr_sampled;
static bool tune_result_visible, tune_result_sampled, calibrated;
static uint16_t reflected, tune_last_swr_adc, tune_result_adc;
static uint8_t swr_trip_pending, swr_high_samples;
static uint32_t fx9a_ticks_10ms;
static Fx9aTuneLogic tune_logic;
static uint16_t adc_swr(void) { return reflected; }
static bool RF_IsTx(void) { return RfIsTx; }
static bool Tune_IsActive(void) { return tuning; }
static bool MeterCal_Active(void) { return calibrated; }
static uint8_t MeterCal_Grid(unsigned meter, unsigned adc)
{ (void)meter; (void)adc; return 9; }
static bool RadioRF_HasSample(void) { return tune_swr_sampled; }
static uint16_t RadioRF_LastSample(void) { return tune_last_swr_adc; }
static void oled_swr_grid(unsigned grid) { displayed = grid; }
static void MICControlOff(void) {}
static void tx_control_off(void) { ++gate_cuts; }
static void RF_ToRx(void) { ++rx_calls; RfIsTx = false; }
static void Tune_Stop(void) { tuning = false; RF_ToRx(); }
static void oled_h_vswr_disp(void) { ++warnings; }
static void RadioUI_SwrTrip(void) { oled_h_vswr_disp(); }
''' + '\n'.join(function(rf, name) for name in
                ("swr_trip_level", "RF_SwrSampleISR", "RF_SwrService")) + r'''
static void refresh_swr(void) {
    uint8_t grid = 0;
''' + swr_display + r'''
}
static void refresh(void) { DRMgr.item = SWRCHKItem; refresh_swr(); }
static void reset(void) {
    RfIsTx = true; tuning = calibrated = false;
    MENU.vswr = 9; reflected = 600;
    swr_trip_pending = swr_high_samples = hswr_timeout = 0;
    rx_calls = warnings = gate_cuts = 0;
}
int main(void) {
    reset();
    RF_SwrSampleISR(); refresh();
    assert(displayed == 9 && !gate_cuts && !rx_calls && !warnings && !hswr_timeout);
    reflected = 0; RF_SwrSampleISR(); refresh();
    assert(!displayed && !swr_high_samples && !gate_cuts);
    reflected = 600;
    RF_SwrSampleISR(); refresh(); RF_SwrSampleISR(); refresh();
    assert(!gate_cuts && !rx_calls && !hswr_timeout);
    RF_SwrSampleISR(); refresh();
    assert(gate_cuts == 1 && swr_trip_pending && hswr_timeout);
    assert(!rx_calls && !warnings); /* UI never performs RF state transitions. */
    RF_SwrService(); assert(rx_calls == 1 && warnings == 1);
    reset(); reflected = 800;
    RF_SwrSampleISR(); assert(gate_cuts == 1 && hswr_timeout);
    reset(); MENU.vswr = 0;
    for (unsigned i = 0; i < 4; ++i) { RF_SwrSampleISR(); refresh(); }
    assert(!gate_cuts && !rx_calls);
    reflected = 800; RF_SwrSampleISR(); assert(gate_cuts == 1);
    reset(); tuning = true; fx9a_ticks_10ms = 40;
    for (unsigned i = 0; i < 29; ++i) { RF_SwrSampleISR(); refresh(); }
    assert(!gate_cuts && !rx_calls);
    RF_SwrSampleISR(); assert(gate_cuts == 1);
    reset(); calibrated = true; reflected = 0;
    for (unsigned i = 0; i < 4; ++i) { RF_SwrSampleISR(); refresh(); }
    assert(displayed == 9 && !gate_cuts && !rx_calls);
    puts("PASS: display cannot trip RF; raw ISR rejects spikes, trips sustained/severe reflection and preserves TUNE policy");
}
'''

controls = (app / "radio_controls.c").read_text(encoding="latin1")
start = controls.index("        if( AIMgr.item & MENUSETItem)           // exit settings and save calibration")
exit_menu = block(controls, start)
calibration = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "meter_cal.h"
#include "bsp_board.h"
enum { MENUSETItem = 1, ALLItem = 2, OK, WRITE_FAIL, READ_FAIL, CORRUPT };
static uint8_t ee[sizeof(Fx9aMeterCal)];
static unsigned fault, writes, errors, cleared, unmuted, stopped, key_clears;
static struct { unsigned item; } AIMgr, DRMgr;
bool e2p_write(uint16_t addr, uint8_t *data, uint16_t len) {
    assert(addr == E2P_ADDR_CAL && len == sizeof ee); ++writes;
    if (fault == WRITE_FAIL) return false;
    memcpy(ee, data, len);
    if (fault == CORRUPT) ee[8] ^= 1;
    return true;
}
bool e2p_read(uint16_t addr, uint8_t *data, uint16_t len) {
    assert(addr == E2P_ADDR_CAL && len == sizeof ee);
    if (fault == READ_FAIL) return false;
    memcpy(data, ee, len); return true;
}
static void oled_ts(uint8_t *s) { assert(!strcmp((char *)s, "ERR")); ++errors; }
static void buzzer_stop(void) { ++stopped; }
static void QuietModeOff(void) { ++unmuted; }
static void oled_set_screen(unsigned value) { assert(value == 0); ++cleared; }
static void key_clear(void) { ++key_clears; }
static void leave_menu(void) {
    switch (5) { case 5:
''' + exit_menu + r'''
    break;
    }
    key_clear();
}
int main(void) {
    memset(ee, 0xff, sizeof ee); MeterCal_Init();
    assert(!MeterCal_Active());
    MeterCal_Adjust(METER_CAL_FIRST, 1);
    uint16_t expected = MeterCal_Value(0);
    AIMgr.item = MENUSETItem;
    for (fault = WRITE_FAIL; fault <= CORRUPT; ++fault) {
        unsigned before = writes, previous_errors = errors;
        leave_menu();
        assert(writes == before + 1 && errors == previous_errors + 1);
        assert(AIMgr.item == MENUSETItem && !DRMgr.item);
        assert(!cleared && !unmuted && !stopped && !MeterCal_Active());
        assert(MeterCal_Value(0) == expected); /* Edited value survives retry. */
    }
    fault = OK; leave_menu();
    assert(!AIMgr.item && DRMgr.item == ALLItem && MeterCal_Active());
    assert(cleared == 1 && unmuted == 1 && stopped == 1 && key_clears == 4);
    MeterCal_Init(); assert(MeterCal_Active() && MeterCal_Value(0) == expected);
    unsigned before = writes;
    AIMgr.item = MENUSETItem; leave_menu(); assert(writes == before);
    puts("PASS: calibration write/read/corruption failures retain menu and edits; retry verifies EEPROM before exit");
}
'''

with tempfile.TemporaryDirectory(prefix="fx9a-integration-") as directory:
    temp = Path(directory)
    (temp / "meter_cal.h").write_bytes((headers / "meter_cal.h").read_bytes())
    (temp / "tune_logic.h").write_bytes((headers / "tune_logic.h").read_bytes())
    # Retain production constants without pulling device registers into host tests.
    board_header = "\n".join((root / "RadioStation/BSP" / name).read_text(encoding="latin1")
                             for name in ("bsp_board.h", "bsp_24lc16.h"))
    constants = [line for line in board_header.splitlines() if re.match(
        r"#define\s+(?:E2P_ADDR_BASE|E2P_ADDR_CAL|METER_CAL_FIRST|METER_CAL_LAST|CONT_TUNE_MENU|RX_LSB_MENU|RX_USB_MENU|MAX_CALLSIGN_LENGTH)\b", line)]
    (temp / "bsp_board.h").write_text("\n".join(constants) + "\n")
    (temp / "bsp_24lc16.h").write_text(
        "#include <stdbool.h>\n#include <stdint.h>\n"
        "bool e2p_read(uint16_t, uint8_t *, uint16_t);\n"
        "bool e2p_write(uint16_t, uint8_t *, uint16_t);\n")
    cal_source = temp / "meter_cal.c"
    cal_source.write_bytes((app / "meter_cal.c").read_bytes())
    failed = False
    for name, program, extra in (("scheduler", scheduler, []),
                                 ("protection", protection, []),
                                 ("calibration", calibration, [str(cal_source)])):
        source, binary = temp / (name + ".c"), temp / name
        source.write_text(program)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                        "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
                        "-fsanitize=address,undefined", "-I" + str(temp), str(source),
                        *extra, "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)])
        failed |= result.returncode != 0
    if failed:
        raise SystemExit(1)
