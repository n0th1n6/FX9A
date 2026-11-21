#ifndef FX9A_RADIO_STATE_H
#define FX9A_RADIO_STATE_H
#include "bsp_board.h"

/* Legacy shared radio data retains its existing types and EEPROM ABI.
 * RX/TX latches belong to radio_rf; display caches belong to radio_ui.
 */
extern TP_AttrDef TP;
extern FIL_AttrDef FIL;
extern FIL_AttrDef CWFilter;
extern bool isRun;
void RadioState_Load(void);
void RadioState_Save(void);
void RadioState_ResetDefaults(void);
void RX_AlignmentInit(void);

#endif
