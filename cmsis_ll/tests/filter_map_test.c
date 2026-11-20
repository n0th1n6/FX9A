#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "filter_map.h"
int main(void)
{
    static const uint8_t expected_hc[10] = {0,1,8,2,3,4,5,6,7,9};
#ifdef FX9A_BOARD_ORIGINAL
    static const uint8_t expected_ne[10] = {0,1,2,2,4,3,6,6,5,5};
#else
    static const uint8_t expected_ne[10] = {6,5,4,4,3,2,1,1,0,0};
#endif
    for (uint8_t i = 0; i < 10u; ++i) {
        Fx9aBandFilter f = fx9a_filter_for_band(i);
        assert(f.ne5090_code == expected_ne[i]);
        assert(f.hc4028_code == expected_hc[i]);
        assert(f.ne5090_code <= 7u && f.hc4028_code <= 15u);
    }
    printf("PASS: filter mapping for %s revision, all 10 bands\n",
#ifdef FX9A_BOARD_ORIGINAL
        "original"
#else
        "2016-03"
#endif
    );
    return 0;
}
