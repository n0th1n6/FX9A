#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "tuning_policy.h"

static const uint32_t bands[3][3] = {
    {179900u, 190500u, 200099u},
    {349900u, 365000u, 400099u},
    {699900u, 705000u, 730099u}
};
int main(void)
{
    assert(fx9a_continuous_on(0xffu)); /* erased legacy EEPROM */
    assert(fx9a_continuous_on(0x00u)); /* old uninitialized reserved byte */
    assert(fx9a_continuous_on(0xc1u));
    assert(!fx9a_continuous_on(0xc0u));
    assert(fx9a_tuning_allowed(true, 0, 300000u, bands, 3));
    assert(!fx9a_tuning_allowed(false, 0, 300000u, bands, 3));
    assert(fx9a_tuning_allowed(false, 1, 365000u, bands, 3));
    assert(!fx9a_tuning_allowed(false, 1, 400100u, bands, 3));
    assert(!fx9a_tuning_allowed(true, 0, 179899u, bands, 3));
    assert(!fx9a_tuning_allowed(true, 0, 730100u, bands, 3));
    assert(!fx9a_tuning_allowed(true, 3, 365000u, bands, 3));
    assert(fx9a_tuning_lower(true, 2, bands, 3) == 179900u);
    assert(fx9a_tuning_upper(true, 2, bands, 3) == 730099u);
    assert(fx9a_tuning_lower(false, 1, bands, 3) == 349900u);
    assert(fx9a_tuning_upper(false, 1, bands, 3) == 400099u);
    puts("PASS: continuous ON/OFF, legacy EEPROM default, CAT band limits and gaps");
    return 0;
}
