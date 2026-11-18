#ifndef FX9A_FREQUENCY_H
#define FX9A_FREQUENCY_H

#include <stdint.h>

/* Frequencies use the legacy radio's 10 Hz units. Each filter remains selected
 * until the next band's lower boundary; amateur-band upper edges do not clamp
 * tuning. The caller supplies the existing ordered band table. */
static inline uint8_t frequency_band(uint32_t frequency,
                                     const uint32_t bands[][3], uint8_t count)
{
    uint8_t band = 0;
    for (uint8_t i = 1; i < count; ++i) {
        if (frequency < bands[i][0])
            break;
        band = i;
    }
    return band;
}

static inline uint32_t frequency_step(uint32_t frequency, int32_t step,
                                       uint32_t minimum, uint32_t maximum)
{
    if (frequency < minimum)
        frequency = minimum;
    if (frequency > maximum)
        frequency = maximum;
    if (step >= 0) {
        uint32_t delta = (uint32_t)step;
        return delta > maximum - frequency ? maximum : frequency + delta;
    }
    uint32_t delta = (uint32_t)(-(int64_t)step);
    return delta > frequency - minimum ? minimum : frequency - delta;
}

#endif
