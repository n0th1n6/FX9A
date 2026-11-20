#!/usr/bin/env python3
"""Check production RX alignment, EEPROM migration, menus and unchanged TX/CW."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from test_frequency import bsp, board, header, typedef


def extract(source, name):
    match = re.search(r'(?:void|int16_t|uint32_t) ' + name + r'\([^)]*\)\s*\{', source)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

constants = '\n'.join(re.search(r'^#define ' + name + r'\s+\S+', header, re.M).group()
    for name in ('RX_USB_MENU', 'RX_LSB_MENU', 'RX_ALIGNMENT_LIMIT', 'RX_ALIGNMENT_MAGIC'))
functions = '\n'.join(extract(board, name) for name in (
    'RX_AlignmentInit', 'RX_AlignmentOffset', 'RX_AlignmentAdjust',
    'MENU_SetUp', 'MENU_SetDown', 'MENU_Ts', 'Get_DDSLO', 'Get_DDSBFO'))
oled = (bsp / 'bsp_oled_api.c').read_text(encoding='latin1')
functions += '\n' + '\n'.join(extract(oled, n) for n in ('Show_RX_Alignment', 'oled_menu_set', 'oled_menu_arrows_set'))
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define MAX_CALLSIGN_LENGTH 10
#define MAX_CWN_LO_OFFSET 9999
#define MORSE_CODE_COUNT 50
#define IF 9000000
#define N 1
''' + constants + '\n' + '\n'.join(typedef(n) for n in (
    'FREQ_ItemDef', 'FREQ_AttrDef', 'RADIO_ModeDef', 'RADIO_AttrDef')) + '\n#pragma pack(push, 1)\n' + typedef('MENU_AttrDef') + '\n#pragma pack(pop)\n' + r'''
MENU_AttrDef MENU;
FREQ_AttrDef FREQ;
RADIO_AttrDef RADIO;
static struct { unsigned func; } FIL;
static bool tx;
static bool RF_IsRx(void) { return !tx; }
static bool RF_IsTx(void) { return tx; }
static void oled_ts(const char *text) { (void)text; }
static char displayed[40];
static void Show_String0705(unsigned x, unsigned y, const void *text) {
    assert(x < 128 && y < 64);
    snprintf(displayed, sizeof(displayed), "%s", (const char *)text);
}
static uint8_t menu_pos, arrows_pos;
static const uint8_t ASCII_1608[96][32];
static void oled_set_screen(unsigned value) { (void)value; }
static void menu_field(unsigned x, unsigned y, unsigned value) {
    assert(x < 128 && y < 64); (void)value;
}
#define Show_CW_KEY_SET menu_field
#define Show_CW_DELAY_SET menu_field
#define Show_CW_FRE_SET menu_field
#define Show_CW_MODE_SET menu_field
#define Show_H_VSWR_P_SET menu_field
#define Show_R_BFO_SET menu_field
#define Show_CW_VOL_SET menu_field
#define Show_REF_SET menu_field
#define Show_CWN_LO_SET menu_field
static void Show_CALL_SET(unsigned x, unsigned y, uint8_t *code, unsigned pos, unsigned all) {
    assert(x < 128 && y < 64 && pos < 10 && code == MENU.callSign); (void)all;
}
static void ascii_1608(unsigned x, unsigned y, const uint8_t *glyph) {
    assert(x < 128 && y < 64); (void)glyph;
}
static void buzzer_freq_set(unsigned value) { (void)value; }
static void sin_para_get(void) {}
static void buzzer_start(void) {}
static void buzzer_stop(void) {}
static void QuietModeOn(void) {}
static void QuietModeOff(void) {}
''' + functions + r'''
int main(void) {
    _Static_assert(sizeof(MENU) == 44, "EEPROM record changed");
    _Static_assert(offsetof(MENU_AttrDef, rxUsbOffset) == 5, "USB field moved");
    _Static_assert(offsetof(MENU_AttrDef, rxLsbOffset) == 7, "LSB field moved");
    _Static_assert(offsetof(MENU_AttrDef, rxAlignmentMagic) == 9, "signature moved");
    _Static_assert(offsetof(MENU_AttrDef, rbfo) == 20, "CW BFO moved");
    _Static_assert(offsetof(MENU_AttrDef, callSign) == 32, "callsign moved");
    _Static_assert(offsetof(MENU_AttrDef, cwnloOffset) == 42, "CW offset moved");
    for (unsigned fill = 0; fill <= 255; ++fill) {
        memset(&MENU, fill, sizeof(MENU));
        MENU_AttrDef old = MENU;
        RX_AlignmentInit();
        assert(!MENU.rxUsbOffset && !MENU.rxLsbOffset);
        assert(MENU.rxAlignmentMagic == RX_ALIGNMENT_MAGIC);
        assert(!memcmp(&MENU, &old, 5));
        assert(!memcmp((uint8_t *)&MENU + 13, (uint8_t *)&old + 13, sizeof(MENU)-13));
    }
    MENU.rxUsbOffset = 120; MENU.rxLsbOffset = -230;
    RX_AlignmentInit();
    assert(MENU.rxUsbOffset == 120 && MENU.rxLsbOffset == -230);
    uint8_t eeprom[sizeof(MENU)];
    memcpy(eeprom, &MENU, sizeof(MENU)); memset(&MENU, 0, sizeof(MENU));
    memcpy(&MENU, eeprom, sizeof(MENU)); RX_AlignmentInit();
    assert(MENU.rxUsbOffset == 120 && MENU.rxLsbOffset == -230);
    MENU.rxUsbOffset = 1001; RX_AlignmentInit();
    assert(!MENU.rxUsbOffset && !MENU.rxLsbOffset);
    MENU.rxLsbOffset = -1001; RX_AlignmentInit();
    assert(!MENU.rxUsbOffset && !MENU.rxLsbOffset);

    for (unsigned pos = RX_USB_MENU; pos <= RX_LSB_MENU; ++pos) {
        FREQ.item = 0;
        MENU_SetUp(pos); MENU_SetUp(pos); MENU_SetDown(pos);
        assert((pos == RX_USB_MENU ? MENU.rxUsbOffset : MENU.rxLsbOffset) == 10);
        FREQ.item = TSItem; MENU_SetUp(pos); MENU_Ts(pos);
        assert((pos == RX_USB_MENU ? MENU.rxUsbOffset : MENU.rxLsbOffset) == 110);
        MENU_SetDown(pos);
        for (unsigned i = 0; i < 50; ++i) MENU_SetUp(pos);
        assert((pos == RX_USB_MENU ? MENU.rxUsbOffset : MENU.rxLsbOffset) == 1000);
        for (unsigned i = 0; i < 50; ++i) MENU_SetDown(pos);
        assert((pos == RX_USB_MENU ? MENU.rxUsbOffset : MENU.rxLsbOffset) == -1000);
    }
    uint8_t callsign[10]; memcpy(callsign, MENU.callSign, 10);
    MENU_SetUp(21); MENU_SetDown(255);
    assert(!memcmp(callsign, MENU.callSign, 10));
    FREQ.item = 0; MENU.callSign[0] = MORSE_CODE_COUNT;
    MENU_SetUp(9); assert(MENU.callSign[0] == 0);
    MENU_SetDown(9); assert(MENU.callSign[0] == MORSE_CODE_COUNT);

    const unsigned frequencies[] = {179900, 705000, 1420000, 2970099};
    MENU.rbfo = 8999570; MENU.cwnloOffset = -1130;
    for (int offset = -1000; offset <= 1000; offset += 10) {
        MENU.rxUsbOffset = offset; MENU.rxLsbOffset = -offset;
        for (unsigned i = 0; i < sizeof(frequencies)/sizeof(*frequencies); ++i) {
            for (unsigned mode = 0; mode < 4; ++mode) {
                RADIO.mode = mode;
                for (unsigned narrow = 0; narrow < 2; ++narrow) {
                    FIL.func = narrow ? N : 0;
                    for (unsigned transmit = 0; transmit < 2; ++transmit) {
                        tx = transmit;
                        int delta = tx ? 0 : mode == RADIO_USB ? offset : mode == LSB ? -offset : 0;
                        int lo = mode == RADIO_USB ? 1500 : mode == LSB ? -1500 :
                            tx ? 0 : narrow ? MENU.cwnloOffset : 800;
                        unsigned bfo = mode == RADIO_USB ? 9001500 : mode == LSB ? 8998500 :
                            tx ? 0 : narrow ? MENU.rbfo : 9001500;
                        assert(Get_DDSLO(frequencies[i]) == frequencies[i]*10 + lo + delta);
                        assert(Get_DDSBFO() == bfo + delta);
                        if (mode == RADIO_USB || mode == LSB)
                            assert(IF + Get_DDSLO(frequencies[i]) - Get_DDSBFO() == frequencies[i]*10);
                    }
                }
            }
        }
    }
    Show_RX_Alignment(20, 1, -1000); assert(!strcmp(displayed, "RX USB: -1000 Hz"));
    Show_RX_Alignment(36, 0, 230); assert(!strcmp(displayed, "RX LSB: +0230 Hz"));
    Show_RX_Alignment(20, 1, 0); assert(!strcmp(displayed, "RX USB: +0000 Hz"));
    for (unsigned pos = 0; pos <= RX_LSB_MENU; ++pos) {
        oled_menu_set(pos); oled_menu_arrows_set(pos);
        assert(menu_pos == pos && arrows_pos == pos);
    }
    for (int pos = RX_LSB_MENU; pos >= 0; --pos) {
        oled_menu_set(pos); oled_menu_arrows_set(pos);
        assert(menu_pos == pos && arrows_pos == pos);
    }
    puts("PASS: RX USB/LSB adjustment, EEPROM migration/layout/persistence, menu limits, labels, constant RF tuning and unchanged TX/CW");
}
'''
with tempfile.TemporaryDirectory(prefix='fx9a-rx-alignment-') as directory:
    source = Path(directory) / 'test.c'
    binary = Path(directory) / 'test'
    source.write_text(program)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fshort-enums', '-fsanitize=undefined,address', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
