#!/usr/bin/env python3
"""Inject optical VFO edges during IRQ handling and coarse-step alignment."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
native = (root / 'cmsis_ll').exists()
app = root / ('cmsis_ll/app' if native else 'RadioStation/BSP')
headers = root / ('cmsis_ll/board' if native else 'RadioStation/BSP')
key = (app / 'bsp_key.c').read_text(encoding='latin1')
vfo = (app / ('frequency_state.c' if native else 'radio_vfo.c')).read_text(encoding='latin1')
state = (app / 'radio_state.c').read_text(encoding='latin1')


def function(text, name):
    m = re.search(r'(?:static )?(?:void|bool|int32_t) ' + name + r'\([^)]*\)\s*\{', text)
    assert m, name
    end, depth = m.end(), 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[m.start():end]


alignment = (function(vfo, 'Frequency_AlignCoarse') if 'void Frequency_AlignCoarse(' in vfo else
             'void Frequency_AlignCoarse(void) { FREQ.val /= 100; inject_edge(); FREQ.val *= 100; }')
program = r'''
#include <assert.h>
#include "bsp_board.h"
#define frequency_step original_frequency_step
#include "bsp_frequency.h"
#undef frequency_step
#include "tuning_policy.h"
FREQ_AttrDef FREQ;
VFO_SaveDef VFO;
RADIO_AttrDef RADIO;
ATT_AttrDef ATT;
DispRefreshMgr DRMgr;
AdjustItemMgr AIMgr;
MENU_AttrDef MENU;
static struct { uint32_t IDR; } gpio;
static struct { uint32_t PR, IMR; } exti_regs;
#define EXTI (&exti_regs)
#define EXTI_Line2 4u
#define KEY_23456R1A1BK2A2B_PORT (&gpio)
#define R1B 8u
#define SET 1
static bool inject_during_step, inject_during_align;
static unsigned irq_mask, direction_samples;
static uint32_t __get_PRIMASK(void) { return irq_mask; }
static void __disable_irq(void) { irq_mask = 1; }
static void dispatch(void);
static void inject_edge(void) { EXTI->PR |= 4u; if (!irq_mask) dispatch(); }
static void __set_PRIMASK(uint32_t mask) {
    if (inject_during_align) { inject_during_align = false; inject_edge(); }
    irq_mask = mask;
    if (!mask) dispatch();
}
static int EXTI_GetITStatus(unsigned line) { return !!(EXTI->PR & EXTI->IMR & line); }
static void EXTI_ClearITPendingBit(unsigned line) { EXTI->PR &= ~line; }
static void LL_EXTI_ClearFlag_0_31(unsigned line) { EXTI->PR &= ~line; }
static int GPIO_ReadInputDataBit(void *port, unsigned pin)
{ (void)port; ++direction_samples; return !!(gpio.IDR & pin); }
void MENU_SetUp(uint8_t position) { (void)position; }
void MENU_SetDown(uint8_t position) { (void)position; }
static uint32_t frequency_step(uint32_t frequency, int32_t step, uint32_t lower, uint32_t upper) {
    if (inject_during_step) {
        inject_during_step = false;
        EXTI->PR |= 4u; /* Another edge arrives while the current IRQ is active. */
    }
    return original_frequency_step(frequency, step, lower, upper);
}
''' + re.search(r'uint32_t BAUD\[.*?\n};', state, re.S).group() + '\n' + '\n'.join(
    function(vfo, name) for name in ('ContinuousTuning_Enabled', 'VFO_Save', 'Frequency_Set', 'Frequency_Tune')) + '\n' + \
    function(key, 'optical_frequency_step') + '\n' + function(key, 'EXTI2_IRQHandler') + '\n' + alignment + r'''
static void dispatch(void) {
    unsigned budget = 4;
    while (EXTI->PR & EXTI->IMR & 4u) { assert(budget--); EXTI2_IRQHandler(); }
}
int main(void) {
    FREQ.item = VFOItem | VFOAItem;
    FREQ.numOfbaud = 3;
    MENU.null = CONT_TUNE_ON;
    EXTI->IMR = 4u;
    for (unsigned down = 0; down < 2; ++down) {
        gpio.IDR = down ? R1B : 0;
        FREQ.val = 705000;
        for (unsigned i = 0; i < 1000; ++i) {
            inject_during_step = true;
            EXTI->PR |= 4u; dispatch();
        }
        assert(FREQ.val == (down ? 703000u : 707000u));
    }
    gpio.IDR = 0;
    FREQ.item |= TSItem;
    FREQ.val = 705037;
    inject_during_align = true;
    Frequency_AlignCoarse();
    assert(FREQ.val == 705100u && !irq_mask);
    irq_mask = 1; FREQ.val = 705037;
    Frequency_AlignCoarse();
    assert(FREQ.val == 705000u && irq_mask == 1);
    __set_PRIMASK(0);
    ATT.val = 1; EXTI->PR |= 4u; dispatch();
    assert(FREQ.val == 705000u && !EXTI->PR);
    puts("PASS: VFO retains edges arriving during IRQ work; TS alignment never exposes a scaled frequency and preserves IRQ state");
}
'''
with tempfile.TemporaryDirectory(prefix='fx9a-vfo-irq-') as folder:
    temp = Path(folder)
    for directory in (root / 'RadioStation/BSP', headers):
        for path in directory.glob('*.h'):
            text = path.read_text(encoding='latin1')
            if path.name == 'bsp_board.h':
                for include in ('stm32f10x_conf.h', 'stm32f1xx.h', 'bsp_delay.h'):
                    text = text.replace('#include "' + include + '"', '')
            (temp / path.name).write_text(text)
    source, binary = temp / 'test.c', temp / 'test'
    source.write_text(program)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-function', '-fsanitize=address,undefined', '-fshort-enums',
                    '-I' + str(temp), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
