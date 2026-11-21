#!/usr/bin/env python3
"""Exercise production EEPROM transactions and deferred CAT receive mailbox."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
bsp = root / 'RadioStation/BSP'

def function(source, name):
    m = re.search(r'(?:void|bool|uint8_t) ' + name + r'\([^)]*\)\s*\{', source)
    assert m, name
    end, depth = m.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[m.start():end]

source = (bsp / 'bsp_24lc16.c').read_text(encoding='latin1')
program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define E2P_CTLCODE 0xa0
#define E2P_INS_WRITE 0
#define E2P_INS_READ 1
#define E2P_ADDR_VOL 992
#define E2P_ADDR_SAVE 1600
#define NOACK 1
static unsigned starts, stops, acks, ack_index, fail_ack, reads, nacks, delays;
static uint8_t sent[128], received[16];
static unsigned sent_count;
static void i2c_start(void) { ++starts; }
static void i2c_stop(void) { ++stops; }
static void i2c_tx(uint8_t value) { assert(sent_count < sizeof sent); sent[sent_count++] = value; }
static uint8_t i2c_getack(void) { return ++ack_index == fail_ack; }
static uint8_t i2c_rx(void) { return received[reads++]; }
static void i2c_ack(void) { ++acks; }
static void i2c_noack(void) { ++nacks; }
static void delay_ms(unsigned ms) { assert(ms == 8); ++delays; }
static void reset(void) {
    starts = stops = acks = ack_index = fail_ack = reads = nacks = delays = sent_count = 0;
}
''' + '\n'.join(function(source, n) for n in ('e2p_write_mulbyte', 'e2p_write',
    'e2p_read', 'e2p_vol_write', 'e2p_vol_read', 'e2p_save_read')) + r'''
int main(void) {
    reset(); received[0] = 60;
    assert(e2p_vol_read() == 60);
    assert(sent_count == 3 && sent[0] == 0xa6 && sent[1] == 0xe0 && sent[2] == 0xa7);
    assert(starts == 2 && stops == 1 && nacks == 1);
    reset(); received[0] = 0x5a;
    assert(e2p_save_read() == 0x5a);
    assert(sent[0] == 0xac && sent[1] == 0x40 && sent[2] == 0xad);
    reset(); e2p_vol_write(37);
    assert(starts == 1 && stops == 1 && sent_count == 3);
    assert(sent[0] == 0xa6 && sent[1] == 0xe0 && sent[2] == 37 && delays == 1);
    for (unsigned phase = 1; phase <= 3; ++phase) {
        reset(); fail_ack = phase; uint8_t value = 0x55;
        assert(!e2p_read(992, &value, 1));
        assert(value == 0x55 && stops == 1 && !reads);
    }
    reset(); uint8_t data[18] = {0};
    assert(e2p_write(7, data, sizeof data));
    assert(starts == 4 && stops == 4 && delays == 4 && sent_count == 26);
    for (unsigned phase = 1; phase <= 3; ++phase) {
        reset(); fail_ack = phase;
        assert(!e2p_write(7, data, sizeof data));
        assert(starts == 1 && stops == 1 && !delays);
    }
    reset(); assert(e2p_read(0, data, 0) && !starts);
    reset(); received[0] = 12; received[1] = 34;
    assert(e2p_read(992, data, 2));
    assert(data[0] == 12 && data[1] == 34 && acks == 1 && nacks == 1);
    puts("PASS: saved volume/marker addresses, save transaction, page boundaries and EEPROM NACK recovery");
}
'''

uart = (bsp / 'bsp_usart.c').read_text(encoding='latin1')
uart_program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char UartRxLen, UartRxBuf[32];
static bool UartRxDiscard;
static uint32_t UartRxOverflowCount, UartCatQueueDropCount;
static uint8_t CatPending[32];
static volatile uint8_t CatPendingLen;
static unsigned irq_mask, calls, parsed_len;
static uint8_t parsed[32];
static void __DMB(void) {}
static unsigned __get_PRIMASK(void) { return irq_mask; }
static void __set_PRIMASK(unsigned value) { irq_mask = value; }
static void __disable_irq(void) { irq_mask = 1; }
static void Kenwood_CmdParser(uint8_t *buf, uint8_t len) {
    ++calls; parsed_len = len; memcpy(parsed, buf, len);
}
''' + '\n'.join(function(uart, n) for n in ('usart_rx_analysis', 'usart_process_rx_commands')) + r'''
static void feed(const char *text) { while (*text) usart_rx_analysis((uint8_t)*text++); }
int main(void) {
    feed("AG0128;"); assert(!calls && CatPendingLen == 6);
    usart_process_rx_commands();
    assert(calls == 1 && parsed_len == 6 && !memcmp(parsed, "AG0128", 6) && !irq_mask);
    for (unsigned i = 0; i < 33; ++i) usart_rx_analysis('A');
    feed("TX0;"); usart_process_rx_commands();
    assert(calls == 1 && UartRxOverflowCount == 1 && !UartRxLen);
    feed("CA;"); feed("TX0;");
    assert(UartCatQueueDropCount == 1);
    usart_process_rx_commands(); assert(calls == 2 && parsed_len == 2 && !memcmp(parsed, "CA", 2));
    feed("IF;"); irq_mask = 1;
    usart_process_rx_commands(); assert(calls == 3 && irq_mask == 1);
    feed(";A;"); usart_process_rx_commands(); assert(calls == 3);
    puts("PASS: CAT defers parsing to main, overflow discards through delimiter, queue drops and IRQ mask restoration");
}
'''
cat = (bsp / 'bsp_kenwood.c').read_text(encoding='latin1')
cat_program = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum { LSB = 1, USB = 2, VFOItem = 1, VFOAItem = 2, RITItem = 4, XITItem = 8,
       VOLItem = 16, MENUSETItem = 32, MEMSETItem = 64 };
static struct { unsigned item, numOfmem; uint32_t val; } FREQ;
static struct { int32_t RIT, XIT; } VFO;
static struct { unsigned mode; } RADIO;
static struct { unsigned item; } AIMgr, DRMgr;
static struct { unsigned val; } VOL;
static bool tx;
static bool RF_IsTx(void) { return tx; }
static bool RF_IsRx(void) { return !tx; }
static uint8_t response[128];
static unsigned response_len;
static void UartTx(uint8_t *data, unsigned len) {
    assert(len < sizeof response); memcpy(response, data, len); response[len] = 0; response_len = len;
}
''' + '\n'.join(function(cat, n) for n in ('DecToString', 'Kenwood_BuildTransceiverStatus',
    'Kenwood_MemoryDataAnswer', 'Kenwood_AFGainSet', 'Kenwood_AFGainAnswer')) + r'''
int main(void) {
    FREQ.val = 705000; FREQ.item = VFOItem | VFOAItem; RADIO.mode = USB;
    Kenwood_BuildTransceiverStatus();
    assert(response_len <= 40 && !memcmp(response + 18, "00000", 5));
    FREQ.item |= RITItem; VFO.RIT = -123; Kenwood_BuildTransceiverStatus();
    assert(!memcmp(response + 18, "-0123", 5));
    FREQ.item |= XITItem; VFO.XIT = 456; Kenwood_BuildTransceiverStatus();
    assert(!memcmp(response + 18, "00456", 5));
    Kenwood_MemoryDataAnswer(); assert(response_len > 40 && response_len <= 64);
    for (unsigned value = 0; value <= 255; ++value) {
        Kenwood_AFGainSet(value);
        assert(VOL.val == (value * 60u + 127u) / 255u);
        Kenwood_AFGainAnswer();
        unsigned reported = (response[3]-'0')*100 + (response[4]-'0')*10 + response[5]-'0';
        assert(response_len == 7 && reported == (VOL.val*255u + 30u)/60u);
    }
    VOL.val = 37; tx = true; Kenwood_AFGainSet(0); assert(VOL.val == 37);
    puts("PASS: production CAT IF offset fields, M4 response bounds and full-range AG gain scaling");
}
'''
with tempfile.TemporaryDirectory(prefix='fx9a-eeprom-uart-') as directory:
    for name, code in (('eeprom', program), ('uart', uart_program), ('cat', cat_program)):
        path, binary = Path(directory) / (name + '.c'), Path(directory) / name
        path.write_text(code)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=undefined,address', str(path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
