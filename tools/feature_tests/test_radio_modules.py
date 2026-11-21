#!/usr/bin/env python3
"""Link production CAT scheduler with mocked DMA transport.

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
#include "radio_cat.h"
#include "bsp_usart.h"
static char events[64];
static unsigned event_count;
static bool ready;
static uint8_t queued, command_reply, sent;
static void event(char tag) { events[event_count++] = tag; events[event_count] = 0; }
static void clear_events(void) { event_count = 0; events[0] = 0; }
void usart_process_rx_commands(void) { event('c'); queued = command_reply; }
bool DMA_IsTxComplete(void) { assert(!strcmp(events, "c")); return ready; }
unsigned char UART_TxBufSize(void) { return queued; }
void CopyTBufToDMA(void) { event('C'); }
void UART_DMAStart(uint8_t length) { event('T'); sent = length; }
int main(void) {
    clear_events(); ready = false; command_reply = 7; sent = 0; RadioCAT_Task();
    assert(!strcmp(events, "c") && !sent);
    clear_events(); ready = true; command_reply = 0; RadioCAT_Task();
    assert(!strcmp(events, "c") && !sent);
    clear_events(); command_reply = 7; RadioCAT_Task();
    assert(!strcmp(events, "cCT") && sent == 7);
    puts("PASS: CAT dispatch precedes DMA service and respects DMA readiness");
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
    for name in ('radio_cat.c',):
        path = temp / name
        path.write_text((bsp / name).read_text(encoding='latin1'))
        sources.append(str(path))
    harness, binary = temp / 'main.c', temp / 'test'
    harness.write_text(program)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fshort-enums', '-fsanitize=undefined,address', '-I', str(temp),
                    *sources, str(harness), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
