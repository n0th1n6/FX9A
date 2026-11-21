#include <assert.h>
#include <stdio.h>
#include "bsp_board.h"
#include "radio_state.h"
#include "settings_store.h"
#include "bsp_kenwood.h"
#include "bsp_key.h"
uint32_t Get_DDSLO(uint32_t frequency);
uint32_t Get_DDSBFO(void);
static uint8_t ee[2048];
uint32_t settings_irq_mask;
static bool encoder_during_save, corrupt_write;
static bool display_off, power_removed, save_error;
void oled_ts(uint8_t *s) { save_error = !strcmp((char *)s,"ERR"); }
void oled_display_off(void) { display_off = true; }
void delay_ms(uint32_t ms) { assert(ms == 100); }
void power_off(void) { power_removed = true; }
static int fail_after = -1, operations, writes;
static bool io_fail, tx, tune;
static int bytes_left = -1, bytes_written;
static unsigned stops, tx_calls;
static char reply[128];
volatile uint32_t CatMalformedCount;
uint8_t rx_grid_old, swr_grid_old, rf_grid_old;
bool e2p_read(uint16_t a, uint8_t *d, uint16_t n) {
    assert(a+n <= sizeof ee); ++operations;
    if (io_fail || (fail_after >= 0 && operations > fail_after)) return false;
    memcpy(d, ee+a, n); return true;
}
bool e2p_write(uint16_t a, uint8_t *d, uint16_t n) {
    assert(a+n <= sizeof ee); ++operations; ++writes;
    if (encoder_during_save) VOL.val = VOL.val == 51 ? 52 : 51;
    if (io_fail || (fail_after >= 0 && operations > fail_after)) return false;
    for (unsigned i=0; i<n; ++i) {
        if (bytes_left == 0) return false;
        if (bytes_left > 0) --bytes_left;
        ee[a+i] = d[i]; ++bytes_written;
    }
    if (corrupt_write) ee[a] ^= 1;
    return true;
}
bool RF_IsTx(void) { return tx; }
bool RF_IsRx(void) { return !tx; }
void RF_ToTx(void) { tx = true; ++tx_calls; }
void RF_ToRx(void) { tx = false; }
bool Tune_IsActive(void) { return tune; }
void Tune_Stop(void) { if(tune) ++stops; tune = false; tx = false; }
bool Tune_Start(void) { tune = true; tx = true; return true; }
void keyer_auto_setup(void) {}
void oled_vfo(const char *s) { (void)s; }
void KEY_SoftwareSet(uint8_t n, KEY_State state) { (void)n; (void)state; }
void UartTx(uint8_t *d, uint8_t n) {
    assert(n < sizeof reply); memcpy(reply, d, n); reply[n] = 0;
    assert(n && reply[n-1] == ';'); assert(strlen(reply) == n);
}
static void command(const char *s) {
    reply[0] = 0; Kenwood_CmdParser((uint8_t *)s, (uint8_t)strlen(s));
}
int main(void) {
    memset(ee, 0xff, sizeof ee);
    int before = writes; LOAD();
    assert(writes == before && VOL.val == 2); /* First boot is read-only. */
    VOL.val = 37; assert(RadioState_Save()); assert(Settings_Load() == SETTINGS_VALID);
    VOL.val = 0; LOAD(); assert(VOL.val == 37);
    encoder_during_save = true; VOL.val = 37; assert(RadioState_Save());
    encoder_during_save = false; assert(Settings_Load() == SETTINGS_VALID && VOL.val == 37);
    settings_irq_mask = 1; assert(RadioState_Save()); assert(settings_irq_mask == 1);
    settings_irq_mask = 0;
    /* Removing metadata simulates a legacy image, retaining old offsets. */
    memset(ee+1152, 0xff, 20); assert(Settings_Load() == SETTINGS_LEGACY);
    assert(RadioState_Save()); uint8_t baseline[2048]; memcpy(baseline, ee, sizeof ee);
    ee[992] ^= 1; assert(Settings_Load() == SETTINGS_CORRUPT);
    before = writes; LOAD(); assert(writes == before && VOL.val == 2 && !RadioState_Save());
    memcpy(ee, baseline, sizeof ee); LOAD(); assert(VOL.val == 37);
    io_fail = true; before = writes; LOAD();
    assert(writes == before && VOL.val == 2 && !RadioState_Save()); io_fail = false;
    LOAD(); assert(VOL.val == 37);
    corrupt_write = true; assert(!RadioState_Save()); corrupt_write = false;
    memcpy(ee, baseline, sizeof ee); LOAD();
    /* Inject a failure at every operation in a complete save, including commit.
     * No failed save may load changed bytes as an intact legacy/valid snapshot.
     */
    operations = 0; assert(RadioState_Save()); int total = operations;
    for (int i = 0; i < total; ++i) {
        memcpy(ee, baseline, sizeof ee); fail_after = -1; LOAD(); VOL.val = 51;
        operations = 0; fail_after = i; assert(!RadioState_Save()); fail_after = -1;
        SettingsResult r = Settings_Load();
        assert(r == SETTINGS_CORRUPT || (r == SETTINGS_VALID && (VOL.val == 37 || VOL.val == 51)));
    }
    memcpy(ee, baseline, sizeof ee); LOAD(); bytes_written = 0;
    assert(RadioState_Save()); int full_bytes = bytes_written;
    for (int i=0; i<full_bytes; ++i) {
        memcpy(ee, baseline, sizeof ee); LOAD(); VOL.val = 51;
        bytes_left = i; assert(!RadioState_Save()); bytes_left = -1;
        SettingsResult r = Settings_Load();
        assert(r == SETTINGS_CORRUPT || (r == SETTINGS_VALID && (VOL.val == 37 || VOL.val == 51)));
    }
    memcpy(ee, baseline, sizeof ee); LOAD();
    for (unsigned i=0; i<20; ++i) {
        memcpy(ee, baseline, sizeof ee); ee[1152+i] ^= 1;
        assert(Settings_Load() == SETTINGS_CORRUPT);
    }
    memcpy(ee, baseline, sizeof ee); ee[992] ^= 1; LOAD();
    assert(!RadioState_Save()); RESET();
    assert(Settings_Load() == SETTINGS_VALID && VOL.val == 2);
    memcpy(ee, baseline, sizeof ee); LOAD();
    /* Calibration region stays byte-for-byte intact. */
    for (unsigned i=1104; i<1134; ++i) assert(ee[i] == 0xff);
    for (unsigned fill=0; fill<256; ++fill) {
        memset(ee, fill, 1068); ee[1600] = 0x5a; memset(ee+1152, 0xff, 20);
        before = writes; LOAD(); assert(writes == before);
        assert(VOL.val <= 60 && WPM.val >= 4 && WPM.val <= 60);
        assert(FREQ.numOfbaud < SIZE_OF_BAUD);
        assert(FREQ.val >= BAUD[0][0] && FREQ.val <= BAUD[9][2]);
    }
    memcpy(ee, baseline, sizeof ee); LOAD();
    MENU.rxUsbOffset = 120; MENU.rxLsbOffset = -230;
    MENU.cwnloOffset = -1130; MENU.rbfo = 8999570;
    for (unsigned band=0; band<SIZE_OF_BAUD; ++band) {
        uint32_t f=BAUD[band][1];
        for (unsigned transmitting=0; transmitting<2; ++transmitting) {
            tx = transmitting; tune = false;
            RADIO.mode = (RADIO_ModeDef)2;
            assert(Get_DDSLO(f) == f*10+1500+(tx ? 0 : 120));
            assert(Get_DDSBFO() == 9001500u+(tx ? 0 : 120));
            RADIO.mode = LSB;
            assert(Get_DDSLO(f) == f*10-1500-(tx ? 0 : 230));
            assert(Get_DDSBFO() == 8998500u-(tx ? 0 : 230));
            RADIO.mode = CWA; FIL.func = W;
            assert(Get_DDSLO(f) == f*10+(tx ? 0 : 800));
            assert(Get_DDSBFO() == (tx ? 0 : 9001500u));
            FIL.func = N;
            assert(Get_DDSLO(f) == f*10-(tx ? 0 : 1130));
            assert(Get_DDSBFO() == (tx ? 0 : MENU.rbfo));
            tune = true;
            assert(Get_DDSLO(f) == f*10 && !Get_DDSBFO());
        }
    }
    tune = tx = false; FIL.func = W;
    isRun = true; io_fail = true; power_button();
    assert(isRun && save_error && !power_removed && !display_off);
    io_fail = false; power_button();
    assert(!isRun && power_removed && display_off);
    isRun = true;
    command("KY"); assert(!strcmp(reply,"KY0;"));
    command("RA"); assert(!strcmp(reply,"RA0000;"));
    command("BD99"); assert(!strcmp(reply,"?;"));
    command("BD05"); assert(FREQ.numOfbaud == 5 && VFO.CurrBaudOfVfoA == 5);
    command("MC008"); assert(FREQ.numOfmem == 8);
    command("MD3"); assert(RADIO.mode == CWA || RADIO.mode == CWB);
    command("MD"); assert(!strcmp(reply,"MD3;"));
    command("MD9"); assert(!strcmp(reply,"?;"));
    command("KS999"); assert(WPM.val == 60);
    command("KS000"); assert(WPM.val == 4);
    command("AG0999"); assert(!strcmp(reply,"?;") && VOL.val == 37);
    FREQ.item = VFOItem | VFOAItem; FREQ.numOfbaud = 3; VFO.CurrBaudOfVfoA = 3; VFO_Get();
    uint32_t active = FREQ.val;
    command("FB00014200000"); assert(FREQ.val == active && VFO.VFOB[VFO.CurrBaudOfVfoB].freq == 1420000);
    command("FB"); assert(!strcmp(reply,"FB00014200000;"));
    MENU.null = CONT_TUNE_OFF;
    command("FB00007100000"); assert(VFO.VFOB[VFO.CurrBaudOfVfoB].freq == 1420000 && FREQ.val == active);
    command("FA00014200000"); assert(FREQ.val == active);
    MENU.null = CONT_TUNE_ON;
    command("FA99999999999"); assert(FREQ.val == active && !strcmp(reply,"?;"));
    command("FA00007100000"); assert(FREQ.val == 710000);
    FREQ.item |= RITItem; VFO.RIT = 0;
    command("RU00100"); assert(VFO.RIT == 10);
    command("IF"); assert(!memcmp(reply+18,"+0100",5) && strlen(reply) == 38);
    command("RD00200"); assert(VFO.RIT == -10);
    command("IF"); assert(!memcmp(reply+18,"-0100",5));
    command("RU99999"); assert(VFO.RIT == 999);
    tx = true; command("RC"); assert(VFO.RIT == 999 && !strcmp(reply,"?;"));
    tx = false; command("RC"); assert(!VFO.RIT);
    FREQ.item = MEMItem; command("MC007"); assert(FREQ.numOfmem == 7);
    command("MC"); assert(!strcmp(reply,"MC 07;"));
    command("MC199"); assert(!strcmp(reply,"?;") && FREQ.numOfmem == 7);
    FREQ.item = VFOItem | VFOAItem; VFO_Get();
    tune = tx = true; command("FA00007200000"); assert(!tune && !tx && FREQ.val == 710000 && stops);
    tune = tx = true; command("TX0"); assert(!tune && !tx && !tx_calls);
    tune = tx = true; command("RX"); assert(!tune && !tx);
    const char *queries[] = {"PS","AI","IF","BY","FA","FB","FR","FT","FL","FW","ID","IS","KS","KY","MC","MD","RA","RG","RM","RT","XT","CA","AG","MR01","SM0"};
    for (unsigned i=0; i<sizeof queries/sizeof *queries; ++i) command(queries[i]);
    uint32_t random = 0x12345678;
    for (unsigned i=0; i<5000; ++i) {
        uint8_t input[33];
        for (unsigned j=0; j<sizeof input; ++j) { random = random*1664525u+1013904223u; input[j] = random>>24; }
        Kenwood_CmdParser(input, i % 34);
    }
    puts("PASS: production settings/CAT: legacy migration, CRC, interrupted saves, I/O failures, responses, independent VFOs and RF interlocks");
}
