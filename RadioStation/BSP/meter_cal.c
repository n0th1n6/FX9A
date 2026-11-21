#include "meter_cal.h"
#include "bsp_24lc16.h"
#include "bsp_board.h"
#include <string.h>

/* Calibration record lives at EEPROM 1104..1133; the original menu
 * remains at 1024 and the original save marker remains at 1600.
 */
static Fx9aMeterCal meter_cal;
static bool meter_cal_active;
static bool meter_cal_dirty;

static void meter_cal_defaults(void)
{
    memset(&meter_cal, 0, sizeof meter_cal);
    meter_cal.magic_low = (uint16_t)FX9A_CAL_MAGIC;
    meter_cal.magic_high = (uint16_t)(FX9A_CAL_MAGIC >> 16);
    meter_cal.version = FX9A_CAL_VERSION;
    const uint16_t s[5] = {32, 80, 131, 2408, 3932};
    const uint16_t rf[3] = {150, 770, 1081};
    const uint16_t reflected[3] = {20, 360, 574};
    memcpy(meter_cal.s, s, sizeof s);
    memcpy(meter_cal.rf, rf, sizeof rf);
    memcpy(meter_cal.reflected, reflected, sizeof reflected);
    meter_cal.crc = fx9a_cal_crc(&meter_cal);
}

void MeterCal_Init(void)
{
    meter_cal_defaults();
    meter_cal_active = false;
    meter_cal_dirty = false;
    Fx9aMeterCal candidate = {0};
    if (e2p_read(E2P_ADDR_CAL, (uint8_t *)&candidate,
                 (uint16_t)sizeof candidate) && fx9a_cal_valid(&candidate)) {
        meter_cal = candidate;
        meter_cal_active = true;
    }
}
bool MeterCal_Active(void)
{
    return meter_cal_active;
}
uint16_t MeterCal_Value(uint8_t index)
{
    uint16_t *point = fx9a_cal_point(&meter_cal, index);
    return point ? *point : 0u;
}
void MeterCal_Adjust(uint8_t position, int16_t step)
{
    if (position < METER_CAL_FIRST || position > METER_CAL_LAST) return;
    uint8_t index = position - METER_CAL_FIRST;
    uint16_t *point = fx9a_cal_point(&meter_cal, index);
    if (!point) return;
    int32_t candidate = (int32_t)*point + step;
    uint16_t *before = 0; /* configured by group below */
    uint16_t *after = 0;
    if (index < 5u) {
        before = index ? &meter_cal.s[index - 1u] : 0;
        after = index < 4u ? &meter_cal.s[index + 1u] : 0;
    } else if (index < 8u) {
        before = index > 5u ? &meter_cal.rf[index - 6u] : 0;
        after = index < 7u ? &meter_cal.rf[index - 4u] : 0;
    } else {
        before = index > 8u ? &meter_cal.reflected[index - 9u] : 0;
        after = index < 10u ? &meter_cal.reflected[index - 7u] : 0;
    }
    if (before && candidate <= *before) candidate = *before + 1;
    if (after && candidate >= *after) candidate = *after - 1;
    if (candidate < 0) candidate = 0;
    if (candidate > 4095) candidate = 4095;
    if (*point != candidate) {
        *point = (uint16_t)candidate;
        meter_cal_dirty = true;
    }
}
bool MeterCal_Save(void)
{
    if (!meter_cal_dirty) return true;
    meter_cal.magic_low = (uint16_t)FX9A_CAL_MAGIC;
    meter_cal.magic_high = (uint16_t)(FX9A_CAL_MAGIC >> 16);
    meter_cal.version = FX9A_CAL_VERSION;
    meter_cal.crc = fx9a_cal_crc(&meter_cal);
    if (!fx9a_cal_valid(&meter_cal)) return false;
    if (!e2p_write(E2P_ADDR_CAL, (uint8_t *)&meter_cal,
                   (uint16_t)sizeof meter_cal))
        return false;
    meter_cal_dirty = false;
    meter_cal_active = true;
    return true;
}
uint8_t MeterCal_Grid(uint8_t meter, uint16_t adc)
{
    static const uint8_t s_y[5] = {0,5,9,17,25};
    static const uint8_t rf_y[3] = {1,6,11};
    static const uint8_t refl_y[3] = {0,6,9};
    switch (meter) {
    case 0: return fx9a_meter_interpolate(adc, meter_cal.s, s_y, 5u);
    case 1: return fx9a_meter_interpolate(adc, meter_cal.rf, rf_y, 3u);
    case 2: return fx9a_meter_interpolate(adc, meter_cal.reflected, refl_y, 3u);
    default: return 0u;
    }
}
