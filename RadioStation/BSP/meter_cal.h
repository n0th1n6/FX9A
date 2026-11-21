#ifndef FX9A_METER_CAL_H
#define FX9A_METER_CAL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FX9A_CAL_MAGIC 0x41433946u /* "F9CA" little endian */
#define FX9A_CAL_VERSION 1u
#define FX9A_CAL_S_POINTS 5u
#define FX9A_CAL_RF_POINTS 3u
#define FX9A_CAL_SWR_POINTS 3u
#define FX9A_CAL_NUM_POINTS 11u

typedef struct {
    uint16_t magic_low;
    uint16_t magic_high;
    uint8_t version;
    uint8_t reserved;
    uint16_t s[5];           /* S0,S5,S9,S9+20,S9+40 */
    uint16_t rf[3];          /* nominal 1/5/10W */
    uint16_t reflected[3];   /* reflected-power / SWR meter calibration */
    uint16_t crc;
} Fx9aMeterCal;
_Static_assert(sizeof(Fx9aMeterCal) == 30u, "Calibration EEPROM record size");

static inline uint16_t fx9a_cal_crc(const Fx9aMeterCal *cal)
{
    const uint8_t *p = (const uint8_t *)cal;
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < offsetof(Fx9aMeterCal, crc); ++i) {
        crc ^= (uint16_t)p[i] << 8;
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                   : (uint16_t)(crc << 1);
    }
    return crc;
}
static inline bool fx9a_cal_increasing(const uint16_t *a, unsigned len)
{
    for (unsigned i = 0; i < len; ++i) {
        if (a[i] > 4095u || (i && a[i] <= a[i-1u])) return false;
    }
    return true;
}
static inline bool fx9a_cal_valid(const Fx9aMeterCal *cal)
{
    return cal->magic_low == (uint16_t)FX9A_CAL_MAGIC &&
        cal->magic_high == (uint16_t)(FX9A_CAL_MAGIC >> 16) &&
        cal->version == FX9A_CAL_VERSION &&
        fx9a_cal_increasing(cal->s, 5u) &&
        fx9a_cal_increasing(cal->rf, 3u) &&
        fx9a_cal_increasing(cal->reflected, 3u) &&
        cal->crc == fx9a_cal_crc(cal);
}
static inline uint8_t fx9a_meter_interpolate(uint16_t adc, const uint16_t *x,
                                              const uint8_t *y, uint8_t size)
{
    if (size == 0u || adc < x[0]) return 0u;
    for (uint8_t i = 1u; i < size; ++i) {
        if (adc <= x[i])
            return (uint8_t)(y[i - 1u] +
                ((uint32_t)(adc - x[i - 1u]) *
                 (uint32_t)(y[i] - y[i - 1u])) / (x[i] - x[i - 1u]));
    }
    return y[size - 1u];
}
static inline uint16_t *fx9a_cal_point(Fx9aMeterCal *cal, uint8_t index)
{
    if (index < 5u) return &cal->s[index];
    if (index < 8u) return &cal->rf[index - 5u];
    if (index < 11u) return &cal->reflected[index - 8u];
    return (uint16_t *)0;
}
/* Optional, EEPROM-validated service calibration for displayed meters.
 * RF protection never depends on this user-editable record.
 */
void MeterCal_Init(void);
bool MeterCal_Active(void);
uint16_t MeterCal_Value(uint8_t index);
void MeterCal_Adjust(uint8_t position, int16_t step);
bool MeterCal_Save(void);
uint8_t MeterCal_Grid(uint8_t meter, uint16_t adc);
#endif
