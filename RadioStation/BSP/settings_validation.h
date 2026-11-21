#ifndef FX9A_SETTINGS_VALIDATION_H
#define FX9A_SETTINGS_VALIDATION_H
#include <stdint.h>
#include <stdbool.h>

/* Primitive checks used when reading existing, unversioned EEPROM records.
 * Avoid overwriting the user's original EEPROM just because a value in RAM
 * is corrupt. A future versioned CRC format can give stronger guarantees.
 */
static inline bool fx9a_mode_valid(uint8_t mode)
{
    return mode >= 1u && mode <= 5u;
}
static inline uint8_t fx9a_saved_band(uint8_t band, uint8_t limit, uint8_t fallback)
{
    return band < limit ? band : fallback;
}
static inline uint8_t fx9a_saved_volume(uint8_t value)
{
    return value <= 60u ? value : 0u;
}
static inline uint8_t fx9a_saved_wpm(uint8_t value)
{
    return value >= 4u && value <= 60u ? value : 15u;
}
static inline uint8_t fx9a_saved_swr(uint8_t value)
{
    return value <= 9u ? value : 9u;
}
static inline uint8_t fx9a_saved_delay(uint8_t value)
{
    return value >= 1u && value <= 15u ? value : 8u;
}
static inline bool fx9a_saved_frequency(uint32_t value, uint32_t min,
                                         uint32_t max)
{
    return value >= min && value <= max;
}
#endif
