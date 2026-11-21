#ifndef FX9A_RADIO_UI_H
#define FX9A_RADIO_UI_H
#include <stdbool.h>
#include <stdint.h>

void RadioUI_Refresh(void);
void RadioUI_ForceMemoryRedraw(void);
/* Main-loop presentation events; never called by the sample ISR. */
void RadioUI_TuneStarting(void);
void RadioUI_ShowTune(void);
void RadioUI_TuneStopped(bool visible, bool sampled, uint16_t reflected_adc);
void RadioUI_SwrTrip(void);

#endif
