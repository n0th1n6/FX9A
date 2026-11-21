#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "settings_validation.h"
int main(void)
{
    assert(fx9a_mode_valid(1u) && fx9a_mode_valid(5u));
    assert(!fx9a_mode_valid(0u) && !fx9a_mode_valid(255u));
    assert(fx9a_saved_band(4u,10u,3u) == 4u);
    assert(fx9a_saved_band(255u,10u,3u) == 3u);
    assert(fx9a_saved_volume(60u) == 60u);
    assert(fx9a_saved_volume(255u) == 0u);
    assert(fx9a_saved_wpm(4u) == 4u);
    assert(fx9a_saved_wpm(0u) == 15u);
    assert(fx9a_saved_wpm(255u) == 15u);
    assert(fx9a_saved_swr(9u) == 9u);
    assert(fx9a_saved_swr(255u) == 9u);
    assert(fx9a_saved_delay(1u) == 1u);
    assert(fx9a_saved_delay(0u) == 8u);
    assert(fx9a_saved_delay(200u) == 8u);
    assert(fx9a_saved_frequency(705000u,179900u,2970099u));
    assert(!fx9a_saved_frequency(0u,179900u,2970099u));
    assert(!fx9a_saved_frequency(0xffffffffu,179900u,2970099u));
    puts("PASS: old EEPROM settings validation of indexes, modes and ranges");
    return 0;
}
