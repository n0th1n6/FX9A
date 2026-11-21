#!/usr/bin/env python3
"""Run production tuning/CAT/recall code with mocked hardware on the host."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import sys
sys.dont_write_bytecode = True
from radio_sources import load_radio_source

root = Path(__file__).resolve().parents[1]
bsp = root / "RadioStation/BSP"
board = load_radio_source(bsp)
header = (bsp / "bsp_board.h").read_text(encoding="latin1")
key = (bsp / "bsp_key.c").read_text(encoding="latin1")
cat = (bsp / "bsp_kenwood.c").read_text(encoding="latin1")


def function(source, name):
    match = re.search(r"(?:static )?(?:void|bool|int32_t) " + name + r"\([^)]*\)\s*\{", source)
    assert match, name
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def typedef(name):
    end = header.index("}" + name + ";") + len(name) + 2
    start = header.rfind("typedef ", 0, end)
    return header[start:end]


types = "\n".join(typedef(n) for n in (
    "FREQ_ItemDef", "FREQ_AttrDef", "RADIO_ModeDef", "RADIO_AttrDef",
    "VFO_SaveInfo", "VFO_SaveDef", "MEM_AttrDef", "ATT_AttrDef",
    "DispRefreshItem", "DispRefreshMgr", "AdjustItemMgr"))
table = re.search(r"uint32_t BAUD\[.*?\n};", board, re.S).group()
functions = "\n".join(function(board, n) for n in (
    "ContinuousTuning_Enabled", "VFO_Save", "Frequency_Set", "Frequency_Tune", "VFO_Get", "MEM_Get"))
functions += "\n" + function(cat, "Kenwood_VFOSet")
functions += "\n" + function(key, "optical_frequency_step")
functions += "\n" + function(key, "EXTI2_IRQHandler")
program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp_frequency.h"
#include "tuning_policy.h"
#define SIZE_OF_BAUD 10
#define SIZE_OF_MEM 100
#define UPPER_OF_RIT 9990
#define LOWER_OF_RIT -9990
#define UPPER_OF_XIT 9990
#define LOWER_OF_XIT -9990
''' + types + "\n" + table + r'''
FREQ_AttrDef FREQ;
VFO_SaveDef VFO;
RADIO_AttrDef RADIO;
MEM_AttrDef MEM[SIZE_OF_MEM];
ATT_AttrDef ATT;
DispRefreshMgr DRMgr;
AdjustItemMgr AIMgr;
static bool transmitting;
static int direction, menu_up, menu_down;
#define EXTI_Line2 2
#define SET 1
#define KEY_23456R1A1BK2A2B_PORT 0
#define R1B 8
static int EXTI_GetITStatus(int line) { (void)line; return SET; }
static void EXTI_ClearITPendingBit(int line) { (void)line; }
static int GPIO_ReadInputDataBit(int port, int pin) {
    (void)port; (void)pin; return direction;
}
static bool RF_IsTx(void) { return transmitting; }
static struct { uint8_t numOfmenu, null; } MENU;
static void MENU_SetUp(uint8_t menu) { (void)menu; ++menu_up; }
static void MENU_SetDown(uint8_t menu) { (void)menu; ++menu_down; }
''' + functions + r'''
static void tick(bool up) { direction = !up; EXTI2_IRQHandler(); }
int main(void) {
    const uint32_t minimum = BAUD[0][0], maximum = BAUD[9][2];
    FREQ.item = VFOItem | VFOAItem;
    RADIO.mode = USB;

    /* Actual encoder path must pass the old 40 m upper edge and all gaps. */
    assert(Frequency_Set(730099)); tick(true);
    assert(FREQ.val == 730100 && FREQ.numOfbaud == 3);
    assert(Frequency_Set(750000)); tick(true); tick(false);
    assert(FREQ.val == 750000 && FREQ.numOfbaud == 3);

    for (unsigned i = 1; i < SIZE_OF_BAUD; ++i) {
        uint32_t start = BAUD[i][0];
        Frequency_Set(start - 1); tick(true);
        assert(FREQ.val == start && FREQ.numOfbaud == i);
        assert(RADIO.mode == USB); /* No saved-frequency/mode recall. */
        tick(false);
        assert(FREQ.val == start - 1 && FREQ.numOfbaud == i - 1);
        assert(Kenwood_VFOSet((int32_t)start));
        assert(FREQ.val == start && FREQ.numOfbaud == i);
    }

    /* Sweep every 10 Hz frequency, including all previously rejected gaps. */
    uint8_t expected = 0;
    for (uint32_t f = minimum; f <= maximum; ++f) {
        if (expected < 9 && f >= BAUD[expected + 1][0]) ++expected;
        assert(frequency_band(f, BAUD, SIZE_OF_BAUD) == expected);
    }

    FREQ.item |= TSItem;
    Frequency_Set(BAUD[4][0] - 50); tick(true);
    assert(FREQ.val == BAUD[4][0] + 50 && FREQ.numOfbaud == 4);
    Frequency_Set(maximum - 1); tick(true); tick(true);
    assert(FREQ.val == maximum);
    Frequency_Set(minimum + 1); tick(false); tick(false);
    assert(FREQ.val == minimum);
    assert(frequency_step(minimum, INT32_MIN, minimum, maximum) == minimum);
    assert(frequency_step(maximum, INT32_MAX, minimum, maximum) == maximum);
    FREQ.item &= ~TSItem;

    /* Menu and offset controls must still work at the overall tuning ends. */
    AIMgr.item = MENUSETItem; tick(true); tick(false);
    assert(menu_up == 1 && menu_down == 1 && FREQ.val == minimum);
    AIMgr.item = 0; FREQ.item |= RITItem; VFO.RIT = 0;
    tick(true); assert(VFO.RIT == 1 && FREQ.val == minimum);
    tick(false); assert(VFO.RIT == 0);
    FREQ.item &= ~RITItem; FREQ.item |= XITItem;
    tick(true); assert(VFO.XIT == 1 && FREQ.val == minimum);
    tick(false); assert(VFO.XIT == 0); FREQ.item &= ~XITItem;

    assert(Kenwood_VFOSet(750000));
    uint32_t before = FREQ.val;
    assert(!Kenwood_VFOSet(-1));
    assert(!Kenwood_VFOSet((int32_t)minimum - 1));
    assert(!Kenwood_VFOSet((int32_t)maximum + 1));
    transmitting = true; assert(!Kenwood_VFOSet(1000000));
    transmitting = false; AIMgr.item = MENUSETItem;
    assert(!Kenwood_VFOSet(1000000)); AIMgr.item = 0;
    assert(FREQ.val == before);

    /* Crossing preserves the old slot; the new frequency survives A/B recall. */
    Frequency_Set(BAUD[4][0] - 1); VFO_Save(); tick(true); VFO_Save();
    assert(VFO.VFOA[3].freq == BAUD[4][0] - 1);
    assert(VFO.VFOA[4].freq == BAUD[4][0]);
    assert(VFO.CurrBaudOfVfoA == 4);
    FREQ.item = VFOItem | VFOBItem; Frequency_Set(750000); VFO_Save();
    FREQ.item = VFOItem | VFOAItem;
    FREQ.numOfbaud = VFO.CurrBaudOfVfoA; VFO_Get();
    assert(FREQ.val == BAUD[4][0] && FREQ.numOfbaud == 4);
    FREQ.item = VFOItem | VFOBItem;
    FREQ.numOfbaud = VFO.CurrBaudOfVfoB; VFO_Get();
    assert(FREQ.val == 750000 && FREQ.numOfbaud == 3);
    MEM[0].freq = 750000; MEM[0].baud = 9; MEM[0].mode = LSB;
    FREQ.numOfmem = 0; MEM_Get();
    assert(FREQ.val == 750000 && FREQ.numOfbaud == 3 && RADIO.mode == LSB);
    /* Band-limited policy must govern both encoder and CAT, including gaps. */
    FREQ.item = VFOItem | VFOAItem; FREQ.numOfbaud = 3;
    FREQ.val = BAUD[3][2]; MENU.null = 0xc0u;
    tick(true); assert(FREQ.val == BAUD[3][2]);
    assert(!Kenwood_VFOSet(BAUD[3][2] + 1));
    assert(!Kenwood_VFOSet(BAUD[4][1]));
    FREQ.val = BAUD[3][0]; tick(false); assert(FREQ.val == BAUD[3][0]);
    assert(Kenwood_VFOSet(BAUD[3][1])); VFO_Save();
    FREQ.item |= RITItem; tick(true); assert(VFO.RIT == 1);
    FREQ.item &= ~RITItem; FREQ.item |= XITItem; tick(true); assert(VFO.XIT == 1);
    FREQ.item &= ~XITItem;
    MENU.null = 0xc1u; assert(Kenwood_VFOSet(BAUD[4][1]));
    MENU.null = 0xffu; assert(ContinuousTuning_Enabled());
    puts("PASS: ON/OFF tuning policy, continuous tuning, all boundaries, range ends, encoder, CAT, offsets, menus, VFO and memory recall");
}
'''
def main():
    with tempfile.TemporaryDirectory(prefix="fx9a-frequency-") as directory:
        source = Path(directory) / "test.c"
        binary = Path(directory) / "test"
        source.write_text(program)
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                        "-Werror", "-fsanitize=undefined", "-I", str(bsp), str(source),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
