#!/usr/bin/env python3
"""Run production Mbed saved-settings startup with valid/corrupt EEPROM records."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from test_frequency import bsp, board, function, table, typedef, types

program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp_frequency.h"
#include "settings_validation.h"
#define SIZE_OF_BAUD 10
#define SIZE_OF_MEM 100
#define MAX_CALLSIGN_LENGTH 10
#define MAX_CWN_LO_OFFSET 9999
#define RX_ALIGNMENT_MAGIC 0x31584252u
#define RX_ALIGNMENT_LIMIT 1000
#define CONT_TUNE_ON 0xc1u
''' + types + '\n' + table + '\n' + '\n'.join(typedef(n) for n in (
    'VOL_AttrDef', 'WPM_AttrDef', 'FIL_FuncDef', 'FIL_AttrDef')) + '\n#pragma pack(push, 1)\n' + typedef('MENU_AttrDef') + '\n#pragma pack(pop)\n' + r'''
enum { AUTO = 0 };
static VFO_SaveDef VFO, stored_vfo;
static MENU_AttrDef MENU, stored_menu;
static MEM_AttrDef MEM[SIZE_OF_MEM], stored_mem[SIZE_OF_MEM];
static FIL_AttrDef FIL, CWFilter;
static VOL_AttrDef VOL;
static WPM_AttrDef WPM;
static ATT_AttrDef ATT;
static FREQ_AttrDef FREQ;
static RADIO_AttrDef RADIO;
static DispRefreshMgr DRMgr;
static unsigned defaults;
static uint8_t stored_vol, stored_fil, stored_att, stored_wpm, marker;
static uint8_t e2p_save_read(void) { return marker; }
static void e2p_mem_read(unsigned start, MEM_AttrDef *dest, unsigned count) {
    assert(!start && count == 100); memcpy(dest, stored_mem, sizeof stored_mem);
}
static VFO_SaveDef e2p_vfo_read(void) { return stored_vfo; }
static MENU_AttrDef e2p_menu_read(void) { return stored_menu; }
static uint8_t e2p_vol_read(void) { return stored_vol; }
static uint8_t e2p_fil_read(void) { return stored_fil; }
static uint8_t e2p_att_read(void) { return stored_att; }
static uint8_t e2p_pwm_read(void) { return stored_wpm; }
static void ResetToDefault(void) { ++defaults; }
''' + '\n'.join(function(board, n) for n in ('sanitize_saved_settings',
    'RX_AlignmentInit', 'VFO_Get', 'software_init')) + r'''
int main(void) {
    marker = 0x5a; stored_vol = 60; stored_fil = N; stored_att = 1; stored_wpm = 20;
    for (unsigned b = 0; b < SIZE_OF_BAUD; ++b) {
        stored_vfo.VFOA[b].freq = stored_vfo.VFOB[b].freq = BAUD[b][1];
        stored_vfo.VFOA[b].mode = USB; stored_vfo.VFOB[b].mode = LSB;
    }
    for (unsigned i = 0; i < SIZE_OF_MEM; ++i) {
        stored_mem[i].freq = BAUD[3][1]; stored_mem[i].baud = 3; stored_mem[i].mode = USB;
    }
    stored_vfo.CurrBaudOfVfoA = stored_vfo.CurrBaudOfVfoB = 3;
    stored_menu.delay = 8; stored_menu.vswr = 9; stored_menu.freqOfbuzzer = 600;
    stored_menu.ref = 50000000; stored_menu.cwnloOffset = -1130;
    stored_menu.rxAlignmentMagic = RX_ALIGNMENT_MAGIC; stored_menu.null = 0xc0u;
    software_init();
    assert(!defaults && VOL.val == 60 && WPM.val == 20 && FIL.func == N);
    assert(FREQ.val == BAUD[3][1] && RADIO.mode == USB && MENU.null == 0xc0u);
    for (unsigned fill = 0; fill <= 255; ++fill) {
        memset(&stored_vfo, fill, sizeof stored_vfo);
        memset(&stored_menu, fill, sizeof stored_menu);
        memset(stored_mem, fill, sizeof stored_mem);
        stored_vol = stored_wpm = stored_fil = stored_att = fill;
        MENU_AttrDef original_menu = stored_menu;
        software_init();
        assert(!defaults && VOL.val <= 60 && WPM.val >= 4 && WPM.val <= 60);
        assert(FIL.func == W || FIL.func == N);
        assert(FREQ.numOfbaud < SIZE_OF_BAUD && FREQ.val >= BAUD[0][0] && FREQ.val <= BAUD[9][2]);
        assert(MENU.delay >= 1 && MENU.delay <= 15 && MENU.vswr <= 9);
        for (unsigned b = 0; b < SIZE_OF_BAUD; ++b) {
            assert(fx9a_mode_valid(VFO.VFOA[b].mode) && fx9a_mode_valid(VFO.VFOB[b].mode));
            assert(fx9a_saved_frequency(VFO.VFOA[b].freq, BAUD[0][0], BAUD[9][2]));
        }
        for (unsigned i = 0; i < SIZE_OF_MEM; ++i)
            assert(MEM[i].baud < SIZE_OF_BAUD && fx9a_mode_valid(MEM[i].mode));
        /* No EEPROM write callbacks exist: repair must remain in RAM. */
        assert(!memcmp(&stored_menu, &original_menu, sizeof stored_menu));
    }
    puts("PASS: production startup restores volume 60 and validates all-byte corrupt legacy settings without EEPROM writes");
}
'''
with tempfile.TemporaryDirectory(prefix='fx9a-settings-startup-') as directory:
    source, binary = Path(directory) / 'test.c', Path(directory) / 'test'
    source.write_text(program)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fshort-enums', '-fsanitize=undefined,address', '-I', str(bsp),
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
