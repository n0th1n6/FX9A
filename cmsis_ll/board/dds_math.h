#ifndef FX9A_DDS_MATH_H
#define FX9A_DDS_MATH_H
#include <stdint.h>

/* All arguments are in Hz. The radio uses low HF frequencies, so a 64-bit
 * numerator holds both (frequency << 32) and (frequency << 28).
 * Out-of-range inputs are clamped, rather than silently wrapping.
 */
static inline uint32_t fx9a_ad9834_ftw(uint32_t hz, uint32_t reference_hz)
{
    if (reference_hz == 0u) return 0u;
    if (hz >= reference_hz) return 0x0fffffffu;
    return (uint32_t)((((uint64_t)hz << 28) + reference_hz / 2u) / reference_hz);
}
static inline uint32_t fx9a_ad9951_ftw(uint32_t hz, uint32_t reference_hz)
{
    if (reference_hz == 0u) return 0u;
    if (hz >= reference_hz) return 0xffffffffu;
    return (uint32_t)((((uint64_t)hz << 32) + reference_hz / 2u) / reference_hz);
}
static inline uint16_t fx9a_ad9834_phase_word(uint16_t degrees)
{
    return (uint16_t)((((uint32_t)(degrees % 360u) * 4096u) + 180u) / 360u) & 0x0fffu;
}
#endif
