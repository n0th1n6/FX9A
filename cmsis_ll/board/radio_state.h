#ifndef FX9A_NATIVE_RADIO_STATE_H
#define FX9A_NATIVE_RADIO_STATE_H
#include "bsp_board.h"
void RX_AlignmentInit(void);
void ResetToDefault(void);
void software_init(void);
bool RadioState_Save(void);
#endif
