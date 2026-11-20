#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "dds_math.h"

int main(void)
{
    assert(fx9a_ad9834_ftw(0, 50000000u) == 0);
    assert(fx9a_ad9834_ftw(12500000u, 50000000u) == (1u << 26));
    assert(fx9a_ad9834_ftw(25000000u, 50000000u) == (1u << 27));
    assert(fx9a_ad9834_ftw(50000000u, 50000000u) == 0x0fffffffu);
    assert(fx9a_ad9834_ftw(100, 0u) == 0u);
    assert(fx9a_ad9951_ftw(0, 200000000u) == 0u);
    assert(fx9a_ad9951_ftw(50000000u, 200000000u) == 0x40000000u);
    assert(fx9a_ad9951_ftw(100000000u, 200000000u) == 0x80000000u);
    assert(fx9a_ad9951_ftw(200000000u, 200000000u) == 0xffffffffu);
    assert(fx9a_ad9951_ftw(100, 0u) == 0u);
    assert(fx9a_ad9834_phase_word(0) == 0);
    assert(fx9a_ad9834_phase_word(90) == 1024);
    assert(fx9a_ad9834_phase_word(180) == 2048);
    assert(fx9a_ad9834_phase_word(270) == 3072);
    assert(fx9a_ad9834_phase_word(360) == 0);
    puts("PASS: integer 28/32-bit FTWs and 12-bit AD9834 phase");
    return 0;
}
