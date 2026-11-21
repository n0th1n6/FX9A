#include <stdint.h>
#include "cmsis_gcc.h"
#include "settings_store.h"
#include "radio_state.h"
#include "bsp_24lc16.h"
#include <string.h>

#define META_ADDR 1152u
#define META_SIZE 20u
static const uint8_t magic[4] = {'F', 'X', 'S', '1'};
static const uint8_t tail[4] = {'1', 'S', 'X', 'F'};
/* Main-loop only. A fixed snapshot prevents encoder ISR edits from changing
 * bytes underneath write verification/CRC. No I2C operation masks interrupts.
 */
static struct {
  MEM_AttrDef memories[SIZE_OF_MEM];
  VFO_SaveDef vfo;
  MENU_AttrDef menu;
  FIL_FuncDef filter;
  uint8_t volume, attenuator, wpm;
} snapshot;
static void copy_live(bool capture)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (capture) {
    memcpy(snapshot.memories, MEM, sizeof MEM);
    snapshot.vfo = VFO; snapshot.menu = MENU; snapshot.filter = FIL.func;
    snapshot.volume = VOL.val; snapshot.attenuator = ATT.val; snapshot.wpm = WPM.val;
  } else {
    memcpy(MEM, snapshot.memories, sizeof MEM);
    VFO = snapshot.vfo; MENU = snapshot.menu; FIL.func = snapshot.filter;
    VOL.val = snapshot.volume; ATT.val = snapshot.attenuator; WPM.val = snapshot.wpm;
  }
  __set_PRIMASK(primask);
}
static uint32_t crc_update(uint32_t crc, const uint8_t *data, unsigned len)
{
  while (len--) {
    crc ^= *data++;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}
static void put32(uint8_t *p, uint32_t value)
{
  for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static uint32_t get32(const uint8_t *p)
{
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
         (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
/* Verify each write against its intended bytes, not merely against ACKs. */
static bool write_verified(uint16_t addr, uint8_t *data, uint16_t len)
{
  uint8_t check[8];
  if (!e2p_write(addr, data, len)) return false;
  while (len) {
    uint16_t n = len > sizeof check ? sizeof check : len;
    if (!e2p_read(addr, check, n) || memcmp(check, data, n)) return false;
    addr += n; data += n; len -= n;
  }
  return true;
}
static bool transfer(bool write, uint16_t addr, void *data, uint16_t len,
                     uint32_t *crc)
{
  if (write ? !write_verified(addr, data, len) : !e2p_read(addr, data, len))
    return false;
  *crc = crc_update(*crc, data, len);
  return true;
}
static bool payload(bool write, uint32_t *crc)
{
  _Static_assert(sizeof(MEM_AttrDef) == 6, "legacy memory ABI");
  _Static_assert(sizeof(VFO_SaveInfo) == 5, "legacy VFO ABI");
  _Static_assert(sizeof(MENU_AttrDef) == 44, "legacy menu ABI");
  _Static_assert(sizeof(FIL_FuncDef) == 1, "legacy filter ABI requires short enums");
#define FIELD(addr, value) do { if (!transfer(write, (addr), &(value), sizeof(value), crc)) return false; } while (0)
  for (unsigned i = 0; i < SIZE_OF_MEM; ++i) FIELD(E2P_ADDR_MEM + i * 8, snapshot.memories[i]);
  for (unsigned i = 0; i < SIZE_OF_BAUD; ++i) {
    FIELD(E2P_ADDR_VFOA + i * 8, snapshot.vfo.VFOA[i]);
    FIELD(E2P_ADDR_VFOB + i * 8, snapshot.vfo.VFOB[i]);
  }
  FIELD(E2P_ADDR_A_BAUD, snapshot.vfo.CurrBaudOfVfoA);
  FIELD(E2P_ADDR_B_BAUD, snapshot.vfo.CurrBaudOfVfoB);
  FIELD(E2P_ADDR_RIT, snapshot.vfo.RIT);
  FIELD(E2P_ADDR_XIT, snapshot.vfo.XIT);
  FIELD(E2P_ADDR_FIL, snapshot.filter);
  FIELD(E2P_ADDR_VOL, snapshot.volume);
  FIELD(E2P_ADDR_ATT, snapshot.attenuator);
  FIELD(E2P_ADDR_PWM, snapshot.wpm);
  FIELD(E2P_ADDR_MENU, snapshot.menu);
#undef FIELD
  return true;
}
SettingsResult Settings_Load(void)
{
  uint8_t marker, meta[META_SIZE];
  if (!e2p_read(E2P_ADDR_SAVE, &marker, 1) ||
      !e2p_read(META_ADDR, meta, sizeof meta)) return SETTINGS_IO_ERROR;
  bool tagged = !memcmp(meta, magic, 4) || !memcmp(meta + 16, tail, 4);
  if (tagged && (memcmp(meta, magic, 4) || memcmp(meta + 16, tail, 4) ||
      meta[4] != 1 || meta[5] != 1 || meta[6] || meta[7] ||
      get32(meta + 12) != ~crc_update(~0u, meta, 12))) return SETTINGS_CORRUPT;
  if (marker != 0x5a) return tagged ? SETTINGS_CORRUPT : SETTINGS_EMPTY;
  uint32_t crc = ~0u;
  if (!payload(false, &crc)) return SETTINGS_IO_ERROR;
  if (tagged && get32(meta + 8) != ~crc) return SETTINGS_CORRUPT;
  copy_live(false);
  return tagged ? SETTINGS_VALID : SETTINGS_LEGACY;
}
bool Settings_Save(void)
{
  copy_live(true);
  uint8_t meta[META_SIZE] = {0};
  memcpy(meta, magic, 4); memcpy(meta + 16, tail, 4); meta[4] = 1;
  put32(meta + 12, ~crc_update(~0u, meta, 12));
  /* A verified in-progress marker precedes every payload write. Interrupted
   * saves are detected at next boot; this is detection, not dual-bank rollback.
   */
  if (!write_verified(META_ADDR, meta, sizeof meta)) return false;
  uint32_t crc = ~0u;
  if (!payload(true, &crc)) return false;
  uint8_t marker = 0x5a;
  if (!write_verified(E2P_ADDR_SAVE, &marker, 1)) return false;
  meta[5] = 1; put32(meta + 8, ~crc);
  put32(meta + 12, ~crc_update(~0u, meta, 12));
  return write_verified(META_ADDR, meta, sizeof meta);
}
