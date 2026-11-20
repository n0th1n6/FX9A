#!/usr/bin/env python3
"""Exercise production CH/VOL decoding and actions with simulated GPIO samples."""
from pathlib import Path
import os
import subprocess
import tempfile
import sys

sys.dont_write_bytecode = True

from test_frequency import bsp, board, function, key, table, typedef, types

extra_types = "\n".join(typedef(n) for n in (
    "VOL_AttrDef", "WPM_AttrDef", "MENU_AttrDef"))
functions = "\n".join(function(board, n) for n in ("VFO_Save", "VFO_Get", "MEM_Get"))
functions += "\n" + "\n".join(function(key, n) for n in (
    "encoders_init", "volume_encoder_step", "channel_encoder_step", "key_timer_callback"))
program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "bsp_frequency.h"
#include "bsp_encoder.h"
#define SIZE_OF_BAUD 10
#define SIZE_OF_MEM 100
#define SIZE_OF_MENU 21
#define MAX_CALLSIGN_LENGTH 10
''' + types + "\n" + extra_types + "\n" + table + r'''
FREQ_AttrDef FREQ;
VFO_SaveDef VFO;
RADIO_AttrDef RADIO;
MEM_AttrDef MEM[SIZE_OF_MEM];
VOL_AttrDef VOL;
WPM_AttrDef WPM;
MENU_AttrDef MENU;
DispRefreshMgr DRMgr;
AdjustItemMgr AIMgr;
static EncoderState channel_encoder, volume_encoder;
static bool transmitting;
static uint16_t channel_port, volume_port;
#define K1A (1u << 12)
#define K1B (1u << 11)
#define K2A (1u << 10)
#define K2B (1u << 11)
#define KEY_K1A1B_PORT 0
#define KEY_23456R1A1BK2A2B_PORT 1
static uint16_t GPIO_ReadInputData(int port) {
    return port == KEY_K1A1B_PORT ? channel_port : volume_port;
}
static bool RF_IsTx(void) { return transmitting; }
static unsigned button_scans;
static uint8_t key_scan_divider;
static void key_calback(void) { ++button_scans; }
''' + key[key.index("static uint8_t channel_phases"):key.index("static void encoders_init(void)\n{")] + functions + r'''
static void sample(unsigned ch, unsigned vol) {
    channel_port = ((ch & 2) ? K1B : 0) | ((ch & 1) ? K1A : 0);
    volume_port = ((vol & 2) ? K2A : 0) | ((vol & 1) ? K2B : 0);
    key_timer_callback();
}
static void cycle(bool channel, bool positive) {
    const unsigned forward[] = {2, 0, 1, 3};
    const unsigned reverse[] = {1, 0, 2, 3};
    const unsigned *path = positive ? forward : reverse;
    for (unsigned i = 0; i < 4; ++i)
        sample(channel ? path[i] : 3, channel ? 3 : path[i]);
}
static int decode(const unsigned *path, unsigned count) {
    EncoderState e;
    encoder_init(&e, 3);
    int steps = 0;
    for (unsigned i = 0; i < count; ++i)
        steps += encoder_update(&e, path[i]);
    return steps;
}
int main(void) {
    const unsigned bounced[] = {3,2,3,2,0,2,0,1,0,1,3};
    const unsigned reversed[] = {3,1,3,1,0,1,0,2,0,2,3};
    const unsigned aborted[] = {3,2,0,2,3,3};
    const unsigned common_bounce[] = {3,2,0,3,0,3,1,3};
    const unsigned common_bounce_reverse[] = {3,1,0,3,0,3,2,3};
    const unsigned invalid[] = {3,0,3,1,2,3};
    assert(decode(bounced, sizeof(bounced)/sizeof(*bounced)) == 1);
    assert(decode(reversed, sizeof(reversed)/sizeof(*reversed)) == -1);
    assert(decode(aborted, sizeof(aborted)/sizeof(*aborted)) == 0);
    assert(decode(invalid, sizeof(invalid)/sizeof(*invalid)) == 0);
    assert(decode(common_bounce, sizeof(common_bounce)/sizeof(*common_bounce)) == 1);
    assert(decode(common_bounce_reverse, sizeof(common_bounce_reverse)/sizeof(*common_bounce_reverse)) == -1);
    EncoderState e;
    encoder_init(&e, 0);
    assert(!encoder_update(&e, 1)); assert(!encoder_update(&e, 3));
    int total = 0;
    for (unsigned i = 0; i < 1000; ++i) {
        total += encoder_update(&e, 2); total += encoder_update(&e, 0);
        total += encoder_update(&e, 1); total += encoder_update(&e, 3);
    }
    assert(total == 1000);

    channel_port = K1A | K1B; volume_port = K2A | K2B;
    encoders_init();
    FREQ.item = VFOItem | VFOAItem;
    for (unsigned i = 0; i < 10; ++i) VFO.VFOA[i].freq = BAUD[i][1];
    VOL.val = 30; FREQ.numOfbaud = 3;
    for (unsigned i = 0; i < 100; ++i) sample(3, 3);
    assert(button_scans == 5 && VOL.val == 30 && FREQ.numOfbaud == 3);

    cycle(false, true); assert(VOL.val == 31);
    cycle(false, false); assert(VOL.val == 30);
    VOL.val = 60; cycle(false, true); assert(VOL.val == 60);
    VOL.val = 0; cycle(false, false); assert(VOL.val == 0);
    cycle(true, true); assert(FREQ.numOfbaud == 4 && FREQ.val == BAUD[4][1]);
    cycle(true, false); assert(FREQ.numOfbaud == 3);
    FREQ.numOfbaud = 9; cycle(true, true); assert(FREQ.numOfbaud == 0);
    cycle(true, false); assert(FREQ.numOfbaud == 9);

    AIMgr.item = MENUSETItem; MENU.numOfmenu = 3; VOL.val = 30;
    cycle(true, true); assert(MENU.numOfmenu == 4);
    cycle(true, false); assert(MENU.numOfmenu == 3);
    cycle(false, true); assert(VOL.val == 30);
    MENU.numOfmenu = 19; cycle(true, true); assert(MENU.numOfmenu == 20);
    cycle(true, true); assert(MENU.numOfmenu == 20);
    MENU.numOfmenu = 0; cycle(true, false); assert(MENU.numOfmenu == 0);
    AIMgr.item = WPMSETItem; WPM.val = 20;
    cycle(false, true); assert(WPM.val == 21);
    cycle(false, false); assert(WPM.val == 20);
    WPM.val = 60; cycle(false, true); assert(WPM.val == 60);
    WPM.val = 4; cycle(false, false); assert(WPM.val == 4);

    AIMgr.item = MEMSETItem; FREQ.numOfbaud = 3;
    cycle(false, true); assert(FREQ.numOfbaud == 4 && FREQ.val == BAUD[4][1]);
    cycle(false, false); assert(FREQ.numOfbaud == 3);
    AIMgr.item = 0; FREQ.item = MEMItem; FREQ.numOfmem = 0;
    MEM[1].freq = 750000; MEM[1].mode = RADIO_USB;
    cycle(true, true); assert(FREQ.numOfmem == 1 && FREQ.val == 750000);
    assert(FREQ.numOfbaud == 3);
    FREQ.numOfmem = 99; cycle(true, true); assert(FREQ.numOfmem == 99);
    FREQ.numOfmem = 0; cycle(true, false); assert(FREQ.numOfmem == 0);

    /* Inhibited motion is consumed, so it cannot leak into a later action. */
    FREQ.item = VFOItem | VFOAItem; FREQ.numOfbaud = 3; VOL.val = 30;
    transmitting = true; cycle(true, true); cycle(false, true);
    assert(FREQ.numOfbaud == 3 && VOL.val == 30);
    transmitting = false; sample(3, 3);
    assert(FREQ.numOfbaud == 3 && VOL.val == 30);
    cycle(false, true); assert(VOL.val == 31);

    /* Both controls can move concurrently, including brief snap transitions. */
    sample(2,2); sample(0,0); sample(1,1); sample(3,3);
    assert(FREQ.numOfbaud == 4 && VOL.val == 32);
    puts("PASS: 250 us quadrature decoder, bounce, invalid/partial transitions, reversals, CH/VOL actions, limits, inhibit and 5 ms button cadence");
}
'''
# Replay the physical contact trace at the production 250 us sample cadence.
# Use the initial ten CH forward clicks and ten VOL clicks in each direction;
# later CH retries are not assumed to be exactly one physical click each.
rows = [tuple(map(int, line.split(","))) for line in
        (Path(__file__).parent / "fixtures/encoder-slow-contacts.csv").read_text().splitlines()
        if line and not line.startswith("#")]
records = ",".join("{%d,%d}" % row for row in rows)
replay = "static const uint32_t captured[][2] = {" + records + "};\n" + r'''
static void replay_contacts(void) {
    EncoderState ch, vol;
    encoder_init(&ch, 3); encoder_init(&vol, 3);
    unsigned index = 0, raw = 15, ch_up = 0, ch_down = 0;
    unsigned vol_up = 0, vol_down = 0;
    for (unsigned tick = 5; tick <= 76 * 20000; tick += 5) {
        while (index < sizeof(captured)/sizeof(*captured) && captured[index][0] <= tick)
            raw = captured[index++][1];
        int a = encoder_update(&ch, raw & 3);
        int b = encoder_update(&vol, (raw >> 2) & 3);
        if (a > 0) ++ch_up;
        if (a < 0) ++ch_down;
        if (tick < 58 * 20000) {
            if (b > 0) ++vol_up;
            if (b < 0) ++vol_down;
        }
    }
    assert(ch_up == 10 && ch_down == 0);
    assert(vol_up == 10 && vol_down == 10);
    puts("PASS: captured slow CH contacts recover all ten forward clicks; VOL ten each direction");
}
'''
program = program.replace("int main(void) {", replay + "\nint main(void) {\n    replay_contacts();")
trace_start = key.index("#ifdef FX9A_ENCODER_TRACE")
trace_globals = key[trace_start:key.index("#endif", trace_start) + len("#endif")]
trace_program = program.replace("static uint8_t key_scan_divider;",
    "static uint8_t key_scan_divider;\n" + trace_globals)
trace_program = trace_program.replace("    key_timer_callback();",
    "    for (unsigned i = 0; i < 5; ++i) key_timer_callback();")
trace_program = trace_program.replace('    puts("PASS: 250 us', r'''    assert(encoder_trace_count > 0 && encoder_trace_count < 2048);
    for (unsigned i = 1; i < encoder_trace_count; ++i) {
        assert(encoder_trace[i][0] > encoder_trace[i-1][0]);
        assert(encoder_trace[i][1] != encoder_trace[i-1][1]);
    }
    encoder_trace_count = 2048;
    sample(2, 2);
    assert(encoder_trace_count == 2048 && !encoder_trace_enabled);
    puts("PASS: diagnostic capture timing, transition records and full-buffer stop");
    puts("PASS: 250 us''')
with tempfile.TemporaryDirectory(prefix="fx9a-encoders-") as directory:
    for name, code, defines in (("normal", program, []),
            ("trace", trace_program, ["-DFX9A_ENCODER_TRACE=1"])):
        source = Path(directory) / (name + ".c")
        binary = Path(directory) / name
        source.write_text(code)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                        "-Werror", "-fsanitize=undefined", *defines, "-I", str(bsp),
                        str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
