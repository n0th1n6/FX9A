#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "meter_cal.h"

int main(void)
{
    Fx9aMeterCal cal = {0};
    cal.magic = FX9A_CAL_MAGIC;
    cal.version = FX9A_CAL_VERSION;
    const uint16_t s[5] = {32, 131, 819, 1098, 1311};
    const uint16_t rf[3] = {150, 770, 1081};
    const uint16_t refl[3] = {20, 360, 574};
    memcpy(cal.s, s, sizeof s);
    memcpy(cal.rf, rf, sizeof rf);
    memcpy(cal.reflected, refl, sizeof refl);
    cal.crc = fx9a_cal_crc(&cal);
    assert(sizeof cal == 30u);
    assert(fx9a_cal_valid(&cal));
    static const uint8_t levels[5] = {0,5,9,17,25};
    assert(fx9a_meter_interpolate(10u, cal.s, levels, 5u) == 0u);
    assert(fx9a_meter_interpolate(131u, cal.s, levels, 5u) == 5u);
    assert(fx9a_meter_interpolate(819u, cal.s, levels, 5u) == 9u);
    assert(fx9a_meter_interpolate(1400u, cal.s, levels, 5u) == 25u);
    assert(*fx9a_cal_point(&cal, 0) == 32u);
    assert(*fx9a_cal_point(&cal, 7) == 1081u);
    assert(*fx9a_cal_point(&cal, 10) == 574u);
    assert(fx9a_cal_point(&cal, 11) == NULL);
    cal.s[2] = 9u;
    assert(!fx9a_cal_valid(&cal));
    cal.s[2] = 819u;
    cal.crc ^= 1u;
    assert(!fx9a_cal_valid(&cal));
    cal.crc = fx9a_cal_crc(&cal);
    cal.version = 2u;
    assert(!fx9a_cal_valid(&cal));
    puts("PASS: calibration EEPROM ABI, CRC, version, monotonic points and interpolation");
    return 0;
}
