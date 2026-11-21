#ifndef FX9A_NATIVE_RADIO_UI_H
#define FX9A_NATIVE_RADIO_UI_H
#include <stdbool.h>
#include <stdint.h>
void RadioUI_ForceMemoryRedraw(void);
void board_refresh_manager(void);
void MODE_Refresh(uint8_t all);
void RadioUI_TuneStarting(void);
void RadioUI_TuneStopped(bool had_carrier);
#endif
