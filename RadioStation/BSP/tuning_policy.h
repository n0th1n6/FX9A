#ifndef FX9A_TUNING_POLICY_H
#define FX9A_TUNING_POLICY_H
#include <stdint.h>
#include <stdbool.h>

/* No EEPROM format change: the previously reserved menu byte is used only
 * for an explicit OFF signature. Unrecognized/erased values preserve the
 * preexisting continuous-tuning behavior for older radios.
 */
static inline bool fx9a_continuous_on(uint8_t stored)
{
    return stored != 0xc0u;
}
static inline bool fx9a_tuning_allowed(bool continuous,
                                       uint8_t selected_band,
                                       uint32_t requested,
                                       const uint32_t bounds[][3],
                                       uint8_t count)
{
    if (count == 0 || selected_band >= count) return false;
    if (requested < bounds[0][0] || requested > bounds[count - 1u][2])
        return false;
    if (!continuous && (requested < bounds[selected_band][0] ||
                        requested > bounds[selected_band][2]))
        return false;
    return true;
}
static inline uint32_t fx9a_tuning_lower(bool continuous, uint8_t selected,
                                          const uint32_t bounds[][3], uint8_t count)
{
    if (count == 0 || selected >= count) return 0u;
    return bounds[continuous ? 0 : selected][0];
}
static inline uint32_t fx9a_tuning_upper(bool continuous, uint8_t selected,
                                          const uint32_t bounds[][3], uint8_t count)
{
    if (count == 0 || selected >= count) return 0u;
    return bounds[continuous ? count - 1u : selected][2];
}
#endif
