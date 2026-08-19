#include <stddef.h>

#include "bsp_board.h"

/* These layouts are part of the 24LC16 EEPROM format used by the IAR
 * firmware. A compiler ABI change here would corrupt adjacent records. */
_Static_assert(sizeof(RADIO_ModeDef) == 1, "radio mode must remain one byte");
_Static_assert(sizeof(FIL_FuncDef) == 1, "filter mode must remain one byte");

_Static_assert(sizeof(VFO_SaveInfo) == 5, "unexpected VFO EEPROM record size");
_Static_assert(offsetof(VFO_SaveInfo, mode) == 4,
               "unexpected VFO mode offset");

_Static_assert(sizeof(MEM_AttrDef) == 6, "unexpected memory EEPROM record size");
_Static_assert(offsetof(MEM_AttrDef, baud) == 4,
               "unexpected memory band offset");
_Static_assert(offsetof(MEM_AttrDef, mode) == 5,
               "unexpected memory mode offset");

_Static_assert(sizeof(MENU_AttrDef) == 44, "unexpected menu EEPROM size");
_Static_assert(offsetof(MENU_AttrDef, freqOfbuzzer) == 16,
               "unexpected buzzer frequency offset");
_Static_assert(offsetof(MENU_AttrDef, rbfo) == 20,
               "unexpected BFO offset");
_Static_assert(offsetof(MENU_AttrDef, callSign) == 32,
               "unexpected callsign offset");
_Static_assert(offsetof(MENU_AttrDef, cwnloOffset) == 42,
               "unexpected CW offset field");

/* These are not EEPROM records, but matching the IAR member offsets keeps the
 * original pointer, enum, and scalar accesses equivalent under GCC. IAR adds
 * extra tail padding to some scalar-only structs; no code persists or copies
 * that padding. */
_Static_assert(sizeof(FREQ_AttrDef) == 12, "unexpected frequency state size");
_Static_assert(offsetof(FREQ_AttrDef, str) == 4,
               "unexpected frequency string offset");
_Static_assert(offsetof(FREQ_AttrDef, item) == 8,
               "unexpected frequency item offset");
_Static_assert(offsetof(FREQ_AttrDef, numOfbaud) == 9,
               "unexpected frequency band offset");
_Static_assert(offsetof(FREQ_AttrDef, numOfmem) == 10,
               "unexpected frequency memory offset");

_Static_assert(sizeof(VFO_SaveDef) == 112, "unexpected VFO state size");
_Static_assert(offsetof(VFO_SaveDef, VFOA) == 8,
               "unexpected VFO A offset");
_Static_assert(offsetof(VFO_SaveDef, VFOB) == 58,
               "unexpected VFO B offset");
_Static_assert(offsetof(VFO_SaveDef, CurrBaudOfVfoA) == 108,
               "unexpected current VFO A band offset");

_Static_assert(sizeof(TP_AttrDef) == 8, "unexpected TX power state size");
_Static_assert(offsetof(TP_AttrDef, level) == 4,
               "unexpected TX power level offset");
_Static_assert(sizeof(FIL_AttrDef) == 8, "unexpected filter state size");
_Static_assert(offsetof(FIL_AttrDef, func) == 4,
               "unexpected filter function offset");

void fx9a_layout_checks_anchor(void)
{
    /* Keep this translation unit non-empty on compilers predating C11. */
}
