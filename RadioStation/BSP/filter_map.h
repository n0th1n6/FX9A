#ifndef FX9A_FILTER_MAP_H
#define FX9A_FILTER_MAP_H
#include <stdint.h>

/* Mapping is from the FX-9A revision notes in bsp_baud.c.
 * The active firmware used the 2016-03-09 wiring by default.
 * hc4028_code is unchanged for both supported boards.
 */
typedef struct {
    uint8_t ne5090_code;
    uint8_t hc4028_code;
} Fx9aBandFilter;

static inline Fx9aBandFilter fx9a_filter_for_band(uint8_t band)
{
#ifdef FX9A_BOARD_ORIGINAL
    static const uint8_t ne5090[10] = {0,1,2,2,4,3,6,6,5,5};
#else
    static const uint8_t ne5090[10] = {6,5,4,4,3,2,1,1,0,0};
#endif
    static const uint8_t hc4028[10] = {0,1,8,2,3,4,5,6,7,9};
    if (band >= 10u) band = 9u;
    Fx9aBandFilter result = {ne5090[band], hc4028[band]};
    return result;
}
#endif
