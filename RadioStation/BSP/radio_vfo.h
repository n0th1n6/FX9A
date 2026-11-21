#ifndef FX9A_RADIO_VFO_H
#define FX9A_RADIO_VFO_H
#include "bsp_board.h"

void RadioVFO_Init(void);
/* Save the same legacy records/order as shutdown (false) or reset (true). */
void RadioVFO_Persist(bool all_memories);

#endif
