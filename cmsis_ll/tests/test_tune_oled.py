#!/usr/bin/env python3
"""Check production SWR-bar drawing and the post-TUNE marker region."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
oled = (root / "RadioStation/BSP/bsp_oled_api.c").read_text(encoding="latin1")


def function(name):
    match = re.search(r"void " + name + r"\([^)]*\)\s*\{", oled)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (oled[end] == "{") - (oled[end] == "}")
        end += 1
    return oled[match.start():end]


program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
uint8_t rx_grid_old, swr_grid_old;
static unsigned drawn, cleared, text_x, text_y;
static char text[16];
static void oled_draw_grid(unsigned x, unsigned y)
{ assert(x >= 1 && x <= 9 && y == 21); ++drawn; }
static void oled_clear_grid(unsigned x, unsigned y)
{ assert(x >= 1 && x <= 9 && y == 21); ++cleared; }
static void Show_String0705(unsigned x, unsigned y, const char *value)
{ text_x = x; text_y = y; strcpy(text, value); }
''' + function("oled_swr_grid") + '\n' + function("oled_tune_result") + r'''
int main(void)
{
    rx_grid_old = 25; /* RX cache must not suppress a growing SWR bar. */
    oled_swr_grid(4);
    assert(swr_grid_old == 4 && drawn == 4 && !cleared);
    oled_swr_grid(4);
    assert(drawn == 4 && !cleared);
    oled_swr_grid(9);
    assert(swr_grid_old == 9 && drawn == 10); /* Includes previous end cell. */
    oled_swr_grid(0);
    assert(!swr_grid_old && cleared == 9);
    oled_tune_result(1, 1);
    assert(!strcmp(text, "LAST SWR"));
    /* Meter scales end at row 39, HI-SWR starts at row 48, VFO at col 36. */
    assert(text_y > 39 && text_y + 6 < 48);
    assert(text_x + (strlen(text) * 6 + 3) / 4 < 36);
    oled_tune_result(1, 0);
    assert(!strcmp(text, "SWR --  "));
    oled_tune_result(0, 0);
    assert(!strcmp(text, "        "));
    puts("PASS: independent SWR bar cache, LAST SWR/unavailable/clear marker and non-overlapping display region");
}
'''
with tempfile.TemporaryDirectory(prefix="fx9a-tune-oled-") as directory:
    source, binary = Path(directory) / "test.c", Path(directory) / "test"
    source.write_text(program)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall",
                    "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
