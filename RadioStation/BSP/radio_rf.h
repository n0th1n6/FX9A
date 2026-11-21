#ifndef FX9A_RADIO_RF_H
#define FX9A_RADIO_RF_H
#include "bsp_board.h"

/* All RF/TUNE/SWR state is private to radio_rf except the legacy trip latch.
 * RF_SwrSampleISR does no display, EEPROM, delay or synthesizer work.
 */
uint32_t Get_DDSLO(uint32_t freq);
uint32_t Get_DDSBFO(void);

#endif
