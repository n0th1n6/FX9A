#!/usr/bin/env python3
"""Link production state/VFO/CAT translation units with an in-memory EEPROM.

Use production headers and types, omitting only the device/delay includes from
bsp_board.h. Hardware transport calls are mocked at the existing BSP boundary.
The module .c files are compiled separately, rather than extracting functions.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
bsp = root / 'RadioStation/BSP'
program = r'''
#include <assert.h>
#include "radio_state.h"
#include "radio_vfo.h"
#include "radio_cat.h"
#include "bsp_24lc16.h"
#include "bsp_iambic_keyer.h"
#include "bsp_usart.h"
static MEM_AttrDef saved_mem[SIZE_OF_MEM];
static VFO_SaveDef saved_vfo;
static MENU_AttrDef saved_menu;
static uint8_t marker, volume, att, wpm;
static FIL_FuncDef filter;
static unsigned memory_count;
static char events[64];
static unsigned event_count;
static bool ready;
static uint8_t queued, command_reply, sent;
static void event(char tag) {
    assert(event_count + 1 < sizeof events);
    events[event_count++] = tag; events[event_count] = 0;
}
static void clear_events(void) { event_count = 0; events[0] = 0; }
void e2p_save_write(uint8_t value) { event('S'); marker = value; }
uint8_t e2p_save_read(void) { return marker; }
void e2p_mem_write(uint8_t start, MEM_AttrDef *data, uint8_t count) {
    assert(!start && count <= SIZE_OF_MEM); event('m'); memory_count = count;
    memcpy(saved_mem, data, count * sizeof *data);
}
void e2p_mem_read(uint8_t start, MEM_AttrDef *data, uint8_t count) {
    assert(!start && count == SIZE_OF_MEM); memcpy(data, saved_mem, sizeof saved_mem);
}
void e2p_vfo_write(VFO_SaveDef *data) { event('v'); saved_vfo = *data; }
VFO_SaveDef e2p_vfo_read(void) { return saved_vfo; }
void e2p_fil_write(FIL_FuncDef value) { event('f'); filter = value; }
FIL_FuncDef e2p_fil_read(void) { return filter; }
void e2p_vol_write(uint8_t value) { event('o'); volume = value; }
uint8_t e2p_vol_read(void) { return volume; }
void e2p_att_write(uint8_t value) { event('a'); att = value; }
uint8_t e2p_att_read(void) { return att; }
void e2p_pwm_write(uint8_t value) { event('w'); wpm = value; }
uint8_t e2p_pwm_read(void) { return wpm; }
void e2p_menu_write(MENU_AttrDef *data) { event('u'); saved_menu = *data; }
MENU_AttrDef e2p_menu_read(void) { return saved_menu; }
void usart_process_rx_commands(void) { event('c'); queued = command_reply; }
bool DMA_IsTxComplete(void) { assert(!strcmp(events, "c")); return ready; }
unsigned char UART_TxBufSize(void) { return queued; }
void CopyTBufToDMA(void) { event('C'); }
void UART_DMAStart(uint8_t length) { event('T'); sent = length; }
int main(void) {
    _Static_assert(sizeof(VFO_SaveInfo) == 5, "VFO record ABI");
    _Static_assert(sizeof(MEM_AttrDef) == 6, "Memory record ABI");
    _Static_assert(sizeof(MENU_AttrDef) == 44, "Menu record ABI");
    RadioState_ResetDefaults();
    assert(!strcmp(events, "Smvfoawu") && memory_count == SIZE_OF_MEM);
    assert(marker == 0x5a && volume == 2);
    clear_events(); RadioState_Load();
    assert(!event_count && VOL.val == 2 && (DRMgr.item & ALLItem));
    assert(FREQ.val == BAUD[3][1] && FREQ.numOfbaud == 3);

    assert(Frequency_Set(750000)); VFO_Save();
    FREQ.item = VFOItem | VFOBItem;
    assert(Frequency_Set(1420000)); VFO_Save();
    FREQ.numOfmem = 7; MEM_Save(); VOL.val = 37;
    assert(!event_count); /* RAM edits do not save on every encoder step. */
    RadioState_Save();
    assert(!strcmp(events, "mvfoawu") && memory_count == 8 && volume == 37);
    assert(saved_vfo.VFOA[3].freq == 750000 && saved_vfo.VFOB[5].freq == 1420000);
    assert(saved_mem[7].freq == 1420000 && saved_mem[7].baud == 5);
    memset(&VFO, 0, sizeof VFO); memset(MEM, 0, sizeof MEM);
    memset(&MENU, 0, sizeof MENU); memset(&FREQ, 0, sizeof FREQ); VOL.val = 0;
    clear_events(); RadioState_Load();
    assert(!event_count && VOL.val == 37);
    assert(VFO.VFOA[3].freq == 750000 && VFO.VFOB[5].freq == 1420000);
    FREQ.item = VFOItem | VFOBItem; FREQ.numOfbaud = VFO.CurrBaudOfVfoB; VFO_Get();
    assert(FREQ.val == 1420000);
    FREQ.numOfmem = 7; MEM_Get(); assert(FREQ.val == 1420000);
    clear_events(); RadioState_ResetDefaults();
    assert(memory_count == SIZE_OF_MEM && volume == 2);

    clear_events(); ready = false; command_reply = 7; sent = 0; RadioCAT_Task();
    assert(!strcmp(events, "c") && !sent);
    clear_events(); ready = true; command_reply = 0; RadioCAT_Task();
    assert(!strcmp(events, "c") && !sent);
    clear_events(); command_reply = 7; RadioCAT_Task();
    assert(!strcmp(events, "cCT") && sent == 7);
    puts("PASS: linked state/VFO modules preserve reset/shutdown record order, RAM-only edits and reload; CAT dispatch precedes DMA service");
}
'''
with tempfile.TemporaryDirectory(prefix='fx9a-radio-modules-') as directory:
    temp = Path(directory)
    for path in bsp.glob('*.h'):
        text = path.read_text(encoding='latin1')
        if path.name == 'bsp_board.h':
            text = text.replace('#include "stm32f10x_conf.h"', '').replace('#include "bsp_delay.h"', '')
        (temp / path.name).write_text(text)
    sources = []
    for name in ('radio_state.c', 'radio_vfo.c', 'radio_cat.c'):
        path = temp / name
        path.write_text((bsp / name).read_text(encoding='latin1'))
        sources.append(str(path))
    harness, binary = temp / 'main.c', temp / 'test'
    harness.write_text(program)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fshort-enums', '-fsanitize=undefined,address', '-I', str(temp),
                    *sources, str(harness), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
